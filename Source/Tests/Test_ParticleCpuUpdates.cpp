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

#include "Test_ParticleCpuUpdates.h"

#include "DefaultSprites.h"
#include "EffekseerCompiler.h"
#include "EffekseerExtension.h"
#include "ModelInstance.h"
#include "ModelManager.h"
#include "ModelSprites.h"
#include "ParticleSprites.h"
#include "SparkExtension.h"
#include "Test_ClientEngineHelpers.h"
#include "Test_ParticleFixtures.h"

#if FO_SPARK_PARTICLES
FO_DISABLE_WARNINGS_PUSH()
#include "SPARK.h"
FO_DISABLE_WARNINGS_POP()
#endif

FO_BEGIN_NAMESPACE

namespace ParticleCpuTests
{
    CapturingRenderEffect::CapturingRenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader, shared_ptr<DrawCapture> capture) :
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
        bool model_layout = false;

#if FO_ENABLE_3D
        model_layout = dbuf->Vertices.empty();
        if (model_layout) {
            FO_VERIFY_AND_THROW(dbuf->VertCount <= dbuf->Vertices3D.size(), "Captured draw exceeds the model vertex buffer", dbuf->VertCount, dbuf->Vertices3D.size());
        }
        else {
#endif
            FO_VERIFY_AND_THROW(dbuf->VertCount <= dbuf->Vertices.size(), "Captured draw exceeds the vertex buffer", dbuf->VertCount, dbuf->Vertices.size());
#if FO_ENABLE_3D
        }
#endif

        const_span<Vertex2D> vertices {dbuf->Vertices.data(), model_layout ? 0 : dbuf->VertCount};
#if FO_ENABLE_3D
        const_span<Vertex3D> vertices_3d {dbuf->Vertices3D.data(), model_layout ? dbuf->VertCount : 0};
#endif
        const_span<vindex_t> indices {dbuf->Indices.data() + start_index, draw_index_count};
        vector<Vertex2D> captured_vertices(vertices.begin(), vertices.end());
#if FO_ENABLE_3D
        vector<Vertex3D> captured_vertices_3d(vertices_3d.begin(), vertices_3d.end());
#endif
        vector<vindex_t> captured_indices(indices.begin(), indices.end());
        optional<array<float32_t, 4>> captured_sub_rect;
        optional<array<float32_t, 4>> captured_sampling;

        if (SpriteBorderBuf.has_value()) {
            captured_sub_rect = array<float32_t, 4> {SpriteBorderBuf->SpriteBorder[0], SpriteBorderBuf->SpriteBorder[1], SpriteBorderBuf->SpriteBorder[2], SpriteBorderBuf->SpriteBorder[3]};
        }
        if (ParticleSamplingBuf.has_value()) {
            captured_sampling = array<float32_t, 4> {ParticleSamplingBuf->ParticleSampling[0], ParticleSamplingBuf->ParticleSampling[1], ParticleSamplingBuf->ParticleSampling[2], ParticleSamplingBuf->ParticleSampling[3]};
        }

        _capture->Draws.emplace_back(CapturedDraw {
            .EffectName = _name,
            .Vertices = std::move(captured_vertices),
#if FO_ENABLE_3D
            .Vertices3D = std::move(captured_vertices_3d),
#endif
            .HasBackgroundTexture = !!BackgroundTex,
            .Indices = std::move(captured_indices),
            .PrimitiveType = dbuf->PrimType,
            .HasMainTexture = !!MainTex,
            .HasProjection = ProjBuf.has_value(),
            .AtlasSubRect = captured_sub_rect,
            .Sampling = captured_sampling,
            .CullMode = CullMode,
        });

        if (_capture->Observer) {
            _capture->Observer(_name);
        }
    }

    CapturingAppRender::CapturingAppRender(ptr<GlobalSettings> settings) :
        _capture {safe_alloc::make_shared<DrawCapture>()}
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

    void CapturingAppRender::SetDrawObserver(function<void(string_view)> observer)
    {
        _capture->Observer = std::move(observer);
    }

    auto CapturingAppRender::GetDraws() const -> const vector<CapturedDraw>&
    {
        return _capture->Draws;
    }

#if FO_EFFEKSEER_PARTICLES
    auto MakeGenerationalEffekseerEffect(int32_t parent_life, int32_t child_life) -> vector<uint8_t>
    {
        // Self-contained Editor 1.80.5/version-3 source. The existing in-process compiler produces the runtime
        // payload; no installed editor, embedding project assets or external executable is needed by this test.
        string child = strex(R"XML(<Node><Name>Child</Name>
<CommonValues><MaxGeneration><Value>2</Value></MaxGeneration><Life><Max>{}</Max><Min>{}</Min></Life>
<LocationEffectType>0</LocationEffectType><Removal><WhenParentIsRemoved>False</WhenParentIsRemoved></Removal>
<Generation><GenerationTime><Max>1</Max><Min>1</Min></GenerationTime></Generation></CommonValues>
<LocationValues><Type>1</Type><PVA><Location><X><Max>2</Max><Min>1</Min></X></Location>
<Velocity><Y><Max>0.3</Max><Min>0.1</Min></Y></Velocity><Acceleration><X><Max>0.01</Max><Min>0.01</Min></X></Acceleration></PVA></LocationValues>
<RendererCommonValues><ColorTexture>Texture/Cpu.png</ColorTexture><FadeOutType>1</FadeOutType><FadeOut><Frame>4</Frame></FadeOut></RendererCommonValues>
<DrawingValues><Type>2</Type><ColorAll><Fixed><R>30</R><G>220</G><B>90</B><A>255</A></Fixed></ColorAll></DrawingValues><Children/></Node>)XML", child_life, child_life).str();
        string project = strex(R"XML(<EffekseerProject><Root><Name>Root</Name><Children><Node><Name>Parent</Name>
<CommonValues><MaxGeneration><Value>128</Value></MaxGeneration><Life><Max>{}</Max><Min>{}</Min></Life>
<Generation><GenerationTime><Max>0</Max><Min>0</Min></GenerationTime></Generation></CommonValues>
<LocationValues><Type>1</Type><PVA><Location><X><Max>1</Max><Min>-1</Min></X></Location>
<Velocity><X><Max>0.2</Max><Min>0.05</Min></X><Y><Max>0.1</Max><Min>-0.1</Min></Y></Velocity></PVA></LocationValues>
<RendererCommonValues><ColorTexture>Texture/Cpu.png</ColorTexture><FadeOutType>1</FadeOutType><FadeOut><Frame>4</Frame></FadeOut></RendererCommonValues>
<DrawingValues><Type>2</Type><ColorAll><Fixed><R>240</R><G>40</G><B>20</B><A>255</A></Fixed></ColorAll></DrawingValues>
<Children>{}</Children></Node></Children></Root><Dynamic><Inputs/><Equations/></Dynamic><ToolVersion>1.80.5</ToolVersion><Version>3</Version></EffekseerProject>)XML", parent_life, parent_life, child).str();
        vector<uint8_t> source(project.begin(), project.end());
        return CompileEffekseerProject("Particles/Cpu.efkproj", source).Binary;
    }
#endif

    auto MakeResources(string_view extension) -> vector<pair<string, vector<uint8_t>>>
    {
        ignore_unused(extension);

        vector<pair<string, vector<uint8_t>>> resources;
        auto add_text = [&resources](string_view path, string_view text) {
            resources.emplace_back(string {path}, vector<uint8_t>(text.begin(), text.end()));
        };

        for (string_view effect : {"ImGui_Default", "2D_Default", "2D_NoDepth", "Primitive_Default", "Primitive_Light", "Primitive_Fog",
                 "Flush_RenderTarget", "Flush_Primitive", "Flush_Map", "Flush_Light", "Flush_Fog", "3D_Skinned",
                 "Particles_ColorMul", "Particles_ColorAdd", "Particles_ColorSub", "Particles_ColorMulAtlas", "Particles_ColorAddAtlas", "Particles_ColorSubAtlas",
                 "Particles_DistortionAtlas", "Particles_DistortionAddAtlas"}) {
            add_text(strex("Effects/{}.fofx", effect), "[Effect]\nPasses = 1\n");
            add_text(strex("Effects/{}.fofx-1-info", effect), "[EffectInfo]\nMainTex = 0\nProjBuf = 0\nSpriteBorderBuf = 1\nParticleSamplingBuf = 2\nMainTexBuf = 3\n");
        }

        resources.emplace_back("Particles/Texture/Cpu.png", BakerTests::MakeMinimalBakedSprite(8, 8));

#if FO_SPARK_PARTICLES
        if (extension == "spk") {
            SPK::SPKContext context;
            SPK::FO::EnsureSparkParticleObjectsRegistered(context);
            auto system = SPK::System::create(context, false);
            auto group = system->createGroup(1024);
            group->setContext(context);
            group->setLifeTime(10.0f, 10.0f);
            group->addEmitter(SPK::StaticEmitter::create(SPK_NULL_REF, true, 1024, -1.0f));
            auto renderer = SPK::FO::CreateSparkQuadRenderer();
            auto renderer_data = SPK::FO::GetSparkQuadRendererData(*renderer);
            renderer_data.Active = true;
            renderer_data.DrawInScene = true;
            renderer_data.EffectName = "Effects/Particles_ColorAdd.fofx";
            renderer_data.TextureName = "Texture/Cpu.png";
            renderer_data.ScaleX = 1.0f;
            renderer_data.ScaleY = 1.0f;
            renderer_data.AtlasDimensionX = 1;
            renderer_data.AtlasDimensionY = 1;
            SPK::FO::SetSparkQuadRendererData(*renderer, renderer_data);
            group->setRenderer(renderer);
            system->setBakedBounds(SPK::Vector3D(-1.0f, -1.0f, -1.0f), SPK::Vector3D(1.0f, 1.0f, 1.0f), 1.5f);
            std::ostringstream output;
            REQUIRE(context.getIOManager().save("spk", output, system));
            auto bytes = output.str();
            resources.emplace_back("Particles/Cpu.spk", vector<uint8_t>(bytes.begin(), bytes.end()));
        }
#endif

#if FO_EFFEKSEER_PARTICLES
        if (extension == "efk") {
            vector<uint8_t> bytes = ParticleTests::MakeModernRingEffect(512);
            AppendEffekseerBoundsTrailer(bytes, vec3 {-10.0f}, vec3 {10.0f}, 1.0f);
            resources.emplace_back("Particles/Cpu.efk", std::move(bytes));
        }
#endif

        return resources;
    }
}

#if FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES
namespace
{
    struct ParticleState
    {
        bool Active {};
        float32_t Elapsed {};
        optional<ParticleBounds3D> Bounds {};
    };

    auto ReadParticleState(const ParticleSystem& particle) -> ParticleState
    {
        return {.Active = particle.IsActive(), .Elapsed = particle.GetElapsedTime(), .Bounds = particle.GetLiveBounds()};
    }

    void CheckParticleState(const ParticleState& actual, const ParticleState& expected)
    {
        CHECK(actual.Active == expected.Active);
        CHECK(actual.Elapsed == Catch::Approx(expected.Elapsed));
        REQUIRE(actual.Bounds.has_value() == expected.Bounds.has_value());

        if (actual.Bounds) {
            CHECK(actual.Bounds->PositionMin.x == Catch::Approx(expected.Bounds->PositionMin.x).margin(0.00001f));
            CHECK(actual.Bounds->PositionMin.y == Catch::Approx(expected.Bounds->PositionMin.y).margin(0.00001f));
            CHECK(actual.Bounds->PositionMin.z == Catch::Approx(expected.Bounds->PositionMin.z).margin(0.00001f));
            CHECK(actual.Bounds->PositionMax.x == Catch::Approx(expected.Bounds->PositionMax.x).margin(0.00001f));
            CHECK(actual.Bounds->PositionMax.y == Catch::Approx(expected.Bounds->PositionMax.y).margin(0.00001f));
            CHECK(actual.Bounds->PositionMax.z == Catch::Approx(expected.Bounds->PositionMax.z).margin(0.00001f));
            CHECK(actual.Bounds->BillboardRadius == Catch::Approx(expected.Bounds->BillboardRadius));
        }
    }

    void CheckSpriteManagerParticleUpdates(string_view extension, size_t sprite_count)
    {
        vector<ParticleState> reference;

        for (int32_t mode = 0; mode < 3; mode++) {
            INFO(strex("extension={}, mode={}", extension, mode).str());
            auto settings = ClientEngineTests::MakeClientTestSettings();
            BakerTests::OverrideSetting(settings.Client.Multithreading, mode != 0);
            BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, mode == 1 ? 0 : 2);
            BakerTests::OverrideSetting(settings.Client.MultithreadingHeadroomMinCores, 0);
            BakerTests::OverrideSetting(settings.Render.Animation3dFPS, 1000);
            auto client = ClientEngineTests::MakeClientEngine(settings, ClientEngineTests::MakeClientTestResources(ParticleCpuTests::MakeResources(extension)));
            auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
            auto scheduler = client->SprMngr.GetWorkScheduler();

            if (mode == 2) {
                REQUIRE(scheduler);
                REQUIRE(scheduler->IsParallel());
            }
            else {
                CHECK((!scheduler || !scheduler->IsParallel()));
            }

            vector<shared_ptr<ParticleSprite>> sprites;

            for (size_t index = 0; index < sprite_count; index++) {
                auto sprite = client->SprMngr.LoadSprite(client->Hashes.to_hashed_string(strex("Particles/Cpu.{}", extension)), AtlasType::MapSprites, false).dyn_cast<ParticleSprite>();
                REQUIRE(sprite);
                sprite->SetDrawInScene(true);

#if FO_SPARK_PARTICLES
                if (extension == "spk") {
                    auto runtime = sprite->GetParticle()->GetRuntimeSystem().dyn_cast<SparkParticleRuntimeSystem>();
                    REQUIRE(runtime);
                    auto base = runtime->GetEditableBaseSystem();
                    base->getContext().setRandomSeed(123456U);
                }
#endif

                REQUIRE(sprite->PlayWithSeed(977));
                sprites.emplace_back(std::move(sprite));
            }

            size_t state_index = 0;

            // The timer does not advance: the existing first-draw rule supplies the same positive millisecond
            // to each scene sprite. No sleeps, performance deadline or scheduler-only worker evidence is used.
            for (int32_t frame = 0; frame < 8; frame++) {
                auto abort_scene = scope_fail([&client]() noexcept { client->SprMngr.AbortScene(); });
                client->SprMngr.BeginScene();
                client->SprMngr.EndScene();

                for (auto& sprite : sprites) {
                    ParticleState state = ReadParticleState(*sprite->GetParticle());
                    CHECK(state.Active);

                    if (mode == 0) {
                        reference.emplace_back(state);
                    }
                    else {
                        REQUIRE(state_index < reference.size());
                        CheckParticleState(state, reference[state_index]);
                    }

                    state_index++;
                }
            }

            uint64_t worker_calls = 0;

            for (auto& sprite : sprites) {
                worker_calls += sprite->GetParticle()->GetRuntimeSystem()->GetCpuUpdateDiagnostics().WorkerKernelCalls;
                ParticleState before_stop = ReadParticleState(*sprite->GetParticle());
                sprite->Stop();
                CheckParticleState(ReadParticleState(*sprite->GetParticle()), before_stop);
                sprite->GetParticle()->Prewarm();
                CHECK(sprite->GetParticle()->GetElapsedTime() >= before_stop.Elapsed);
            }

            bool workers_expected = mode == 2;
#ifdef SPK_DEBUG
            workers_expected = workers_expected && extension != "spk";
#endif
            if (workers_expected) {
                INFO("Missing worker participation leaves the parallel execution proof incomplete; it is separate from the simulation equality assertions above");
                CHECK(worker_calls > 0);
            }
            else {
                CHECK(worker_calls == 0);
            }
        }
    }
}
#endif

#if FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES
TEST_CASE("Particle CPU persistent field sizes", "[.particle-cpu-storage]")
{
    CHECK(sizeof(CpuUpdateDiagnostics) == 3 * sizeof(uint64_t));
    logging::write("PARTICLECPUSTORAGE diagnostics={} schedulerBorrow={} eligibilityFlag={} services={}",
        sizeof(CpuUpdateDiagnostics), sizeof(nptr<WorkScheduler>), sizeof(bool), sizeof(ParticleRuntimeServices));
#if FO_SPARK_PARTICLES
    logging::write("PARTICLECPUSTORAGE sparkContext={} sparkContextOwner={}", sizeof(SPK::SPKContext), sizeof(unique_nptr<SPK::SPKContext>));
    logging::write("PARTICLECPUSTORAGE sparkIoManager={} sparkDefaultPoint={}", sizeof(SPK::IO::IOManager), sizeof(SPK::Point));
#endif
#if FO_ENABLE_3D
    logging::write("PARTICLECPUSTORAGE modelManager={} modelInstance={}", sizeof(ModelManager), sizeof(ModelInstance));
#endif
}

TEST_CASE("Project particle runtime CPU admission census", "[.particle-cpu-census]")
{
    std::filesystem::path corpus {"Baking/Visual/Particles"};
    REQUIRE(std::filesystem::is_directory(corpus));
    vector<string> paths;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(corpus)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        bool selected = false;
#if FO_SPARK_PARTICLES
        selected = selected || entry.path().extension() == ".spk";
#endif
#if FO_EFFEKSEER_PARTICLES
        selected = selected || entry.path().extension() == ".efk";
#endif
        if (selected) {
            paths.emplace_back(strex("Particles/{}", strex(fs::path_to_string(std::filesystem::relative(entry.path(), corpus))).normalize_path_slashes()));
        }
    }

    std::sort(paths.begin(), paths.end());
    REQUIRE(!paths.empty());

    for (bool parallel : {false, true}) {
        auto settings = ClientEngineTests::MakeClientTestSettings();
        BakerTests::OverrideSetting(settings.Client.Multithreading, parallel);
        BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, 2);
        BakerTests::OverrideSetting(settings.Client.MultithreadingHeadroomMinCores, 0);
        auto resources = ClientEngineTests::MakeClientTestResources(ParticleCpuTests::MakeResources({}));
        resources.AddDirSource("Baking/Core", true);
        resources.AddDirSource("Baking/Visual", true);
        auto client = ClientEngineTests::MakeClientEngine(settings, std::move(resources));
        auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
        size_t accepted = 0;
        size_t create_rejected = 0;
        size_t update_or_draw_failed = 0;
        size_t scene_assets = 0;
        size_t admitted_assets = 0;
        size_t worker_assets = 0;

        for (const auto& path : paths) {
            CAPTURE(parallel, path);
            vector<shared_ptr<ParticleSprite>> sprites;
            bool admitted = false;
            bool scene = false;

            try {
                for (size_t index = 0; index < 2; index++) {
                    auto sprite = client->SprMngr.LoadSprite(client->Hashes.to_hashed_string(path), AtlasType::MapSprites, false).dyn_cast<ParticleSprite>();

                    if (!sprite) {
                        break;
                    }

                    auto runtime = sprite->GetParticle()->GetRuntimeSystem();
                    scene = sprite->GetParticle()->GetDrawInScene();
                    REQUIRE(sprite->PlayWithSeed(977));
                    admitted = sprite->GetParticle()->CanUpdateCpu();
#if FO_EFFEKSEER_PARTICLES
                    if (auto effekseer = runtime.dyn_cast<EffekseerParticleRuntimeSystem>(); effekseer) {
                        admitted = effekseer->IsCpuEffectEligible();
                    }
#endif
                    sprite->Prewarm();
                    sprites.emplace_back(std::move(sprite));
                }
            }
            catch (const std::exception& exception) {
                logging::write("PARTICLECPUCREATEFAIL parallel={} path={} error={}", parallel, path, exception.what());
            }

            if (sprites.size() != 2) {
                create_rejected++;
                logging::write("PARTICLECPUCREATE parallel={} path={} accepted=false", parallel, path);
            }
            else {
                accepted++;
                scene_assets += scene;
                admitted_assets += admitted;
                bool failed = false;

                try {
                    for (size_t frame = 0; frame < 8; frame++) {
                        client->GameTime.FrameAdvance(false);
                        auto abort_scene = scope_fail([&client]() noexcept { client->SprMngr.AbortScene(); });
                        client->SprMngr.BeginScene();

                        for (auto& sprite : sprites) {
                            if (sprite->IsDirectDraw()) {
                                sprite->DrawInScene({0.0f, 0.0f}, 0.0f);
                            }
                        }

                        client->SprMngr.EndScene();
                    }
                }
                catch (const std::exception& exception) {
                    failed = true;
                    update_or_draw_failed++;
                    logging::write("PARTICLECPUUPDATEFAIL parallel={} path={} error={}", parallel, path, exception.what());
                }

                CpuUpdateDiagnostics total;
                size_t active = 0;

                for (auto& sprite : sprites) {
                    auto diagnostics = sprite->GetParticle()->GetRuntimeSystem()->GetCpuUpdateDiagnostics();
                    total.OwnerKernelCalls += diagnostics.OwnerKernelCalls;
                    total.WorkerKernelCalls += diagnostics.WorkerKernelCalls;
                    total.UpdatedInstances += diagnostics.UpdatedInstances;
                    active += sprite->IsPlaying();
                }

                worker_assets += total.WorkerKernelCalls != 0;
                CHECK((parallel || total.WorkerKernelCalls == 0));
                logging::write("PARTICLECPURUNTIME parallel={} path={} scene={} admitted={} prewarmRequested=true scenePrewarmDeferred={} ownerCalls={} workerCalls={} instances={} active={} exception={}",
                    parallel, path, scene, admitted, scene, total.OwnerKernelCalls, total.WorkerKernelCalls, total.UpdatedInstances, active, failed);
            }

            sprites.clear();
            auto abort_scene = scope_fail([&client]() noexcept { client->SprMngr.AbortScene(); });
            client->SprMngr.BeginScene();
            client->SprMngr.EndScene();
        }

        logging::write("PARTICLECPUCOUNTS parallel={} walked={} accepted={} createRejected={} updateOrDrawExceptions={} sceneAssets={} admittedAssets={} workerAssets={}",
            parallel, paths.size(), accepted, create_rejected, update_or_draw_failed, scene_assets, admitted_assets, worker_assets);
    }
}
#endif

#if FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES
static void CheckModelParticleDraws(const vector<ParticleCpuTests::CapturedDraw>& actual, const vector<ParticleCpuTests::CapturedDraw>& expected, bool compare_effect_names = true)
{
    REQUIRE(actual.size() == expected.size());

    for (size_t draw_index = 0; draw_index < actual.size(); draw_index++) {
        CAPTURE(draw_index);
        const auto& left = actual[draw_index];
        const auto& right = expected[draw_index];
        if (compare_effect_names) {
            CHECK(left.EffectName == right.EffectName);
        }
        CHECK(left.Indices == right.Indices);
        REQUIRE(left.Vertices.size() == right.Vertices.size());
#if FO_ENABLE_3D
        REQUIRE(left.Vertices3D.size() == right.Vertices3D.size());
#endif
        bool equal = true;

        for (size_t i = 0; i < left.Vertices.size(); i++) {
            const auto& a = left.Vertices[i];
            const auto& b = right.Vertices[i];
            equal = equal && a.PosX == b.PosX && a.PosY == b.PosY && a.PosZ == b.PosZ && a.Color == b.Color && a.TexU == b.TexU && a.TexV == b.TexV && a.EggFlags[0] == b.EggFlags[0] && a.EggFlags[1] == b.EggFlags[1];
        }

#if FO_ENABLE_3D
        for (size_t i = 0; i < left.Vertices3D.size(); i++) {
            const auto& a = left.Vertices3D[i];
            const auto& b = right.Vertices3D[i];
            equal = equal && a.Position == b.Position && a.Normal == b.Normal && a.Color == b.Color && a.TexCoord[0] == b.TexCoord[0] && a.TexCoord[1] == b.TexCoord[1];
        }
#endif

        CHECK(equal);
    }
}
#endif

#if FO_ENABLE_3D && (FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES)
TEST_CASE("Model particles preserve geometry across direct and worker owners", "[particle][particle-cpu-model]")
{
    for (string_view extension : {
#if FO_SPARK_PARTICLES
             "spk",
#endif
#if FO_EFFEKSEER_PARTICLES
             "efk",
#endif
         }) {
        CAPTURE(extension);
        auto settings = ClientEngineTests::MakeClientTestSettings();
        BakerTests::OverrideSetting(settings.Client.Multithreading, true);
        BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, 2);
        BakerTests::OverrideSetting(settings.Client.MultithreadingHeadroomMinCores, 0);
        auto resources = ParticleCpuTests::MakeResources(extension);
#if FO_EFFEKSEER_PARTICLES
        if (extension == "efk") {
            auto bytes = ParticleTests::MakeModernRingEffect(128);
            AppendEffekseerBoundsTrailer(bytes, vec3 {-10.0f}, vec3 {10.0f}, 1.0f);

            for (auto& [name, data] : resources) {
                if (name == "Particles/Cpu.efk") {
                    data = std::move(bytes);
                }
            }
        }
#endif
        auto mesh = ClientEngineTests::MakeRuntimeModelTriangleMesh();
        resources.emplace_back("ModelAnimationInfo.foinfo", ClientEngineTests::MakeUnitTestModelAnimationInfo("Models/Cpu.fo3d"));
        resources.emplace_back("Models/Cpu.fo3d", ClientEngineTests::MakeRuntimeModelDescription("Models/Cpu.fo3d", "Models/Cpu.fbx", mesh));
        resources.emplace_back("Models/Cpu.fbx", std::move(mesh));
        auto client = ClientEngineTests::MakeClientEngine(settings, ClientEngineTests::MakeClientTestResources(std::move(resources)));
        auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
        REQUIRE(client->WorkSched.IsParallel());
        WorkScheduler zero_scheduler("ModelParticleZero", 0);
        ParticleCpuTests::CapturingAppRender render(&settings);
        EffectManager effects(&settings, &client->Resources, &render);
        effects.LoadDefaultEffects();
        auto texture = render.CreateTexture({8, 8}, true, false);
        vector<unique_ptr<ModelManager>> managers;
        vector<unique_ptr<ModelInstance>> models;
        size_t particle_count = extension == "spk" ? 8 : 2;

        for (size_t mode = 0; mode < 3; mode++) {
            nptr<WorkScheduler> scheduler = mode == 0 ? nullptr : mode == 1 ? &zero_scheduler : &client->WorkSched;
            auto manager = safe_alloc::make_unique<ModelManager>(
                &settings, &client->Resources, client.as_ptr(), &effects, &render, &client->GameTime, client.as_ptr(),
                [&texture](string_view) -> pair<nptr<RenderTexture>, frect32> { return {texture.as_nptr(), frect32 {0.0f, 0.0f, 1.0f, 1.0f}}; },
                [] { return false; }, ParticleSceneBackgroundProvider {}, scheduler);
            auto model = manager->CreateModel("Models/Cpu.fo3d");
            REQUIRE(model);
            REQUIRE(model->FindBone(manager->GetBoneHashedString("Models/Cpu.fbx")));
            model->StartMeshGeneration();
            model->SetupFrame({128, 128}, {64, 96});
            model->EnableShadow(false);
            model->SetSpeed(0.0f);

            if (extension == "efk") {
                model->SetDir(mdir {180}, false);
            }

            for (size_t index = 0; index < particle_count; index++) {
                model->RunParticle(strex("Particles/Cpu.{}", extension), manager->GetBoneHashedString("Models/Cpu.fbx"), vec3 {numeric_cast<float32_t>(index), 0.0f, 0.0f});
            }

            model->PoseSpriteFrame(true);
            REQUIRE(model->GetAttachPoints().size() == particle_count);
            models.emplace_back(model.take_not_null());
            managers.emplace_back(std::move(manager));
        }

        // Every owner reads the same cached frame time; model-created Effekseer systems retain their seed zero.
        // The SPARK fixture uses fixed life, point burst, fixed tank and zero force, so constructor fraction is irrelevant
        for (size_t frame = 0; frame < 8; frame++) {
            CAPTURE(frame);
            client->GameTime.FrameAdvance(false);
            vector<ParticleCpuTests::CapturedDraw> reference;
            vector<ModelAttachPoint> reference_points;

            for (size_t mode = 0; mode < models.size(); mode++) {
                CAPTURE(mode);
                auto& model = models[mode];
                model->PrepareSpriteFramePose(true);
                model->EvaluateFramePose();
                model->FinalizeFramePose();
                render.ClearDraws();
                model->DrawSpriteFrame();
                auto points = model->GetAttachPoints();
                REQUIRE(points.size() == particle_count);
                REQUIRE(render.GetDraws().size() == particle_count + 1);

                if (mode == 0) {
                    reference = render.GetDraws();
                    reference_points = points;
                    const auto& particle_draw = reference[1];
                    CHECK(particle_draw.Vertices.size() == (extension == "spk" ? 4096 : 16384));
                    CHECK(particle_draw.Indices.size() == (extension == "spk" ? 6144 : 24576));
                    REQUIRE(!particle_draw.Vertices.empty());
                    const auto& first = particle_draw.Vertices.front();
                    logging::write("Model CPU old geometry {} frame {} first {} {} {}", extension, frame, first.PosX, first.PosY, first.PosZ);

                    if (extension == "spk") {
                        CHECK(first.PosX == Catch::Approx(4.95f));
                        CHECK(first.PosY == Catch::Approx(5.25f));
                        CHECK(first.PosZ == 0.0f);
                    }
                    else {
                        CHECK(first.PosX == Catch::Approx(2.1479497f));
                        CHECK(first.PosY == Catch::Approx(25.904024f));
                        CHECK(first.PosZ == Catch::Approx(-0.2269392f));
                    }
                }
                else {
                    CheckModelParticleDraws(render.GetDraws(), reference);

                    for (size_t index = 0; index < points.size(); index++) {
                        CHECK(points[index].SpritePos == reference_points[index].SpritePos);
                    }
                }

                model->PoseSpriteFrame(false);
                render.ClearDraws();
                model->DrawSpriteFrame();
                CheckModelParticleDraws(render.GetDraws(), reference);
            }
        }

        for (size_t mode = 0; mode < models.size(); mode++) {
            auto diagnostics = models[mode]->GetParticleCpuUpdateDiagnostics();
            CHECK(diagnostics.OwnerKernelCalls > 0);

            bool workers_expected = mode == 2;
#ifdef SPK_DEBUG
            workers_expected = workers_expected && extension != "spk";
#endif
            if (workers_expected) {
                INFO("Missing actual worker participation is incomplete execution evidence, separate from geometry equality");
                CHECK(diagnostics.WorkerKernelCalls > 0);
            }
            else {
                CHECK(diagnostics.WorkerKernelCalls == 0);
            }

            if (extension == "efk") {
                auto& model = models[mode];
                model->SetDir(mdir {0}, false);
                model->RunParticle("Particles/Cpu.efk", managers[mode]->GetBoneHashedString("Models/Cpu.fbx"), vec3 {});
                REQUIRE(model->GetAttachPoints().size() == particle_count + 1);
                model->PoseSpriteFrame(false);
                CHECK(model->GetAttachPoints().size() == particle_count);
            }
        }
    }
}
#endif

#if FO_ENABLE_3D && FO_SPARK_PARTICLES
TEST_CASE("Authored SPARK asset reaches workers through the model sprite factory", "[.particle-cpu-model-corpus]")
{
    REQUIRE(std::filesystem::is_regular_file("Baking/Visual/Particles/FurnaceSmoke.spk"));

    for (bool parallel : {false, true}) {
        auto settings = ClientEngineTests::MakeClientTestSettings();
        BakerTests::OverrideSetting(settings.Client.Multithreading, parallel);
        BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, 2);
        BakerTests::OverrideSetting(settings.Client.MultithreadingHeadroomMinCores, 0);
        auto resources = ParticleCpuTests::MakeResources({});
        auto mesh = ClientEngineTests::MakeRuntimeModelTriangleMesh();
        resources.emplace_back("ModelAnimationInfo.foinfo", ClientEngineTests::MakeUnitTestModelAnimationInfo("Models/Cpu.fo3d"));
        resources.emplace_back("Models/Cpu.fo3d", ClientEngineTests::MakeRuntimeModelDescription("Models/Cpu.fo3d", "Models/Cpu.fbx", mesh));
        resources.emplace_back("Models/Cpu.fbx", std::move(mesh));
        auto files = ClientEngineTests::MakeClientTestResources(std::move(resources));
        files.AddDirSource("Baking/Core", true);
        files.AddDirSource("Baking/Visual", true);
        auto client = ClientEngineTests::MakeClientEngine(settings, std::move(files));
        auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
        auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
        REQUIRE(factory);
        auto probe = client->SprMngr.LoadSprite(client->Hashes.to_hashed_string("Particles/FurnaceSmoke.spk"), AtlasType::MapSprites, false).dyn_cast<ParticleSprite>();
        REQUIRE(probe);
        auto probe_runtime = probe->GetParticle()->GetRuntimeSystem().dyn_cast<SparkParticleRuntimeSystem>();
        REQUIRE(probe_runtime);
        bool original_eligible = probe_runtime->CanUpdateCpu();
        auto probe_base = probe_runtime->GetEditableBaseSystem();
        logging::write("PARTICLECPUMODELPROBE parallel={} originalEligible={} detachedEligible={} detachedBaseEligible={}",
            parallel, original_eligible, probe_runtime->CanUpdateCpu(), SparkParticleRuntimeSystem::IsCpuGraphSafe(probe_base));
        auto sprite = client->SprMngr.LoadSprite(client->Hashes.to_hashed_string("Models/Cpu.fo3d"), AtlasType::MapSprites, false).dyn_cast<ModelSprite>();
        REQUIRE(sprite);
        auto model = sprite->GetModel();
        hstring bone = factory->GetModelMngr()->GetBoneHashedString("Models/Cpu.fbx");
        REQUIRE(model->FindBone(bone));

        // This is an asset-capability fixture with many attachments, not a census of authored model populations.
        for (size_t index = 0; index < 32; index++) {
            model->RunParticle("Particles/FurnaceSmoke.spk", bone, vec3 {});
        }

        sprite->Prewarm();
        REQUIRE(model->GetAttachPoints().size() == 32);
        auto before_frames = model->GetParticleCpuUpdateDiagnostics();
        logging::write("PARTICLECPUMODELPREWARM parallel={} ownerCalls={} workerCalls={}", parallel, before_frames.OwnerKernelCalls, before_frames.WorkerKernelCalls);

        for (size_t frame = 0; frame < 16; frame++) {
            client->GameTime.FrameAdvance(false);
            model->RequestRedraw();
            auto abort_scene = scope_fail([&client]() noexcept { client->SprMngr.AbortScene(); });
            client->SprMngr.BeginScene();
            (void)sprite->Update();
            client->SprMngr.EndScene();
            CHECK(model->GetAttachPoints().size() == 32);
        }

        auto diagnostics = model->GetParticleCpuUpdateDiagnostics();
        CHECK(diagnostics.OwnerKernelCalls > 0);
#ifdef SPK_DEBUG
        CHECK(diagnostics.WorkerKernelCalls == 0);
#else
        INFO("Missing worker participation leaves this asset-route proof incomplete; it is distinct from a simulation-state mismatch");
        CHECK((parallel ? diagnostics.WorkerKernelCalls > 0 : diagnostics.WorkerKernelCalls == 0));
#endif
        logging::write("PARTICLECPUMODELASSET parallel={} asset=Particles/FurnaceSmoke.spk fixtureAttachments=32 ownerCalls={} workerCalls={}",
            parallel, diagnostics.OwnerKernelCalls, diagnostics.WorkerKernelCalls);
    }
}
#endif

#if FO_SPARK_PARTICLES
struct CpuOwnerActionState
{
    mutable std::thread::id CalledOn {};
    mutable size_t Calls {};
};

class CpuOwnerFailureAction final : public SPK::Action
{
    SPK_IMPLEMENT_OBJECT(CpuOwnerFailureAction)

public:
    nptr<CpuOwnerActionState> State {};

    void apply(SPK::Particle& particle) const override
    {
        ignore_unused(particle);
        FO_VERIFY_AND_THROW(State, "Particle owner action has no observation state");
        State->CalledOn = std::this_thread::get_id();
        State->Calls++;
        throw std::runtime_error("Unknown SPARK owner action failed");
    }
};

TEST_CASE("Unknown SPARK action failure stays on the owner and releases the client", "[particle][particle-cpu-fallback][spark]")
{
    auto settings = ClientEngineTests::MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Client.Multithreading, true);
    BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, 2);
    BakerTests::OverrideSetting(settings.Client.MultithreadingHeadroomMinCores, 0);
    auto client = ClientEngineTests::MakeClientEngine(settings, ClientEngineTests::MakeClientTestResources(ParticleCpuTests::MakeResources("spk")));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    CpuOwnerActionState action_state;
    auto sprite = client->SprMngr.LoadSprite(client->Hashes.to_hashed_string("Particles/Cpu.spk"), AtlasType::MapSprites, false).dyn_cast<ParticleSprite>();
    REQUIRE(sprite);
    auto runtime = sprite->GetParticle()->GetRuntimeSystem().dyn_cast<SparkParticleRuntimeSystem>();
    REQUIRE(runtime);
    auto base = runtime->GetEditableBaseSystem();
    SPK::Ref<CpuOwnerFailureAction> action = SPK_NEW(CpuOwnerFailureAction);
    action->State = &action_state;
    base->getGroup(0)->setBirthAction(action);
    runtime->ReplaceBaseSystem(base);
    REQUIRE(sprite->PlayWithSeed(977));
    CHECK_FALSE(sprite->CanUpdateParticleCpu());
    uint32_t context_seed = base->getContext().getRandomSeed();

    auto frame = [&client] {
        auto abort_scene = scope_fail([&client]() noexcept { client->SprMngr.AbortScene(); });
        client->SprMngr.BeginScene();
        client->SprMngr.EndScene();
    };
    CHECK_THROWS_WITH(frame(), "Unknown SPARK owner action failed");
    CHECK(action_state.Calls == 1);
    CHECK(action_state.CalledOn == std::this_thread::get_id());
    CHECK(base->getContext().getRandomSeed() == context_seed);
    CHECK(runtime->GetCpuUpdateDiagnostics().WorkerKernelCalls == 0);

    // Retire through the existing recreate contract after a failed unknown callback; no unsafe worker hook exists.
    base->getGroup(0)->setBirthAction(SPK_NULL_REF);
    runtime->ReplaceBaseSystem(base);
    REQUIRE(sprite->PlayWithSeed(977));
    CHECK_NOTHROW(frame());
    CHECK(sprite->IsPlaying());
}

class CpuCopyFailureEmitter final : public SPK::Emitter
{
    SPK_IMPLEMENT_OBJECT(CpuCopyFailureEmitter)

public:
    CpuCopyFailureEmitter() :
        SPK::Emitter(SPK_NULL_REF, true, 12, 7.0f)
    {
    }

    CpuCopyFailureEmitter(const CpuCopyFailureEmitter& other) :
        SPK::Emitter(other),
        FailCopy {other.FailCopy}
    {
        if (FailCopy) {
            throw std::runtime_error("SPARK copy failed after emitter constructor draws");
        }
    }

    bool FailCopy {true};

private:
    void generateVelocity(SPK::Particle& particle, float32_t speed) const override
    {
        ignore_unused(speed);
        particle.velocity().set(0.0f, 0.0f, 0.0f);
    }
};

TEST_CASE("SPARK destination copy preserves consumed draws and bindings on failure", "[particle][particle-cpu-isolation][spark]")
{
    SPK::SPKContext source;
    SPK::SPKContext destination;
    auto base = SPK::System::create(source, false);
    auto group = base->createGroup(16);
    group->setContext(source);
    SPK::Ref<CpuCopyFailureEmitter> emitter = SPK_NEW(CpuCopyFailureEmitter);
    emitter->setTank(8, 12);
    group->addEmitter(emitter);
    CHECK_FALSE(SparkParticleRuntimeSystem::IsCpuGraphSafe(base));

    source.setRandomSeed(123456U);
    CHECK_THROWS_WITH(SPK::SPKObject::copy(base), "SPARK copy failed after emitter constructor draws");
    uint32_t direct_seed = source.getRandomSeed();
    CHECK(direct_seed != 123456U);

    source.setRandomSeed(123456U);
    CHECK_THROWS_WITH(SPK::SPKObject::copy(base, destination), "SPARK copy failed after emitter constructor draws");
    CHECK(source.getRandomSeed() == direct_seed);
    CHECK(destination.getRandomSeed() == direct_seed);
    CHECK(&emitter->getContext() == &source);

    // A retry reaches the same objects: neither the root nor child copy binding remains installed after unwinding.
    emitter->FailCopy = false;
    source.setRandomSeed(123456U);
    auto copied = SPK::SPKObject::copy(base, destination);
    REQUIRE(copied);
    CHECK(source.getRandomSeed() == direct_seed);
    CHECK(&copied->getGroup(0)->getEmitter(0)->getContext() == &destination);
    CHECK_FALSE(SparkParticleRuntimeSystem::IsCpuGraphSafe(copied));
}

TEST_CASE("SPARK destination context preserves old constructor and update streams", "[particle][particle-cpu-isolation][spark]")
{
    SPK::SPKContext context;
    context.setRandomSeed(123456U);
    auto base = SPK::System::create(context, false);
    auto group = base->createGroup(16);
    group->setContext(context);
    group->setLifeTime(0.6f, 1.2f);
    auto emitter = SPK::StraightEmitter::create(SPK::Vector3D(0.0f, 1.0f, 0.0f), SPK_NULL_REF, true, 12, 7.0f, 0.5f, 1.5f);
    emitter->setTank(8, 12);
    group->addEmitter(emitter);

    // These literals were recorded by the original direct-copy baseline before this overload existed.
    constexpr array<uint32_t, 4> context_seeds {277396911U, 237697967U, 1772333975U, 1981022945U};
    constexpr array<int32_t, 3> initial_tanks {8, 11, 11};
    constexpr array<array<size_t, 6>, 3> particle_counts {{{0, 0, 1, 3, 3, 5}, {0, 0, 1, 3, 3, 6}, {0, 1, 2, 4, 4, 5}}};
    constexpr array<array<uint32_t, 6>, 3> system_seeds {{{977U, 977U, 1100411457U, 1982395933U, 1982395933U, 1008878176U},
        {977U, 977U, 1100411457U, 1982395933U, 1982395933U, 1668491683U},
        {977U, 1100411457U, 217280010U, 538649845U, 538649845U, 1668491683U}}};

    for (size_t recreate = 0; recreate < 3; recreate++) {
        SPK::SPKContext destination;
        CHECK(context.getRandomSeed() == context_seeds[recreate]);
        auto system = SPK::SPKObject::copy(base, destination);
        REQUIRE(system);
        CHECK(context.getRandomSeed() == context_seeds[recreate + 1]);
        CHECK(&system->getContext() == &destination);
        CHECK(&system->getGroup(0)->getContext() == &destination);
        CHECK(&system->getGroup(0)->getEmitter(0)->getContext() == &destination);
        CHECK(&system->getGroup(0)->getEmitter(0)->getZone()->getContext() == &destination);
        CHECK(system->getGroup(0)->getEmitter(0)->getCurrentTank() == initial_tanks[recreate]);
        system->setRandomSeed(977U);
        system->initialize();
        CHECK(context.getRandomSeed() == context_seeds[recreate + 1]);
        size_t step = 0;

        for (float32_t delta : {0.0f, 0.0625f, 0.125f, 0.25f, 0.03125f, 0.5f}) {
            if (delta > 0.0f) {
                (void)system->updateParticles(delta);
            }

            CHECK(context.getRandomSeed() == context_seeds[recreate + 1]);
            CHECK(system->getNbParticles() == particle_counts[recreate][step]);
            CHECK(system->getRandomSeed() == system_seeds[recreate][step]);
            step++;
        }
    }
}

TEST_CASE("SPARK destination copy preserves shared zone aliases inside its graph", "[particle][particle-cpu-isolation][spark]")
{
    SPK::SPKContext source;
    SPK::SPKContext destination;
    auto base = SPK::System::create(source, false);
    auto group = base->createGroup(16);
    group->setContext(source);
    auto zone = SPK::Sphere::create(SPK::Vector3D(), 1.0f);
    group->addEmitter(SPK::StaticEmitter::create(zone, true, 8, 2.0f));
    group->addEmitter(SPK::StraightEmitter::create(SPK::Vector3D(0.0f, 1.0f, 0.0f), zone));
    auto system = SPK::SPKObject::copy(base, destination);
    REQUIRE(system);
    const auto& first_zone = system->getGroup(0)->getEmitter(0)->getZone();
    const auto& second_zone = system->getGroup(0)->getEmitter(1)->getZone();
    CHECK(first_zone.get() == second_zone.get());
    CHECK(first_zone.get() != zone.get());
    CHECK(&first_zone->getContext() == &destination);
    CHECK(&zone->getContext() == &source);
}

TEST_CASE("SPARK CPU admission rejects callback and diagnostic graph families", "[particle][particle-cpu-admission][spark]")
{
    SPK::SPKContext context;
    auto system = SPK::System::create(context, false);
    auto group = system->createGroup(16);
    group->setContext(context);
    auto emitter = SPK::StaticEmitter::create(SPK_NULL_REF, true, 8, 2.0f);
    group->addEmitter(emitter);
#ifdef SPK_DEBUG
    CHECK_FALSE(SparkParticleRuntimeSystem::IsCpuGraphSafe(system));
#else
    CHECK(SparkParticleRuntimeSystem::IsCpuGraphSafe(system));
#endif

    auto rotator = SPK::Rotator::create();
    group->addModifier(rotator);
    CHECK_FALSE(SparkParticleRuntimeSystem::IsCpuGraphSafe(system));
    group->setParamInterpolator(SPK::PARAM_ANGLE, SPK::FloatDefaultInitializer::create(0.0f));
    CHECK_FALSE(SparkParticleRuntimeSystem::IsCpuGraphSafe(system));
    group->setParamInterpolator(SPK::PARAM_ROTATION_SPEED, SPK::FloatDefaultInitializer::create(1.0f));
#ifndef SPK_DEBUG
    CHECK(SparkParticleRuntimeSystem::IsCpuGraphSafe(system));
#endif
    CHECK_FALSE(group->isInitialized());
    system->initialize();
    CHECK(group->isInitialized());
    CHECK(group->isEnabled(SPK::PARAM_ANGLE));
    CHECK(group->isEnabled(SPK::PARAM_ROTATION_SPEED));
#ifndef SPK_DEBUG
    CHECK(SparkParticleRuntimeSystem::IsCpuGraphSafe(system));
#endif
    group->setParamInterpolator(SPK::PARAM_ANGLE, SPK_NULL_REF);
    CHECK_FALSE(group->isEnabled(SPK::PARAM_ANGLE));
    CHECK_FALSE(SparkParticleRuntimeSystem::IsCpuGraphSafe(system));
    group->removeModifier(rotator);

    emitter->setZone(SPK::Cylinder::create());
    CHECK_FALSE(SparkParticleRuntimeSystem::IsCpuGraphSafe(system));
    emitter->setZone(SPK::Point::create());

    auto actions = SPK::ActionSet::create();
    group->setBirthAction(actions);
    CHECK_FALSE(SparkParticleRuntimeSystem::IsCpuGraphSafe(system));
    auto nested = SPK::ActionSet::create();
    actions->addAction(nested);
    nested->addAction(actions);
    auto break_cycle = scope_exit([&actions, &nested]() noexcept {
        actions->clearActions();
        nested->clearActions();
    });
    CHECK_FALSE(SparkParticleRuntimeSystem::IsCpuGraphSafe(system));
}

static auto MakeMovingSparkResources(string_view effect_name = "Effects/Particles_ColorAdd.fofx") -> vector<pair<string, vector<uint8_t>>>
{
    auto resources = ParticleCpuTests::MakeResources("spk");
    SPK::SPKContext context;
    SPK::FO::EnsureSparkParticleObjectsRegistered(context);
    auto system = SPK::System::create(context, false);
    auto group = system->createGroup(512);
    group->setContext(context);
    group->setLifeTime(2.0f, 3.0f);
    group->setColorInterpolator(SPK::ColorSimpleInterpolator::create(SPK::Color(255, 180, 90, 255), SPK::Color(20, 60, 240, 40)));
    group->addEmitter(SPK::RandomEmitter::create(SPK::Sphere::create(SPK::Vector3D(), 0.5f), true, 512, -1.0f, 2.0f, 5.0f));
    group->addModifier(SPK::LinearForce::createAsGravity(SPK::Vector3D(0.0f, -2.0f, 0.0f), SPK::Point::create(), SPK::ZONE_TEST_ALWAYS));
    auto renderer = SPK::FO::CreateSparkQuadRenderer();
    auto data = SPK::FO::GetSparkQuadRendererData(*renderer);
    data.Active = true;
    data.DrawInScene = true;
    data.EffectName = string {effect_name};
    data.TextureName = "Texture/Cpu.png";
    data.ScaleX = 0.2f;
    data.ScaleY = 0.2f;
    data.AtlasDimensionX = 1;
    data.AtlasDimensionY = 1;
    SPK::FO::SetSparkQuadRendererData(*renderer, data);
    group->setRenderer(renderer);
    system->setBakedBounds(SPK::Vector3D(-20.0f, -20.0f, -20.0f), SPK::Vector3D(20.0f, 20.0f, 20.0f), 0.5f);
    std::ostringstream output;
    REQUIRE(context.getIOManager().save("spk", output, system));
    auto bytes = output.str();

    for (auto& [name, payload] : resources) {
        if (name == "Particles/Cpu.spk") {
            payload.assign(bytes.begin(), bytes.end());
        }
    }

    return resources;
}

TEST_CASE("SPARK old direct copy and isolated copy preserve mixed-step random dynamics", "[particle][particle-cpu-dynamics][spark]")
{
    SPK::SPKContext source;
    SPK::SPKContext destination;
    SPK::FO::EnsureSparkParticleObjectsRegistered(source);
    auto resources = MakeMovingSparkResources();
    auto resource = std::ranges::find_if(resources, [](const auto& entry) { return entry.first == "Particles/Cpu.spk"; });
    REQUIRE(resource != resources.end());
    const auto& bytes = resource->second;
    auto base = source.getIOManager().loadFromBuffer("spk", ptr<const uint8_t> {bytes.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned int>(bytes.size()));
    REQUIRE(base);
    REQUIRE(base->getGroup(0)->getNbModifiers() == 1);
    auto gravity = SPK::dynamicCast<SPK::LinearForce>(base->getGroup(0)->getModifier(0));
    REQUIRE(gravity);
    REQUIRE(gravity->getZoneTest() == SPK::ZONE_TEST_ALWAYS);
    REQUIRE(gravity->getValue() == SPK::Vector3D(0.0f, -2.0f, 0.0f));
    REQUIRE(gravity->getCoef() == 1.0f);
    REQUIRE(gravity->getZone());
    REQUIRE(gravity->getZone().get() != source.getDefaultZone().get());
    REQUIRE(&gravity->getContext() == &source);
    REQUIRE(&gravity->getZone()->getContext() == &source);
    source.setRandomSeed(123456U);
    auto original = SPK::SPKObject::copy(base);
    source.setRandomSeed(123456U);
    auto isolated = SPK::SPKObject::copy(base, destination);
    original->setRandomSeed(977U);
    isolated->setRandomSeed(977U);
    original->initialize();
    isolated->initialize();
    REQUIRE(original->getStepMode() == SPK::STEP_MODE_REAL);
    vector<SPK::Vector3D> positions;
    vector<SPK::Vector3D> velocities;
    bool changed_color = false;
    size_t step = 0;

    // This is an explicit old-overload direct reference, not a claim about the consumer's wall-clock deltas.
    // SPARK advances position with the previous velocity, then applies gravity; every live particle is checked.
    for (float32_t delta : {0.0625f, 0.125f, 0.0f, 0.25f, 0.03125f, 0.5f}) {
        CAPTURE(step, delta);

        if (step > 0) {
            for (size_t index = 0; index < positions.size(); index++) {
                positions[index] += velocities[index] * delta;
                velocities[index].y -= 2.0f * delta;
            }
        }

        if (delta > 0.0f) {
            (void)original->updateParticles(delta);
            (void)isolated->updateParticles(delta);
        }

        REQUIRE(original->getNbParticles() == 512);
        REQUIRE(isolated->getNbParticles() == 512);
        CHECK(original->getRandomSeed() == isolated->getRandomSeed());

        for (size_t index = 0; index < 512; index++) {
            const auto expected = original->getGroup(0)->getParticle(index);
            const auto actual = isolated->getGroup(0)->getParticle(index);
            CHECK(actual.position() == expected.position());
            CHECK(actual.velocity() == expected.velocity());
            CHECK(actual.getColor() == expected.getColor());
            CHECK(actual.getAge() == expected.getAge());
            CHECK(actual.getEnergy() == expected.getEnergy());
            changed_color = changed_color || actual.getColor().r < 255;

            if (step == 0) {
                positions.emplace_back(expected.position());
                velocities.emplace_back(expected.velocity());
                CHECK(expected.getColor().r == 255);
                CHECK(expected.getAge() == 0.0f);
            }
            else {
                CHECK(actual.position().x == Catch::Approx(positions[index].x).margin(0.00001f));
                CHECK(actual.position().y == Catch::Approx(positions[index].y).margin(0.00001f));
                CHECK(actual.position().z == Catch::Approx(positions[index].z).margin(0.00001f));
                CHECK(actual.velocity().y == Catch::Approx(velocities[index].y).margin(0.00001f));
            }
        }

        step++;
    }

    CHECK(changed_color);
}
#endif

#if FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES
class CpuSpriteCaptureRig final
{
public:
    CpuSpriteCaptureRig(ptr<GlobalSettings> settings, ptr<GameTimer> timer, nptr<WorkScheduler> scheduler, vector<pair<string, vector<uint8_t>>> resources) :
        Resources {ClientEngineTests::MakeClientTestResources(std::move(resources))},
        Window {GetAppWindowStub(*settings)},
        Render {settings},
        Effects {settings, &Resources, &Render},
        Sprites {settings, Window.as_ptr(), &Resources, timer, &Effects, &Hashes, scheduler}
    {
        Effects.LoadDefaultEffects();
        Sprites.RegisterSpriteFactory(safe_alloc::make_unique<DefaultSpriteFactory>(&Sprites));
        Sprites.RegisterSpriteFactory(safe_alloc::make_unique<ParticleSpriteFactory>(&Sprites, settings, &Effects, timer, &Hashes));
    }

    auto AddParticle(int32_t seed, string_view extension = "spk") -> shared_ptr<ParticleSprite>
    {
        auto sprite = Sprites.LoadSprite(Hashes.to_hashed_string(strex("Particles/Cpu.{}", extension)), AtlasType::MapSprites, false).dyn_cast<ParticleSprite>();
        REQUIRE(sprite);
        sprite->SetDrawInScene(true);

#if FO_SPARK_PARTICLES
        if (extension == "spk") {
            auto runtime = sprite->GetParticle()->GetRuntimeSystem().dyn_cast<SparkParticleRuntimeSystem>();
            REQUIRE(runtime);
            runtime->GetEditableBaseSystem()->getContext().setRandomSeed(123456U);
        }

#endif
        REQUIRE(sprite->PlayWithSeed(seed));
        return sprite;
    }

    void Update()
    {
        auto abort = scope_fail([this]() noexcept { Sprites.AbortScene(); });
        Sprites.BeginScene();
        Sprites.EndScene();
    }

    auto Draw(ptr<ParticleSprite> sprite) -> vector<ParticleCpuTests::CapturedDraw>
    {
        Render.ClearDraws();
        sprite->DrawInScene({100.0f, 100.0f}, 0.0f);
        return Render.GetDraws();
    }

    FileSystem Resources;
    unique_ptr<IAppWindow> Window;
    ParticleCpuTests::CapturingAppRender Render;
    EffectManager Effects;
    hash_storage Hashes;
    SpriteManager Sprites;
};

static auto ParticleVerticesDiffer(const vector<ParticleCpuTests::CapturedDraw>& left, const vector<ParticleCpuTests::CapturedDraw>& right, bool colors) -> bool
{
    if (left.size() != right.size()) {
        return true;
    }

    for (size_t draw = 0; draw < left.size(); draw++) {
        if (left[draw].Vertices.size() != right[draw].Vertices.size()) {
            return true;
        }

        for (size_t vertex = 0; vertex < left[draw].Vertices.size(); vertex++) {
            const auto& a = left[draw].Vertices[vertex];
            const auto& b = right[draw].Vertices[vertex];

            if (colors ? a.Color != b.Color : a.PosX != b.PosX || a.PosY != b.PosY || a.PosZ != b.PosZ) {
                return true;
            }
        }
    }

    return false;
}

TEST_CASE("SpriteManager preserves moving random particle geometry and colors", "[particle][particle-cpu-dynamics]")
{
    auto settings = ClientEngineTests::MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Render.Animation3dFPS, 1000);
    GameTimer timer {&settings};
    WorkScheduler zero {"ParticleDynamicsZero", 0};
    WorkScheduler parallel {"ParticleDynamics", 2};
    REQUIRE(parallel.IsParallel());

    for (string_view extension : {
#if FO_SPARK_PARTICLES
             "spk",
#endif
#if FO_EFFEKSEER_PARTICLES
             "efk",
#endif
         }) {
        CAPTURE(extension);
        vector<unique_ptr<CpuSpriteCaptureRig>> rigs;
        vector<vector<shared_ptr<ParticleSprite>>> particles;

        for (size_t mode = 0; mode < 3; mode++) {
            nptr<WorkScheduler> scheduler = mode == 0 ? nullptr : mode == 1 ? &zero : &parallel;
            auto resources = ParticleCpuTests::MakeResources(extension);
#if FO_SPARK_PARTICLES
            if (extension == "spk") {
                resources = MakeMovingSparkResources();
            }
#endif
#if FO_EFFEKSEER_PARTICLES
            if (extension == "efk") {
                auto payload = ParticleCpuTests::MakeGenerationalEffekseerEffect(600, 600);
                AppendEffekseerBoundsTrailer(payload, vec3 {-20.0f}, vec3 {20.0f}, 1.0f);

                for (auto& [name, data] : resources) {
                    if (name == "Particles/Cpu.efk") {
                        data = std::move(payload);
                    }
                }
            }
#endif
            rigs.emplace_back(safe_alloc::make_unique<CpuSpriteCaptureRig>(&settings, &timer, scheduler, std::move(resources)));
            particles.emplace_back();

            for (int32_t index = 0; index < 16; index++) {
                // The first pair deliberately repeats a seed. Every other system has an independent stream.
                particles.back().emplace_back(rigs.back()->AddParticle(index < 2 ? 977 : 977 + index, extension));
            }
        }

        vector<ParticleCpuTests::CapturedDraw> previous;
        bool positions_changed = false;
        bool colors_changed = false;
        bool zero_delta_seen = false;
        bool positive_delta_seen = false;

        for (size_t frame = 0; frame < 12; frame++) {
            CAPTURE(frame);

            if (frame % 3 != 2) {
                timer.FrameAdvance(false);
            }

            float32_t before = particles[0][0]->GetParticle()->GetElapsedTime();
            vector<vector<ParticleCpuTests::CapturedDraw>> reference;

            for (size_t mode = 0; mode < rigs.size(); mode++) {
                CAPTURE(mode);
                rigs[mode]->Update();

                for (size_t index = 0; index < particles[mode].size(); index++) {
                    CAPTURE(index);
                    auto draws = rigs[mode]->Draw(particles[mode][index].as_ptr());
                    REQUIRE(!draws.empty());
                    REQUIRE(!draws[0].Vertices.empty());

                    if (mode == 0) {
                        reference.emplace_back(std::move(draws));
                    }
                    else {
                        CheckModelParticleDraws(draws, reference[index]);
                    }
                }
            }

            float32_t delta = particles[0][0]->GetParticle()->GetElapsedTime() - before;
            zero_delta_seen = zero_delta_seen || delta == 0.0f;
            positive_delta_seen = positive_delta_seen || delta > 0.0f;
            CheckModelParticleDraws(reference[0], reference[1]);
            CHECK(ParticleVerticesDiffer(reference[0], reference[2], false));

            if (!previous.empty() && delta > 0.0f) {
                positions_changed = positions_changed || ParticleVerticesDiffer(reference[0], previous, false);
                colors_changed = colors_changed || ParticleVerticesDiffer(reference[0], previous, true);
            }

            if (!previous.empty() && delta == 0.0f) {
                CheckModelParticleDraws(reference[0], previous);
            }

            previous = std::move(reference[0]);
        }

        CHECK(positive_delta_seen);
        CHECK(zero_delta_seen);
        CHECK(positions_changed);

        if (extension == "spk") {
            CHECK(colors_changed);
        }

        for (size_t mode = 0; mode < rigs.size(); mode++) {
            uint64_t workers = 0;

            for (auto& particle : particles[mode]) {
                workers += particle->GetParticle()->GetRuntimeSystem()->GetCpuUpdateDiagnostics().WorkerKernelCalls;
            }

            bool expected_workers = mode == 2;
#ifdef SPK_DEBUG
            expected_workers = expected_workers && extension != "spk";
#endif
            if (expected_workers) {
                INFO("Missing worker participation leaves the parallel execution proof incomplete; simulation geometry and RNG equality are checked separately above");
                CHECK(workers > 0);
            }
            else {
                CHECK(workers == 0);
            }
        }
    }
}
#endif

#if FO_SPARK_PARTICLES
TEST_CASE("SpriteManager executes SPARK particle CPU work", "[particle][particle-cpu][spark]")
{
    CheckSpriteManagerParticleUpdates("spk", 32);
}
#endif

#if FO_EFFEKSEER_PARTICLES
TEST_CASE("SpriteManager executes Effekseer particle CPU work", "[particle][particle-cpu][effekseer]")
{
    CheckSpriteManagerParticleUpdates("efk", 2);
}
#endif

#if FO_SPARK_PARTICLES
TEST_CASE("SpriteManager atlas callbacks observe committed spans and mutate only later particles", "[particle][particle-cpu-order][spark]")
{
    constexpr size_t count = 9;
    constexpr size_t barrier = 4;
    auto settings = ClientEngineTests::MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Render.Animation3dFPS, 0);
    GameTimer timer(&settings);
    WorkScheduler zero("ParticleOrderZero", 0);
    WorkScheduler parallel("ParticleOrderParallel", 2);
    auto resources = MakeMovingSparkResources();
    vector<string> effect_names;

    for (size_t index = 0; index < count; index++) {
        string effect = strex("Effects/CpuOrder{}.fofx", index).str();
        effect_names.emplace_back(effect);
        auto named = MakeMovingSparkResources(effect);

        for (auto& [name, bytes] : named) {
            if (name == "Particles/Cpu.spk") {
                resources.emplace_back(strex("Particles/Order{}.spk", index).str(), std::move(bytes));
            }
        }

        string definition = "[Effect]\nPasses = 1\n";
        string info = "[EffectInfo]\nMainTex = 0\nProjBuf = 0\n";
        resources.emplace_back(effect, vector<uint8_t>(definition.begin(), definition.end()));
        resources.emplace_back(effect + "-1-info", vector<uint8_t>(info.begin(), info.end()));
    }

    vector<unique_ptr<CpuSpriteCaptureRig>> rigs;
    vector<vector<shared_ptr<ParticleSprite>>> ordered;
    vector<vector<string>> ordered_effects;

    for (size_t mode = 0; mode < 3; mode++) {
        auto rig = safe_alloc::make_unique<CpuSpriteCaptureRig>(&settings, &timer, mode == 0 ? nullptr : mode == 1 ? &zero : &parallel, resources);
        vector<shared_ptr<ParticleSprite>> sprites;

        for (size_t index = 0; index < count; index++) {
            auto sprite = rig->Sprites.LoadSprite(rig->Hashes.to_hashed_string(strex("Particles/Order{}.spk", index)), AtlasType::MapSprites, false).dyn_cast<ParticleSprite>();
            REQUIRE(sprite);
            sprite->SetDrawInScene(false);
            REQUIRE(sprite->PlayWithSeed(977));
            sprite->GetParticle()->Update();
            sprite->GetParticle()->Draw();
            sprite->GetParticle()->Update(0.0625f);
            sprites.emplace_back(std::move(sprite));
        }

        vector<vector<ParticleCpuTests::CapturedDraw>> before;
        vector<float32_t> elapsed;
        vector<CpuUpdateDiagnostics> diagnostics;

        for (auto& sprite : sprites) {
            rig->Render.ClearDraws();
            sprite->GetParticle()->GetRuntimeSystem()->Draw();
            before.emplace_back(rig->Render.GetDraws());
            elapsed.emplace_back(sprite->GetParticle()->GetElapsedTime());
            diagnostics.emplace_back(sprite->GetParticle()->GetRuntimeSystem()->GetCpuUpdateDiagnostics());
        }

        vector<size_t> order;
        size_t flush_draws = 0;
        bool ending_scene = false;
        rig->Render.SetDrawObserver([&](string_view effect) {
            if (effect == "Effects/Flush_RenderTarget.fofx") {
                flush_draws++;
                CHECK(flush_draws == order.size() + (ending_scene ? 1 : 0));
                return;
            }

            const auto found = std::find(effect_names.begin(), effect_names.end(), effect);
            REQUIRE(found != effect_names.end());
            order.emplace_back(numeric_cast<size_t>(found - effect_names.begin()));
        });
        auto abort_scene = scope_fail([&rig]() noexcept { rig->Sprites.AbortScene(); });
        rig->Sprites.BeginScene();
        REQUIRE(order.size() == count);
        CHECK(flush_draws == count);
        ending_scene = true;
        rig->Sprites.EndScene();
        rig->Render.SetDrawObserver({});
        REQUIRE(order.size() == count);
        CHECK(flush_draws == count + 1);
        auto sorted = order;
        std::sort(sorted.begin(), sorted.end());

        for (size_t index = 0; index < count; index++) {
            CHECK(sorted[index] == index);
            auto& sprite = sprites[index];
            CHECK(sprite->GetParticle()->IsActive());
            CHECK(sprite->GetParticle()->GetElapsedTime() == elapsed[index]);
            const auto after = sprite->GetParticle()->GetRuntimeSystem()->GetCpuUpdateDiagnostics();
            CHECK(after.OwnerKernelCalls == diagnostics[index].OwnerKernelCalls);
            CHECK(after.WorkerKernelCalls == diagnostics[index].WorkerKernelCalls);
            rig->Render.ClearDraws();
            sprite->GetParticle()->GetRuntimeSystem()->Draw();
            CheckModelParticleDraws(rig->Render.GetDraws(), before[index]);
        }

        vector<shared_ptr<ParticleSprite>> logical;
        vector<string> logical_effects;

        // Calibration observes the stable container order only. This is a separate, explicit seeded setup.
        for (size_t ordinal = 0; ordinal < count; ordinal++) {
            auto sprite = sprites[order[ordinal]];
            auto runtime = sprite->GetParticle()->GetRuntimeSystem().dyn_cast<SparkParticleRuntimeSystem>();
            REQUIRE(runtime);
            runtime->GetEditableBaseSystem()->getContext().setRandomSeed(123456U);
            REQUIRE(sprite->PlayWithSeed(numeric_cast<int32_t>(977 + ordinal)));
            sprite->GetParticle()->Update(0.125f);
            sprite->GetParticle()->Draw();
            sprite->SetDrawInScene(ordinal != barrier);
            logical.emplace_back(std::move(sprite));
            logical_effects.emplace_back(effect_names[order[ordinal]]);
        }

        ordered.emplace_back(std::move(logical));
        ordered_effects.emplace_back(std::move(logical_effects));
        rigs.emplace_back(std::move(rig));
    }

    uint64_t worker_calls = 0;

    for (size_t frame = 0; frame < 8; frame++) {
        timer.FrameAdvance(false);
        vector<vector<ParticleCpuTests::CapturedDraw>> reference;

        for (size_t mode = 0; mode < rigs.size(); mode++) {
            CAPTURE(frame, mode);
            auto& rig = *rigs[mode];
            auto& sprites = ordered[mode];
            const auto capture = [&](size_t ordinal) {
                rig.Render.ClearDraws();
                sprites[ordinal]->GetParticle()->GetRuntimeSystem()->Draw();
                auto draws = rig.Render.GetDraws();
                REQUIRE(draws.size() == 1);
                CHECK(draws.front().EffectName == ordered_effects[mode][ordinal]);
                return draws;
            };
            vector<vector<ParticleCpuTests::CapturedDraw>> previous;
            vector<float32_t> elapsed;

            for (size_t ordinal = 0; ordinal < count; ordinal++) {
                previous.emplace_back(capture(ordinal));
                elapsed.emplace_back(sprites[ordinal]->GetParticle()->GetElapsedTime());
            }

            bool inside_observer = false;
            bool ending_scene = false;
            size_t callbacks = 0;
            size_t flushes = 0;
            vector<ParticleCpuTests::CapturedDraw> committed;
            rig.Render.SetDrawObserver([&](string_view effect) {
                if (inside_observer) {
                    return;
                }

                if (effect == "Effects/Flush_RenderTarget.fofx") {
                    flushes++;
                    CHECK(callbacks == 1);
                    CHECK(flushes == (ending_scene ? 2 : 1));
                    return;
                }

                REQUIRE(effect == ordered_effects[mode][barrier]);
                inside_observer = true;
                auto reset = scope_exit([&inside_observer]() noexcept { inside_observer = false; });
                callbacks++;
                CHECK(sprites[barrier - 1]->GetParticle()->GetElapsedTime() > elapsed[barrier - 1]);
                CHECK(sprites[barrier + 1]->GetParticle()->GetElapsedTime() == elapsed[barrier + 1]);
                committed = capture(barrier - 1);
                CHECK(ParticleVerticesDiffer(committed, previous[barrier - 1], false));
                CheckModelParticleDraws(capture(barrier + 1), previous[barrier + 1]);
                auto runtime = sprites[barrier + 1]->GetParticle()->GetRuntimeSystem().dyn_cast<SparkParticleRuntimeSystem>();
                REQUIRE(runtime);
                runtime->GetEditableBaseSystem()->getContext().setRandomSeed(123456U);
                REQUIRE(sprites[barrier + 1]->PlayWithSeed(numeric_cast<int32_t>(2000 + frame)));
                CHECK(sprites[barrier + 1]->GetParticle()->GetElapsedTime() == 0.0f);
            });
            auto clear_observer = scope_exit([&rig]() noexcept { rig.Render.SetDrawObserver({}); });
            auto abort_scene = scope_fail([&rig]() noexcept { rig.Sprites.AbortScene(); });
            rig.Sprites.BeginScene();
            REQUIRE(callbacks == 1);
            CHECK(flushes == 1);
            ending_scene = true;
            rig.Sprites.EndScene();
            rig.Render.SetDrawObserver({});
            REQUIRE(callbacks == 1);
            CHECK(flushes == 2);
            CheckModelParticleDraws(capture(barrier - 1), committed);
            // Respawn occurred inside the atlas callback at the current cached time. The later sprite must
            // compute its fresh forced-draw delta, rather than consume a descriptor prepared before the barrier.
            CHECK(sprites[barrier + 1]->GetParticle()->GetElapsedTime() == Catch::Approx(0.001f));

            for (size_t ordinal = 0; ordinal < count; ordinal++) {
                auto draws = capture(ordinal);
                const auto diagnostics = sprites[ordinal]->GetParticle()->GetRuntimeSystem()->GetCpuUpdateDiagnostics();

                if (mode == 0) {
                    reference.emplace_back(draws);
                }
                else {
                    // Names identify physical entries during calibration. Logical simulation order is compared
                    // across independent unordered containers; every other captured geometry field stays exact.
                    CheckModelParticleDraws(draws, reference[ordinal], false);
                }

                if (mode == 2) {
                    worker_calls += diagnostics.WorkerKernelCalls;
                }
                else {
                    CHECK(diagnostics.WorkerKernelCalls == 0);
                }
            }
        }
    }

#ifdef SPK_DEBUG
    CHECK(worker_calls == 0);
#else
    INFO("Zero worker calls means participation evidence is incomplete; state and callback ordering have separate assertions above");
    CHECK(worker_calls > 0);
#endif
}
#endif

#if FO_ENABLE_3D && FO_SPARK_PARTICLES
static auto MakeMovingChildParticleResources() -> vector<pair<string, vector<uint8_t>>>
{
    auto resources = ParticleCpuTests::MakeResources("spk");
    BakerTests::TestRig baker;

    for (string_view owner : {"Root", "Child"}) {
        SPK::SPKContext context;
        SPK::FO::EnsureSparkParticleObjectsRegistered(context);
        const auto found = std::find_if(resources.begin(), resources.end(), [](const auto& entry) { return entry.first == "Particles/Cpu.spk"; });
        REQUIRE(found != resources.end());
        const auto& payload = found->second;
        auto system = context.getIOManager().loadFromBuffer("spk", ptr<const uint8_t> {payload.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned int>(payload.size()));
        REQUIRE(system);
        auto renderer = system->getGroup(0)->getRenderer();
        auto data = SPK::FO::GetSparkQuadRendererData(*renderer);
        string effect = strex("Effects/Cpu{}.fofx", owner).str();
        data.EffectName = effect;
        SPK::FO::SetSparkQuadRendererData(*renderer, data);
        std::ostringstream output;
        REQUIRE(context.getIOManager().save("spk", output, system));
        auto bytes = output.str();
        string path = strex("Particles/{}.spk", owner).str();
        vector<uint8_t> particle(bytes.begin(), bytes.end());
        baker.AddBakedFile(path, particle, 1);
        resources.emplace_back(path, std::move(particle));
        string effect_source = "[Effect]\nPasses = 1\n";
        string effect_info = "[EffectInfo]\nMainTex = 0\nProjBuf = 0\n";
        resources.emplace_back(effect, vector<uint8_t>(effect_source.begin(), effect_source.end()));
        resources.emplace_back(effect + "-1-info", vector<uint8_t>(effect_info.begin(), effect_info.end()));

        string mesh_path = strex("Models/Order{}.fbx", owner).str();
        auto mesh = ClientEngineTests::MakeSkinnedRuntimeModelMesh();
        data_reader mesh_reader {mesh};
        auto mesh_data = ReadModelMeshData(mesh_reader, mesh_path);
        REQUIRE(mesh_data.RootBone);
        auto anchor = safe_alloc::make_unique<ModelMeshBoneData>();
        anchor->Name = "Anchor";
        anchor->TransformationMatrix = mat44 {1.0f};
        anchor->GlobalTransformationMatrix = mat44 {1.0f};
        mesh_data.RootBone->Children.emplace_back(std::move(anchor));
        mesh.clear();
        data_writer mesh_writer {mesh};
        WriteModelMeshData(mesh_writer, mesh_data, mesh_path);
        data_reader decoded_reader {mesh};
        auto decoded_mesh = ReadModelMeshData(decoded_reader, mesh_path);
        REQUIRE(decoded_mesh.RootBone);
        REQUIRE(decoded_mesh.RootBone->Children.size() == 2);
        REQUIRE(decoded_mesh.RootBone->Children[0]->Name == "Limb");
        REQUIRE(decoded_mesh.RootBone->Children[1]->Name == "Anchor");
        baker.AddSourceFile(mesh_path, "model source fixture", 1);
        baker.AddBakedFile(mesh_path, mesh, 1);
        resources.emplace_back(mesh_path, std::move(mesh));
        string description = strex("Model Order{}.fbx\nAnim 1 1 Order{}.fbx Base\n", owner, owner).str();

        if (owner == "Root") {
            description += "Layer 2\nValue 1\nAttach OrderChild.fo3d Link Limb\n";
        }

        description += "Layer 3\nValue 1\n";

        for (size_t index = 0; index < 4; index++) {
            description += strex("AttachParticles Particles/{}.spk Link {} MoveZ {}\n", owner, owner == "Root" ? "Anchor" : "Limb", index).str();
        }

        baker.AddSourceFile(strex("Models/Order{}.fo3d", owner), description, 1);
    }

    baker.AddBakedFile("Metadata.fometa-client", BakerTests::MakeEmptyMetadataBlob());
    ModelInfoBaker info_baker(baker.MakeContext(), [](string_view path, const File& file) -> ModelSourceAsset {
        ModelSourceAsset asset;
        asset.FileName = path;
        asset.WriteTime = file.GetWriteTime();
        asset.Skeleton.FileName = path;
        asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Root", .Hierarchy = {"Root"}, .RestLocalTransform = mat44 {1.0f}});
        asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Limb", .Hierarchy = {"Root", "Limb"}, .RestLocalTransform = mat44 {1.0f}});
        asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Anchor", .Hierarchy = {"Root", "Anchor"}, .RestLocalTransform = mat44 {1.0f}});
        ModelAnimationSource animation;
        animation.FileName = path;
        animation.Name = "Base";
        animation.Duration = 1.0f;

        for (string_view name : {"Root", "Limb", "Anchor"}) {
            ModelAnimationJointSource joint;
            joint.OutputName = string {name};
            joint.Hierarchy = name == "Root" ? vector<string> {"Root"} : vector<string> {"Root", string {name}};
            joint.Translation.Times = {0.0f, 0.5f, 1.0f};
            joint.Translation.Values = {vec3 {}, name == "Limb" ? vec3 {4.0f, 0.0f, 0.0f} : vec3 {0.0f, 2.0f, 0.0f}, vec3 {}};
            joint.Rotation.Times = {0.0f, 1.0f};
            joint.Rotation.Values = {quaternion {1.0f, 0.0f, 0.0f, 0.0f}, quaternion {1.0f, 0.0f, 0.0f, 0.0f}};
            joint.Scale.Times = {0.0f, 1.0f};
            joint.Scale.Values = {vec3 {1.0f}, vec3 {1.0f}};
            animation.Joints.emplace_back(std::move(joint));
        }

        asset.Animations.emplace_back(std::move(animation));
        return asset;
    });
    info_baker.BakeFiles(baker.GetAllSourceFiles(), "");
    vector<uint8_t> animation_info;

    for (string_view owner : {"Root", "Child"}) {
        string path = strex("Models/Order{}.fo3d", owner).str();
        REQUIRE(baker.Outputs.count(path) == 1);
        resources.emplace_back(path, baker.Outputs.at(path));
        auto info = ClientEngineTests::MakeUnitTestModelAnimationInfo(path);
        animation_info.insert(animation_info.end(), info.begin(), info.end());
    }

    resources.emplace_back("ModelAnimationInfo.foinfo", std::move(animation_info));
    return resources;
}

TEST_CASE("Model root callbacks observe moving child-joint particles and zero repose preserves state", "[particle][particle-cpu-order][particle-cpu-model][spark]")
{
    auto settings = ClientEngineTests::MakeClientTestSettings();
    auto client = ClientEngineTests::MakeClientEngine(settings, ClientEngineTests::MakeClientTestResources(MakeMovingChildParticleResources()));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    WorkScheduler zero("ChildParticleZero", 0);
    WorkScheduler parallel("ChildParticleParallel", 2);
    ParticleCpuTests::CapturingAppRender render(&settings);
    EffectManager effects(&settings, &client->Resources, &render);
    effects.LoadDefaultEffects();
    auto texture = render.CreateTexture({8, 8}, true, false);
    vector<unique_ptr<ModelManager>> managers;
    vector<unique_ptr<ModelInstance>> models;
    array<int32_t, MODEL_LAYERS_COUNT> layers {};
    layers[2] = 1;
    layers[3] = 1;
    const auto state = static_cast<CritterStateAnim>(1);
    const auto action = static_cast<CritterActionAnim>(1);

    for (size_t mode = 0; mode < 3; mode++) {
        auto manager = safe_alloc::make_unique<ModelManager>(
            &settings, &client->Resources, client.as_ptr(), &effects, &render, &client->GameTime, client.as_ptr(),
            [&texture](string_view) -> pair<nptr<RenderTexture>, frect32> { return {texture.as_nptr(), frect32 {0.0f, 0.0f, 1.0f, 1.0f}}; },
            [] { return false; }, ParticleSceneBackgroundProvider {}, mode == 0 ? nullptr : mode == 1 ? &zero : &parallel);
        auto model = manager->CreateModel("Models/OrderRoot.fo3d");
        REQUIRE(model);
        REQUIRE(model->FindBone(manager->GetBoneHashedString("Models/OrderRoot.fbx")));
        REQUIRE(model->FindBone(manager->GetBoneHashedString("Limb")));
        REQUIRE(model->FindBone(manager->GetBoneHashedString("Anchor")));
        model->StartMeshGeneration();
        model->EnableShadow(false);
        REQUIRE(model->PlayAnim(state, action, layers.data(), 0.0f, combine_enum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoSmooth)));
        model->SetupFrame({256, 256}, {128, 192});
        model->PoseSpriteFrame(true);
        auto points = model->GetAttachPoints();
        REQUIRE(points.size() == 9);
        CHECK(std::count_if(points.begin(), points.end(), [](const auto& point) { return point.Kind == ModelAttachKind::Model; }) == 1);
        CHECK(std::count_if(points.begin(), points.end(), [](const auto& point) { return point.Kind == ModelAttachKind::Particle && point.ParentIndex >= 0 && point.Name == "Particles/Child.spk"; }) == 4);
        CHECK(std::count_if(points.begin(), points.end(), [](const auto& point) { return point.Kind == ModelAttachKind::Particle && point.ParentIndex == -1 && point.Name == "Particles/Root.spk"; }) == 4);
        models.emplace_back(model.take_not_null());
        managers.emplace_back(std::move(manager));
    }

    const auto capture = [&](ptr<ModelInstance> model, string_view owner) {
        render.ClearDraws();
        model->DrawSpriteFrame();
        vector<ParticleCpuTests::CapturedDraw> draws;

        for (const auto& draw : render.GetDraws()) {
            if (draw.EffectName == strex("Effects/Cpu{}.fofx", owner).str()) {
                draws.emplace_back(draw);
            }
        }

        REQUIRE(draws.size() == 4);
        REQUIRE(draws.front().Vertices.size() == 4096);
        return draws;
    };

    // Fixed zero-force point bursts have no seed-dependent visible state and remain in world space after birth.
    // Fresh authored bursts below observe birth placement at new joints, not rebasing of old stationary particles.
    // Child attachments belong to the child instance; RunParticle on the root would not exercise this boundary.
    client->GameTime.FrameAdvance(false);

    for (auto& model : models) {
        model->PoseSpriteFrame(true);
    }

    uint64_t workers = 0;

    for (size_t frame = 0; frame < 4; frame++) {
        for (auto& model : models) {
            model->ClearAnimationCallbacks();
            // The return value reports changed mesh configuration, not whether the timeline restarted.
            (void)model->PlayAnim(state, action, layers.data(), 0.0f, combine_enum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoSmooth));
            REQUIRE(model->GetStateAnim() == state);
            REQUIRE(model->GetActionAnim() == action);
            REQUIRE(model->GetAnimDuration().milliseconds() == 1000);
            model->PoseSpriteFrame(false);
        }

        auto previous_time = client->GameTime.GetFrameTime();
        client->GameTime.FrameAdvance(false);
        float32_t elapsed = numeric_cast<float32_t>((client->GameTime.GetFrameTime() - previous_time).nanoseconds()) * 1e-9f;
        REQUIRE(elapsed > 0.0f);
        float32_t target_phase = 0.125f * numeric_cast<float32_t>(frame + 1);
        float32_t speed = target_phase / elapsed;
        REQUIRE(std::isfinite(speed));
        REQUIRE(speed > 0.0f);
        vector<ParticleCpuTests::CapturedDraw> root_reference;
        vector<ParticleCpuTests::CapturedDraw> child_reference;

        for (size_t mode = 0; mode < models.size(); mode++) {
            CAPTURE(frame, mode, elapsed);
            auto& model = models[mode];
            auto old_root = capture(model.as_ptr(), "Root");
            auto old_child = capture(model.as_ptr(), "Child");
            layers[3] = 0;
            (void)model->PlayAnim(state, action, layers.data(), 0.0f, combine_enum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoSmooth));
            auto removed_points = model->GetAttachPoints();
            REQUIRE(removed_points.size() == 1);
            REQUIRE(removed_points.front().Kind == ModelAttachKind::Model);
            REQUIRE(removed_points.front().Name == "Models/OrderChild.fo3d");
            layers[3] = 1;
            (void)model->PlayAnim(state, action, layers.data(), 0.0f, combine_enum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoSmooth));
            auto recreated_points = model->GetAttachPoints();
            REQUIRE(recreated_points.size() == 9);
            CHECK(std::count_if(recreated_points.begin(), recreated_points.end(), [](const auto& point) { return point.Kind == ModelAttachKind::Particle && point.ParentIndex == -1 && point.Name == "Particles/Root.spk"; }) == 4);
            CHECK(std::count_if(recreated_points.begin(), recreated_points.end(), [](const auto& point) { return point.Kind == ModelAttachKind::Particle && point.ParentIndex >= 0 && point.Name == "Particles/Child.spk"; }) == 4);
            model->PoseSpriteFrame(false);
            render.ClearDraws();
            model->DrawSpriteFrame();
            CHECK(std::none_of(render.GetDraws().begin(), render.GetDraws().end(), [](const auto& draw) { return draw.EffectName == "Effects/CpuRoot.fofx" || draw.EffectName == "Effects/CpuChild.fofx"; }));
            vector<ParticleCpuTests::CapturedDraw> callback_root;
            vector<ParticleCpuTests::CapturedDraw> callback_child;
            size_t callbacks = 0;
            model->SetSpeed(speed);
            model->AddAnimationCallback(ModelAnimationCallback {
                .StateAnim = state,
                .ActionAnim = action,
                .NormalizedTime = 0.0625f,
                .Callback = [&] {
                    callbacks++;
                    callback_root = capture(model.as_ptr(), "Root");
                    callback_child = capture(model.as_ptr(), "Child");
                },
            });
            auto clear_callbacks = scope_exit([&model]() noexcept { model->ClearAnimationCallbacks(); });
            model->PrepareSpriteFramePose(true);
            model->EvaluateFramePose();
            model->FinalizeFramePose();
            REQUIRE(callbacks == 1);
            auto new_root = capture(model.as_ptr(), "Root");
            auto new_child = capture(model.as_ptr(), "Child");
            CHECK(ParticleVerticesDiffer(old_root, new_root, false));
            CHECK(ParticleVerticesDiffer(old_child, new_child, false));
            CheckModelParticleDraws(callback_root, new_root);
            CheckModelParticleDraws(callback_child, new_child);
            const auto& root_before = old_root.front().Vertices.front();
            const auto& root_after = new_root.front().Vertices.front();
            const auto& child_before = old_child.front().Vertices.front();
            const auto& child_after = new_child.front().Vertices.front();
            // The root Anchor track moves only on Y; the child's parent Limb track also moves on X.
            // These decoded draw coordinates observe the authored axes, separately from mode equality.
            CHECK(root_after.PosX == root_before.PosX);
            CHECK(root_after.PosY != root_before.PosY);
            CHECK(child_after.PosX != child_before.PosX);
            CHECK((root_after.PosX - root_before.PosX != child_after.PosX - child_before.PosX || root_after.PosY - root_before.PosY != child_after.PosY - child_before.PosY));

            if (mode == 0) {
                root_reference = new_root;
                child_reference = new_child;
            }
            else {
                CheckModelParticleDraws(new_root, root_reference);
                CheckModelParticleDraws(new_child, child_reference);
            }

            const auto diagnostics = model->GetParticleCpuUpdateDiagnostics();
            workers += mode == 2 ? diagnostics.WorkerKernelCalls : 0;

            if (mode != 2) {
                CHECK(diagnostics.WorkerKernelCalls == 0);
            }

            model->PoseSpriteFrame(false);
            CHECK(callbacks == 1);
            CheckModelParticleDraws(capture(model.as_ptr(), "Root"), new_root);
            CheckModelParticleDraws(capture(model.as_ptr(), "Child"), new_child);
            const auto zero_diagnostics = model->GetParticleCpuUpdateDiagnostics();
            CHECK(zero_diagnostics.OwnerKernelCalls == diagnostics.OwnerKernelCalls);
            CHECK(zero_diagnostics.WorkerKernelCalls == diagnostics.WorkerKernelCalls);
            CHECK(zero_diagnostics.UpdatedInstances == diagnostics.UpdatedInstances);
        }
    }

#ifdef SPK_DEBUG
    CHECK(workers == 0);
#else
    INFO("Missing actual worker calls is incomplete participation evidence, separately from geometry, placement and callback assertions");
    CHECK(workers > 0);
#endif
}
#endif

FO_END_NAMESPACE

