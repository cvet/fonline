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
#include "Application.h"
#include "Geometry.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

ModelSprite::ModelSprite(ptr<SpriteManager> spr_mngr, ptr<ModelSpriteFactory> factory, unique_ptr<ModelInstance> model, AtlasType atlas_type) :
    AtlasSprite(spr_mngr, {}, {}, {}, {}, {}, {}),
    _factory {factory},
    _model {std::move(model)},
    _atlasType {atlas_type}
{
    FO_STACK_TRACE_ENTRY();
}

auto ModelSprite::IsHitTest(ipos32 pos) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_size.is_valid_pos(pos)) {
        return false;
    }

    int32_t atlas_x = pos.x + iround<int32_t>(GetAtlas()->GetTexture()->SizeData[0] * GetAtlasRect().x);
    int32_t atlas_y = pos.y + iround<int32_t>(GetAtlas()->GetTexture()->SizeData[1] * GetAtlasRect().y);

    uint8_t alpha = _sprMngr->GetRtMngr().GetRenderTargetPixel(GetAtlas()->GetRenderTarget(), {atlas_x, atlas_y}).comp.a;
    return _sprMngr->CheckHitTest(numeric_cast<int32_t>(alpha));
}

auto ModelSprite::GetViewSize() const -> optional<irect32>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto&& [view_width, view_height] = _model->GetViewSize();

    return irect32 {0, -_offset.y + view_height / 8, view_width, view_height};
}

auto ModelSprite::IsDirectDraw() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _factory->_settings->ModelDirectDraw;
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

    if (_model->NeedForceDraw() || (!IsDirectDraw() && _model->NeedDraw())) {
        DrawToAtlas();
    }

    return true;
}

void ModelSprite::SetSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0, "Size width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Size height must be positive", size.height);

    if (size == _size) {
        return;
    }

    _model->SetupFrame(size);
    int16_t new_offset_y = numeric_cast<int16_t>(size.height / 4);

    if (_atlasNode) {
        _atlasNode.reset(); // Frees the previous atlas slot via the owning handle's deleter
        _atlas = nullptr;
        _atlasRect = {};
    }

    auto&& [atlas, atlas_node, pos] = _sprMngr->GetAtlasMngr()->FindAtlasPlace(_atlasType, size);

    frect32 new_rect;
    new_rect.x = numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(atlas->GetSize().width);
    new_rect.y = numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(atlas->GetSize().height);
    new_rect.width = numeric_cast<float32_t>(size.width) / numeric_cast<float32_t>(atlas->GetSize().width);
    new_rect.height = numeric_cast<float32_t>(size.height) / numeric_cast<float32_t>(atlas->GetSize().height);

    _size = size;
    _offset.y = new_offset_y;
    _atlas = atlas;
    _atlasNode = std::move(atlas_node);
    _atlasRect = new_rect;
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
    mat44 scene_ortho = _sprMngr->GetRender().GetProjMatrix();
    mat44 cam_view = GeometryHelper::MakeMapCameraView(settings.MapCameraAngle, 0.0f, fpos32 {0.0f, 0.0f}, 1.0f);
    mat44 proj_base = scene_ortho * cam_view;
    mat44 proj = GeometryHelper::MakeMapAnchoredProj(proj_base, scene_ortho, scene_pos, depth);

    _model->Draw(proj, settings.ModelProjFactor);
}

ModelSpriteFactory::ModelSpriteFactory(ptr<SpriteManager> spr_mngr, ptr<RenderSettings> settings, ptr<EffectManager> effect_mngr, ptr<GameTimer> game_time, ptr<HashResolver> hash_resolver, ptr<NameResolver> name_resolver, ptr<AnimationResolver> anim_name_resolver) :
    _sprMngr {spr_mngr},
    _settings {settings},
    _modelMngr {SafeAlloc::MakeUnique<ModelManager>(settings, spr_mngr->GetResources(), effect_mngr, &spr_mngr->GetRender(), game_time, hash_resolver, name_resolver, anim_name_resolver, //
        [this, hash_resolver](string_view path) mutable FO_DEFERRED { return LoadTexture(hash_resolver->ToHashedString(path)); })}
{
    FO_STACK_TRACE_ENTRY();
}

auto ModelSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    auto model = _modelMngr->CreateModel(path);

    if (!model) {
        return nullptr;
    }

    isize32 draw_size = model->GetDrawSize();
    auto model_owner = model.take_not_null();
    auto model_spr = SafeAlloc::MakeShared<ModelSprite>(_sprMngr, this, std::move(model_owner), atlas_type);
    model_spr->SetSize(draw_size);

    return model_spr;
}

auto ModelSpriteFactory::LoadTexture(hstring path) -> pair<nptr<RenderTexture>, frect32>
{
    FO_STACK_TRACE_ENTRY();

    auto result = pair<nptr<RenderTexture>, frect32>();

    if (auto it = _loadedMeshTextures.find(path); it == _loadedMeshTextures.end()) {
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

    // Find place for render
    isize32 frame_size = isize32 {model_spr->_size.width * ModelInstance::FRAME_SCALE, model_spr->_size.height * ModelInstance::FRAME_SCALE};
    ptr<RenderTarget> rt_model = [&]() -> ptr<RenderTarget> {
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

    // Copy render
    int32_t l = iround<int32_t>(model_spr->GetAtlasRect().x * numeric_cast<float32_t>(model_spr->GetAtlas()->GetSize().width));
    int32_t t = iround<int32_t>(model_spr->GetAtlasRect().y * numeric_cast<float32_t>(model_spr->GetAtlas()->GetSize().height));
    int32_t w = iround<int32_t>(model_spr->GetAtlasRect().width * numeric_cast<float32_t>(model_spr->GetAtlas()->GetSize().width));
    int32_t h = iround<int32_t>(model_spr->GetAtlasRect().height * numeric_cast<float32_t>(model_spr->GetAtlas()->GetSize().height));
    irect32 region_to = irect32(l, t, w, h);

    _sprMngr->GetRtMngr().PushRenderTarget(model_spr->GetAtlas()->GetRenderTarget());
    auto pop_atlas_rt_on_fail = scope_fail([this]() noexcept { safe_call([this] { _sprMngr->GetRtMngr().PopRenderTarget(); }); });
    _sprMngr->DrawRenderTarget(rt_model, false, nullptr, &region_to);
    _sprMngr->GetRtMngr().PopRenderTarget();
    pop_atlas_rt_on_fail.release();
}

FO_END_NAMESPACE

#endif
