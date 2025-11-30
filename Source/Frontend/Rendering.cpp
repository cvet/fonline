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

FO_BEGIN_NAMESPACE();

auto Null_Renderer::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
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

auto Null_Renderer::CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44
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

auto Null_Renderer::GetViewPort() -> irect32
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

void Null_Renderer::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(rect);
}

void Null_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();
}

void Null_Renderer::OnResizeWindow(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(size);
}

RenderTexture::RenderTexture(isize32 size, bool linear_filtered, bool with_depth) :
    Size {size},
    SizeData {numeric_cast<float32>(size.width), numeric_cast<float32>(size.height), 1.0f / numeric_cast<float32>(size.width), 1.0f / numeric_cast<float32>(size.height)},
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
        Vertices.resize(VertCount + std::max(vcount, const_numeric_cast<size_t>(1024)));

        if constexpr (sizeof(vindex_t) == 2) {
            FO_RUNTIME_ASSERT(Vertices.size() <= 0xFFFF);
        }
    }
    if (IndCount + icount >= Indices.size()) {
        Indices.resize(IndCount + std::max(icount, const_numeric_cast<size_t>(1024)));
    }
}

RenderEffect::RenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) :
    _name {name},
    _usage {usage}
{
    FO_STACK_TRACE_ENTRY();

    const auto fofx_content = loader(name);
    const auto fofx = ConfigFile(name, fofx_content, nullptr, ConfigFileOption::CollectContent);
    FO_RUNTIME_ASSERT(fofx.HasSection("Effect"));

    const auto passes = fofx.GetAsInt("Effect", "Passes", 1);
    FO_RUNTIME_ASSERT(passes >= 1);
    FO_RUNTIME_ASSERT(passes <= const_numeric_cast<int32>(EFFECT_MAX_PASSES));

#if FO_ENABLE_3D
    const auto shadow_pass = fofx.GetAsInt("Effect", "ShadowPass", -1);
    FO_RUNTIME_ASSERT(shadow_pass == -1 || (shadow_pass >= 1 && shadow_pass <= const_numeric_cast<int32>(EFFECT_MAX_PASSES)));
    if (shadow_pass != -1) {
        _isShadow[shadow_pass - 1] = true;
    }
#endif

    _passCount = numeric_cast<size_t>(passes);

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

        _depthWrite[pass] = strex(fofx.GetAsStr("Effect", strex("DepthWrite{}", pass_str), depth_write_default)).to_bool();

        const auto pass_info_content = loader(strex("{}.fofx-{}-info", strex(name).erase_file_extension(), pass + 1));
        const auto pass_info = ConfigFile(name, pass_info_content);
        FO_RUNTIME_ASSERT(pass_info.HasSection("EffectInfo"));

        _posMainTex[pass] = pass_info.GetAsInt("EffectInfo", "MainTex", -1);
        _needMainTex |= _posMainTex[pass] != -1;
        _posEggTex[pass] = pass_info.GetAsInt("EffectInfo", "EggTex", -1);
        _needEggTex |= _posEggTex[pass] != -1;
        _posProjBuf[pass] = pass_info.GetAsInt("EffectInfo", "ProjBuf", -1);
        _needProjBuf |= _posProjBuf[pass] != -1;
        _posMainTexBuf[pass] = pass_info.GetAsInt("EffectInfo", "MainTexBuf", -1);
        _needMainTexBuf |= _posMainTexBuf[pass] != -1;
        _posContourBuf[pass] = pass_info.GetAsInt("EffectInfo", "ContourBuf", -1);
        _needContourBuf |= _posContourBuf[pass] != -1;
        _posTimeBuf[pass] = pass_info.GetAsInt("EffectInfo", "TimeBuf", -1);
        _needTimeBuf |= _posTimeBuf[pass] != -1;
        _posRandomValueBuf[pass] = pass_info.GetAsInt("EffectInfo", "RandomValueBuf", -1);
        _needRandomValueBuf |= _posRandomValueBuf[pass] != -1;
        _posScriptValueBuf[pass] = pass_info.GetAsInt("EffectInfo", "ScriptValueBuf", -1);
        _needScriptValueBuf |= _posScriptValueBuf[pass] != -1;

#if FO_ENABLE_3D
        _posModelBuf[pass] = pass_info.GetAsInt("EffectInfo", "ModelBuf", -1);
        _needModelBuf |= _posModelBuf[pass] != -1;

        for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
            _posModelTex[pass][i] = pass_info.GetAsInt("EffectInfo", strex("ModelTex{}", i), -1);
            _needModelTex[i] |= _posModelTex[pass][i] != -1;
            _needAnyModelTex |= _needModelTex[i];
        }

        _posModelTexBuf[pass] = pass_info.GetAsInt("EffectInfo", "ModelTexBuf", -1);
        _needModelTexBuf |= _posModelTexBuf[pass] != -1;
        _posModelAnimBuf[pass] = pass_info.GetAsInt("EffectInfo", "ModelAnimBuf", -1);
        _needModelAnimBuf |= _posModelAnimBuf[pass] != -1;
#endif
    }
}

auto RenderEffect::CanBatch(const RenderEffect* other) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_name != other->_name) {
        return false;
    }
    if (_usage != other->_usage) {
        return false;
    }
    if (MainTex != other->MainTex) {
        return false;
    }

    return true;
}

FO_END_NAMESPACE();
