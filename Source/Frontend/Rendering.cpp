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

#include "Rendering.h"
#include "ConfigFile.h"

FO_BEGIN_NAMESPACE

RenderTexture::RenderTexture(isize32 size, bool linear_filtered, bool with_depth) :
    Size {size},
    SizeData {numeric_cast<float32_t>(size.width), numeric_cast<float32_t>(size.height), 1.0f / numeric_cast<float32_t>(size.width), 1.0f / numeric_cast<float32_t>(size.height)},
    LinearFiltered {linear_filtered},
    WithDepth {with_depth}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(Size.width > 0, "Size width must be positive", Size.width);
    FO_VERIFY_AND_THROW(Size.height > 0, "Size height must be positive", Size.height);
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
            FO_VERIFY_AND_THROW(Vertices.size() <= 0xFFFF, "Render draw buffer vertex index type cannot address the allocated vertex buffer", Vertices.size(), 0xFFFF);
        }
    }
    if (IndCount + icount >= Indices.size()) {
        Indices.resize(IndCount + std::max(icount, const_numeric_cast<size_t>(1024)));
    }
}

RenderEffect::RenderEffect(EffectUsage usage, u8string_view name, const RenderEffectLoader& loader) :
    _name {name},
    _usage {usage}
{
    FO_STACK_TRACE_ENTRY();

    vector<byte> fofx_data = loader(name);
    u8string fofx_content = utf8_from_byte_span(fofx_data);
    auto fofx = ConfigFile(std::move(fofx_content), ConfigFileOption::CollectContent);
    FO_VERIFY_AND_THROW(fofx.HasSection("Effect"), "FOFX file does not contain the required Effect section", name);

    int32_t passes = fofx.GetAsInt("Effect", "Passes", 1);
    FO_VERIFY_AND_THROW(passes >= 1, "FOFX effect must declare at least one render pass", name, passes);
    FO_VERIFY_AND_THROW(passes <= const_numeric_cast<int32_t>(EFFECT_MAX_PASSES), "FOFX effect declares more render passes than the renderer supports", name, passes, EFFECT_MAX_PASSES);

#if FO_ENABLE_3D
    int32_t shadow_pass = fofx.GetAsInt("Effect", "ShadowPass", -1);
    FO_VERIFY_AND_THROW(shadow_pass == -1 || (shadow_pass >= 1 && shadow_pass <= const_numeric_cast<int32_t>(EFFECT_MAX_PASSES)), "FOFX shadow pass index is outside the supported pass range", name, shadow_pass, EFFECT_MAX_PASSES);

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

    static auto get_depth_func = [](string_view s) -> DepthFuncType {
        if (s == "Always") {
            return DepthFuncType::Always;
        }
        if (s == "Never") {
            return DepthFuncType::Never;
        }
        if (s == "Less") {
            return DepthFuncType::Less;
        }
        if (s == "LessEqual") {
            return DepthFuncType::LessEqual;
        }
        if (s == "Equal") {
            return DepthFuncType::Equal;
        }
        if (s == "GreaterEqual") {
            return DepthFuncType::GreaterEqual;
        }
        if (s == "Greater") {
            return DepthFuncType::Greater;
        }
        if (s == "NotEqual") {
            return DepthFuncType::NotEqual;
        }

        throw GenericException("Unknown depth func type", s);
    };

    u8string_view blend_func_default = fofx.GetAsStr("Effect", "BlendFunc", u8"SrcAlpha InvSrcAlpha");
    u8string_view blend_equation_default = fofx.GetAsStr("Effect", "BlendEquation", u8"FuncAdd");
    u8string_view depth_write_default = fofx.GetAsStr("Effect", "DepthWrite", u8"True");
    u8string_view depth_func_default = fofx.GetAsStr("Effect", "DepthFunc", u8"Always");

    // Opting in builds the alternative depth-state variants so a draw can select one; without it the effect only ever
    // uses the state declared here and costs exactly what it did before.
    _depthVariants = u8strvex(fofx.GetAsStr("Effect", "DepthVariants", u8"False")).to_bool();
    _cullVariants = u8strvex(fofx.GetAsStr("Effect", "CullVariants", u8"False")).to_bool();

    for (size_t pass = 0; pass < _passCount; pass++) {
        string pass_str = strex("_Pass{}", pass + 1);

        string blend_func_value = utf8_to_string(fofx.GetAsStr("Effect", strex("BlendFunc{}", pass_str), blend_func_default));
        auto blend_func = strvex(blend_func_value).split(' ');
        FO_VERIFY_AND_THROW(blend_func.size() == 2, "FOFX blend function must contain source and destination factors", name, pass + 1, blend_func.size(), blend_func_value);

        _srcBlendFunc[pass] = get_blend_func(blend_func[0]);
        _destBlendFunc[pass] = get_blend_func(blend_func[1]);
        string blend_equation_value = utf8_to_string(fofx.GetAsStr("Effect", strex("BlendEquation{}", pass_str), blend_equation_default));
        _blendEquation[pass] = get_blend_equation(blend_equation_value);

        _depthWrite[pass] = u8strvex(fofx.GetAsStr("Effect", strex("DepthWrite{}", pass_str), depth_write_default)).to_bool();
        string depth_func_value = utf8_to_string(fofx.GetAsStr("Effect", strex("DepthFunc{}", pass_str), depth_func_default));
        _depthFunc[pass] = get_depth_func(depth_func_value);

        vector<byte> pass_info_data = loader(u8strex("{}.fofx-{}-info", u8strex(name).erase_file_extension(), pass + 1));
        u8string pass_info_content = utf8_from_byte_span(pass_info_data);
        auto pass_info = ConfigFile(std::move(pass_info_content));
        FO_VERIFY_AND_THROW(pass_info.HasSection("EffectInfo"), "FOFX pass EffectInfo section is missing");

        _posMainTex[pass] = pass_info.GetAsInt("EffectInfo", "MainTex", -1);
        _needMainTex |= _posMainTex[pass] != -1;
        _posIndoorMaskTex[pass] = pass_info.GetAsInt("EffectInfo", "IndoorMaskTex", -1);
        _needIndoorMaskTex |= _posIndoorMaskTex[pass] != -1;
        _posBackgroundTex[pass] = pass_info.GetAsInt("EffectInfo", "BackgroundTex", -1);
        _needBackgroundTex |= _posBackgroundTex[pass] != -1;
        _posEggBuf[pass] = pass_info.GetAsInt("EffectInfo", "EggBuf", -1);
        _needEggBuf |= _posEggBuf[pass] != -1;
        _posProjBuf[pass] = pass_info.GetAsInt("EffectInfo", "ProjBuf", -1);
        _needProjBuf |= _posProjBuf[pass] != -1;
        _posMainTexBuf[pass] = pass_info.GetAsInt("EffectInfo", "MainTexBuf", -1);
        _needMainTexBuf |= _posMainTexBuf[pass] != -1;
        _posSpriteBorderBuf[pass] = pass_info.GetAsInt("EffectInfo", "SpriteBorderBuf", -1);
        _needSpriteBorderBuf |= _posSpriteBorderBuf[pass] != -1;
        _posParticleSamplingBuf[pass] = pass_info.GetAsInt("EffectInfo", "ParticleSamplingBuf", -1);
        _needParticleSamplingBuf |= _posParticleSamplingBuf[pass] != -1;
        _posTimeBuf[pass] = pass_info.GetAsInt("EffectInfo", "TimeBuf", -1);
        _needTimeBuf |= _posTimeBuf[pass] != -1;
        _posRandomValueBuf[pass] = pass_info.GetAsInt("EffectInfo", "RandomValueBuf", -1);
        _needRandomValueBuf |= _posRandomValueBuf[pass] != -1;
        _posScriptValueBuf[pass] = pass_info.GetAsInt("EffectInfo", "ScriptValueBuf", -1);
        _needScriptValueBuf |= _posScriptValueBuf[pass] != -1;
        _posCameraBuf[pass] = pass_info.GetAsInt("EffectInfo", "CameraBuf", -1);
        _needCameraBuf |= _posCameraBuf[pass] != -1;

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

auto RenderEffect::CanBatch(ptr<const RenderEffect> other) const -> bool
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
    if (DepthVariant != other->DepthVariant) {
        return false;
    }
    if (CullMode != other->CullMode) {
        return false;
    }

    return true;
}

auto RenderEffect::ResolveCullMode() const -> CullModeType
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsCullModeUsed(CullMode), "Draw asks for a cull mode the effect did not build", _name, static_cast<int32_t>(CullMode));

    return CullMode;
}

auto RenderEffect::GetDepthWrite(size_t pass) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (DepthVariant) {
    case DepthVariantType::TestWrite:
    case DepthVariantType::NoTestWrite:
        return true;
    case DepthVariantType::TestNoWrite:
    case DepthVariantType::NoTestNoWrite:
        return false;
    case DepthVariantType::FromEffect:
        break;
    }

    return _depthWrite[pass];
}

auto RenderEffect::GetDepthFunc(size_t pass) const noexcept -> DepthFuncType
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (DepthVariant) {
    case DepthVariantType::NoTestWrite:
    case DepthVariantType::NoTestNoWrite:
        return DepthFuncType::Always;
    case DepthVariantType::TestWrite:
    case DepthVariantType::TestNoWrite:
    case DepthVariantType::FromEffect:
        break;
    }

    return _depthFunc[pass];
}

auto RenderEffect::ResolveDepthVariantSlot(size_t pass) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(pass < _passCount, "Depth variant pass is outside the effect's pass range", _name, pass + 1, _passCount);

    // The slot encodes the resolved state rather than the requested variant, so an effect that declares no variants
    // can still use an equivalent requested state when it lands on the single slot the effect built.
    size_t slot = GetDepthWrite(pass) ? 1 : 0;

    if (GetDepthFunc(pass) == DepthFuncType::Always) {
        slot += 2;
    }

    FO_VERIFY_AND_THROW(IsDepthVariantSlotUsed(pass, slot), "Draw asks for a depth state the effect did not build", _name, pass + 1, static_cast<int32_t>(DepthVariant), slot);

    return slot;
}

auto RenderEffect::IsDepthVariantSlotUsed(size_t pass, size_t slot) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_depthVariants) {
        return true;
    }

    size_t own_slot = _depthWrite[pass] ? 1 : 0;

    if (_depthFunc[pass] == DepthFuncType::Always) {
        own_slot += 2;
    }

    return slot == own_slot;
}

auto RenderEffect::GetDepthVariantWrite(size_t slot) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return (slot & 1) != 0;
}

auto RenderEffect::GetDepthVariantFunc(size_t pass, size_t slot) const noexcept -> DepthFuncType
{
    FO_NO_STACK_TRACE_ENTRY();

    return (slot & 2) != 0 ? DepthFuncType::Always : _depthFunc[pass];
}

FO_END_NAMESPACE
