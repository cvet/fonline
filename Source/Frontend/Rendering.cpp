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

#include "Rendering.h"
#include "ConfigFile.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE();

auto Null_Renderer::CreateTexture(isize size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(size);
    ignore_unused(linear_filtered);
    ignore_unused(with_depth);

    return nullptr;
}

auto Null_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(is_static);

    return nullptr;
}

auto Null_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(usage);
    ignore_unused(name);
    ignore_unused(loader);

    return nullptr;
}

auto Null_Renderer::CreateOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp) -> mat44
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(left);
    ignore_unused(right);
    ignore_unused(bottom);
    ignore_unused(top);
    ignore_unused(nearp);
    ignore_unused(farp);

    return {};
}

auto Null_Renderer::GetViewPort() -> IRect
{
    FO_STACK_TRACE_ENTRY();

    return {};
}

auto Null_Renderer::IsRenderTargetFlipped() -> bool
{
    FO_STACK_TRACE_ENTRY();

    return false;
}

void Null_Renderer::Init(GlobalSettings& settings, WindowInternalHandle* window)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(settings);
    ignore_unused(window);
}

void Null_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();
}

void Null_Renderer::SetRenderTarget(RenderTexture* tex)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(tex);
}

void Null_Renderer::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(color);
    ignore_unused(depth);
    ignore_unused(stencil);
}

void Null_Renderer::EnableScissor(irect rect)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(rect);
}

void Null_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();
}

void Null_Renderer::OnResizeWindow(isize size)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(size);
}

RenderTexture::RenderTexture(isize size, bool linear_filtered, bool with_depth) :
    Size {size},
    SizeData {static_cast<float>(size.width), static_cast<float>(size.height), 1.0f / static_cast<float>(size.width), 1.0f / static_cast<float>(size.height)},
    LinearFiltered {linear_filtered},
    WithDepth {with_depth}
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Size.width > 0);
    FO_RUNTIME_ASSERT(Size.height > 0);
}

RenderDrawBuffer::RenderDrawBuffer(bool is_static) :
    IsStatic {is_static}
{
    FO_STACK_TRACE_ENTRY();
}

void RenderDrawBuffer::CheckAllocBuf(size_t vcount, size_t icount)
{
    if (VertCount + vcount >= Vertices.size()) {
        Vertices.resize(VertCount + std::max(vcount, static_cast<size_t>(1024)));

        if constexpr (sizeof(vindex_t) == 2) {
            FO_RUNTIME_ASSERT(Vertices.size() <= 0xFFFF);
        }
    }
    if (IndCount + icount >= Indices.size()) {
        Indices.resize(IndCount + std::max(icount, static_cast<size_t>(1024)));
    }
}

RenderEffect::RenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) :
    Name {name},
    Usage {usage}
{
    FO_STACK_TRACE_ENTRY();

    const auto fofx_content = loader(name);
    const auto fofx = ConfigFile(name, fofx_content, nullptr, ConfigFileOption::CollectContent);
    FO_RUNTIME_ASSERT(fofx.HasSection("Effect"));

    const auto passes = fofx.GetAsInt("Effect", "Passes", 1);
    FO_RUNTIME_ASSERT(passes >= 1);
    FO_RUNTIME_ASSERT(passes <= static_cast<int>(EFFECT_MAX_PASSES));

#if FO_ENABLE_3D
    const auto shadow_pass = fofx.GetAsInt("Effect", "ShadowPass", -1);
    FO_RUNTIME_ASSERT(shadow_pass == -1 || (shadow_pass >= 1 && shadow_pass <= static_cast<int>(EFFECT_MAX_PASSES)));
    if (shadow_pass != -1) {
        _isShadow[shadow_pass - 1] = true;
    }
#endif

    _passCount = static_cast<size_t>(passes);

    static auto get_blend_func = [](string_view s) -> BlendFuncType {
        if (s == "Zero") {
            return BlendFuncType::Zero;
        }
        if (s == "One") {
            return BlendFuncType::One;
        }
        if (s == "SrcColor") {
            return BlendFuncType::SrcColor;
        }
        if (s == "InvSrcColor") {
            return BlendFuncType::InvSrcColor;
        }
        if (s == "DstColor") {
            return BlendFuncType::DstColor;
        }
        if (s == "InvDstColor") {
            return BlendFuncType::InvDstColor;
        }
        if (s == "SrcAlpha") {
            return BlendFuncType::SrcAlpha;
        }
        if (s == "InvSrcAlpha") {
            return BlendFuncType::InvSrcAlpha;
        }
        if (s == "DstAlpha") {
            return BlendFuncType::DstAlpha;
        }
        if (s == "InvDstAlpha") {
            return BlendFuncType::InvDstAlpha;
        }
        if (s == "ConstantColor") {
            return BlendFuncType::ConstantColor;
        }
        if (s == "InvConstantColor") {
            return BlendFuncType::InvConstantColor;
        }
        if (s == "SrcAlphaSaturate") {
            return BlendFuncType::SrcAlphaSaturate;
        }

        throw GenericException("Unknown blend func type", s);
    };

    static auto get_blend_equation = [](string_view s) -> BlendEquationType {
        if (s == "FuncAdd") {
            return BlendEquationType::FuncAdd;
        }
        if (s == "FuncSubtract") {
            return BlendEquationType::FuncSubtract;
        }
        if (s == "FuncReverseSubtract") {
            return BlendEquationType::FuncReverseSubtract;
        }
        if (s == "Max") {
            return BlendEquationType::Max;
        }
        if (s == "Min") {
            return BlendEquationType::Min;
        }

        throw GenericException("Unknown blend equation type", s);
    };

    const auto blend_func_default = fofx.GetAsStr("Effect", "BlendFunc", "SrcAlpha InvSrcAlpha");
    const auto blend_equation_default = fofx.GetAsStr("Effect", "BlendEquation", "FuncAdd");
    const auto depth_write_default = fofx.GetAsStr("Effect", "DepthWrite", "True");

    for (size_t pass = 0; pass < _passCount; pass++) {
        const string pass_str = strex("_Pass{}", pass + 1);

        auto blend_func = strex(fofx.GetAsStr("Effect", strex("BlendFunc{}", pass_str), blend_func_default)).split(' ');
        FO_RUNTIME_ASSERT(blend_func.size() == 2);

        _srcBlendFunc[pass] = get_blend_func(blend_func[0]);
        _destBlendFunc[pass] = get_blend_func(blend_func[1]);
        _blendEquation[pass] = get_blend_equation(fofx.GetAsStr("Effect", strex("BlendEquation{}", pass_str), blend_equation_default));

        _depthWrite[pass] = strex(fofx.GetAsStr("Effect", strex("DepthWrite{}", pass_str), depth_write_default)).toBool();

        const auto pass_info_content = loader(strex("{}.{}.info", strex(name).eraseFileExtension(), pass + 1));
        const auto pass_info = ConfigFile(name, pass_info_content);
        FO_RUNTIME_ASSERT(pass_info.HasSection("EffectInfo"));

        const_cast<int&>(_posMainTex[pass]) = pass_info.GetAsInt("EffectInfo", "MainTex", -1);
        const_cast<bool&>(NeedMainTex) |= _posMainTex[pass] != -1;
        const_cast<int&>(_posEggTex[pass]) = pass_info.GetAsInt("EffectInfo", "EggTex", -1);
        const_cast<bool&>(NeedEggTex) |= _posEggTex[pass] != -1;
        const_cast<int&>(_posProjBuf[pass]) = pass_info.GetAsInt("EffectInfo", "ProjBuf", -1);
        const_cast<bool&>(NeedProjBuf) |= _posProjBuf[pass] != -1;
        const_cast<int&>(_posMainTexBuf[pass]) = pass_info.GetAsInt("EffectInfo", "MainTexBuf", -1);
        const_cast<bool&>(NeedMainTexBuf) |= _posMainTexBuf[pass] != -1;
        const_cast<int&>(_posContourBuf[pass]) = pass_info.GetAsInt("EffectInfo", "ContourBuf", -1);
        const_cast<bool&>(NeedContourBuf) |= _posContourBuf[pass] != -1;
        const_cast<int&>(_posTimeBuf[pass]) = pass_info.GetAsInt("EffectInfo", "TimeBuf", -1);
        const_cast<bool&>(NeedTimeBuf) |= _posTimeBuf[pass] != -1;
        const_cast<int&>(_posRandomValueBuf[pass]) = pass_info.GetAsInt("EffectInfo", "RandomValueBuf", -1);
        const_cast<bool&>(NeedRandomValueBuf) |= _posRandomValueBuf[pass] != -1;
        const_cast<int&>(_posScriptValueBuf[pass]) = pass_info.GetAsInt("EffectInfo", "ScriptValueBuf", -1);
        const_cast<bool&>(NeedScriptValueBuf) |= _posScriptValueBuf[pass] != -1;
#if FO_ENABLE_3D
        const_cast<int&>(_posModelBuf[pass]) = pass_info.GetAsInt("EffectInfo", "ModelBuf", -1);
        const_cast<bool&>(NeedModelBuf) |= _posModelBuf[pass] != -1;
        for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
            const_cast<int&>(_posModelTex[pass][i]) = pass_info.GetAsInt("EffectInfo", strex("ModelTex{}", i), -1);
            const_cast<bool&>(NeedModelTex[i]) |= _posModelTex[pass][i] != -1;
            const_cast<bool&>(NeedAnyModelTex) |= NeedModelTex[i];
        }
        const_cast<int&>(_posModelTexBuf[pass]) = pass_info.GetAsInt("EffectInfo", "ModelTexBuf", -1);
        const_cast<bool&>(NeedModelTexBuf) |= _posModelTexBuf[pass] != -1;
        const_cast<int&>(_posModelAnimBuf[pass]) = pass_info.GetAsInt("EffectInfo", "ModelAnimBuf", -1);
        const_cast<bool&>(NeedModelAnimBuf) |= _posModelAnimBuf[pass] != -1;
#endif
    }
}

FO_END_NAMESPACE();
