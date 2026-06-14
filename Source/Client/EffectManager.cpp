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

#include "EffectManager.h"
#include "Application.h"

FO_BEGIN_NAMESPACE

EffectManager::EffectManager(RenderSettings& settings, FileSystem& resources, IAppRender& render) :
    _settings {&settings},
    _resources {&resources},
    _render {&render}
{
    FO_STACK_TRACE_ENTRY();
}

auto EffectManager::LoadEffect(EffectUsage usage, string_view path) -> RenderEffect*
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _loadedEffects.find(path); it != _loadedEffects.end()) {
        return it->second.get();
    }

    // Load new
    auto effect = _render->CreateEffect(usage, path, [this](string_view path2) -> string {
        if (const auto file = _resources->ReadFile(path2)) {
            return file.GetStr();
        }

        BreakIntoDebugger();
        WriteLog("Effect file '{}' not found", path2);
        return {};
    });

    if (effect->IsNeedScriptValueBuf()) {
        effect->ScriptValueBuf = RenderEffect::ScriptValueBuffer();
    }

    auto* effect_raw_ptr = effect.get();
    _loadedEffects.emplace(path, std::move(effect));
    return effect_raw_ptr;
}

auto EffectManager::ResolveEffect(raw_ptr<RenderEffect> defaultEffect, string_view effectPath) -> RenderEffect*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(defaultEffect != nullptr, "Missing required default effect");

    if (!effectPath.empty()) {
        RenderEffect* effect = LoadEffect(defaultEffect->GetUsage(), effectPath);

        if (effect == nullptr) {
            throw EffectManagerException("Effect not found or have some errors, see log file");
        }

        return effect;
    }

    return defaultEffect.get();
}

void EffectManager::SetEffect(raw_ptr<RenderEffect>& effect, raw_ptr<RenderEffect> defaultEffect, string_view effectPath)
{
    FO_STACK_TRACE_ENTRY();

    effect = ResolveEffect(defaultEffect, effectPath);
}

void EffectManager::SetEffectScriptValue(RenderEffect* effect, int32_t valueIndex, float32_t value)
{
    FO_STACK_TRACE_ENTRY();

    SetEffectScriptValues(effect, valueIndex, const_span<float32_t> {&value, 1});
}

void EffectManager::SetEffectScriptValues(RenderEffect* effect, int32_t valueStartIndex, const_span<float32_t> values)
{
    FO_STACK_TRACE_ENTRY();

    if (effect == nullptr) {
        throw EffectManagerException("Effect script value target is not loaded");
    }
    if (valueStartIndex < 0 || valueStartIndex > numeric_cast<int32_t>(EFFECT_SCRIPT_VALUES)) {
        throw EffectManagerException("Effect script value index is out of range", valueStartIndex);
    }
    if (values.size() > numeric_cast<size_t>(EFFECT_SCRIPT_VALUES - valueStartIndex)) {
        throw EffectManagerException("Effect script value range is out of range", valueStartIndex, values.size());
    }
    if (!effect->IsNeedScriptValueBuf()) {
        throw EffectManagerException("Effect does not declare ScriptValueBuf");
    }

    RenderEffect::ScriptValueBuffer& script_value_buf = GetOrCreateScriptValueBuf(effect);
    const size_t value_start_index = numeric_cast<size_t>(valueStartIndex);

    for (size_t i = 0; i < values.size(); i++) {
        script_value_buf.ScriptValue[value_start_index + i] = values[i];
    }
}

void EffectManager::ClearEffectScriptValues(RenderEffect* effect)
{
    FO_STACK_TRACE_ENTRY();

    if (effect == nullptr) {
        throw EffectManagerException("Effect script value target is not loaded");
    }
    if (!effect->IsNeedScriptValueBuf()) {
        throw EffectManagerException("Effect does not declare ScriptValueBuf");
    }

    RenderEffect::ScriptValueBuffer& script_value_buf = GetOrCreateScriptValueBuf(effect);

    script_value_buf = RenderEffect::ScriptValueBuffer();
}

auto EffectManager::GetOrCreateScriptValueBuf(RenderEffect* effect) -> RenderEffect::ScriptValueBuffer&
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(effect != nullptr, "Missing required effect");

    if (!effect->ScriptValueBuf.has_value()) {
        effect->ScriptValueBuf = RenderEffect::ScriptValueBuffer();
    }

    return effect->ScriptValueBuf.value();
}

void EffectManager::UpdateEffects(const GameTimer& game_time)
{
    FO_STACK_TRACE_ENTRY();

    for (auto& effect : _loadedEffects | std::views::values) {
        PerFrameEffectUpdate(effect.get(), game_time);
    }
}

void EffectManager::PerFrameEffectUpdate(RenderEffect* effect, const GameTimer& game_time)
{
    FO_STACK_TRACE_ENTRY();

    if (effect->IsNeedTimeBuf()) {
        auto& time_buf = effect->TimeBuf = RenderEffect::TimeBuffer();

        time_buf->FrameTime[0] = timespan(game_time.GetFrameTime().duration_value()).to_ms<float32_t>() / 1000.0f;
        time_buf->GameTime[0] = timespan(game_time.GetFrameTime().duration_value()).to_ms<float32_t>() / 1000.0f;
    }

    if (effect->IsNeedRandomValueBuf()) {
        auto& random_value_buf = effect->RandomValueBuf = RenderEffect::RandomValueBuffer();

        std::uniform_int_distribution<int32_t> random_distribution {0, 99999};

        random_value_buf->RandomValue[0] = numeric_cast<float32_t>(random_distribution(_randomGenerator)) / 100000.0f;
        random_value_buf->RandomValue[1] = numeric_cast<float32_t>(random_distribution(_randomGenerator)) / 100000.0f;
        random_value_buf->RandomValue[2] = numeric_cast<float32_t>(random_distribution(_randomGenerator)) / 100000.0f;
        random_value_buf->RandomValue[3] = numeric_cast<float32_t>(random_distribution(_randomGenerator)) / 100000.0f;
    }
}

#define LOAD_DEFAULT_EFFECT(effect_handle, effect_usage, effect_path) \
    if (!((effect_handle) = effect_handle##Default = LoadEffect(effect_usage, effect_path))) \
    effect_errors++

void EffectManager::LoadMinimalEffects()
{
    FO_STACK_TRACE_ENTRY();

    auto effect_errors = 0;

    LOAD_DEFAULT_EFFECT(Effects.ImGui, EffectUsage::ImGui, _settings->ImGuiDefaultEffect);
    LOAD_DEFAULT_EFFECT(Effects.Font, EffectUsage::QuadSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Iface, EffectUsage::QuadSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushRenderTarget, EffectUsage::QuadSprite, "Effects/Flush_RenderTarget.fofx");

    if (effect_errors != 0) {
        throw EffectManagerException("Minimal effects not loaded");
    }
}

void EffectManager::LoadDefaultEffects()
{
    FO_STACK_TRACE_ENTRY();

    int32_t effect_errors = 0;

    LOAD_DEFAULT_EFFECT(Effects.ImGui, EffectUsage::ImGui, _settings->ImGuiDefaultEffect);
    LOAD_DEFAULT_EFFECT(Effects.Font, EffectUsage::QuadSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Generic, EffectUsage::QuadSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Critter, EffectUsage::QuadSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Roof, EffectUsage::QuadSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Rain, EffectUsage::QuadSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Iface, EffectUsage::QuadSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Primitive, EffectUsage::Primitive, "Effects/Primitive_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Light, EffectUsage::Primitive, "Effects/Primitive_Light.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Fog, EffectUsage::Primitive, "Effects/Primitive_Fog.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Tile, EffectUsage::QuadSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushRenderTarget, EffectUsage::QuadSprite, "Effects/Flush_RenderTarget.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushPrimitive, EffectUsage::QuadSprite, "Effects/Flush_Primitive.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushMap, EffectUsage::QuadSprite, "Effects/Flush_Map.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushLight, EffectUsage::QuadSprite, "Effects/Flush_Light.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushFog, EffectUsage::QuadSprite, "Effects/Flush_Fog.fofx");
#if FO_ENABLE_3D
    LOAD_DEFAULT_EFFECT(Effects.SkinnedModel, EffectUsage::Model, "Effects/3D_Skinned.fofx");
#endif

    if (effect_errors != 0) {
        throw EffectManagerException("Default effects not loaded");
    }
}

#undef LOAD_DEFAULT_EFFECT

FO_END_NAMESPACE
