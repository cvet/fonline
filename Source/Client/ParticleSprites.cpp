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

#include "ParticleSprites.h"

FO_BEGIN_NAMESPACE();

ParticleSprite::ParticleSprite(SpriteManager& spr_mngr, isize32 size, ipos32 offset, TextureAtlas* atlas, TextureAtlas::SpaceNode* atlas_node, frect32 atlas_rect, ParticleSpriteFactory* factory, unique_ptr<ParticleSystem>&& particle) :
    AtlasSprite(spr_mngr, size, offset, atlas, atlas_node, atlas_rect, {}),
    _factory {factory},
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

void ParticleSprite::SetTime(float32 normalized_time)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(normalized_time);
}

void ParticleSprite::SetDir(uint8 dir)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(dir);
}

void ParticleSprite::SetDirAngle(int16 dir_angle)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(dir_angle);
}

void ParticleSprite::Play(hstring anim_name, bool looped, bool reversed)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(anim_name);
    ignore_unused(looped);
    ignore_unused(reversed);

    StartUpdate();
}

void ParticleSprite::Stop()
{
    FO_STACK_TRACE_ENTRY();
}

auto ParticleSprite::Update() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_particle->NeedForceDraw() || _particle->NeedDraw()) {
        DrawToAtlas();
    }

    return true;
}

void ParticleSprite::DrawToAtlas()
{
    FO_STACK_TRACE_ENTRY();

    _factory->DrawParticleToAtlas(this);
}

ParticleSpriteFactory::ParticleSpriteFactory(SpriteManager& spr_mngr, RenderSettings& settings, EffectManager& effect_mngr, GameTimer& game_time, HashResolver& hash_resolver) :
    _sprMngr {&spr_mngr},
    _settings {&settings}
{
    FO_STACK_TRACE_ENTRY();

    _particleMngr = SafeAlloc::MakeUnique<ParticleManager>(settings, effect_mngr, spr_mngr.GetResources(), game_time, //
        [this, &hash_resolver](string_view path) { return LoadTexture(hash_resolver.ToHashedString(path)); });
}

auto ParticleSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    auto particle = _particleMngr->CreateParticle(path);

    if (!particle) {
        return nullptr;
    }

    const auto draw_size = particle->GetDrawSize();
    const auto frame_ratio = numeric_cast<float32>(draw_size.width) / numeric_cast<float32>(draw_size.height);
    const auto proj_height = numeric_cast<float32>(draw_size.height) * (1.0f / _settings->ModelProjFactor);
    const auto proj_width = proj_height * frame_ratio;
    const mat44 proj = App->Render.CreateOrthoMatrix(0.0f, proj_width, 0.0f, proj_height, -10.0f, 10.0f);
    mat44 world;
    mat44::Translation({proj_width / 2.0f, proj_height / 4.0f, 0.0f}, world);

    particle->Setup(proj, world, {}, {}, {});

    auto&& [atlas, atlas_node, pos] = _sprMngr->GetAtlasMngr().FindAtlasPlace(atlas_type, draw_size);

    frect32 atlas_rect;
    atlas_rect.x = numeric_cast<float32>(pos.x) / numeric_cast<float32>(atlas->GetSize().width);
    atlas_rect.y = numeric_cast<float32>(pos.y) / numeric_cast<float32>(atlas->GetSize().height);
    atlas_rect.width = numeric_cast<float32>(draw_size.width) / numeric_cast<float32>(atlas->GetSize().width);
    atlas_rect.height = numeric_cast<float32>(draw_size.height) / numeric_cast<float32>(atlas->GetSize().height);

    const ipos32 offset = ipos32(0, draw_size.height / 4);
    auto particle_spr = SafeAlloc::MakeShared<ParticleSprite>(*_sprMngr, draw_size, offset, atlas, atlas_node, atlas_rect, this, std::move(particle));
    return particle_spr;
}

auto ParticleSpriteFactory::LoadTexture(hstring path) -> pair<RenderTexture*, frect32>
{
    FO_STACK_TRACE_ENTRY();

    auto result = pair<RenderTexture*, frect32>();

    if (const auto it = _loadedParticleTextures.find(path); it == _loadedParticleTextures.end()) {
        auto any_spr = _sprMngr->LoadSprite(path, AtlasType::MeshTextures);
        auto atlas_spr = dynamic_ptr_cast<AtlasSprite>(std::move(any_spr));

        if (atlas_spr) {
            _loadedParticleTextures[path] = atlas_spr;
            result = pair {atlas_spr->GetAtlas()->GetTexture(), atlas_spr->GetAtlasRect()};
        }
        else {
            BreakIntoDebugger();
            WriteLog("Texture '{}' not found", path);
            _loadedParticleTextures[path] = nullptr;
        }
    }
    else if (auto& atlas_spr = it->second) {
        result = pair {atlas_spr->GetAtlas()->GetTexture(), atlas_spr->GetAtlasRect()};
    }

    return result;
}

void ParticleSpriteFactory::DrawParticleToAtlas(ParticleSprite* particle_spr)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_particleMngr);

    // Find place for render
    const auto frame_size = particle_spr->GetSize();

    RenderTarget* rt_intermediate = nullptr;

    for (auto& rt : _rtIntermediate) {
        if (rt->GetTexture()->Size == frame_size) {
            rt_intermediate = rt.get();
            break;
        }
    }

    if (rt_intermediate == nullptr) {
        rt_intermediate = _sprMngr->GetRtMngr().CreateRenderTarget(true, RenderTarget::SizeKindType::Custom, frame_size, true);
        _rtIntermediate.emplace_back(rt_intermediate);
    }

    _sprMngr->GetRtMngr().PushRenderTarget(rt_intermediate);
    _sprMngr->GetRtMngr().ClearCurrentRenderTarget(ucolor::clear, true);

    // Draw particles
    particle_spr->GetParticle()->Draw();

    // Restore render target
    _sprMngr->GetRtMngr().PopRenderTarget();

    // Copy render
    const int32 l = iround<int32>(particle_spr->GetAtlasRect().x * numeric_cast<float32>(particle_spr->GetAtlas()->GetSize().width));
    const int32 t = iround<int32>(particle_spr->GetAtlasRect().y * numeric_cast<float32>(particle_spr->GetAtlas()->GetSize().height));
    const int32 w = iround<int32>(particle_spr->GetAtlasRect().width * numeric_cast<float32>(particle_spr->GetAtlas()->GetSize().width));
    const int32 h = iround<int32>(particle_spr->GetAtlasRect().height * numeric_cast<float32>(particle_spr->GetAtlas()->GetSize().height));
    const irect32 region_to = irect32(l, t, w, h);

    _sprMngr->GetRtMngr().PushRenderTarget(particle_spr->GetAtlas()->GetRenderTarget());
    _sprMngr->DrawRenderTarget(rt_intermediate, false, nullptr, &region_to);
    _sprMngr->GetRtMngr().PopRenderTarget();
}

FO_END_NAMESPACE();
