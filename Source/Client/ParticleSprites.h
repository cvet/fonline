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

// Todo: optimize sprite atlas filling

#pragma once

#include "Common.h"

#include "DefaultSprites.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE();

class ParticleSpriteFactory;

class ParticleSprite final : public AtlasSprite
{
public:
    explicit ParticleSprite(SpriteManager& spr_mngr, isize32 size, ipos32 offset, TextureAtlas* atlas, TextureAtlas::SpaceNode* atlas_node, frect32 atlas_rect, ParticleSpriteFactory* factory, unique_ptr<ParticleSystem>&& particle);
    ParticleSprite(const ParticleSprite&) = delete;
    ParticleSprite(ParticleSprite&&) noexcept = default;
    auto operator=(const ParticleSprite&) = delete;
    auto operator=(ParticleSprite&&) noexcept -> ParticleSprite& = delete;
    ~ParticleSprite() override = default;

    [[nodiscard]] auto IsHitTest(ipos32 pos) const -> bool override;
    [[nodiscard]] auto IsCopyable() const -> bool override { return false; }
    [[nodiscard]] auto IsDynamicDraw() const -> bool override { return true; }
    [[nodiscard]] auto GetParticle() -> ParticleSystem* { return _particle.get(); }
    [[nodiscard]] auto IsPlaying() const -> bool override { return _particle->IsActive(); }

    void Prewarm() override;
    void SetTime(float32 normalized_time) override;
    void SetDir(uint8 dir) override;
    void SetDirAngle(int16 dir_angle) override;
    void Play(hstring anim_name, bool looped, bool reversed) override;
    void Stop() override;
    auto Update() -> bool override;
    void DrawToAtlas();

private:
    raw_ptr<ParticleSpriteFactory> _factory {};
    unique_ptr<ParticleSystem> _particle {};
};

class ParticleSpriteFactory : public SpriteFactory
{
    friend class ParticleSprite;

public:
    ParticleSpriteFactory(SpriteManager& spr_mngr, RenderSettings& settings, EffectManager& effect_mngr, GameTimer& game_time, HashResolver& hash_resolver);
    ParticleSpriteFactory(const ParticleSpriteFactory&) = delete;
    ParticleSpriteFactory(ParticleSpriteFactory&&) noexcept = default;
    auto operator=(const ParticleSpriteFactory&) = delete;
    auto operator=(ParticleSpriteFactory&&) noexcept -> ParticleSpriteFactory& = delete;
    ~ParticleSpriteFactory() override = default;

    [[nodiscard]] auto GetExtensions() const -> vector<string> override { return {"fopts"}; }

    auto LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite> override;

private:
    auto LoadTexture(hstring path) -> pair<RenderTexture*, frect32>;
    void DrawParticleToAtlas(ParticleSprite* particle_spr);

    raw_ptr<SpriteManager> _sprMngr;
    raw_ptr<RenderSettings> _settings;
    unique_ptr<ParticleManager> _particleMngr {};
    unordered_map<hstring, shared_ptr<AtlasSprite>> _loadedParticleTextures {};
    vector<raw_ptr<RenderTarget>> _rtIntermediate {};
};

FO_END_NAMESPACE();
