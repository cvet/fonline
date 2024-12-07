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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Log.h"

ModelSprite::ModelSprite(SpriteManager& spr_mngr) :
    AtlasSprite(spr_mngr)
{
    STACK_TRACE_ENTRY();
}

auto ModelSprite::IsHitTest(int x, int y) const -> bool
{
    NO_STACK_TRACE_ENTRY();

    auto&& [view_width, view_height] = _model->GetViewSize();

    const auto x2 = x - (Width - view_width) / 2;
    const auto y2 = y - (Height - view_height - (OffsY - view_height / 8));

    if (x2 >= 0 && y2 >= 0 && x2 < view_width && y2 < view_height) {
        const auto atlas_x = x + iround(Atlas->MainTex->SizeData[0] * AtlasRect.Left);
        const auto atlas_y = y + iround(Atlas->MainTex->SizeData[1] * AtlasRect.Top);

        return _sprMngr.GetRtMngr().GetRenderTargetPixel(Atlas->RTarg, atlas_x, atlas_y).comp.a > 0;
    }

    return false;
}

auto ModelSprite::GetViewSize() const -> optional<IRect>
{
    NO_STACK_TRACE_ENTRY();

    auto&& [view_width, view_height] = _model->GetViewSize();

    return IRect {view_width, view_height, 0, -OffsY + view_height / 8};
}

void ModelSprite::Prewarm()
{
    STACK_TRACE_ENTRY();

    _model->PrewarmParticles();
}

void ModelSprite::SetTime(float normalized_time)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(normalized_time);
}

void ModelSprite::SetDir(uint8 dir)
{
    STACK_TRACE_ENTRY();

    SetDirAngle(GeometryHelper::DirToAngle(dir));
}

void ModelSprite::SetDirAngle(short dir_angle)
{
    STACK_TRACE_ENTRY();

    _model->SetLookDirAngle(dir_angle);
    _model->SetMoveDirAngle(dir_angle, true);
}

void ModelSprite::Play(hstring anim_name, bool looped, bool reversed)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(anim_name);
    UNUSED_VARIABLE(looped);
    UNUSED_VARIABLE(reversed);

    StartUpdate();
}

void ModelSprite::Stop()
{
    STACK_TRACE_ENTRY();
}

auto ModelSprite::Update() -> bool
{
    STACK_TRACE_ENTRY();

    if (_model->NeedForceDraw() || _model->NeedDraw()) {
        DrawToAtlas();
    }

    return true;
}

void ModelSprite::SetSize(int width, int height)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(width > 0);
    RUNTIME_ASSERT(height > 0);

    if (width == Width && height == Height) {
        return;
    }

    if (AtlasNode != nullptr) {
        AtlasNode->Free();
        AtlasNode = nullptr;
    }

    _model->SetupFrame(width, height);

    Width = width;
    Height = height;
    OffsY = static_cast<int16>(height / 4);

    int x = 0;
    int y = 0;
    auto&& [atlas, atlas_node] = _sprMngr.GetAtlasMngr().FindAtlasPlace(_atlasType, width, height, x, y);

    Atlas = atlas;
    AtlasNode = atlas_node;
    AtlasRect.Left = static_cast<float>(x) / static_cast<float>(atlas->Width);
    AtlasRect.Top = static_cast<float>(y) / static_cast<float>(atlas->Height);
    AtlasRect.Right = static_cast<float>(x + width) / static_cast<float>(atlas->Width);
    AtlasRect.Bottom = static_cast<float>(y + height) / static_cast<float>(atlas->Height);
}

void ModelSprite::DrawToAtlas()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _factory->DrawModelToAtlas(this);
}

ModelSpriteFactory::ModelSpriteFactory(SpriteManager& spr_mngr, RenderSettings& settings, EffectManager& effect_mngr, GameTimer& game_time, HashResolver& hash_resolver, NameResolver& name_resolver, AnimationResolver& anim_name_resolver) :
    _sprMngr {spr_mngr}
{
    STACK_TRACE_ENTRY();

    _modelMngr = std::make_unique<ModelManager>(settings, spr_mngr.GetResources(), effect_mngr, game_time, hash_resolver, name_resolver, anim_name_resolver, //
        [this, &hash_resolver](string_view path) { return LoadTexture(hash_resolver.ToHashedString(path)); });
}

auto ModelSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    STACK_TRACE_ENTRY();

    auto&& model = _modelMngr->CreateModel(path);

    if (!model) {
        return nullptr;
    }

    auto&& model_spr = std::make_shared<ModelSprite>(_sprMngr);
    auto&& [draw_width, draw_height] = model->GetDrawSize();

    model_spr->_factory = this;
    model_spr->_model = std::move(model);
    model_spr->_atlasType = atlas_type;

    model_spr->SetSize(draw_width, draw_height);

    return model_spr;
}

auto ModelSpriteFactory::LoadTexture(hstring path) -> pair<RenderTexture*, FRect>
{
    STACK_TRACE_ENTRY();

    auto result = pair<RenderTexture*, FRect>();

    if (const auto it = _loadedMeshTextures.find(path); it == _loadedMeshTextures.end()) {
        auto&& any_spr = _sprMngr.LoadSprite(path, AtlasType::MeshTextures);
        auto&& atlas_spr = dynamic_pointer_cast<AtlasSprite>(any_spr);

        if (atlas_spr) {
            _loadedMeshTextures[path] = atlas_spr;
            result = pair {atlas_spr->Atlas->MainTex, FRect {atlas_spr->AtlasRect[0], atlas_spr->AtlasRect[1], atlas_spr->AtlasRect[2] - atlas_spr->AtlasRect[0], atlas_spr->AtlasRect[3] - atlas_spr->AtlasRect[1]}};
        }
        else {
            BreakIntoDebugger();

            if (any_spr) {
                WriteLog("Texture '{}' is not atlas sprite", path);
            }
            else {
                WriteLog("Texture '{}' not found", path);
            }

            _loadedMeshTextures[path] = nullptr;
        }
    }
    else if (auto&& atlas_spr = it->second) {
        result = pair {atlas_spr->Atlas->MainTex, FRect {atlas_spr->AtlasRect[0], atlas_spr->AtlasRect[1], atlas_spr->AtlasRect[2] - atlas_spr->AtlasRect[0], atlas_spr->AtlasRect[3] - atlas_spr->AtlasRect[1]}};
    }

    return result;
}

void ModelSpriteFactory::DrawModelToAtlas(ModelSprite* model_spr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_modelMngr);

    // Find place for render
    const auto frame_width = model_spr->Width * ModelInstance::FRAME_SCALE;
    const auto frame_height = model_spr->Height * ModelInstance::FRAME_SCALE;

    RenderTarget* rt_model = nullptr;

    for (auto* rt : _rtIntermediate) {
        if (rt->MainTex->Width == frame_width && rt->MainTex->Height == frame_height) {
            rt_model = rt;
            break;
        }
    }

    if (rt_model == nullptr) {
        rt_model = _sprMngr.GetRtMngr().CreateRenderTarget(true, RenderTarget::SizeType::Custom, frame_width, frame_height, true);
        _rtIntermediate.push_back(rt_model);
    }

    _sprMngr.GetRtMngr().PushRenderTarget(rt_model);
    _sprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear, true);

    // Draw model
    model_spr->GetModel()->Draw();

    // Restore render target
    _sprMngr.GetRtMngr().PopRenderTarget();

    // Copy render
    IRect region_to;

    // Render to atlas
    if (rt_model->MainTex->FlippedHeight) {
        // Preserve flip
        const auto l = iround(model_spr->AtlasRect.Left * static_cast<float>(model_spr->Atlas->Width));
        const auto t = iround((1.0f - model_spr->AtlasRect.Top) * static_cast<float>(model_spr->Atlas->Height));
        const auto r = iround(model_spr->AtlasRect.Right * static_cast<float>(model_spr->Atlas->Width));
        const auto b = iround((1.0f - model_spr->AtlasRect.Bottom) * static_cast<float>(model_spr->Atlas->Height));
        region_to = IRect(l, t, r, b);
    }
    else {
        const auto l = iround(model_spr->AtlasRect.Left * static_cast<float>(model_spr->Atlas->Width));
        const auto t = iround(model_spr->AtlasRect.Top * static_cast<float>(model_spr->Atlas->Height));
        const auto r = iround(model_spr->AtlasRect.Right * static_cast<float>(model_spr->Atlas->Width));
        const auto b = iround(model_spr->AtlasRect.Bottom * static_cast<float>(model_spr->Atlas->Height));
        region_to = IRect(l, t, r, b);
    }

    _sprMngr.GetRtMngr().PushRenderTarget(model_spr->Atlas->RTarg);
    _sprMngr.DrawRenderTarget(rt_model, false, nullptr, &region_to);
    _sprMngr.GetRtMngr().PopRenderTarget();
}

#endif
