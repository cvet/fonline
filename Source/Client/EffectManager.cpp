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

#include "EffectManager.h"
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"

EffectManager::EffectManager(EffectSettings& settings, FileManager& file_mngr, GameTimer& game_time) : _settings {settings}, _fileMngr {file_mngr}, _gameTime {game_time}
{
    _eventUnsubscriber += App->OnFrameBegin += [this]() {
        for (auto& effect : _loadedEffects) {
            PerFrameEffectUpdate(effect.get());
        }
    };
}

auto EffectManager::LoadEffect(const string& name, const string& defines, const string& base_path) -> RenderEffect*
{
    // Try find already loaded effect
    for (auto& effect : _loadedEffects) {
        if (effect->IsSame(name, defines)) {
            return effect.get();
        }
    }

    // Load new
    auto* effect = App->Render.CreateEffect(name, defines, [this, &base_path](const string& path) -> vector<uchar> {
        auto file = _fileMngr.ReadFile(_str(base_path).extractDir() + path);
        if (!file) {
            file = _fileMngr.ReadFile(path);
            if (!file) {
                WriteLog("Effect file '{}' not found.\n", path);
                return {};
            }
        }

        const auto* buf = file.GetBuf();
        const auto len = file.GetFsize();
        vector<uchar> result(len);
        std::memcpy(&result[0], buf, len);
        return result;
    });

    PerFrameEffectUpdate(effect);

    _loadedEffects.push_back(unique_ptr<RenderEffect>(effect));
    return effect;
}

void EffectManager::PerFrameEffectUpdate(RenderEffect* effect) const
{
    if (effect->TimeBuf) {
        effect->TimeBuf->GameTime = static_cast<float>(_gameTime.GameTick());
        effect->TimeBuf->RealTime = static_cast<float>(_gameTime.FrameTick());
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

#define LOAD_DEFAULT_EFFECT(effect_handle, effect_name) \
    if (!((effect_handle) = effect_handle##Default = LoadEffect(effect_name, "", "Effects/"))) \
    effect_errors++

void EffectManager::LoadMinimalEffects()
{
    uint effect_errors = 0;
    LOAD_DEFAULT_EFFECT(Effects.Font, "Font_Default");
    LOAD_DEFAULT_EFFECT(Effects.FlushRenderTarget, "Flush_RenderTarget");
    if (effect_errors != 0u) {
        throw EffectManagerException("Minimal effects not loaded");
    }
}

void EffectManager::LoadDefaultEffects()
{
    uint effect_errors = 0;
    LOAD_DEFAULT_EFFECT(Effects.Generic, "2D_Default");
    LOAD_DEFAULT_EFFECT(Effects.Critter, "2D_Default");
    LOAD_DEFAULT_EFFECT(Effects.Roof, "2D_Default");
    LOAD_DEFAULT_EFFECT(Effects.Rain, "2D_WithoutEgg");
    LOAD_DEFAULT_EFFECT(Effects.Iface, "Interface_Default");
    LOAD_DEFAULT_EFFECT(Effects.Primitive, "Primitive_Default");
    LOAD_DEFAULT_EFFECT(Effects.Light, "Primitive_Light");
    LOAD_DEFAULT_EFFECT(Effects.Fog, "Primitive_Fog");
    LOAD_DEFAULT_EFFECT(Effects.Font, "Font_Default");
    LOAD_DEFAULT_EFFECT(Effects.Tile, "2D_WithoutEgg");
    LOAD_DEFAULT_EFFECT(Effects.FlushRenderTarget, "Flush_RenderTarget");
    LOAD_DEFAULT_EFFECT(Effects.FlushPrimitive, "Flush_Primitive");
    LOAD_DEFAULT_EFFECT(Effects.FlushMap, "Flush_Map");
    LOAD_DEFAULT_EFFECT(Effects.FlushLight, "Flush_Light");
    LOAD_DEFAULT_EFFECT(Effects.FlushFog, "Flush_Fog");
    if (effect_errors > 0) {
        throw EffectManagerException("Default effects not loaded");
    }

    LOAD_DEFAULT_EFFECT(Effects.Contour, "Contour_Default");
}

void EffectManager::Load3dEffects()
{
    uint effect_errors = 0;
    LOAD_DEFAULT_EFFECT(Effects.Skinned3d, "3D_Skinned");
    if (effect_errors > 0) {
        throw EffectManagerException("Default 3D effects not loaded");
    }
}

#undef LOAD_DEFAULT_EFFECT
