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

RenderEffect::RenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) : Name {name}, Usage {usage}
{
    const auto content = loader(name);
    const auto fofx = ConfigFile(name, content, nullptr, ConfigFileOption::CollectContent);
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
    static auto get_alpha_test = [](string_view s) -> AlphaTestType {
#define CHECK_ENTRY(name) \
    if (s == #name) \
    return AlphaTestType::name
        CHECK_ENTRY(Disabled);
        CHECK_ENTRY(Never);
        CHECK_ENTRY(Always);
        CHECK_ENTRY(Equal);
        CHECK_ENTRY(NotEqual);
        CHECK_ENTRY(Less);
        CHECK_ENTRY(LessEqual);
        CHECK_ENTRY(Greater);
        CHECK_ENTRY(GreaterEqual);
#undef CHECK_ENTRY
        RUNTIME_ASSERT(false);
    };

    const auto blend_func_default = fofx.GetStr("Effect", "BlendFunc", "SrcAlpha InvSrcAlpha");
    const auto blend_equation_default = fofx.GetStr("Effect", "BlendEquation", "FuncAdd");
    const auto depth_write_default = fofx.GetStr("Effect", "DepthWrite", "True");

    for (size_t pass = 0; pass < _passCount; pass++) {
        const auto pass_str = _str("_Pass{}", pass + 1).str();

        auto blend_func = _str(fofx.GetStr("Effect", _str("BlendFunc{}", pass_str), blend_func_default)).split(' ');
        RUNTIME_ASSERT(blend_func.size() == 2);

        _srcBlendFunc[pass] = get_blend_func(blend_func[0]);
        _destBlendFunc[pass] = get_blend_func(blend_func[1]);
        _blendEquation[pass] = get_blend_equation(fofx.GetStr("Effect", _str("BlendEquation{}", pass_str), blend_equation_default));

        _depthWrite[pass] = _str(fofx.GetStr("Effect", _str("DepthWrite{}", pass_str), depth_write_default)).toBool();
    }
}
