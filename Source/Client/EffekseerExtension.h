// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_EFFEKSEER_PARTICLES

#include "ParticleRuntime.h"

FO_BEGIN_NAMESPACE

void InitializeEffekseerMemory() noexcept;

class EffekseerParticleRuntimeBackend;

class EffekseerParticleRuntimeSystem final : public ParticleRuntimeSystem
{
    friend class EffekseerParticleRuntimeBackend;
    friend class safe_alloc;

public:
    struct Impl;

    EffekseerParticleRuntimeSystem(const EffekseerParticleRuntimeSystem&) = delete;
    EffekseerParticleRuntimeSystem(EffekseerParticleRuntimeSystem&&) noexcept = delete;
    auto operator=(const EffekseerParticleRuntimeSystem&) = delete;
    auto operator=(EffekseerParticleRuntimeSystem&&) noexcept = delete;
    ~EffekseerParticleRuntimeSystem() override;

    [[nodiscard]] auto IsActive() const -> bool override;
    [[nodiscard]] auto GetDrawInScene() const -> bool override;
    [[nodiscard]] auto GetBakedBounds() const noexcept -> optional<ParticleBounds3D> override;
    [[nodiscard]] auto GetLiveBounds() const noexcept -> optional<ParticleBounds3D> override;

    void Setup(const ParticleRuntimeSetup& setup) override;
    auto Prewarm() -> float32_t override;
    void Respawn(optional<int32_t> seed) override;
    void Update(float32_t delta_seconds) override;
    void RefreshRenderTransform() override;
    void Draw() override;

private:
    explicit EffekseerParticleRuntimeSystem(unique_ptr<Impl>&& impl);

    unique_ptr<Impl> _impl;
};

class EffekseerParticleRuntimeBackend final : public ParticleRuntimeBackend
{
public:
    explicit EffekseerParticleRuntimeBackend(const ParticleRuntimeServices& services);
    EffekseerParticleRuntimeBackend(const EffekseerParticleRuntimeBackend&) = delete;
    EffekseerParticleRuntimeBackend(EffekseerParticleRuntimeBackend&&) noexcept = delete;
    auto operator=(const EffekseerParticleRuntimeBackend&) = delete;
    auto operator=(EffekseerParticleRuntimeBackend&&) noexcept = delete;
    ~EffekseerParticleRuntimeBackend() override;

    [[nodiscard]] auto GetExtensions() const -> vector<string> override;

    void InvalidateResource(string_view path) override;
    auto Create(string_view path) -> unique_nptr<ParticleRuntimeSystem> override;

private:
    class Impl;

    unique_ptr<Impl> _impl;
};

// Enforced before Effekseer's unchecked parser allocates from serialized counts, so a compact count bomb cannot
// amplify into an unbounded decoded model
inline constexpr size_t EFFEKSEER_MODEL_PAYLOAD_SIZE_MAX {64U * 1024U * 1024U};
inline constexpr int32_t EFFEKSEER_MODEL_FRAME_COUNT_MAX {4096};
inline constexpr int32_t EFFEKSEER_MODEL_VERTEX_COUNT_MAX {64000};
inline constexpr int32_t EFFEKSEER_MODEL_FACE_COUNT_MAX {EFFEKSEER_MODEL_VERTEX_COUNT_MAX / 3};

// Effekseer's model constructor trusts every serialized count without consulting the buffer size, so the part it
// will read is validated first; a valid legacy payload may carry trailing bytes it ignores
auto ValidateEffekseerModelPayload(const_span<uint8_t> data) -> optional<string>;

// Only our runtime reads the .efk — the Effekseer editor never reopens it — so the baker may append this trailer
// after the payload; it is mandatory, and a missing or malformed one throws
constexpr uint32_t EFFEKSEER_BOUNDS_TRAILER_MAGIC {0x42424546u}; // bytes 'F','E','B','B' little-endian

struct EffekseerBoundsTrailer
{
    size_t PayloadSize {}; // number of Effekseer payload bytes preceding the trailer
    vec3 PositionMin {};
    vec3 PositionMax {};
    float32_t BillboardRadius {};
};

void AppendEffekseerBoundsTrailer(vector<uint8_t>& binary, const vec3& min_bounds, const vec3& max_bounds, float32_t billboard_radius);
auto ReadEffekseerBoundsTrailer(const_span<uint8_t> binary) -> EffekseerBoundsTrailer;

FO_END_NAMESPACE

#endif
