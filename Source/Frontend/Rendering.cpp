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

#include "Rendering.h"
#include "ConfigFile.h"
#include "StringUtils.h"

// clang-format off
RenderTexture::RenderTexture(uint width, uint height, bool linear_filtered, bool with_depth) :
    Width {width},
    Height {height},
    SizeData {static_cast<float>(width), static_cast<float>(height), 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(width)},
    LinearFiltered {linear_filtered},
    WithDepth {with_depth}
// clang-format on
{
}

RenderDrawBuffer::RenderDrawBuffer(bool is_static) : IsStatic {is_static}
{
}

RenderEffect::RenderEffect(EffectUsage usage, string_view name, string_view defines, const RenderEffectLoader& loader) : Name {name}, Usage {usage}
{
    const auto fname = _str("{}.fofx", name);
    const auto content = loader(fname);
    const auto fofx = ConfigFile(fname, content, nullptr, true);
    RUNTIME_ASSERT(fofx.HasSection("Effect"));

    const auto passes = fofx.GetInt("Effect", "Passes", 1);
    RUNTIME_ASSERT(passes >= 1);
    RUNTIME_ASSERT(passes <= EFFECT_MAX_PASSES);

#if FO_ENABLE_3D
    const auto shadow_pass = fofx.GetInt("Effect", "ShadowPass", -1);
    RUNTIME_ASSERT(shadow_pass == -1 || (shadow_pass >= 1 && shadow_pass <= EFFECT_MAX_PASSES));
    if (shadow_pass != -1) {
        _isShadow[shadow_pass - 1] = true;
    }
#endif

    _passCount = static_cast<size_t>(passes);

    static auto get_blend_func = [](string_view s) -> BlendFuncType {
#define CHECK_ENTRY(name) \
    if (s == #name) \
    return BlendFuncType::name
        CHECK_ENTRY(Zero);
        CHECK_ENTRY(One);
        CHECK_ENTRY(SrcColor);
        CHECK_ENTRY(InvSrcColor);
        CHECK_ENTRY(DstColor);
        CHECK_ENTRY(InvDstColor);
        CHECK_ENTRY(SrcAlpha);
        CHECK_ENTRY(InvSrcAlpha);
        CHECK_ENTRY(DstAlpha);
        CHECK_ENTRY(InvDstAlpha);
        CHECK_ENTRY(ConstantColor);
        CHECK_ENTRY(InvConstantColor);
        CHECK_ENTRY(SrcAlphaSaturate);
#undef CHECK_ENTRY
        RUNTIME_ASSERT(false);
    };
    static auto get_blend_equation = [](string_view s) -> BlendEquationType {
#define CHECK_ENTRY(name) \
    if (s == #name) \
    return BlendEquationType::name
        CHECK_ENTRY(FuncAdd);
        CHECK_ENTRY(FuncSubtract);
        CHECK_ENTRY(FuncReverseSubtract);
        CHECK_ENTRY(Max);
        CHECK_ENTRY(Min);
#undef CHECK_ENTRY
        RUNTIME_ASSERT(false);
    };

    for (size_t pass = 0; pass < _passCount; pass++) {
        auto blend_func = _str(fofx.GetStr("Effect", _str("BlendFunc_Pass{}", pass + 1), "")).split(' ');
        if (blend_func.size() != 2) {
            blend_func = {"SrcAlpha", "InvSrcAlpha"};
        }

        _srcBlendFunc[pass] = get_blend_func(blend_func[0]);
        _destBlendFunc[pass] = get_blend_func(blend_func[1]);
        _blendEquation[pass] = get_blend_equation(fofx.GetStr("Effect", _str("BlendEquation_Pass{}", pass + 1), "FuncAdd"));
    }
}
