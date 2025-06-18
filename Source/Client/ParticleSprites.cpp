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

ParticleSprite::ParticleSprite(SpriteManager& spr_mngr) :
    AtlasSprite(spr_mngr)
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
    _sprMngr {spr_mngr},
    _settings {settings}
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
    const auto proj_height = numeric_cast<float32>(draw_size.height) * (1.0f / _settings.ModelProjFactor);
    const auto proj_width = proj_height * frame_ratio;
    const mat44 proj = App->Render.CreateOrthoMatrix(0.0f, proj_width, 0.0f, proj_height, -10.0f, 10.0f);
    mat44 world;
    mat44::Translation({proj_width / 2.0f, proj_height / 4.0f, 0.0f}, world);

    particle->Setup(proj, world, {}, {}, {});

    auto particle_spr = SafeAlloc::MakeShared<ParticleSprite>(_sprMngr);

    particle_spr->_factory = this;
    particle_spr->_particle = std::move(particle);
    particle_spr->Size.width = draw_size.width;
    particle_spr->Size.height = draw_size.height;
    particle_spr->Offset.y = numeric_cast<int16>(draw_size.height / 4);

    auto&& [atlas, atlas_node, pos] = _sprMngr.GetAtlasMngr().FindAtlasPlace(atlas_type, draw_size);

    particle_spr->Atlas = atlas;
    particle_spr->AtlasNode = atlas_node;
    particle_spr->AtlasRect.x = numeric_cast<float32>(pos.x) / numeric_cast<float32>(atlas->Size.width);
    particle_spr->AtlasRect.y = numeric_cast<float32>(pos.y) / numeric_cast<float32>(atlas->Size.height);
    particle_spr->AtlasRect.width = numeric_cast<float32>(draw_size.width) / numeric_cast<float32>(atlas->Size.width);
    particle_spr->AtlasRect.height = numeric_cast<float32>(draw_size.height) / numeric_cast<float32>(atlas->Size.height);

    return particle_spr;
}

auto ParticleSpriteFactory::LoadTexture(hstring path) -> pair<RenderTexture*, frect32>
{
    FO_STACK_TRACE_ENTRY();

    auto result = pair<RenderTexture*, frect32>();

    if (const auto it = _loadedParticleTextures.find(path); it == _loadedParticleTextures.end()) {
        auto any_spr = _sprMngr.LoadSprite(path, AtlasType::MeshTextures);
        auto atlas_spr = dynamic_ptr_cast<AtlasSprite>(std::move(any_spr));

        if (atlas_spr) {
            _loadedParticleTextures[path] = atlas_spr;
            result = pair {atlas_spr->Atlas->MainTex, atlas_spr->AtlasRect};
        }
        else {
            BreakIntoDebugger();
            WriteLog("Texture '{}' not found", path);
            _loadedParticleTextures[path] = nullptr;
        }
    }
    else if (const auto& atlas_spr = it->second) {
        result = pair {atlas_spr->Atlas->MainTex, atlas_spr->AtlasRect};
    }

    return result;
}

void ParticleSpriteFactory::DrawParticleToAtlas(ParticleSprite* particle_spr)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_particleMngr);

    // Find place for render
    const auto frame_size = particle_spr->Size;

    RenderTarget* rt_intermediate = nullptr;
    for (auto* rt : _rtIntermediate) {
        if (rt->MainTex->Size == frame_size) {
            rt_intermediate = rt;
            break;
        }
    }

    if (rt_intermediate == nullptr) {
        rt_intermediate = _sprMngr.GetRtMngr().CreateRenderTarget(true, RenderTarget::SizeKindType::Custom, frame_size, true);
        _rtIntermediate.push_back(rt_intermediate);
    }

    _sprMngr.GetRtMngr().PushRenderTarget(rt_intermediate);
    _sprMngr.GetRtMngr().ClearCurrentRenderTarget(ucolor::clear, true);

    // Draw particles
    particle_spr->GetParticle()->Draw();

    // Restore render target
    _sprMngr.GetRtMngr().PopRenderTarget();

    // Copy render
    irect32 region_to;

    // Render to atlas
    if (rt_intermediate->MainTex->FlippedHeight) {
        // Preserve flip
        const auto l = iround<int32>(particle_spr->AtlasRect.x * numeric_cast<float32>(particle_spr->Atlas->Size.width));
        const auto t = iround<int32>((1.0f - particle_spr->AtlasRect.y) * numeric_cast<float32>(particle_spr->Atlas->Size.height));
        const auto w = iround<int32>(particle_spr->AtlasRect.width * numeric_cast<float32>(particle_spr->Atlas->Size.width));
        const auto h = iround<int32>(particle_spr->AtlasRect.height * numeric_cast<float32>(particle_spr->Atlas->Size.height));

        region_to = irect32(l, t, w, h);
    }
    else {
        const auto l = iround<int32>(particle_spr->AtlasRect.x * numeric_cast<float32>(particle_spr->Atlas->Size.width));
        const auto t = iround<int32>(particle_spr->AtlasRect.y * numeric_cast<float32>(particle_spr->Atlas->Size.height));
        const auto w = iround<int32>(particle_spr->AtlasRect.width * numeric_cast<float32>(particle_spr->Atlas->Size.width));
        const auto h = iround<int32>(particle_spr->AtlasRect.height * numeric_cast<float32>(particle_spr->Atlas->Size.height));

        region_to = irect32(l, t, w, h);
    }

    _sprMngr.GetRtMngr().PushRenderTarget(particle_spr->Atlas->RTarg);
    _sprMngr.DrawRenderTarget(rt_intermediate, false, nullptr, &region_to);
    _sprMngr.GetRtMngr().PopRenderTarget();
}

FO_END_NAMESPACE();
