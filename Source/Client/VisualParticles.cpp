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

FO_BEGIN_NAMESPACE

struct ParticleManager::Impl
{
    explicit Impl(const ParticleRuntimeServices& services);

    auto FindBackend(string_view ext) -> nptr<ParticleRuntimeBackend>;
    auto FindBackend(string_view ext) const -> nptr<const ParticleRuntimeBackend>;

    vector<unique_ptr<ParticleRuntimeBackend>> Backends;
};

ParticleManager::Impl::Impl(const ParticleRuntimeServices& services) :
    Backends {CreateParticleRuntimeBackends(services)}
{
    FO_STACK_TRACE_ENTRY();

    unordered_set<string> registered_exts;

    for (const auto& backend_owner : Backends) {
        auto backend = backend_owner.as_ptr();
        vector<string> exts = backend->GetExtensions();

        FO_VERIFY_AND_THROW(!exts.empty(), "Particle runtime backend does not declare any resource extensions");

        for (const string& ext : exts) {
            FO_VERIFY_AND_THROW(!ext.empty(), "Particle runtime backend declares an empty resource extension");

            auto [it, inserted] = registered_exts.emplace(ext);
            ignore_unused(it);
            FO_VERIFY_AND_THROW(inserted, "Particle resource extension is handled by more than one runtime backend", ext);
        }
    }
}

auto ParticleManager::Impl::FindBackend(string_view ext) -> nptr<ParticleRuntimeBackend>
{
    FO_STACK_TRACE_ENTRY();

    for (auto& backend_owner : Backends) {
        auto backend = backend_owner.as_ptr();
        vector<string> exts = backend->GetExtensions();

        if (std::ranges::find(exts, ext) != exts.end()) {
            return backend;
        }
    }

    return nullptr;
}

auto ParticleManager::Impl::FindBackend(string_view ext) const -> nptr<const ParticleRuntimeBackend>
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& backend_owner : Backends) {
        auto backend = backend_owner.as_ptr();
        vector<string> exts = backend->GetExtensions();

        if (std::ranges::find(exts, ext) != exts.end()) {
            return backend;
        }
    }

    return nullptr;
}

ParticleManager::ParticleManager(ptr<RenderSettings> settings, ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<FileSystem> resources, ptr<GameTimer> game_time, ParticleTextureLoader tex_loader) :
    _impl {SafeAlloc::MakeUnique<Impl>(ParticleRuntimeServices {.EffectMngr = effect_mngr, .Render = render, .Resources = resources, .TextureLoader = std::move(tex_loader), .Settings = settings})},
    _settings {settings},
    _gameTime {game_time}
{
    FO_STACK_TRACE_ENTRY();

    if (_settings->Animation3dFPS != 0) {
        _animUpdateThreshold = iround<int32_t>(1000.0f / numeric_cast<float32_t>(_settings->Animation3dFPS));
    }
}

ParticleManager::~ParticleManager()
{
    FO_STACK_TRACE_ENTRY();
}

auto ParticleManager::GetExtensions() const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> exts;

    for (const auto& backend_owner : _impl->Backends) {
        vector<string> backend_exts = backend_owner->GetExtensions();
        exts.insert(exts.end(), backend_exts.begin(), backend_exts.end());
    }

    return exts;
}

void ParticleManager::InvalidateResource(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    for (auto& backend_owner : _impl->Backends) {
        backend_owner->InvalidateResource(name);
    }
}

auto ParticleManager::CreateParticle(string_view name) -> optional<ParticleSystem>
{
    FO_STACK_TRACE_ENTRY();

    string ext = strex(name).get_file_extension();
    auto backend = _impl->FindBackend(ext);

    if (!backend) {
        WriteLog("Particle resource '{}' has an unsupported extension", name);
        return {};
    }

    auto runtime_system = backend->Create(name);

    if (!runtime_system) {
        return {};
    }

    ParticleSystem particles {this, runtime_system.take_not_null()};
    return std::move(particles);
}

ParticleSystem::ParticleSystem(ptr<ParticleManager> particle_mngr, unique_ptr<ParticleRuntimeSystem>&& runtime_system) :
    _runtimeSystem {std::move(runtime_system)},
    _particleMngr {particle_mngr}
{
    FO_STACK_TRACE_ENTRY();

    ResetTiming();
}

ParticleSystem::ParticleSystem(ParticleSystem&&) noexcept = default;

ParticleSystem::~ParticleSystem()
{
    FO_STACK_TRACE_ENTRY();
}

auto ParticleSystem::GetRuntimeSystem() -> ptr<ParticleRuntimeSystem>
{
    FO_STACK_TRACE_ENTRY();

    return _runtimeSystem;
}

auto ParticleSystem::GetRuntimeSystem() const -> ptr<const ParticleRuntimeSystem>
{
    FO_STACK_TRACE_ENTRY();

    return _runtimeSystem;
}

auto ParticleSystem::IsActive() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _runtimeSystem->IsActive();
}

auto ParticleSystem::GetElapsedTime() const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    return numeric_cast<float32_t>(_elapsedTime);
}

auto ParticleSystem::GetDrawInScene() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _runtimeSystem->GetDrawInScene();
}

auto ParticleSystem::GetBakedBounds() const -> optional<ParticleBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    return _runtimeSystem->GetBakedBounds();
}

auto ParticleSystem::GetLiveBounds() const noexcept -> optional<ParticleBounds3D>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _runtimeSystem->GetLiveBounds();
}

auto ParticleSystem::ComputeSpriteFrame(const RenderSettings& settings) const -> ParticleSpriteFrame
{
    FO_STACK_TRACE_ENTRY();

    // Size the sprite frame to the effect's baked extent. The atlas draws the effect through the map camera (a tilt
    // about X) at ModelProjFactor px per world unit; project the 8 baked-box corners through that tilt (the ortho
    // drops view Z) to get the 2D frame, and place the emitter - which projects to the view origin - so the box exactly
    // fills the frame. An effect that emitted no particle (no box) falls back to a small default square.
    optional<ParticleBounds3D> baked = GetBakedBounds();
    float32_t proj_factor = settings.ModelProjFactor;
    ParticleSpriteFrame layout;

    if (!baked) {
        layout.DrawSize = {settings.DefaultParticleDrawWidth, settings.DefaultParticleDrawHeight};
        layout.Offset = {0, settings.DefaultParticleDrawHeight / 4};
        layout.ProjHeight = numeric_cast<float32_t>(layout.DrawSize.height) / proj_factor;
        layout.ProjWidth = numeric_cast<float32_t>(layout.DrawSize.width) / proj_factor;
        layout.World = glm::translate(mat44 {1.0f}, vec3 {layout.ProjWidth / 2.0f, layout.ProjHeight / 4.0f, 0.0f});
        return layout;
    }

    float32_t cos_a = std::cos(settings.MapCameraAngle * DEG_TO_RAD_FLOAT);
    float32_t sin_a = std::sin(settings.MapCameraAngle * DEG_TO_RAD_FLOAT);
    float32_t min_x = std::numeric_limits<float32_t>::max();
    float32_t max_x = std::numeric_limits<float32_t>::lowest();
    float32_t min_y = std::numeric_limits<float32_t>::max();
    float32_t max_y = std::numeric_limits<float32_t>::lowest();

    for (uint32_t corner_index = 0; corner_index < 8; corner_index++) {
        float32_t cx = (corner_index & 1U) != 0 ? baked->Max.x : baked->Min.x;
        float32_t cy = (corner_index & 2U) != 0 ? baked->Max.y : baked->Min.y;
        float32_t cz = (corner_index & 4U) != 0 ? baked->Max.z : baked->Min.z;
        float32_t view_y = cy * cos_a - cz * sin_a;
        min_x = std::min(min_x, cx);
        max_x = std::max(max_x, cx);
        min_y = std::min(min_y, view_y);
        max_y = std::max(max_y, view_y);
    }

    // A small margin so anti-aliased edges are not clipped by the tight frame.
    float32_t margin = 2.0f / proj_factor;
    min_x -= margin;
    max_x += margin;
    min_y -= margin;
    max_y += margin;

    layout.ProjWidth = max_x - min_x;
    layout.ProjHeight = max_y - min_y;
    layout.DrawSize = {std::max(2, iround<int32_t>(layout.ProjWidth * proj_factor)), std::max(2, iround<int32_t>(layout.ProjHeight * proj_factor))};

    // tilt(translate(T)).xy = (T.x, T.y*cos - T.z*sin); with T.z = 0 the box min corner maps to the frame origin.
    layout.World = glm::translate(mat44 {1.0f}, vec3 {-min_x, -min_y / cos_a, 0.0f});

    // Root convention is root = (width/2 - offset.x, height - offset.y) from the top-left; the emitter projects to
    // (-min_x, -min_y) world units from the box origin, i.e. those pixels from the frame's left and bottom.
    int32_t emitter_px_x = iround<int32_t>(-min_x * proj_factor);
    int32_t emitter_px_y = iround<int32_t>(-min_y * proj_factor);
    layout.Offset = {layout.DrawSize.width / 2 - emitter_px_x, emitter_px_y};

    return layout;
}

void ParticleSystem::RebaseWorldParticles(vec3 delta) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _runtimeSystem->RebaseWorldParticles(delta);
}

auto ParticleSystem::GetTime() const -> nanotime
{
    FO_STACK_TRACE_ENTRY();

    return _particleMngr->_gameTime->GetFrameTime();
}

auto ParticleSystem::NeedDraw() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _renderPending && (!IsActive() || GetTime() - _lastRenderTime >= std::chrono::milliseconds(_particleMngr->_animUpdateThreshold));
}

void ParticleSystem::Setup(const mat44& proj, const mat44& world, const vec3& pos_offset, float32_t look_dir_angle, const vec3& view_offset, bool tilt_in_proj)
{
    FO_STACK_TRACE_ENTRY();

    _runtimeSetup = ParticleRuntimeSetup {
        .Projection = proj,
        .World = world,
        .PositionOffset = pos_offset,
        .ViewOffset = view_offset,
        .LookDirectionAngle = look_dir_angle,
        .Scale = _scale,
        .MapCameraAngle = _particleMngr->_settings->MapCameraAngle,
        .TiltInProjection = tilt_in_proj,
    };

    if (IsActive()) {
        ApplyRuntimeSetup();
    }
}

void ParticleSystem::Prewarm()
{
    FO_STACK_TRACE_ENTRY();

    if (!IsActive()) {
        return;
    }

    float32_t elapsed_seconds = _runtimeSystem->Prewarm();
    FO_VERIFY_AND_THROW(std::isfinite(elapsed_seconds) && elapsed_seconds >= 0.0f, "Particle runtime returned an invalid prewarm duration", elapsed_seconds);

    _elapsedTime += numeric_cast<float64_t>(elapsed_seconds);
    _forceDraw = true;
    _renderPending = true;
    _lastUpdateTime = GetTime();
}

void ParticleSystem::Respawn()
{
    FO_STACK_TRACE_ENTRY();

    _runtimeSystem->Respawn(std::nullopt);

    if (IsActive() && _runtimeSetup) {
        ApplyRuntimeSetup();
    }

    ResetTiming();
}

auto ParticleSystem::Respawn(int32_t seed) -> bool
{
    FO_STACK_TRACE_ENTRY();

    _runtimeSystem->Respawn(seed);

    if (IsActive() && _runtimeSetup) {
        ApplyRuntimeSetup();
    }

    ResetTiming();
    return IsActive();
}

void ParticleSystem::Update()
{
    FO_STACK_TRACE_ENTRY();

    nanotime time = GetTime();

    // Use the full-resolution delta. Truncating to whole milliseconds drops sub-millisecond frames, so on an uncapped
    // preview running at a very high frame rate the effect never accumulates time and looks frozen until an occasional
    // slower frame (e.g. while the mouse moves) crosses the 1 ms boundary.
    float32_t delta_seconds = numeric_cast<float32_t>((time - _lastUpdateTime).nanoseconds()) * 1e-9f;

    if (_forceDraw && delta_seconds <= 0.0f) {
        delta_seconds = numeric_cast<float32_t>(std::max(_particleMngr->_animUpdateThreshold, 1)) * 0.001f;
    }

    _lastUpdateTime = time;

    bool was_active = _runtimeSystem->IsActive();

    if (!was_active) {
        return;
    }

    _runtimeSystem->Update(delta_seconds);

    bool is_active = _runtimeSystem->IsActive();

    if (delta_seconds > 0.0f || !is_active) {
        _renderPending = true;
    }
    if (delta_seconds > 0.0f) {
        _elapsedTime += numeric_cast<float64_t>(delta_seconds);
    }
}

void ParticleSystem::RefreshRenderTransform()
{
    FO_STACK_TRACE_ENTRY();

    _runtimeSystem->RefreshRenderTransform();
}

void ParticleSystem::Draw()
{
    FO_STACK_TRACE_ENTRY();

    _lastRenderTime = GetTime();
    _forceDraw = false;
    _renderPending = false;
    _runtimeSystem->Draw();
}

void ParticleSystem::SetScale(float32_t scale)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(std::isfinite(scale) && scale > 0.0f, "Particle scale must be finite and positive", scale);

    _scale = scale;

    if (_runtimeSetup) {
        _runtimeSetup->Scale = scale;

        if (IsActive()) {
            ApplyRuntimeSetup();
            _runtimeSystem->RefreshRenderTransform();
            _forceDraw = true;
            _renderPending = true;
        }
    }
}

void ParticleSystem::ApplyRuntimeSetup()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_runtimeSetup, "Particle runtime setup is missing");
    _runtimeSystem->Setup(*_runtimeSetup);
}

void ParticleSystem::ResetTiming()
{
    FO_STACK_TRACE_ENTRY();

    _elapsedTime = 0.0;
    _forceDraw = true;
    _renderPending = true;
    _lastUpdateTime = GetTime();
    _lastRenderTime = _lastUpdateTime;
}

FO_END_NAMESPACE
