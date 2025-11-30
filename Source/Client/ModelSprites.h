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

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

#include "DefaultSprites.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE();

class ModelSpriteFactory;

class ModelSprite final : public AtlasSprite
{
    friend class ModelSpriteFactory;

public:
    explicit ModelSprite(SpriteManager& spr_mngr, ModelSpriteFactory* factory, unique_ptr<ModelInstance>&& model, AtlasType atlas_type);
    ModelSprite(const ModelSprite&) = delete;
    ModelSprite(ModelSprite&&) noexcept = default;
    auto operator=(const ModelSprite&) = delete;
    auto operator=(ModelSprite&&) noexcept -> ModelSprite& = delete;
    ~ModelSprite() override = default;

    [[nodiscard]] auto IsHitTest(ipos32 pos) const -> bool override;
    [[nodiscard]] auto GetViewSize() const -> optional<irect32> override;
    [[nodiscard]] auto IsCopyable() const -> bool override { return false; }
    [[nodiscard]] auto IsDynamicDraw() const -> bool override { return true; }
    [[nodiscard]] auto GetModel() -> ModelInstance* { return _model.get(); }
    [[nodiscard]] auto IsPlaying() const -> bool override { return _model->IsAnimationPlaying(); }

    void Prewarm() override;
    void SetDir(uint8 dir) override;
    void SetDirAngle(int16 dir_angle) override;
    void Play(hstring anim_name, bool looped, bool reversed) override;
    void Stop() override;
    auto Update() -> bool override;
    void SetSize(isize32 size);
    void DrawToAtlas();

private:
    raw_ptr<ModelSpriteFactory> _factory {};
    unique_ptr<ModelInstance> _model {};
    AtlasType _atlasType {};
};

class ModelSpriteFactory : public SpriteFactory
{
    friend class ModelSprite;

public:
    ModelSpriteFactory(SpriteManager& spr_mngr, RenderSettings& settings, EffectManager& effect_mngr, GameTimer& game_time, HashResolver& hash_resolver, NameResolver& name_resolver, AnimationResolver& anim_name_resolver);
    ModelSpriteFactory(const ModelSpriteFactory&) = delete;
    ModelSpriteFactory(ModelSpriteFactory&&) noexcept = default;
    auto operator=(const ModelSpriteFactory&) = delete;
    auto operator=(ModelSpriteFactory&&) noexcept -> ModelSpriteFactory& = delete;
    ~ModelSpriteFactory() override = default;

    [[nodiscard]] auto GetExtensions() const -> vector<string> override { return {"fo3d", "fbx", "dae", "obj"}; }
    [[nodiscard]] auto GetModelMngr() -> ModelManager* { return _modelMngr.get(); }

    auto LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite> override;

private:
    auto LoadTexture(hstring path) -> pair<RenderTexture*, frect32>;
    void DrawModelToAtlas(ModelSprite* model_spr);

    raw_ptr<SpriteManager> _sprMngr;
    unique_ptr<ModelManager> _modelMngr {};
    unordered_map<hstring, shared_ptr<AtlasSprite>> _loadedMeshTextures {};
    vector<raw_ptr<RenderTarget>> _rtIntermediate {};
};

FO_END_NAMESPACE();

#endif
