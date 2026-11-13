// FOnline Engine
// SPDX-License-Identifier: MIT

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
    friend class safe_alloc;

public:
    SparkParticleRuntimeSystem(const SparkParticleRuntimeSystem&) = delete;
    SparkParticleRuntimeSystem(SparkParticleRuntimeSystem&&) noexcept = delete;
    auto operator=(const SparkParticleRuntimeSystem&) = delete;
    auto operator=(SparkParticleRuntimeSystem&&) noexcept = delete;
    ~SparkParticleRuntimeSystem() override;

    [[nodiscard]] auto IsActive() const -> bool override;
    [[nodiscard]] auto GetDrawInScene() const -> bool override;
    [[nodiscard]] auto GetBakedBounds() const noexcept -> optional<ParticleBounds3D> override;
    [[nodiscard]] auto GetLiveBounds() const noexcept -> optional<ParticleBounds3D> override;

    void RebaseWorldParticles(vec3 delta) noexcept override;
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

    void RecreateRuntimeSystem(uint32_t random_seed);

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
