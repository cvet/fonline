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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "Application.h"
#include "EffectManager.h"
#include "EffekseerCompiler.h"
#include "EffekseerExtension.h"
#include "FileSystem.h"
#if FO_ENABLE_3D
#include "ModelManager.h"
#endif
#include "Rendering.h"
#include "Test_BakerHelpers.h"
#include "Test_ParticleFixtures.h"
#include "VisualParticles.h"

#if FO_EFFEKSEER_PARTICLES
#include "Effekseer/SIMD/Mat43f.h"
#endif

FO_BEGIN_NAMESPACE

#if FO_EFFEKSEER_PARTICLES

static constexpr string_view EffekseerFixturePath = "Particles/Effekseer01/Simple_Sprite_FixedYAxis.efk";
static constexpr string_view EffekseerZSortFixturePath = "Particles/EffekseerTests/ZSort_Sprite.efk";
static constexpr string_view EffekseerRingFixturePath = "Particles/EffekseerTests/Modern_Ring.efk";
static constexpr string_view EffekseerStripFixturePath = "Particles/EffekseerTests/Strip.efk";
static constexpr string_view EffekseerModelFixturePath = "Particles/EffekseerTests/Mesh.efk";
static constexpr string_view EffekseerDistortionFixturePath = "Particles/EffekseerTests/Refraction.efk";
static constexpr frect32 EffekseerFixtureAtlasRect {0.125f, 0.25f, 0.5f, 0.375f};

struct CapturedEffekseerDraw final
{
    string EffectName {};
    vector<Vertex2D> Vertices {};
    // A refracting draw carries the particle's own plane per vertex, which is the model vertex layout
    vector<Vertex3D> Vertices3D {};
    bool HasBackgroundTexture {};
    vector<vindex_t> Indices {};
    RenderPrimitiveType PrimitiveType {};
    bool HasMainTexture {};
    bool HasProjection {};
    optional<array<float32_t, 4>> AtlasSubRect {};
    optional<array<float32_t, 4>> Sampling {};
    CullModeType CullMode {};
};

struct EffekseerDrawCapture final
{
    vector<CapturedEffekseerDraw> Draws {};
};

enum class TestSceneBackgroundMode : uint8_t
{
    Available,
    Deferred,
    Unavailable,
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
    AppScreenState _screen {};
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
    // Extra resources an effect loads by itself, such as the .efkmodel payloads a model node references
    EffekseerRuntimeTestRig(string_view effect_path, vector<uint8_t> effect_data, const map<string, vector<uint8_t>>& dependencies, bool provide_texture = true);

    [[nodiscard]] auto CreateSystem() -> unique_ptr<ParticleRuntimeSystem>;
    [[nodiscard]] auto CreateManagedSystem() -> optional<ParticleSystem>;
    [[nodiscard]] auto CanCreateSystem() -> bool;
    [[nodiscard]] auto TryCreateSystem() -> unique_nptr<ParticleRuntimeSystem>;
    [[nodiscard]] auto GetDraws() const -> const vector<CapturedEffekseerDraw>&;
    [[nodiscard]] auto GetTextureRequests() const -> const vector<string>&;
    [[nodiscard]] auto GetSceneBackground() const -> nptr<const RenderTexture>;
    void SetSceneBackgroundMode(TestSceneBackgroundMode mode);
    void ClearDraws();

private:
    [[nodiscard]] auto ProvideSceneBackground() const -> ParticleSceneBackgroundResult;

    EffekseerRuntimeTestSettings _settings {};
    string _effectPath;
    FileSystem _resources;
    unique_ptr<CapturingAppRender> _render;
    unique_ptr<EffectManager> _effectManager;
    unique_ptr<RenderTexture> _texture;
    unique_ptr<RenderTexture> _sceneBackground;
    vector<string> _textureRequests {};
    bool _provideTexture {};
    TestSceneBackgroundMode _sceneBackgroundMode {TestSceneBackgroundMode::Available};
    unique_ptr<GameTimer> _gameTimer;
    unique_ptr<ParticleManager> _particleManager;
    unique_ptr<EffekseerParticleRuntimeBackend> _backend;
};

EffekseerRuntimeTestSettings::EffekseerRuntimeTestSettings() :
    GlobalSettings {false}
{
    ApplyDefaultSettings();
    BakerTests::ApplySelfContainedClientSettings(*this);
}

CapturingRenderEffect::CapturingRenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader, shared_ptr<EffekseerDrawCapture> capture) :
    RenderEffect(usage, name, loader),
    _capture {std::move(capture)}
{
    FO_VERIFY_AND_THROW(_capture, "Capturing render effect requires capture storage");
}

void CapturingRenderEffect::DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex)
{
    ignore_unused(custom_tex);
    FO_VERIFY_AND_THROW(start_index <= dbuf->IndCount, "Captured draw starts outside the index buffer", start_index, dbuf->IndCount);

    size_t draw_index_count = indices_to_draw.value_or(dbuf->IndCount - start_index);
    FO_VERIFY_AND_THROW(draw_index_count <= dbuf->IndCount - start_index, "Captured draw exceeds the index buffer", start_index, draw_index_count, dbuf->IndCount);
    FO_VERIFY_AND_THROW(dbuf->IndCount <= dbuf->Indices.size(), "Captured draw exceeds the allocated index buffer", dbuf->IndCount, dbuf->Indices.size());

    // A draw carries either the 2D vertex layout or the model one, never both
    bool model_layout = dbuf->Vertices.empty();

    if (model_layout) {
        FO_VERIFY_AND_THROW(dbuf->VertCount <= dbuf->Vertices3D.size(), "Captured draw exceeds the model vertex buffer", dbuf->VertCount, dbuf->Vertices3D.size());
    }
    else {
        FO_VERIFY_AND_THROW(dbuf->VertCount <= dbuf->Vertices.size(), "Captured draw exceeds the vertex buffer", dbuf->VertCount, dbuf->Vertices.size());
    }

    const_span<Vertex2D> vertices {dbuf->Vertices.data(), model_layout ? 0 : dbuf->VertCount};
    const_span<Vertex3D> vertices_3d {dbuf->Vertices3D.data(), model_layout ? dbuf->VertCount : 0};
    const_span<vindex_t> indices {dbuf->Indices.data() + start_index, draw_index_count};
    vector<Vertex2D> captured_vertices(vertices.begin(), vertices.end());
    vector<Vertex3D> captured_vertices_3d(vertices_3d.begin(), vertices_3d.end());
    vector<vindex_t> captured_indices(indices.begin(), indices.end());
    optional<array<float32_t, 4>> captured_sub_rect;
    optional<array<float32_t, 4>> captured_sampling;

    if (SpriteBorderBuf.has_value()) {
        captured_sub_rect = array<float32_t, 4> {SpriteBorderBuf->SpriteBorder[0], SpriteBorderBuf->SpriteBorder[1], SpriteBorderBuf->SpriteBorder[2], SpriteBorderBuf->SpriteBorder[3]};
    }
    if (ParticleSamplingBuf.has_value()) {
        captured_sampling = array<float32_t, 4> {ParticleSamplingBuf->ParticleSampling[0], ParticleSamplingBuf->ParticleSampling[1], ParticleSamplingBuf->ParticleSampling[2], ParticleSamplingBuf->ParticleSampling[3]};
    }

    _capture->Draws.emplace_back(CapturedEffekseerDraw {
        .EffectName = _name,
        .Vertices = std::move(captured_vertices),
        .Vertices3D = std::move(captured_vertices_3d),
        .HasBackgroundTexture = !!BackgroundTex,
        .Indices = std::move(captured_indices),
        .PrimitiveType = dbuf->PrimType,
        .HasMainTexture = !!MainTex,
        .HasProjection = ProjBuf.has_value(),
        .AtlasSubRect = captured_sub_rect,
        .Sampling = captured_sampling,
        .CullMode = CullMode,
    });
}

CapturingAppRender::CapturingAppRender(ptr<GlobalSettings> settings) :
    _capture {safe_alloc::make_shared<EffekseerDrawCapture>()}
{
    _screen.Size = {settings->View.ScreenWidth, settings->View.ScreenHeight};
    _screen.Fullscreen = settings->Render.Fullscreen;
    _renderer.Init(*settings, &_screen, nullptr);
}

auto CapturingAppRender::GetRenderTarget() -> nptr<RenderTexture>
{
    return _renderTarget;
}

auto CapturingAppRender::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    return _renderer.CreateTexture(size, linear_filtered, with_depth);
}

auto CapturingAppRender::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    return _renderer.CreateDrawBuffer(is_static);
}

auto CapturingAppRender::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    return safe_alloc::make_unique<CapturingRenderEffect>(usage, name, loader, _capture);
}

auto CapturingAppRender::CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44
{
    return _renderer.CreateOrthoMatrix(left, right, bottom, top, nearp, farp);
}

auto CapturingAppRender::IsRenderTargetFlipped() const -> bool
{
    return _renderer.IsRenderTargetFlipped();
}

auto CapturingAppRender::GetProjMatrix() const -> mat44
{
    return _renderer.GetProjMatrix();
}

void CapturingAppRender::SetRenderTarget(nptr<RenderTexture> tex)
{
    _renderTarget = tex;
    _renderer.SetRenderTarget(tex);
}

void CapturingAppRender::SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept
{
    _renderer.SetOrthoDepthRange(nearp, farp);
}

void CapturingAppRender::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    _renderer.ClearRenderTarget(color, depth, stencil);
}

void CapturingAppRender::EnableScissor(irect32 rect)
{
    _renderer.EnableScissor(rect);
}

void CapturingAppRender::DisableScissor()
{
    _renderer.DisableScissor();
}

void CapturingAppRender::ClearDraws()
{
    _capture->Draws.clear();
}

auto CapturingAppRender::GetDraws() const -> const vector<CapturedEffekseerDraw>&
{
    return _capture->Draws;
}

static void AddEffekseerRuntimeTestResources(BakerTests::MemoryDataSource& source, string_view effect_path, vector<uint8_t> effect_data)
{
    static constexpr string_view effect_config = "[Effect]\nPasses = 1\n";
    static constexpr string_view effect_info = "[EffectInfo]\nMainTex = 0\nProjBuf = 1\n";

    // The runtime throws on a binary without the mandatory bounds trailer, and the cooked fixtures are raw Effekseer
    // payloads, so a representative trailer is appended here
    AppendEffekseerBoundsTrailer(effect_data, vec3 {-1.0f, -1.0f, -1.0f}, vec3 {1.0f, 1.0f, 1.0f}, 0.5f);
    source.AddFile(effect_path, std::move(effect_data));
    source.AddFile("Effects/Particles_ColorMul.fofx", effect_config);
    source.AddFile("Effects/Particles_ColorMul.fofx-1-info", effect_info);
    source.AddFile("Effects/Particles_ColorAdd.fofx", effect_config);
    source.AddFile("Effects/Particles_ColorAdd.fofx-1-info", effect_info);
    source.AddFile("Effects/Particles_ColorSub.fofx", effect_config);
    source.AddFile("Effects/Particles_ColorSub.fofx-1-info", effect_info);

    // The atlas-mapping variants additionally read the sub-rectangle and the sampling flags, so their reflected info
    // declares those buffers
    static constexpr string_view atlas_effect_info = "[EffectInfo]\nMainTex = 0\nProjBuf = 0\nSpriteBorderBuf = 1\nParticleSamplingBuf = 2\nMainTexBuf = 3\n";

    for (string_view atlas_effect : {"Effects/Particles_ColorMulAtlas.fofx", "Effects/Particles_ColorAddAtlas.fofx", "Effects/Particles_ColorSubAtlas.fofx"}) {
        source.AddFile(atlas_effect, effect_config);
        source.AddFile(strex("{}-1-info", atlas_effect), atlas_effect_info);
    }

    // The distortion variants read the same buffers plus the scene copy they refract
    static constexpr string_view distortion_effect_info = "[EffectInfo]\nMainTex = 0\nBackgroundTex = 1\nProjBuf = 0\nSpriteBorderBuf = 1\nParticleSamplingBuf = 2\nMainTexBuf = 3\n";

    for (string_view distortion_effect : {"Effects/Particles_DistortionAtlas.fofx", "Effects/Particles_DistortionAddAtlas.fofx"}) {
        source.AddFile(distortion_effect, effect_config);
        source.AddFile(strex("{}-1-info", distortion_effect), distortion_effect_info);
    }
}

static auto MakeEffekseerRuntimeTestResources(string_view effect_path, vector<uint8_t> effect_data, const map<string, vector<uint8_t>>& dependencies) -> FileSystem
{
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("EffekseerRuntimeTests");
    AddEffekseerRuntimeTestResources(*source, effect_path, std::move(effect_data));

    for (const auto& [path, data] : dependencies) {
        source->AddFile(path, data);
    }

    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

EffekseerRuntimeTestRig::EffekseerRuntimeTestRig(bool provide_texture) :
    EffekseerRuntimeTestRig {EffekseerFixturePath, ParticleTests::MakeSimpleSpriteFixedYAxisEffect(), provide_texture}
{
}

EffekseerRuntimeTestRig::EffekseerRuntimeTestRig(string_view effect_path, vector<uint8_t> effect_data, bool provide_texture) :
    EffekseerRuntimeTestRig {effect_path, std::move(effect_data), map<string, vector<uint8_t>> {}, provide_texture}
{
}

EffekseerRuntimeTestRig::EffekseerRuntimeTestRig(string_view effect_path, vector<uint8_t> effect_data, const map<string, vector<uint8_t>>& dependencies, bool provide_texture) :
    _effectPath {effect_path},
    _resources {MakeEffekseerRuntimeTestResources(effect_path, std::move(effect_data), dependencies)},
    _render {safe_alloc::make_unique<CapturingAppRender>(&_settings)},
    _effectManager {safe_alloc::make_unique<EffectManager>(&_settings, &_resources, _render.as_ptr())},
    _texture {_render->CreateTexture({8, 8}, true, false)},
    _sceneBackground {_render->CreateTexture({16, 16}, true, false)},
    _provideTexture {provide_texture},
    _gameTimer {safe_alloc::make_unique<GameTimer>(&_settings)},
    _particleManager {safe_alloc::make_unique<ParticleManager>(
        &_settings, _effectManager.as_ptr(), _render.as_ptr(), &_resources, _gameTimer.as_ptr(),
        [this](string_view path) -> pair<nptr<RenderTexture>, frect32> {
            _textureRequests.emplace_back(path);

            if (!_provideTexture) {
                return {nullptr, {}};
            }

            return {_texture.as_nptr(), EffekseerFixtureAtlasRect};
        },
        [this]() { return _settings.Render.DrawWireframe; }, [this]() { return ProvideSceneBackground(); })},
    _backend {safe_alloc::make_unique<EffekseerParticleRuntimeBackend>(ParticleRuntimeServices {
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
        // A refracting draw needs a scene to refract; the rig stands in for the sprite manager's snapshot with a
        // texture of its own, so a distortion effect is measured on its renderer rather than on a missing background
        .SceneBackgroundProvider = [this]() { return ProvideSceneBackground(); },
        .Settings = &_settings,
        .DrawWireframe = [this]() { return _settings.Render.DrawWireframe; },
    })}
{
}

auto EffekseerRuntimeTestRig::CreateSystem() -> unique_ptr<ParticleRuntimeSystem>
{
    auto system = _backend->Create(_effectPath);
    FO_VERIFY_AND_THROW(system, "Effekseer runtime test fixture failed to create");
    return system.take_not_null();
}

auto EffekseerRuntimeTestRig::CreateManagedSystem() -> optional<ParticleSystem>
{
    return _particleManager->CreateParticle(_effectPath);
}

auto EffekseerRuntimeTestRig::CanCreateSystem() -> bool
{
    return !!_backend->Create(_effectPath);
}

auto EffekseerRuntimeTestRig::TryCreateSystem() -> unique_nptr<ParticleRuntimeSystem>
{
    return _backend->Create(_effectPath);
}

auto EffekseerRuntimeTestRig::GetDraws() const -> const vector<CapturedEffekseerDraw>&
{
    return _render->GetDraws();
}

auto EffekseerRuntimeTestRig::GetTextureRequests() const -> const vector<string>&
{
    return _textureRequests;
}

auto EffekseerRuntimeTestRig::GetSceneBackground() const -> nptr<const RenderTexture>
{
    return _sceneBackground.as_nptr();
}

void EffekseerRuntimeTestRig::SetSceneBackgroundMode(TestSceneBackgroundMode mode)
{
    _sceneBackgroundMode = mode;
}

auto EffekseerRuntimeTestRig::ProvideSceneBackground() const -> ParticleSceneBackgroundResult
{
    switch (_sceneBackgroundMode) {
    case TestSceneBackgroundMode::Available:
        return {.State = ParticleSceneBackgroundState::Available, .Texture = _sceneBackground.as_nptr()};
    case TestSceneBackgroundMode::Deferred:
        return {.State = ParticleSceneBackgroundState::Deferred};
    case TestSceneBackgroundMode::Unavailable:
        return {};
    }

    throw GenericException("Unexpected test scene-background mode");
}

void EffekseerRuntimeTestRig::ClearDraws()
{
    _render->ClearDraws();
}

static auto MakeEffekseerIdentitySetup() -> ParticleRuntimeSetup
{
    return ParticleRuntimeSetup {
        .Projection = mat44 {1.0f},
        .World = mat44 {1.0f},
        .Scale = 1.0f,
    };
}

static auto DrawEffekseerFixture(EffekseerRuntimeTestRig& rig, int32_t seed, int32_t frame_count = 1) -> vector<CapturedEffekseerDraw>
{
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
    REQUIRE(draws.size() == 1);
    const CapturedEffekseerDraw& draw = draws.front();
    CHECK(draw.EffectName == "Effects/Particles_ColorAddAtlas.fofx");
    CHECK(draw.PrimitiveType == RenderPrimitiveType::TriangleList);
    CHECK(draw.HasMainTexture);
    CHECK(draw.HasProjection);
    REQUIRE(draw.Vertices.size() == 4);
    CHECK(draw.Indices == vector<vindex_t> {0, 1, 2, 2, 1, 3});

    // The vertex carries the emitter's raw coordinate and the shader maps it into the atlas entry, so the fixture's
    // full-sprite UV rectangle stays [0,1] here while the entry travels in the sub-rectangle below
    const float32_t expected_u[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    const float32_t expected_v[4] = {1.0f, 1.0f, 0.0f, 0.0f};

    REQUIRE(draw.AtlasSubRect.has_value());
    CHECK((*draw.AtlasSubRect)[0] == Catch::Approx(EffekseerFixtureAtlasRect.x));
    CHECK((*draw.AtlasSubRect)[1] == Catch::Approx(EffekseerFixtureAtlasRect.y));
    CHECK((*draw.AtlasSubRect)[2] == Catch::Approx(EffekseerFixtureAtlasRect.x + EffekseerFixtureAtlasRect.width));
    CHECK((*draw.AtlasSubRect)[3] == Catch::Approx(EffekseerFixtureAtlasRect.y + EffekseerFixtureAtlasRect.height));

    // The fixture clamps and asks for the atlas's own filtering, so the shader clamps and does not snap
    REQUIRE(draw.Sampling.has_value());
    CHECK((*draw.Sampling)[0] == 0.0f);
    CHECK((*draw.Sampling)[1] == 1.0f);

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
    REQUIRE(draws.size() == 1);
    const CapturedEffekseerDraw& draw = draws.front();
    REQUIRE(draw.Vertices.size() == expected_instance_count * 4);
    REQUIRE(draw.Indices.size() == expected_instance_count * 6);

    for (size_t instance_index = 0; instance_index < expected_instance_count; instance_index++) {
        vindex_t vertex_base = numeric_cast<vindex_t>(instance_index * 4);
        size_t index_base = instance_index * 6;
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
        size_t vertex_base = segment_index * vertices_per_segment;
        size_t index_base = segment_index * indices_per_segment;
        float32_t expected_u = numeric_cast<float32_t>(segment_index) / numeric_cast<float32_t>(segment_count);
        float32_t expected_next_u = numeric_cast<float32_t>(segment_index + 1) / numeric_cast<float32_t>(segment_count);

        for (size_t vertex_offset = 0; vertex_offset < vertices_per_segment; vertex_offset++) {
            const Vertex2D& vertex = draw.Vertices[vertex_base + vertex_offset];
            vec3 position {vertex.PosX, vertex.PosY, vertex.PosZ};
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
    REQUIRE(draws.size() == 1);
    const vector<Vertex2D>& vertices = draws.front().Vertices;
    REQUIRE(vertices.size() % 4 == 0);

    vector<float32_t> depths;
    depths.reserve(vertices.size() / 4);

    for (size_t vertex_base = 0; vertex_base < vertices.size(); vertex_base += 4) {
        float32_t depth = vertices[vertex_base].PosZ;
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

    vector<CapturedEffekseerDraw> first_draws = DrawEffekseerFixture(first_rig, 173);
    vector<CapturedEffekseerDraw> second_draws = DrawEffekseerFixture(second_rig, 173);

    CheckEffekseerDrawsEqual(first_draws, second_draws);
    CheckEffekseerFixtureGeometry(first_draws);
    REQUIRE(first_rig.GetTextureRequests().size() == 1);
    CHECK(first_rig.GetTextureRequests().front() == "Particles/Effekseer01/Texture/Particle01.png");
}

TEST_CASE("Effekseer particle runtime produces deterministic modern Ring geometry", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig first_rig {EffekseerRingFixturePath, ParticleTests::MakeModernRingEffect()};
    EffekseerRuntimeTestRig second_rig {EffekseerRingFixturePath, ParticleTests::MakeModernRingEffect()};

    vector<CapturedEffekseerDraw> first_draws = DrawEffekseerFixture(first_rig, 307);
    vector<CapturedEffekseerDraw> second_draws = DrawEffekseerFixture(second_rig, 307);

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
    vector<CapturedEffekseerDraw> draws = DrawEffekseerFixture(rig, 313);

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

    vector<float32_t> none_depths = GetEffekseerRingDepths(DrawEffekseerFixture(none_rig, seed));
    vector<float32_t> normal_depths = GetEffekseerRingDepths(DrawEffekseerFixture(normal_rig, seed));
    vector<float32_t> reverse_depths = GetEffekseerRingDepths(DrawEffekseerFixture(reverse_rig, seed));

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

    vector<CapturedEffekseerDraw> first_draws = DrawEffekseerFixture(first_rig, 419, 6);
    vector<CapturedEffekseerDraw> second_draws = DrawEffekseerFixture(second_rig, 419, 6);

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

    vector<CapturedEffekseerDraw> none_draws = DrawEffekseerFixture(none_rig, seed, frame_count);
    vector<CapturedEffekseerDraw> normal_draws = DrawEffekseerFixture(normal_rig, seed, frame_count);
    vector<CapturedEffekseerDraw> reverse_draws = DrawEffekseerFixture(reverse_rig, seed, frame_count);

    CheckEffekseerMultiInstanceTopology(none_draws, instance_count);
    CheckEffekseerMultiInstanceTopology(normal_draws, instance_count);
    CheckEffekseerMultiInstanceTopology(reverse_draws, instance_count);
    CHECK(normal_draws.front().EffectName == "Effects/Particles_ColorMulAtlas.fofx");

    vector<float32_t> none_depths = GetEffekseerQuadDepths(none_draws);
    vector<float32_t> normal_depths = GetEffekseerQuadDepths(normal_draws);
    vector<float32_t> reverse_depths = GetEffekseerQuadDepths(reverse_draws);

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
    vector<CapturedEffekseerDraw> first_draws = rig.GetDraws();
    CheckEffekseerFixtureGeometry(first_draws);

    rig.ClearDraws();
    system->Draw();
    vector<CapturedEffekseerDraw> repeated_draws = rig.GetDraws();
    CheckEffekseerDrawsEqual(first_draws, repeated_draws);

    system->Respawn(811);
    system->Update(1.0f / 60.0f);
    rig.ClearDraws();
    system->Draw();
    vector<CapturedEffekseerDraw> respawned_draws = rig.GetDraws();
    CheckEffekseerDrawsEqual(first_draws, respawned_draws);
}

TEST_CASE("Particle facade reapplies scale to an active Effekseer runtime", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig rig;
    optional<ParticleSystem> created_system = rig.CreateManagedSystem();
    REQUIRE(created_system);

    ParticleSystem& system = *created_system;
    ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
    system.Setup(setup.Projection, setup.World, setup.PositionOffset, setup.LookDirectionAngle, setup.ViewOffset, setup.TiltInProjection);
    REQUIRE(system.Respawn(977));
    system.Update();
    REQUIRE(system.GetElapsedTime() > timespan::zero);

    rig.ClearDraws();
    system.Draw();
    vector<CapturedEffekseerDraw> unscaled_draws = rig.GetDraws();
    CheckEffekseerFixtureGeometry(unscaled_draws);
    CHECK_FALSE(system.NeedForceDraw());

    timespan elapsed_before_scale = system.GetElapsedTime();
    system.SetScale(2.0f);
    CHECK(system.NeedForceDraw());
    CHECK(system.GetElapsedTime() == elapsed_before_scale);

    rig.ClearDraws();
    system.Draw();
    vector<CapturedEffekseerDraw> scaled_draws = rig.GetDraws();
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

TEST_CASE("Particle facade advances a model-attached effect from an explicit frame delta", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig rig;
    optional<ParticleSystem> created_system = rig.CreateManagedSystem();
    REQUIRE(created_system);

    ParticleSystem& system = *created_system;
    ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
    system.Setup(setup.Projection, setup.World, setup.PositionOffset, setup.LookDirectionAngle, setup.ViewOffset, setup.TiltInProjection);
    REQUIRE(system.Respawn(977));

    constexpr timespan frame_delta {std::chrono::nanoseconds {33333333}};
    system.Update(frame_delta);

    CHECK(system.GetElapsedTime() == frame_delta);
    CHECK(system.IsActive());

    rig.ClearDraws();
    system.Draw();
    CheckEffekseerFixtureGeometry(rig.GetDraws());
}

TEST_CASE("Particle facade preserves fractional elapsed time", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig rig;
    optional<ParticleSystem> created_system = rig.CreateManagedSystem();
    REQUIRE(created_system);

    ParticleSystem& system = *created_system;
    REQUIRE(system.Respawn(977));

    system.Update(timespan {500001});
    system.Update(timespan {1});
    CHECK(system.GetElapsedTime() == timespan {500002});
    CHECK_THROWS(system.Update(timespan {-1}));
    CHECK(system.GetElapsedTime() == timespan {500002});
}

TEST_CASE("Effekseer particle runtime rejects a missing color texture", "[particle][effekseer-runtime]")
{
    EffekseerRuntimeTestRig rig {false};

    CHECK_FALSE(rig.CanCreateSystem());
    REQUIRE(rig.GetTextureRequests().size() == 1);
    CHECK(rig.GetTextureRequests().front() == "Particles/Effekseer01/Texture/Particle01.png");
}

// A strip is delivered as one draw per instance group, so these fixtures are compiled from source here: the geometry only
// exists once several instances of one group are alive together, which a cooked single-instance fixture cannot express
static auto MakeStripFixtureRig(string_view project) -> unique_ptr<EffekseerRuntimeTestRig>
{
    EffekseerCompilerOutput compiled = CompileEffekseerProject("Particles/EffekseerTests/Strip.efkproj", {reinterpret_cast<const uint8_t*>(project.data()), project.size()});

    return safe_alloc::make_unique<EffekseerRuntimeTestRig>(EffekseerStripFixturePath, std::move(compiled.Binary));
}

static auto DrawStripFixture(EffekseerRuntimeTestRig& rig, const ParticleRuntimeSetup& setup) -> vector<CapturedEffekseerDraw>
{
    unique_ptr<ParticleRuntimeSystem> system = rig.CreateSystem();

    system->Setup(setup);
    system->Respawn(4711);
    REQUIRE(system->IsActive());

    // Long enough for every generation of the group to exist, short enough that none of them has expired
    for (int32_t frame = 0; frame < 8; frame++) {
        system->Update(1.0f / 60.0f);
    }

    rig.ClearDraws();
    system->Draw();
    REQUIRE(system->IsActive());
    return rig.GetDraws();
}

// The structure a strip draw must have whatever produced it: two quads per segment sharing the band centre line, the
// far edge of one segment being the near edge of the next, and the texture stretched across the whole chain
static void CheckStripGeometry(const CapturedEffekseerDraw& draw, size_t segment_count)
{
    REQUIRE(draw.EffectName == "Effects/Particles_ColorAddAtlas.fofx");
    REQUIRE(draw.PrimitiveType == RenderPrimitiveType::TriangleList);
    REQUIRE(draw.HasMainTexture);
    REQUIRE(draw.HasProjection);
    REQUIRE(draw.Vertices.size() == segment_count * 8);
    REQUIRE(draw.Indices.size() == segment_count * 12);
    // An untextured node draws through the renderer's white pixel, whose whole area is the sampled rectangle and which
    // never needs the point-sampling snap
    REQUIRE(draw.AtlasSubRect);
    CHECK((*draw.AtlasSubRect)[0] == Catch::Approx(0.0f));
    CHECK((*draw.AtlasSubRect)[1] == Catch::Approx(0.0f));
    CHECK((*draw.AtlasSubRect)[2] == Catch::Approx(1.0f));
    CHECK((*draw.AtlasSubRect)[3] == Catch::Approx(1.0f));
    REQUIRE(draw.Sampling);
    CHECK((*draw.Sampling)[0] == Catch::Approx(0.0f));

    for (size_t segment_index = 0; segment_index < segment_count; segment_index++) {
        size_t vertex_base = segment_index * 8;
        size_t index_base = segment_index * 12;
        const vector<Vertex2D>& vertices = draw.Vertices;

        // Two quads per segment, wound exactly like every other particle quad the renderer emits
        const size_t expected_indices[12] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};

        for (size_t offset = 0; offset < 12; offset++) {
            CHECK(numeric_cast<size_t>(draw.Indices[index_base + offset]) == vertex_base + expected_indices[offset]);
        }

        // The two quads meet on the centre line, so the inner corners are shared
        CHECK(vertices[vertex_base + 1].PosX == vertices[vertex_base + 4].PosX);
        CHECK(vertices[vertex_base + 1].PosY == vertices[vertex_base + 4].PosY);
        CHECK(vertices[vertex_base + 1].PosZ == vertices[vertex_base + 4].PosZ);
        CHECK(vertices[vertex_base + 3].PosX == vertices[vertex_base + 6].PosX);
        CHECK(vertices[vertex_base + 3].PosY == vertices[vertex_base + 6].PosY);
        CHECK(vertices[vertex_base + 3].PosZ == vertices[vertex_base + 6].PosZ);

        // The centre of a cross-section sits halfway between its edges
        CHECK(vertices[vertex_base + 1].PosX == Catch::Approx((vertices[vertex_base + 0].PosX + vertices[vertex_base + 5].PosX) * 0.5f));
        CHECK(vertices[vertex_base + 1].PosY == Catch::Approx((vertices[vertex_base + 0].PosY + vertices[vertex_base + 5].PosY) * 0.5f));
        CHECK(vertices[vertex_base + 1].PosZ == Catch::Approx((vertices[vertex_base + 0].PosZ + vertices[vertex_base + 5].PosZ) * 0.5f));

        // Effekseer stretches the texture across the strip: half the width per quad, and the V range of the segment
        float32_t v_near = numeric_cast<float32_t>(segment_index) / numeric_cast<float32_t>(segment_count);
        float32_t v_far = numeric_cast<float32_t>(segment_index + 1) / numeric_cast<float32_t>(segment_count);
        const float32_t expected_u[8] = {0.0f, 0.5f, 0.0f, 0.5f, 0.5f, 1.0f, 0.5f, 1.0f};
        const float32_t expected_v[8] = {v_near, v_near, v_far, v_far, v_near, v_near, v_far, v_far};

        for (size_t offset = 0; offset < 8; offset++) {
            CHECK(vertices[vertex_base + offset].TexU == Catch::Approx(expected_u[offset]));
            CHECK(vertices[vertex_base + offset].TexV == Catch::Approx(expected_v[offset]));
        }

        // A segment continues the previous one instead of starting a new band
        if (segment_index != 0) {
            for (size_t offset : {size_t {0}, size_t {1}, size_t {5}}) {
                size_t previous_offset = offset == 5 ? 7 : offset + 2;
                CHECK(vertices[vertex_base + offset].PosX == vertices[vertex_base - 8 + previous_offset].PosX);
                CHECK(vertices[vertex_base + offset].PosY == vertices[vertex_base - 8 + previous_offset].PosY);
                CHECK(vertices[vertex_base + offset].PosZ == vertices[vertex_base - 8 + previous_offset].PosZ);
            }
        }
    }
}

// A band whose orientation comes from a normalized axis inherits the precision of the engine's - and upstream
// Effekseer's - fast reciprocal square root, so widths are compared with a tolerance rather than bit-exactly
static constexpr float32_t StripWidthTolerance = 0.001f;

static auto GetStripWidthVector(const CapturedEffekseerDraw& draw, size_t segment_index) -> vec3
{
    const Vertex2D& left = draw.Vertices[segment_index * 8 + 0];
    const Vertex2D& right = draw.Vertices[segment_index * 8 + 5];

    return vec3 {left.PosX - right.PosX, left.PosY - right.PosY, left.PosZ - right.PosZ};
}

TEST_CASE("Effekseer particle runtime builds ribbon strip geometry", "[particle][effekseer-runtime]")
{
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeStripFixtureRig(ParticleTests::MakeSimpleRibbonProject(false));
    vector<CapturedEffekseerDraw> draws = DrawStripFixture(*rig, MakeEffekseerIdentitySetup());

    REQUIRE(draws.size() == 1);
    CheckStripGeometry(draws.front(), 3);

    // The authored edge offsets are -0.5 and 0.5, so an unrotated, unscaled band is one unit wide along its own X axis.
    // No normalization is involved on this path, so the width is exact
    for (size_t segment_index = 0; segment_index < 3; segment_index++) {
        vec3 width = GetStripWidthVector(draws.front(), segment_index);
        CHECK(glm::length(width) == Catch::Approx(1.0f));
        CHECK(width.x == Catch::Approx(-1.0f));
    }
}

TEST_CASE("Effekseer particle runtime orients a viewpoint-dependent ribbon toward the camera", "[particle][effekseer-runtime]")
{
    // A quarter turn aims the emitter's X axis at the camera, the one placement where the two ribbon orientations
    // disagree: an ordinary band collapses to a line, a viewpoint-dependent one keeps its width
    ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
    setup.World = glm::rotate(mat44 {1.0f}, 90.0f * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f});

    unique_ptr<EffekseerRuntimeTestRig> plain_rig = MakeStripFixtureRig(ParticleTests::MakeSimpleRibbonProject(false));
    vector<CapturedEffekseerDraw> plain_draws = DrawStripFixture(*plain_rig, setup);

    REQUIRE(plain_draws.size() == 1);
    CheckStripGeometry(plain_draws.front(), 3);
    vec3 plain_width = GetStripWidthVector(plain_draws.front(), 0);
    CHECK(glm::length(plain_width) == Catch::Approx(1.0f).margin(StripWidthTolerance));
    CHECK(std::abs(plain_width.z) == Catch::Approx(1.0f).margin(StripWidthTolerance));

    unique_ptr<EffekseerRuntimeTestRig> facing_rig = MakeStripFixtureRig(ParticleTests::MakeSimpleRibbonProject(true));
    vector<CapturedEffekseerDraw> facing_draws = DrawStripFixture(*facing_rig, setup);

    REQUIRE(facing_draws.size() == 1);
    CheckStripGeometry(facing_draws.front(), 3);
    vec3 facing_width = GetStripWidthVector(facing_draws.front(), 0);
    CHECK(glm::length(facing_width) == Catch::Approx(1.0f).margin(StripWidthTolerance));
    CHECK(facing_width.z == Catch::Approx(0.0f).margin(StripWidthTolerance));
}

TEST_CASE("Effekseer particle runtime builds track strip geometry", "[particle][effekseer-runtime]")
{
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeStripFixtureRig(ParticleTests::MakeSimpleTrackProject());
    vector<CapturedEffekseerDraw> draws = DrawStripFixture(*rig, MakeEffekseerIdentitySetup());

    REQUIRE(draws.size() == 1);
    CheckStripGeometry(draws.front(), 3);

    for (size_t segment_index = 0; segment_index < 3; segment_index++) {
        vec3 width = GetStripWidthVector(draws.front(), segment_index);
        const Vertex2D& near_center = draws.front().Vertices[segment_index * 8 + 1];
        const Vertex2D& far_center = draws.front().Vertices[segment_index * 8 + 3];
        vec3 trail {far_center.PosX - near_center.PosX, far_center.PosY - near_center.PosY, far_center.PosZ - near_center.PosZ};

        // The authored widths are all 1, so every cross-section is one unit wide however far along the trail it sits
        CHECK(glm::length(width) == Catch::Approx(1.0f).margin(StripWidthTolerance));

        // A track cross-section faces the camera across the direction of travel, so it is perpendicular to both
        CHECK(glm::dot(width, vec3 {0.0f, 0.0f, 1.0f}) == Catch::Approx(0.0f).margin(StripWidthTolerance));
        CHECK(glm::dot(glm::normalize(width), glm::normalize(trail)) == Catch::Approx(0.0f).margin(StripWidthTolerance));
    }
}

// A model node draws its mesh once per instance, so the fixture pairs a compiled project with the .efkmodel payload it
// references - four vertices, two faces, a distinct red channel per corner so the vertex mapping is visible
static auto MakeModelFixtureRig(int32_t culling, vector<uint8_t> model_payload) -> unique_ptr<EffekseerRuntimeTestRig>
{
    string project = ParticleTests::MakeModelProject(culling);
    EffekseerCompilerOutput compiled = CompileEffekseerProject("Particles/EffekseerTests/Mesh.efkproj", {reinterpret_cast<const uint8_t*>(project.data()), project.size()});
    map<string, vector<uint8_t>> dependencies;
    dependencies.emplace("Particles/EffekseerTests/Model/Fixture.efkmodel", std::move(model_payload));

    return safe_alloc::make_unique<EffekseerRuntimeTestRig>(EffekseerModelFixturePath, std::move(compiled.Binary), dependencies);
}

static auto MakeModelFixtureRig(int32_t culling) -> unique_ptr<EffekseerRuntimeTestRig>
{
    return MakeModelFixtureRig(culling, ParticleTests::MakeFixtureModelPayload());
}

TEST_CASE("Effekseer particle runtime rejects malformed model payloads", "[particle][effekseer-runtime]")
{
    vector<uint8_t> truncated = ParticleTests::MakeFixtureModelPayload();
    truncated.resize(5 * sizeof(int32_t));
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeModelFixtureRig(2, std::move(truncated));

    CHECK_FALSE(rig->CanCreateSystem());
}

TEST_CASE("Effekseer particle runtime draws model node meshes", "[particle][effekseer-runtime]")
{
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeModelFixtureRig(2);
    unique_ptr<ParticleRuntimeSystem> system = rig->CreateSystem();

    system->Setup(MakeEffekseerIdentitySetup());
    system->Respawn(913);
    REQUIRE(system->IsActive());

    for (int32_t frame = 0; frame < 4; frame++) {
        system->Update(1.0f / 60.0f);
    }

    rig->ClearDraws();
    system->Draw();
    REQUIRE(system->IsActive());

    const vector<CapturedEffekseerDraw>& draws = rig->GetDraws();
    REQUIRE(draws.size() == 1);

    const CapturedEffekseerDraw& draw = draws.front();
    CHECK(draw.EffectName == "Effects/Particles_ColorAddAtlas.fofx");
    CHECK(draw.PrimitiveType == RenderPrimitiveType::TriangleList);

    // Two generations are alive, and each contributes the mesh's two faces as three vertices apiece
    constexpr size_t mesh_vertices = 6;
    REQUIRE(draw.Vertices.size() % mesh_vertices == 0);
    REQUIRE(draw.Vertices.size() / mesh_vertices >= 2);
    REQUIRE(draw.Indices.size() == draw.Vertices.size());

    // The mesh is emitted as a plain triangle list, so the indices run straight through the vertices
    for (size_t index = 0; index < draw.Indices.size(); index++) {
        CHECK(numeric_cast<size_t>(draw.Indices[index]) == index);
    }

    // The face indices pin both winding and vertex mapping, and the expected reds follow Effekseer's colour multiply,
    // a shift by 8 rather than a divide by 255, so even white scales by 255/256
    array<uint8_t, mesh_vertices> expected_red {9, 19, 29, 29, 19, 39};
    array<float32_t, mesh_vertices> expected_u {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    array<float32_t, mesh_vertices> expected_v {1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};

    for (size_t corner = 0; corner < mesh_vertices; corner++) {
        CHECK(draw.Vertices[corner].Color.comp.r == expected_red[corner]);
        CHECK(draw.Vertices[corner].TexU == Catch::Approx(expected_u[corner]));
        CHECK(draw.Vertices[corner].TexV == Catch::Approx(expected_v[corner]));
    }

    // Every instance draws the same mesh, so the second copy repeats the first one's colours and texture coordinates
    for (size_t corner = 0; corner < mesh_vertices; corner++) {
        CHECK(draw.Vertices[mesh_vertices + corner].Color.comp.r == expected_red[corner]);
        CHECK(draw.Vertices[mesh_vertices + corner].TexU == Catch::Approx(expected_u[corner]));
        CHECK(draw.Vertices[mesh_vertices + corner].TexV == Catch::Approx(expected_v[corner]));
    }

    // An untextured mesh draws through the white pixel, whose whole area is the sampled rectangle
    REQUIRE(draw.AtlasSubRect);
    CHECK((*draw.AtlasSubRect)[2] == Catch::Approx(1.0f));
    CHECK((*draw.AtlasSubRect)[3] == Catch::Approx(1.0f));
}

TEST_CASE("Effekseer particle runtime carries the model node culling mode into the draw", "[particle][effekseer-runtime]")
{
    // Effekseer chooses which faces to discard per node, and the renderer has to ask for that mode per draw rather than
    // relying on the effect's own state
    array<pair<int32_t, CullModeType>, 3> cases {
        pair {0, CullModeType::Front},
        pair {1, CullModeType::Back},
        pair {2, CullModeType::None},
    };

    for (const auto& [culling, expected] : cases) {
        unique_ptr<EffekseerRuntimeTestRig> rig = MakeModelFixtureRig(culling);
        unique_ptr<ParticleRuntimeSystem> system = rig->CreateSystem();

        system->Setup(MakeEffekseerIdentitySetup());
        system->Respawn(913);
        REQUIRE(system->IsActive());

        for (int32_t frame = 0; frame < 4; frame++) {
            system->Update(1.0f / 60.0f);
        }

        rig->ClearDraws();
        system->Draw();

        const vector<CapturedEffekseerDraw>& draws = rig->GetDraws();
        REQUIRE(draws.size() == 1);
        CHECK(draws.front().CullMode == expected);
    }
}

// A distortion fixture has to be compiled from a real directory: the compiler assigns a texture index only for an image
// whose size it can read, and a refracting node without a texture is rejected by design
static auto MakeDistortionFixtureRig(float32_t intensity, int32_t alpha_blend) -> unique_ptr<EffekseerRuntimeTestRig>
{
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / std::format("fo_effekseer_distortion_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    string project_path = fs::path_to_string(temp_dir / "Refraction.efkproj");
    string texture_path = fs::path_to_string(temp_dir / "Texture" / "Distortion.png");
    auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(fs::path_to_string(temp_dir)); });

    string project = ParticleTests::MakeDistortionProject(intensity, alpha_blend);
    vector<uint8_t> image = ParticleTests::MakeFixtureImageHeader(8, 8);
    REQUIRE(fs::write_file(project_path, project));
    REQUIRE(fs::write_file(texture_path, string_view {reinterpret_cast<const char*>(image.data()), image.size()}));

    EffekseerCompilerOutput compiled = CompileEffekseerProject(project_path, {reinterpret_cast<const uint8_t*>(project.data()), project.size()});

    return safe_alloc::make_unique<EffekseerRuntimeTestRig>(EffekseerDistortionFixturePath, std::move(compiled.Binary));
}

static auto DrawDistortionFixture(EffekseerRuntimeTestRig& rig) -> vector<CapturedEffekseerDraw>
{
    unique_ptr<ParticleRuntimeSystem> system = rig.CreateSystem();

    system->Setup(MakeEffekseerIdentitySetup());
    system->Respawn(5501);
    REQUIRE(system->IsActive());

    for (int32_t frame = 0; frame < 4; frame++) {
        system->Update(1.0f / 60.0f);
    }

    rig.ClearDraws();
    system->Draw();
    REQUIRE(system->IsActive());
    return rig.GetDraws();
}

TEST_CASE("Effekseer particle runtime refracts the scene through distortion nodes", "[particle][effekseer-runtime]")
{
    constexpr float32_t intensity = 0.25f;
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeDistortionFixtureRig(intensity, 2);
    vector<CapturedEffekseerDraw> draws = DrawDistortionFixture(*rig);

    REQUIRE(draws.size() == 1);

    const CapturedEffekseerDraw& draw = draws.front();
    CHECK(draw.EffectName == "Effects/Particles_DistortionAddAtlas.fofx");

    // A refracting draw carries the model vertex layout, the scene copy it samples, and the intensity that scales the
    // displacement its texture describes
    CHECK(draw.Vertices.empty());
    CHECK(draw.HasBackgroundTexture);
    REQUIRE(draw.Sampling);
    CHECK((*draw.Sampling)[2] == Catch::Approx(intensity));

    // Two generations are alive, and each contributes one quad
    REQUIRE(draw.Vertices3D.size() % 4 == 0);
    REQUIRE(draw.Vertices3D.size() / 4 >= 2);
    REQUIRE(draw.Indices.size() == draw.Vertices3D.size() / 4 * 6);

    for (size_t quad = 0; quad < draw.Vertices3D.size() / 4; quad++) {
        const Vertex3D& first = draw.Vertices3D[quad * 4];

        // The particle's own plane travels per vertex: two unit axes, perpendicular to each other, shared by the quad
        CHECK(glm::length(first.Tangent) == Catch::Approx(1.0f).margin(0.001f));
        CHECK(glm::length(first.Bitangent) == Catch::Approx(1.0f).margin(0.001f));
        CHECK(glm::dot(first.Tangent, first.Bitangent) == Catch::Approx(0.0f).margin(0.001f));

        for (size_t corner = 1; corner < 4; corner++) {
            const Vertex3D& vertex = draw.Vertices3D[quad * 4 + corner];
            CHECK(vertex.Tangent == first.Tangent);
            CHECK(vertex.Bitangent == first.Bitangent);
        }
    }
}

TEST_CASE("Effekseer direct-model distortion survives its atlas preview", "[particle][effekseer-runtime]")
{
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeDistortionFixtureRig(1.0f, 1);
    unique_ptr<ParticleRuntimeSystem> system = rig->CreateSystem();

    system->Setup(MakeEffekseerIdentitySetup());
    system->Respawn(5501);
    REQUIRE(system->IsActive());

    for (int32_t frame = 0; frame < 4; frame++) {
        system->Update(1.0f / 60.0f);
    }

    // ModelDirectDraw keeps one auxiliary atlas frame for preview/hit testing. The ModelManager provider marks only
    // that offscreen draw as deferred, so its live distortion attachment must remain available for the scene replay
    rig->SetSceneBackgroundMode(TestSceneBackgroundMode::Deferred);
    rig->ClearDraws();
    system->Draw();
    CHECK(system->IsActive());
    CHECK(rig->GetDraws().empty());

    rig->SetSceneBackgroundMode(TestSceneBackgroundMode::Available);
    system->Draw();
    REQUIRE(system->IsActive());
    REQUIRE(rig->GetDraws().size() == 1);
    CHECK(rig->GetDraws().front().HasBackgroundTexture);

    // The deferred state is explicit. A normal runtime with no scene background retains the fail-closed contract
    rig->SetSceneBackgroundMode(TestSceneBackgroundMode::Unavailable);
    system->Draw();
    CHECK_FALSE(system->IsActive());
}

#if FO_ENABLE_3D
TEST_CASE("Model particle background policy defers atlas preview then supplies scene replay", "[particle][effekseer-runtime]")
{
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeDistortionFixtureRig(1.0f, 1);
    int32_t provider_calls = 0;
    ParticleSceneBackgroundProvider provider = [&provider_calls, &rig]() -> ParticleSceneBackgroundResult {
        provider_calls++;
        return {.State = ParticleSceneBackgroundState::Available, .Texture = rig->GetSceneBackground()};
    };

    ParticleSceneBackgroundResult atlas_preview = ResolveModelParticleSceneBackground(false, true, provider);
    CHECK(atlas_preview.State == ParticleSceneBackgroundState::Deferred);
    CHECK(provider_calls == 0);

    ParticleSceneBackgroundResult scene_replay = ResolveModelParticleSceneBackground(true, true, provider);
    CHECK(scene_replay.State == ParticleSceneBackgroundState::Available);
    CHECK(scene_replay.Texture == rig->GetSceneBackground());
    CHECK(provider_calls == 1);

    CHECK(ResolveModelParticleSceneBackground(false, false, provider).State == ParticleSceneBackgroundState::Unavailable);
    CHECK(ResolveModelParticleSceneBackground(false, true, {}).State == ParticleSceneBackgroundState::Unavailable);
    CHECK(ResolveModelParticleSceneBackground(true, true, {}).State == ParticleSceneBackgroundState::Unavailable);
}
#endif

TEST_CASE("Effekseer particle runtime picks the distortion blend the node asks for", "[particle][effekseer-runtime]")
{
    unique_ptr<EffekseerRuntimeTestRig> blended_rig = MakeDistortionFixtureRig(1.0f, 1);
    vector<CapturedEffekseerDraw> blended_draws = DrawDistortionFixture(*blended_rig);

    REQUIRE(blended_draws.size() == 1);
    CHECK(blended_draws.front().EffectName == "Effects/Particles_DistortionAtlas.fofx");

    unique_ptr<EffekseerRuntimeTestRig> added_rig = MakeDistortionFixtureRig(1.0f, 2);
    vector<CapturedEffekseerDraw> added_draws = DrawDistortionFixture(*added_rig);

    REQUIRE(added_draws.size() == 1);
    CHECK(added_draws.front().EffectName == "Effects/Particles_DistortionAddAtlas.fofx");
}

// A diagnostic census, never a normative assertion: it measures whatever corpus the working tree holds, and it draws
// what it accepts because a node's renderer parameters only arrive at draw time
TEST_CASE("Effekseer capability census", "[.census]")
{
    std::filesystem::path corpus {"Baking/Visual/Particles"};

    if (!std::filesystem::is_directory(corpus)) {
        WARN("baked particle corpus is absent");
        return;
    }

    static constexpr int32_t census_seed = 977;
    static constexpr int32_t census_frames = 4;
    static constexpr float32_t census_frame_delta = 1.0f / 30.0f;

    // The mesh loads through the runtime's own loader, so the payloads must be reachable exactly as in a resource
    // pack, or the census measures a missing file instead of the renderer
    map<string, vector<uint8_t>> dependencies;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(corpus)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".efkmodel") {
            continue;
        }

        auto file = std::ifstream {entry.path(), std::ios::binary};
        string relative = strex(fs::path_to_string(std::filesystem::relative(entry.path(), corpus))).normalize_path_slashes();
        dependencies.emplace(strex("Particles/{}", relative), vector<uint8_t> {std::istreambuf_iterator<char> {file}, std::istreambuf_iterator<char> {}});
    }

    size_t walked = 0;
    size_t accepted = 0;
    size_t drawn = 0;
    size_t retired_while_drawing = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(corpus)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".efk") {
            continue;
        }

        string relative_path = strex(fs::path_to_string(std::filesystem::relative(entry.path(), corpus))).normalize_path_slashes();
        vector<uint8_t> data;

        {
            auto file = std::ifstream {entry.path(), std::ios::binary};
            data.assign(std::istreambuf_iterator<char> {file}, std::istreambuf_iterator<char> {});
        }

        // The rig appends its own mandatory bounds trailer, so hand it the bare payload
        EffekseerBoundsTrailer trailer = ReadEffekseerBoundsTrailer(data);
        data.resize(trailer.PayloadSize);

        EffekseerRuntimeTestRig rig {strex("Particles/{}", relative_path), std::move(data), dependencies};
        walked++;
        unique_nptr<ParticleRuntimeSystem> created = rig.TryCreateSystem();

        if (!created) {
            continue;
        }

        accepted++;
        logging::write("CENSUSPASS\tParticles/{}", relative_path);

        // Play it for a few frames so the node renderers run: a rejection that only a real draw can reach retires the
        // handle, which the runtime reports as the system going inactive
        unique_ptr<ParticleRuntimeSystem> system = created.take_not_null();
        system->Setup(MakeEffekseerIdentitySetup());
        system->Respawn(census_seed);

        for (int32_t frame = 0; frame < census_frames && system->IsActive(); frame++) {
            system->Update(census_frame_delta);
            system->Draw();
        }

        if (system->IsActive()) {
            drawn++;
        }
        else {
            retired_while_drawing++;
            logging::write("CENSUSRETIRED\tParticles/{}", relative_path);
        }
    }

    logging::write("CENSUSDONE\twalked={}\taccepted={}\tdrewToCompletion={}\tretiredWhileDrawing={}", walked, accepted, drawn, retired_while_drawing);
    CHECK(walked != 0);
}

#endif

#if FO_EFFEKSEER_PARTICLES

// The compiler writes a distinct section per authored value type, so these cases walk the type switches directly
// with a caller-supplied node body, one node per type compiled as a whole project
static auto MakeEffekseerNodeBodyProject(string_view node_body) -> string
{
    return strex(R"EFFEKSEER(<?xml version="1.0" encoding="utf-8"?>
<EffekseerProject>
  <Root>
    <Name>Root</Name>
    <Children>
      <Node>
        <CommonValues>
          <MaxGeneration>
            <Value>1</Value>
          </MaxGeneration>
          <Life>
            <Center>30</Center>
            <Max>30</Max>
            <Min>30</Min>
          </Life>
          <Generation>
            <GenerationTime>
              <Center>1</Center>
              <Max>1</Max>
              <Min>1</Min>
            </GenerationTime>
          </Generation>
        </CommonValues>
        <RendererCommonValues>
          <AlphaBlend>2</AlphaBlend>
        </RendererCommonValues>
{}
        <Name>NodeBody</Name>
        <Children />
      </Node>
    </Children>
  </Root>
  <Dynamic>
    <Inputs>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
    </Inputs>
    <Equations />
  </Dynamic>
  <ProceduralModel>
    <ProceduralModels />
  </ProceduralModel>
  <ToolVersion>1.80.5</ToolVersion>
  <Version>3</Version>
  <StartFrame>0</StartFrame>
  <EndFrame>60</EndFrame>
  <IsLoop>False</IsLoop>
</EffekseerProject>
)EFFEKSEER",
        node_body)
        .str();
}

static auto CompileEffekseerNodeBody(string_view node_body) -> EffekseerCompilerOutput
{
    string project = MakeEffekseerNodeBodyProject(node_body);
    return CompileEffekseerProject("Particles/EffekseerTests/NodeBody.efkproj", {reinterpret_cast<const uint8_t*>(project.data()), project.size()});
}

static auto MakeEffekseerTypeSweepProject(string_view location_values, string_view rotation_values, string_view scaling_values) -> string
{
    return strex(R"EFFEKSEER(<?xml version="1.0" encoding="utf-8"?>
<EffekseerProject>
  <Root>
    <Name>Root</Name>
    <Children>
      <Node>
        <CommonValues>
          <MaxGeneration>
            <Value>1</Value>
          </MaxGeneration>
          <Life>
            <Center>30</Center>
            <Max>30</Max>
            <Min>30</Min>
          </Life>
          <Generation>
            <GenerationTime>
              <Center>1</Center>
              <Max>1</Max>
              <Min>1</Min>
            </GenerationTime>
          </Generation>
        </CommonValues>
{}
{}
{}
        <RendererCommonValues>
          <AlphaBlend>2</AlphaBlend>
        </RendererCommonValues>
        <DrawingValues>
          <Type>2</Type>
          <Sprite />
        </DrawingValues>
        <Name>TypeSweep</Name>
        <Children />
      </Node>
    </Children>
  </Root>
  <Dynamic>
    <Inputs>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
    </Inputs>
    <Equations />
  </Dynamic>
  <ProceduralModel>
    <ProceduralModels />
  </ProceduralModel>
  <ToolVersion>1.80.5</ToolVersion>
  <Version>3</Version>
  <StartFrame>0</StartFrame>
  <EndFrame>60</EndFrame>
  <IsLoop>False</IsLoop>
</EffekseerProject>
)EFFEKSEER",
        location_values, rotation_values, scaling_values)
        .str();
}

static auto CompileEffekseerTypeSweep(string_view location_values, string_view rotation_values, string_view scaling_values) -> EffekseerCompilerOutput
{
    string project = MakeEffekseerTypeSweepProject(location_values, rotation_values, scaling_values);
    return CompileEffekseerProject("Particles/EffekseerTests/TypeSweep.efkproj", {reinterpret_cast<const uint8_t*>(project.data()), project.size()});
}

TEST_CASE("Effekseer compiler writes every authored location type", "[particle][effekseer-compiler]")
{
    vector<string> location_variants {
        "<LocationValues><Type>0</Type><Fixed><Location><X>1</X><Y>2</Y><Z>3</Z></Location></Fixed></LocationValues>",
        "<LocationValues><Type>1</Type><PVA><Location><X><Center>1</Center><Max>2</Max><Min>0</Min></X></Location></PVA></LocationValues>",
        "<LocationValues><Type>2</Type><Easing><End><Y><Center>4</Center><Max>4</Max><Min>4</Min></Y></End></Easing></LocationValues>",
        "<LocationValues><Type>3</Type><LocationFCurve><FCurve /></LocationFCurve></LocationValues>",
        "<LocationValues><Type>5</Type><ViewOffset><Distance><Center>3</Center><Max>3</Max><Min>3</Min></Distance></ViewOffset></LocationValues>",
    };

    for (const string& location : location_variants) {
        INFO(location);
        EffekseerCompilerOutput compiled = CompileEffekseerTypeSweep(location, "", "");
        CHECK_FALSE(compiled.Binary.empty());
    }

    // An unknown type is rejected rather than silently written as garbage
    CHECK_THROWS_AS(CompileEffekseerTypeSweep("<LocationValues><Type>99</Type></LocationValues>", "", ""), EffekseerCompilerException);
}

TEST_CASE("Effekseer compiler writes every authored rotation and scale type", "[particle][effekseer-compiler]")
{
    for (int32_t type = 0; type <= 7; type++) {
        string rotation = strex("<RotationValues><Type>{}</Type></RotationValues>", type).str();
        INFO(rotation);
        EffekseerCompilerOutput compiled = CompileEffekseerTypeSweep("", rotation, "");
        CHECK_FALSE(compiled.Binary.empty());
    }

    for (int32_t type = 0; type <= 6; type++) {
        string scaling = strex("<ScalingValues><Type>{}</Type></ScalingValues>", type).str();
        INFO(scaling);
        EffekseerCompilerOutput compiled = CompileEffekseerTypeSweep("", "", scaling);
        CHECK_FALSE(compiled.Binary.empty());
    }

    CHECK_THROWS_AS(CompileEffekseerTypeSweep("", "<RotationValues><Type>99</Type></RotationValues>", ""), EffekseerCompilerException);
    CHECK_THROWS_AS(CompileEffekseerTypeSweep("", "", "<ScalingValues><Type>99</Type></ScalingValues>"), EffekseerCompilerException);
}

TEST_CASE("EffekseerCompilerWritesOptionalNodeSections", "[effekseer-compiler]")
{
    SECTION("RingRendererShapesAndColours")
    {
        // Renderer type 4 is the ring. Its shape, per-ring locations and per-ring colours are all selected by
        // plain integers on the Ring node, with the payload in sibling <Name>_Fixed / _Random / _Easing nodes
        for (int32_t shape_type : {0, 1, 2, 3}) {
            string body = strex(R"(        <DrawingValues>
          <Type>4</Type>
          <Ring>
            <RingShape>
              <Type>{}</Type>
              <Crescent>
                <StartingFade>0</StartingFade>
                <EndingFade>0</EndingFade>
                <StartingAngle>0</StartingAngle>
                <StartingAngle_Fixed>0</StartingAngle_Fixed>
                <EndingAngle>1</EndingAngle>
                <EndingAngle_Random><Max>360</Max><Min>0</Min></EndingAngle_Random>
              </Crescent>
            </RingShape>
            <VertexCount>8</VertexCount>
            <ViewingAngle>0</ViewingAngle>
            <ViewingAngle_Fixed>360</ViewingAngle_Fixed>
            <OuterColor>0</OuterColor>
            <OuterColor_Fixed><R>255</R><G>255</G><B>255</B><A>255</A></OuterColor_Fixed>
            <CenterColor>1</CenterColor>
            <CenterColor_Random><R><Max>255</Max><Min>0</Min></R><G><Max>255</Max><Min>0</Min></G><B><Max>255</Max><Min>0</Min></B><A><Max>255</Max><Min>0</Min></A></CenterColor_Random>
            <InnerColor>2</InnerColor>
            <InnerColor_Easing><Start><Max><R>255</R><G>255</G><B>255</B><A>255</A></Max><Min><R>0</R><G>0</G><B>0</B><A>0</A></Min></Start><End><Max><R>255</R><G>255</G><B>255</B><A>255</A></Max><Min><R>0</R><G>0</G><B>0</B><A>0</A></Min></End></InnerColor_Easing>
          </Ring>
        </DrawingValues>)",
                shape_type)
                              .str();

            INFO(body);
            EffekseerCompilerOutput compiled = CompileEffekseerNodeBody(body);
            CHECK_FALSE(compiled.Binary.empty());
        }
    }

    SECTION("RibbonAndTrackRenderers")
    {
        // Renderer types 3 and 6 are the ribbon and the track. Their colour selectors are plain integers on
        // the renderer node, not nested colour blocks, so the defaults are enough to run both writers
        for (int32_t renderer_type : {3, 6}) {
            string body = strex(R"(        <DrawingValues>
          <Type>{}</Type>
          <Ribbon>
            <ViewpointDependent>false</ViewpointDependent>
            <ColorAll>0</ColorAll>
            <ColorAll_Fixed><R>255</R><G>255</G><B>255</B><A>255</A></ColorAll_Fixed>
          </Ribbon>
          <Track>
            <TrackSizeFor>0</TrackSizeFor>
            <TrackSizeFor_Fixed>1</TrackSizeFor_Fixed>
            <SplineDivision>2</SplineDivision>
          </Track>
        </DrawingValues>)",
                renderer_type)
                              .str();

            INFO(body);
            EffekseerCompilerOutput compiled = CompileEffekseerNodeBody(body);
            CHECK_FALSE(compiled.Binary.empty());
        }

        // The ribbon colour selector also has random and easing variants
        for (int32_t color_all : {1, 2}) {
            string body = strex(R"(        <DrawingValues>
          <Type>3</Type>
          <Ribbon>
            <ColorAll>{}</ColorAll>
            <ColorAll_Random><R><Max>255</Max><Min>0</Min></R><G><Max>255</Max><Min>0</Min></G><B><Max>255</Max><Min>0</Min></B><A><Max>255</Max><Min>0</Min></A></ColorAll_Random>
          </Ribbon>
        </DrawingValues>)",
                color_all)
                              .str();

            INFO(body);
            CHECK_FALSE(CompileEffekseerNodeBody(body).Binary.empty());
        }

        CHECK_THROWS_AS(CompileEffekseerNodeBody("        <DrawingValues>\n          <Type>9</Type>\n        </DrawingValues>"), EffekseerCompilerException);
    }

    SECTION("EveryStandardColourType")
    {
        // 0 fixed, 1 random, 2 easing, 3 f-curve, 4 gradient
        string curve_colour = R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
          <ColorAll>
              <Type>3</Type>
              <FCurve>
                <FCurve>
                  <Timeline>0</Timeline>
                  <Keys>
                    <R><StartType>0</StartType><EndType>0</EndType><OffsetMax>0</OffsetMax><OffsetMin>0</OffsetMin><Keys>
                      <Key><Frame>0</Frame><Value>0</Value><LeftX>0</LeftX><LeftY>0</LeftY><RightX>1</RightX><RightY>0</RightY><InterpolationType>1</InterpolationType></Key>
                      <Key><Frame>30</Frame><Value>255</Value><LeftX>29</LeftX><LeftY>255</LeftY><RightX>31</RightX><RightY>255</RightY><InterpolationType>1</InterpolationType></Key>
                    </Keys></R>
                    <G><StartType>0</StartType><EndType>0</EndType><Keys>
                      <Key><Frame>0</Frame><Value>128</Value><InterpolationType>0</InterpolationType></Key>
                    </Keys></G>
                    <B><StartType>0</StartType><EndType>0</EndType><Keys /></B>
                    <A><StartType>0</StartType><EndType>0</EndType><Keys>
                      <Key><Frame>0</Frame><Value>255</Value><InterpolationType>2</InterpolationType></Key>
                      <Key><Frame>60</Frame><Value>0</Value><InterpolationType>2</InterpolationType></Key>
                    </Keys></A>
                  </Keys>
                </FCurve>
              </FCurve>
          </ColorAll>
        </DrawingValues>)";
        CHECK_FALSE(CompileEffekseerNodeBody(curve_colour).Binary.empty());

        string easing_colour = R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
          <ColorAll><Type>2</Type><Easing><Start><Max><R>255</R><G>255</G><B>255</B><A>255</A></Max><Min><R>0</R><G>0</G><B>0</B><A>0</A></Min></Start><End><Max><R>255</R><G>255</G><B>255</B><A>255</A></Max><Min><R>0</R><G>0</G><B>0</B><A>0</A></Min></End></Easing></ColorAll>
        </DrawingValues>)";
        CHECK_FALSE(CompileEffekseerNodeBody(easing_colour).Binary.empty());

        string gradient_colour = R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
          <ColorAll>
              <Type>4</Type>
              <Gradient>
                <ColorMarkers>
                  <ColorMarker><Position>0</Position><ColorR>1</ColorR><ColorG>0</ColorG><ColorB>0</ColorB><Intensity>1</Intensity></ColorMarker>
                  <ColorMarker><Position>1</Position><ColorR>0</ColorR><ColorG>0</ColorG><ColorB>1</ColorB><Intensity>1</Intensity></ColorMarker>
                </ColorMarkers>
                <AlphaMarkers>
                  <AlphaMarker><Position>0</Position><Alpha>1</Alpha></AlphaMarker>
                </AlphaMarkers>
              </Gradient>
          </ColorAll>
        </DrawingValues>)";
        CHECK_FALSE(CompileEffekseerNodeBody(gradient_colour).Binary.empty());

        CHECK_THROWS_AS(CompileEffekseerNodeBody(R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
          <ColorAll><Type>9</Type></ColorAll>
        </DrawingValues>)"),
            EffekseerCompilerException);
    }

    SECTION("CurveDrivenLocationSamplesTheCurveSolver")
    {
        // Location type 3 is the f-curve, which is the only path that runs the cubic curve solver
        string body = R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
        </DrawingValues>
        <LocationValues>
          <Type>3</Type>
          <LocationFCurve>
            <FCurve>
              <Timeline>0</Timeline>
              <Keys>
                <X><StartType>0</StartType><EndType>0</EndType><OffsetMax>0</OffsetMax><OffsetMin>0</OffsetMin><Keys>
                  <Key><Frame>0</Frame><Value>0</Value><LeftX>-1</LeftX><LeftY>0</LeftY><RightX>5</RightX><RightY>2</RightY><InterpolationType>1</InterpolationType></Key>
                  <Key><Frame>30</Frame><Value>10</Value><LeftX>25</LeftX><LeftY>8</LeftY><RightX>35</RightX><RightY>12</RightY><InterpolationType>1</InterpolationType></Key>
                  <Key><Frame>60</Frame><Value>0</Value><LeftX>55</LeftX><LeftY>2</LeftY><RightX>65</RightX><RightY>0</RightY><InterpolationType>1</InterpolationType></Key>
                </Keys></X>
                <Y><StartType>1</StartType><EndType>1</EndType><Keys>
                  <Key><Frame>0</Frame><Value>0</Value><InterpolationType>0</InterpolationType></Key>
                  <Key><Frame>60</Frame><Value>5</Value><InterpolationType>0</InterpolationType></Key>
                </Keys></Y>
                <Z><StartType>2</StartType><EndType>2</EndType><Keys>
                  <Key><Frame>0</Frame><Value>1</Value><InterpolationType>2</InterpolationType></Key>
                </Keys></Z>
              </Keys>
            </FCurve>
          </LocationFCurve>
        </LocationValues>)";
        CHECK_FALSE(CompileEffekseerNodeBody(body).Binary.empty());

        // Type 5 is the view-offset variant, and an unknown type must be rejected
        CHECK_FALSE(CompileEffekseerNodeBody(R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
        </DrawingValues>
        <LocationValues>
          <Type>5</Type>
          <ViewOffset><Distance><Max>5</Max><Min>1</Min></Distance></ViewOffset>
        </LocationValues>)")
                .Binary.empty());
    }

    SECTION("AbsoluteLocationFields")
    {
        // The location-abs section carries the external force fields
        for (int32_t field_type : {0, 1, 2}) {
            string body = strex(R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
        </DrawingValues>
        <LocationAbsValues>
          <Type>{}</Type>
          <Position><X>0</X><Y>0</Y><Z>0</Z></Position>
          <Rotation><X>0</X><Y>0</Y><Z>0</Z></Rotation>
        </LocationAbsValues>)",
                field_type)
                              .str();

            INFO(body);
            CHECK_FALSE(CompileEffekseerNodeBody(body).Binary.empty());
        }
    }

    SECTION("KillRuleShapes")
    {
        // 0 none, 1 box, 2 plane, 3 sphere - each writes a different payload
        for (int32_t rule_type : {0, 1, 2, 3}) {
            string body = strex(R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
        </DrawingValues>
        <KillRulesValues>
          <Type>{}</Type>
          <BoxCenter><X>0</X><Y>0</Y><Z>0</Z></BoxCenter>
          <BoxSize><X>2</X><Y>2</Y><Z>2</Z></BoxSize>
          <PlaneAxis>2</PlaneAxis>
          <PlaneOffset>3</PlaneOffset>
          <SphereCenter><X>0</X><Y>0</Y><Z>0</Z></SphereCenter>
          <SphereRadius>4</SphereRadius>
        </KillRulesValues>)",
                rule_type)
                              .str();

            INFO(body);
            EffekseerCompilerOutput compiled = CompileEffekseerNodeBody(body);
            CHECK_FALSE(compiled.Binary.empty());
        }

        // An out-of-range plane axis has no normal to write, so it must be rejected
        CHECK_THROWS_AS(CompileEffekseerNodeBody(R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
        </DrawingValues>
        <KillRulesValues>
          <Type>2</Type>
          <PlaneAxis>9</PlaneAxis>
        </KillRulesValues>)"),
            EffekseerCompilerException);
    }

    SECTION("RotationAndScaleEasingAndCurves")
    {
        // The easing and f-curve variants of rotation and scale are separate writers from the fixed ones
        string rotation_easing = R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
        </DrawingValues>
        <RotationValues>
          <Type>2</Type>
          <Easing>
            <Start><Max><X>0</X><Y>0</Y><Z>0</Z></Max><Min><X>0</X><Y>0</Y><Z>0</Z></Min></Start>
            <End><Max><X>360</X><Y>360</Y><Z>360</Z></Max><Min><X>0</X><Y>0</Y><Z>0</Z></Min></End>
          </Easing>
        </RotationValues>)";
        CHECK_FALSE(CompileEffekseerNodeBody(rotation_easing).Binary.empty());

        string scale_easing = R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
        </DrawingValues>
        <ScalingValues>
          <Type>2</Type>
          <Easing>
            <Start><Max><X>1</X><Y>1</Y><Z>1</Z></Max><Min><X>1</X><Y>1</Y><Z>1</Z></Min></Start>
            <End><Max><X>2</X><Y>2</Y><Z>2</Z></Max><Min><X>2</X><Y>2</Y><Z>2</Z></Min></End>
          </Easing>
        </ScalingValues>)";
        CHECK_FALSE(CompileEffekseerNodeBody(scale_easing).Binary.empty());

        string location_easing = R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
        </DrawingValues>
        <LocationValues>
          <Type>2</Type>
          <Easing>
            <Start><Max><X>0</X><Y>0</Y><Z>0</Z></Max><Min><X>0</X><Y>0</Y><Z>0</Z></Min></Start>
            <End><Max><X>5</X><Y>5</Y><Z>5</Z></Max><Min><X>0</X><Y>0</Y><Z>0</Z></Min></End>
          </Easing>
        </LocationValues>)";
        CHECK_FALSE(CompileEffekseerNodeBody(location_easing).Binary.empty());
    }

    SECTION("DepthAndTextureSections")
    {
        // Depth ordering and the texture UV modes are written for every node
        for (int32_t uv_type : {0, 1, 2, 3}) {
            string body = strex(R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
          <TextureUVType>
            <Type>{}</Type>
            <Strech><Start>0</Start><End>1</End></Strech>
            <Tile><FrameLength>1</FrameLength><FrameCountX>1</FrameCountX><FrameCountY>1</FrameCountY><LoopType>0</LoopType></Tile>
            <Animation><FrameLength>1</FrameLength><FrameCountX>1</FrameCountX><FrameCountY>1</FrameCountY><LoopType>0</LoopType><StartSheet><Max>0</Max><Min>0</Min></StartSheet></Animation>
            <Scroll><Speed><Max><X>0</X><Y>0</Y></Max><Min><X>0</X><Y>0</Y></Min></Speed></Scroll>
            <FCurve><Start><X /><Y /></Start><Size><X /><Y /></Size></FCurve>
          </TextureUVType>
        </DrawingValues>
        <DepthValues>
          <DepthOffset>1</DepthOffset>
          <ZSort>1</ZSort>
          <DrawingPriority>2</DrawingPriority>
        </DepthValues>)",
                uv_type)
                              .str();

            INFO(body);
            CHECK_FALSE(CompileEffekseerNodeBody(body).Binary.empty());
        }
    }

    SECTION("SoundSectionIsWrittenWhenEnabled")
    {
        for (int32_t sound_type : {0, 1}) {
            string body = strex(R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
        </DrawingValues>
        <SoundValues>
          <Type>{}</Type>
          <Sound>
            <Wave>NoSuchWave.wav</Wave>
            <Volume><Max>1</Max><Min>1</Min></Volume>
            <Pitch><Max>0</Max><Min>0</Min></Pitch>
            <PanType>0</PanType>
            <Pan><Max>0</Max><Min>0</Min></Pan>
            <Distance>10</Distance>
            <Delay><Max>0</Max><Min>0</Min></Delay>
          </Sound>
        </SoundValues>)",
                sound_type)
                              .str();

            INFO(body);
            EffekseerCompilerOutput compiled = CompileEffekseerNodeBody(body);
            CHECK_FALSE(compiled.Binary.empty());
        }
    }

    SECTION("SpriteColourTypes")
    {
        // Fixed, random and gradient colours each take their own writer
        string fixed_colour = R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
          <ColorAll><Type>0</Type><Fixed><R>10</R><G>20</G><B>30</B><A>255</A></Fixed></ColorAll>
        </DrawingValues>)";
        CHECK_FALSE(CompileEffekseerNodeBody(fixed_colour).Binary.empty());

        string random_colour = R"(        <DrawingValues>
          <Type>2</Type>
          <Sprite />
          <ColorAll><Type>1</Type><Random><R><Max>255</Max><Min>0</Min></R><G><Max>255</Max><Min>0</Min></G><B><Max>255</Max><Min>0</Min></B><A><Max>255</Max><Min>0</Min></A></Random></ColorAll>
        </DrawingValues>)";
        CHECK_FALSE(CompileEffekseerNodeBody(random_colour).Binary.empty());
    }
}

static auto MakeModelTrailFixture(int32_t life, int32_t count, float32_t interval,
    string_view location = "<X>0</X><Y>0</Y><Z>0</Z>", int32_t binding = 1, string_view node_values = {}) -> unique_ptr<EffekseerRuntimeTestRig>
{
    string project = strex(R"EFFEKSEER(<?xml version="1.0" encoding="utf-8"?>
<EffekseerProject>
  <Root><Name>Root</Name><Children><Node>
    <CommonValues>
      <MaxGeneration><Value>{}</Value></MaxGeneration>
      <Life><Max>{}</Max><Min>{}</Min></Life>
      <Generation><GenerationTime><Max>{}</Max><Min>{}</Min></GenerationTime></Generation>
      <LocationEffectType>{}</LocationEffectType>
      <RotationEffectType>{}</RotationEffectType>
      <ScaleEffectType>{}</ScaleEffectType>
    </CommonValues>
    <LocationValues><Type>0</Type><Fixed><Location>{}</Location></Fixed></LocationValues>
    <RendererCommonValues><AlphaBlend>1</AlphaBlend></RendererCommonValues>
    <DrawingValues><Type>2</Type><Sprite>
      <Billboard>2</Billboard><Position>1</Position>
      <Position_Fixed_LL><X>-0.25</X><Y>-0.25</Y></Position_Fixed_LL>
      <Position_Fixed_LR><X>0.25</X><Y>-0.25</Y></Position_Fixed_LR>
      <Position_Fixed_UL><X>-0.25</X><Y>0.25</Y></Position_Fixed_UL>
      <Position_Fixed_UR><X>0.25</X><Y>0.25</Y></Position_Fixed_UR>
    </Sprite></DrawingValues>
    <Name>ModelTrail</Name><Children />
  </Node></Children></Root>
  <ToolVersion>1.80.5</ToolVersion><Version>3</Version>
  <StartFrame>0</StartFrame><EndFrame>60</EndFrame><IsLoop>False</IsLoop>
</EffekseerProject>
)EFFEKSEER", count, life, life, interval, interval, binding, binding, binding, location).str();

    if (!node_values.empty()) {
        size_t drawing_position = project.find("<DrawingValues>");
        REQUIRE(drawing_position != string::npos);
        project.insert(drawing_position, node_values);
    }

    EffekseerCompilerOutput compiled = CompileEffekseerProject("Particles/EffekseerTests/ModelTrail.efkproj", {reinterpret_cast<const uint8_t*>(project.data()), project.size()});

    return safe_alloc::make_unique<EffekseerRuntimeTestRig>("Particles/EffekseerTests/ModelTrail.efk", std::move(compiled.Binary));
}

static auto DrawModelTrail(EffekseerRuntimeTestRig& rig, ParticleRuntimeSystem& system) -> vector<CapturedEffekseerDraw>
{
    rig.ClearDraws();
    system.Draw();

    return rig.GetDraws();
}

static void CheckLiveBoundsContainDraws(const ParticleBounds3D& bounds, const vector<CapturedEffekseerDraw>& draws)
{
    REQUIRE(!draws.empty());

    for (const CapturedEffekseerDraw& draw : draws) {
        for (const Vertex2D& vertex : draw.Vertices) {
            CHECK(vertex.PosX >= bounds.PositionMin.x - bounds.BillboardRadius - 0.001f);
            CHECK(vertex.PosX <= bounds.PositionMax.x + bounds.BillboardRadius + 0.001f);
            CHECK(vertex.PosY >= bounds.PositionMin.y - bounds.BillboardRadius - 0.001f);
            CHECK(vertex.PosY <= bounds.PositionMax.y + bounds.BillboardRadius + 0.001f);
            CHECK(vertex.PosZ >= bounds.PositionMin.z - bounds.BillboardRadius - 0.001f);
            CHECK(vertex.PosZ <= bounds.PositionMax.z + bounds.BillboardRadius + 0.001f);
        }
    }
}

TEST_CASE("Effekseer model placement accepts facing hints without rotating the bone pose twice", "[particle][effekseer-runtime][model-particle-integration]")
{
    for (float32_t facing : {0.0f, 60.0f, 120.0f, 180.0f, 240.0f, 300.0f}) {
        for (float32_t scale : {1.0f, 2.0f}) {
            CAPTURE(facing, scale);

            unique_ptr<EffekseerRuntimeTestRig> rig = MakeModelTrailFixture(30, 1, 1.0f, "<X>2</X><Y>3</Y><Z>4</Z>");
            unique_ptr<ParticleRuntimeSystem> system = rig->CreateSystem();
            ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
            setup.World = glm::translate(mat44 {1.0f}, vec3 {7.0f, 11.0f, 13.0f}) * glm::rotate(mat44 {1.0f}, 90.0f * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f});
            setup.PositionOffset = {0.25f, 0.5f, 0.75f};
            setup.LookDirectionAngle = facing;
            setup.Scale = scale;
            system->Setup(setup);
            system->Respawn(541);

            REQUIRE(system->IsActive());

            system->Update(1.0f / 60.0f);
            vector<CapturedEffekseerDraw> draws = DrawModelTrail(*rig, *system);
            REQUIRE(draws.size() == 1);
            REQUIRE(draws.front().Vertices.size() == 4);

            vec3 center {};

            for (const Vertex2D& vertex : draws.front().Vertices) {
                center += vec3 {vertex.PosX, vertex.PosY, vertex.PosZ} / 4.0f;
            }

            vec3 expected = scale == 1.0f ? vec3 {11.75f, 14.5f, 10.75f} : vec3 {15.75f, 17.5f, 8.75f};
            CHECK(center.x == Catch::Approx(expected.x).margin(0.001f));
            CHECK(center.y == Catch::Approx(expected.y).margin(0.001f));
            CHECK(center.z == Catch::Approx(expected.z).margin(0.001f));
        }
    }
}

TEST_CASE("Effekseer model bounds include old world particles and release their expired envelope", "[particle][effekseer-runtime][model-particle-integration]")
{
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeModelTrailFixture(6, 2, 4.0f);
    unique_ptr<ParticleRuntimeSystem> system = rig->CreateSystem();
    ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
    setup.Scale = 4.0f;
    system->Setup(setup);
    system->Respawn(541);
    system->Update(1.0f / 60.0f);
    vector<CapturedEffekseerDraw> first = DrawModelTrail(*rig, *system);
    REQUIRE(first.size() == 1);
    REQUIRE(first.front().Vertices.size() == 4);

    setup.World = glm::translate(mat44 {1.0f}, vec3 {100.0f, 0.0f, 0.0f});
    setup.Scale = 0.25f;
    system->Setup(setup);
    system->Update(4.0f / 60.0f);
    vector<CapturedEffekseerDraw> both = DrawModelTrail(*rig, *system);
    REQUIRE(both.size() == 1);
    REQUIRE(both.front().Vertices.size() == 8);

    bool near_origin = false;
    bool near_moved = false;

    for (const Vertex2D& vertex : both.front().Vertices) {
        near_origin |= std::abs(vertex.PosX) < 2.0f;
        near_moved |= std::abs(vertex.PosX - 100.0f) < 2.0f;
    }

    REQUIRE(near_origin);
    REQUIRE(near_moved);

    rig->ClearDraws();
    optional<ParticleBounds3D> live = system->GetLiveBounds();
    REQUIRE(live);
    CHECK(rig->GetDraws().empty());

    CheckLiveBoundsContainDraws(*live, both);

    CHECK(live->BillboardRadius >= 1.4f);
    CHECK(live->BillboardRadius < 2.1f);

    system->Update(3.0f / 60.0f);
    vector<CapturedEffekseerDraw> last = DrawModelTrail(*rig, *system);
    REQUIRE(last.size() == 1);
    REQUIRE(last.front().Vertices.size() == 4);

    live = system->GetLiveBounds();

    REQUIRE(live);
    CHECK(live->PositionMin.x - live->BillboardRadius > 50.0f);

    for (int32_t frame = 0; frame < 20; frame++) {
        system->Update(1.0f / 60.0f);
    }

    CHECK_FALSE(system->GetLiveBounds());
}

TEST_CASE("Effekseer atlas rebases move each live particle once without advancing generation", "[particle][effekseer-runtime][model-particle-integration]")
{
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeModelTrailFixture(30, 2, 4.0f);
    unique_ptr<ParticleRuntimeSystem> system = rig->CreateSystem();
    ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
    system->Setup(setup);
    system->Respawn(541);
    system->Update(1.0f / 60.0f);
    vec3 total {};

    for (vec3 delta : {vec3 {10.0f, 20.0f, 30.0f}, vec3 {-3.0f, 4.0f, 7.0f}, vec3 {1.0f, -2.0f, 3.0f}, vec3 {-1.0f, 2.0f, -3.0f}}) {
        total += delta;
        system->RebaseWorldParticles(delta);
        setup.World = glm::translate(mat44 {1.0f}, total);
        system->Setup(setup);
        system->RefreshRenderTransform();
        vector<CapturedEffekseerDraw> draws = DrawModelTrail(*rig, *system);
        REQUIRE(draws.size() == 1);
        REQUIRE(draws.front().Vertices.size() == 4);

        vec3 center {};

        for (const Vertex2D& vertex : draws.front().Vertices) {
            center += vec3 {vertex.PosX, vertex.PosY, vertex.PosZ} / 4.0f;
        }

        CHECK(center.x == Catch::Approx(total.x).margin(0.001f));
        CHECK(center.y == Catch::Approx(total.y).margin(0.001f));
        CHECK(center.z == Catch::Approx(total.z).margin(0.001f));

        optional<ParticleBounds3D> live = system->GetLiveBounds();
        REQUIRE(live);

        CheckLiveBoundsContainDraws(*live, draws);
        system->Setup(setup);
        system->RefreshRenderTransform();
        CheckEffekseerDrawsEqual(draws, DrawModelTrail(*rig, *system));
    }

    system->Respawn(541);
    system->Update(1.0f / 60.0f);
    vector<CapturedEffekseerDraw> restarted = DrawModelTrail(*rig, *system);
    REQUIRE(restarted.size() == 1);
    REQUIRE(restarted.front().Vertices.size() == 4);

    vec3 center {};

    for (const Vertex2D& vertex : restarted.front().Vertices) {
        center += vec3 {vertex.PosX, vertex.PosY, vertex.PosZ} / 4.0f;
    }

    CHECK(center.x == Catch::Approx(total.x).margin(0.001f));
    CHECK(center.y == Catch::Approx(total.y).margin(0.001f));
    CHECK(center.z == Catch::Approx(total.z).margin(0.001f));
}

TEST_CASE("Effekseer live bounds preserve a valid static envelope and shared draw renderers", "[particle][effekseer-runtime][model-particle-integration]")
{
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeModelTrailFixture(30, 1, 1.0f);
    unique_ptr<ParticleRuntimeSystem> first = rig->CreateSystem();
    unique_ptr<ParticleRuntimeSystem> second = rig->CreateSystem();
    ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
    setup.World = glm::translate(mat44 {1.0f}, vec3 {3.0f, 4.0f, 5.0f});
    setup.Scale = 2.0f;
    first->Setup(setup);
    first->Respawn(541);
    first->Update(1.0f / 60.0f);
    setup.World = glm::translate(mat44 {1.0f}, vec3 {100.0f, 0.0f, 0.0f});
    second->Setup(setup);
    second->Respawn(542);
    second->Update(1.0f / 60.0f);
    vector<CapturedEffekseerDraw> second_before = DrawModelTrail(*rig, *second);
    REQUIRE(!second_before.empty());

    rig->ClearDraws();
    optional<ParticleBounds3D> live = first->GetLiveBounds();
    REQUIRE(live);
    CHECK(live->PositionMin == vec3 {1.0f, 2.0f, 3.0f});
    CHECK(live->PositionMax == vec3 {5.0f, 6.0f, 7.0f});
    CHECK(live->BillboardRadius == Catch::Approx(1.0f));

    optional<ParticleBounds3D> baked = first->GetBakedBounds();
    REQUIRE(baked);
    CHECK(baked->PositionMin == vec3 {-1.0f, -1.0f, -1.0f});
    CHECK(baked->PositionMax == vec3 {1.0f, 1.0f, 1.0f});
    CHECK(baked->BillboardRadius == Catch::Approx(0.5f));
    CHECK(rig->GetDraws().empty());

    CheckEffekseerDrawsEqual(second_before, DrawModelTrail(*rig, *second));
}

TEST_CASE("Effekseer rejects nonfinite hints and ignores invalid rebases without mutation", "[particle][effekseer-runtime][model-particle-integration]")
{
    for (float32_t hint : {std::numeric_limits<float32_t>::quiet_NaN(), std::numeric_limits<float32_t>::infinity()}) {
        unique_ptr<EffekseerRuntimeTestRig> rig = MakeModelTrailFixture(30, 1, 1.0f);
        unique_ptr<ParticleRuntimeSystem> system = rig->CreateSystem();
        ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
        setup.LookDirectionAngle = hint;
        system->Setup(setup);
        system->Respawn(541);

        CHECK_FALSE(system->IsActive());
    }

    unique_ptr<EffekseerRuntimeTestRig> rig = MakeModelTrailFixture(30, 2, 4.0f);
    unique_ptr<ParticleRuntimeSystem> system = rig->CreateSystem();
    system->Setup(MakeEffekseerIdentitySetup());
    system->Respawn(541);
    system->Update(1.0f / 60.0f);
    vector<CapturedEffekseerDraw> before = DrawModelTrail(*rig, *system);
    REQUIRE(before.size() == 1);
    REQUIRE(before.front().Vertices.size() == 4);

    for (vec3 delta : {vec3 {std::numeric_limits<float32_t>::quiet_NaN(), 0.0f, 0.0f}, vec3 {0.0f, std::numeric_limits<float32_t>::infinity(), 0.0f}}) {
        system->RebaseWorldParticles(delta);
        system->RefreshRenderTransform();

        REQUIRE(system->IsActive());

        CheckEffekseerDrawsEqual(before, DrawModelTrail(*rig, *system));
    }
}

TEST_CASE("Effekseer bounds queries do not acquire a distortion scene background", "[particle][effekseer-runtime][model-particle-integration]")
{
    unique_ptr<EffekseerRuntimeTestRig> rig = MakeDistortionFixtureRig(1.0f, 1);
    rig->SetSceneBackgroundMode(TestSceneBackgroundMode::Unavailable);
    unique_ptr<ParticleRuntimeSystem> system = rig->CreateSystem();
    system->Setup(MakeEffekseerIdentitySetup());
    system->Respawn(541);
    system->Update(1.0f / 60.0f);
    optional<ParticleBounds3D> live = system->GetLiveBounds();
    REQUIRE(live);
    REQUIRE(system->IsActive());
    CHECK(rig->GetDraws().empty());

    live = system->GetLiveBounds();

    REQUIRE(live);

    rig->SetSceneBackgroundMode(TestSceneBackgroundMode::Available);
    vector<CapturedEffekseerDraw> draws = DrawModelTrail(*rig, *system);
    REQUIRE(!draws.empty());
    CHECK(draws.front().HasBackgroundTexture);
}

TEST_CASE("Effekseer live bounds measure each supported renderer family", "[particle][effekseer-runtime][model-particle-integration]")
{
    for (int32_t type : {2, 3, 4, 5, 6}) {
        CAPTURE(type);

        string drawing;

        switch (type) {
        case 2:
            drawing = "<Type>2</Type><Sprite />";
            break;
        case 3:
            drawing = "<Type>3</Type><Ribbon><ViewpointDependent>False</ViewpointDependent></Ribbon>";
            break;
        case 4:
            drawing = "<Type>4</Type><Ring><Billboard>2</Billboard><VertexCount>16</VertexCount></Ring>";
            break;
        case 5:
            drawing = "<Type>5</Type><Model><Model>Model/Fixture.efkmodel</Model><Billboard>2</Billboard><Culling>2</Culling></Model>";
            break;
        case 6:
            drawing = "<Type>6</Type><TrailSmoothing>0</TrailSmoothing><Track />";
            break;
        }

        string project = strex(ParticleTests::StripProjectTemplate, drawing).str();
        size_t drawing_position = project.find("<DrawingValues>");

        REQUIRE(drawing_position != string::npos);

        project.insert(drawing_position, "<ScalingValues><Type>0</Type><Fixed><Scale><X>16</X><Y>16</Y><Z>16</Z></Scale></Fixed></ScalingValues>");
        EffekseerCompilerOutput compiled = CompileEffekseerProject("Particles/EffekseerTests/ShapeBounds.efkproj", {reinterpret_cast<const uint8_t*>(project.data()), project.size()});
        map<string, vector<uint8_t>> dependencies;
        dependencies.emplace("Particles/EffekseerTests/Model/Fixture.efkmodel", ParticleTests::MakeFixtureModelPayload());
        EffekseerRuntimeTestRig rig {"Particles/EffekseerTests/ShapeBounds.efk", std::move(compiled.Binary), dependencies};
        unique_ptr<ParticleRuntimeSystem> system = rig.CreateSystem();
        ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
        setup.Scale = 0.25f;
        system->Setup(setup);
        system->Respawn(541);

        for (int32_t frame = 0; frame < 6; frame++) {
            system->Update(1.0f / 60.0f);
        }

        optional<ParticleBounds3D> live = system->GetLiveBounds();
        REQUIRE(live);
        CHECK(rig.GetDraws().empty());

        vector<CapturedEffekseerDraw> draws = DrawModelTrail(rig, *system);
        REQUIRE(!draws.empty());
        REQUIRE(!draws.front().Vertices.empty());

        CheckLiveBoundsContainDraws(*live, draws);

        CHECK(live->BillboardRadius > 1.0f);
    }
}

TEST_CASE("Effekseer bounds queries preserve future seeded generations", "[particle][effekseer-runtime][model-particle-integration]")
{
    EffekseerRuntimeTestRig queried_rig;
    EffekseerRuntimeTestRig control_rig;
    unique_ptr<ParticleRuntimeSystem> queried = queried_rig.CreateSystem();
    unique_ptr<ParticleRuntimeSystem> control = control_rig.CreateSystem();
    queried->Setup(MakeEffekseerIdentitySetup());
    control->Setup(MakeEffekseerIdentitySetup());
    queried->Respawn(173);
    control->Respawn(173);

    for (int32_t frame = 0; frame < 12; frame++) {
        queried->Update(1.0f / 60.0f);
        control->Update(1.0f / 60.0f);
        optional<ParticleBounds3D> first_query = queried->GetLiveBounds();
        optional<ParticleBounds3D> second_query = queried->GetLiveBounds();
        REQUIRE(first_query);
        REQUIRE(second_query);
        CHECK(queried_rig.GetDraws().empty());
    }

    vector<CapturedEffekseerDraw> queried_draws = DrawModelTrail(queried_rig, *queried);
    vector<CapturedEffekseerDraw> control_draws = DrawModelTrail(control_rig, *control);
    REQUIRE(!queried_draws.empty());

    CheckEffekseerDrawsEqual(queried_draws, control_draws);
}

static void ApplyEffekseerRefreshOrZero(ParticleRuntimeSystem& system, const ParticleRuntimeSetup& setup, bool use_refresh)
{
    system.Setup(setup);

    if (use_refresh) {
        system.RefreshRenderTransform();
    }
    else {
        system.Update(0.0f);
    }
}

static auto DrawAndCompareEffekseerPair(EffekseerRuntimeTestRig& queried_rig, EffekseerRuntimeTestRig& control_rig, ParticleRuntimeSystem& queried, ParticleRuntimeSystem& control) -> vector<CapturedEffekseerDraw>
{
    vector<CapturedEffekseerDraw> queried_draws = DrawModelTrail(queried_rig, queried);
    vector<CapturedEffekseerDraw> control_draws = DrawModelTrail(control_rig, control);
    REQUIRE(!queried_draws.empty());
    CheckEffekseerDrawsEqual(queried_draws, control_draws);
    return queried_draws;
}

TEST_CASE("Effekseer unchanged repeated refresh preserves seeded draws and expiration", "[particle][effekseer-runtime][model-particle-integration][effekseer-refresh-contract]")
{
    unique_ptr<EffekseerRuntimeTestRig> queried_rig = MakeModelTrailFixture(6, 4, 2.0f);
    unique_ptr<EffekseerRuntimeTestRig> control_rig = MakeModelTrailFixture(6, 4, 2.0f);
    unique_ptr<ParticleRuntimeSystem> queried = queried_rig->CreateSystem();
    unique_ptr<ParticleRuntimeSystem> control = control_rig->CreateSystem();
    ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
    queried->Setup(setup);
    control->Setup(setup);
    queried->Respawn(901);
    control->Respawn(901);

    for (int32_t frame = 0; frame < 8; frame++) {
        queried->Update(1.0f / 60.0f);
        control->Update(1.0f / 60.0f);

        for (int32_t repeat = 0; repeat < 3; repeat++) {
            ApplyEffekseerRefreshOrZero(*queried, setup, true);
            ApplyEffekseerRefreshOrZero(*control, setup, false);
        }

        DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried, *control);
    }

    bool both_went_inactive = false;

    for (int32_t frame = 0; frame < 30; frame++) {
        queried->Update(1.0f / 60.0f);
        control->Update(1.0f / 60.0f);
        queried->RefreshRenderTransform();
        control->Update(0.0f);
        CHECK(queried->IsActive() == control->IsActive());

        if (!queried->IsActive()) {
            both_went_inactive = true;
            break;
        }
    }

    REQUIRE(both_went_inactive);
    CHECK_FALSE(queried->IsActive());
    CHECK_FALSE(control->IsActive());

    // A clean flag retained across a new handle would skip the zero-time work these first-draw packets prove
    queried->Respawn(902);
    control->Respawn(902);
    ApplyEffekseerRefreshOrZero(*queried, setup, true);
    ApplyEffekseerRefreshOrZero(*control, setup, false);
    DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried, *control);
}

TEST_CASE("Effekseer transform and rebase refresh match explicit-zero control packets", "[particle][effekseer-runtime][model-particle-integration][effekseer-refresh-contract]")
{
    unique_ptr<EffekseerRuntimeTestRig> queried_rig = MakeModelTrailFixture(6, 4, 2.0f, "<X>0</X><Y>0</Y><Z>0</Z>", 0);
    unique_ptr<EffekseerRuntimeTestRig> control_rig = MakeModelTrailFixture(6, 4, 2.0f, "<X>0</X><Y>0</Y><Z>0</Z>", 0);
    unique_ptr<ParticleRuntimeSystem> queried = queried_rig->CreateSystem();
    unique_ptr<ParticleRuntimeSystem> control = control_rig->CreateSystem();
    ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
    queried->Setup(setup);
    control->Setup(setup);
    queried->Respawn(733);
    control->Respawn(733);
    queried->Update(1.0f / 60.0f);
    control->Update(1.0f / 60.0f);

    setup.World = glm::translate(mat44 {1.0f}, vec3 {12.0f, -4.0f, 7.0f});
    ApplyEffekseerRefreshOrZero(*queried, setup, true);
    ApplyEffekseerRefreshOrZero(*control, setup, false);
    DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried, *control);

    setup.Scale = 1.75f;
    ApplyEffekseerRefreshOrZero(*queried, setup, true);
    ApplyEffekseerRefreshOrZero(*control, setup, false);
    DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried, *control);

    setup.PositionOffset = {0.5f, -0.25f, 0.75f};
    ApplyEffekseerRefreshOrZero(*queried, setup, true);
    ApplyEffekseerRefreshOrZero(*control, setup, false);
    DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried, *control);

    setup.ViewOffset = {-1.5f, 0.5f, 0.25f};
    ApplyEffekseerRefreshOrZero(*queried, setup, true);
    ApplyEffekseerRefreshOrZero(*control, setup, false);
    DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried, *control);

    // Projection matrix contents are not part of the captured packet; this stage only keeps the refresh path
    // equivalent when a camera-only field changes
    setup.Projection = glm::scale(mat44 {1.0f}, vec3 {1.5f, 0.5f, 2.0f});
    ApplyEffekseerRefreshOrZero(*queried, setup, true);
    ApplyEffekseerRefreshOrZero(*control, setup, false);
    DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried, *control);

    setup.ViewOffset = {};
    setup.Projection = mat44 {1.0f};
    vec3 rebase_total {};

    for (vec3 delta : {vec3 {10.0f, 20.0f, 30.0f}, vec3 {-3.0f, 4.0f, 7.0f}, vec3 {1.0f, -2.0f, 3.0f}}) {
        rebase_total += delta;
        queried->RebaseWorldParticles(delta);
        control->RebaseWorldParticles(delta);
        setup.World = glm::translate(mat44 {1.0f}, rebase_total);
        ApplyEffekseerRefreshOrZero(*queried, setup, true);
        ApplyEffekseerRefreshOrZero(*control, setup, false);
        vector<CapturedEffekseerDraw> draws = DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried, *control);

        // Bounds and captured vertices use the same view space in this stage
        optional<ParticleBounds3D> live = queried->GetLiveBounds();
        REQUIRE(live);
        CheckLiveBoundsContainDraws(*live, draws);

        ApplyEffekseerRefreshOrZero(*queried, setup, true);
        ApplyEffekseerRefreshOrZero(*control, setup, false);
        CheckEffekseerDrawsEqual(draws, DrawModelTrail(*queried_rig, *queried));
    }
}

TEST_CASE("Effekseer shared-manager identities track their own refresh controls", "[particle][effekseer-runtime][model-particle-integration][effekseer-refresh-contract]")
{
    unique_ptr<EffekseerRuntimeTestRig> queried_rig = MakeModelTrailFixture(6, 4, 2.0f);
    unique_ptr<EffekseerRuntimeTestRig> control_rig = MakeModelTrailFixture(6, 4, 2.0f);
    unique_nptr<ParticleRuntimeSystem> queried_first = queried_rig->CreateSystem();
    unique_ptr<ParticleRuntimeSystem> queried_second = queried_rig->CreateSystem();
    unique_nptr<ParticleRuntimeSystem> control_first = control_rig->CreateSystem();
    unique_ptr<ParticleRuntimeSystem> control_second = control_rig->CreateSystem();
    ParticleRuntimeSetup first_setup = MakeEffekseerIdentitySetup();
    first_setup.World = glm::translate(mat44 {1.0f}, vec3 {3.0f, 0.0f, -2.0f});
    ParticleRuntimeSetup second_setup = MakeEffekseerIdentitySetup();
    second_setup.World = glm::translate(mat44 {1.0f}, vec3 {40.0f, 5.0f, 8.0f});
    queried_first->Setup(first_setup);
    control_first->Setup(first_setup);
    queried_first->Respawn(541);
    control_first->Respawn(541);
    queried_second->Setup(second_setup);
    control_second->Setup(second_setup);
    queried_second->Respawn(542);
    control_second->Respawn(542);

    for (int32_t frame = 0; frame < 8; frame++) {
        bool first_turn = frame % 2 == 0;

        if (first_turn) {
            ApplyEffekseerRefreshOrZero(*queried_first, first_setup, true);
            ApplyEffekseerRefreshOrZero(*control_first, first_setup, false);
            ApplyEffekseerRefreshOrZero(*queried_second, second_setup, true);
            ApplyEffekseerRefreshOrZero(*control_second, second_setup, false);
        }
        else {
            ApplyEffekseerRefreshOrZero(*queried_second, second_setup, true);
            ApplyEffekseerRefreshOrZero(*control_second, second_setup, false);
            ApplyEffekseerRefreshOrZero(*queried_first, first_setup, true);
            ApplyEffekseerRefreshOrZero(*control_first, first_setup, false);
        }

        queried_first->Update(1.0f / 60.0f);
        control_first->Update(1.0f / 60.0f);
        queried_second->Update(1.0f / 60.0f);
        control_second->Update(1.0f / 60.0f);

        vector<CapturedEffekseerDraw> queried_first_draws;
        vector<CapturedEffekseerDraw> queried_second_draws;

        if (first_turn) {
            queried_first_draws = DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried_first, *control_first);
            queried_second_draws = DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried_second, *control_second);
        }
        else {
            queried_second_draws = DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried_second, *control_second);
            queried_first_draws = DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried_first, *control_first);
        }

        // Distinct world placements keep the two identities' packets apart on the shared manager
        REQUIRE(!queried_first_draws.front().Vertices.empty());
        REQUIRE(!queried_second_draws.front().Vertices.empty());
        CHECK(queried_first_draws.front().Vertices.front().PosX != queried_second_draws.front().Vertices.front().PosX);
    }

    queried_first.reset();
    control_first.reset();
    DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried_second, *control_second);

    {
        unique_ptr<ParticleRuntimeSystem> pending = queried_rig->CreateSystem();
        unique_ptr<ParticleRuntimeSystem> control_pending = control_rig->CreateSystem();
        pending->Setup(first_setup);
        control_pending->Setup(first_setup);
        pending->Respawn(903);
        control_pending->Respawn(903);

        // A neighbour's refresh publishes the new root and its bounds before the handle's own first update
        queried_second->RefreshRenderTransform();
        control_second->Update(0.0f);
        optional<ParticleBounds3D> control_pending_bounds = control_pending->GetLiveBounds();
        optional<ParticleBounds3D> pending_bounds = pending->GetLiveBounds();
        REQUIRE(control_pending_bounds);
        REQUIRE(pending_bounds);
        CHECK(pending_bounds->PositionMin == control_pending_bounds->PositionMin);
        CHECK(pending_bounds->PositionMax == control_pending_bounds->PositionMax);
        CHECK(pending_bounds->BillboardRadius == control_pending_bounds->BillboardRadius);

        pending->RefreshRenderTransform();
        control_pending->Update(0.0f);
        DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *pending, *control_pending);
    }

    bool second_expired = false;

    for (int32_t frame = 0; frame < 30; frame++) {
        queried_second->Update(1.0f / 60.0f);
        control_second->Update(1.0f / 60.0f);
        queried_second->RefreshRenderTransform();
        control_second->Update(0.0f);
        CHECK(queried_second->IsActive() == control_second->IsActive());

        if (!queried_second->IsActive()) {
            second_expired = true;
            break;
        }
    }

    REQUIRE(second_expired);
    CHECK_FALSE(control_second->IsActive());

    // A new owner for the first identity must produce first-draw geometry before any nonzero tick
    unique_ptr<ParticleRuntimeSystem> respawned_first = queried_rig->CreateSystem();
    unique_ptr<ParticleRuntimeSystem> control_respawned_first = control_rig->CreateSystem();
    respawned_first->Setup(first_setup);
    control_respawned_first->Setup(first_setup);
    respawned_first->Respawn(777);
    control_respawned_first->Respawn(777);
    ApplyEffekseerRefreshOrZero(*respawned_first, first_setup, true);
    ApplyEffekseerRefreshOrZero(*control_respawned_first, first_setup, false);
    DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *respawned_first, *control_respawned_first);
}

TEST_CASE("Effekseer expired prewarm retires on its first render refresh", "[particle][effekseer-runtime][effekseer-refresh-contract]")
{
    unique_ptr<EffekseerRuntimeTestRig> queried_rig = MakeModelTrailFixture(6, 4, 2.0f);
    unique_ptr<EffekseerRuntimeTestRig> control_rig = MakeModelTrailFixture(6, 4, 2.0f);
    unique_ptr<ParticleRuntimeSystem> queried = queried_rig->CreateSystem();
    unique_ptr<ParticleRuntimeSystem> control = control_rig->CreateSystem();
    ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
    queried->Setup(setup);
    control->Setup(setup);
    queried->Respawn(904);
    control->Respawn(904);
    CHECK(queried->Prewarm() == control->Prewarm());

    queried->RefreshRenderTransform();
    control->Update(0.0f);
    CHECK_FALSE(control->IsActive());
    CHECK_FALSE(queried->IsActive());
}

TEST_CASE("Effekseer neighbour refresh publishes rebased kill-rule coordinates", "[particle][effekseer-runtime][model-particle-integration][effekseer-refresh-contract]")
{
    for (bool transformed_rules : {false, true}) {
        CAPTURE(transformed_rules);
        string rules = strex(R"(<RotationValues><Type>6</Type></RotationValues>
<KillRulesValues><Type>3</Type>
<IsScaleAndRotationApplied>{}</IsScaleAndRotationApplied>
<SphereRadius>1</SphereRadius><SphereIsKillInside>False</SphereIsKillInside>
</KillRulesValues>)", transformed_rules ? "True" : "False").str();
        unique_ptr<EffekseerRuntimeTestRig> queried_rig = MakeModelTrailFixture(30, 1, 1.0f, "<X>0.25</X><Y>0</Y><Z>0</Z>", 1, rules);
        unique_ptr<EffekseerRuntimeTestRig> control_rig = MakeModelTrailFixture(30, 1, 1.0f, "<X>0.25</X><Y>0</Y><Z>0</Z>", 1, rules);
        unique_ptr<ParticleRuntimeSystem> queried = queried_rig->CreateSystem();
        unique_ptr<ParticleRuntimeSystem> neighbour = queried_rig->CreateSystem();
        unique_ptr<ParticleRuntimeSystem> control = control_rig->CreateSystem();
        unique_ptr<ParticleRuntimeSystem> control_neighbour = control_rig->CreateSystem();
        ParticleRuntimeSetup setup = MakeEffekseerIdentitySetup();
        setup.World = glm::translate(mat44 {1.0f}, vec3 {0.0f, 0.0f, -5.0f});

        array<ptr<ParticleRuntimeSystem>, 4> systems {queried, neighbour, control, control_neighbour};

        for (ptr<ParticleRuntimeSystem> system : systems) {
            system->Setup(setup);
            system->Respawn(905);
            system->Update(1.0f / 60.0f);
        }

        DrawAndCompareEffekseerPair(*queried_rig, *control_rig, *queried, *control);

        vec3 delta {10.0f, 0.0f, 0.0f};
        queried->RebaseWorldParticles(delta);
        control->RebaseWorldParticles(delta);
        neighbour->RefreshRenderTransform();
        control_neighbour->Update(0.0f);

        setup.World = glm::translate(mat44 {1.0f}, delta) * setup.World;
        queried->Setup(setup);
        control->Setup(setup);
        queried->RefreshRenderTransform();
        control->Update(0.0f);

        vector<CapturedEffekseerDraw> control_draws = DrawModelTrail(*control_rig, *control);
        vector<CapturedEffekseerDraw> queried_draws = DrawModelTrail(*queried_rig, *queried);

        // The SDK's inverse matrix stays in compensated simulation space; translation-only rules use the rendered origin
        if (transformed_rules) {
            REQUIRE(!control_draws.empty());
            CheckEffekseerDrawsEqual(queried_draws, control_draws);
        }
        else {
            REQUIRE(control_draws.empty());
            CHECK(queried_draws.empty());
        }
    }
}

TEST_CASE("Effekseer decomposes collapsed diagonal scales into finite canonical frames", "[particle][effekseer-runtime][effekseer-singular-transform]")
{
    const array<vec3, 8> scales {
        vec3 {0.0f, 2.0f, 3.0f}, vec3 {1.0f, 0.0f, 3.0f},
        vec3 {1.0f, 2.0f, 0.0f}, vec3 {0.0f, 0.0f, 3.0f},
        vec3 {0.0f, 2.0f, 0.0f}, vec3 {1.0f, 0.0f, 0.0f},
        vec3 {0.0f, 0.0f, 0.0f}, vec3 {1.0f, 2.0f, 3.0f},
    };
    const array<float32_t, 12> identity {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };

    const auto check_identity = [&](const Effekseer::SIMD::Mat43f& rotation) {
        const array<float32_t, 12> values {
            rotation.X.GetX(), rotation.X.GetY(), rotation.X.GetZ(), rotation.X.GetW(),
            rotation.Y.GetX(), rotation.Y.GetY(), rotation.Y.GetZ(), rotation.Y.GetW(),
            rotation.Z.GetX(), rotation.Z.GetY(), rotation.Z.GetZ(), rotation.Z.GetW(),
        };

        for (size_t index = 0; index < values.size(); index++) {
            CHECK(std::isfinite(values[index]));
            CHECK(values[index] == Catch::Approx(identity[index]).margin(StripWidthTolerance));
        }
    };

    for (const vec3& scale : scales) {
        CAPTURE(scale.x, scale.y, scale.z);

        const Effekseer::SIMD::Mat43f matrix {
            scale.x, 0.0f, 0.0f,
            0.0f, scale.y, 0.0f,
            0.0f, 0.0f, scale.z,
            3.0f, 4.0f, 5.0f,
        };
        Effekseer::SIMD::Vec3f extracted_scale;
        Effekseer::SIMD::Mat43f rotation;
        Effekseer::SIMD::Vec3f translation;
        matrix.GetSRT(extracted_scale, rotation, translation);

        CHECK(extracted_scale.GetX() == Catch::Approx(scale.x).margin(StripWidthTolerance));
        CHECK(extracted_scale.GetY() == Catch::Approx(scale.y).margin(StripWidthTolerance));
        CHECK(extracted_scale.GetZ() == Catch::Approx(scale.z).margin(StripWidthTolerance));
        CHECK(translation.GetX() == Catch::Approx(3.0f));
        CHECK(translation.GetY() == Catch::Approx(4.0f));
        CHECK(translation.GetZ() == Catch::Approx(5.0f));

        check_identity(rotation);
        check_identity(matrix.GetRotation());
    }
}

TEST_CASE("Effekseer singular SRT decomposition preserves surviving rotated axes", "[particle][effekseer-runtime][effekseer-singular-transform]")
{
    const array<vec3, 12> scales {
        vec3 {0.0f, 2.0f, 3.0f}, vec3 {1.0f, 0.0f, 3.0f},
        vec3 {1.0f, 2.0f, 0.0f}, vec3 {0.0f, 0.0f, 3.0f},
        vec3 {0.0f, 2.0f, 0.0f}, vec3 {1.0f, 0.0f, 0.0f},
        vec3 {0.0f, 0.0f, 0.0f}, vec3 {1.0f, 2.0f, 3.0f},
        vec3 {-1.0f, 2.0f, 3.0f}, vec3 {1.0f, -2.0f, 3.0f},
        vec3 {0.0f, -2.0f, 3.0f}, vec3 {-1.0f, 0.0f, 0.0f},
    };

    for (const vec3& scale : scales) {
        CAPTURE(scale.x, scale.y, scale.z);

        const Effekseer::SIMD::Mat43f matrix {
            0.0f, scale.x, 0.0f,
            -scale.y, 0.0f, 0.0f,
            0.0f, 0.0f, scale.z,
            3.0f, 4.0f, 5.0f,
        };
        Effekseer::SIMD::Vec3f extracted_scale;
        Effekseer::SIMD::Mat43f rotation;
        Effekseer::SIMD::Vec3f translation;
        matrix.GetSRT(extracted_scale, rotation, translation);

        CHECK(extracted_scale.GetX() == Catch::Approx(std::abs(scale.x)).margin(StripWidthTolerance));
        CHECK(extracted_scale.GetY() == Catch::Approx(std::abs(scale.y)).margin(StripWidthTolerance));
        CHECK(extracted_scale.GetZ() == Catch::Approx(std::abs(scale.z)).margin(StripWidthTolerance));

        const auto check_rotation = [](const Effekseer::SIMD::Mat43f& value) {
            const array<vec3, 3> axes {
                vec3 {value.X.GetX(), value.Y.GetX(), value.Z.GetX()},
                vec3 {value.X.GetY(), value.Y.GetY(), value.Z.GetY()},
                vec3 {value.X.GetZ(), value.Y.GetZ(), value.Z.GetZ()},
            };

            for (const vec3& axis : axes) {
                CHECK(std::isfinite(axis.x));
                CHECK(std::isfinite(axis.y));
                CHECK(std::isfinite(axis.z));
                CHECK(glm::dot(axis, axis) == Catch::Approx(1.0f).margin(StripWidthTolerance));
            }

            CHECK(glm::dot(axes[0], axes[1]) == Catch::Approx(0.0f).margin(StripWidthTolerance));
            CHECK(glm::dot(axes[0], axes[2]) == Catch::Approx(0.0f).margin(StripWidthTolerance));
            CHECK(glm::dot(axes[1], axes[2]) == Catch::Approx(0.0f).margin(StripWidthTolerance));
        };

        check_rotation(rotation);
        check_rotation(matrix.GetRotation());

        const Effekseer::SIMD::Mat43f reconstructed = Effekseer::SIMD::Mat43f::SRT(extracted_scale, rotation, translation);
        CHECK(reconstructed.X.GetX() == Catch::Approx(0.0f).margin(StripWidthTolerance));
        CHECK(reconstructed.X.GetY() == Catch::Approx(-scale.y).margin(StripWidthTolerance));
        CHECK(reconstructed.X.GetZ() == Catch::Approx(0.0f).margin(StripWidthTolerance));
        CHECK(reconstructed.Y.GetX() == Catch::Approx(scale.x).margin(StripWidthTolerance));
        CHECK(reconstructed.Y.GetY() == Catch::Approx(0.0f).margin(StripWidthTolerance));
        CHECK(reconstructed.Y.GetZ() == Catch::Approx(0.0f).margin(StripWidthTolerance));
        CHECK(reconstructed.Z.GetX() == Catch::Approx(0.0f).margin(StripWidthTolerance));
        CHECK(reconstructed.Z.GetY() == Catch::Approx(0.0f).margin(StripWidthTolerance));
        CHECK(reconstructed.Z.GetZ() == Catch::Approx(scale.z).margin(StripWidthTolerance));
        CHECK(reconstructed.X.GetW() == Catch::Approx(3.0f));
        CHECK(reconstructed.Y.GetW() == Catch::Approx(4.0f));
        CHECK(reconstructed.Z.GetW() == Catch::Approx(5.0f));
    }
}

TEST_CASE("Effekseer singular SRT decomposition preserves an independent 3D basis", "[particle][effekseer-runtime][effekseer-singular-transform]")
{
    // Independent orthonormal, right-handed basis: each row has unit length, the rows are pairwise perpendicular,
    // and cross(a, b) == c. Every coordinate is nonzero, unlike the identity and axis-aligned fixtures above
    const array<vec3, 3> authored_basis {
        vec3 {1.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f},
        vec3 {2.0f / 3.0f, 1.0f / 3.0f, -2.0f / 3.0f},
        vec3 {-2.0f / 3.0f, 2.0f / 3.0f, -1.0f / 3.0f},
    };

    // The constructor takes each authored row as one argument triplet, so these are exactly the scaled source rows
    const auto make_matrix = [](const vec3& scale) {
        return Effekseer::SIMD::Mat43f {
            scale.x * (1.0f / 3.0f), scale.x * (2.0f / 3.0f), scale.x * (2.0f / 3.0f),
            scale.y * (2.0f / 3.0f), scale.y * (1.0f / 3.0f), scale.y * (-2.0f / 3.0f),
            scale.z * (-2.0f / 3.0f), scale.z * (2.0f / 3.0f), scale.z * (-1.0f / 3.0f),
            3.0f, 4.0f, 5.0f,
        };
    };

    const auto get_basis_rows = [](const Effekseer::SIMD::Mat43f& rotation) {
        return array<vec3, 3> {
            vec3 {rotation.X.GetX(), rotation.Y.GetX(), rotation.Z.GetX()},
            vec3 {rotation.X.GetY(), rotation.Y.GetY(), rotation.Z.GetY()},
            vec3 {rotation.X.GetZ(), rotation.Y.GetZ(), rotation.Z.GetZ()},
        };
    };

    const auto check_frame = [](const array<vec3, 3>& axes, bool require_right_handed) {
        for (const vec3& axis : axes) {
            CHECK(std::isfinite(axis.x));
            CHECK(std::isfinite(axis.y));
            CHECK(std::isfinite(axis.z));
            CHECK(glm::dot(axis, axis) == Catch::Approx(1.0f).margin(StripWidthTolerance));
        }

        CHECK(glm::dot(axes[0], axes[1]) == Catch::Approx(0.0f).margin(StripWidthTolerance));
        CHECK(glm::dot(axes[0], axes[2]) == Catch::Approx(0.0f).margin(StripWidthTolerance));
        CHECK(glm::dot(axes[1], axes[2]) == Catch::Approx(0.0f).margin(StripWidthTolerance));

        // A singular input leaves missing axes underdetermined, so only their completion order is pinned
        if (require_right_handed) {
            CHECK(glm::dot(glm::cross(axes[0], axes[1]), axes[2]) == Catch::Approx(1.0f).margin(StripWidthTolerance));
        }
    };

    struct ScaleCase final
    {
        vec3 Scale {};
        bool Singular {};
    };

    // All seven singular zero/nonzero patterns, then fully nonzero positive and reflected controls. Negative
    // survivors cover every single-survivor axis and both positive and negative two-survivor scales
    const array<ScaleCase, 10> cases {
        ScaleCase {vec3 {0.0f, 0.0f, 0.0f}, true},
        ScaleCase {vec3 {0.0f, 0.0f, -3.0f}, true},
        ScaleCase {vec3 {0.0f, 2.0f, 0.0f}, true},
        ScaleCase {vec3 {-1.0f, 0.0f, 0.0f}, true},
        ScaleCase {vec3 {0.0f, -2.0f, 3.0f}, true},
        ScaleCase {vec3 {-1.0f, 0.0f, 3.0f}, true},
        ScaleCase {vec3 {1.0f, 2.0f, 0.0f}, true},
        ScaleCase {vec3 {1.0f, 2.0f, 3.0f}, false},
        ScaleCase {vec3 {-1.0f, 2.0f, 3.0f}, false},
        ScaleCase {vec3 {1.0f, -2.0f, -3.0f}, false},
    };

    for (const ScaleCase& scale_case : cases) {
        const vec3& scale = scale_case.Scale;
        CAPTURE(scale.x, scale.y, scale.z);
        CAPTURE(scale_case.Singular);

        const Effekseer::SIMD::Mat43f matrix = make_matrix(scale);
        Effekseer::SIMD::Vec3f extracted_scale;
        Effekseer::SIMD::Mat43f decomposed_rotation;
        Effekseer::SIMD::Vec3f extracted_translation;
        matrix.GetSRT(extracted_scale, decomposed_rotation, extracted_translation);

        const Effekseer::SIMD::Mat43f direct_rotation = matrix.GetRotation();
        const array<vec3, 3> decomposed_axes = get_basis_rows(decomposed_rotation);
        const array<vec3, 3> direct_axes = get_basis_rows(direct_rotation);

        CHECK(extracted_scale.GetX() == Catch::Approx(std::abs(scale.x)).margin(StripWidthTolerance));
        CHECK(extracted_scale.GetY() == Catch::Approx(std::abs(scale.y)).margin(StripWidthTolerance));
        CHECK(extracted_scale.GetZ() == Catch::Approx(std::abs(scale.z)).margin(StripWidthTolerance));

        check_frame(decomposed_axes, scale_case.Singular);
        check_frame(direct_axes, scale_case.Singular);

        const array<float32_t, 3> scale_components {scale.x, scale.y, scale.z};

        for (size_t axis_index = 0; axis_index < scale_components.size(); axis_index++) {
            if (scale_components[axis_index] == 0.0f) {
                continue;
            }

            const float32_t expected_sign = scale_components[axis_index] < 0.0f ? -1.0f : 1.0f;
            const vec3 expected_axis = authored_basis[axis_index] * expected_sign;
            const vec3& decomposed_axis = decomposed_axes[axis_index];
            const vec3& direct_axis = direct_axes[axis_index];

            CHECK(decomposed_axis.x == Catch::Approx(expected_axis.x).margin(StripWidthTolerance));
            CHECK(decomposed_axis.y == Catch::Approx(expected_axis.y).margin(StripWidthTolerance));
            CHECK(decomposed_axis.z == Catch::Approx(expected_axis.z).margin(StripWidthTolerance));
            CHECK(direct_axis.x == Catch::Approx(expected_axis.x).margin(StripWidthTolerance));
            CHECK(direct_axis.y == Catch::Approx(expected_axis.y).margin(StripWidthTolerance));
            CHECK(direct_axis.z == Catch::Approx(expected_axis.z).margin(StripWidthTolerance));
        }

        const Effekseer::SIMD::Mat43f reconstructed = Effekseer::SIMD::Mat43f::SRT(extracted_scale, decomposed_rotation, extracted_translation);

        // Every field of the source matrix is pinned literally, including the rows whose scale collapsed to zero
        CHECK(reconstructed.X.GetX() == Catch::Approx(scale.x * (1.0f / 3.0f)).margin(StripWidthTolerance));
        CHECK(reconstructed.X.GetY() == Catch::Approx(scale.y * (2.0f / 3.0f)).margin(StripWidthTolerance));
        CHECK(reconstructed.X.GetZ() == Catch::Approx(scale.z * (-2.0f / 3.0f)).margin(StripWidthTolerance));
        CHECK(reconstructed.Y.GetX() == Catch::Approx(scale.x * (2.0f / 3.0f)).margin(StripWidthTolerance));
        CHECK(reconstructed.Y.GetY() == Catch::Approx(scale.y * (1.0f / 3.0f)).margin(StripWidthTolerance));
        CHECK(reconstructed.Y.GetZ() == Catch::Approx(scale.z * (2.0f / 3.0f)).margin(StripWidthTolerance));
        CHECK(reconstructed.Z.GetX() == Catch::Approx(scale.x * (2.0f / 3.0f)).margin(StripWidthTolerance));
        CHECK(reconstructed.Z.GetY() == Catch::Approx(scale.y * (-2.0f / 3.0f)).margin(StripWidthTolerance));
        CHECK(reconstructed.Z.GetZ() == Catch::Approx(scale.z * (-1.0f / 3.0f)).margin(StripWidthTolerance));
        CHECK(reconstructed.X.GetW() == Catch::Approx(3.0f).margin(StripWidthTolerance));
        CHECK(reconstructed.Y.GetW() == Catch::Approx(4.0f).margin(StripWidthTolerance));
        CHECK(reconstructed.Z.GetW() == Catch::Approx(5.0f).margin(StripWidthTolerance));
    }
}

TEST_CASE("Effekseer singular SRT decomposition retains invalid surviving basis components", "[particle][effekseer-runtime][effekseer-singular-transform]")
{
    const array<float32_t, 2> invalid_components {
        std::numeric_limits<float32_t>::quiet_NaN(),
        std::numeric_limits<float32_t>::infinity(),
    };

    for (float32_t invalid_component : invalid_components) {
        CAPTURE(invalid_component);

        // X is collapsed while Y survives with a nonfinite component. A repair that substitutes a finite
        // canonical identity for any singular matrix would erase the invalid input instead of propagating it
        const Effekseer::SIMD::Mat43f matrix {
            0.0f, 0.0f, 0.0f,
            invalid_component, 1.0f / 3.0f, -2.0f / 3.0f,
            -2.0f / 3.0f, 2.0f / 3.0f, -1.0f / 3.0f,
            3.0f, 4.0f, 5.0f,
        };

        Effekseer::SIMD::Vec3f extracted_scale;
        Effekseer::SIMD::Mat43f decomposed_rotation;
        Effekseer::SIMD::Vec3f extracted_translation;
        matrix.GetSRT(extracted_scale, decomposed_rotation, extracted_translation);

        const Effekseer::SIMD::Mat43f direct_rotation = matrix.GetRotation();

        const auto has_nonfinite_component = [](const Effekseer::SIMD::Mat43f& rotation) {
            return !std::isfinite(rotation.X.GetX()) || !std::isfinite(rotation.X.GetY()) ||
                !std::isfinite(rotation.X.GetZ()) || !std::isfinite(rotation.Y.GetX()) ||
                !std::isfinite(rotation.Y.GetY()) || !std::isfinite(rotation.Y.GetZ()) ||
                !std::isfinite(rotation.Z.GetX()) || !std::isfinite(rotation.Z.GetY()) ||
                !std::isfinite(rotation.Z.GetZ());
        };

        // Only at least one nonfinite basis component is required; the routines need not fill every lane with NaN
        CHECK(has_nonfinite_component(decomposed_rotation));
        CHECK(has_nonfinite_component(direct_rotation));
    }
}

#endif

FO_END_NAMESPACE
