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

#include "ParticleSprites.h"
#include "Application.h"
#include "Geometry.h"

FO_BEGIN_NAMESPACE

ParticleSprite::ParticleSprite(ptr<SpriteManager> spr_mngr, isize32 size, ipos32 offset, ptr<TextureAtlas> atlas, unique_del_ptr<TextureAtlasLayout::Allocation> atlas_allocation, frect32 atlas_rect, ptr<ParticleSpriteFactory> factory, unique_ptr<ParticleSystem>&& particle, bool draw_in_scene) :
    AtlasSprite(spr_mngr, size, offset, atlas, std::move(atlas_allocation), atlas_rect, {}),
    _factory {factory},
    _drawInScene {draw_in_scene},
    _particle {std::move(particle)}
{
    FO_STACK_TRACE_ENTRY();
}

auto ParticleSprite::IsHitTest(ipos32 pos) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(pos);

    return false;
}

void ParticleSprite::Prewarm()
{
    FO_STACK_TRACE_ENTRY();

    _particle->Prewarm();
}

void ParticleSprite::SetTime(float32_t normalized_time)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(normalized_time);
}

void ParticleSprite::SetDir(mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(dir);
}

void ParticleSprite::Play(hstring anim_name, bool looped, bool reversed)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(anim_name);
    ignore_unused(looped);
    ignore_unused(reversed);

    _particle->Respawn();
    StartUpdate();
}

void ParticleSprite::Stop()
{
    FO_STACK_TRACE_ENTRY();
}

auto ParticleSprite::Update() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_drawInScene) {
        if (_particle->NeedForceDraw() || _particle->NeedDraw()) {
            DrawToAtlas();
        }
    }

    return true;
}

void ParticleSprite::DrawToAtlas()
{
    FO_STACK_TRACE_ENTRY();

    _factory->DrawParticleToAtlas(this);
}

void ParticleSprite::DrawInScene(fpos32 scene_pos, float32_t depth) const
{
    FO_STACK_TRACE_ENTRY();

    const RenderSettings& settings = *_factory->_settings;
    const mat44 scene_ortho = _sprMngr->GetRender().GetProjMatrix();
    const mat44 cam_view = GeometryHelper::MakeMapCameraView(settings.MapCameraAngle, 0.0f, fpos32 {0.0f, 0.0f}, 1.0f);
    const mat44 local_to_world = glm::scale(mat44 {1.0f}, vec3 {settings.ModelProjFactor, settings.ModelProjFactor, settings.ModelProjFactor});

    const mat44 proj_base = scene_ortho * cam_view * local_to_world;
    const mat44 proj = GeometryHelper::MakeMapAnchoredProj(proj_base, scene_ortho, scene_pos, depth);

    _particle->Setup(proj, mat44 {1.0f}, {}, 0.0f, vec3 {0.0f, 0.0f, 0.0f}, true);
    _particle->Draw();
}

ParticleSpriteFactory::ParticleSpriteFactory(ptr<SpriteManager> spr_mngr, ptr<RenderSettings> settings, ptr<EffectManager> effect_mngr, ptr<GameTimer> game_time, ptr<HashResolver> hash_resolver) :
    _sprMngr {spr_mngr},
    _settings {settings},
    _particleMngr {settings, effect_mngr, &spr_mngr->GetRender(), spr_mngr->GetResources(), game_time, //
        [this, hash_resolver](string_view path) mutable FO_DEFERRED { return LoadTexture(hash_resolver->ToHashedString(path)); }}
{
    FO_STACK_TRACE_ENTRY();
}

auto ParticleSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    optional<ParticleSystem> particle = _particleMngr.CreateParticle(path);

    if (!particle) {
        return nullptr;
    }

    const auto draw_size = particle->GetDrawSize();
    const auto frame_ratio = numeric_cast<float32_t>(draw_size.width) / numeric_cast<float32_t>(draw_size.height);
    const auto proj_height = numeric_cast<float32_t>(draw_size.height) * (1.0f / _settings->ModelProjFactor);
    const auto proj_width = proj_height * frame_ratio;
    const mat44 proj = _sprMngr->GetRender().CreateOrthoMatrix(0.0f, proj_width, 0.0f, proj_height, -10.0f, 10.0f);
    const auto world = glm::translate(mat44 {1.0f}, vec3 {proj_width / 2.0f, proj_height / 4.0f, 0.0f});

    particle->Setup(proj, world, {}, {}, {});

    auto&& [atlas, atlas_allocation, pos] = _sprMngr->GetAtlasMngr()->FindAtlasPlace(atlas_type, draw_size);

    frect32 atlas_rect;
    atlas_rect.x = numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(atlas->GetSize().width);
    atlas_rect.y = numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(atlas->GetSize().height);
    atlas_rect.width = numeric_cast<float32_t>(draw_size.width) / numeric_cast<float32_t>(atlas->GetSize().width);
    atlas_rect.height = numeric_cast<float32_t>(draw_size.height) / numeric_cast<float32_t>(atlas->GetSize().height);

    const ipos32 offset = ipos32(0, draw_size.height / 4);
    const bool draw_in_scene = particle->GetDrawInScene();
    auto particle_value = SafeAlloc::MakeUnique<ParticleSystem>(std::move(*particle));
    return SafeAlloc::MakeShared<ParticleSprite>(_sprMngr, draw_size, offset, atlas, std::move(atlas_allocation), atlas_rect, this, std::move(particle_value), draw_in_scene);
}

auto ParticleSpriteFactory::LoadTexture(hstring path) -> pair<nptr<RenderTexture>, frect32>
{
    FO_STACK_TRACE_ENTRY();

    auto result = pair<nptr<RenderTexture>, frect32>();

    if (const auto it = _loadedParticleTextures.find(path); it == _loadedParticleTextures.end()) {
        // Particle UVs address the complete source bitmap; this callback cannot carry a cropped frame's SourceOffset.
        auto atlas_spr = _sprMngr->LoadSpriteAsQuad(path, AtlasType::MeshTextures);

        if (atlas_spr) {
            _loadedParticleTextures[path] = atlas_spr;
            result = {atlas_spr->GetAtlas()->GetTexture(), atlas_spr->GetAtlasRect()};
        }
        else {
            BreakIntoDebugger();
            WriteLog("Texture '{}' not found", path);
            _loadedParticleTextures[path] = nullptr;
        }
    }
    else if (auto& atlas_spr = it->second) {
        result = {atlas_spr->GetAtlas()->GetTexture(), atlas_spr->GetAtlasRect()};
    }

    return result;
}

void ParticleSpriteFactory::DrawParticleToAtlas(ptr<ParticleSprite> particle_spr)
{
    FO_STACK_TRACE_ENTRY();

    // Find place for render
    const auto frame_size = particle_spr->GetSize();

    const ptr<RenderTarget> rt_intermediate = [&]() -> ptr<RenderTarget> {
        for (ptr<RenderTarget> rt : _rtIntermediate) {
            if (rt->GetTexture()->Size == frame_size) {
                return rt;
            }
        }

        auto rt = _sprMngr->GetRtMngr().CreateRenderTarget(true, frame_size, true);
        _rtIntermediate.emplace_back(rt);
        return rt;
    }();

    _sprMngr->GetRtMngr().PushRenderTarget(rt_intermediate);
    _sprMngr->GetRtMngr().ClearCurrentRenderTarget(ucolor::clear, true);

    // Draw particles
    particle_spr->GetParticle()->Draw();

    // Restore render target
    _sprMngr->GetRtMngr().PopRenderTarget();

    // Copy render
    const int32_t l = iround<int32_t>(particle_spr->GetAtlasRect().x * numeric_cast<float32_t>(particle_spr->GetAtlas()->GetSize().width));
    const int32_t t = iround<int32_t>(particle_spr->GetAtlasRect().y * numeric_cast<float32_t>(particle_spr->GetAtlas()->GetSize().height));
    const int32_t w = iround<int32_t>(particle_spr->GetAtlasRect().width * numeric_cast<float32_t>(particle_spr->GetAtlas()->GetSize().width));
    const int32_t h = iround<int32_t>(particle_spr->GetAtlasRect().height * numeric_cast<float32_t>(particle_spr->GetAtlas()->GetSize().height));
    const irect32 region_to = irect32(l, t, w, h);

    _sprMngr->GetRtMngr().PushRenderTarget(particle_spr->GetAtlas()->GetRenderTarget());
    _sprMngr->DrawRenderTarget(rt_intermediate, false, nullptr, &region_to);
    _sprMngr->GetRtMngr().PopRenderTarget();
}

FO_END_NAMESPACE
