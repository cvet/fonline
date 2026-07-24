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

#include "catch_amalgamated.hpp"

#include "Application.h"
#include "EffectManager.h"
#include "EffekseerExtension.h"
#include "FileSystem.h"
#include "Rendering.h"
#include "Test_BakerHelpers.h"
#include "Test_ParticleFixtures.h"
#include "VisualParticles.h"

FO_BEGIN_NAMESPACE

#if FO_EFFEKSEER_PARTICLES

static constexpr string_view EffekseerFixturePath = "Particles/Effekseer01/Simple_Sprite_FixedYAxis.efk";
static constexpr string_view EffekseerZSortFixturePath = "Particles/EffekseerTests/ZSort_Sprite.efk";
static constexpr string_view EffekseerRingFixturePath = "Particles/EffekseerTests/Modern_Ring.efk";
static constexpr frect32 EffekseerFixtureAtlasRect {0.125f, 0.25f, 0.5f, 0.375f};

struct CapturedEffekseerDraw final
{
    string EffectName {};
    vector<Vertex2D> Vertices {};
    vector<vindex_t> Indices {};
    RenderPrimitiveType PrimitiveType {};
    bool HasMainTexture {};
    bool HasProjection {};
};

struct EffekseerDrawCapture final
{
    vector<CapturedEffekseerDraw> Draws {};
};

class CapturingRenderEffect final : public RenderEffect
{
public:
    CapturingRenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader, shared_ptr<EffekseerDrawCapture> capture);

    void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override;

private:
    shared_ptr<EffekseerDrawCapture> _capture;
};

class CapturingAppRender final : public IAppRender
{
public:
    explicit CapturingAppRender(ptr<GlobalSettings> settings);

    [[nodiscard]] auto GetRenderTarget() -> nptr<RenderTexture> override;
    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
    [[nodiscard]] auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 override;
    [[nodiscard]] auto IsRenderTargetFlipped() const -> bool override;
    [[nodiscard]] auto GetProjMatrix() const -> mat44 override;

    void SetRenderTarget(nptr<RenderTexture> tex) override;
    void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept override;
    void ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;

    void ClearDraws();
    [[nodiscard]] auto GetDraws() const -> const vector<CapturedEffekseerDraw>&;

private:
    Null_Renderer _renderer;
    shared_ptr<EffekseerDrawCapture> _capture;
    nptr<RenderTexture> _renderTarget {};
};

class EffekseerRuntimeTestSettings final : public GlobalSettings
{
public:
    EffekseerRuntimeTestSettings();
};

class EffekseerRuntimeTestRig final
{
public:
    explicit EffekseerRuntimeTestRig(bool provide_texture = true);
    EffekseerRuntimeTestRig(string_view effect_path, vector<uint8_t> effect_data, bool provide_texture = true);

    [[nodiscard]] auto CreateSystem() -> unique_ptr<ParticleRuntimeSystem>;
    [[nodiscard]] auto CreateManagedSystem() -> optional<ParticleSystem>;
    [[nodiscard]] auto CanCreateSystem() -> bool;
    [[nodiscard]] auto GetDraws() const -> const vector<CapturedEffekseerDraw>&;
    [[nodiscard]] auto GetTextureRequests() const -> const vector<string>&;
    void ClearDraws();

private:
    EffekseerRuntimeTestSettings _settings {};
    string _effectPath;
    FileSystem _resources;
    unique_ptr<CapturingAppRender> _render;
    unique_ptr<EffectManager> _effectManager;
    unique_ptr<RenderTexture> _texture;
    vector<string> _textureRequests {};
    bool _provideTexture {};
    unique_ptr<GameTimer> _gameTimer;
    unique_ptr<ParticleManager> _particleManager;
    unique_ptr<EffekseerParticleRuntimeBackend> _backend;
};

EffekseerRuntimeTestSettings::EffekseerRuntimeTestSettings() :
    GlobalSettings {false}
{
    FO_STACK_TRACE_ENTRY();

    ApplyDefaultSettings();
    BakerTests::ApplySelfContainedClientSettings(*this);
}

CapturingRenderEffect::CapturingRenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader, shared_ptr<EffekseerDrawCapture> capture) :
    RenderEffect(usage, name, loader),
    _capture {std::move(capture)}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_capture, "Capturing render effect requires capture storage");
}

void CapturingRenderEffect::DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(custom_tex);
    FO_VERIFY_AND_THROW(start_index <= dbuf->IndCount, "Captured draw starts outside the index buffer", start_index, dbuf->IndCount);

    const size_t draw_index_count = indices_to_draw.value_or(dbuf->IndCount - start_index);
    FO_VERIFY_AND_THROW(draw_index_count <= dbuf->IndCount - start_index, "Captured draw exceeds the index buffer", start_index, draw_index_count, dbuf->IndCount);
    FO_VERIFY_AND_THROW(dbuf->VertCount <= dbuf->Vertices.size(), "Captured draw exceeds the vertex buffer", dbuf->VertCount, dbuf->Vertices.size());
    FO_VERIFY_AND_THROW(dbuf->IndCount <= dbuf->Indices.size(), "Captured draw exceeds the allocated index buffer", dbuf->IndCount, dbuf->Indices.size());

    const_span<Vertex2D> vertices {dbuf->Vertices.data(), dbuf->VertCount};
    const_span<vindex_t> indices {dbuf->Indices.data() + start_index, draw_index_count};
    vector<Vertex2D> captured_vertices(vertices.begin(), vertices.end());
    vector<vindex_t> captured_indices(indices.begin(), indices.end());
    _capture->Draws.emplace_back(CapturedEffekseerDraw {
        .EffectName = _name,
        .Vertices = std::move(captured_vertices),
        .Indices = std::move(captured_indices),
        .PrimitiveType = dbuf->PrimType,
        .HasMainTexture = MainTex != nullptr,
        .HasProjection = ProjBuf.has_value(),
    });
}

CapturingAppRender::CapturingAppRender(ptr<GlobalSettings> settings) :
    _capture {SafeAlloc::MakeShared<EffekseerDrawCapture>()}
{
    FO_STACK_TRACE_ENTRY();

    _renderer.Init(*settings, nullptr);
}

auto CapturingAppRender::GetRenderTarget() -> nptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    return _renderTarget;
}

auto CapturingAppRender::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    return _renderer.CreateTexture(size, linear_filtered, with_depth);
}

auto CapturingAppRender::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    return _renderer.CreateDrawBuffer(is_static);
}

auto CapturingAppRender::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<CapturingRenderEffect>(usage, name, loader, _capture);
}

auto CapturingAppRender::CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    return _renderer.CreateOrthoMatrix(left, right, bottom, top, nearp, farp);
}

auto CapturingAppRender::IsRenderTargetFlipped() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _renderer.IsRenderTargetFlipped();
}

auto CapturingAppRender::GetProjMatrix() const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    return _renderer.GetProjMatrix();
}

void CapturingAppRender::SetRenderTarget(nptr<RenderTexture> tex)
{
    FO_STACK_TRACE_ENTRY();

    _renderTarget = tex;
    _renderer.SetRenderTarget(tex);
}

void CapturingAppRender::SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _renderer.SetOrthoDepthRange(nearp, farp);
}

void CapturingAppRender::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    _renderer.ClearRenderTarget(color, depth, stencil);
}

void CapturingAppRender::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    _renderer.EnableScissor(rect);
}

void CapturingAppRender::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    _renderer.DisableScissor();
}

void CapturingAppRender::ClearDraws()
{
    FO_STACK_TRACE_ENTRY();

    _capture->Draws.clear();
}

auto CapturingAppRender::GetDraws() const -> const vector<CapturedEffekseerDraw>&
{
    FO_STACK_TRACE_ENTRY();

    return _capture->Draws;
}

static void AddEffekseerRuntimeTestResources(BakerTests::MemoryDataSource& source, string_view effect_path, vector<uint8_t> effect_data)
{
    FO_STACK_TRACE_ENTRY();

    static constexpr string_view effect_config = "[Effect]\nPasses = 1\n";
    static constexpr string_view effect_info = "[EffectInfo]\nMainTex = 0\nProjBuf = 1\n";

    // A baked .efk always carries the mandatory bounds trailer, and the runtime throws on a binary missing one. The
    // cooked fixtures are the raw Effekseer payload, so append a representative trailer here (its box does not affect
    // the geometry these tests assert).
    AppendEffekseerBoundsTrailer(effect_data, vec3 {-1.0f, -1.0f, -1.0f}, vec3 {1.0f, 1.0f, 1.0f});
    source.AddFile(effect_path, std::move(effect_data));
    source.AddFile("Effects/Particles_ColorMul.fofx", effect_config);
    source.AddFile("Effects/Particles_ColorMul.fofx-1-info", effect_info);
    source.AddFile("Effects/Particles_ColorAdd.fofx", effect_config);
    source.AddFile("Effects/Particles_ColorAdd.fofx-1-info", effect_info);
}

static auto MakeEffekseerRuntimeTestResources(string_view effect_path, vector<uint8_t> effect_data) -> FileSystem
{
    FO_STACK_TRACE_ENTRY();

    auto source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("EffekseerRuntimeTests");
    AddEffekseerRuntimeTestResources(*source, effect_path, std::move(effect_data));

    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

EffekseerRuntimeTestRig::EffekseerRuntimeTestRig(bool provide_texture) :
    EffekseerRuntimeTestRig {EffekseerFixturePath, ParticleTests::MakeSimpleSpriteFixedYAxisEffect(), provide_texture}
{
    FO_STACK_TRACE_ENTRY();
}

EffekseerRuntimeTestRig::EffekseerRuntimeTestRig(string_view effect_path, vector<uint8_t> effect_data, bool provide_texture) :
    _effectPath {effect_path},
    _resources {MakeEffekseerRuntimeTestResources(effect_path, std::move(effect_data))},
    _render {SafeAlloc::MakeUnique<CapturingAppRender>(&_settings)},
    _effectManager {SafeAlloc::MakeUnique<EffectManager>(&_settings, &_resources, _render.as_ptr())},
    _texture {_render->CreateTexture({8, 8}, true, false)},
    _provideTexture {provide_texture},
    _gameTimer {SafeAlloc::MakeUnique<GameTimer>(&_settings)},
    _particleManager {SafeAlloc::MakeUnique<ParticleManager>(&_settings, _effectManager.as_ptr(), _render.as_ptr(), &_resources, _gameTimer.as_ptr(),
        [this](string_view path) -> pair<nptr<RenderTexture>, frect32> {
            _textureRequests.emplace_back(path);

            if (!_provideTexture) {
                return {nullptr, {}};
            }

            return {_texture.as_nptr(), EffekseerFixtureAtlasRect};
        })},
    _backend {SafeAlloc::MakeUnique<EffekseerParticleRuntimeBackend>(ParticleRuntimeServices {
        .EffectMngr = _effectManager.as_ptr(),
        .Render = _render.as_ptr(),
        .Resources = &_resources,
        .TextureLoader = [this](string_view path) -> pair<nptr<RenderTexture>, frect32> {
            _textureRequests.emplace_back(path);

            if (!_provideTexture) {
                return {nullptr, {}};
            }

            return {_texture.as_nptr(), EffekseerFixtureAtlasRect};
        },
    })}
{
    FO_STACK_TRACE_ENTRY();
}

auto EffekseerRuntimeTestRig::CreateSystem() -> unique_ptr<ParticleRuntimeSystem>
{
    FO_STACK_TRACE_ENTRY();

    auto system = _backend->Create(_effectPath);
    FO_VERIFY_AND_THROW(system, "Effekseer runtime test fixture failed to create");
    return system.take_not_null();
}

auto EffekseerRuntimeTestRig::CreateManagedSystem() -> optional<ParticleSystem>
{
    FO_STACK_TRACE_ENTRY();

    return _particleManager->CreateParticle(_effectPath);
}

auto EffekseerRuntimeTestRig::CanCreateSystem() -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _backend->Create(_effectPath) != nullptr;
}

auto EffekseerRuntimeTestRig::GetDraws() const -> const vector<CapturedEffekseerDraw>&
{
    FO_STACK_TRACE_ENTRY();

    return _render->GetDraws();
}

auto EffekseerRuntimeTestRig::GetTextureRequests() const -> const vector<string>&
{
    FO_STACK_TRACE_ENTRY();

    return _textureRequests;
}

void EffekseerRuntimeTestRig::ClearDraws()
{
    FO_STACK_TRACE_ENTRY();

    _render->ClearDraws();
}

static auto MakeEffekseerIdentitySetup() -> ParticleRuntimeSetup
{
    FO_STACK_TRACE_ENTRY();

    return ParticleRuntimeSetup {
        .Projection = mat44 {1.0f},
        .World = mat44 {1.0f},
        .Scale = 1.0f,
    };
}

static auto DrawEffekseerFixture(EffekseerRuntimeTestRig& rig, int32_t seed, int32_t frame_count = 1) -> vector<CapturedEffekseerDraw>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(frame_count > 0, "Effekseer runtime test requires a positive frame count", frame_count);
    unique_ptr<ParticleRuntimeSystem> system = rig.CreateSystem();

    system->Setup(MakeEffekseerIdentitySetup());
    system->Respawn(seed);
    REQUIRE(system->IsActive());

    for (int32_t frame = 0; frame < frame_count; frame++) {
        system->Update(1.0f / 60.0f);
    }

    rig.ClearDraws();
    system->Draw();
    return rig.GetDraws();
}

static void CheckEffekseerDrawsEqual(const vector<CapturedEffekseerDraw>& left, const vector<CapturedEffekseerDraw>& right)
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(left.size() == right.size());

    for (size_t draw_index = 0; draw_index < left.size(); draw_index++) {
        const CapturedEffekseerDraw& left_draw = left[draw_index];
        const CapturedEffekseerDraw& right_draw = right[draw_index];
        CAPTURE(draw_index);
        CHECK(left_draw.EffectName == right_draw.EffectName);
        CHECK(left_draw.Indices == right_draw.Indices);
        CHECK(left_draw.PrimitiveType == right_draw.PrimitiveType);
        CHECK(left_draw.HasMainTexture == right_draw.HasMainTexture);
        CHECK(left_draw.HasProjection == right_draw.HasProjection);
        REQUIRE(left_draw.Vertices.size() == right_draw.Vertices.size());

        for (size_t vertex_index = 0; vertex_index < left_draw.Vertices.size(); vertex_index++) {
            const Vertex2D& left_vertex = left_draw.Vertices[vertex_index];
            const Vertex2D& right_vertex = right_draw.Vertices[vertex_index];
            CAPTURE(vertex_index);
            CHECK(left_vertex.PosX == right_vertex.PosX);
            CHECK(left_vertex.PosY == right_vertex.PosY);
            CHECK(left_vertex.PosZ == right_vertex.PosZ);
            CHECK(left_vertex.Color == right_vertex.Color);
            CHECK(left_vertex.TexU == right_vertex.TexU);
            CHECK(left_vertex.TexV == right_vertex.TexV);
            CHECK(left_vertex.EggFlags[0] == right_vertex.EggFlags[0]);
            CHECK(left_vertex.EggFlags[1] == right_vertex.EggFlags[1]);
        }
    }
}

static void CheckEffekseerFixtureGeometry(const vector<CapturedEffekseerDraw>& draws)
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(draws.size() == 1);
    const CapturedEffekseerDraw& draw = draws.front();
    CHECK(draw.EffectName == "Effects/Particles_ColorAdd.fofx");
    CHECK(draw.PrimitiveType == RenderPrimitiveType::TriangleList);
    CHECK(draw.HasMainTexture);
    CHECK(draw.HasProjection);
    REQUIRE(draw.Vertices.size() == 4);
    CHECK(draw.Indices == vector<vindex_t> {0, 1, 2, 2, 1, 3});

    const float32_t expected_u[4] = {0.125f, 0.625f, 0.125f, 0.625f};
    const float32_t expected_v[4] = {0.625f, 0.625f, 0.25f, 0.25f};

    for (size_t vertex_index = 0; vertex_index < draw.Vertices.size(); vertex_index++) {
        const Vertex2D& vertex = draw.Vertices[vertex_index];
        CAPTURE(vertex_index);
        CHECK(std::isfinite(vertex.PosX));
        CHECK(std::isfinite(vertex.PosY));
        CHECK(std::isfinite(vertex.PosZ));
        CHECK(vertex.TexU == Catch::Approx(expected_u[vertex_index]));
        CHECK(vertex.TexV == Catch::Approx(expected_v[vertex_index]));
        CHECK(vertex.EggFlags[0] == 0.0f);
        CHECK(vertex.EggFlags[1] == 0.0f);
    }

    CHECK(draw.Vertices[0].PosX == Catch::Approx(draw.Vertices[2].PosX));
    CHECK(draw.Vertices[1].PosX == Catch::Approx(draw.Vertices[3].PosX));
    CHECK(draw.Vertices[0].PosY == Catch::Approx(draw.Vertices[1].PosY));
    CHECK(draw.Vertices[2].PosY == Catch::Approx(draw.Vertices[3].PosY));
    CHECK(draw.Vertices[0].PosZ == Catch::Approx(draw.Vertices[1].PosZ));
    CHECK(draw.Vertices[0].PosZ == Catch::Approx(draw.Vertices[2].PosZ));
    CHECK(draw.Vertices[0].PosZ == Catch::Approx(draw.Vertices[3].PosZ));
    CHECK(draw.Vertices[1].PosX - draw.Vertices[0].PosX == Catch::Approx(1.0f).margin(0.001f));
    CHECK(draw.Vertices[2].PosY - draw.Vertices[0].PosY == Catch::Approx(16.0f).margin(0.01f));
    CHECK((draw.Vertices[0].PosY + draw.Vertices[2].PosY) * 0.5f == Catch::Approx(0.0f).margin(0.001f));
}

static void CheckEffekseerMultiInstanceTopology(const vector<CapturedEffekseerDraw>& draws, size_t expected_instance_count)
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(draws.size() == 1);
    const CapturedEffekseerDraw& draw = draws.front();
    REQUIRE(draw.Vertices.size() == expected_instance_count * 4);
    REQUIRE(draw.Indices.size() == expected_instance_count * 6);

    for (size_t instance_index = 0; instance_index < expected_instance_count; instance_index++) {
        const vindex_t vertex_base = numeric_cast<vindex_t>(instance_index * 4);
        const size_t index_base = instance_index * 6;
        CAPTURE(instance_index);
        CHECK(draw.Indices[index_base + 0] == vertex_base + 0);
        CHECK(draw.Indices[index_base + 1] == vertex_base + 1);
        CHECK(draw.Indices[index_base + 2] == vertex_base + 2);
        CHECK(draw.Indices[index_base + 3] == vertex_base + 2);
        CHECK(draw.Indices[index_base + 4] == vertex_base + 1);
        CHECK(draw.Indices[index_base + 5] == vertex_base + 3);
    }
}

static void CheckEffekseerRingGeometry(const vector<CapturedEffekseerDraw>& draws)
{
    FO_STACK_TRACE_ENTRY();

    constexpr size_t segment_count = 16;
    constexpr size_t vertices_per_segment = 8;
    constexpr size_t indices_per_segment = 12;

    REQUIRE(draws.size() == 1);
    const CapturedEffekseerDraw& draw = draws.front();
    CHECK(draw.PrimitiveType == RenderPrimitiveType::TriangleList);
    CHECK(draw.HasMainTexture);
    CHECK(draw.HasProjection);
    REQUIRE(draw.Vertices.size() == segment_count * vertices_per_segment);
    REQUIRE(draw.Indices.size() == segment_count * indices_per_segment);

    vec3 ring_center {};

    for (const Vertex2D& vertex : draw.Vertices) {
        CHECK(std::isfinite(vertex.PosX));
        CHECK(std::isfinite(vertex.PosY));
        CHECK(std::isfinite(vertex.PosZ));
        CHECK(vertex.TexU >= 0.0f);
        CHECK(vertex.TexU <= 1.0f);
        CHECK(vertex.TexV >= 0.0f);
        CHECK(vertex.TexV <= 1.0f);
        CHECK(vertex.EggFlags[0] == 0.0f);
        CHECK(vertex.EggFlags[1] == 0.0f);
        ring_center += vec3 {vertex.PosX, vertex.PosY, vertex.PosZ};
    }

    ring_center /= numeric_cast<float32_t>(draw.Vertices.size());

    const float32_t expected_radii[vertices_per_segment] = {2.0f, 1.5f, 2.0f, 1.5f, 1.5f, 1.0f, 1.5f, 1.0f};
    const float32_t expected_v[vertices_per_segment] = {0.0f, 0.5f, 0.0f, 0.5f, 0.5f, 1.0f, 0.5f, 1.0f};
    const vindex_t local_indices[indices_per_segment] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};

    for (size_t segment_index = 0; segment_index < segment_count; segment_index++) {
        const size_t vertex_base = segment_index * vertices_per_segment;
        const size_t index_base = segment_index * indices_per_segment;
        const float32_t expected_u = numeric_cast<float32_t>(segment_index) / numeric_cast<float32_t>(segment_count);
        const float32_t expected_next_u = numeric_cast<float32_t>(segment_index + 1) / numeric_cast<float32_t>(segment_count);

        for (size_t vertex_offset = 0; vertex_offset < vertices_per_segment; vertex_offset++) {
            const Vertex2D& vertex = draw.Vertices[vertex_base + vertex_offset];
            const vec3 position {vertex.PosX, vertex.PosY, vertex.PosZ};
            CAPTURE(segment_index, vertex_offset);
            CHECK(glm::length(position - ring_center) == Catch::Approx(expected_radii[vertex_offset]).margin(0.001f));
            CHECK(vertex.TexU == Catch::Approx(vertex_offset < 2 || vertex_offset == 4 || vertex_offset == 5 ? expected_u : expected_next_u));
            CHECK(vertex.TexV == Catch::Approx(expected_v[vertex_offset]));
        }

        for (size_t index_offset = 0; index_offset < indices_per_segment; index_offset++) {
            CAPTURE(segment_index, index_offset);
            CHECK(draw.Indices[index_base + index_offset] == numeric_cast<vindex_t>(vertex_base) + local_indices[index_offset]);
        }
    }
}

static auto GetEffekseerRingDepths(const vector<CapturedEffekseerDraw>& draws) -> vector<float32_t>
{
    FO_STACK_TRACE_ENTRY();

    constexpr size_t vertices_per_ring = 16 * 8;

    REQUIRE(draws.size() == 1);
    const vector<Vertex2D>& vertices = draws.front().Vertices;
    REQUIRE(vertices.size() % vertices_per_ring == 0);

    vector<float32_t> depths;
    depths.reserve(vertices.size() / vertices_per_ring);

    for (size_t vertex_base = 0; vertex_base < vertices.size(); vertex_base += vertices_per_ring) {
        float32_t depth = 0.0f;

        for (size_t vertex_offset = 0; vertex_offset < vertices_per_ring; vertex_offset++) {
            depth += vertices[vertex_base + vertex_offset].PosZ;
        }

        depths.emplace_back(depth / numeric_cast<float32_t>(vertices_per_ring));
    }

    return depths;
}

static auto GetEffekseerQuadDepths(const vector<CapturedEffekseerDraw>& draws) -> vector<float32_t>
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(draws.size() == 1);
    const vector<Vertex2D>& vertices = draws.front().Vertices;
    REQUIRE(vertices.size() % 4 == 0);

    vector<float32_t> depths;
    depths.reserve(vertices.size() / 4);

    for (size_t vertex_base = 0; vertex_base < vertices.size(); vertex_base += 4) {
        const float32_t depth = vertices[vertex_base].PosZ;
        CHECK(vertices[vertex_base + 1].PosZ == Catch::Approx(depth));
        CHECK(vertices[vertex_base + 2].PosZ == Catch::Approx(depth));
        CHECK(vertices[vertex_base + 3].PosZ == Catch::Approx(depth));
        depths.emplace_back(depth);
    }

    return depths;
}

TEST_CASE("Effekseer particle runtime produces deterministic FOnline callback geometry", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig first_rig;
    EffekseerRuntimeTestRig second_rig;

    const vector<CapturedEffekseerDraw> first_draws = DrawEffekseerFixture(first_rig, 173);
    const vector<CapturedEffekseerDraw> second_draws = DrawEffekseerFixture(second_rig, 173);

    CheckEffekseerDrawsEqual(first_draws, second_draws);
    CheckEffekseerFixtureGeometry(first_draws);
    REQUIRE(first_rig.GetTextureRequests().size() == 1);
    CHECK(first_rig.GetTextureRequests().front() == "Particles/Effekseer01/Texture/Particle01.png");
}

TEST_CASE("Effekseer particle runtime produces deterministic modern Ring geometry", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig first_rig {EffekseerRingFixturePath, ParticleTests::MakeModernRingEffect()};
    EffekseerRuntimeTestRig second_rig {EffekseerRingFixturePath, ParticleTests::MakeModernRingEffect()};

    const vector<CapturedEffekseerDraw> first_draws = DrawEffekseerFixture(first_rig, 307);
    const vector<CapturedEffekseerDraw> second_draws = DrawEffekseerFixture(second_rig, 307);

    CheckEffekseerDrawsEqual(first_draws, second_draws);
    CheckEffekseerRingGeometry(first_draws);
    CHECK(first_rig.GetTextureRequests().empty());
}

TEST_CASE("Effekseer particle runtime chunks Ring geometry within the index budget", "[particle][effekseer-runtime]")
{
    constexpr int32_t instance_count = 501;
    constexpr size_t segment_count = 16;
    constexpr size_t instances_in_full_chunk = 500;

    EffekseerRuntimeTestRig rig {EffekseerRingFixturePath, ParticleTests::MakeModernRingEffect(instance_count)};
    const vector<CapturedEffekseerDraw> draws = DrawEffekseerFixture(rig, 313);

    REQUIRE(draws.size() == 2);
    CHECK(draws[0].Vertices.size() == instances_in_full_chunk * segment_count * 8);
    CHECK(draws[0].Indices.size() == instances_in_full_chunk * segment_count * 12);
    CHECK(draws[1].Vertices.size() == segment_count * 8);
    CHECK(draws[1].Indices.size() == segment_count * 12);
    CHECK(*std::max_element(draws[0].Indices.begin(), draws[0].Indices.end()) == numeric_cast<vindex_t>(draws[0].Vertices.size() - 1));
    CHECK(*std::max_element(draws[1].Indices.begin(), draws[1].Indices.end()) == numeric_cast<vindex_t>(draws[1].Vertices.size() - 1));
}

TEST_CASE("Effekseer particle runtime honors cooked Ring Z-sort modes", "[particle][effekseer-runtime]")
{
    constexpr int32_t none = 0;
    constexpr int32_t normal_order = 1;
    constexpr int32_t reverse_order = 2;
    constexpr int32_t seed = 317;
    constexpr int32_t instance_count = 6;

    EffekseerRuntimeTestRig none_rig {EffekseerRingFixturePath, ParticleTests::MakeModernRingEffect(instance_count, none)};
    EffekseerRuntimeTestRig normal_rig {EffekseerRingFixturePath, ParticleTests::MakeModernRingEffect(instance_count, normal_order)};
    EffekseerRuntimeTestRig reverse_rig {EffekseerRingFixturePath, ParticleTests::MakeModernRingEffect(instance_count, reverse_order)};

    const vector<float32_t> none_depths = GetEffekseerRingDepths(DrawEffekseerFixture(none_rig, seed));
    const vector<float32_t> normal_depths = GetEffekseerRingDepths(DrawEffekseerFixture(normal_rig, seed));
    const vector<float32_t> reverse_depths = GetEffekseerRingDepths(DrawEffekseerFixture(reverse_rig, seed));

    vector<float32_t> sorted_depths = none_depths;
    std::sort(sorted_depths.begin(), sorted_depths.end());
    REQUIRE(sorted_depths.size() == numeric_cast<size_t>(instance_count));

    for (size_t depth_index = 1; depth_index < sorted_depths.size(); depth_index++) {
        CHECK(sorted_depths[depth_index - 1] < sorted_depths[depth_index]);
    }

    CHECK(normal_depths == sorted_depths);
    std::reverse(sorted_depths.begin(), sorted_depths.end());
    CHECK(reverse_depths == sorted_depths);
}

TEST_CASE("Effekseer particle runtime batches multiple callback instances deterministically", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig first_rig;
    EffekseerRuntimeTestRig second_rig;

    const vector<CapturedEffekseerDraw> first_draws = DrawEffekseerFixture(first_rig, 419, 6);
    const vector<CapturedEffekseerDraw> second_draws = DrawEffekseerFixture(second_rig, 419, 6);

    CheckEffekseerDrawsEqual(first_draws, second_draws);
    CheckEffekseerMultiInstanceTopology(first_draws, 6);
}

TEST_CASE("Effekseer particle runtime honors cooked sprite Z-sort modes", "[particle][effekseer-runtime]")
{
    constexpr int32_t none = 0;
    constexpr int32_t normal_order = 1;
    constexpr int32_t reverse_order = 2;
    constexpr int32_t seed = 503;
    constexpr int32_t frame_count = 6;
    constexpr size_t instance_count = 6;

    EffekseerRuntimeTestRig none_rig {EffekseerZSortFixturePath, ParticleTests::MakeZSortSpriteEffect(none)};
    EffekseerRuntimeTestRig normal_rig {EffekseerZSortFixturePath, ParticleTests::MakeZSortSpriteEffect(normal_order)};
    EffekseerRuntimeTestRig reverse_rig {EffekseerZSortFixturePath, ParticleTests::MakeZSortSpriteEffect(reverse_order)};

    const vector<CapturedEffekseerDraw> none_draws = DrawEffekseerFixture(none_rig, seed, frame_count);
    const vector<CapturedEffekseerDraw> normal_draws = DrawEffekseerFixture(normal_rig, seed, frame_count);
    const vector<CapturedEffekseerDraw> reverse_draws = DrawEffekseerFixture(reverse_rig, seed, frame_count);

    CheckEffekseerMultiInstanceTopology(none_draws, instance_count);
    CheckEffekseerMultiInstanceTopology(normal_draws, instance_count);
    CheckEffekseerMultiInstanceTopology(reverse_draws, instance_count);
    CHECK(normal_draws.front().EffectName == "Effects/Particles_ColorMul.fofx");

    const vector<float32_t> none_depths = GetEffekseerQuadDepths(none_draws);
    const vector<float32_t> normal_depths = GetEffekseerQuadDepths(normal_draws);
    const vector<float32_t> reverse_depths = GetEffekseerQuadDepths(reverse_draws);

    vector<float32_t> sorted_depths = none_depths;
    std::sort(sorted_depths.begin(), sorted_depths.end());
    REQUIRE(sorted_depths.size() == instance_count);

    for (size_t depth_index = 1; depth_index < sorted_depths.size(); depth_index++) {
        CHECK(sorted_depths[depth_index - 1] < sorted_depths[depth_index]);
    }

    CHECK(normal_depths == sorted_depths);
    std::reverse(sorted_depths.begin(), sorted_depths.end());
    CHECK(reverse_depths == sorted_depths);
}

TEST_CASE("Effekseer particle runtime repeats draw and seeded respawn packets", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig rig;
    unique_ptr<ParticleRuntimeSystem> system = rig.CreateSystem();

    system->Setup(MakeEffekseerIdentitySetup());
    system->Respawn(811);
    system->Update(1.0f / 60.0f);
    REQUIRE(system->IsActive());

    rig.ClearDraws();
    system->Draw();
    const vector<CapturedEffekseerDraw> first_draws = rig.GetDraws();
    CheckEffekseerFixtureGeometry(first_draws);

    rig.ClearDraws();
    system->Draw();
    const vector<CapturedEffekseerDraw> repeated_draws = rig.GetDraws();
    CheckEffekseerDrawsEqual(first_draws, repeated_draws);

    system->Respawn(811);
    system->Update(1.0f / 60.0f);
    rig.ClearDraws();
    system->Draw();
    const vector<CapturedEffekseerDraw> respawned_draws = rig.GetDraws();
    CheckEffekseerDrawsEqual(first_draws, respawned_draws);
}

TEST_CASE("Particle facade reapplies scale to an active Effekseer runtime", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig rig;
    optional<ParticleSystem> created_system = rig.CreateManagedSystem();
    REQUIRE(created_system);

    ParticleSystem& system = *created_system;
    const ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
    system.Setup(setup.Projection, setup.World, setup.PositionOffset, setup.LookDirectionAngle, setup.ViewOffset, setup.TiltInProjection);
    REQUIRE(system.Respawn(977));
    system.Update();
    REQUIRE(system.GetElapsedTime() > 0.0f);

    rig.ClearDraws();
    system.Draw();
    const vector<CapturedEffekseerDraw> unscaled_draws = rig.GetDraws();
    CheckEffekseerFixtureGeometry(unscaled_draws);
    CHECK_FALSE(system.NeedForceDraw());

    const float32_t elapsed_before_scale = system.GetElapsedTime();
    system.SetScale(2.0f);
    CHECK(system.NeedForceDraw());
    CHECK(system.GetElapsedTime() == elapsed_before_scale);

    rig.ClearDraws();
    system.Draw();
    const vector<CapturedEffekseerDraw> scaled_draws = rig.GetDraws();
    REQUIRE(scaled_draws.size() == unscaled_draws.size());

    for (size_t draw_index = 0; draw_index < unscaled_draws.size(); draw_index++) {
        const vector<Vertex2D>& unscaled_vertices = unscaled_draws[draw_index].Vertices;
        const vector<Vertex2D>& scaled_vertices = scaled_draws[draw_index].Vertices;
        REQUIRE(scaled_vertices.size() == unscaled_vertices.size());

        for (size_t vertex_index = 0; vertex_index < unscaled_vertices.size(); vertex_index++) {
            CAPTURE(draw_index, vertex_index);
            CHECK(scaled_vertices[vertex_index].PosX == Catch::Approx(unscaled_vertices[vertex_index].PosX * 2.0f).margin(0.001f));
            CHECK(scaled_vertices[vertex_index].PosY == Catch::Approx(unscaled_vertices[vertex_index].PosY * 2.0f).margin(0.001f));
            CHECK(scaled_vertices[vertex_index].PosZ == Catch::Approx(unscaled_vertices[vertex_index].PosZ * 2.0f).margin(0.001f));
        }
    }
}

TEST_CASE("Effekseer particle runtime rejects a missing color texture", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig rig {false};

    CHECK_FALSE(rig.CanCreateSystem());
    REQUIRE(rig.GetTextureRequests().size() == 1);
    CHECK(rig.GetTextureRequests().front() == "Particles/Effekseer01/Texture/Particle01.png");
}

#endif

FO_END_NAMESPACE
