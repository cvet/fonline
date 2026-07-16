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

#include "VisualParticles.h"
#include "SparkExtension.h"

#include "SPARK.h"

FO_BEGIN_NAMESPACE

struct ParticleManager::Impl
{
    unordered_map<string, SPK::Ref<SPK::System>> BaseSystems {};
};

struct ParticleSystem::Impl
{
    SPK::Ref<SPK::System> System {};
    SPK::Ref<SPK::System> BaseSystem {};
};

ParticleSystem::ParticleSystem(ParticleSystem&&) noexcept = default;

ParticleManager::ParticleManager(ptr<RenderSettings> settings, ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<FileSystem> resources, ptr<GameTimer> game_time, TextureLoader tex_loader) :
    _impl {SafeAlloc::MakeUnique<Impl>()},
    _settings {settings},
    _effectMngr {effect_mngr},
    _render {render},
    _resources {resources},
    _gameTime {game_time},
    _textureLoader {std::move(tex_loader)}
{
    FO_STACK_TRACE_ENTRY();

    static std::once_flag once;
    std::call_once(once, [] { SPK::IO::IOManager::get().registerObject<SPK::FO::SparkQuadRenderer>(); });

    if (_settings->Animation3dFPS != 0) {
        _animUpdateThreshold = iround<int32_t>(1000.0f / numeric_cast<float32_t>(_settings->Animation3dFPS));
    }
}

ParticleManager::~ParticleManager()
{
    FO_STACK_TRACE_ENTRY();
}

auto ParticleManager::Random(int32_t min_value, int32_t max_value) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(min_value <= max_value, "Particle random integer range has an inverted min/max", min_value, max_value);

    return std::uniform_int_distribution<int32_t> {min_value, max_value}(_randomGenerator);
}

auto ParticleManager::CreateParticle(string_view name) -> optional<ParticleSystem>
{
    FO_STACK_TRACE_ENTRY();

    SPK::Ref<SPK::System> base_system;

    if (const auto it = _impl->BaseSystems.find(name); it == _impl->BaseSystems.end()) {
        if (const auto file = _resources->ReadFile(name)) {
            const_span<uint8_t> file_data = file.GetDataSpan();
            base_system = SPK::IO::IOManager::get().loadFromBuffer("xml", make_ptr(file_data.data()).reinterpret_as<char>().get(), numeric_cast<unsigned>(file_data.size()));
        }

        if (base_system) {
            for (size_t i = 0; i < base_system->getNbGroups(); i++) {
                auto&& group = base_system->getGroup(i);

                if (auto&& renderer = SPK::dynamicCast<SPK::FO::SparkQuadRenderer>(group->getRenderer())) {
                    renderer->Setup(name, this);
                }
            }
        }

        _impl->BaseSystems.emplace(name, base_system);
    }
    else {
        base_system = it->second;
    }

    if (!base_system) {
        return std::nullopt;
    }

    auto&& system = SPK::SPKObject::copy(base_system);
    system->initialize();

    ParticleSystem particles {this};
    particles._impl->System = system;
    particles._impl->BaseSystem = base_system;

    return std::move(particles);
}

ParticleSystem::ParticleSystem(ptr<ParticleManager> particle_mngr) :
    _impl {SafeAlloc::MakeUnique<Impl>()},
    _particleMngr {particle_mngr}
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

auto ParticleSystem::GetElapsedTime() const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<float32_t>(_elapsedTime);
}

auto ParticleSystem::GetBaseSystem() -> nptr<SPK::System>
{
    FO_STACK_TRACE_ENTRY();

    return make_nptr(_impl->BaseSystem.get());
}

void ParticleSystem::SetBaseSystem(nptr<SPK::System> system)
{
    FO_STACK_TRACE_ENTRY();

    _impl->BaseSystem = system.get();
}

auto ParticleSystem::GetDrawSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    int32_t max_draw_width = 0;
    int32_t max_draw_height = 0;

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

auto ParticleSystem::GetDrawInScene() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < _impl->System->getNbGroups(); i++) {
        auto&& group = _impl->System->getGroup(i);
        auto&& renderer = SPK::dynamicCast<SPK::FO::SparkQuadRenderer>(group->getRenderer());

        if (renderer && renderer->GetDrawInScene()) {
            return true;
        }
    }

    return false;
}

auto ParticleSystem::GetRenderViewBounds() const noexcept -> optional<ParticleBounds3D>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_impl->System->isAABBComputationEnabled() || _boundsDirty || _impl->System->getNbParticles() == 0) {
        return std::nullopt;
    }

    const SPK::Vector3D& aabb_min = _impl->System->getAABBMin();
    const SPK::Vector3D& aabb_max = _impl->System->getAABBMax();
    const vec3 raw_min {aabb_min.x, aabb_min.y, aabb_min.z};
    const vec3 raw_max {aabb_max.x, aabb_max.y, aabb_max.z};

    if (!std::isfinite(raw_min.x) || !std::isfinite(raw_min.y) || !std::isfinite(raw_min.z) || !std::isfinite(raw_max.x) || !std::isfinite(raw_max.y) || !std::isfinite(raw_max.z) || raw_min.x > raw_max.x || raw_min.y > raw_max.y || raw_min.z > raw_max.z) {
        return std::nullopt;
    }

    const mat44 view = GetRenderViewMatrix();
    ParticleBounds3D result;
    bool initialized = false;

    for (uint32_t corner_index = 0; corner_index < 8; corner_index++) {
        const vec3 corner {
            (corner_index & 1U) != 0 ? raw_max.x : raw_min.x,
            (corner_index & 2U) != 0 ? raw_max.y : raw_min.y,
            (corner_index & 4U) != 0 ? raw_max.z : raw_min.z,
        };
        const glm::vec4 transformed = view * glm::vec4 {corner, 1.0f};

        if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z) || !is_float_equal(transformed.w, 1.0f)) {
            return std::nullopt;
        }

        const vec3 point {transformed};

        if (!initialized) {
            result.Min = point;
            result.Max = point;
            initialized = true;
        }
        else {
            result.Min = glm::min(result.Min, point);
            result.Max = glm::max(result.Max, point);
        }
    }

    return result;
}

void ParticleSystem::EnableBoundsComputation() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_impl->System->isAABBComputationEnabled()) {
        _impl->System->enableAABBComputation(true);
        _boundsDirty = true;
    }
}

void ParticleSystem::RebaseWorldParticles(vec3 delta) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_tiltInProj) {
        return;
    }

    FO_STRONG_ASSERT(std::isfinite(delta.x) && std::isfinite(delta.y) && std::isfinite(delta.z), "Particle world rebase delta must be finite", delta.x, delta.y, delta.z);

    if (delta == vec3 {}) {
        return;
    }

    const SPK::Vector3D spark_delta(delta.x, delta.y, delta.z);

    for (size_t group_index = 0; group_index < _impl->System->getNbGroups(); group_index++) {
        auto&& group = _impl->System->getGroup(group_index);

        for (SPK::GroupIterator particle_it(*group); !particle_it.end(); ++particle_it) {
            particle_it->position() += spark_delta;
            particle_it->oldPosition() += spark_delta;
        }
    }

    if (_impl->System->isAABBComputationEnabled()) {
        _boundsDirty = true;
    }
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

void ParticleSystem::Setup(const mat44& proj, const mat44& world, const vec3& pos_offset, float32_t look_dir_angle, const vec3& view_offset, bool tilt_in_proj)
{
    FO_STACK_TRACE_ENTRY();

    if (!_impl->System->isActive()) {
        return;
    }

    _projMatrix = proj;
    _viewOffset = view_offset;
    _tiltInProj = tilt_in_proj;

    const auto pos_offset_mat = glm::translate(mat44 {1.0f}, pos_offset);
    const auto view_offset_mat = glm::translate(mat44 {1.0f}, view_offset);

    mat44 result_pos_mat;

    if (!_impl->BaseSystem->getTransform().isLocalIdentity()) {
        vec3 result_pos_rot {};
        vec3 result_pos_pos {};
        vec3 result_pos_scale {};
        vec3 skew {};
        glm::vec<4, float32_t, glm::defaultp> perspective {};
        quaternion rotation {};
        glm::decompose(view_offset_mat * world * pos_offset_mat, result_pos_scale, rotation, result_pos_pos, skew, perspective);
        result_pos_rot = glm::eulerAngles(rotation);

        const auto result_pos_pos_mat = glm::translate(mat44 {1.0f}, result_pos_pos);
        const auto look_dir_mat = glm::rotate(mat44 {1.0f}, (look_dir_angle - 90.0f) * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f});

        result_pos_mat = result_pos_pos_mat * look_dir_mat;
    }
    else {
        result_pos_mat = view_offset_mat * world * pos_offset_mat;
    }

    auto result_pos_matrix_values = make_ptr(glm::value_ptr(result_pos_mat));
    _impl->System->getTransform().set(result_pos_matrix_values.get());

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

    const float32_t max_lifetime = _impl->System->getGroup(0)->getMaxLifeTime();
    const float32_t init_time = numeric_cast<float32_t>(_particleMngr->Random(0, iround<int32_t>(max_lifetime * 1000.0f))) / 1000.0f;
    bool updated = false;

    for (float32_t dt = 0.0f; dt < init_time;) {
        (void)_impl->System->updateParticles(std::min(PREWARM_STEP, init_time - dt));
        dt += PREWARM_STEP;
        updated = true;
    }

    if (updated) {
        _boundsDirty = false;
    }
    _elapsedTime += numeric_cast<float64_t>(init_time);
}

void ParticleSystem::Respawn()
{
    FO_STACK_TRACE_ENTRY();

    const bool bounds_computation_enabled = _impl->System->isAABBComputationEnabled();
    auto&& system = SPK::SPKObject::copy(_impl->BaseSystem);
    system->initialize();
    system->enableAABBComputation(bounds_computation_enabled);

    _impl->System = system;
    _elapsedTime = 0.0;
    _lastDrawTime = GetTime();
    _forceDraw = true;
    _boundsDirty = bounds_computation_enabled;
}

void ParticleSystem::Draw()
{
    FO_STACK_TRACE_ENTRY();

    const auto time = GetTime();
    float32_t dt = (time - _lastDrawTime).to_ms<float32_t>() * 0.001f;

    if (_forceDraw && dt <= 0.0f) {
        dt = numeric_cast<float32_t>(std::max(_particleMngr->_animUpdateThreshold, 1)) * 0.001f;
    }

    _lastDrawTime = time;
    _forceDraw = false;

    if (!_impl->System->isActive()) {
        return;
    }

    _elapsedTime += numeric_cast<float64_t>(dt);

    if (dt > 0.0f) {
        (void)_impl->System->updateParticles(dt);
        _boundsDirty = false;
    }
    else if (_boundsDirty) {
        (void)_impl->System->updateParticles(0.0f);
        _boundsDirty = false;
    }

    const mat44 view = GetRenderViewMatrix();
    _particleMngr->_viewProjMatrix = _projMatrix * view;
    _particleMngr->_viewMatrix = view;

    _impl->System->renderParticles();
}

auto ParticleSystem::GetRenderViewMatrix() const noexcept -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    const mat44 view_offset = glm::translate(mat44 {1.0f}, -_viewOffset);
    const mat44 camera_rotation = _tiltInProj ? mat44 {1.0f} : glm::rotate(mat44 {1.0f}, _particleMngr->_settings->MapCameraAngle * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});
    return camera_rotation * view_offset;
}

FO_END_NAMESPACE
