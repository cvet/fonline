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

#if FO_SPARK_PARTICLES

#include "ParticleRuntime.h"

namespace SPK
{
    template<typename T>
    class Ref;

    class Renderer;
    class SPKContext;
    class System;

namespace FO
{
    class SparkQuadRenderer;
}
}

FO_BEGIN_NAMESPACE
class SparkParticleRuntimeBackend;

class SparkParticleRuntimeSystem final : public ParticleRuntimeSystem
{
    friend class SparkParticleRuntimeBackend;
    friend class SafeAlloc;

public:
    SparkParticleRuntimeSystem(const SparkParticleRuntimeSystem&) = delete;
    SparkParticleRuntimeSystem(SparkParticleRuntimeSystem&&) noexcept = delete;
    auto operator=(const SparkParticleRuntimeSystem&) = delete;
    auto operator=(SparkParticleRuntimeSystem&&) noexcept = delete;
    ~SparkParticleRuntimeSystem() override;

    [[nodiscard]] auto IsActive() const -> bool override;
    [[nodiscard]] auto GetDrawSize(isize32 default_size) const -> isize32 override;
    [[nodiscard]] auto GetDrawInScene() const -> bool override;

    void Setup(const ParticleRuntimeSetup& setup) override;
    auto Prewarm() -> float32_t override;
    void Respawn(optional<int32_t> seed) override;
    void Update(float32_t delta_seconds) override;
    void RefreshRenderTransform() override;
    void Draw() override;

    auto GetEditableBaseSystem() -> SPK::Ref<SPK::System>;
    void ReplaceBaseSystem(SPK::Ref<SPK::System> system);

private:
    SparkParticleRuntimeSystem(ptr<SparkParticleRuntimeBackend> runtime, string_view path, SPK::Ref<SPK::System> base_system);

    struct Impl;
    unique_ptr<Impl> _impl;
};

class SparkParticleRuntimeBackend final : public ParticleRuntimeBackend
{
    friend class SparkParticleRuntimeSystem;
    friend class SPK::FO::SparkQuadRenderer;

public:
    explicit SparkParticleRuntimeBackend(const ParticleRuntimeServices& services);
    SparkParticleRuntimeBackend(const SparkParticleRuntimeBackend&) = delete;
    SparkParticleRuntimeBackend(SparkParticleRuntimeBackend&&) noexcept = delete;
    auto operator=(const SparkParticleRuntimeBackend&) = delete;
    auto operator=(SparkParticleRuntimeBackend&&) noexcept = delete;
    ~SparkParticleRuntimeBackend() override;

    [[nodiscard]] auto GetExtensions() const -> vector<string> override;

    void InvalidateResource(string_view path) override;
    auto Create(string_view path) -> unique_nptr<ParticleRuntimeSystem> override;

private:
    struct Impl;
    unique_ptr<Impl> _impl;
};
FO_END_NAMESPACE

namespace SPK::FO
{
    FO_USING_NAMESPACE();

    struct SparkQuadRendererData
    {
        bool Active {};
        bool AlphaTest {};
        bool DepthWrite {};
        float32_t AlphaTestThreshold {};
        int32_t DrawWidth {};
        int32_t DrawHeight {};
        bool DrawInScene {};
        string EffectName {};
        string TextureName {};
        float32_t ScaleX {};
        float32_t ScaleY {};
        int32_t AtlasDimensionX {};
        int32_t AtlasDimensionY {};
        int32_t LookOrientation {};
        int32_t UpOrientation {};
        int32_t LockedAxis {};
        array<float32_t, 3> LookVector {};
        array<float32_t, 3> UpVector {};
    };

    void EnsureSparkParticleObjectsRegistered(SPKContext& context);
    auto IsSparkParticleObjectRegistered(const SPKContext& context) -> bool;
    auto IsSparkQuadRenderer(const Renderer& renderer) -> bool;
    auto CreateSparkQuadRenderer() -> Ref<Renderer>;
    auto GetSparkQuadRendererData(const Renderer& renderer) -> SparkQuadRendererData;
    void SetSparkQuadRendererData(Renderer& renderer, const SparkQuadRendererData& data);
}

#endif
