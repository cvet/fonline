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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: pre-compile HLSH shaders with D3DCompile

#include "EffectBaker.h"
#include "Application.h"
#include "ConfigFile.h"
#include "Log.h"
#include "StringUtils.h"

#include "GlslangToSpv.h"
#include "ShaderLang.h"
#include "spirv_glsl.hpp"
#include "spirv_hlsl.hpp"
#include "spirv_msl.hpp"

constexpr TBuiltInResource GLSLANG_BUILT_IN_RESOURCE = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,
    /* .maxMeshOutputVerticesEXT = */ 256,
    /* .maxMeshOutputPrimitivesEXT = */ 256,
    /* .maxMeshWorkGroupSizeX_EXT = */ 128,
    /* .maxMeshWorkGroupSizeY_EXT = */ 128,
    /* .maxMeshWorkGroupSizeZ_EXT = */ 128,
    /* .maxTaskWorkGroupSizeX_EXT = */ 128,
    /* .maxTaskWorkGroupSizeY_EXT = */ 128,
    /* .maxTaskWorkGroupSizeZ_EXT = */ 128,
    /* .maxMeshViewCountEXT = */ 4,
    /* .maxDualSourceDrawBuffersEXT = */ 1,

    /* .limits = */
    {
        /* .nonInductiveForLoops = */ true,
        /* .whileLoops = */ true,
        /* .doWhileLoops = */ true,
        /* .generalUniformIndexing = */ true,
        /* .generalAttributeMatrixVectorIndexing = */ true,
        /* .generalVaryingIndexing = */ true,
        /* .generalSamplerIndexing = */ true,
        /* .generalVariableIndexing = */ true,
        /* .generalConstantMatrixVectorIndexing = */ true,
    }};

EffectBaker::EffectBaker(BakerSettings& settings, FileCollection files, BakeCheckerCallback bake_checker, WriteDataCallback write_data) :
    BaseBaker(settings, std::move(files), std::move(bake_checker), std::move(write_data))
{
    STACK_TRACE_ENTRY();

    glslang::InitializeProcess();
}

EffectBaker::~EffectBaker()
{
    STACK_TRACE_ENTRY();

    glslang::FinalizeProcess();
}

void EffectBaker::AutoBake()
{
    STACK_TRACE_ENTRY();

    _errors = 0;

#if FO_ASYNC_BAKE
    vector<std::future<void>> futs;
#endif

    _files.ResetCounter();
    while (_files.MoveNext()) {
        auto file_header = _files.GetCurFileHeader();

        string ext = _str(file_header.GetPath()).getFileExtension();
        if (ext != "fofx") {
            continue;
        }

#if !FO_ENABLE_3D
        if (file_header.GetPath().find("3D") != string::npos) {
            continue;
        }
#endif

        {
#if FO_ASYNC_BAKE
            std::lock_guard locker(_bakedFilesLocker);
#endif
            if (_bakeChecker && !_bakeChecker(file_header)) {
                continue;
            }
        }

        auto file = _files.GetCurFile();
        string content = file.GetStr();

#if FO_ASYNC_BAKE
        futs.emplace_back(std::async(std::launch::async | std::launch::deferred, &EffectBaker::BakeShaderProgram, this, relative_path, content));
#else
        try {
            BakeShaderProgram(file_header.GetPath(), content);
        }
        catch (const EffectBakerException& ex) {
            ReportExceptionAndContinue(ex);
            _errors++;
        }
        catch (const FileSystemExeption& ex) {
            ReportExceptionAndContinue(ex);
            _errors++;
        }
#endif
    }

#if FO_ASYNC_BAKE
    for (auto& fut : futs) {
        fut.wait();
    }
#endif

    if (_errors > 0) {
        throw EffectBakerException("Errors during effects bakering", _errors);
    }
}

void EffectBaker::BakeShaderProgram(string_view fname, string_view content)
{
    STACK_TRACE_ENTRY();

    auto fofx = ConfigFile(fname, string(content), nullptr, ConfigFileOption::CollectContent);
    if (!fofx.HasSection("Effect")) {
        throw EffectBakerException(".fofx file truncated", fname);
    }

    constexpr bool old_code_profile = false;

    const auto passes = fofx.GetInt("Effect", "Passes", 1);
    const auto shader_common_content = fofx.GetSectionContent("ShaderCommon");
    const auto shader_version = fofx.GetInt("Effect", "Version", 310);
    const auto shader_version_str = _str("#version {} es\n", shader_version).str();
#if FO_ENABLE_3D
    const auto shader_defines = _str("precision mediump float;\n#define MAX_BONES {}\n#define MAX_TEXTURES {}\n", MODEL_MAX_BONES, MODEL_MAX_TEXTURES).str();
#else
    const auto shader_defines = _str("precision mediump float;\n").str();
#endif
    const auto* shader_defines_ex = old_code_profile ? "#define layout(x)\n#define in attribute\n#define out varying\n#define texture texture2D\n#define FragColor gl_FragColor" : "";
    const auto shader_defines_ex2 = _str("#define MAX_SCRIPT_VALUES {}\n", EFFECT_SCRIPT_VALUES).str();

    for (auto pass = 1; pass <= passes; pass++) {
        string vertex_pass_content = fofx.GetSectionContent(_str("VertexShader Pass{}", pass));
        if (vertex_pass_content.empty()) {
            vertex_pass_content = fofx.GetSectionContent("VertexShader");
        }
        if (vertex_pass_content.empty()) {
            throw EffectBakerException("No content for vertex shader", fname, pass);
        }

        string fragment_pass_content = fofx.GetSectionContent(_str("FragmentShader Pass{}", pass));
        if (fragment_pass_content.empty()) {
            fragment_pass_content = fofx.GetSectionContent("FragmentShader");
        }
        if (fragment_pass_content.empty()) {
            throw EffectBakerException("No content for fragment shader", fname, pass);
        }

        glslang::TShader vert(EShLangVertex);
        vert.setEnvInput(glslang::EShSourceGlsl, EShLangVertex, glslang::EShClientNone, shader_version);
        vert.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
        vert.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
        const char* vertext_strings[] = {shader_version_str.c_str(), shader_defines.c_str(), shader_defines_ex, shader_defines_ex2.c_str(), shader_common_content.c_str(), vertex_pass_content.c_str()};
        vert.setStrings(vertext_strings, 6);
        // vert.setAutoMapBindings(true); // Todo: enable auto map bindings
        if (!vert.parse(&GLSLANG_BUILT_IN_RESOURCE, shader_version, true, EShMessages::EShMsgDefault)) {
            throw EffectBakerException("Failed to parse vertex shader", fname, pass, vert.getInfoLog());
        }

        glslang::TShader frag(EShLangFragment);
        frag.setEnvInput(glslang::EShSourceGlsl, EShLangFragment, glslang::EShClientNone, shader_version);
        frag.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
        frag.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
        const char* fragment_strings[] = {shader_version_str.c_str(), shader_defines.c_str(), shader_defines_ex, shader_defines_ex2.c_str(), shader_common_content.c_str(), fragment_pass_content.c_str()};
        frag.setStrings(fragment_strings, 6);
        // frag.setAutoMapBindings(true);
        if (!frag.parse(&GLSLANG_BUILT_IN_RESOURCE, shader_version, true, EShMessages::EShMsgDefault)) {
            throw EffectBakerException("Failed to parse fragment shader", fname, pass, frag.getInfoLog());
        }

        glslang::TProgram program;
        program.addShader(&vert);
        program.addShader(&frag);

        if (!program.link(EShMsgDefault)) {
            throw EffectBakerException("Failed to link shader program", fname, program.getInfoLog());
        }

        // if (!program.mapIO()) {
        //    throw EffectBakerException("Failed to map IO shader program", fname, program.getInfoLog());
        // }

        if (!program.buildReflection()) {
            throw EffectBakerException("Failed to build reflection shader program", fname, program.getInfoLog());
        }

        string program_info;
        program_info.reserve(1024);
        program_info = "[EffectInfo]\n";

        for (int i = 0; i < program.getNumUniformVariables(); i++) {
            const auto& uniform = program.getUniform(i);
            if (uniform.getType()->getBasicType() == glslang::EbtSampler) {
                program_info += _str("{} = {}\n", uniform.name, program.getUniformBinding(program.getReflectionIndex(uniform.name.c_str())));

#define CHECK_TEX(tex_name) \
    if (uniform.name == tex_name) { \
        continue; \
    }
                CHECK_TEX("MainTex");
                CHECK_TEX("EggTex");
#if FO_ENABLE_3D
                for (size_t j = 0; j < MODEL_MAX_TEXTURES; j++) {
                    CHECK_TEX(_str("ModelTex{}", j).str());
                }
#endif
#undef CHECK_TEX
            }
        }

        for (int i = 0; i < program.getNumUniformBlocks(); i++) {
            const auto& uniform_block = program.getUniformBlock(i);
            program_info += _str("{} = {}\n", uniform_block.name, program.getUniformBlockBinding(program.getReflectionIndex(uniform_block.name.c_str())));

#define CHECK_BUF(buf) \
    if (uniform_block.name == #buf) { \
        if (uniform_block.size != sizeof(RenderEffect::buf##fer)) { \
            throw EffectBakerException("Invalid uniform buffer size", fname, #buf, uniform_block.size, sizeof(RenderEffect::buf##fer)); \
        } \
        continue; \
    }
            CHECK_BUF(ProjBuf);
            CHECK_BUF(MainTexBuf);
            CHECK_BUF(ContourBuf);
            CHECK_BUF(TimeBuf);
            CHECK_BUF(RandomValueBuf);
            CHECK_BUF(ScriptValueBuf);
#if FO_ENABLE_3D
            CHECK_BUF(ModelBuf);
            CHECK_BUF(ModelTexBuf);
            CHECK_BUF(ModelAnimBuf);
#endif
#undef CHECK_BUF

            throw EffectBakerException("Invalid uniform buffer", fname, uniform_block.name, uniform_block.size);
        }

        const string fname_wo_ext = _str(fname).eraseFileExtension();
        BakeShaderStage(_str("{}.{}.vert", fname_wo_ext, pass), program.getIntermediate(EShLangVertex));
        BakeShaderStage(_str("{}.{}.frag", fname_wo_ext, pass), program.getIntermediate(EShLangFragment));

        {
#if FO_ASYNC_BAKE
            std::lock_guard locker(_bakedFilesLocker);
#endif
            _writeData(_str("{}.{}.info", fname_wo_ext, pass), vector<uchar>(program_info.begin(), program_info.end()));
        }
    }

    {
#if FO_ASYNC_BAKE
        std::lock_guard locker(_bakedFilesLocker);
#endif
        _writeData(fname, vector<uchar>(content.begin(), content.end()));
    }
}

void EffectBaker::BakeShaderStage(string_view fname_wo_ext, const glslang::TIntermediate* intermediate)
{
    STACK_TRACE_ENTRY();

    // Glslang to SPIR-V
    std::vector<uint32_t> spirv;

    glslang::SpvOptions spv_options;
    // spv_options.generateDebugInfo = true;
    spv_options.disableOptimizer = false;
    spv_options.optimizeSize = true;
    spv_options.disassemble = false;
    spv_options.validate = true;

    spv::SpvBuildLogger logger;
    GlslangToSpv(*intermediate, spirv, &logger, &spv_options);

    // SPIR-V
    auto make_spirv = [this, &fname_wo_ext, &spirv]() {
        vector<uchar> data(spirv.size() * sizeof(uint32_t));
        std::memcpy(data.data(), spirv.data(), data.size());
#if FO_ASYNC_BAKE
        std::lock_guard locker(_bakedFilesLocker);
#endif
        _writeData(_str("{}.spv", fname_wo_ext), data);
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
#if FO_ASYNC_BAKE
        std::lock_guard locker(_bakedFilesLocker);
#endif
        _writeData(_str("{}.glsl", fname_wo_ext), vector<uchar>(source.begin(), source.end()));
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
#if FO_ASYNC_BAKE
        std::lock_guard locker(_bakedFilesLocker);
#endif
        _writeData(_str("{}.glsl-es", fname_wo_ext), vector<uchar>(source.begin(), source.end()));
    };

    // SPIR-V to HLSL
    auto make_hlsl = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerHLSL compiler {spirv};
        auto options = compiler.get_hlsl_options();
        options.shader_model = 40;
        compiler.set_hlsl_options(options);
        auto source = compiler.compile();
#if FO_ASYNC_BAKE
        std::lock_guard locker(_bakedFilesLocker);
#endif
        _writeData(_str("{}.hlsl", fname_wo_ext), vector<uchar>(source.begin(), source.end()));
    };

    // SPIR-V to Metal macOS
    auto make_msl_mac = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerMSL compiler {spirv};
        auto options = compiler.get_msl_options();
        options.platform = spirv_cross::CompilerMSL::Options::macOS;
        compiler.set_msl_options(options);
        auto source = compiler.compile();
#if FO_ASYNC_BAKE
        std::lock_guard locker(_bakedFilesLocker);
#endif
        _writeData(_str("{}.msl-mac", fname_wo_ext), vector<uchar>(source.begin(), source.end()));
    };

    // SPIR-V to Metal iOS
    auto make_msl_ios = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerMSL compiler {spirv};
        spirv_cross::CompilerMSL::Options options;
        options.platform = spirv_cross::CompilerMSL::Options::iOS;
        compiler.set_msl_options(options);
        auto source = compiler.compile();
#if FO_ASYNC_BAKE
        std::lock_guard locker(_bakedFilesLocker);
#endif
        _writeData(_str("{}.msl-ios", fname_wo_ext), vector<uchar>(source.begin(), source.end()));
    };

#if FO_ASYNC_BAKE
    // Make all asynchronously
    auto futs = {
        std::async(std::launch::async | std::launch::deferred, make_spirv),
        std::async(std::launch::async | std::launch::deferred, make_glsl),
        std::async(std::launch::async | std::launch::deferred, make_glsl_es),
        std::async(std::launch::async | std::launch::deferred, make_hlsl),
        std::async(std::launch::async | std::launch::deferred, make_msl_mac),
        std::async(std::launch::async | std::launch::deferred, make_msl_ios),
    };
    for (const auto& fut : futs) {
        fut.wait();
    }
#else
    make_spirv();
    make_glsl();
    make_glsl_es();
    make_hlsl();
    make_msl_mac();
    make_msl_ios();
#endif
}
