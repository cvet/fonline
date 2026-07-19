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

#include "ModelSprites.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "EngineBase.h"
#include "Geometry.h"
#include "ModelInstance.h"
#include "ModelManager.h"

FO_BEGIN_NAMESPACE

ModelSprite::ModelSprite(ptr<SpriteManager> spr_mngr, ptr<ModelSpriteFactory> factory, unique_ptr<ModelInstance> model, AtlasType atlas_type) :
    AtlasSprite(spr_mngr, {}, {}, {}, {}, {}, {}),
    _factory {factory},
    _model {std::move(model)},
    _atlasType {atlas_type}
{
    FO_STACK_TRACE_ENTRY();
}

ModelSprite::~ModelSprite() = default;

auto ModelSprite::GetModel() -> ptr<ModelInstance>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _model;
}

auto ModelSprite::IsPlaying() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _model->IsAnimationPlaying();
}

auto ModelSprite::IsHitTest(ipos32 pos) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_size.is_valid_pos(pos)) {
        return false;
    }

    const auto atlas_x = pos.x + iround<int32_t>(GetAtlas()->GetTexture()->SizeData[0] * GetAtlasRect().x);
    const auto atlas_y = pos.y + iround<int32_t>(GetAtlas()->GetTexture()->SizeData[1] * GetAtlasRect().y);

    const auto alpha = _sprMngr->GetRtMngr().GetRenderTargetPixel(GetAtlas()->GetRenderTarget(), {atlas_x, atlas_y}).comp.a;
    return _sprMngr->CheckHitTest(numeric_cast<int32_t>(alpha));
}

auto ModelSprite::GetViewSize() const -> optional<irect32>
{
    FO_NO_STACK_TRACE_ENTRY();

    const irect32 view_rect = _model->GetViewRect();

    return irect32 {
        view_rect.x + view_rect.width / 2 - _offset.x,
        view_rect.y + view_rect.height - _offset.y,
        view_rect.width,
        view_rect.height,
    };
}

auto ModelSprite::IsDirectDraw() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _factory->_settings->ModelDirectDraw;
}

auto ModelSprite::FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_frameSize.width > 0 && _frameSize.height > 0 && _cropRect.x >= 0 && _cropRect.y >= 0 && _cropRect.width > 0 && _cropRect.height > 0 && numeric_cast<int64_t>(_cropRect.x) + _cropRect.width <= _frameSize.width && numeric_cast<int64_t>(_cropRect.y) + _cropRect.height <= _frameSize.height, "Model sprite crop is outside the logical frame", _cropRect, _frameSize);

    const isize32 lighting_size = _model->GetLightingSize();
    FO_STRONG_ASSERT(lighting_size.width > 0, "Model sprite lighting frame width must be positive", lighting_size.width);

    const uint32_t color_width = numeric_cast<uint32_t>(lighting_size.width);
    const ucolor color_left = std::get<0>(colors);
    const ucolor color_right = std::get<1>(colors);
    const auto interpolate_color = [color_width, color_left, color_right](int64_t source_x) noexcept -> ucolor {
        const uint32_t color_x = numeric_cast<uint32_t>(std::clamp<int64_t>(source_x, 0, color_width));
        const auto interpolate_component = [color_x, color_width](uint8_t left_component, uint8_t right_component) noexcept -> uint8_t {
            const uint32_t weighted = numeric_cast<uint32_t>(left_component) * (color_width - color_x) + numeric_cast<uint32_t>(right_component) * color_x;
            return numeric_cast<uint8_t>((weighted + color_width / 2) / color_width);
        };

        return {
            interpolate_component(color_left.comp.r, color_right.comp.r),
            interpolate_component(color_left.comp.g, color_right.comp.g),
            interpolate_component(color_left.comp.b, color_right.comp.b),
            interpolate_component(color_left.comp.a, color_right.comp.a),
        };
    };

    const int64_t lighting_origin = numeric_cast<int64_t>(lighting_size.width) / 2;
    const int64_t frame_origin = numeric_cast<int64_t>(_frameSize.width) / 2;
    const int64_t crop_left = lighting_origin + _cropRect.x - frame_origin;
    const ucolor crop_color_left = interpolate_color(crop_left);
    const ucolor crop_color_right = interpolate_color(crop_left + _cropRect.width);
    return AtlasSprite::FillData(dbuf, pos, {crop_color_left, crop_color_right});
}

void ModelSprite::Prewarm()
{
    FO_STACK_TRACE_ENTRY();

    _model->PrewarmParticles();
}

void ModelSprite::SetDir(mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    _model->SetLookDir(dir);
    _model->SetMoveDir(dir, true);
}

void ModelSprite::Play(hstring anim_name, bool looped, bool reversed)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(anim_name);
    ignore_unused(looped);
    ignore_unused(reversed);

    StartUpdate();
}

void ModelSprite::Stop()
{
    FO_STACK_TRACE_ENTRY();
}

auto ModelSprite::Update() -> bool
{
    FO_STACK_TRACE_ENTRY();

    _model->PrepareFrameLayout();
    const bool direct_draw = IsDirectDraw();

    if (_model->NeedForceDraw() || (!direct_draw && _model->NeedDraw())) {
        DrawToAtlas();
    }

    return true;
}

void ModelSprite::SetSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0, "Size width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Size height must be positive", size.height);

    _requestedFrameSize = size;
    _model->RequestRedraw();
}

void ModelSprite::DrawToAtlas()
{
    FO_STACK_TRACE_ENTRY();

    _factory->DrawModelToAtlas(this);
}

void ModelSprite::DrawInScene(fpos32 scene_pos, float32_t depth) const
{
    FO_STACK_TRACE_ENTRY();

    const auto& settings = *_factory->_settings;
    const mat44 scene_ortho = _sprMngr->GetRender().GetProjMatrix();
    const mat44 cam_view = GeometryHelper::MakeMapCameraView(settings.MapCameraAngle, 0.0f, fpos32 {0.0f, 0.0f}, 1.0f);
    const mat44 proj_base = scene_ortho * cam_view;
    const mat44 proj = GeometryHelper::MakeMapAnchoredProj(proj_base, scene_ortho, scene_pos, depth);

    _model->Draw(proj, settings.ModelProjFactor);
}

void ModelSprite::SetupFrame(isize32 frame_size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(frame_size.width > 0, "Frame width must be positive", frame_size.width);
    FO_VERIFY_AND_THROW(frame_size.height > 0, "Frame height must be positive", frame_size.height);

    if (frame_size == _frameSize) {
        return;
    }

    if (_model->GetDrawSize() != frame_size) {
        _model->SetupFrame(frame_size);
    }
    _frameSize = frame_size;
    _boundedCropEstablished = false;
    _cropEnvelopeId.reset();
}

auto ModelSprite::PrepareFrameCrop(isize32 frame_size, optional<ModelSpriteBounds> bounds) -> PreparedFrameCrop
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(frame_size.width > 0, "Frame width must be positive", frame_size.width);
    FO_VERIFY_AND_THROW(frame_size.height > 0, "Frame height must be positive", frame_size.height);

    if (IsDirectDraw()) {
        bounds.reset();
    }

    const irect32 full_frame_crop = {0, 0, frame_size.width, frame_size.height};
    irect32 normalized_crop = full_frame_crop;
    bool has_bounded_crop = false;
    optional<ModelSpriteBoundsEnvelopeId> envelope_id {};

    if (bounds && bounds->Rect.width > 0 && bounds->Rect.height > 0) {
        const auto& crop_rect = bounds->Rect;
        const int64_t crop_left = std::clamp<int64_t>(crop_rect.x, 0, frame_size.width);
        const int64_t crop_top = std::clamp<int64_t>(crop_rect.y, 0, frame_size.height);
        const int64_t crop_right = std::clamp<int64_t>(numeric_cast<int64_t>(crop_rect.x) + crop_rect.width, 0, frame_size.width);
        const int64_t crop_bottom = std::clamp<int64_t>(numeric_cast<int64_t>(crop_rect.y) + crop_rect.height, 0, frame_size.height);

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

    const bool same_frame = frame_size == _frameSize;
    const bool same_envelope = envelope_id && _cropEnvelopeId && envelope_id->BodyAnimationIndices == _cropEnvelopeId->BodyAnimationIndices && envelope_id->MoveAnimationIndices == _cropEnvelopeId->MoveAnimationIndices && envelope_id->CombinedMeshGenerationRevision == _cropEnvelopeId->CombinedMeshGenerationRevision && envelope_id->BodyAnimationCount == _cropEnvelopeId->BodyAnimationCount && envelope_id->MoveAnimationCount == _cropEnvelopeId->MoveAnimationCount && envelope_id->ShadowEnabled == _cropEnvelopeId->ShadowEnabled && envelope_id->FullFrame == _cropEnvelopeId->FullFrame;

    if (same_frame && same_envelope && has_bounded_crop && _boundedCropEstablished) {
        // Keep the slot stable across small pose-to-pose bounds changes.
        normalized_crop.expand(_cropRect);
    }

    const int32_t offset_x = numeric_cast<int32_t>(numeric_cast<int64_t>(normalized_crop.x) + normalized_crop.width / 2 - frame_size.width / 2);
    const int32_t offset_y = numeric_cast<int32_t>(numeric_cast<int64_t>(frame_size.height) / 4 + normalized_crop.y + normalized_crop.height - frame_size.height);
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

    auto&& [atlas, atlas_allocation, pos] = _sprMngr->GetAtlasMngr()->FindAtlasPlace(_atlasType, normalized_crop.size());

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    CommitFrameCrop(PrepareFrameCrop(frame_size, bounds));
}

ModelSpriteFactory::ModelSpriteFactory(ptr<SpriteManager> spr_mngr, ptr<RenderSettings> settings, ptr<const EngineMetadata> engine_metadata, ptr<EffectManager> effect_mngr, ptr<GameTimer> game_time, ptr<AnimationResolver> anim_name_resolver) :
    _sprMngr {spr_mngr},
    _settings {settings},
    _modelMngr {SafeAlloc::MakeUnique<ModelManager>(settings, spr_mngr->GetResources(), engine_metadata, effect_mngr, &spr_mngr->GetRender(), game_time, anim_name_resolver, //
        [this, engine_metadata](string_view path) mutable FO_DEFERRED { return LoadTexture(engine_metadata->Hashes.ToHashedString(path)); })}
{
    FO_STACK_TRACE_ENTRY();
}

ModelSpriteFactory::~ModelSpriteFactory() = default;

auto ModelSpriteFactory::GetModelMngr() -> ptr<ModelManager>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _modelMngr;
}

auto ModelSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    auto model = _modelMngr->CreateModel(path);

    if (!model) {
        return nullptr;
    }

    model->PrepareFrameLayout();
    const auto draw_size = model->GetDrawSize();
    auto model_owner = model.take_not_null();
    auto model_spr = SafeAlloc::MakeShared<ModelSprite>(_sprMngr, this, std::move(model_owner), atlas_type);
    model_spr->ApplyFrameCrop(draw_size, model_spr->_model->GetSpriteBounds());

    return model_spr;
}

auto ModelSpriteFactory::LoadTexture(hstring path) -> pair<nptr<RenderTexture>, frect32>
{
    FO_STACK_TRACE_ENTRY();

    auto result = pair<nptr<RenderTexture>, frect32>();

    if (const auto it = _loadedMeshTextures.find(path); it == _loadedMeshTextures.end()) {
        auto any_spr = _sprMngr->LoadSprite(path, AtlasType::MeshTextures);
        auto atlas_spr = any_spr.dyn_cast<AtlasSprite>();

        if (atlas_spr) {
            _loadedMeshTextures[path] = atlas_spr;
            result = {atlas_spr->GetAtlas()->GetTexture(), atlas_spr->GetAtlasRect()};
        }
        else {
            BreakIntoDebugger();
            WriteLog("Texture '{}' not found", path);
            _loadedMeshTextures[path] = nullptr;
        }
    }
    else if (auto& atlas_spr = it->second) {
        result = {atlas_spr->GetAtlas()->GetTexture(), atlas_spr->GetAtlasRect()};
    }

    return result;
}

void ModelSpriteFactory::DrawModelToAtlas(ptr<ModelSprite> model_spr)
{
    FO_STACK_TRACE_ENTRY();

    auto request_redraw_on_fail = scope_fail([model = model_spr->GetModel()]() mutable noexcept { model->RequestRedraw(); });
    model_spr->GetModel()->PrepareFrameLayout();
    isize32 render_frame_size = model_spr->_requestedFrameSize.value_or(model_spr->GetModel()->GetDrawSize());

    if (model_spr->GetModel()->GetDrawSize() != render_frame_size) {
        model_spr->GetModel()->SetupFrame(render_frame_size);
    }

    for (size_t render_attempt = 0; render_attempt < 3; render_attempt++) {
        // Render into the full logical frame before applying the tight atlas crop.
        const isize32 frame_size = {render_frame_size.width * ModelInstance::FRAME_SCALE, render_frame_size.height * ModelInstance::FRAME_SCALE};
        const ptr<RenderTarget> rt_model = [&]() -> ptr<RenderTarget> {
            for (ptr<RenderTarget> rt : _rtIntermediate) {
                if (rt->GetTexture()->Size == frame_size) {
                    return rt;
                }
            }

            auto rt = _sprMngr->GetRtMngr().CreateRenderTarget(true, frame_size, true);
            _rtIntermediate.emplace_back(rt);
            return rt;
        }();

        _sprMngr->GetRtMngr().PushRenderTarget(rt_model);
        auto pop_model_rt_on_fail = scope_fail([this]() noexcept { safe_call([this] { _sprMngr->GetRtMngr().PopRenderTarget(); }); });
        _sprMngr->GetRtMngr().ClearCurrentRenderTarget(ucolor::clear, true);

        // Draw model
        model_spr->GetModel()->Draw();

        // Restore render target
        _sprMngr->GetRtMngr().PopRenderTarget();
        pop_model_rt_on_fail.release();

        const optional<ModelSpriteBounds> bounds = model_spr->_model->GetSpriteBounds();

        if (bounds && (bounds->RequiredFrameSize.width > render_frame_size.width || bounds->RequiredFrameSize.height > render_frame_size.height)) {
            FO_VERIFY_AND_THROW(render_attempt + 1 < 3, "Model sprite frame did not converge after expansion", render_frame_size, bounds->RequiredFrameSize);
            model_spr->_model->SetupFrame(bounds->RequiredFrameSize);
            render_frame_size = bounds->RequiredFrameSize;
            continue;
        }

        auto prepared_crop = model_spr->PrepareFrameCrop(render_frame_size, bounds);

        // Copy render
        const int32_t l = iround<int32_t>(prepared_crop.AtlasRect.x * numeric_cast<float32_t>(prepared_crop.Atlas->GetSize().width));
        const int32_t t = iround<int32_t>(prepared_crop.AtlasRect.y * numeric_cast<float32_t>(prepared_crop.Atlas->GetSize().height));
        const int32_t w = iround<int32_t>(prepared_crop.AtlasRect.width * numeric_cast<float32_t>(prepared_crop.Atlas->GetSize().width));
        const int32_t h = iround<int32_t>(prepared_crop.AtlasRect.height * numeric_cast<float32_t>(prepared_crop.Atlas->GetSize().height));
        const irect32 region_to = irect32(l, t, w, h);
        const float32_t frame_scale = numeric_cast<float32_t>(ModelInstance::FRAME_SCALE);
        const frect32 region_from = {
            numeric_cast<float32_t>(prepared_crop.CropRect.x) * frame_scale,
            numeric_cast<float32_t>(prepared_crop.CropRect.y) * frame_scale,
            numeric_cast<float32_t>(prepared_crop.CropRect.width) * frame_scale,
            numeric_cast<float32_t>(prepared_crop.CropRect.height) * frame_scale,
        };

        _sprMngr->GetRtMngr().PushRenderTarget(prepared_crop.Atlas->GetRenderTarget());
        auto pop_atlas_rt_on_fail = scope_fail([this]() noexcept { safe_call([this] { _sprMngr->GetRtMngr().PopRenderTarget(); }); });
        prepared_crop.Atlas->GetRenderTarget()->ClearLastPixelPicks();
        _sprMngr->DrawRenderTarget(rt_model, false, &region_from, &region_to);
        _sprMngr->GetRtMngr().PopRenderTarget();
        pop_atlas_rt_on_fail.release();
        model_spr->CommitFrameCrop(std::move(prepared_crop));
        model_spr->_requestedFrameSize.reset();
        request_redraw_on_fail.release();
        return;
    }

    FO_UNREACHABLE_PLACE();
}

FO_END_NAMESPACE

#endif
