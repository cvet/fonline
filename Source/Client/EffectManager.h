// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "Rendering.h"
#include "Settings.h"
#include "Timer.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(EffectManagerException);

class IAppRender;

// Bit mask selecting the render pipeline or flush stage served by an effect
///@ ExportEnum
enum class EffectType : uint32_t
{
    None = 0, // Selects no effect category
    GenericSprite = 0x00000001, // Replaces the default generic-sprite effect or one map item's draw effect
    CritterSprite = 0x00000002, // Replaces the default critter-sprite effect or one map critter's draw effect
    TileSprite = 0x00000004, // Replaces the default flat tile-sprite effect
    RoofSprite = 0x00000008, // Replaces the default roof-sprite effect
    RainSprite = 0x00000010, // Replaces the default rain-sprite effect
    SkinnedMesh = 0x00000400, // Replaces the default skinned-model effect when 3D rendering is enabled
    Interface = 0x00001000, // Replaces the default interface-sprite effect
    Font = 0x00010000, // Replaces the default font effect or the effect of one font slot selected by subtype
    Primitive = 0x00100000, // Replaces the default generic primitive effect
    Light = 0x00200000, // Replaces the primitive effect used for map light accumulation
    Fog = 0x00400000, // Replaces the primitive effect used for map fog rendering
    FlushRenderTarget = 0x01000000, // Replaces the effect used to flush a render-target texture
    FlushPrimitive = 0x04000000, // Replaces the effect used to flush primitive rendering
    FlushMap = 0x08000000, // Replaces the effect used to composite the rendered map
    FlushLight = 0x10000000, // Replaces the effect used to composite the light buffer
    FlushFog = 0x20000000, // Replaces the effect used to composite the fog buffer
    Offscreen = 0x40000000, // Registers the effect in an indexed offscreen-effect subtype slot
};

struct EffectCollection
{
    nptr<RenderEffect> ImGui {};
    nptr<RenderEffect> ImGuiDefault {};
    nptr<RenderEffect> Font {};
    nptr<RenderEffect> FontDefault {};
    nptr<RenderEffect> Generic {};
    nptr<RenderEffect> GenericDefault {};
    nptr<RenderEffect> Critter {};
    nptr<RenderEffect> CritterDefault {};
    nptr<RenderEffect> Tile {};
    nptr<RenderEffect> TileDefault {};
    nptr<RenderEffect> Roof {};
    nptr<RenderEffect> RoofDefault {};
    nptr<RenderEffect> Flat {};
    nptr<RenderEffect> FlatDefault {};
    nptr<RenderEffect> Rain {};
    nptr<RenderEffect> RainDefault {};
    nptr<RenderEffect> Iface {};
    nptr<RenderEffect> IfaceDefault {};
    nptr<RenderEffect> Primitive {};
    nptr<RenderEffect> PrimitiveDefault {};
    nptr<RenderEffect> Light {};
    nptr<RenderEffect> LightDefault {};
    nptr<RenderEffect> Fog {};
    nptr<RenderEffect> FogDefault {};
    nptr<RenderEffect> FlushRenderTarget {};
    nptr<RenderEffect> FlushRenderTargetDefault {};
    nptr<RenderEffect> FlushPrimitive {};
    nptr<RenderEffect> FlushPrimitiveDefault {};
    nptr<RenderEffect> FlushMap {};
    nptr<RenderEffect> FlushMapDefault {};
    nptr<RenderEffect> FlushLight {};
    nptr<RenderEffect> FlushLightDefault {};
    nptr<RenderEffect> FlushFog {};
    nptr<RenderEffect> FlushFogDefault {};
#if FO_ENABLE_3D
    nptr<RenderEffect> SkinnedModel {};
    nptr<RenderEffect> SkinnedModelDefault {};
#endif
};

class EffectManager final
{
public:
    explicit EffectManager(ptr<RenderSettings> settings, ptr<FileSystem> resources, ptr<IAppRender> render);
    EffectManager(const EffectManager&) = delete;
    EffectManager(EffectManager&&) = delete;
    auto operator=(const EffectManager&) -> EffectManager& = delete;
    auto operator=(EffectManager&&) -> EffectManager& = delete;
    ~EffectManager() = default;

    auto LoadEffect(EffectUsage usage, string_view path) -> nptr<RenderEffect>;
    auto ResolveEffect(ptr<RenderEffect> defaultEffect, string_view effectPath) -> ptr<RenderEffect>;
    void SetEffectScriptValue(ptr<RenderEffect> effect, int32_t valueIndex, float32_t value);
    void SetEffectScriptValues(ptr<RenderEffect> effect, int32_t valueStartIndex, const_span<float32_t> values);
    void ClearEffectScriptValues(ptr<RenderEffect> effect);
    void ClearIndoorMaskTexture(ptr<const RenderTexture> texture);
    auto GetOrCreateScriptValueBuf(ptr<RenderEffect> effect) -> ptr<RenderEffect::ScriptValueBuffer>;
    void LoadMinimalEffects();
    void LoadDefaultEffects();
    void UpdateEffects(const GameTimer& game_time);

    EffectCollection Effects {};

private:
    void PerFrameEffectUpdate(ptr<RenderEffect> effect, const GameTimer& game_time);

    ptr<RenderSettings> _settings;
    ptr<FileSystem> _resources;
    ptr<IAppRender> _render;
    random_generator _randomGenerator {};
    unordered_map<string, unique_ptr<RenderEffect>> _loadedEffects {};
    optional<nanotime> _shaderTimeEpoch {};
};

FO_END_NAMESPACE
