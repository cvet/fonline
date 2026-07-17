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

#include "SparkExtension.h"

#if FO_SPARK_PARTICLES

#include "Application.h"
#include "EffectManager.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

static constexpr float32_t SPARK_PREWARM_STEP = 0.5f;

struct SparkParticleRuntimeBackend::Impl
{
    explicit Impl(const ParticleRuntimeServices& services) :
        Services {services}
    {
        FO_STACK_TRACE_ENTRY();
    }

    ParticleRuntimeServices Services;
    unordered_map<string, SPK::Ref<SPK::System>> BaseSystems {};
    std::mt19937 RandomGenerator {MakeSeededRandomGenerator()};
    mat44 ViewProjectionMatrix {};
    mat44 ViewMatrix {};
};

struct SparkParticleRuntimeSystem::Impl
{
    Impl(ptr<SparkParticleRuntimeBackend> runtime, string_view path, SPK::Ref<SPK::System> base_system) :
        Runtime {runtime},
        Path {path},
        BaseSystem {std::move(base_system)}
    {
        FO_STACK_TRACE_ENTRY();

        RecreateRuntimeSystem();
    }

    void RecreateRuntimeSystem();

    ptr<SparkParticleRuntimeBackend> Runtime;
    string Path;
    SPK::Ref<SPK::System> RuntimeSystem {};
    SPK::Ref<SPK::System> BaseSystem {};
    mat44 ViewProjectionMatrix {};
    mat44 ViewMatrix {};
    bool BaseSystemDetached {};
};

static auto SetupSparkSystemRenderers(string_view path, const SPK::Ref<SPK::System>& system, ptr<SparkParticleRuntimeBackend> runtime) -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool render_dependencies_loaded = true;

    for (size_t i = 0; i < system->getNbGroups(); i++) {
        auto&& group = system->getGroup(i);

        if (auto&& renderer = SPK::dynamicCast<SPK::FO::SparkQuadRenderer>(group->getRenderer())) {
            render_dependencies_loaded &= renderer->Setup(path, runtime);
        }
    }

    return render_dependencies_loaded;
}

SparkParticleRuntimeBackend::SparkParticleRuntimeBackend(const ParticleRuntimeServices& services) :
    _impl {SafeAlloc::MakeUnique<Impl>(services)}
{
    FO_STACK_TRACE_ENTRY();

    SPK::FO::EnsureSparkParticleObjectsRegistered();
}

SparkParticleRuntimeBackend::~SparkParticleRuntimeBackend()
{
    FO_STACK_TRACE_ENTRY();
}

auto SparkParticleRuntimeBackend::GetExtensions() const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    return {"spark"};
}

auto SparkParticleRuntimeBackend::SupportsSeededRespawn() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return false;
}

void SparkParticleRuntimeBackend::InvalidateResource(string_view path)
{
    FO_STACK_TRACE_ENTRY();

    if (strex(path).get_file_extension() == "spark") {
        _impl->BaseSystems.erase(string {path});
    }
    else {
        _impl->BaseSystems.clear();
    }
}

auto SparkParticleRuntimeBackend::Create(string_view path) -> unique_nptr<ParticleRuntimeSystem>
{
    FO_STACK_TRACE_ENTRY();

    if (strex(path).get_file_extension() != "spark") {
        return nullptr;
    }

    SPK::Ref<SPK::System> base_system;

    if (const auto it = _impl->BaseSystems.find(path); it == _impl->BaseSystems.end()) {
        if (const auto file = _impl->Services.Resources->ReadFile(path)) {
            const_span<uint8_t> file_data = file.GetDataSpan();
            base_system = SPK::IO::IOManager::get().loadFromBuffer("spk", ptr<const uint8_t> {file_data.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(file_data.size()));

            if (!base_system) {
                base_system = SPK::IO::IOManager::get().loadFromBuffer("xml", ptr<const uint8_t> {file_data.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(file_data.size()));
            }
        }

        if (base_system && !SetupSparkSystemRenderers(path, base_system, this)) {
            WriteLog("SPARK particle '{}' has a missing render effect or texture", path);
            base_system = SPK::Ref<SPK::System>();
        }

        if (base_system) {
            _impl->BaseSystems.emplace(path, base_system);
        }
    }
    else {
        base_system = it->second;
    }

    if (!base_system) {
        return nullptr;
    }

    return SafeAlloc::MakeUnique<SparkParticleRuntimeSystem>(this, path, std::move(base_system));
}

SparkParticleRuntimeSystem::SparkParticleRuntimeSystem(ptr<SparkParticleRuntimeBackend> runtime, string_view path, SPK::Ref<SPK::System> base_system) :
    _impl {SafeAlloc::MakeUnique<Impl>(runtime, path, std::move(base_system))}
{
    FO_STACK_TRACE_ENTRY();
}

SparkParticleRuntimeSystem::~SparkParticleRuntimeSystem()
{
    FO_STACK_TRACE_ENTRY();
}

auto SparkParticleRuntimeSystem::IsActive() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _impl->RuntimeSystem->isActive();
}

auto SparkParticleRuntimeSystem::GetDrawSize(isize32 default_size) const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    int32_t max_draw_width = 0;
    int32_t max_draw_height = 0;

    for (size_t i = 0; i < _impl->RuntimeSystem->getNbGroups(); i++) {
        auto&& group = _impl->RuntimeSystem->getGroup(i);

        if (auto&& renderer = SPK::dynamicCast<SPK::FO::SparkQuadRenderer>(group->getRenderer())) {
            max_draw_width = std::max(max_draw_width, renderer->GetDrawWidth());
            max_draw_height = std::max(max_draw_height, renderer->GetDrawHeight());
        }
    }

    if (max_draw_width == 0) {
        max_draw_width = default_size.width;
    }
    if (max_draw_height == 0) {
        max_draw_height = default_size.height;
    }

    return {max_draw_width, max_draw_height};
}

auto SparkParticleRuntimeSystem::GetDrawInScene() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < _impl->RuntimeSystem->getNbGroups(); i++) {
        auto&& group = _impl->RuntimeSystem->getGroup(i);
        auto&& renderer = SPK::dynamicCast<SPK::FO::SparkQuadRenderer>(group->getRenderer());

        if (renderer && renderer->GetDrawInScene()) {
            return true;
        }
    }

    return false;
}

void SparkParticleRuntimeSystem::Setup(const ParticleRuntimeSetup& setup)
{
    FO_STACK_TRACE_ENTRY();

    const auto position_offset_matrix = glm::translate(mat44 {1.0f}, setup.PositionOffset);
    const auto view_offset_matrix = glm::translate(mat44 {1.0f}, setup.ViewOffset);
    mat44 result_position_matrix;

    if (!_impl->BaseSystem->getTransform().isLocalIdentity()) {
        vec3 result_position {};
        vec3 result_position_scale {};
        vec3 skew {};
        glm::vec<4, float32_t, glm::defaultp> perspective {};
        quaternion rotation {};
        glm::decompose(view_offset_matrix * setup.World * position_offset_matrix, result_position_scale, rotation, result_position, skew, perspective);
        const auto result_position_translation_matrix = glm::translate(mat44 {1.0f}, result_position);
        const auto look_direction_matrix = glm::rotate(mat44 {1.0f}, (setup.LookDirectionAngle - 90.0f) * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f});
        result_position_matrix = result_position_translation_matrix * look_direction_matrix;
    }
    else {
        result_position_matrix = view_offset_matrix * setup.World * position_offset_matrix;
    }

    result_position_matrix *= glm::scale(mat44 {1.0f}, vec3 {setup.Scale, setup.Scale, setup.Scale});

    ptr<const float32_t> result_position_matrix_values = glm::value_ptr(result_position_matrix);
    _impl->RuntimeSystem->getTransform().set(result_position_matrix_values.get());

    if (const auto local_position = _impl->BaseSystem->getTransform().getLocalPos(); local_position != SPK::Vector3D()) {
        _impl->RuntimeSystem->getTransform().setPosition(_impl->RuntimeSystem->getTransform().getLocalPos() + local_position);
    }

    _impl->RuntimeSystem->updateTransform();

    const auto camera_rotation_matrix = setup.TiltInProjection ? mat44 {1.0f} : glm::rotate(mat44 {1.0f}, setup.MapCameraAngle * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});
    _impl->ViewMatrix = camera_rotation_matrix * glm::translate(mat44 {1.0f}, -setup.ViewOffset);
    _impl->ViewProjectionMatrix = setup.Projection * _impl->ViewMatrix;
}

auto SparkParticleRuntimeSystem::Prewarm() -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    if (!IsActive()) {
        return 0.0f;
    }

    FO_VERIFY_AND_THROW(_impl->RuntimeSystem->getNbGroups() != 0, "Cannot prewarm a SPARK particle system without groups", _impl->Path);
    const float32_t max_lifetime = _impl->RuntimeSystem->getGroup(0)->getMaxLifeTime();
    const int32_t max_lifetime_ms = iround<int32_t>(max_lifetime * 1000.0f);
    FO_VERIFY_AND_THROW(max_lifetime_ms >= 0, "SPARK particle system has a negative maximum lifetime", _impl->Path, max_lifetime);
    const int32_t init_time_ms = std::uniform_int_distribution<int32_t> {0, max_lifetime_ms}(_impl->Runtime->_impl->RandomGenerator);
    const float32_t init_time = numeric_cast<float32_t>(init_time_ms) / 1000.0f;

    for (float32_t delta_seconds = 0.0f; delta_seconds < init_time; delta_seconds += SPARK_PREWARM_STEP) {
        _impl->RuntimeSystem->updateParticles(std::min(SPARK_PREWARM_STEP, init_time - delta_seconds));
    }

    return init_time;
}

void SparkParticleRuntimeSystem::Respawn(optional<int32_t> seed)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!seed.has_value(), "SPARK particle runtime does not support seeded respawn", _impl->Path, seed.value_or(0));
    _impl->RecreateRuntimeSystem();
}

void SparkParticleRuntimeSystem::Update(float32_t delta_seconds)
{
    FO_STACK_TRACE_ENTRY();

    if (_impl->RuntimeSystem->isActive() && delta_seconds > 0.0f) {
        _impl->RuntimeSystem->updateParticles(delta_seconds);
    }
}

void SparkParticleRuntimeSystem::RefreshRenderTransform()
{
    FO_STACK_TRACE_ENTRY();
}

void SparkParticleRuntimeSystem::Draw()
{
    FO_STACK_TRACE_ENTRY();

    if (!_impl->RuntimeSystem->isActive()) {
        return;
    }

    _impl->Runtime->_impl->ViewProjectionMatrix = _impl->ViewProjectionMatrix;
    _impl->Runtime->_impl->ViewMatrix = _impl->ViewMatrix;
    _impl->RuntimeSystem->renderParticles();
}

auto SparkParticleRuntimeSystem::GetEditableBaseSystem() -> SPK::Ref<SPK::System>
{
    FO_STACK_TRACE_ENTRY();

    if (!_impl->BaseSystemDetached) {
        _impl->BaseSystem = SPK::SPKObject::copy(_impl->BaseSystem);
        _impl->BaseSystemDetached = true;
        _impl->RecreateRuntimeSystem();
    }

    return _impl->BaseSystem;
}

void SparkParticleRuntimeSystem::ReplaceBaseSystem(SPK::Ref<SPK::System> system)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(system, "Cannot replace a SPARK base system with a null system", _impl->Path);
    _impl->BaseSystem = std::move(system);
    _impl->BaseSystemDetached = true;
    _impl->RecreateRuntimeSystem();
}

void SparkParticleRuntimeSystem::Impl::RecreateRuntimeSystem()
{
    FO_STACK_TRACE_ENTRY();

    (void)SetupSparkSystemRenderers(Path, BaseSystem, Runtime);
    RuntimeSystem = SPK::SPKObject::copy(BaseSystem);
    RuntimeSystem->initialize();
}

FO_END_NAMESPACE

namespace SPK::FO
{
    void EnsureSparkParticleObjectsRegistered()
    {
        FO_STACK_TRACE_ENTRY();

        auto& io_mngr = IO::IOManager::get();
        io_mngr.ensureObjectRegistered<SparkQuadRenderer>();
    }

    SparkRenderBuffer::SparkRenderBuffer(size_t vertices, ptr<FO_NAMESPACE IAppRender> render) :
        _renderBuf {render->CreateDrawBuffer(false)},
        _render {render}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(vertices > 0, "Spark render buffer cannot be created without vertices");
        FO_VERIFY_AND_THROW(vertices % 4 == 0, "Spark render buffer vertex count must describe whole particle quads", vertices, 4);

        auto& vbuf = _renderBuf->Vertices;
        auto& vpos = _renderBuf->VertCount;
        auto& ibuf = _renderBuf->Indices;
        auto& ipos = _renderBuf->IndCount;

        vpos = vertices;
        ipos = vertices / 4 * 6;

        vbuf.resize(vpos);
        ibuf.resize(ipos);

        if constexpr (sizeof(vindex_t) == 2) {
            FO_VERIFY_AND_THROW(ibuf.size() <= 0xFFFF, "Spark render buffer index count exceeds 16-bit vertex index capacity", ibuf.size(), 0xFFFF, vertices);
        }

        for (size_t i = 0; i < ibuf.size() / 6; i++) {
            ibuf[i * 6 + 0] = numeric_cast<vindex_t>(i * 4 + 0);
            ibuf[i * 6 + 1] = numeric_cast<vindex_t>(i * 4 + 1);
            ibuf[i * 6 + 2] = numeric_cast<vindex_t>(i * 4 + 2);
            ibuf[i * 6 + 3] = numeric_cast<vindex_t>(i * 4 + 2);
            ibuf[i * 6 + 4] = numeric_cast<vindex_t>(i * 4 + 3);
            ibuf[i * 6 + 5] = numeric_cast<vindex_t>(i * 4 + 0);
        }
    }

    void SparkRenderBuffer::PositionAtStart()
    {
        FO_STACK_TRACE_ENTRY();

        _curVertexIndex = 0;
        _curTexCoordIndex = 0;
    }

    void SparkRenderBuffer::SetNextVertex(const Vector3D& pos, const Color& color)
    {
        FO_STACK_TRACE_ENTRY();

        auto& v = _renderBuf->Vertices[_curVertexIndex++];

        v.PosX = pos.x;
        v.PosY = pos.y;
        v.PosZ = pos.z;
        v.Color = ucolor {color.r, color.g, color.b, color.a};
    }

    void SparkRenderBuffer::SetNextTexCoord(float32_t tu, float32_t tv)
    {
        FO_STACK_TRACE_ENTRY();

        auto& v = _renderBuf->Vertices[_curTexCoordIndex++];

        v.TexU = tu;
        v.TexV = tv;
        v.EggFlags[0] = 0.0f;
        v.EggFlags[1] = 0.0f;
    }

    void SparkRenderBuffer::Render(size_t vertices, ptr<RenderEffect> effect) const
    {
        FO_STACK_TRACE_ENTRY();

        if (vertices == 0) {
            return;
        }

        _renderBuf->Upload(EffectUsage::QuadSprite, vertices, vertices / 4 * 6);
        effect->DrawBuffer(_renderBuf, 0, vertices / 4 * 6);
    }

    SparkQuadRenderer::SparkQuadRenderer(bool needs_dataset) :
        Renderer(needs_dataset)
    {
        FO_STACK_TRACE_ENTRY();
    }

    auto SparkQuadRenderer::Create() -> Ref<SparkQuadRenderer>
    {
        FO_STACK_TRACE_ENTRY();

        return SPK_NEW(SparkQuadRenderer);
    }

    auto SparkQuadRenderer::Setup(string_view path, ptr<FO_NAMESPACE SparkParticleRuntimeBackend> runtime) -> bool
    {
        FO_STACK_TRACE_ENTRY();

        if (_runtime) {
            FO_VERIFY_AND_THROW(_runtime == runtime && _path == path, "SPARK particle renderer is already bound to another runtime", _path, path);
            return _effect && _texture;
        }

        _path = path;
        _runtime = runtime;

        if (!_effectName.empty()) {
            SetEffectName(_effectName);
        }
        if (!_textureName.empty()) {
            SetTextureName(_textureName);
        }

        return _effect && _texture;
    }

    void SparkQuadRenderer::AddPosAndColor(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        render_buffer->SetNextVertex(particle.position() + quadSide() + quadUp(), particle.getColor()); // top right vertex
        render_buffer->SetNextVertex(particle.position() - quadSide() + quadUp(), particle.getColor()); // top left vertex
        render_buffer->SetNextVertex(particle.position() - quadSide() - quadUp(), particle.getColor()); // bottom left vertex
        render_buffer->SetNextVertex(particle.position() + quadSide() - quadUp(), particle.getColor()); // bottom right vertex
    }

    void SparkQuadRenderer::AddTexture2D(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(particle);

        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + 1.0f * _textureAtlasOffset.width, _textureAtlasOffset.y + 0.0f * _textureAtlasOffset.height);
        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + 0.0f * _textureAtlasOffset.width, _textureAtlasOffset.y + 0.0f * _textureAtlasOffset.height);
        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + 0.0f * _textureAtlasOffset.width, _textureAtlasOffset.y + 1.0f * _textureAtlasOffset.height);
        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + 1.0f * _textureAtlasOffset.width, _textureAtlasOffset.y + 1.0f * _textureAtlasOffset.height);
    }

    void SparkQuadRenderer::AddTexture2DAtlas(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        computeAtlasCoordinates(particle);

        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + textureAtlasU1() * _textureAtlasOffset.width, _textureAtlasOffset.y + textureAtlasV0() * _textureAtlasOffset.height);
        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + textureAtlasU0() * _textureAtlasOffset.width, _textureAtlasOffset.y + textureAtlasV0() * _textureAtlasOffset.height);
        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + textureAtlasU0() * _textureAtlasOffset.width, _textureAtlasOffset.y + textureAtlasV1() * _textureAtlasOffset.height);
        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + textureAtlasU1() * _textureAtlasOffset.width, _textureAtlasOffset.y + textureAtlasV1() * _textureAtlasOffset.height);
    }

    RenderBuffer* SparkQuadRenderer::attachRenderBuffer(const Group& group) const
    {
        FO_STACK_TRACE_ENTRY();

        return SPK_NEW(SparkRenderBuffer, group.getCapacity() << 2, _runtime->_impl->Services.Render);
    }

    void SparkQuadRenderer::render(const Group& group, const DataSet* dataSet, RenderBuffer* renderBuffer) const
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(dataSet);

        FO_VERIFY_AND_THROW(_runtime, "SPARK particle runtime is null");

        if (!_effect || !_texture) {
            return;
        }

        FO_VERIFY_AND_THROW(renderBuffer, "Missing required render buffer");
        ptr<RenderBuffer> render_buffer = renderBuffer;
        auto nullable_spark_render_buffer = render_buffer.dyn_cast<SparkRenderBuffer>();
        FO_VERIFY_AND_THROW(nullable_spark_render_buffer, "Render buffer is not a spark render buffer");

        auto spark_render_buffer = nullable_spark_render_buffer.as_ptr();
        spark_render_buffer->PositionAtStart();

        if (_modelView != _runtime->_impl->ViewMatrix) {
            _modelView = _runtime->_impl->ViewMatrix;
            _invModelView = glm::inverse(_modelView);
        }

        if (!group.isEnabled(PARAM_TEXTURE_INDEX)) {
            if (!group.isEnabled(PARAM_ANGLE)) {
                _renderParticle = &SparkQuadRenderer::Render2D;
            }
            else {
                _renderParticle = &SparkQuadRenderer::Render2DRot;
            }
        }
        else {
            if (!group.isEnabled(PARAM_ANGLE)) {
                _renderParticle = &SparkQuadRenderer::Render2DAtlas;
            }
            else {
                _renderParticle = &SparkQuadRenderer::Render2DAtlasRot;
            }
        }

        const bool globalOrientation = precomputeOrientation3D(group, //
            Vector3D(-_invModelView[0][2], -_invModelView[1][2], -_invModelView[2][2]), //
            Vector3D(_invModelView[0][1], _invModelView[1][1], _invModelView[2][1]), //
            Vector3D(_invModelView[0][3], _invModelView[1][3], _invModelView[2][3]));

        if (globalOrientation) {
            computeGlobalOrientation3D(group);

            for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt) {
                (this->*_renderParticle)(*particleIt, spark_render_buffer);
            }
        }
        else {
            for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt) {
                computeSingleOrientation3D(*particleIt);
                (this->*_renderParticle)(*particleIt, spark_render_buffer);
            }
        }

        FO_VERIFY_AND_THROW(_effect, "Missing required effect");
        FO_VERIFY_AND_THROW(_texture, "Missing required texture");

        _effect->ProjBuf = RenderEffect::ProjBuffer();
        ptr<float32_t> projection_matrix = _effect->ProjBuf->ProjMatrix;
        ptr<const float32_t> projection_matrix_values = glm::value_ptr(_runtime->_impl->ViewProjectionMatrix);
        MemCopy(projection_matrix, projection_matrix_values, 16 * sizeof(float32_t));
        _effect->MainTex = _texture;

        spark_render_buffer->Render(group.getNbParticles() << 2, _effect.as_ptr());
    }

    void SparkQuadRenderer::computeAABB(Vector3D& aabbMin, Vector3D& aabbMax, const Group& group, const DataSet* dataSet) const
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(dataSet);

        const float32_t diagonal = group.getGraphicalRadius() * std::sqrt(scaleX * scaleX + scaleY * scaleY);
        const Vector3D diag_v(diagonal, diagonal, diagonal);

        if (group.isEnabled(PARAM_SCALE)) {
            for (ConstGroupIterator it(group); !it.end(); ++it) {
                Vector3D scaledDiagV = diag_v * it->getParamNC(PARAM_SCALE);
                aabbMin.setMin(it->position() - scaledDiagV);
                aabbMax.setMax(it->position() + scaledDiagV);
            }
        }
        else {
            for (ConstGroupIterator it(group); !it.end(); ++it) {
                aabbMin.setMin(it->position());
                aabbMax.setMax(it->position());
            }
            aabbMin -= diag_v;
            aabbMax += diag_v;
        }
    }

    void SparkQuadRenderer::Render2D(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        scaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2D(particle, render_buffer);
    }

    void SparkQuadRenderer::Render2DRot(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        rotateAndScaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2D(particle, render_buffer);
    }

    void SparkQuadRenderer::Render2DAtlas(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        scaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2DAtlas(particle, render_buffer);
    }

    void SparkQuadRenderer::Render2DAtlasRot(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        rotateAndScaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2DAtlas(particle, render_buffer);
    }

    auto SparkQuadRenderer::GetDrawWidth() const -> int32_t
    {
        FO_STACK_TRACE_ENTRY();

        return _drawWidth;
    }

    auto SparkQuadRenderer::GetDrawHeight() const -> int32_t
    {
        FO_STACK_TRACE_ENTRY();

        return _drawHeight;
    }

    void SparkQuadRenderer::SetDrawSize(int32_t width, int32_t height)
    {
        FO_STACK_TRACE_ENTRY();

        _drawWidth = width;
        _drawHeight = height;
    }

    auto SparkQuadRenderer::GetDrawInScene() const -> bool
    {
        FO_STACK_TRACE_ENTRY();

        return _drawInScene;
    }

    void SparkQuadRenderer::SetDrawInScene(bool draw_in_scene)
    {
        FO_STACK_TRACE_ENTRY();

        _drawInScene = draw_in_scene;
    }

    auto SparkQuadRenderer::GetEffectName() const -> string_view
    {
        FO_STACK_TRACE_ENTRY();

        return _effectName;
    }

    void SparkQuadRenderer::SetEffectName(string_view effect_name)
    {
        FO_STACK_TRACE_ENTRY();

        _effectName = string(effect_name);

        if (!_effectName.empty() && _runtime) {
            _effect = _runtime->_impl->Services.EffectMngr->LoadEffect(EffectUsage::QuadSprite, _effectName);
        }
        else {
            _effect = nullptr;
        }
    }

    auto SparkQuadRenderer::GetTextureName() const -> string_view
    {
        FO_STACK_TRACE_ENTRY();

        return _textureName;
    }

    void SparkQuadRenderer::SetTextureName(string_view tex_name)
    {
        FO_STACK_TRACE_ENTRY();

        _textureName = string(tex_name);

        if (!_textureName.empty() && _runtime) {
            const string tex_path = strex(_path).extract_dir().combine_path(_textureName);
            auto&& [tex, tex_data] = _runtime->_impl->Services.TextureLoader(tex_path);
            _texture = tex;
            _textureAtlasOffset = tex_data;
        }
        else {
            _texture = nullptr;
        }
    }

    void SparkQuadRenderer::innerImport(const IO::Descriptor& descriptor)
    {
        FO_STACK_TRACE_ENTRY();

        Renderer::innerImport(descriptor);

        _drawWidth = 0;
        _drawHeight = 0;
        _drawInScene = false;

        _effectName = "";
        _textureName = "";

        scaleX = 1.0f;
        scaleY = 1.0f;
        textureAtlasNbX = 1;
        textureAtlasNbY = 1;
        textureAtlasW = 1.0f;
        textureAtlasH = 1.0f;

        lookOrientation = LOOK_CAMERA_PLANE;
        upOrientation = UP_CAMERA;
        lockedAxis = LOCK_UP;
        lookVector.set(0.0f, 0.0f, 1.0f);
        upVector.set(0.0f, 1.0f, 0.0f);

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("draw size"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            const auto tmpSize = attrib->getValues<int32_t>();

            switch (tmpSize.size()) {
            case 1:
                _drawWidth = tmpSize[0];
                break;
            case 2:
                _drawWidth = tmpSize[0];
                _drawHeight = tmpSize[1];
                break;
            default:
                break;
            }
        }

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("draw in scene"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            _drawInScene = attrib->getValue<bool>();
        }

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("effect"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            SetEffectName(string(attrib->getValue<std::string>()));
        }

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("texture"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            SetTextureName(string(attrib->getValue<std::string>()));
        }

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("scale"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            const auto tmpScale = attrib->getValues<float32_t>();

            switch (tmpScale.size()) {
            case 1:
                setScale(tmpScale[0], scaleY);
                break;
            case 2:
                setScale(tmpScale[0], tmpScale[1]);
                break;
            default:
                break;
            }
        }

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("atlas dimensions"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            const auto tmpAtlasDimensions = attrib->getValues<uint32_t>();

            switch (tmpAtlasDimensions.size()) {
            case 1:
                setAtlasDimensions(tmpAtlasDimensions[0], textureAtlasNbY);
                break;
            case 2:
                setAtlasDimensions(tmpAtlasDimensions[0], tmpAtlasDimensions[1]);
                break;
            default:
                break;
            }
        }

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("look orientation"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            const auto lookOrient = attrib->getValue<std::string>();

            if (lookOrient == "LOOK_CAMERA_PLANE") {
                lookOrientation = LOOK_CAMERA_PLANE;
            }
            else if (lookOrient == "LOOK_CAMERA_POINT") {
                lookOrientation = LOOK_CAMERA_POINT;
            }
            else if (lookOrient == "LOOK_AXIS") {
                lookOrientation = LOOK_AXIS;
            }
            else if (lookOrient == "LOOK_POINT") {
                lookOrientation = LOOK_POINT;
            }
        }

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("up orientation"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            const auto upOrient = attrib->getValue<std::string>();

            if (upOrient == "UP_CAMERA") {
                upOrientation = UP_CAMERA;
            }
            else if (upOrient == "UP_DIRECTION") {
                upOrientation = UP_DIRECTION;
            }
            else if (upOrient == "UP_AXIS") {
                upOrientation = UP_AXIS;
            }
            else if (upOrient == "UP_POINT") {
                upOrientation = UP_POINT;
            }
        }

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("locked axis"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            const auto lockAx = attrib->getValue<std::string>();

            if (lockAx == "LOCK_LOOK") {
                lockedAxis = LOCK_LOOK;
            }
            else if (lockAx == "LOCK_UP") {
                lockedAxis = LOCK_UP;
            }
        }

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("locked look vector"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            lookVector = attrib->getValue<Vector3D>();
        }

        if (nptr<const IO::Attribute> nullable_attrib = descriptor.getAttributeWithValue("locked up vector"); nullable_attrib) {
            auto attrib = nullable_attrib.as_ptr();
            upVector = attrib->getValue<Vector3D>();
        }
    }

    void SparkQuadRenderer::innerExport(IO::Descriptor& descriptor) const
    {
        FO_STACK_TRACE_ENTRY();

        Renderer::innerExport(descriptor);

        if (_drawWidth != 0 || _drawHeight != 0) {
            const std::vector tmpSize = {_drawWidth, _drawHeight};
            descriptor.getAttribute("draw size")->setValues(tmpSize.data(), 2);
        }

        if (_drawInScene) {
            descriptor.getAttribute("draw in scene")->setValue(_drawInScene);
        }

        descriptor.getAttribute("effect")->setValue(std::string(_effectName));

        descriptor.getAttribute("texture")->setValue(std::string(_textureName));

        const std::vector tmpScale = {scaleX, scaleY};
        descriptor.getAttribute("scale")->setValues(tmpScale.data(), 2);

        const std::vector tmpAtlasDimensions = {numeric_cast<uint32_t>(textureAtlasNbX), numeric_cast<uint32_t>(textureAtlasNbY)};
        descriptor.getAttribute("atlas dimensions")->setValues(tmpAtlasDimensions.data(), 2);

        if (lookOrientation == LOOK_CAMERA_PLANE) {
            descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_CAMERA_PLANE"));
        }
        else if (lookOrientation == LOOK_CAMERA_POINT) {
            descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_CAMERA_POINT"));
        }
        else if (lookOrientation == LOOK_AXIS) {
            descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_AXIS"));
        }
        else if (lookOrientation == LOOK_POINT) {
            descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_POINT"));
        }

        if (upOrientation == UP_CAMERA) {
            descriptor.getAttribute("up orientation")->setValue(std::string("UP_CAMERA"));
        }
        else if (upOrientation == UP_DIRECTION) {
            descriptor.getAttribute("up orientation")->setValue(std::string("UP_DIRECTION"));
        }
        else if (upOrientation == UP_AXIS) {
            descriptor.getAttribute("up orientation")->setValue(std::string("UP_AXIS"));
        }
        else if (upOrientation == UP_POINT) {
            descriptor.getAttribute("up orientation")->setValue(std::string("UP_POINT"));
        }

        if (lockedAxis == LOCK_LOOK) {
            descriptor.getAttribute("locked axis")->setValue(std::string("LOCK_LOOK"));
        }
        else if (lockedAxis == LOCK_UP) {
            descriptor.getAttribute("locked axis")->setValue(std::string("LOCK_UP"));
        }

        descriptor.getAttribute("locked look vector")->setValue(lookVector);

        descriptor.getAttribute("locked up vector")->setValue(upVector);
    }

}

#endif
