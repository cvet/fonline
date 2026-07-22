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
#include "Rendering.h"

FO_DISABLE_WARNINGS_PUSH()
#include "SPARK.h"
FO_DISABLE_WARNINGS_POP()

namespace SPK::FO
{
    FO_USING_NAMESPACE();

    class SparkRenderBuffer final : public RenderBuffer
    {
    public:
        SparkRenderBuffer(size_t vertices, ptr<FO_NAMESPACE IAppRender> render);

        void PositionAtStart();
        void SetNextVertex(const Vector3D& pos, const Color& color);
        void SetNextTexCoord(float32_t tu, float32_t tv);
        void Render(size_t vertices, ptr<RenderEffect> effect) const;

    private:
        mutable unique_ptr<RenderDrawBuffer> _renderBuf;
        nptr<FO_NAMESPACE IAppRender> _render {};
        size_t _curVertexIndex {};
        size_t _curTexCoordIndex {};
    };

    class SparkQuadRenderer final : public Renderer, public QuadRenderBehavior, public Oriented3DRenderBehavior
    {
        SPK_IMPLEMENT_OBJECT(SparkQuadRenderer)

        SPK_START_DESCRIPTION
        SPK_PARENT_ATTRIBUTES(Renderer)
        SPK_ATTRIBUTE("draw size", ATTRIBUTE_TYPE_INT32S)
        SPK_ATTRIBUTE("draw in scene", ATTRIBUTE_TYPE_BOOL)
        SPK_ATTRIBUTE("effect", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("blend mode", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("texture", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("scale", ATTRIBUTE_TYPE_FLOATS)
        SPK_ATTRIBUTE("atlas dimensions", ATTRIBUTE_TYPE_UINT32S)
        SPK_ATTRIBUTE("look orientation", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("up orientation", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("locked axis", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("locked look vector", ATTRIBUTE_TYPE_VECTOR)
        SPK_ATTRIBUTE("locked up vector", ATTRIBUTE_TYPE_VECTOR)
        SPK_END_DESCRIPTION

    public:
        static auto Create() -> Ref<SparkQuadRenderer>;
        ~SparkQuadRenderer() override = default;

        [[nodiscard]] auto Setup(string_view path, ptr<FO_NAMESPACE SparkParticleRuntimeBackend> runtime) -> bool;

        auto GetDrawWidth() const -> int32_t;
        auto GetDrawHeight() const -> int32_t;
        void SetDrawSize(int32_t width, int32_t height);

        auto GetDrawInScene() const -> bool;
        void SetDrawInScene(bool draw_in_scene);

        auto GetEffectName() const -> string_view;
        void SetEffectName(string_view effect_name);

        auto GetTextureName() const -> string_view;
        void SetTextureName(string_view tex_name);

    private:
        SparkQuadRenderer() :
            Renderer(false)
        {
        }
        explicit SparkQuadRenderer(bool needs_dataset);
        SparkQuadRenderer(const SparkQuadRenderer& renderer) = default;

        string _path {};
        nptr<FO_NAMESPACE SparkParticleRuntimeBackend> _runtime {};

        int32_t _drawWidth {};
        int32_t _drawHeight {};
        bool _drawInScene {};

        string _effectName {};
        string _textureName {};

        void AddPosAndColor(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const;
        void AddTexture2D(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const;
        void AddTexture2DAtlas(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const;

        void Render2D(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const;
        void Render2DRot(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const;
        void Render2DAtlas(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const;
        void Render2DAtlasRot(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const;

        mutable nptr<RenderEffect> _effect {};
        nptr<RenderTexture> _texture {};
        frect32 _textureAtlasOffset {};

        mutable mat44 _modelView {};
        mutable mat44 _invModelView {};

        using RenderParticleFunc = void (SparkQuadRenderer::*)(const Particle&, nptr<SparkRenderBuffer> render_buffer) const;
        mutable RenderParticleFunc _renderParticle {};

    private:
        void innerImport(const IO::Descriptor& descriptor) override;
        void innerExport(IO::Descriptor& descriptor) const override;
        RenderBuffer* attachRenderBuffer(const Group& group) const override;
        void render(const Group& group, const DataSet* data_set, RenderBuffer* render_buffer) const override;
        void computeAABB(Vector3D& aabb_min, Vector3D& aabb_max, const Group& group, const DataSet* data_set) const override;
    };
}

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
    SPK::SPKContext Context {};
    unordered_map<string, SPK::Ref<SPK::System>> BaseSystems {};
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

        RandomSeed = std::uniform_int_distribution<uint32_t> {}(RandomGenerator);
        RecreateRuntimeSystem();
    }

    void RecreateRuntimeSystem();

    ptr<SparkParticleRuntimeBackend> Runtime;
    string Path;
    SPK::Ref<SPK::System> RuntimeSystem {};
    SPK::Ref<SPK::System> BaseSystem {};
    mat44 ViewProjectionMatrix {};
    mat44 ViewMatrix {};
    std::mt19937 RandomGenerator {MakeSeededRandomGenerator()};
    uint32_t RandomSeed {};
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

    SPK::FO::EnsureSparkParticleObjectsRegistered(_impl->Context);
}

SparkParticleRuntimeBackend::~SparkParticleRuntimeBackend()
{
    FO_STACK_TRACE_ENTRY();
}

auto SparkParticleRuntimeBackend::GetExtensions() const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    return {"spk"};
}

void SparkParticleRuntimeBackend::InvalidateResource(string_view path)
{
    FO_STACK_TRACE_ENTRY();

    if (strex(path).get_file_extension() == "spk") {
        _impl->BaseSystems.erase(string {path});
    }
    else {
        _impl->BaseSystems.clear();
    }
}

auto SparkParticleRuntimeBackend::Create(string_view path) -> unique_nptr<ParticleRuntimeSystem>
{
    FO_STACK_TRACE_ENTRY();

    if (strex(path).get_file_extension() != "spk") {
        return nullptr;
    }

    SPK::Ref<SPK::System> base_system;

    if (const auto it = _impl->BaseSystems.find(path); it == _impl->BaseSystems.end()) {
        if (const auto file = _impl->Services.Resources->ReadFile(path)) {
            const_span<uint8_t> file_data = file.GetDataSpan();
            base_system = _impl->Context.getIOManager().loadFromBuffer("spk", ptr<const uint8_t> {file_data.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(file_data.size()));
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

    SPK::RandomSeedScope random_seed_scope {_impl->Runtime->_impl->Context, _impl->RandomSeed};
    const uint32_t init_time_range = numeric_cast<uint32_t>(max_lifetime_ms) + 1U;
    const int32_t init_time_ms = numeric_cast<int32_t>(SPK_RANDOM(_impl->Runtime->_impl->Context, 0U, init_time_range));
    const float32_t init_time = numeric_cast<float32_t>(init_time_ms) / 1000.0f;

    for (float32_t delta_seconds = 0.0f; delta_seconds < init_time; delta_seconds += SPARK_PREWARM_STEP) {
        _impl->RuntimeSystem->updateParticles(std::min(SPARK_PREWARM_STEP, init_time - delta_seconds));
    }

    return init_time;
}

void SparkParticleRuntimeSystem::Respawn(optional<int32_t> seed)
{
    FO_STACK_TRACE_ENTRY();

    _impl->RandomSeed = seed ? std::bit_cast<uint32_t>(*seed) : std::uniform_int_distribution<uint32_t> {}(_impl->RandomGenerator);
    _impl->RecreateRuntimeSystem();
}

void SparkParticleRuntimeSystem::Update(float32_t delta_seconds)
{
    FO_STACK_TRACE_ENTRY();

    if (_impl->RuntimeSystem->isActive() && delta_seconds > 0.0f) {
        SPK::RandomSeedScope random_seed_scope {_impl->Runtime->_impl->Context, _impl->RandomSeed};
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

    SPK::RandomSeedScope random_seed_scope {Runtime->_impl->Context, RandomSeed};
    SetupSparkSystemRenderers(Path, BaseSystem, Runtime);
    RuntimeSystem = SPK::SPKObject::copy(BaseSystem);
    RuntimeSystem->initialize();
}

FO_END_NAMESPACE

namespace SPK::FO
{
    void EnsureSparkParticleObjectsRegistered(SPKContext& context)
    {
        FO_STACK_TRACE_ENTRY();

        auto& io_mngr = context.getIOManager();
        io_mngr.ensureObjectRegistered<SparkQuadRenderer>();
    }

    auto IsSparkParticleObjectRegistered(const SPKContext& context) -> bool
    {
        FO_STACK_TRACE_ENTRY();

        return context.getIOManager().isObjectRegistered<SparkQuadRenderer>();
    }

    auto IsSparkQuadRenderer(const Renderer& renderer) -> bool
    {
        FO_STACK_TRACE_ENTRY();

        return dynamic_cast<const SparkQuadRenderer*>(&renderer) != nullptr;
    }

    auto CreateSparkQuadRenderer() -> Ref<Renderer>
    {
        FO_STACK_TRACE_ENTRY();

        return SparkQuadRenderer::Create();
    }

    auto GetSparkQuadRendererData(const Renderer& renderer) -> SparkQuadRendererData
    {
        FO_STACK_TRACE_ENTRY();

        const nptr<const SparkQuadRenderer> spark_renderer {dynamic_cast<const SparkQuadRenderer*>(&renderer)};
        FO_VERIFY_AND_THROW(spark_renderer, "SPARK renderer has an unexpected type");

        SparkQuadRendererData data;
        data.Active = spark_renderer->isActive();
        data.AlphaTest = spark_renderer->isRenderingOptionEnabled(RENDERING_OPTION_ALPHA_TEST);
        data.DepthWrite = spark_renderer->isRenderingOptionEnabled(RENDERING_OPTION_DEPTH_WRITE);
        data.AlphaTestThreshold = spark_renderer->getAlphaTestThreshold();
        data.DrawWidth = spark_renderer->GetDrawWidth();
        data.DrawHeight = spark_renderer->GetDrawHeight();
        data.DrawInScene = spark_renderer->GetDrawInScene();
        data.EffectName = spark_renderer->GetEffectName();
        data.TextureName = spark_renderer->GetTextureName();
        data.ScaleX = spark_renderer->getScaleX();
        data.ScaleY = spark_renderer->getScaleY();
        data.AtlasDimensionX = numeric_cast<int32_t>(spark_renderer->getAtlasDimensionX());
        data.AtlasDimensionY = numeric_cast<int32_t>(spark_renderer->getAtlasDimensionY());
        data.LookOrientation = static_cast<int32_t>(spark_renderer->getLookOrientation());
        data.UpOrientation = static_cast<int32_t>(spark_renderer->getUpOrientation());
        data.LockedAxis = static_cast<int32_t>(spark_renderer->getLockedAxis());
        data.LookVector = {spark_renderer->lookVector.x, spark_renderer->lookVector.y, spark_renderer->lookVector.z};
        data.UpVector = {spark_renderer->upVector.x, spark_renderer->upVector.y, spark_renderer->upVector.z};
        return data;
    }

    void SetSparkQuadRendererData(Renderer& renderer, const SparkQuadRendererData& data)
    {
        FO_STACK_TRACE_ENTRY();

        nptr<SparkQuadRenderer> spark_renderer {dynamic_cast<SparkQuadRenderer*>(&renderer)};
        FO_VERIFY_AND_THROW(spark_renderer, "SPARK renderer has an unexpected type");

        spark_renderer->setActive(data.Active);
        spark_renderer->enableRenderingOption(RENDERING_OPTION_ALPHA_TEST, data.AlphaTest);
        spark_renderer->enableRenderingOption(RENDERING_OPTION_DEPTH_WRITE, data.DepthWrite);
        spark_renderer->setAlphaTestThreshold(data.AlphaTestThreshold);
        spark_renderer->SetDrawSize(data.DrawWidth, data.DrawHeight);
        spark_renderer->SetDrawInScene(data.DrawInScene);
        spark_renderer->SetEffectName(data.EffectName);
        spark_renderer->SetTextureName(data.TextureName);
        spark_renderer->setScale(data.ScaleX, data.ScaleY);
        spark_renderer->setAtlasDimensions(numeric_cast<size_t>(data.AtlasDimensionX), numeric_cast<size_t>(data.AtlasDimensionY));
        spark_renderer->setOrientation(static_cast<LookOrientation>(data.LookOrientation), static_cast<UpOrientation>(data.UpOrientation), static_cast<LockedAxis>(data.LockedAxis));
        spark_renderer->lookVector = Vector3D(data.LookVector[0], data.LookVector[1], data.LookVector[2]);
        spark_renderer->upVector = Vector3D(data.UpVector[0], data.UpVector[1], data.UpVector[2]);
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

    void SparkQuadRenderer::AddPosAndColor(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        render_buffer->SetNextVertex(particle.position() + quadSide() + quadUp(), particle.getColor()); // top right vertex
        render_buffer->SetNextVertex(particle.position() - quadSide() + quadUp(), particle.getColor()); // top left vertex
        render_buffer->SetNextVertex(particle.position() - quadSide() - quadUp(), particle.getColor()); // bottom left vertex
        render_buffer->SetNextVertex(particle.position() + quadSide() - quadUp(), particle.getColor()); // bottom right vertex
    }

    void SparkQuadRenderer::AddTexture2D(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(particle);

        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + 1.0f * _textureAtlasOffset.width, _textureAtlasOffset.y + 0.0f * _textureAtlasOffset.height);
        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + 0.0f * _textureAtlasOffset.width, _textureAtlasOffset.y + 0.0f * _textureAtlasOffset.height);
        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + 0.0f * _textureAtlasOffset.width, _textureAtlasOffset.y + 1.0f * _textureAtlasOffset.height);
        render_buffer->SetNextTexCoord(_textureAtlasOffset.x + 1.0f * _textureAtlasOffset.width, _textureAtlasOffset.y + 1.0f * _textureAtlasOffset.height);
    }

    void SparkQuadRenderer::AddTexture2DAtlas(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const
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
        nptr<SparkRenderBuffer> spark_render_buffer = render_buffer.dyn_cast<SparkRenderBuffer>();
        FO_VERIFY_AND_THROW(spark_render_buffer, "Render buffer is not a spark render buffer");

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

    void SparkQuadRenderer::Render2D(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        scaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2D(particle, render_buffer);
    }

    void SparkQuadRenderer::Render2DRot(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        rotateAndScaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2D(particle, render_buffer);
    }

    void SparkQuadRenderer::Render2DAtlas(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const
    {
        FO_STACK_TRACE_ENTRY();

        scaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2DAtlas(particle, render_buffer);
    }

    void SparkQuadRenderer::Render2DAtlasRot(const Particle& particle, nptr<SparkRenderBuffer> render_buffer) const
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
