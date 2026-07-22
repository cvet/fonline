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

#if FO_EFFEKSEER_PARTICLES

#include "ParticleRuntime.h"

FO_BEGIN_NAMESPACE

class EffekseerParticleRuntimeBackend;

class EffekseerParticleRuntimeSystem final : public ParticleRuntimeSystem
{
    friend class EffekseerParticleRuntimeBackend;
    friend class SafeAlloc;

public:
    struct Impl;

    EffekseerParticleRuntimeSystem(const EffekseerParticleRuntimeSystem&) = delete;
    EffekseerParticleRuntimeSystem(EffekseerParticleRuntimeSystem&&) noexcept = delete;
    auto operator=(const EffekseerParticleRuntimeSystem&) = delete;
    auto operator=(EffekseerParticleRuntimeSystem&&) noexcept = delete;
    ~EffekseerParticleRuntimeSystem() override;

    [[nodiscard]] auto IsActive() const -> bool override;
    [[nodiscard]] auto GetDrawSize(isize32 default_size) const -> isize32 override;
    [[nodiscard]] auto GetDrawInScene() const -> bool override;

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
    struct Impl;

    unique_ptr<Impl> _impl;
};

FO_END_NAMESPACE

#endif
