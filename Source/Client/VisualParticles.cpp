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

// Todo: improve particles in 2D

#include "VisualParticles.h"
#include "SparkExtension.h"

#include "SPARK.h"

struct ParticleManager::Impl
{
    std::unordered_map<string, SPK::Ref<SPK::System>> BaseSystems {};
};

struct ParticleSystem::Impl
{
    SPK::Ref<SPK::System> System {};
    SPK::Ref<SPK::System> BaseSystem {};
};

ParticleManager::ParticleManager(RenderSettings& settings, EffectManager& effect_mngr, FileSystem& resources, TextureLoader tex_loader) :
    _settings {settings}, //
    _effectMngr {effect_mngr},
    _resources {resources},
    _textureLoader {std::move(tex_loader)}
{
    STACK_TRACE_ENTRY();

    static std::once_flag once;
    std::call_once(once, [] { SPK::IO::IOManager::get().registerObject<SPK::FO::SparkQuadRenderer>(); });

    _ipml = std::make_unique<Impl>();
}

ParticleManager::~ParticleManager()
{
    STACK_TRACE_ENTRY();
}

auto ParticleManager::CreateParticles(string_view name) -> unique_ptr<ParticleSystem>
{
    STACK_TRACE_ENTRY();

    SPK::Ref<SPK::System> base_system;

    if (const auto it = _ipml->BaseSystems.find(string(name)); it == _ipml->BaseSystems.end()) {
        if (const auto file = _resources.ReadFile(name)) {
            base_system = SPK::IO::IOManager::get().loadFromBuffer("xml", reinterpret_cast<const char*>(file.GetBuf()), static_cast<unsigned>(file.GetSize()));
        }

        if (base_system) {
            for (size_t i = 0; i < base_system->getNbGroups(); i++) {
                auto&& group = base_system->getGroup(i);
                if (auto&& renderer = SPK::dynamicCast<SPK::FO::SparkQuadRenderer>(group->getRenderer())) {
                    renderer->Setup(name, *this);
                }
            }
        }

        _ipml->BaseSystems.emplace(name, base_system);
    }
    else {
        base_system = it->second;
    }

    if (!base_system) {
        return nullptr;
    }

    auto&& system = SPK::SPKObject::copy(base_system);
    system->initialize();

    auto&& particles = unique_ptr<ParticleSystem>(new ParticleSystem(*this));
    particles->_impl->System = system;
    particles->_impl->BaseSystem = base_system;

    return std::move(particles);
}

ParticleSystem::ParticleSystem(ParticleManager& particle_mngr) : _particleMngr {particle_mngr}
{
    STACK_TRACE_ENTRY();

    _impl = std::make_unique<Impl>();
}

ParticleSystem::~ParticleSystem()
{
    STACK_TRACE_ENTRY();
}

bool ParticleSystem::IsActive() const
{
    STACK_TRACE_ENTRY();

    return _impl->System->isActive();
}

auto ParticleSystem::GetElapsedTime() const -> float
{
    STACK_TRACE_ENTRY();

    return static_cast<float>(_elapsedTime);
}

auto ParticleSystem::GetBaseSystem() -> SPK::System*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _impl->BaseSystem.get();
}

void ParticleSystem::SetBaseSystem(SPK::System* system)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _impl->BaseSystem = system;
}

void ParticleSystem::Respawn()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _impl->System = SPK::SPKObject::copy(_impl->BaseSystem);
    _impl->System->initialize();

    _elapsedTime = 0.0;
}

void ParticleSystem::Update(float dt, const mat44& world, const vec3& pos_offest, float look_dir, const vec3& view_offset)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!_impl->System->isActive()) {
        return;
    }

    _elapsedTime += static_cast<double>(dt);

    mat44 pos_offset_mat;
    mat44::Translation(pos_offest, pos_offset_mat);

    mat44 view_offset_mat;
    mat44::Translation(view_offset, view_offset_mat);

    if (!_impl->BaseSystem->getTransform().isLocalIdentity()) {
        vec3 result_pos_rot;
        vec3 result_pos_pos;
        vec3 result_pos_scale;
        (view_offset_mat * world * pos_offset_mat).Decompose(result_pos_scale, result_pos_rot, result_pos_pos);

        mat44 result_pos_pos_mat;
        mat44::Translation(result_pos_pos, result_pos_pos_mat);

        mat44 look_dir_mat;
        mat44::RotationY((look_dir - 90.0f) * PI_FLOAT / 180.0f, look_dir_mat);

        mat44 result_pos_mat = result_pos_pos_mat * look_dir_mat;

        _impl->System->getTransform().set(result_pos_mat.Transpose()[0]);
    }
    else {
        mat44 result_pos_mat = view_offset_mat * world * pos_offset_mat;

        _impl->System->getTransform().set(result_pos_mat.Transpose()[0]);
    }

    if (const auto local_pos = _impl->BaseSystem->getTransform().getLocalPos(); local_pos != SPK::Vector3D()) {
        _impl->System->getTransform().setPosition(_impl->System->getTransform().getLocalPos() + local_pos);
    }

    _impl->System->updateTransform();
    _impl->System->updateParticles(dt);
}

void ParticleSystem::Draw(const mat44& proj, const vec3& view_offset, float cam_rot) const
{
    STACK_TRACE_ENTRY();

    if (!_impl->System->isActive()) {
        return;
    }

    mat44 view_offset_mat;
    mat44::Translation({-view_offset.x, -view_offset.y, -view_offset.z}, view_offset_mat);

    mat44 cam_rot_mat;
    mat44::RotationX(cam_rot * PI_FLOAT / 180.0f, cam_rot_mat);

    mat44 view = view_offset_mat * cam_rot_mat;
    view.Transpose();

    _particleMngr._projMat = view * proj;
    _particleMngr._viewMat = view;

    _impl->System->renderParticles();
}
