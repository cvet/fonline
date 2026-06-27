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

#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "Rendering.h"
#include "Settings.h"
#include "Timer.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(EffectManagerException);

class IAppRender;

///@ ExportEnum
enum class EffectType : uint32_t
{
    None = 0,
    GenericSprite = 0x00000001,
    CritterSprite = 0x00000002,
    TileSprite = 0x00000004,
    RoofSprite = 0x00000008,
    RainSprite = 0x00000010,
    SkinnedMesh = 0x00000400,
    Interface = 0x00001000,
    Font = 0x00010000,
    Primitive = 0x00100000,
    Light = 0x00200000,
    Fog = 0x00400000,
    FlushRenderTarget = 0x01000000,
    FlushPrimitive = 0x04000000,
    FlushMap = 0x08000000,
    FlushLight = 0x10000000,
    FlushFog = 0x20000000,
    Offscreen = 0x40000000,
};

struct EffectCollection
{
    raw_ptr<RenderEffect> ImGui {};
    raw_ptr<RenderEffect> ImGuiDefault {};
    raw_ptr<RenderEffect> Font {};
    raw_ptr<RenderEffect> FontDefault {};
    raw_ptr<RenderEffect> Generic {};
    raw_ptr<RenderEffect> GenericDefault {};
    raw_ptr<RenderEffect> Critter {};
    raw_ptr<RenderEffect> CritterDefault {};
    raw_ptr<RenderEffect> Tile {};
    raw_ptr<RenderEffect> TileDefault {};
    raw_ptr<RenderEffect> Roof {};
    raw_ptr<RenderEffect> RoofDefault {};
    raw_ptr<RenderEffect> Flat {};
    raw_ptr<RenderEffect> FlatDefault {};
    raw_ptr<RenderEffect> Rain {};
    raw_ptr<RenderEffect> RainDefault {};
    raw_ptr<RenderEffect> Iface {};
    raw_ptr<RenderEffect> IfaceDefault {};
    raw_ptr<RenderEffect> Primitive {};
    raw_ptr<RenderEffect> PrimitiveDefault {};
    raw_ptr<RenderEffect> Light {};
    raw_ptr<RenderEffect> LightDefault {};
    raw_ptr<RenderEffect> Fog {};
    raw_ptr<RenderEffect> FogDefault {};
    raw_ptr<RenderEffect> FlushRenderTarget {};
    raw_ptr<RenderEffect> FlushRenderTargetDefault {};
    raw_ptr<RenderEffect> FlushPrimitive {};
    raw_ptr<RenderEffect> FlushPrimitiveDefault {};
    raw_ptr<RenderEffect> FlushMap {};
    raw_ptr<RenderEffect> FlushMapDefault {};
    raw_ptr<RenderEffect> FlushLight {};
    raw_ptr<RenderEffect> FlushLightDefault {};
    raw_ptr<RenderEffect> FlushFog {};
    raw_ptr<RenderEffect> FlushFogDefault {};
#if FO_ENABLE_3D
    raw_ptr<RenderEffect> SkinnedModel {};
    raw_ptr<RenderEffect> SkinnedModelDefault {};
#endif
};

class EffectManager final
{
public:
    explicit EffectManager(RenderSettings& settings, FileSystem& resources, IAppRender& render);
    EffectManager(const EffectManager&) = delete;
    EffectManager(EffectManager&&) = delete;
    auto operator=(const EffectManager&) -> EffectManager& = delete;
    auto operator=(EffectManager&&) -> EffectManager& = delete;
    ~EffectManager() = default;

    auto LoadEffect(EffectUsage usage, string_view path) -> RenderEffect*;
    auto ResolveEffect(raw_ptr<RenderEffect> defaultEffect, string_view effectPath) -> RenderEffect*;
    void SetEffect(raw_ptr<RenderEffect>& effect, raw_ptr<RenderEffect> defaultEffect, string_view effectPath);
    void SetEffectScriptValue(RenderEffect* effect, int32_t valueIndex, float32_t value);
    void SetEffectScriptValues(RenderEffect* effect, int32_t valueStartIndex, const_span<float32_t> values);
    void ClearEffectScriptValues(RenderEffect* effect);
    auto GetOrCreateScriptValueBuf(RenderEffect* effect) -> RenderEffect::ScriptValueBuffer&;
    void LoadMinimalEffects();
    void LoadDefaultEffects();
    void UpdateEffects(const GameTimer& game_time);

    EffectCollection Effects {};

private:
    void PerFrameEffectUpdate(RenderEffect* effect, const GameTimer& game_time);

    raw_ptr<RenderSettings> _settings;
    raw_ptr<FileSystem> _resources;
    raw_ptr<IAppRender> _render;
    std::mt19937 _randomGenerator {MakeSeededRandomGenerator()};
    unordered_map<string, unique_ptr<RenderEffect>> _loadedEffects {};
};

FO_END_NAMESPACE
