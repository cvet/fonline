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

// Todo: optimize sprite atlas filling

#pragma once

#include "Common.h"

#include "DefaultSprites.h"
#include "SpriteManager.h"

class ParticleSpriteFactory;

class ParticleSprite final : public AtlasSprite
{
    friend class ParticleSpriteFactory;

public:
    explicit ParticleSprite(SpriteManager& spr_mngr);
    ParticleSprite(const ParticleSprite&) = delete;
    ParticleSprite(ParticleSprite&&) noexcept = default;
    auto operator=(const ParticleSprite&) = delete;
    auto operator=(ParticleSprite&&) noexcept -> ParticleSprite& = delete;
    ~ParticleSprite() override = default;

    [[nodiscard]] auto IsHitTest(ipos pos) const -> bool override;
    [[nodiscard]] auto IsCopyable() const -> bool override { return false; }
    [[nodiscard]] auto IsDynamicDraw() const -> bool override { return true; }
    [[nodiscard]] auto GetParticle() -> ParticleSystem* { NON_CONST_METHOD_HINT_ONELINE() return _particle.get(); }
    [[nodiscard]] auto IsPlaying() const -> bool override { return _particle->IsActive(); }

    void Prewarm() override;
    void SetTime(float normalized_time) override;
    void SetDir(uint8 dir) override;
    void SetDirAngle(short dir_angle) override;
    void Play(hstring anim_name, bool looped, bool reversed) override;
    void Stop() override;
    auto Update() -> bool override;
    void DrawToAtlas();

private:
    ParticleSpriteFactory* _factory {};
    unique_ptr<ParticleSystem> _particle {};
    bool _nonConstHelper {};
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
    auto LoadTexture(hstring path) -> pair<RenderTexture*, FRect>;
    void DrawParticleToAtlas(ParticleSprite* particle_spr);

    SpriteManager& _sprMngr;
    RenderSettings& _settings;
    unique_ptr<ParticleManager> _particleMngr {};
    unordered_map<hstring, shared_ptr<AtlasSprite>> _loadedParticleTextures {};
    vector<RenderTarget*> _rtIntermediate {};
};
