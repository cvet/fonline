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

#if FO_ENABLE_3D

#include "EffectManager.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "Settings.h"
#include "VisualParticles.h"

FO_BEGIN_NAMESPACE

class IAppRender;
struct ModelBone;
class ModelHierarchy;
class ModelInformation;
class ModelInstance;

class ModelManager final
{
    friend class ModelInstance;
    friend class ModelInformation;
    friend class ModelHierarchy;

public:
    using TextureLoader = function<pair<nptr<RenderTexture>, frect32>(string_view)>;

    ModelManager() = delete;
    ModelManager(ptr<RenderSettings> settings, ptr<FileSystem> resources, ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<GameTimer> game_time, ptr<HashResolver> hash_resolver, ptr<NameResolver> name_resolver, ptr<AnimationResolver> anim_name_resolver, TextureLoader tex_loader);
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
    ptr<EffectManager> _effectMngr;
    ptr<IAppRender> _render;
    ptr<GameTimer> _gameTime;
    mutable ptr<HashResolver> _hashResolver;
    ptr<NameResolver> _nameResolver;
    ptr<AnimationResolver> _animNameResolver;
    TextureLoader _textureLoader;
    ParticleManager _particleMngr;
    set<hstring> _processedFiles {};
    vector<unique_ptr<ModelBone>> _loadedModels {};
    vector<unique_ptr<ModelInformation>> _allModelInfos {};
    vector<unique_ptr<ModelHierarchy>> _hierarchyFiles {};
    float32_t _moveTransitionTime {0.25f};
    float32_t _globalSpeedAdjust {1.0f};
    int32_t _animUpdateThreshold {};
    color4 _lightColor {};
    hstring _headBone {};
    unordered_set<hstring> _legBones {};
    uint32_t _linkId {};
};

FO_END_NAMESPACE

#endif
