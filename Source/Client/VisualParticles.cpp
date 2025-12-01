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

#include "VisualParticles.h"
#include "SparkExtension.h"

#include "SPARK.h"

FO_BEGIN_NAMESPACE();

struct ParticleManager::Impl
{
    unordered_map<string, SPK::Ref<SPK::System>> BaseSystems {};
};

struct ParticleSystem::Impl
{
    SPK::Ref<SPK::System> System {};
    SPK::Ref<SPK::System> BaseSystem {};
};

ParticleManager::ParticleManager(RenderSettings& settings, EffectManager& effect_mngr, FileSystem& resources, GameTimer& game_time, TextureLoader tex_loader) :
    _impl {SafeAlloc::MakeUnique<Impl>()},
    _settings {&settings},
    _effectMngr {&effect_mngr},
    _resources {&resources},
    _gameTime {&game_time},
    _textureLoader {std::move(tex_loader)}
{
    FO_STACK_TRACE_ENTRY();

    static std::once_flag once;
    std::call_once(once, [] { SPK::IO::IOManager::get().registerObject<SPK::FO::SparkQuadRenderer>(); });

    if (_settings->Animation3dFPS != 0) {
        _animUpdateThreshold = iround<int32>(1000.0f / numeric_cast<float32>(_settings->Animation3dFPS));
    }
}

ParticleManager::~ParticleManager()
{
    FO_STACK_TRACE_ENTRY();
}

auto ParticleManager::CreateParticle(string_view name) -> unique_ptr<ParticleSystem>
{
    FO_STACK_TRACE_ENTRY();

    SPK::Ref<SPK::System> base_system;

    if (const auto it = _impl->BaseSystems.find(name); it == _impl->BaseSystems.end()) {
        if (const auto file = _resources->ReadFile(name)) {
            base_system = SPK::IO::IOManager::get().loadFromBuffer("xml", reinterpret_cast<const char*>(file.GetBuf()), numeric_cast<unsigned>(file.GetSize()));
        }

        if (base_system) {
            for (size_t i = 0; i < base_system->getNbGroups(); i++) {
                auto&& group = base_system->getGroup(i);

                if (auto&& renderer = SPK::dynamicCast<SPK::FO::SparkQuadRenderer>(group->getRenderer())) {
                    renderer->Setup(name, *this);
                }
            }
        }

        _impl->BaseSystems.emplace(name, base_system);
    }
    else {
        base_system = it->second;
    }

    if (!base_system) {
        return nullptr;
    }

    auto&& system = SPK::SPKObject::copy(base_system);
    system->initialize();

    auto particles = SafeAlloc::MakeUnique<ParticleSystem>(*this);
    particles->_impl->System = system;
    particles->_impl->BaseSystem = base_system;

    return particles;
}

ParticleSystem::ParticleSystem(ParticleManager& particle_mngr) :
    _impl {SafeAlloc::MakeUnique<Impl>()},
    _particleMngr {&particle_mngr}
{
    FO_STACK_TRACE_ENTRY();

    _forceDraw = true;
    _lastDrawTime = GetTime();
}

ParticleSystem::~ParticleSystem()
{
    FO_STACK_TRACE_ENTRY();
}

bool ParticleSystem::IsActive() const
{
    FO_STACK_TRACE_ENTRY();

    return _impl->System->isActive();
}

auto ParticleSystem::GetElapsedTime() const -> float32
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<float32>(_elapsedTime);
}

auto ParticleSystem::GetBaseSystem() -> SPK::System*
{
    FO_STACK_TRACE_ENTRY();

    return _impl->BaseSystem.get();
}

void ParticleSystem::SetBaseSystem(SPK::System* system)
{
    FO_STACK_TRACE_ENTRY();

    _impl->BaseSystem = system;
}

auto ParticleSystem::GetDrawSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    int32 max_draw_width = 0;
    int32 max_draw_height = 0;

    for (size_t i = 0; i < _impl->System->getNbGroups(); i++) {
        auto&& group = _impl->System->getGroup(i);
        if (auto&& renderer = SPK::dynamicCast<SPK::FO::SparkQuadRenderer>(group->getRenderer())) {
            max_draw_width = std::max(max_draw_width, renderer->GetDrawWidth());
            max_draw_height = std::max(max_draw_height, renderer->GetDrawHeight());
        }
    }

    if (max_draw_width == 0) {
        max_draw_width = _particleMngr->_settings->DefaultParticleDrawWidth;
    }
    if (max_draw_height == 0) {
        max_draw_height = _particleMngr->_settings->DefaultParticleDrawHeight;
    }

    return {max_draw_width, max_draw_height};
}

auto ParticleSystem::GetTime() const -> nanotime
{
    FO_STACK_TRACE_ENTRY();

    return _particleMngr->_gameTime->GetFrameTime();
}

auto ParticleSystem::NeedDraw() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetTime() - _lastDrawTime >= std::chrono::milliseconds(_particleMngr->_animUpdateThreshold) && _impl->System->isActive();
}

void ParticleSystem::Setup(const mat44& proj, const mat44& world, const vec3& pos_offset, float32 look_dir_angle, const vec3& view_offset)
{
    FO_STACK_TRACE_ENTRY();

    if (!_impl->System->isActive()) {
        return;
    }

    _projMat = proj;
    _viewOffset = view_offset;

    mat44 pos_offset_mat;
    mat44::Translation(pos_offset, pos_offset_mat);

    mat44 view_offset_mat;
    mat44::Translation(view_offset, view_offset_mat);

    mat44 result_pos_mat;

    if (_impl->BaseSystem->getTransform().isLocalIdentity()) {
        vec3 result_pos_rot;
        vec3 result_pos_pos;
        vec3 result_pos_scale;
        (view_offset_mat * world * pos_offset_mat).Decompose(result_pos_scale, result_pos_rot, result_pos_pos);

        mat44 result_pos_pos_mat;
        mat44::Translation(result_pos_pos, result_pos_pos_mat);

        mat44 look_dir_mat;
        mat44::RotationY((look_dir_angle - 90.0f) * DEG_TO_RAD_FLOAT, look_dir_mat);

        result_pos_mat = result_pos_pos_mat * look_dir_mat;
    }
    else {
        result_pos_mat = view_offset_mat * world * pos_offset_mat;
    }

    result_pos_mat.Transpose();

    _impl->System->getTransform().set(result_pos_mat[0]);

    if (const auto local_pos = _impl->BaseSystem->getTransform().getLocalPos(); local_pos != SPK::Vector3D()) {
        _impl->System->getTransform().setPosition(_impl->System->getTransform().getLocalPos() + local_pos);
    }

    _impl->System->updateTransform();
}

void ParticleSystem::Prewarm()
{
    FO_STACK_TRACE_ENTRY();

    if (!_impl->System->isActive()) {
        return;
    }

    const float32 max_lifetime = _impl->System->getGroup(0)->getMaxLifeTime();
    const float32 init_time = numeric_cast<float32>(GenericUtils::Random(0, iround<int32>(max_lifetime * 1000.0f))) / 1000.0f;

    for (float32 dt = 0.0f; dt < init_time;) {
        _impl->System->updateParticles(std::min(PREWARM_STEP, init_time - dt));
        dt += PREWARM_STEP;
    }

    _elapsedTime += numeric_cast<float64>(init_time);
}

void ParticleSystem::Respawn()
{
    FO_STACK_TRACE_ENTRY();

    _impl->System = SPK::SPKObject::copy(_impl->BaseSystem);
    _impl->System->initialize();

    _elapsedTime = 0.0;
}

void ParticleSystem::Draw()
{
    FO_STACK_TRACE_ENTRY();

    const auto time = GetTime();
    const auto dt = (time - _lastDrawTime).to_ms<float32>() * 0.001f;

    _lastDrawTime = time;
    _forceDraw = false;

    if (!_impl->System->isActive()) {
        return;
    }

    _elapsedTime += numeric_cast<float64>(dt);

    if (dt > 0.0f) {
        _impl->System->updateParticles(dt);
    }

    mat44 view_offset_mat;
    mat44::Translation({-_viewOffset.x, -_viewOffset.y, -_viewOffset.z}, view_offset_mat);

    mat44 cam_rot_mat;
    mat44::RotationX(_particleMngr->_settings->MapCameraAngle * DEG_TO_RAD_FLOAT, cam_rot_mat);

    mat44 view = view_offset_mat * cam_rot_mat;
    view.Transpose();

    mat44 proj = _projMat;
    proj.Transpose();

    _particleMngr->_projMatColMaj = view * proj;
    _particleMngr->_viewMatColMaj = view;

    _impl->System->renderParticles();
}

FO_END_NAMESPACE();
