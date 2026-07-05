//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "EffectBaker.h"
#include "Application.h"
#include "ConfigFile.h"

#include "../Include/Types.h"
#include "GlslangToSpv.h"
#include "ResourceLimits.h"
#include "ShaderLang.h"
#include "spirv_glsl.hpp"
#include "spirv_hlsl.hpp"
#include "spirv_msl.hpp"

FO_BEGIN_NAMESPACE

// The engine bakes one Vulkan-1.0 SPIR-V module per stage (native Vulkan renderer convention: set 0 = uniform
// buffers, set 1 = combined image samplers, shared by both stages). The SDL_GPU backend needs the same SPIR-V
// remapped to SDL_GPU's per-stage descriptor convention (SDL3 SDL_gpu.h, SDL_CreateGPUShader docs):
//   vertex stage:   set 0 = sampled textures, set 1 = uniform buffers;
//   fragment stage: set 2 = sampled textures, set 3 = uniform buffers;
// with dense 0..N-1 slot indices per set. The extra `-spv_sdl` flavor and the SDL-remapped MSL are produced from
// the native SPIR-V by rewriting the descriptor decorations, so the native `-spv` (consumed by Rendering-Vulkan)
// stays untouched.
static constexpr int32_t SDLGPU_MAX_SAMPLERS_PER_STAGE = 16;
static constexpr int32_t SDLGPU_MAX_UNIFORM_BUFFERS_PER_STAGE = 4;

// Per-stage SDL_GPU slot assignment: resources ordered by SDL slot index, pair = resource name, authored Vulkan binding
struct EffectBaker::SdlStageSlots
{
    vector<pair<string, int32_t>> Samplers {};
    vector<pair<string, int32_t>> UniformBufs {};
};

static auto AssignSdlStageSlots(const glslang::TProgram& program, EShLanguage stage, string_view fname) -> EffectBaker::SdlStageSlots;
static void PatchSpirvForSdlGpu(std::vector<uint32_t>& spirv, const EffectBaker::SdlStageSlots& sdl_slots, bool is_vertex, string_view fname);
static void ApplySdlMslResourceBindings(spirv_cross::CompilerMSL& compiler, const EffectBaker::SdlStageSlots& sdl_slots, bool is_vertex);

EffectBaker::EffectBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx))
{
    FO_STACK_TRACE_ENTRY();

    glslang::InitializeProcess();
}

EffectBaker::~EffectBaker()
{
    FO_STACK_TRACE_ENTRY();

    glslang::FinalizeProcess();
}

void EffectBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    // Collect files
    vector<File> filtered_files;

    const auto check_file = [&](const File& file) -> bool {
        const string_view path = file.GetPath();
        const auto fofx = ConfigFile(path, file.GetStr());
        const auto passes = fofx.GetAsInt("Effect", "Passes", 1);
        const auto write_time = file.GetWriteTime();

        for (int32_t pass = 1; pass <= passes; pass++) {
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-info", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-vert-spv", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-frag-spv", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-vert-spv_sdl", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-frag-spv_sdl", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-vert-glsl", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-frag-glsl", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-vert-glsl_es", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-frag-glsl_es", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-vert-hlsl", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-frag-hlsl", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-vert-msl_mac", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-frag-msl_mac", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-vert-msl_ios", pass)), write_time);
            (void)_context->BakeChecker(strex(path).change_file_extension(strex("fofx-{}-frag-msl_ios", pass)), write_time);
        }

        if (!_context->BakeChecker(path, write_time)) {
            return false;
        }

        return true;
    };

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            const string ext = strex(file_header.GetPath()).get_file_extension();

            if (ext != "fofx") {
                continue;
            }

            if constexpr (!FO_ENABLE_3D) {
                if (file_header.GetPath().find("3D") != string::npos) {
                    continue;
                }
            }

            auto file = File(File::Load(file_header));

            if (_context->BakeChecker && !check_file(file)) {
                continue;
            }

            filtered_files.emplace_back(std::move(file));
        }
    }
    else {
        if (!strex(target_path).get_file_extension().starts_with("fofx")) {
            return;
        }

        const string base_name = strex(target_path).change_file_extension("fofx");
        auto file = files.FindFileByPath(base_name);

        if (!file) {
            return;
        }
        if (_context->BakeChecker && !check_file(file)) {
            return;
        }

        filtered_files.emplace_back(std::move(file));
    }

    if (filtered_files.empty()) {
        return;
    }

    vector<std::future<void>> file_bakings;

    for (auto& file_ : filtered_files) {
        auto task_name = strex("BakeEffect-{}", file_.GetPath()).str();
        file_bakings.emplace_back(run_async(GetAsyncMode(), task_name, [this, file = std::move(file_)]() FO_DEFERRED {
            const string_view path = file.GetPath();
            const auto content = file.GetStr();
            BakeShaderProgram(path, content);
        }));
    }

    size_t errors = 0;

    for (auto& file_baking : file_bakings) {
        try {
            file_baking.get();
        }
        catch (const std::exception& ex) {
            WriteLog("Effect baking error: {}", ex.what());
            errors++;
        }
    }

    if (errors != 0) {
        throw EffectBakerException("Errors during effects baking", errors);
    }
}

void EffectBaker::BakeShaderProgram(string_view fname, string_view content) const
{
    FO_STACK_TRACE_ENTRY();

    auto fofx = ConfigFile(fname, string(content), ConfigFileOption::CollectContent);

    if (!fofx.HasSection("Effect")) {
        throw EffectBakerException(".fofx file truncated", fname);
    }

    constexpr bool old_code_profile = false;

    const auto passes = fofx.GetAsInt("Effect", "Passes", 1);
    const string shader_common_content = string(fofx.GetSectionContent("ShaderCommon"));
    const auto shader_version = fofx.GetAsInt("Effect", "Version", 310);
    const auto shader_version_str = strex("#version {} es\n", shader_version).str();
#if FO_ENABLE_3D
    const auto shader_defines = strex("precision highp float;\n#define MAX_BONES {}\n#define MAX_TEXTURES {}\n", MODEL_MAX_BONES, MODEL_MAX_TEXTURES).str();
#else
    const auto shader_defines = strex("precision highp float;\n").str();
#endif
    const string_view_nt shader_defines_ex = old_code_profile ? "#define layout(x)\n#define in attribute\n#define out varying\n#define texture texture2D\n#define FragColor gl_FragColor" : "";
    const auto shader_defines_ex2 = strex("#define MAX_SCRIPT_VALUES {}\n", EFFECT_SCRIPT_VALUES).str();

    for (auto pass = 1; pass <= passes; pass++) {
        string vertex_pass_content = string(fofx.GetSectionContent(strex("VertexShader Pass{}", pass)));
        if (vertex_pass_content.empty()) {
            vertex_pass_content = string(fofx.GetSectionContent("VertexShader"));
        }
        if (vertex_pass_content.empty()) {
            throw EffectBakerException("No content for vertex shader", fname, pass);
        }

        string fragment_pass_content = string(fofx.GetSectionContent(strex("FragmentShader Pass{}", pass)));
        if (fragment_pass_content.empty()) {
            fragment_pass_content = string(fofx.GetSectionContent("FragmentShader"));
        }
        if (fragment_pass_content.empty()) {
            throw EffectBakerException("No content for fragment shader", fname, pass);
        }

        glslang::TShader vert(EShLangVertex);
        vert.setEnvInput(glslang::EShSourceGlsl, EShLangVertex, glslang::EShClientVulkan, shader_version);
        vert.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        vert.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
        vert.setShiftBindingForSet(glslang::EResUbo, 0, 0);
        ptr<const char> shader_version_text = shader_version_str.c_str();
        ptr<const char> shader_defines_text = shader_defines.c_str();
        nptr<const char> shader_defines_ex_text = shader_defines_ex.c_str();
        ptr<const char> shader_defines_ex2_text = shader_defines_ex2.c_str();
        ptr<const char> shader_common_text = shader_common_content.c_str();
        ptr<const char> vertex_pass_text = vertex_pass_content.c_str();
        const char* vertex_strings[] = {shader_version_text.get(), shader_defines_text.get(), shader_defines_ex_text.get(), shader_defines_ex2_text.get(), shader_common_text.get(), vertex_pass_text.get()};
        vert.setStrings(vertex_strings, 6);
        if (!vert.parse(GetDefaultResources(), shader_version, true, EShMessages::EShMsgDefault)) {
            throw EffectBakerException("Failed to parse vertex shader", fname, pass, vert.getInfoLog());
        }

        glslang::TShader frag(EShLangFragment);
        frag.setEnvInput(glslang::EShSourceGlsl, EShLangFragment, glslang::EShClientVulkan, shader_version);
        frag.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        frag.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
        frag.setShiftBindingForSet(glslang::EResUbo, 0, 0);
        ptr<const char> fragment_pass_text = fragment_pass_content.c_str();
        const char* fragment_strings[] = {shader_version_text.get(), shader_defines_text.get(), shader_defines_ex_text.get(), shader_defines_ex2_text.get(), shader_common_text.get(), fragment_pass_text.get()};
        frag.setStrings(fragment_strings, 6);
        if (!frag.parse(GetDefaultResources(), shader_version, true, EShMessages::EShMsgDefault)) {
            throw EffectBakerException("Failed to parse fragment shader", fname, pass, frag.getInfoLog());
        }

        glslang::TProgram program;
        program.addShader(&vert);
        program.addShader(&frag);

        if (!program.link(EShMsgDefault)) {
            throw EffectBakerException("Failed to link shader program", fname, program.getInfoLog());
        }

        if (!program.buildReflection()) {
            throw EffectBakerException("Failed to build reflection shader program", fname, program.getInfoLog());
        }

        string program_info;
        program_info.reserve(1024);
        program_info = "[EffectInfo]\n";

        for (int32_t i = 0; i < program.getNumUniformVariables(); i++) {
            const auto& uniform = program.getUniform(i);
            if (uniform.getType()->getBasicType() == glslang::EbtSampler) {
                ptr<const char> uniform_name = uniform.name.c_str();
                program_info += strex("{} = {}\n", uniform.name, program.getUniformBinding(program.getReflectionIndex(uniform_name.get())));

#define CHECK_TEX(tex_name) \
    if (uniform.name == tex_name) { \
        continue; \
    }
                CHECK_TEX("MainTex");
                CHECK_TEX("IndoorMaskTex");
#if FO_ENABLE_3D
                for (size_t j = 0; j < MODEL_MAX_TEXTURES; j++) {
                    CHECK_TEX(strex("ModelTex{}", j).strv());
                }
#endif
#undef CHECK_TEX
            }
        }

        for (int32_t i = 0; i < program.getNumUniformBlocks(); i++) {
            const auto& uniform_block = program.getUniformBlock(i);
            ptr<const char> uniform_block_name = uniform_block.name.c_str();
            program_info += strex("{} = {}\n", uniform_block.name, program.getUniformBlockBinding(program.getReflectionIndex(uniform_block_name.get())));

#define CHECK_BUF(buf) \
    if (uniform_block.name == #buf) { \
        if (uniform_block.size != sizeof(RenderEffect::buf##fer)) { \
            throw EffectBakerException("Invalid uniform buffer size", fname, #buf, uniform_block.size, sizeof(RenderEffect::buf##fer)); \
        } \
        continue; \
    }
            CHECK_BUF(ProjBuf);
            CHECK_BUF(MainTexBuf);
            CHECK_BUF(EggBuf);
            CHECK_BUF(SpriteBorderBuf);
            CHECK_BUF(TimeBuf);
            CHECK_BUF(RandomValueBuf);
            CHECK_BUF(ScriptValueBuf);
            CHECK_BUF(CameraBuf);
#if FO_ENABLE_3D
            CHECK_BUF(ModelBuf);
            CHECK_BUF(ModelTexBuf);
            CHECK_BUF(ModelAnimBuf);
#endif
#undef CHECK_BUF

            throw EffectBakerException("Invalid uniform buffer", fname, uniform_block.name, uniform_block.size);
        }

        if (program.getNumBufferBlocks() != 0) {
            throw EffectBakerException("Storage buffers are not supported in effects", fname, program.getBufferBlock(0).name);
        }

        // Per-stage SDL_GPU descriptor slots for the SDL_GPU backend (see the flavor note at the top of this file).
        const SdlStageSlots vert_sdl_slots = AssignSdlStageSlots(program, EShLangVertex, fname);
        const SdlStageSlots frag_sdl_slots = AssignSdlStageSlots(program, EShLangFragment, fname);

        program_info += "\n[EffectInfoSdl]\n";
        program_info += strex("VertexSamplers = {}\n", vert_sdl_slots.Samplers.size());
        program_info += strex("VertexUniformBuffers = {}\n", vert_sdl_slots.UniformBufs.size());
        program_info += strex("FragmentSamplers = {}\n", frag_sdl_slots.Samplers.size());
        program_info += strex("FragmentUniformBuffers = {}\n", frag_sdl_slots.UniformBufs.size());

        for (size_t i = 0; i < vert_sdl_slots.Samplers.size(); i++) {
            program_info += strex("Vert{} = {}\n", vert_sdl_slots.Samplers[i].first, i);
        }
        for (size_t i = 0; i < vert_sdl_slots.UniformBufs.size(); i++) {
            program_info += strex("Vert{} = {}\n", vert_sdl_slots.UniformBufs[i].first, i);
        }
        for (size_t i = 0; i < frag_sdl_slots.Samplers.size(); i++) {
            program_info += strex("Frag{} = {}\n", frag_sdl_slots.Samplers[i].first, i);
        }
        for (size_t i = 0; i < frag_sdl_slots.UniformBufs.size(); i++) {
            program_info += strex("Frag{} = {}\n", frag_sdl_slots.UniformBufs[i].first, i);
        }

        const string_view fname_wo_ext = strvex(fname).erase_file_extension();
        const nptr<const glslang::TIntermediate> vertex_intermediate = program.getIntermediate(EShLangVertex);
        const nptr<const glslang::TIntermediate> fragment_intermediate = program.getIntermediate(EShLangFragment);
        FO_VERIFY_AND_THROW(vertex_intermediate, "Linked program has no vertex shader intermediate");
        FO_VERIFY_AND_THROW(fragment_intermediate, "Linked program has no fragment shader intermediate");

        BakeShaderStage(strex("{}.fofx-{}-vert", fname_wo_ext, pass), *vertex_intermediate, vert_sdl_slots, true);
        BakeShaderStage(strex("{}.fofx-{}-frag", fname_wo_ext, pass), *fragment_intermediate, frag_sdl_slots, false);

        _context->WriteData(strex("{}.fofx-{}-info", fname_wo_ext, pass), vector<uint8_t>(program_info.begin(), program_info.end()));
    }

    _context->WriteData(fname, vector<uint8_t>(content.begin(), content.end()));
}

void EffectBaker::BakeShaderStage(string_view fname_wo_ext, const glslang::TIntermediate& intermediate, const SdlStageSlots& sdl_slots, bool is_vertex) const
{
    FO_STACK_TRACE_ENTRY();

    glslang::SpvOptions spv_options;
    spv_options.generateDebugInfo = FO_DEBUG;
    spv_options.disableOptimizer = FO_DEBUG;
    spv_options.optimizeSize = !FO_DEBUG;
    spv_options.disassemble = FO_DEBUG;
    spv_options.validate = true;

    // Native Vulkan-1.0 SPIR-V (set 0 = uniform buffers, set 1 = samplers) consumed by Rendering-Vulkan and the
    // GLSL / GLSL ES / HLSL cross-compilation. Left untouched.
    std::vector<uint32_t> spirv;
    spv::SpvBuildLogger logger;
    GlslangToSpv(intermediate, spirv, &logger, &spv_options);

    // Same SPIR-V remapped to the SDL_GPU descriptor convention, consumed by the SDL_GPU backend (Vulkan driver)
    // and the SDL-remapped MSL (Metal driver).
    std::vector<uint32_t> sdl_spirv = spirv;
    PatchSpirvForSdlGpu(sdl_spirv, sdl_slots, is_vertex, fname_wo_ext);

    // SPIR-V (native Vulkan renderer)
    auto make_spirv = [this, &fname_wo_ext, &spirv]() {
        vector<uint8_t> data(spirv.size() * sizeof(uint32_t));

        if (!data.empty()) {
            MemCopy(data.data(), spirv.data(), data.size());
        }

        _context->WriteData(strex("{}-spv", fname_wo_ext), data);
    };

    // SPIR-V (SDL_GPU Vulkan driver)
    auto make_spirv_sdl = [this, &fname_wo_ext, &sdl_spirv]() {
        vector<uint8_t> data(sdl_spirv.size() * sizeof(uint32_t));

        if (!data.empty()) {
            MemCopy(data.data(), sdl_spirv.data(), data.size());
        }

        _context->WriteData(strex("{}-spv_sdl", fname_wo_ext), data);
    };

    // SPIR-V to GLSL
    auto make_glsl = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerGLSL compiler {spirv};
        auto options = compiler.get_common_options();
        options.es = false;
        options.version = 330;
        options.enable_420pack_extension = false;
        compiler.set_common_options(options);
        auto source = compiler.compile();
        _context->WriteData(strex("{}-glsl", fname_wo_ext), vector<uint8_t>(source.begin(), source.end()));
    };

    // SPIR-V to GLSL ES
    auto make_glsl_es = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerGLSL compiler {spirv};
        auto options = compiler.get_common_options();
        options.es = true;
        options.version = 300;
        options.enable_420pack_extension = false;
        compiler.set_common_options(options);
        auto source = compiler.compile();
        _context->WriteData(strex("{}-glsl_es", fname_wo_ext), vector<uint8_t>(source.begin(), source.end()));
    };

    // SPIR-V to HLSL
    auto make_hlsl = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerHLSL compiler {spirv};
        auto options = compiler.get_hlsl_options();
        options.shader_model = 40;
        compiler.set_hlsl_options(options);
        auto source = compiler.compile();
        _context->WriteData(strex("{}-hlsl", fname_wo_ext), vector<uint8_t>(source.begin(), source.end()));
    };

    // SPIR-V to Metal macOS (SDL_GPU Metal driver)
    auto make_msl_mac = [this, &fname_wo_ext, &sdl_spirv, &sdl_slots, is_vertex]() {
        spirv_cross::CompilerMSL compiler {sdl_spirv};
        auto options = compiler.get_msl_options();
        options.platform = spirv_cross::CompilerMSL::Options::macOS;
        compiler.set_msl_options(options);
        ApplySdlMslResourceBindings(compiler, sdl_slots, is_vertex);
        auto source = compiler.compile();
        _context->WriteData(strex("{}-msl_mac", fname_wo_ext), vector<uint8_t>(source.begin(), source.end()));
    };

    // SPIR-V to Metal iOS (SDL_GPU Metal driver)
    auto make_msl_ios = [this, &fname_wo_ext, &sdl_spirv, &sdl_slots, is_vertex]() {
        spirv_cross::CompilerMSL compiler {sdl_spirv};
        auto options = compiler.get_msl_options();
        options.platform = spirv_cross::CompilerMSL::Options::iOS;
        compiler.set_msl_options(options);
        ApplySdlMslResourceBindings(compiler, sdl_slots, is_vertex);
        auto source = compiler.compile();
        _context->WriteData(strex("{}-msl_ios", fname_wo_ext), vector<uint8_t>(source.begin(), source.end()));
    };

    // Make all asynchronously
    vector<std::future<void>> file_bakings;
    file_bakings.emplace_back(run_async(GetAsyncMode(), "BakeShader-spirv", make_spirv));
    file_bakings.emplace_back(run_async(GetAsyncMode(), "BakeShader-spirv_sdl", make_spirv_sdl));
    file_bakings.emplace_back(run_async(GetAsyncMode(), "BakeShader-glsl", make_glsl));
    file_bakings.emplace_back(run_async(GetAsyncMode(), "BakeShader-glsl_es", make_glsl_es));
    file_bakings.emplace_back(run_async(GetAsyncMode(), "BakeShader-hlsl", make_hlsl));
    file_bakings.emplace_back(run_async(GetAsyncMode(), "BakeShader-msl_mac", make_msl_mac));
    file_bakings.emplace_back(run_async(GetAsyncMode(), "BakeShader-msl_ios", make_msl_ios));

    for (auto& file_baking : file_bakings) {
        file_baking.get();
    }
}

static auto AssignSdlStageSlots(const glslang::TProgram& program, EShLanguage stage, string_view fname) -> EffectBaker::SdlStageSlots
{
    FO_STACK_TRACE_ENTRY();

    EffectBaker::SdlStageSlots slots;

    for (int32_t i = 0; i < program.getNumUniformVariables(); i++) {
        const auto& uniform = program.getUniform(i);

        if ((uniform.stages & (1 << stage)) == 0) {
            continue;
        }
        if (uniform.getType()->getBasicType() != glslang::EbtSampler) {
            continue;
        }
        if (uniform.getType()->getSampler().isImage()) {
            throw EffectBakerException("Storage images are not supported in effects", fname, uniform.name);
        }
        if (uniform.getBinding() < 0) {
            throw EffectBakerException("Sampler must have an explicit binding", fname, uniform.name);
        }

        slots.Samplers.emplace_back(uniform.name, uniform.getBinding());
    }

    for (int32_t i = 0; i < program.getNumUniformBlocks(); i++) {
        const auto& uniform_block = program.getUniformBlock(i);

        if ((uniform_block.stages & (1 << stage)) == 0) {
            continue;
        }
        if (uniform_block.getBinding() < 0) {
            throw EffectBakerException("Uniform buffer must have an explicit binding", fname, uniform_block.name);
        }

        slots.UniformBufs.emplace_back(uniform_block.name, uniform_block.getBinding());
    }

    const auto sort_and_check = [&fname](vector<pair<string, int32_t>>& resources, string_view resource_class, int32_t limit) {
        std::sort(resources.begin(), resources.end(), [](const pair<string, int32_t>& res1, const pair<string, int32_t>& res2) { return res1.second < res2.second; });

        for (size_t i = 1; i < resources.size(); i++) {
            if (resources[i].second == resources[i - 1].second) {
                throw EffectBakerException("Duplicate resource binding within one shader stage", fname, resource_class, resources[i - 1].first, resources[i].first);
            }
        }

        if (resources.size() > numeric_cast<size_t>(limit)) {
            throw EffectBakerException("Too many resources for one SDL_GPU shader stage", fname, resource_class, resources.size(), limit);
        }
    };

    sort_and_check(slots.Samplers, "sampler", SDLGPU_MAX_SAMPLERS_PER_STAGE);
    sort_and_check(slots.UniformBufs, "uniform buffer", SDLGPU_MAX_UNIFORM_BUFFERS_PER_STAGE);

    return slots;
}

static void PatchSpirvForSdlGpu(std::vector<uint32_t>& spirv, const EffectBaker::SdlStageSlots& sdl_slots, bool is_vertex, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    // SPIR-V binary layout constants (SPIR-V specification)
    constexpr uint32_t spirv_magic = 0x07230203;
    constexpr uint32_t op_variable = 59;
    constexpr uint32_t op_decorate = 71;
    constexpr uint32_t decoration_binding = 33;
    constexpr uint32_t decoration_descriptor_set = 34;
    constexpr uint32_t storage_class_uniform_constant = 0; // Sampled textures
    constexpr uint32_t storage_class_uniform = 2; // Uniform buffers (storage buffers are rejected at reflection time)
    constexpr size_t header_size = 5;

    FO_VERIFY_AND_THROW(spirv.size() > header_size && spirv[0] == spirv_magic, "Invalid SPIR-V module", fname);

    // First pass: collect descriptor resource variables (annotations precede variables in a module, so two passes are needed)
    unordered_map<uint32_t, uint32_t> var_storage_class;

    for (size_t pos = header_size; pos < spirv.size();) {
        const uint32_t word_count = spirv[pos] >> 16;
        const uint32_t opcode = spirv[pos] & 0xFFFF;
        FO_VERIFY_AND_THROW(word_count != 0 && pos + word_count <= spirv.size(), "Malformed SPIR-V instruction", fname);

        if (opcode == op_variable && word_count >= 4) {
            const uint32_t storage_class = spirv[pos + 3];

            if (storage_class == storage_class_uniform_constant || storage_class == storage_class_uniform) {
                var_storage_class.emplace(spirv[pos + 2], storage_class);
            }
        }

        pos += word_count;
    }

    unordered_map<uint32_t, uint32_t> sampler_slots;
    unordered_map<uint32_t, uint32_t> uniform_buf_slots;

    for (size_t i = 0; i < sdl_slots.Samplers.size(); i++) {
        sampler_slots.emplace(numeric_cast<uint32_t>(sdl_slots.Samplers[i].second), numeric_cast<uint32_t>(i));
    }
    for (size_t i = 0; i < sdl_slots.UniformBufs.size(); i++) {
        uniform_buf_slots.emplace(numeric_cast<uint32_t>(sdl_slots.UniformBufs[i].second), numeric_cast<uint32_t>(i));
    }

    const uint32_t sampler_set = is_vertex ? 0 : 2;
    const uint32_t uniform_buf_set = is_vertex ? 1 : 3;

    // Second pass: rewrite DescriptorSet / Binding decoration literals to the SDL_GPU convention
    size_t patched_bindings = 0;
    size_t patched_sets = 0;

    for (size_t pos = header_size; pos < spirv.size();) {
        const uint32_t word_count = spirv[pos] >> 16;
        const uint32_t opcode = spirv[pos] & 0xFFFF;

        if (opcode == op_decorate && word_count == 4) {
            const auto var_it = var_storage_class.find(spirv[pos + 1]);

            if (var_it != var_storage_class.end()) {
                const bool is_sampler = var_it->second == storage_class_uniform_constant;
                const uint32_t decoration = spirv[pos + 2];

                if (decoration == decoration_binding) {
                    const auto& slot_map = is_sampler ? sampler_slots : uniform_buf_slots;
                    const auto slot_it = slot_map.find(spirv[pos + 3]);

                    if (slot_it == slot_map.end()) {
                        throw EffectBakerException("Shader declares an unused resource, remove the dead declaration from the effect source", fname, is_sampler ? "sampler" : "uniform buffer", spirv[pos + 3]);
                    }

                    spirv[pos + 3] = slot_it->second;
                    patched_bindings++;
                }
                else if (decoration == decoration_descriptor_set) {
                    spirv[pos + 3] = is_sampler ? sampler_set : uniform_buf_set;
                    patched_sets++;
                }
            }
        }

        pos += word_count;
    }

    const size_t expected_patches = sdl_slots.Samplers.size() + sdl_slots.UniformBufs.size();

    if (patched_bindings != expected_patches || patched_sets != expected_patches) {
        throw EffectBakerException("SPIR-V descriptor decoration patching mismatch", fname, patched_bindings, patched_sets, expected_patches);
    }
}

static void ApplySdlMslResourceBindings(spirv_cross::CompilerMSL& compiler, const EffectBaker::SdlStageSlots& sdl_slots, bool is_vertex)
{
    FO_STACK_TRACE_ENTRY();

    // Match the SDL_GPU Metal convention: uniform buffers at [[buffer(slot)]], sampled textures at [[texture(slot)]] + [[sampler(slot)]]
    // (vertex buffers are bound by SDL starting at [[buffer(14)]] and flow through [[stage_in]], so they never collide with uniform slots)
    const auto stage = is_vertex ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;

    for (size_t i = 0; i < sdl_slots.Samplers.size(); i++) {
        spirv_cross::MSLResourceBinding binding;
        binding.stage = stage;
        binding.desc_set = is_vertex ? 0 : 2;
        binding.binding = numeric_cast<uint32_t>(i);
        binding.count = 1;
        binding.msl_texture = numeric_cast<uint32_t>(i);
        binding.msl_sampler = numeric_cast<uint32_t>(i);
        compiler.add_msl_resource_binding(binding);
    }

    for (size_t i = 0; i < sdl_slots.UniformBufs.size(); i++) {
        spirv_cross::MSLResourceBinding binding;
        binding.stage = stage;
        binding.desc_set = is_vertex ? 1 : 3;
        binding.binding = numeric_cast<uint32_t>(i);
        binding.count = 1;
        binding.msl_buffer = numeric_cast<uint32_t>(i);
        compiler.add_msl_resource_binding(binding);
    }
}

FO_END_NAMESPACE
