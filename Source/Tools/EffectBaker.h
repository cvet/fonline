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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"

DECLARE_EXCEPTION(EffectBakerException);

namespace glslang
{
    class TIntermediate;
}

class EffectBaker final : public BaseBaker
{
public:
    EffectBaker() = delete;
    EffectBaker(const BakerSettings& settings, BakeCheckerCallback bake_checker, WriteDataCallback write_data);
    EffectBaker(const EffectBaker&) = delete;
    EffectBaker(EffectBaker&&) noexcept = delete;
    auto operator=(const EffectBaker&) = delete;
    auto operator=(EffectBaker&&) noexcept = delete;
    ~EffectBaker() override;

    [[nodiscard]] auto IsExtSupported(string_view ext) const -> bool override { return ext == "fofx"; }

    void BakeFiles(FileCollection&& files) override;

private:
    void BakeShaderProgram(string_view fname, string_view content);
    void BakeShaderStage(string_view fname_wo_ext, const glslang::TIntermediate* intermediate);

    bool _nonConstHelper {};
};
