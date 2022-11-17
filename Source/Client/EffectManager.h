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

#pragma once

#include "Common.h"

#include "Application.h"
#include "FileSystem.h"
#include "Settings.h"
#include "Timer.h"

DECLARE_EXCEPTION(EffectManagerException);

class FOServer;

struct EffectCollection
{
    RenderEffect* ImGui {};
    RenderEffect* ImGuiDefault {};
    RenderEffect* Font {};
    RenderEffect* FontDefault {};
    RenderEffect* ContourSprite {};
    RenderEffect* ContourSpriteDefault {};
    RenderEffect* Generic {};
    RenderEffect* GenericDefault {};
    RenderEffect* Critter {};
    RenderEffect* CritterDefault {};
    RenderEffect* Tile {};
    RenderEffect* TileDefault {};
    RenderEffect* Roof {};
    RenderEffect* RoofDefault {};
    RenderEffect* Rain {};
    RenderEffect* RainDefault {};
    RenderEffect* Iface {};
    RenderEffect* IfaceDefault {};
    RenderEffect* Primitive {};
    RenderEffect* PrimitiveDefault {};
    RenderEffect* Light {};
    RenderEffect* LightDefault {};
    RenderEffect* Fog {};
    RenderEffect* FogDefault {};
    RenderEffect* FlushRenderTarget {};
    RenderEffect* FlushRenderTargetDefault {};
    RenderEffect* FlushPrimitive {};
    RenderEffect* FlushPrimitiveDefault {};
    RenderEffect* FlushMap {};
    RenderEffect* FlushMapDefault {};
    RenderEffect* FlushLight {};
    RenderEffect* FlushLightDefault {};
    RenderEffect* FlushFog {};
    RenderEffect* FlushFogDefault {};
#if FO_ENABLE_3D
    RenderEffect* SkinnedModel {};
    RenderEffect* SkinnedModelDefault {};
    RenderEffect* ContourModelSprite {};
    RenderEffect* ContourModelSpriteDefault {};
#endif
};

class EffectManager final
{
public:
    EffectManager(RenderSettings& settings, FileSystem& resources);
    EffectManager(const EffectManager&) = delete;
    EffectManager(EffectManager&&) = delete;
    auto operator=(const EffectManager&) -> EffectManager& = delete;
    auto operator=(EffectManager&&) -> EffectManager& = delete;
    ~EffectManager() = default;

    auto LoadEffect(EffectUsage usage, string_view path) -> RenderEffect*;
    void LoadMinimalEffects();
    void LoadDefaultEffects();
    void UpdateEffects(const GameTimer& game_time);

    EffectCollection Effects {};

private:
    void PerFrameEffectUpdate(RenderEffect* effect, const GameTimer& game_time);

    RenderSettings& _settings;
    FileSystem& _resources;
    unordered_map<string, unique_ptr<RenderEffect>> _loadedEffects {};
    bool _nonConstHelper {};
};
