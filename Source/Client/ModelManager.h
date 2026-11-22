// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

#include "EffectManager.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "Settings.h"
#include "VisualParticles.h"

FO_BEGIN_NAMESPACE

inline constexpr timespan MODEL_DEFAULT_MOVE_TRANSITION_TIME {std::chrono::milliseconds {250}};

class IAppRender;
class EngineMetadata;
struct ModelBone;
class ModelHierarchy;
class ModelInformation;
class ModelInstance;

auto ResolveModelParticleSceneBackground(bool direct_scene_draw, bool direct_model_draw, const ParticleSceneBackgroundProvider& scene_background_provider) -> ParticleSceneBackgroundResult;

class ModelManager final
{
    friend class ModelInstance;
    friend class ModelInformation;
    friend class ModelHierarchy;

public:
    using TextureLoader = copyable_function<pair<nptr<RenderTexture>, frect32>(string_view)>;

    ModelManager() = delete;
    ModelManager(ptr<RenderSettings> settings, ptr<FileSystem> resources, ptr<const EngineMetadata> engine_metadata, ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<GameTimer> game_time, ptr<AnimationResolver> anim_name_resolver, TextureLoader tex_loader, ParticleWireframeQuery draw_wireframe, ParticleSceneBackgroundProvider scene_background_provider = {});
    ModelManager(const ModelManager&) = delete;
    ModelManager(ModelManager&&) noexcept = delete;
    auto operator=(const ModelManager&) = delete;
    auto operator=(ModelManager&&) noexcept = delete;
    ~ModelManager();

    [[nodiscard]] auto GetBoneHashedString(string_view name) const -> hstring;

    auto CreateModel(string_view name) -> unique_nptr<ModelInstance>;
    void PreloadModel(string_view name);

private:
    auto LoadModel(string_view fname) -> nptr<ModelBone>;
    auto GetInformation(string_view name) -> nptr<ModelInformation>;
    auto GetHierarchy(string_view name) -> nptr<ModelHierarchy>;

    ptr<RenderSettings> _settings;
    ptr<FileSystem> _resources;
    ptr<const EngineMetadata> _engineMetadata;
    ptr<EffectManager> _effectMngr;
    ptr<IAppRender> _render;
    ptr<GameTimer> _gameTime;
    ptr<AnimationResolver> _animNameResolver;
    TextureLoader _textureLoader {};
    ParticleSceneBackgroundProvider _sceneBackgroundProvider {};
    bool _directSceneDraw {};
    ParticleManager _particleMngr;
    set<hstring> _processedFiles {};
    vector<unique_ptr<ModelBone>> _loadedModels {};
    vector<unique_ptr<ModelInformation>> _allModelInfos {};
    vector<unique_ptr<ModelHierarchy>> _hierarchyFiles {};
    timespan _moveTransitionTime {MODEL_DEFAULT_MOVE_TRANSITION_TIME};
    float32_t _globalSpeedAdjust {1.0f};
    timespan _animUpdateThreshold {};
    color4 _lightColor {};
    hstring _headBone {};
    unordered_set<hstring> _legBones {};
    uint32_t _linkId {};
};

FO_END_NAMESPACE

#endif
