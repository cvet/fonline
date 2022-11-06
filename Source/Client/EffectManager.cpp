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

#include "EffectManager.h"
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"

EffectManager::EffectManager(RenderSettings& settings, FileSystem& resources) : _settings {settings}, _resources {resources}
{
}

auto EffectManager::LoadEffect(EffectUsage usage, string_view name, string_view base_path) -> RenderEffect*
{
    if (const auto it = _loadedEffects.find(string(name)); it != _loadedEffects.end()) {
        return it->second.get();
    }

    // Load new
    auto* effect = App->Render.CreateEffect(usage, name, [this, &base_path](string_view path) -> string {
        if (!base_path.empty()) {
            if (const auto file = _resources.ReadFile(_str(base_path).extractDir().combinePath(path))) {
                return file.GetStr();
            }
        }

        if (const auto file = _resources.ReadFile(path)) {
            return file.GetStr();
        }

        BreakIntoDebugger();
        WriteLog("Effect file '{}' not found", path);
        return {};
    });

    _loadedEffects.emplace(name, unique_ptr<RenderEffect>(effect));
    return effect;
}

void EffectManager::UpdateEffects(const GameTimer& game_time)
{
    for (auto&& [name, effect] : _loadedEffects) {
        PerFrameEffectUpdate(effect.get(), game_time);
    }
}

void EffectManager::PerFrameEffectUpdate(RenderEffect* effect, const GameTimer& game_time)
{
    NON_CONST_METHOD_HINT();

    if (effect->TimeBuf) {
        effect->TimeBuf->GameTime = static_cast<float>(game_time.GameTick());
        effect->TimeBuf->RealTime = static_cast<float>(game_time.FrameTick());
    }

    if (effect->RandomValueBuf) {
        for (auto i = 0; i < EFFECT_SCRIPT_VALUES; i++) {
            const auto rnd = static_cast<float>(static_cast<double>(GenericUtils::Random(0, 2000000000)) / (2000000000.0 - 1000.0));
            effect->RandomValueBuf->Value[i] = rnd;
        }
    }

    if (effect->ScriptValueBuf) {
        for (auto i = 0; i < EFFECT_SCRIPT_VALUES; i++) {
            effect->ScriptValueBuf->Value[i] = _settings.EffectValues[i];
        }
    }
}

#define LOAD_DEFAULT_EFFECT(effect_handle, effect_usage, effect_name) \
    if (!((effect_handle) = effect_handle##Default = LoadEffect(effect_usage, effect_name, ""))) \
    effect_errors++

void EffectManager::LoadMinimalEffects()
{
    auto effect_errors = 0;

    LOAD_DEFAULT_EFFECT(Effects.ImGui, EffectUsage::ImGui, "Effects/ImGui_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Font, EffectUsage::Font, "Effects/Font_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushRenderTarget, EffectUsage::ImGui, "Effects/Flush_RenderTarget.fofx");

    if (effect_errors != 0) {
        throw EffectManagerException("Minimal effects not loaded");
    }
}

void EffectManager::LoadDefaultEffects()
{
    auto effect_errors = 0;

    LOAD_DEFAULT_EFFECT(Effects.ImGui, EffectUsage::ImGui, "Effects/ImGui_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Font, EffectUsage::Font, "Effects/Font_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Generic, EffectUsage::MapSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Critter, EffectUsage::MapSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Roof, EffectUsage::MapSprite, "Effects/2D_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Rain, EffectUsage::MapSprite, "Effects/2D_WithoutEgg.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Iface, EffectUsage::Interface, "Effects/Interface_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Primitive, EffectUsage::Primitive, "Effects/Primitive_Default.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Light, EffectUsage::Primitive, "Effects/Primitive_Light.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Fog, EffectUsage::Primitive, "Effects/Primitive_Fog.fofx");
    LOAD_DEFAULT_EFFECT(Effects.Tile, EffectUsage::MapSprite, "Effects/2D_WithoutEgg.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushRenderTarget, EffectUsage::Flush, "Effects/Flush_RenderTarget.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushPrimitive, EffectUsage::Flush, "Effects/Flush_Primitive.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushMap, EffectUsage::Flush, "Effects/Flush_Map.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushLight, EffectUsage::Flush, "Effects/Flush_Light.fofx");
    LOAD_DEFAULT_EFFECT(Effects.FlushFog, EffectUsage::Flush, "Effects/Flush_Fog.fofx");
#if FO_ENABLE_3D
    LOAD_DEFAULT_EFFECT(Effects.Skinned3d, EffectUsage::Model, "Effects/3D_Skinned.fofx");
#endif

    if (effect_errors != 0) {
        throw EffectManagerException("Default effects not loaded");
    }

    LOAD_DEFAULT_EFFECT(Effects.Contour, EffectUsage::Contour, "Effects/Contour_Default.fofx");
}

#undef LOAD_DEFAULT_EFFECT
