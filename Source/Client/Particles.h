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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "FileSystem.h"
#include "Settings.h"

class ParticleSystem;

class ParticleManager final
{
    friend class ParticleSystem;

public:
    ParticleManager(RenderSettings& settings, FileSystem& file_sys);
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager(ParticleManager&&) noexcept = delete;
    auto operator=(const ParticleManager&) = delete;
    auto operator=(ParticleManager&&) noexcept = delete;
    ~ParticleManager();

    [[nodiscard]] auto CreateParticles(string_view name) -> unique_ptr<ParticleSystem>;

private:
    struct ParticleManagerData;
    ParticleManagerData* _data {};
    RenderSettings& _settings;
    FileSystem& _fileSys;
};

class ParticleSystem final
{
    friend class ParticleManager;

public:
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) noexcept = delete;
    auto operator=(const ParticleSystem&) = delete;
    auto operator=(ParticleSystem&&) noexcept = delete;
    ~ParticleSystem();

    [[nodiscard]] auto IsActive() const -> bool;

    void Update(float dt, const mat44& world, const vec3& pos_offest, float look_dir, const vec3& view_offset);
    void Draw(const mat44& proj, const vec3& view_offset) const;

private:
    explicit ParticleSystem(ParticleManager& particle_mngr);

    struct ParticleSystemData;
    ParticleSystemData* _data {};
    ParticleManager& _particleMngr;
    bool _nonConstHelper {};
};