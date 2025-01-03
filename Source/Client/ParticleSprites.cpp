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
#include "Log.h"

ParticleSprite::ParticleSprite(SpriteManager& spr_mngr) :
    AtlasSprite(spr_mngr)
{
    STACK_TRACE_ENTRY();
}

auto ParticleSprite::IsHitTest(ipos pos) const -> bool
{
    NO_STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(pos);

    return false;
}

void ParticleSprite::Prewarm()
{
    STACK_TRACE_ENTRY();

    _particle->Prewarm();
}

void ParticleSprite::SetTime(float normalized_time)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(normalized_time);
}

void ParticleSprite::SetDir(uint8 dir)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(dir);
}

void ParticleSprite::SetDirAngle(short dir_angle)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(dir_angle);
}

void ParticleSprite::Play(hstring anim_name, bool looped, bool reversed)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(anim_name);
    UNUSED_VARIABLE(looped);
    UNUSED_VARIABLE(reversed);

    StartUpdate();
}

void ParticleSprite::Stop()
{
    STACK_TRACE_ENTRY();
}

auto ParticleSprite::Update() -> bool
{
    STACK_TRACE_ENTRY();

    if (_particle->NeedForceDraw() || _particle->NeedDraw()) {
        DrawToAtlas();
    }

    return true;
}

void ParticleSprite::DrawToAtlas()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _factory->DrawParticleToAtlas(this);
}

ParticleSpriteFactory::ParticleSpriteFactory(SpriteManager& spr_mngr, RenderSettings& settings, EffectManager& effect_mngr, GameTimer& game_time, HashResolver& hash_resolver) :
    _sprMngr {spr_mngr},
    _settings {settings}
{
    STACK_TRACE_ENTRY();

    _particleMngr = SafeAlloc::MakeUnique<ParticleManager>(settings, effect_mngr, spr_mngr.GetResources(), game_time, //
        [this, &hash_resolver](string_view path) { return LoadTexture(hash_resolver.ToHashedString(path)); });
}

auto ParticleSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    STACK_TRACE_ENTRY();

    auto&& particle = _particleMngr->CreateParticle(path);
    if (!particle) {
        return nullptr;
    }

    const auto draw_size = particle->GetDrawSize();
    const auto frame_ratio = static_cast<float>(draw_size.width) / static_cast<float>(draw_size.height);
    const auto proj_height = static_cast<float>(draw_size.height) * (1.0f / _settings.ModelProjFactor);
    const auto proj_width = proj_height * frame_ratio;
    const mat44 proj = App->Render.CreateOrthoMatrix(0.0f, proj_width, 0.0f, proj_height, -10.0f, 10.0f);
    mat44 world;
    mat44::Translation({proj_width / 2.0f, proj_height / 4.0f, 0.0f}, world);

    particle->Setup(proj, world, {}, {}, {});

    auto&& particle_spr = std::make_shared<ParticleSprite>(_sprMngr);

    particle_spr->_factory = this;
    particle_spr->_particle = std::move(particle);
    particle_spr->Size.width = draw_size.width;
    particle_spr->Size.height = draw_size.height;
    particle_spr->Offset.y = static_cast<int16>(draw_size.height / 4);

    auto&& [atlas, atlas_node, pos] = _sprMngr.GetAtlasMngr().FindAtlasPlace(atlas_type, draw_size);

    particle_spr->Atlas = atlas;
    particle_spr->AtlasNode = atlas_node;
    particle_spr->AtlasRect.Left = static_cast<float>(pos.x) / static_cast<float>(atlas->Size.width);
    particle_spr->AtlasRect.Top = static_cast<float>(pos.y) / static_cast<float>(atlas->Size.height);
    particle_spr->AtlasRect.Right = static_cast<float>(pos.x + draw_size.width) / static_cast<float>(atlas->Size.width);
    particle_spr->AtlasRect.Bottom = static_cast<float>(pos.y + draw_size.height) / static_cast<float>(atlas->Size.height);

    return particle_spr;
}

auto ParticleSpriteFactory::LoadTexture(hstring path) -> pair<RenderTexture*, FRect>
{
    STACK_TRACE_ENTRY();

    auto result = pair<RenderTexture*, FRect>();

    if (const auto it = _loadedParticleTextures.find(path); it == _loadedParticleTextures.end()) {
        auto&& any_spr = _sprMngr.LoadSprite(path, AtlasType::MeshTextures);
        auto&& atlas_spr = dynamic_pointer_cast<AtlasSprite>(any_spr);

        if (atlas_spr) {
            _loadedParticleTextures[path] = atlas_spr;
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

            _loadedParticleTextures[path] = nullptr;
        }
    }
    else if (auto&& atlas_spr = it->second) {
        result = pair {atlas_spr->Atlas->MainTex, FRect {atlas_spr->AtlasRect[0], atlas_spr->AtlasRect[1], atlas_spr->AtlasRect[2] - atlas_spr->AtlasRect[0], atlas_spr->AtlasRect[3] - atlas_spr->AtlasRect[1]}};
    }

    return result;
}

void ParticleSpriteFactory::DrawParticleToAtlas(ParticleSprite* particle_spr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_particleMngr);

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
    IRect region_to;

    // Render to atlas
    if (rt_intermediate->MainTex->FlippedHeight) {
        // Preserve flip
        const auto l = iround(particle_spr->AtlasRect.Left * static_cast<float>(particle_spr->Atlas->Size.width));
        const auto t = iround((1.0f - particle_spr->AtlasRect.Top) * static_cast<float>(particle_spr->Atlas->Size.height));
        const auto r = iround(particle_spr->AtlasRect.Right * static_cast<float>(particle_spr->Atlas->Size.width));
        const auto b = iround((1.0f - particle_spr->AtlasRect.Bottom) * static_cast<float>(particle_spr->Atlas->Size.height));

        region_to = IRect(l, t, r, b);
    }
    else {
        const auto l = iround(particle_spr->AtlasRect.Left * static_cast<float>(particle_spr->Atlas->Size.width));
        const auto t = iround(particle_spr->AtlasRect.Top * static_cast<float>(particle_spr->Atlas->Size.height));
        const auto r = iround(particle_spr->AtlasRect.Right * static_cast<float>(particle_spr->Atlas->Size.width));
        const auto b = iround(particle_spr->AtlasRect.Bottom * static_cast<float>(particle_spr->Atlas->Size.height));

        region_to = IRect(l, t, r, b);
    }

    _sprMngr.GetRtMngr().PushRenderTarget(particle_spr->Atlas->RTarg);
    _sprMngr.DrawRenderTarget(rt_intermediate, false, nullptr, &region_to);
    _sprMngr.GetRtMngr().PopRenderTarget();
}
