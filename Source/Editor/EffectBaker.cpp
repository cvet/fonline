#include "EffectBaker.h"
#include "FileUtils.h"
#include "Log.h"
#include "StringUtils.h"
#include "Testing.h"

#include "GlslangToSpv.h"
#include "ShaderLang.h"

#include "spirv_glsl.hpp"
#include "spirv_hlsl.hpp"
#include "spirv_msl.hpp"

EffectBaker::EffectBaker(FileCollection& all_files) : allFiles {all_files}
{
    glslang::InitializeProcess();
}

EffectBaker::~EffectBaker()
{
    glslang::FinalizeProcess();
}

void EffectBaker::AutoBakeEffects()
{
    vector<future<void>> futs;

    allFiles.ResetCounter();
    while (allFiles.IsNextFile())
    {
        string relative_path;
        allFiles.GetNextFile(nullptr, nullptr, &relative_path, true);

        {
            SCOPE_LOCK(bakedFilesLocker);
            if (bakedFiles.count(relative_path))
                continue;
        }

        string ext = _str(relative_path).getFileExtension();
        if (ext != "glsl")
            continue;

        File& file = allFiles.GetCurFile();
        string content(file.GetCStr(), file.GetFsize());
        futs.emplace_back(std::async(&EffectBaker::BakeShaderProgram, this, relative_path, content));
    }

    for (auto& fut : futs)
        fut.wait();
}

void EffectBaker::BakeShaderProgram(const string& fname, const string& content)
{
    string fname_wo_ext = _str(fname).eraseFileExtension();

    glslang::TShader vert(EShLangVertex);
    vert.setEnvInput(glslang::EShSourceGlsl, EShLangVertex, glslang::EShClientOpenGL, 100);
    vert.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    vert.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_4);
    // vert.setStrings()
    // vert.preprocess
    // vert.parse()

    glslang::TShader frag(EShLangFragment);
    frag.setEnvInput(glslang::EShSourceGlsl, EShLangFragment, glslang::EShClientOpenGL, 100);
    frag.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    frag.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_4);
    // if (!frag.parse())

    glslang::TProgram program;
    program.addShader(&vert);
    program.addShader(&frag);
    if (!program.link(EShMsgDefault))
        throw fo_exception("Can't link shader program", fname, program.getInfoLog());

    BakeShaderStage(fname_wo_ext + ".vert", program.getIntermediate(EShLangVertex));
    BakeShaderStage(fname_wo_ext + ".frag", program.getIntermediate(EShLangFragment));

    {
        SCOPE_LOCK(bakedFilesLocker);
        string dummy_content = "BAKED";
        bakedFiles.emplace(fname, UCharVec(dummy_content.begin(), dummy_content.end()));
    }
}

void EffectBaker::BakeShaderStage(const string& fname_wo_ext, glslang::TIntermediate* intermediate)
{
    // Glslang to SPIR-V
    std::vector<uint32_t> spirv;

    glslang::SpvOptions options;
    // options.generateDebugInfo = true;
    options.disableOptimizer = false;
    options.optimizeSize = true;
    options.disassemble = false;
    options.validate = true;

    spv::SpvBuildLogger logger;
    glslang::GlslangToSpv(*intermediate, spirv, &logger, &options);

    // SPIR-V
    auto make_spirv = [this, &fname_wo_ext, &spirv]() {
        UCharVec data(spirv.size() * sizeof(uint32_t));
        memcpy(&data[0], &spirv[0], data.size());
        SCOPE_LOCK(bakedFilesLocker);
        bakedFiles.emplace(fname_wo_ext + ".spv", std::move(data));
    };

    // SPIR-V to GLSL
    auto make_glsl = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerGLSL compiler {spirv};
        auto options = compiler.get_common_options();
        compiler.set_common_options(options);
        std::string source = compiler.compile();
        SCOPE_LOCK(bakedFilesLocker);
        bakedFiles.emplace(fname_wo_ext + ".glsl", UCharVec(source.begin(), source.end()));
    };

    // SPIR-V to GLSL ES
    auto make_glsl_es = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerGLSL compiler {spirv};
        auto options = compiler.get_common_options();
        options.es = true;
        compiler.set_common_options(options);
        std::string source = compiler.compile();
        SCOPE_LOCK(bakedFilesLocker);
        bakedFiles.emplace(fname_wo_ext + ".glsl-es", UCharVec(source.begin(), source.end()));
    };

    // SPIR-V to HLSL
    auto make_hlsl = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerHLSL compiler {spirv};
        auto options = compiler.get_hlsl_options();
        compiler.set_hlsl_options(options);
        std::string source = compiler.compile();
        SCOPE_LOCK(bakedFilesLocker);
        bakedFiles.emplace(fname_wo_ext + ".hlsl", UCharVec(source.begin(), source.end()));
    };

    // SPIR-V to Metal macOS
    auto make_msl_mac = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerMSL compiler {spirv};
        auto options = compiler.get_msl_options();
        options.platform = spirv_cross::CompilerMSL::Options::macOS;
        compiler.set_msl_options(options);
        std::string source = compiler.compile();
        SCOPE_LOCK(bakedFilesLocker);
        bakedFiles.emplace(fname_wo_ext + ".msl-mac", UCharVec(source.begin(), source.end()));
    };

    // SPIR-V to Metal iOS
    auto make_msl_ios = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerMSL compiler {spirv};
        spirv_cross::CompilerMSL::Options options;
        options.platform = spirv_cross::CompilerMSL::Options::iOS;
        compiler.set_msl_options(options);
        std::string source = compiler.compile();
        SCOPE_LOCK(bakedFilesLocker);
        bakedFiles.emplace(fname_wo_ext + ".msl-ios", UCharVec(source.begin(), source.end()));
    };

    // Make all asynchronously
    auto futs = {
        std::async(make_spirv),
        std::async(make_glsl),
        std::async(make_glsl_es),
        std::async(make_hlsl),
        std::async(make_msl_mac),
        std::async(make_msl_ios),
    };
    for (auto& fut : futs)
        fut.wait();
}

void EffectBaker::FillBakedFiles(map<string, UCharVec>& baked_files)
{
    SCOPE_LOCK(bakedFilesLocker);

    for (const auto& kv : bakedFiles)
        baked_files.emplace(kv.first, kv.second);
}
