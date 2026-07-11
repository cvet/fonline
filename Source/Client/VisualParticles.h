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

FO_BEGIN_NAMESPACE

class ParticleManager;

class ParticleSystem final
{
    friend class ParticleManager;
    friend class SafeAlloc;

public:
    static constexpr float32_t PREWARM_STEP = 0.5f;

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) noexcept;
    auto operator=(const ParticleSystem&) = delete;
    auto operator=(ParticleSystem&&) noexcept = delete;
    ~ParticleSystem();

    [[nodiscard]] auto IsActive() const -> bool;
    [[nodiscard]] auto GetElapsedTime() const -> float32_t;
    [[nodiscard]] auto GetBaseSystem() -> nptr<SPK::System>;
    [[nodiscard]] auto GetDrawSize() const -> isize32;
    [[nodiscard]] auto GetDrawInScene() const -> bool;
    [[nodiscard]] auto NeedForceDraw() const -> bool { return _forceDraw; }
    [[nodiscard]] auto NeedDraw() const -> bool;

    void Setup(const mat44& proj, const mat44& world, const vec3& pos_offset, float32_t look_dir_angle, const vec3& view_offset, bool tilt_in_proj = false);
    void Prewarm();
    void Respawn();
    void Draw();
    void SetBaseSystem(nptr<SPK::System> system);

private:
    explicit ParticleSystem(ptr<ParticleManager> particle_mngr);

    struct Impl;

    [[nodiscard]] auto GetTime() const -> nanotime;

    unique_ptr<Impl> _impl;
    ptr<ParticleManager> _particleMngr;
    mat44 _projMatrix {};
    vec3 _viewOffset {};
    bool _tiltInProj {};
    float64_t _elapsedTime {};
    bool _forceDraw {};
    nanotime _lastDrawTime {};
};

class ParticleManager final
{
    friend class ParticleSystem;
    friend class SPK::FO::SparkQuadRenderer;

public:
    using TextureLoader = function<pair<nptr<RenderTexture>, frect32>(string_view)>;

    explicit ParticleManager(ptr<RenderSettings> settings, ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<FileSystem> resources, ptr<GameTimer> game_time, TextureLoader tex_loader);
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager(ParticleManager&&) noexcept = delete;
    auto operator=(const ParticleManager&) = delete;
    auto operator=(ParticleManager&&) noexcept = delete;
    ~ParticleManager();

    [[nodiscard]] auto CreateParticle(string_view name) -> optional<ParticleSystem>;

private:
    struct Impl;
    auto Random(int32_t min_value, int32_t max_value) -> int32_t;

    unique_ptr<Impl> _impl;
    ptr<RenderSettings> _settings;
    ptr<EffectManager> _effectMngr;
    ptr<IAppRender> _render;
    ptr<FileSystem> _resources;
    ptr<GameTimer> _gameTime;
    TextureLoader _textureLoader;
    std::mt19937 _randomGenerator {MakeSeededRandomGenerator()};
    int32_t _animUpdateThreshold {};
    mat44 _viewProjMatrix {};
    mat44 _viewMatrix {};
};

FO_END_NAMESPACE
