//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Log.h"
#include "StringUtils.h"

#include "GlslangToSpv.h"
#include "ShaderLang.h"
#include "spirv_glsl.hpp"
#include "spirv_hlsl.hpp"
#include "spirv_msl.hpp"

EffectBaker::EffectBaker(FileCollection& all_files) : _allFiles {all_files}
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

    _allFiles.ResetCounter();
    while (_allFiles.MoveNext()) {
        auto file_header = _allFiles.GetCurFileHeader();
        auto relative_path = file_header.GetPath().substr(_allFiles.GetPath().length());

        {
            SCOPE_LOCK(_bakedFilesLocker);
            if (_bakedFiles.count(relative_path) != 0u) {
                continue;
            }
        }

        string ext = _str(relative_path).getFileExtension();
        if (ext != "glsl") {
            continue;
        }

        auto file = _allFiles.GetCurFile();
        string content(file.GetCStr(), file.GetFsize());
        futs.emplace_back(std::async(std::launch::async | std::launch::deferred, &EffectBaker::BakeShaderProgram, this, relative_path, content));
    }

    for (auto& fut : futs) {
        fut.wait();
    }
}

void EffectBaker::BakeShaderProgram(const string& fname, const string& /*content*/)
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
    if (!program.link(EShMsgDefault)) {
        throw GenericException("Can't link shader program", fname, program.getInfoLog());
    }

    BakeShaderStage(fname_wo_ext + ".vert", program.getIntermediate(EShLangVertex));
    BakeShaderStage(fname_wo_ext + ".frag", program.getIntermediate(EShLangFragment));

    {
        SCOPE_LOCK(_bakedFilesLocker);
        string dummy_content = "BAKED";
        _bakedFiles.emplace(fname, vector<uchar>(dummy_content.begin(), dummy_content.end()));
    }
}

void EffectBaker::BakeShaderStage(const string& fname_wo_ext, glslang::TIntermediate* intermediate)
{
    // Glslang to SPIR-V
    std::vector<uint32_t> spirv;

    glslang::SpvOptions spv_options;
    // options.generateDebugInfo = true;
    spv_options.disableOptimizer = false;
    spv_options.optimizeSize = true;
    spv_options.disassemble = false;
    spv_options.validate = true;

    spv::SpvBuildLogger logger;
    GlslangToSpv(*intermediate, spirv, &logger, &spv_options);

    // SPIR-V
    auto make_spirv = [this, &fname_wo_ext, &spirv]() {
        vector<uchar> data(spirv.size() * sizeof(uint32_t));
        std::memcpy(&data[0], &spirv[0], data.size());
        SCOPE_LOCK(_bakedFilesLocker);
        _bakedFiles.emplace(fname_wo_ext + ".spv", std::move(data));
    };

    // SPIR-V to GLSL
    auto make_glsl = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerGLSL compiler {spirv};
        const auto& options = compiler.get_common_options();
        compiler.set_common_options(options);
        auto source = compiler.compile();
        SCOPE_LOCK(_bakedFilesLocker);
        _bakedFiles.emplace(fname_wo_ext + ".glsl", vector<uchar>(source.begin(), source.end()));
    };

    // SPIR-V to GLSL ES
    auto make_glsl_es = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerGLSL compiler {spirv};
        auto options = compiler.get_common_options();
        options.es = true;
        compiler.set_common_options(options);
        auto source = compiler.compile();
        SCOPE_LOCK(_bakedFilesLocker);
        _bakedFiles.emplace(fname_wo_ext + ".glsl-es", vector<uchar>(source.begin(), source.end()));
    };

    // SPIR-V to HLSL
    auto make_hlsl = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerHLSL compiler {spirv};
        const auto& options = compiler.get_hlsl_options();
        compiler.set_hlsl_options(options);
        auto source = compiler.compile();
        SCOPE_LOCK(_bakedFilesLocker);
        _bakedFiles.emplace(fname_wo_ext + ".hlsl", vector<uchar>(source.begin(), source.end()));
    };

    // SPIR-V to Metal macOS
    auto make_msl_mac = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerMSL compiler {spirv};
        auto options = compiler.get_msl_options();
        options.platform = spirv_cross::CompilerMSL::Options::macOS;
        compiler.set_msl_options(options);
        auto source = compiler.compile();
        SCOPE_LOCK(_bakedFilesLocker);
        _bakedFiles.emplace(fname_wo_ext + ".msl-mac", vector<uchar>(source.begin(), source.end()));
    };

    // SPIR-V to Metal iOS
    auto make_msl_ios = [this, &fname_wo_ext, &spirv]() {
        spirv_cross::CompilerMSL compiler {spirv};
        spirv_cross::CompilerMSL::Options options;
        options.platform = spirv_cross::CompilerMSL::Options::iOS;
        compiler.set_msl_options(options);
        auto source = compiler.compile();
        SCOPE_LOCK(_bakedFilesLocker);
        _bakedFiles.emplace(fname_wo_ext + ".msl-ios", vector<uchar>(source.begin(), source.end()));
    };

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
}

void EffectBaker::FillBakedFiles(map<string, vector<uchar>>& baked_files)
{
    SCOPE_LOCK(_bakedFilesLocker);

    for (const auto& [name, data] : _bakedFiles) {
        baked_files.emplace(name, data);
    }
}
