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

#include "DefaultSprites.h"
#include "ModelSpriteLayout.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

class EngineMetadata;
class ModelInstance;
class ModelManager;
class ModelSpriteFactory;

class ModelSprite final : public AtlasSprite
{
    friend class ModelSpriteFactory;

public:
    explicit ModelSprite(ptr<SpriteManager> spr_mngr, ptr<ModelSpriteFactory> factory, unique_ptr<ModelInstance> model, AtlasType atlas_type);
    ModelSprite(const ModelSprite&) = delete;
    ModelSprite(ModelSprite&&) noexcept = default;
    auto operator=(const ModelSprite&) = delete;
    auto operator=(ModelSprite&&) noexcept -> ModelSprite& = delete;
    ~ModelSprite() override;

    [[nodiscard]] auto IsHitTest(ipos32 pos) const -> bool override;
    [[nodiscard]] auto GetViewSize() const -> optional<irect32> override;
    [[nodiscard]] auto IsCopyable() const -> bool override { return false; }
    [[nodiscard]] auto IsDirectDraw() const -> bool override;
    [[nodiscard]] auto GetModel() -> ptr<ModelInstance>;
    [[nodiscard]] auto IsPlaying() const -> bool override;

    auto FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override;
    void Prewarm() override;
    void SetDir(mdir dir) override;
    void Play(hstring anim_name, bool looped, bool reversed) override;
    void Stop() override;
    auto Update() -> bool override;
    void SetSize(isize32 size);
    void DrawToAtlas();
    void DrawInScene(fpos32 scene_pos, float32_t depth) const override;

private:
    struct PreparedFrameCrop
    {
        isize32 FrameSize {};
        irect32 CropRect {};
        isize32 Size {};
        ipos32 Offset {};
        nptr<TextureAtlas> Atlas {};
        frect32 AtlasRect {};
        unique_del_nptr<TextureAtlasLayout::Allocation> AtlasAllocation {};
        bool ReuseAtlasAllocation {};
        bool BoundedCropEstablished {};
        optional<ModelSpriteBoundsEnvelopeId> CropEnvelopeId {};
    };

    void SetupFrame(isize32 frame_size);
    auto PrepareFrameCrop(isize32 frame_size, optional<ModelSpriteBounds> bounds) -> PreparedFrameCrop;
    void CommitFrameCrop(PreparedFrameCrop&& prepared_crop);
    void ApplyFrameCrop(isize32 frame_size, optional<ModelSpriteBounds> bounds);

    ptr<ModelSpriteFactory> _factory;
    mutable unique_ptr<ModelInstance> _model;
    AtlasType _atlasType {};
    isize32 _frameSize {};
    irect32 _cropRect {};
    optional<isize32> _requestedFrameSize {};
    bool _boundedCropEstablished {};
    optional<ModelSpriteBoundsEnvelopeId> _cropEnvelopeId {};
};

class ModelSpriteFactory : public SpriteFactory
{
    friend class ModelSprite;

public:
    ModelSpriteFactory(ptr<SpriteManager> spr_mngr, ptr<RenderSettings> settings, ptr<const EngineMetadata> engine_metadata, ptr<EffectManager> effect_mngr, ptr<GameTimer> game_time, ptr<AnimationResolver> anim_name_resolver);
    ModelSpriteFactory(const ModelSpriteFactory&) = delete;
    ModelSpriteFactory(ModelSpriteFactory&&) noexcept = delete;
    auto operator=(const ModelSpriteFactory&) = delete;
    auto operator=(ModelSpriteFactory&&) noexcept -> ModelSpriteFactory& = delete;
    ~ModelSpriteFactory() override;

    [[nodiscard]] auto GetExtensions() const -> vector<string> override { return {"fo3d", "fbx", "dae", "obj"}; }
    [[nodiscard]] auto GetModelMngr() -> ptr<ModelManager>;

    auto LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite> override;

private:
    auto LoadTexture(hstring path) -> pair<nptr<RenderTexture>, frect32>;
    void DrawModelToAtlas(ptr<ModelSprite> model_spr);

    ptr<SpriteManager> _sprMngr;
    ptr<RenderSettings> _settings;
    unique_ptr<ModelManager> _modelMngr;
    unordered_map<hstring, shared_ptr<AtlasSprite>> _loadedMeshTextures {};
    vector<ptr<RenderTarget>> _rtIntermediate {};
};

FO_END_NAMESPACE

#endif
