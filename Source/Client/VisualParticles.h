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
#include "ParticleRuntime.h"
#include "Rendering.h"
#include "Settings.h"
#include "Timer.h"

FO_BEGIN_NAMESPACE

class ParticleManager;

class ParticleSystem final
{
    friend class ParticleManager;
    friend class SafeAlloc;

public:
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) noexcept;
    auto operator=(const ParticleSystem&) = delete;
    auto operator=(ParticleSystem&&) noexcept = delete;
    ~ParticleSystem();

    [[nodiscard]] auto GetRuntimeSystem() -> ptr<ParticleRuntimeSystem>;
    [[nodiscard]] auto GetRuntimeSystem() const -> ptr<const ParticleRuntimeSystem>;
    [[nodiscard]] auto IsActive() const -> bool;
    [[nodiscard]] auto GetElapsedTime() const -> float32_t;
    [[nodiscard]] auto GetDrawSize() const -> isize32;
    [[nodiscard]] auto GetDrawInScene() const -> bool;
    [[nodiscard]] auto GetRenderViewBounds() const noexcept -> optional<ParticleBounds3D>;
    [[nodiscard]] auto NeedForceDraw() const -> bool { return _forceDraw; }
    [[nodiscard]] auto NeedDraw() const -> bool;

    void EnableBoundsComputation() noexcept;
    void RebaseWorldParticles(vec3 delta) noexcept;
    void Setup(const mat44& proj, const mat44& world, const vec3& pos_offset, float32_t look_dir_angle, const vec3& view_offset, bool tilt_in_proj = false);
    void Prewarm();
    void Respawn();
    auto Respawn(int32_t seed) -> bool;
    void Update();
    void RefreshRenderTransform();
    void Draw();
    void SetScale(float32_t scale);

private:
    explicit ParticleSystem(ptr<ParticleManager> particle_mngr, unique_ptr<ParticleRuntimeSystem>&& runtime_system);

    [[nodiscard]] auto GetTime() const -> nanotime;

    void ApplyRuntimeSetup();
    void ResetTiming();

    unique_ptr<ParticleRuntimeSystem> _runtimeSystem;
    ptr<ParticleManager> _particleMngr;
    optional<ParticleRuntimeSetup> _runtimeSetup {};
    float64_t _elapsedTime {};
    float32_t _scale {1.0f};
    bool _forceDraw {};
    bool _renderPending {};
    nanotime _lastUpdateTime {};
    nanotime _lastRenderTime {};
};

class ParticleManager final
{
    friend class ParticleSystem;

public:
    explicit ParticleManager(ptr<RenderSettings> settings, ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<FileSystem> resources, ptr<GameTimer> game_time, ParticleTextureLoader tex_loader);
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager(ParticleManager&&) noexcept = delete;
    auto operator=(const ParticleManager&) = delete;
    auto operator=(ParticleManager&&) noexcept = delete;
    ~ParticleManager();

    [[nodiscard]] auto GetExtensions() const -> vector<string>;

    void InvalidateResource(string_view name);
    auto CreateParticle(string_view name) -> optional<ParticleSystem>;

private:
    struct Impl;

    unique_ptr<Impl> _impl;
    ptr<RenderSettings> _settings;
    ptr<GameTimer> _gameTime;
    int32_t _animUpdateThreshold {};
};

FO_END_NAMESPACE
