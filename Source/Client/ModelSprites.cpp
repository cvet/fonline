// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ModelSprites.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "EngineBase.h"
#include "Geometry.h"
#include "ModelInstance.h"
#include "ModelManager.h"

FO_BEGIN_NAMESPACE

static constexpr timespan HIT_MASK_REFRESH_INTERVAL {std::chrono::milliseconds {100}};

auto ModelSpriteHitMask::IsHitTest(ipos32 pos) const -> bool
{
    if (!_maskSize.is_valid_pos(pos)) {
        return false;
    }

    return _mask[numeric_cast<size_t>(pos.y) * numeric_cast<size_t>(_maskSize.width) + numeric_cast<size_t>(pos.x)];
}

void ModelSpriteHitMask::Refresh(ptr<const RenderTexture> texture, ipos32 pos, isize32 size, const function<bool(int32_t)>& check_hit, nanotime now)
{
    FO_TRACE_ZONE(Render);

    auto discard_readback_on_fail {scope_fail([this]() noexcept {
        _readback.reset();
        _stale = true;
        _refreshRequested = true;
    })};

    if (!_readback && _stale && _refreshRequested && now >= _nextRefreshTime) {
        _readback = texture->RequestTextureRegion(pos, size);
        _readbackSize = size;
        _stale = false;
        _refreshRequested = false;
        _nextRefreshTime = now + HIT_MASK_REFRESH_INTERVAL;
    }

    if (!_readback) {
        return;
    }

    optional<vector<ucolor>> pixels {_readback->TakePixels()};

    if (!pixels.has_value()) {
        return;
    }

    vector<bool> mask {vector<bool>(pixels->size())};

    for (size_t i {0}; i < pixels->size(); i++) {
        mask[i] = check_hit(numeric_cast<int32_t>((*pixels)[i].comp.a));
    }

    _mask = std::move(mask);
    _maskSize = _readbackSize;
    _readback.reset();
}

ModelSprite::ModelSprite(ptr<SpriteManager> spr_mngr, ptr<ModelSpriteFactory> factory, unique_ptr<ModelInstance> model, AtlasType atlas_type) :
    AtlasSprite(spr_mngr, {}, {}, {}, {}, {}, {}),
    _factory {factory},
    _model {std::move(model)},
    _atlasType {atlas_type}
{
}

ModelSprite::~ModelSprite() = default;

auto ModelSprite::GetModel() -> ptr<ModelInstance>
{
    return _model;
}

auto ModelSprite::IsPlaying() const -> bool
{
    return _model->IsAnimationPlaying();
}

auto ModelSprite::IsHitTest(ipos32 pos) const -> bool
{
    if (!_size.is_valid_pos(pos)) {
        return false;
    }

    _hitMask.RequestRefresh();

    return _hitMask.IsHitTest(pos);
}

// New copies are throttled; pending readbacks are polled independently until their pixels arrive
void ModelSprite::RefreshHitMask() const
{
    ptr<const RenderTexture> atlas_tex {GetAtlas()->GetTexture()};
    ipos32 atlas_pos {iround<int32_t>(atlas_tex->SizeData[0] * GetAtlasRect().x), iround<int32_t>(atlas_tex->SizeData[1] * GetAtlasRect().y)};
    _hitMask.Refresh(atlas_tex, atlas_pos, _size, [this](int32_t value) { return _sprMngr->CheckHitTest(value); }, _sprMngr->GetTimer().GetFrameTime());
}

auto ModelSprite::GetViewSize() const -> optional<irect32>
{
    irect32 view_rect {_model->GetViewRect()};

    return irect32 {
        view_rect.x + view_rect.width / 2 - _offset.x,
        view_rect.y + view_rect.height - _offset.y,
        view_rect.width,
        view_rect.height,
    };
}

auto ModelSprite::IsDirectDraw() const -> bool
{
    return _factory->_settings->Render.ModelDirectDraw;
}

auto ModelSprite::FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t
{
    FO_STRONG_ASSERT(_frameSize.width > 0 && _frameSize.height > 0 && _cropRect.x >= 0 && _cropRect.y >= 0 && _cropRect.width > 0 && _cropRect.height > 0 && numeric_cast<int64_t>(_cropRect.x) + _cropRect.width <= _frameSize.width && numeric_cast<int64_t>(_cropRect.y) + _cropRect.height <= _frameSize.height, "Model sprite crop is outside the logical frame", _cropRect, _frameSize);

    isize32 lighting_size {_model->GetLightingSize()};
    FO_STRONG_ASSERT(lighting_size.width > 0, "Model sprite lighting frame width must be positive", lighting_size.width);

    uint32_t color_width {numeric_cast<uint32_t>(lighting_size.width)};
    ucolor color_left {std::get<0>(colors)};
    ucolor color_right {std::get<1>(colors)};
    auto interpolate_color {[color_width, color_left, color_right](int64_t source_x) noexcept -> ucolor {
        uint32_t color_x {numeric_cast<uint32_t>(std::clamp<int64_t>(source_x, 0, color_width))};
        auto interpolate_component {[color_x, color_width](uint8_t left_component, uint8_t right_component) noexcept -> uint8_t {
            uint32_t weighted {numeric_cast<uint32_t>(left_component) * (color_width - color_x) + numeric_cast<uint32_t>(right_component) * color_x};
            return numeric_cast<uint8_t>((weighted + color_width / 2) / color_width);
        }};

        return {
            interpolate_component(color_left.comp.r, color_right.comp.r),
            interpolate_component(color_left.comp.g, color_right.comp.g),
            interpolate_component(color_left.comp.b, color_right.comp.b),
            interpolate_component(color_left.comp.a, color_right.comp.a),
        };
    }};

    int64_t lighting_origin {numeric_cast<int64_t>(lighting_size.width) / 2};
    int64_t frame_origin {numeric_cast<int64_t>(_frameSize.width) / 2};
    int64_t crop_left {lighting_origin + _cropRect.x - frame_origin};
    ucolor crop_color_left {interpolate_color(crop_left)};
    ucolor crop_color_right {interpolate_color(crop_left + _cropRect.width)};
    return AtlasSprite::FillData(dbuf, pos, {crop_color_left, crop_color_right});
}

void ModelSprite::Prewarm()
{
    FO_TRACE_ZONE(Model);

    _model->PrewarmParticles();
    _model->RequestRedraw();
}

void ModelSprite::SetDirection(mdir dir)
{
    _model->SetLookDir(dir);
    _model->SetMoveDir(dir, true);
}

void ModelSprite::Play(hstring anim_name, bool looped, bool reversed)
{
    ignore_unused(anim_name);
    ignore_unused(looped);
    ignore_unused(reversed);

    StartUpdate();
}

void ModelSprite::Stop()
{
}

auto ModelSprite::Update() -> bool
{
    _model->PrepareFrameLayout();
    bool direct_draw {IsDirectDraw()};

    if (_model->NeedForceDraw() || (!direct_draw && _model->NeedDraw())) {
        DrawToAtlas();
    }

    RefreshHitMask();

    return true;
}

void ModelSprite::SetSize(isize32 size)
{
    FO_VERIFY_AND_THROW(size.width > 0, "Size width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Size height must be positive", size.height);

    int32_t max_draw_width {AppRender::MAX_ATLAS_WIDTH / ModelInstance::FRAME_SCALE};
    int32_t max_draw_height {AppRender::MAX_ATLAS_HEIGHT / ModelInstance::FRAME_SCALE};
    FO_VERIFY_AND_THROW(size.width <= max_draw_width, "Requested model sprite width exceeds the renderable frame", size.width, max_draw_width);
    FO_VERIFY_AND_THROW(size.height <= max_draw_height, "Requested model sprite height exceeds the renderable frame", size.height, max_draw_height);

    _requestedFrameSize = size;
    _model->RequestRedraw();
}

void ModelSprite::DrawToAtlas()
{
    _factory->DrawModelToAtlas(this);

    // Redraws alone do not request readback; picking asks for a newer mask when needed
    _hitMask.MarkStale();
}

void ModelSprite::DrawInScene(fpos32 scene_pos, float32_t depth) const
{
    const auto& settings {*_factory->_settings};
    mat44 scene_ortho {_sprMngr->GetRender().GetProjMatrix()};
    mat44 cam_view {GeometryHelper::MakeMapCameraView(settings.Geometry.MapCameraAngle, 0.0f, fpos32 {0.0f, 0.0f}, 1.0f)};
    mat44 proj_base {scene_ortho * cam_view};
    mat44 proj {GeometryHelper::MakeMapAnchoredProj(proj_base, scene_ortho, scene_pos, depth)};

    _model->DrawInScene(proj, settings.Render.ModelProjFactor);
}

void ModelSprite::SetupFrame(isize32 frame_size)
{
    FO_VERIFY_AND_THROW(frame_size.width > 0, "Frame width must be positive", frame_size.width);
    FO_VERIFY_AND_THROW(frame_size.height > 0, "Frame height must be positive", frame_size.height);

    if (frame_size == _frameSize) {
        return;
    }

    if (_model->GetDrawSize() != frame_size) {
        _model->SetupFrame(frame_size, _model->GetFramePivot());
    }

    _frameSize = frame_size;
    _boundedCropEstablished = false;
    _cropEnvelopeId.reset();
}

auto ModelSprite::PrepareFrameCrop(isize32 frame_size, optional<ModelSpriteBounds> bounds) -> PreparedFrameCrop
{
    FO_VERIFY_AND_THROW(frame_size.width > 0, "Frame width must be positive", frame_size.width);
    FO_VERIFY_AND_THROW(frame_size.height > 0, "Frame height must be positive", frame_size.height);

    if (IsDirectDraw()) {
        bounds.reset();
    }

    irect32 full_frame_crop {0, 0, frame_size.width, frame_size.height};
    irect32 normalized_crop {full_frame_crop};
    bool has_bounded_crop {false};
    optional<ModelSpriteBoundsEnvelopeId> envelope_id {};

    if (bounds && bounds->Rect.width > 0 && bounds->Rect.height > 0) {
        const irect32& crop_rect {bounds->Rect};
        int64_t crop_left {std::clamp<int64_t>(crop_rect.x, 0, frame_size.width)};
        int64_t crop_top {std::clamp<int64_t>(crop_rect.y, 0, frame_size.height)};
        int64_t crop_right {std::clamp<int64_t>(numeric_cast<int64_t>(crop_rect.x) + crop_rect.width, 0, frame_size.width)};
        int64_t crop_bottom {std::clamp<int64_t>(numeric_cast<int64_t>(crop_rect.y) + crop_rect.height, 0, frame_size.height)};

        if (crop_right > crop_left && crop_bottom > crop_top) {
            normalized_crop = {
                numeric_cast<int32_t>(crop_left),
                numeric_cast<int32_t>(crop_top),
                numeric_cast<int32_t>(crop_right - crop_left),
                numeric_cast<int32_t>(crop_bottom - crop_top),
            };
            has_bounded_crop = true;
            envelope_id = bounds->EnvelopeId;
        }
    }

    bool same_frame {frame_size == _frameSize};
    bool same_envelope {envelope_id && _cropEnvelopeId && envelope_id->BodyAnimationIndices == _cropEnvelopeId->BodyAnimationIndices && envelope_id->MoveAnimationIndices == _cropEnvelopeId->MoveAnimationIndices && envelope_id->CombinedMeshGenerationRevision == _cropEnvelopeId->CombinedMeshGenerationRevision && envelope_id->BodyAnimationCount == _cropEnvelopeId->BodyAnimationCount && envelope_id->MoveAnimationCount == _cropEnvelopeId->MoveAnimationCount && envelope_id->ShadowEnabled == _cropEnvelopeId->ShadowEnabled && envelope_id->FullFrame == _cropEnvelopeId->FullFrame};

    if (same_frame && same_envelope && has_bounded_crop && _boundedCropEstablished) {
        // Keep the slot stable across small pose-to-pose bounds changes
        normalized_crop.expand(_cropRect);
    }

    // Anchor the sprite on the model origin's exact pixel inside the frame (its ground point), not a fixed
    // centre-x / three-quarter-y fraction: X keeps the crop centred on the origin, Y hangs it from the origin row
    ipos32 pivot {_model->GetFramePivot()};
    int32_t offset_x {numeric_cast<int32_t>(numeric_cast<int64_t>(normalized_crop.x) + normalized_crop.width / 2 - pivot.x)};
    int32_t offset_y {numeric_cast<int32_t>(numeric_cast<int64_t>(normalized_crop.y) + normalized_crop.height - pivot.y)};
    PreparedFrameCrop prepared_crop {
        .FrameSize = frame_size,
        .CropRect = normalized_crop,
        .Size = normalized_crop.size(),
        .Offset = {offset_x, offset_y},
        .BoundedCropEstablished = has_bounded_crop,
        .CropEnvelopeId = envelope_id,
    };

    if (_atlasAllocation && same_frame && normalized_crop == _cropRect) {
        prepared_crop.Atlas = _atlas;
        prepared_crop.AtlasRect = _atlasRect;
        prepared_crop.ReuseAtlasAllocation = true;
        return prepared_crop;
    }

    auto&& [atlas, atlas_allocation, pos] {_sprMngr->GetAtlasMngr()->FindAtlasPlace(_atlasType, normalized_crop.size())};

    prepared_crop.Atlas = atlas;
    prepared_crop.AtlasRect.x = numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(atlas->GetSize().width);
    prepared_crop.AtlasRect.y = numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(atlas->GetSize().height);
    prepared_crop.AtlasRect.width = numeric_cast<float32_t>(normalized_crop.width) / numeric_cast<float32_t>(atlas->GetSize().width);
    prepared_crop.AtlasRect.height = numeric_cast<float32_t>(normalized_crop.height) / numeric_cast<float32_t>(atlas->GetSize().height);
    prepared_crop.AtlasAllocation = std::move(atlas_allocation);
    return prepared_crop;
}

void ModelSprite::CommitFrameCrop(PreparedFrameCrop&& prepared_crop)
{
    SetupFrame(prepared_crop.FrameSize);
    _cropRect = prepared_crop.CropRect;
    _size = prepared_crop.Size;
    _offset = prepared_crop.Offset;
    _boundedCropEstablished = prepared_crop.BoundedCropEstablished;
    _cropEnvelopeId = prepared_crop.CropEnvelopeId;

    if (!prepared_crop.ReuseAtlasAllocation) {
        _atlas = prepared_crop.Atlas;
        _atlasRect = prepared_crop.AtlasRect;
        _atlasAllocation = std::move(prepared_crop.AtlasAllocation);
    }
}

void ModelSprite::ApplyFrameCrop(isize32 frame_size, optional<ModelSpriteBounds> bounds)
{
    CommitFrameCrop(PrepareFrameCrop(frame_size, bounds));
}

ModelSpriteFactory::ModelSpriteFactory(ptr<SpriteManager> spr_mngr, ptr<ResourceManager> res_mngr, ptr<RenderSettings> settings, ptr<const EngineMetadata> engine_metadata, ptr<EffectManager> effect_mngr, ptr<GameTimer> game_time, ptr<AnimationResolver> anim_name_resolver) :
    _sprMngr {spr_mngr},
    _resMngr {res_mngr},
    _settings {settings},
    _effectMngr {effect_mngr},
    _modelMngr {safe_alloc::make_unique<ModelManager>(
        settings, res_mngr->GetResources(), engine_metadata, effect_mngr, &spr_mngr->GetRender(), game_time, anim_name_resolver, //
        [this, engine_metadata](string_view path) mutable FO_DEFERRED { return LoadTexture(engine_metadata->Hashes.to_hashed_string(path)); }, //
        [spr_mngr]() mutable FO_DEFERRED { return spr_mngr->IsDrawWireframe(); }, //
        [spr_mngr]() mutable FO_DEFERRED {
            nptr<const RenderTexture> texture {spr_mngr->AcquireSceneBackground()};
            return ParticleSceneBackgroundResult {.State = texture ? ParticleSceneBackgroundState::Available : ParticleSceneBackgroundState::Unavailable, .Texture = texture};
        })}
{
}

ModelSpriteFactory::~ModelSpriteFactory() = default;

auto ModelSpriteFactory::GetModelMngr() -> ptr<ModelManager>
{
    return _modelMngr;
}

auto ModelSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    FO_TRACE_ZONE(Model);

    auto model {_modelMngr->CreateModel(path)};

    if (!model) {
        return nullptr;
    }

    model->PrepareFrameLayout();
    isize32 draw_size {model->GetDrawSize()};
    auto model_owner {model.take_not_null()};
    auto model_spr {safe_alloc::make_shared<ModelSprite>(_sprMngr, this, std::move(model_owner), atlas_type)};
    model_spr->ApplyFrameCrop(draw_size, model_spr->_model->GetSpriteBounds());

    return model_spr;
}

void ModelSpriteFactory::ClenupCache()
{
    FO_TRACE_ZONE(Model);

    _sprMngr->Flush();

    while (!_rtIntermediate.empty()) {
        ptr<RenderTarget> rt {_rtIntermediate.back()};
        _rtIntermediate.pop_back();
        ReleaseScratchRenderTarget(rt);
    }

    for (nptr<RenderTarget>& rt : _rtShadowBlur) {
        if (rt) {
            ptr<RenderTarget> blur_target {rt};
            rt.reset();
            ReleaseScratchRenderTarget(blur_target);
        }
    }
}

auto ModelSpriteFactory::AcquireIntermediateRenderTarget(isize32 size) -> ptr<RenderTarget>
{
    FO_TRACE_ZONE(Model);

    // 32 MiB of RGBA colour storage, plus backend depth storage. One oversized frame may use the cache alone
    constexpr uint64_t pixel_budget {8 * 1024 * 1024};
    uint64_t cached_pixels {0};

    for (auto it {_rtIntermediate.begin()}; it != _rtIntermediate.end(); ++it) {
        isize32 cached_size {(*it)->GetTexture()->Size};

        if (cached_size == size) {
            std::rotate(it, std::next(it), _rtIntermediate.end());
            return _rtIntermediate.back();
        }

        cached_pixels += numeric_cast<uint64_t>(cached_size.width) * numeric_cast<uint64_t>(cached_size.height);
    }

    uint64_t requested_pixels {numeric_cast<uint64_t>(size.width) * numeric_cast<uint64_t>(size.height)};

    while (!_rtIntermediate.empty() && cached_pixels + requested_pixels > pixel_budget) {
        _sprMngr->Flush();
        ptr<RenderTarget> rt {_rtIntermediate.front()};
        isize32 retired_size {rt->GetTexture()->Size};
        cached_pixels -= numeric_cast<uint64_t>(retired_size.width) * numeric_cast<uint64_t>(retired_size.height);
        _rtIntermediate.erase(_rtIntermediate.begin());
        ReleaseScratchRenderTarget(rt);
    }

    ptr<RenderTarget> rt {_sprMngr->GetRtMngr().CreateRenderTarget(true, size, true)};
    _rtIntermediate.emplace_back(rt);
    return rt;
}

void ModelSpriteFactory::ReleaseScratchRenderTarget(ptr<RenderTarget> rt)
{
    FO_TRACE_ZONE(Model);

    // DrawTexture stores this borrow after the immediate draw; discard it before retiring its owner
    nptr<const RenderTexture> texture {rt->GetTexture()};

    for (nptr<RenderEffect> effect : {_effectMngr->Effects.FlushRenderTarget, _effectMngr->Effects.ModelShadowBlur}) {
        if (effect && effect->MainTex == texture) {
            effect->MainTex.reset();
        }
    }

    _sprMngr->GetRtMngr().DeleteRenderTarget(rt);
}

// Blur targets stay outside the intermediate cache: a pass reads one and writes the other, and the cache hands out
// a single target per size
auto ModelSpriteFactory::AcquireShadowBlurTarget(size_t index, isize32 size) -> ptr<RenderTarget>
{
    FO_TRACE_ZONE(Model);

    isize32 aligned_size {(size.width + MODEL_SHADOW_BLUR_TARGET_ALIGNMENT - 1) / MODEL_SHADOW_BLUR_TARGET_ALIGNMENT * MODEL_SHADOW_BLUR_TARGET_ALIGNMENT, (size.height + MODEL_SHADOW_BLUR_TARGET_ALIGNMENT - 1) / MODEL_SHADOW_BLUR_TARGET_ALIGNMENT * MODEL_SHADOW_BLUR_TARGET_ALIGNMENT};
    nptr<RenderTarget>& rt {_rtShadowBlur.at(index)};

    if (!rt) {
        rt = _sprMngr->GetRtMngr().CreateRenderTarget(false, aligned_size, true);
    }
    else if (rt->GetSize().width < aligned_size.width || rt->GetSize().height < aligned_size.height) {
        // Targets only grow, so a later smaller frame reuses the allocation instead of recreating it every redraw
        isize32 grown_size {std::max(rt->GetSize().width, aligned_size.width), std::max(rt->GetSize().height, aligned_size.height)};
        _sprMngr->GetRtMngr().ResizeRenderTarget(rt, grown_size);
    }

    return rt;
}

auto ModelSpriteFactory::RenderBlurredShadow(ptr<ModelInstance> model, isize32 mask_size) -> nptr<const RenderTexture>
{
    FO_TRACE_ZONE(Model);

    // The frame projection fills any viewport, so the half-size target receives the same picture at one texel per
    // logical pixel
    ptr<RenderTarget> rt_mask {AcquireIntermediateRenderTarget(mask_size)};

    _sprMngr->GetRtMngr().PushRenderTarget(rt_mask);
    auto pop_mask_rt_on_fail {scope_fail([this]() noexcept { safe_call([this] { _sprMngr->GetRtMngr().PopRenderTarget(); }); })};

    // The shadow pass tests and writes depth, and a leased target still carries the previous draw's depth
    _sprMngr->GetRtMngr().ClearCurrentRenderTarget(ucolor::clear, true);
    model->DrawSpriteFrame(ModelSpritePasses::Shadow);
    _sprMngr->GetRtMngr().PopRenderTarget();
    pop_mask_rt_on_fail.release();

    nptr<const RenderTexture> shadow_texture {rt_mask->GetTexture()};
    float32_t blur {model->GetShadowBlur()};
    int32_t iteration_count {CalculateModelShadowBlurPassCount(blur)};
    // Iterations share the requested sigma, and the kernel turns a pass sigma into its tap stride
    float32_t pass_stride {blur * 0.5f / std::sqrt(numeric_cast<float32_t>(iteration_count)) * MODEL_SHADOW_BLUR_STRIDE_FACTOR};

    for (int32_t blur_pass {0}; blur_pass < iteration_count * 2; blur_pass++) {
        size_t target_index {numeric_cast<size_t>(blur_pass % 2)};
        ptr<RenderTarget> rt_blur {AcquireShadowBlurTarget(target_index, mask_size)};
        BlurShadow(shadow_texture, rt_blur, mask_size, blur_pass % 2 == 0 ? fpos32 {1.0f, 0.0f} : fpos32 {0.0f, 1.0f}, pass_stride);
        shadow_texture = rt_blur->GetTexture();
    }

    return shadow_texture;
}

void ModelSpriteFactory::BlurShadow(nptr<const RenderTexture> source, ptr<RenderTarget> target, isize32 mask_size, fpos32 direction, float32_t stride)
{
    FO_TRACE_ZONE(Model);

    nptr<RenderEffect> effect {_effectMngr->Effects.ModelShadowBlur};
    FO_VERIFY_AND_THROW(effect, "Model shadow blur effect is not loaded");

    _sprMngr->GetRtMngr().PushRenderTarget(target);
    auto pop_target_on_fail {scope_fail([this]() noexcept { safe_call([this] { _sprMngr->GetRtMngr().PopRenderTarget(); }); })};
    auto& script_values {effect->ScriptValueBuf = RenderEffect::ScriptValueBuffer()};
    script_values->ScriptValue[0] = direction.x;
    script_values->ScriptValue[1] = direction.y;
    script_values->ScriptValue[2] = stride;

    // Only the mask-sized corner of a grown source or target takes part, and the taps stay inside it: the corner's
    // edge is clear, while whatever an earlier larger frame left beside it is not. The backend fills the texture size
    // buffer itself
    isize32 source_size {source->Size};
    frect32 mask_region_from {0.0f, 0.0f, numeric_cast<float32_t>(mask_size.width), numeric_cast<float32_t>(mask_size.height)};
    irect32 mask_region_to {0, 0, mask_size.width, mask_size.height};
    auto& sprite_border {effect->SpriteBorderBuf = RenderEffect::SpriteBorderBuffer()};
    sprite_border->SpriteBorder[0] = 0.0f;
    sprite_border->SpriteBorder[1] = source->FlippedHeight ? 1.0f - mask_region_from.height / numeric_cast<float32_t>(source_size.height) : 0.0f;
    sprite_border->SpriteBorder[2] = mask_region_from.width / numeric_cast<float32_t>(source_size.width);
    sprite_border->SpriteBorder[3] = source->FlippedHeight ? 1.0f : mask_region_from.height / numeric_cast<float32_t>(source_size.height);
    _sprMngr->DrawTexture(source, false, &mask_region_from, &mask_region_to, effect);
    _sprMngr->GetRtMngr().PopRenderTarget();
    pop_target_on_fail.release();
}

auto ModelSpriteFactory::LoadTexture(hstring path) -> pair<nptr<RenderTexture>, frect32>
{
    FO_TRACE_ZONE(Model);

    auto result {pair<nptr<RenderTexture>, frect32>()};

    if (auto it {_loadedMeshTextures.find(path)}; it == _loadedMeshTextures.end()) {
        // Model UVs address the complete source bitmap; this callback cannot carry a cropped frame's SourceOffset
        auto atlas_spr {_resMngr->LoadSpriteAsQuad(path, AtlasType::MeshTextures)};

        if (atlas_spr) {
            _loadedMeshTextures[path] = atlas_spr;
            result = {atlas_spr->GetAtlas()->GetTexture(), atlas_spr->GetAtlasRect()};
        }
        else {
            break_into_debugger();
            logging::write(logging::type::warning, "Texture '{}' not found", path);
            _loadedMeshTextures[path] = nullptr;
        }
    }
    else if (auto& atlas_spr {it->second}) {
        result = {atlas_spr->GetAtlas()->GetTexture(), atlas_spr->GetAtlasRect()};
    }

    return result;
}

void ModelSpriteFactory::DrawModelToAtlas(ptr<ModelSprite> model_spr)
{
    FO_TRACE_ZONE(Model);

    auto request_redraw_on_fail {scope_fail([model = model_spr->GetModel()]() mutable noexcept { model->RequestRedraw(); })};
    model_spr->GetModel()->PrepareFrameLayout();
    isize32 max_logical_frame {ResolveModelSpriteMaxLogicalFrame(_settings->Render.ModelSpriteMaxTextureWidth, _settings->Render.ModelSpriteMaxTextureHeight, AppRender::MAX_ATLAS_WIDTH, AppRender::MAX_ATLAS_HEIGHT)};
    isize32 render_frame_size {model_spr->_requestedFrameSize.value_or(model_spr->GetModel()->GetDrawSize())};
    ModelSpriteFramePlacement start_placement {ClampModelSpriteFramePlacement({.Size = render_frame_size, .Pivot = model_spr->GetModel()->GetFramePivot()}, max_logical_frame)};
    render_frame_size = start_placement.Size;

    if (model_spr->GetModel()->GetDrawSize() != render_frame_size) {
        model_spr->GetModel()->SetupFrame(render_frame_size, start_placement.Pivot);
    }

    // The frame size is known from the posed skeleton without any GPU read-back, so the model is posed and measured
    // first; placements merge in root-relative coordinates, or rounding could alternate pivots forever
    optional<ModelSpriteBounds> bounds {};

    for (size_t size_pass {0}; size_pass < 3; size_pass++) {
        model_spr->GetModel()->PoseSpriteFrame(size_pass == 0);
        bounds = model_spr->_model->GetSpriteBounds();

        if (bounds) {
            ModelSpriteFramePlacement current_placement {
                .Size = render_frame_size,
                .Pivot = model_spr->GetModel()->GetFramePivot(),
            };
            ModelSpriteFramePlacement required_placement {.Size = bounds->RequiredFrameSize, .Pivot = bounds->Pivot};
            optional<ModelSpriteFramePlacement> settled_placement {MergeModelSpriteFramePlacements(current_placement, required_placement)};
            FO_VERIFY_AND_THROW(settled_placement, "Model sprite frame placements could not be merged", current_placement.Size, current_placement.Pivot, required_placement.Size, required_placement.Pivot);
            *settled_placement = ClampModelSpriteFramePlacement(*settled_placement, max_logical_frame);

            // A full-frame particle crop may already have enough pixels but still need its root moved inside that frame
            if (settled_placement->Size != current_placement.Size || settled_placement->Pivot != current_placement.Pivot) {
                FO_VERIFY_AND_THROW(size_pass + 1 < 3, "Model sprite frame did not converge after expansion", current_placement.Size, current_placement.Pivot, required_placement.Size, required_placement.Pivot, settled_placement->Size, settled_placement->Pivot);
                model_spr->_model->SetupFrame(settled_placement->Size, settled_placement->Pivot);
                render_frame_size = settled_placement->Size;
                continue;
            }
        }

        break;
    }

    // Render the posed model once, at the settled size, into the full logical frame before applying the tight atlas crop
    isize32 frame_size {render_frame_size.width * ModelInstance::FRAME_SCALE, render_frame_size.height * ModelInstance::FRAME_SCALE};
    FO_VERIFY_AND_THROW(frame_size.width <= AppRender::MAX_ATLAS_WIDTH && frame_size.height <= AppRender::MAX_ATLAS_HEIGHT, "Model sprite frame exceeds the device texture limit", frame_size.width, frame_size.height, render_frame_size.width, render_frame_size.height, ModelInstance::FRAME_SCALE, AppRender::MAX_ATLAS_WIDTH, AppRender::MAX_ATLAS_HEIGHT);

    // The mask is drawn and blurred before the frame target is leased, so the intermediate cache may retire the mask
    // target to make room for the frame
    isize32 mask_size {render_frame_size};
    nptr<const RenderTexture> shadow_texture {};
    float32_t shadow_blur {model_spr->GetModel()->GetShadowBlur()};

    if (shadow_blur > 0.0f) {
        shadow_texture = RenderBlurredShadow(model_spr->GetModel(), mask_size);
    }

    ptr<RenderTarget> rt_model {AcquireIntermediateRenderTarget(frame_size)};

    _sprMngr->GetRtMngr().PushRenderTarget(rt_model);
    auto pop_model_rt_on_fail {scope_fail([this]() noexcept { safe_call([this] { _sprMngr->GetRtMngr().PopRenderTarget(); }); })};
    _sprMngr->GetRtMngr().ClearCurrentRenderTarget(ucolor::clear, true);

    if (shadow_texture) {
        frect32 shadow_region {0.0f, 0.0f, numeric_cast<float32_t>(mask_size.width), numeric_cast<float32_t>(mask_size.height)};
        _sprMngr->DrawTexture(shadow_texture, false, &shadow_region, nullptr);
    }

    model_spr->GetModel()->DrawSpriteFrame(shadow_texture ? ModelSpritePasses::Body : ModelSpritePasses::All);
    _sprMngr->GetRtMngr().PopRenderTarget();
    pop_model_rt_on_fail.release();

    auto prepared_crop {model_spr->PrepareFrameCrop(render_frame_size, bounds)};

    // Copy render
    int32_t l {iround<int32_t>(prepared_crop.AtlasRect.x * numeric_cast<float32_t>(prepared_crop.Atlas->GetSize().width))};
    int32_t t {iround<int32_t>(prepared_crop.AtlasRect.y * numeric_cast<float32_t>(prepared_crop.Atlas->GetSize().height))};
    int32_t w {iround<int32_t>(prepared_crop.AtlasRect.width * numeric_cast<float32_t>(prepared_crop.Atlas->GetSize().width))};
    int32_t h {iround<int32_t>(prepared_crop.AtlasRect.height * numeric_cast<float32_t>(prepared_crop.Atlas->GetSize().height))};
    irect32 region_to {irect32(l, t, w, h)};
    float32_t frame_scale {numeric_cast<float32_t>(ModelInstance::FRAME_SCALE)};
    frect32 region_from {
        numeric_cast<float32_t>(prepared_crop.CropRect.x) * frame_scale,
        numeric_cast<float32_t>(prepared_crop.CropRect.y) * frame_scale,
        numeric_cast<float32_t>(prepared_crop.CropRect.width) * frame_scale,
        numeric_cast<float32_t>(prepared_crop.CropRect.height) * frame_scale,
    };

    _sprMngr->GetRtMngr().PushRenderTarget(prepared_crop.Atlas->GetRenderTarget());
    auto pop_atlas_rt_on_fail {scope_fail([this]() noexcept { safe_call([this] { _sprMngr->GetRtMngr().PopRenderTarget(); }); })};
    _sprMngr->DrawRenderTarget(rt_model, false, &region_from, &region_to);
    _sprMngr->GetRtMngr().PopRenderTarget();
    pop_atlas_rt_on_fail.release();

    model_spr->CommitFrameCrop(std::move(prepared_crop));
    model_spr->_requestedFrameSize.reset();
    request_redraw_on_fail.release();
}

FO_END_NAMESPACE

#endif
