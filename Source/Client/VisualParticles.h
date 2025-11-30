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

#pragma once

#include "Common.h"

#include "EffectManager.h"
#include "FileSystem.h"
#include "Rendering.h"
#include "Settings.h"
#include "Timer.h"

namespace SPK
{
    class System;

    namespace FO
    {
        class SparkQuadRenderer;
    }
}

FO_BEGIN_NAMESPACE();

class ParticleSystem;

class ParticleManager final
{
    friend class ParticleSystem;
    friend class SPK::FO::SparkQuadRenderer;

public:
    using TextureLoader = function<pair<RenderTexture*, frect32>(string_view)>;

    ParticleManager(RenderSettings& settings, EffectManager& effect_mngr, FileSystem& resources, GameTimer& game_time, TextureLoader tex_loader);
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager(ParticleManager&&) noexcept = delete;
    auto operator=(const ParticleManager&) = delete;
    auto operator=(ParticleManager&&) noexcept = delete;
    ~ParticleManager();

    [[nodiscard]] auto CreateParticle(string_view name) -> unique_ptr<ParticleSystem>;

private:
    struct Impl;
    unique_ptr<Impl> _impl;
    raw_ptr<RenderSettings> _settings;
    raw_ptr<EffectManager> _effectMngr;
    raw_ptr<FileSystem> _resources;
    raw_ptr<GameTimer> _gameTime;
    TextureLoader _textureLoader;
    int32 _animUpdateThreshold {};
    mat44 _projMatColMaj {};
    mat44 _viewMatColMaj {};
};

class ParticleSystem final
{
    friend class ParticleManager;
    friend class SafeAlloc;

public:
    static constexpr float32 PREWARM_STEP = 0.5f;

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) noexcept = default;
    auto operator=(const ParticleSystem&) = delete;
    auto operator=(ParticleSystem&&) noexcept = delete;
    ~ParticleSystem();

    [[nodiscard]] auto IsActive() const -> bool;
    [[nodiscard]] auto GetElapsedTime() const -> float32;
    [[nodiscard]] auto GetBaseSystem() -> SPK::System*;
    [[nodiscard]] auto GetDrawSize() const -> isize32;
    [[nodiscard]] auto NeedForceDraw() const -> bool { return _forceDraw; }
    [[nodiscard]] auto NeedDraw() const -> bool;

    void Setup(const mat44& proj, const mat44& world, const vec3& pos_offset, float32 look_dir_angle, const vec3& view_offset);
    void Prewarm();
    void Respawn();
    void Draw();
    void SetBaseSystem(SPK::System* system);

private:
    explicit ParticleSystem(ParticleManager& particle_mngr);

    [[nodiscard]] auto GetTime() const -> nanotime;

    struct Impl;
    unique_ptr<Impl> _impl;
    raw_ptr<ParticleManager> _particleMngr;
    mat44 _projMat {};
    vec3 _viewOffset {};
    float64 _elapsedTime {};
    bool _forceDraw {};
    nanotime _lastDrawTime {};
};

FO_END_NAMESPACE();
