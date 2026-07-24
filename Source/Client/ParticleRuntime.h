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

FO_BEGIN_NAMESPACE

class EffectManager;
class FileSystem;
class IAppRender;
class RenderDrawBuffer;
class RenderTexture;
struct RenderSettings;

using ParticleTextureLoader = function<pair<nptr<RenderTexture>, frect32>(string_view)>;

struct ParticleBounds3D
{
    vec3 Min {};
    vec3 Max {};
};

struct ParticleRuntimeSetup
{
    mat44 Projection {};
    mat44 World {};
    vec3 PositionOffset {};
    vec3 ViewOffset {};
    float32_t LookDirectionAngle {};
    float32_t Scale {1.0f};
    float32_t MapCameraAngle {};
    bool TiltInProjection {};
};

struct ParticleRuntimeServices
{
    ptr<EffectManager> EffectMngr;
    ptr<IAppRender> Render;
    ptr<FileSystem> Resources;
    ParticleTextureLoader TextureLoader;
    ptr<RenderSettings> Settings;
};

class ParticleRuntimeSystem
{
public:
    ParticleRuntimeSystem() = default;
    ParticleRuntimeSystem(const ParticleRuntimeSystem&) = delete;
    ParticleRuntimeSystem(ParticleRuntimeSystem&&) noexcept = delete;
    auto operator=(const ParticleRuntimeSystem&) -> ParticleRuntimeSystem& = delete;
    auto operator=(ParticleRuntimeSystem&&) noexcept -> ParticleRuntimeSystem& = delete;
    virtual ~ParticleRuntimeSystem() = default;

    [[nodiscard]] virtual auto IsActive() const -> bool = 0;
    [[nodiscard]] virtual auto GetDrawInScene() const -> bool = 0;
    [[nodiscard]] virtual auto GetBakedBounds() const noexcept -> optional<ParticleBounds3D>;
    [[nodiscard]] virtual auto GetLiveBounds() const noexcept -> optional<ParticleBounds3D>;

    virtual void RebaseWorldParticles(vec3 delta) noexcept;
    virtual void Setup(const ParticleRuntimeSetup& setup) = 0;
    virtual auto Prewarm() -> float32_t = 0;
    virtual void Respawn(optional<int32_t> seed) = 0;
    virtual void Update(float32_t delta_seconds) = 0;
    virtual void RefreshRenderTransform() = 0;
    virtual void Draw() = 0;
};

class ParticleRuntimeBackend
{
public:
    ParticleRuntimeBackend() = default;
    ParticleRuntimeBackend(const ParticleRuntimeBackend&) = delete;
    ParticleRuntimeBackend(ParticleRuntimeBackend&&) noexcept = delete;
    auto operator=(const ParticleRuntimeBackend&) -> ParticleRuntimeBackend& = delete;
    auto operator=(ParticleRuntimeBackend&&) noexcept -> ParticleRuntimeBackend& = delete;
    virtual ~ParticleRuntimeBackend() = default;

    [[nodiscard]] virtual auto GetExtensions() const -> vector<string> = 0;

    virtual auto Create(string_view path) -> unique_nptr<ParticleRuntimeSystem> = 0;
    virtual void InvalidateResource(string_view path) = 0;
};

// Create particle runtime backends for all available particle systems.
// The returned vector is sorted by backend priority, with the first backend being the most preferred.
auto CreateParticleRuntimeBackends(const ParticleRuntimeServices& services) -> vector<unique_ptr<ParticleRuntimeBackend>>;

// Debug wireframe overlay for particle geometry: re-draws a particle draw buffer's triangles as a line list through
// the primitive effect with the same projection, mirroring the sprite-batch wireframe from Render.DrawWireframe. The
// overlay buffer is created lazily on first use and reused between draws.
void DrawParticleBufferWireframe(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, unique_nptr<RenderDrawBuffer>& overlay_buf, const RenderDrawBuffer& source_buf, size_t index_count, const mat44& proj_matrix);

FO_END_NAMESPACE
