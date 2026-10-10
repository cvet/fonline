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

#include "catch_amalgamated.hpp"

#include "Application.h"
#include "Client.h"
#include "ItemHexView.h"
#include "MapView.h"
#include "ModelInfoBaker.h"
#include "ModelMeshData.h"
#include "ModelSprites.h"
#include "PlayerView.h"
#include "Test_BakerHelpers.h"
#include "TextureAtlas.h"

FO_BEGIN_NAMESPACE

class LifetimeMaskReadEffect final : public RenderEffect
{
public:
    explicit LifetimeMaskReadEffect(ptr<RenderTargetManager> targets) :
        RenderEffect(EffectUsage::QuadSprite, "LifetimeQueued.fofx", [](string_view name) -> string { return name.ends_with("-info") ? "[EffectInfo]\nMainTex = 0\nIndoorMaskTex = 1\n" : "[Effect]\nPasses = 1\n"; }),
        Targets {targets}
    {
    }

    void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override
    {
        ignore_unused(dbuf, start_index, indices_to_draw, custom_tex);
        REQUIRE(IndoorMaskTex);
        OwnersAtDraw = Targets->GetRenderTargetCount();
        Draws++;
    }

    size_t OwnersAtDraw {};
    size_t Draws {};

private:
    ptr<RenderTargetManager> Targets;
};

static auto MakeClientLifetimeSettings() -> GlobalSettings
{
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    return settings;
}

static auto MakeClientLifetimeEngine(GlobalSettings& settings, vector<pair<string, vector<uint8_t>>> extra_resources = {}) -> refcount_ptr<ClientEngine>
{
    vector<uint8_t> metadata = BakerTests::MakeEmptyMetadataBlob();
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientEntityLifetime");
    source->AddFile("Metadata.fometa-client", metadata);
    source->AddFile("LifetimeSprite.png", BakerTests::MakeMinimalBakedSprite(2, 2));

    for (string_view name : {"ImGui_Default", "2D_Default", "2D_NoDepth", "Primitive_Default", "Primitive_Light", "Primitive_Fog", "Flush_RenderTarget", "Flush_Primitive", "Flush_Map", "Flush_Light", "Flush_Fog", "3D_Skinned", "Particles_ColorMulAtlas", "Particles_ColorAddAtlas", "Particles_ColorSubAtlas", "Particles_DistortionAtlas", "Particles_DistortionAddAtlas"}) {
        source->AddFile(strex("Effects/{}.fofx", name).str(), "[Effect]\nPasses = 1\n");
        source->AddFile(strex("Effects/{}.fofx-1-info", name).str(), "[EffectInfo]\n");
    }

    for (string_view name : {"LifetimeMaskA", "LifetimeMaskB", "LifetimeMaskC"}) {
        source->AddFile(strex("Effects/{}.fofx", name).str(), "[Effect]\nPasses = 1\n");
        source->AddFile(strex("Effects/{}.fofx-1-info", name).str(), "[EffectInfo]\nMainTex = 0\nIndoorMaskTex = 1\n");
    }

#if FO_ANGELSCRIPT_SCRIPTING
    auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientEntityLifetimeCompiler");
    compiler_source->AddFile("Metadata.fometa-client", metadata);
    FileSystem compiler_resources;
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerClientEngine compiler {compiler_resources};
    source->AddFile("ClientEntityLifetime.fos-bin-client", BakerTests::CompileInlineScripts(&compiler, "ClientEntityLifetimeScripts", {{"Scripts/Lifetime.fos", "void LifetimeFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    for (auto& [path, data] : extra_resources) {
        source->AddFile(path, std::move(data));
    }

    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return safe_alloc::make_refcounted<ClientEngine>(&settings, std::move(resources), &GetApp()->MainWindow);
}

#if FO_ENABLE_3D
static auto MakeScratchModelResources() -> vector<pair<string, vector<uint8_t>>>
{
    constexpr string_view model_path = "Models/ScratchLifetime.fo3d";
    constexpr string_view mesh_path = "Models/ScratchLifetime.fbx";
    ModelMeshData mesh;
    mesh.RootBone = safe_alloc::make_unique<ModelMeshBoneData>();
    mesh.RootBone->Name = "Root";
    mesh.RootBone->TransformationMatrix = mat44 {1.0f};
    mesh.RootBone->GlobalTransformationMatrix = mat44 {1.0f};
    ModelMeshGeometryData geometry;
    geometry.SkinBoneNames = {"Root"};
    geometry.SkinBoneOffsets = {mat44 {1.0f}};

    for (vec3 position : {vec3 {}, vec3 {1.0f, 0.0f, 0.0f}, vec3 {0.0f, 1.0f, 0.0f}}) {
        ModelMeshVertexData vertex {};
        vertex.Position = position;
        vertex.BlendWeights[0] = 1.0f;
        geometry.Vertices.emplace_back(vertex);
    }

    geometry.Indices = {0, 1, 2};
    mesh.RootBone->AttachedMesh = std::move(geometry);
    vector<uint8_t> mesh_blob;
    data_writer writer {mesh_blob};
    WriteModelMeshData(writer, mesh, "ScratchLifetime");
    BakerTests::TestRig rig;
    rig.AddSourceFile(model_path, "Model ScratchLifetime.fbx\n", 1);
    rig.AddSourceFile(mesh_path, "scratch mesh fixture", 1);
    rig.AddBakedFile(mesh_path, mesh_blob, 1);
    rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeEmptyMetadataBlob());
    ModelInfoBaker baker(rig.MakeContext(), [](string_view path, const File& file) -> ModelSourceAsset {
        ModelSourceAsset asset;
        asset.FileName = path;
        asset.WriteTime = file.GetWriteTime();
        asset.Skeleton.FileName = path;
        asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Root", .Hierarchy = {"Root"}, .RestLocalTransform = mat44 {1.0f}});
        return asset;
    });
    baker.BakeFiles(rig.GetAllSourceFiles(), "");
    REQUIRE(rig.Outputs.count(string {model_path}) == 1);
    vector<pair<string, vector<uint8_t>>> resources;
    resources.emplace_back(string {mesh_path}, std::move(mesh_blob));

    for (auto& [path, data] : rig.Outputs) {
        resources.emplace_back(path, std::move(data));
    }

    return resources;
}

TEST_CASE("ModelSpriteScratchTargetsAreReleasedByCacheCleanup")
{
    constexpr string_view model_path = "Models/ScratchLifetime.fo3d";
    auto settings = MakeClientLifetimeSettings();
    BakerTests::OverrideSetting(settings.Render.ModelSpriteMaxTextureWidth, int32_t {4096});
    BakerTests::OverrideSetting(settings.Render.ModelSpriteMaxTextureHeight, int32_t {4096});
    auto client = MakeClientLifetimeEngine(settings, MakeScratchModelResources());
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    auto sprite = factory->LoadSprite(client->Hashes.to_hashed_string(model_path), AtlasType::MapSprites).dyn_cast<ModelSprite>();
    REQUIRE(sprite);
    size_t initial_targets = client->SprMngr.GetRtMngr().GetRenderTargetCount();

    SECTION("Changing frame sizes does not retain every temporary target")
    {
        for (int32_t size = 512; size < 1152; size += 32) {
            sprite->SetSize({size, size});
            sprite->DrawToAtlas();
        }

        CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() <= initial_targets + 12);
    }

    SECTION("Cache cleanup releases scratch targets while preserving a live sprite")
    {
        for (int32_t size = 256; size < 768; size += 32) {
            sprite->SetSize({size, size});
            sprite->DrawToAtlas();
        }

        client->SprMngr.CleanupSpriteCache();
        CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() <= initial_targets + 1);
        CHECK_FALSE(client->EffectMngr.Effects.FlushRenderTarget->MainTex);
        REQUIRE(sprite->GetAtlas());
        sprite->SetSize({256, 256});
        REQUIRE_NOTHROW(sprite->DrawToAtlas());
        CHECK(client->SprMngr.GetRtMngr().GetRenderTargetStack().empty());
    }

    SECTION("An oversized frame occupies the scratch cache alone")
    {
        // The headless application's default 2048 texture cap cannot admit a frame beyond this cache budget
        int32_t previous_max_width = AppRender::MAX_ATLAS_WIDTH;
        int32_t previous_max_height = AppRender::MAX_ATLAS_HEIGHT;
        auto restore_texture_caps = scope_exit([&]() noexcept {
            AppRender::MAX_ATLAS_WIDTH = previous_max_width;
            AppRender::MAX_ATLAS_HEIGHT = previous_max_height;
        });
        AppRender::MAX_ATLAS_WIDTH = 4096;
        AppRender::MAX_ATLAS_HEIGHT = 4096;
        sprite->SetSize({512, 512});
        sprite->DrawToAtlas();
        auto large_sprite = factory->LoadSprite(client->Hashes.to_hashed_string(model_path), AtlasType::MapSprites).dyn_cast<ModelSprite>();
        REQUIRE(large_sprite);
        large_sprite->SetSize({1536, 1536});
        large_sprite->DrawToAtlas();
        isize32 large_size = client->EffectMngr.Effects.FlushRenderTarget->MainTex->Size;
        CAPTURE(large_size.width, large_size.height);
        REQUIRE(numeric_cast<uint64_t>(large_size.width) * numeric_cast<uint64_t>(large_size.height) > 8 * 1024 * 1024);
        CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() <= initial_targets + 2);
        REQUIRE_NOTHROW(sprite->DrawToAtlas());
        isize32 returned_size = client->EffectMngr.Effects.FlushRenderTarget->MainTex->Size;
        CAPTURE(returned_size.width, returned_size.height);
        CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() <= initial_targets + 2);
    }

    SECTION("Recently used fitting sizes reuse their existing targets")
    {
        sprite->SetSize({512, 512});
        sprite->DrawToAtlas();
        nptr<const RenderTexture> first_texture = client->EffectMngr.Effects.FlushRenderTarget->MainTex;
        isize32 first_size = first_texture->Size;
        auto second_sprite = factory->LoadSprite(client->Hashes.to_hashed_string(model_path), AtlasType::MapSprites).dyn_cast<ModelSprite>();
        REQUIRE(second_sprite);
        second_sprite->SetSize({768, 768});
        second_sprite->DrawToAtlas();
        size_t targets_after_two_sizes = client->SprMngr.GetRtMngr().GetRenderTargetCount();
        // Redraw the settled frame; a new size request can require a different root-relative placement
        sprite->DrawToAtlas();
        isize32 returned_size = client->EffectMngr.Effects.FlushRenderTarget->MainTex->Size;
        CAPTURE(first_size.width, first_size.height, returned_size.width, returned_size.height);
        CHECK(client->EffectMngr.Effects.FlushRenderTarget->MainTex == first_texture);
        CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() == targets_after_two_sizes);
    }
}

#endif

TEST_CASE("MapViewRenderTargetsAreReleasedOnDestroy")
{
    GlobalSettings settings = MakeClientLifetimeSettings();
    bool direct_draw = false;
    bool disable_mask = false;

    SECTION("DefaultSettings")
    {
    }
    SECTION("DisabledIndoorMask")
    {
        disable_mask = true;
    }
    SECTION("DirectDraw")
    {
        direct_draw = true;
    }

    BakerTests::OverrideSetting(settings.View.MapDirectDraw, direct_draw);
    BakerTests::OverrideSetting(settings.View.DisableIndoorMask, disable_mask);
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    ProtoMap proto {client->Hashes.to_hashed_string("LifetimeMap"), client->GetPropertyRegistrar(MapView::ENTITY_TYPE_NAME)};
    proto.SetSize(msize {16, 16});
    auto& targets = client->SprMngr.GetRtMngr();
    size_t baseline = targets.GetRenderTargetCount();
    size_t map_targets = direct_draw ? 1 : (disable_mask ? 2 : 3);

    for (int32_t cycle = 0; cycle < 5; cycle++) {
        auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, make_ptr(&proto), isize32 {320, 200}, nullptr);
        CHECK(targets.GetRenderTargetCount() == baseline + map_targets);
        map->DestroySelf();
        CHECK(targets.GetRenderTargetCount() == baseline);
    }
}

TEST_CASE("MapViewDestroyClearsOnlyItsCachedIndoorMaskReferences")
{
    GlobalSettings settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    ProtoMap proto {client->Hashes.to_hashed_string("LifetimeMap"), client->GetPropertyRegistrar(MapView::ENTITY_TYPE_NAME)};
    proto.SetSize(msize {16, 16});
    auto sprite = client->SprMngr.LoadSprite("LifetimeSprite.png", AtlasType::IfaceSprites);
    REQUIRE(sprite);
    auto& targets = client->SprMngr.GetRtMngr();
    size_t baseline = targets.GetRenderTargetCount();
    auto outer_target = targets.CreateRenderTarget(false, {320, 200}, false);
    targets.PushRenderTarget(outer_target);
    auto outer_cleanup = scope_exit([&]() noexcept {
        safe_call([&] {
            targets.PopRenderTarget();
            targets.DeleteRenderTarget(outer_target);
        });
    });
    auto mask_a = client->EffectMngr.LoadEffect(EffectUsage::QuadSprite, "Effects/LifetimeMaskA.fofx");
    auto mask_b = client->EffectMngr.LoadEffect(EffectUsage::QuadSprite, "Effects/LifetimeMaskB.fofx");
    auto mask_c = client->EffectMngr.LoadEffect(EffectUsage::QuadSprite, "Effects/LifetimeMaskC.fofx");
    REQUIRE(mask_a);
    REQUIRE(mask_b);
    REQUIRE(mask_c);
    client->EffectMngr.Effects.FlushMap = mask_a;
    auto first = safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, make_ptr(&proto), isize32 {320, 200}, nullptr);
    auto first_cleanup = scope_exit([&]() noexcept {
        safe_call([&] {
            if (!first->IsDestroyed()) {
                first->DestroySelf();
            }
        });
    });
    first->DrawMap();
    REQUIRE(mask_a->IndoorMaskTex);
    auto retiring_texture = mask_a->IndoorMaskTex;
    mask_b->IndoorMaskTex = mask_a->IndoorMaskTex;
    client->EffectMngr.Effects.FlushMap = mask_c;
    auto second = safe_alloc::make_refcounted<MapView>(client, ident_t {1002}, make_ptr(&proto), isize32 {320, 200}, nullptr);
    auto second_cleanup = scope_exit([&]() noexcept {
        safe_call([&] {
            if (!second->IsDestroyed()) {
                second->DestroySelf();
            }
        });
    });
    second->DrawMap();
    REQUIRE(mask_c->IndoorMaskTex);
    auto second_texture = mask_c->IndoorMaskTex;
    REQUIRE(second_texture != mask_a->IndoorMaskTex);
    LifetimeMaskReadEffect queued_effect {make_ptr(&targets)};
    queued_effect.IndoorMaskTex = retiring_texture;
    sprite->SetDrawEffect(make_nptr(&queued_effect));
    client->SprMngr.DrawSprite(sprite, {}, ucolor {255, 255, 255, 255});
    CHECK(queued_effect.Draws == 0);
    first->DestroySelf();
    CHECK(queued_effect.Draws == 1);
    CHECK(queued_effect.OwnersAtDraw == baseline + 7);
    client->SprMngr.Flush();
    sprite->SetDrawEffect(nullptr);
    queued_effect.IndoorMaskTex = nullptr;
    CHECK_FALSE(mask_a->IndoorMaskTex);
    CHECK_FALSE(mask_b->IndoorMaskTex);
    CHECK(mask_c->IndoorMaskTex == second_texture);
    CHECK(targets.GetRenderTargetCount() == baseline + 4);
    REQUIRE(targets.GetRenderTargetStack().size() == 1);
    CHECK(targets.GetCurrentRenderTarget() == nptr<RenderTarget> {outer_target});
    CHECK(client->SprMngr.GetRender().GetRenderTarget() == nptr<RenderTexture> {outer_target->GetTexture()});
    CHECK_NOTHROW(second->DrawMap());
    second->DestroySelf();
    CHECK_FALSE(mask_c->IndoorMaskTex);
    CHECK(targets.GetRenderTargetCount() == baseline + 1);
}

TEST_CASE("ClientEngineFinishesStartingUpOnceItRuns")
{
    // An engine is starting up from the moment it exists, so the client owes the finish; one that never
    // performed it reported every one-shot load of its own construction as a responsiveness failure
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    CHECK_FALSE(client->IsStartingUp());

    // There is no way back into the state, so a second finish is a start path running twice
    CHECK_THROWS_AS(client->FinishStartingUp(), VerificationException);
}

TEST_CASE("ClientEntityLookupRetainsUntilCallerFinishes")
{
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto player = safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001});

    {
        auto retained = client->GetEntity(ident_t {1001});
        REQUIRE(retained);
        REQUIRE(player->GetRefCount() == 2);

        std::thread release_thread {[owner = std::move(player)] { ignore_unused(owner); }};
        release_thread.join();

        CHECK(retained->GetId() == ident_t {1001});
        CHECK(retained->GetRefCount() == 1);
    }

    CHECK_FALSE(client->GetEntity(ident_t {1001}));
}

TEST_CASE("ClientEntityFinalReleasePreservesSuccessorRegistration")
{
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto old_player = safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001});
    auto new_player = safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001});
    std::thread release_thread {[owner = std::move(old_player)] { ignore_unused(owner); }};
    release_thread.join();

    CHECK(client->GetEntity(ident_t {1001}) == new_player);
    new_player->DestroySelf();
    CHECK_FALSE(client->GetEntity(ident_t {1001}));
}

TEST_CASE("ClientMapUnloadReleasesRenderTargetsWithRetainedHandles")
{
    auto settings = MakeClientLifetimeSettings();
    BakerTests::OverrideSetting(settings.View.MapDirectDraw, false);
    BakerTests::OverrideSetting(settings.View.DisableIndoorMask, false);
    BakerTests::OverrideSetting(settings.View.DisableLighting, false);
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto proto = safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("LifetimeMap"), client->GetPropertyRegistrar("Map").as_ptr());
    proto->SetSize(msize {500, 500});
    auto& rt_mngr = client->SprMngr.GetRtMngr();
    size_t initial_targets = rt_mngr.GetRenderTargetCount();

    // Warm the committed pages the allocator keeps for reuse before measuring repeated unloads
    {
        auto map = safe_alloc::make_refcounted<MapView>(client.as_ptr(), ident_t {9000}, proto.as_ptr(), isize32 {320, 200});
        auto destroy_map = scope_exit([&map]() noexcept {
            safe_call([&map] {
                if (!map->IsDestroyed()) {
                    map->DestroySelf();
                }
            });
        });
        CHECK(rt_mngr.GetRenderTargetCount() == initial_targets + 3);
        map->DestroySelf();
        CHECK(map->IsDestroyed());
        CHECK_FALSE(client->GetEntity(map->GetId()));
        CHECK(rt_mngr.GetRenderTargetCount() == initial_targets);
    }

    size_t initial_memory = memory::get_in_use_bytes();
    vector<refcount_ptr<MapView>> retired_maps;

    for (uint32_t cycle = 0; cycle < 12; cycle++) {
        auto map = safe_alloc::make_refcounted<MapView>(client.as_ptr(), ident_t {9001 + cycle}, proto.as_ptr(), isize32 {320, 200});
        auto destroy_map = scope_exit([&map]() noexcept {
            safe_call([&map] {
                if (!map->IsDestroyed()) {
                    map->DestroySelf();
                }
            });
        });
        CHECK(rt_mngr.GetRenderTargetCount() == initial_targets + 3);
        map->DestroySelf();
        CHECK(map->IsDestroyed());
        CHECK_FALSE(client->GetEntity(map->GetId()));
        CHECK(rt_mngr.GetRenderTargetCount() == initial_targets);
        retired_maps.emplace_back(map);
    }

    // Managed wrappers can retain every retired map; native storage must be released before their finalizers run
    if (initial_memory != 0) {
        size_t retained_memory = memory::get_in_use_bytes();
        INFO("Retained map memory: " << retained_memory << "; initial: " << initial_memory);
        CHECK(retained_memory < initial_memory + 8 * 1024 * 1024);
    }
}

TEST_CASE("ClientLargeMapFieldsStayWithinMemoryBudget")
{
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto proto = safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("LargeSparseMap"), client->GetPropertyRegistrar("Map"));
    proto->SetSize(msize {1200, 1200});
    proto->SetWorkHex(mpos {200, 200});

    size_t initial_memory = platform::get_process_private_memory_usage();
    auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {9001}, proto, isize32 {320, 200});
    auto destroy_map = scope_exit([&map]() noexcept {
        safe_call([&map] {
            if (!map->IsDestroyed()) {
                map->DestroySelf();
            }
        });
    });
    size_t map_memory = platform::get_process_private_memory_usage();

    // Measure the real constructor, including view and light buffers, without optional allocator diagnostics
    if (initial_memory != 0 && map_memory != 0) {
        INFO("Large map private memory: " << map_memory << "; initial: " << initial_memory);
        CHECK(map_memory < initial_memory + 64 * 1024 * 1024);
    }

    map->SetScrollCheck(false);
    map->InstantScrollTo(mpos {300, 300});
    map->InstantScrollTo(mpos {200, 200});
    REQUIRE(map->GetField(mpos {200, 200}).IsView);
    ptr<const MapView::Field> first_field = &map->GetField(mpos {200, 200});

    for (mpos center : {mpos {500, 500}, mpos {900, 900}, mpos {1100, 1100}}) {
        map->InstantScrollTo(center);
        CHECK(map->GetField(center).IsView);
        CHECK(&map->GetField(mpos {200, 200}) == first_field);
    }

    CHECK_FALSE(first_field->IsView);
    CHECK(map->GetField(mpos {1199, 1199}).Items.empty());
    CHECK_FALSE(map->GetField(mpos {1199, 1199}).MoveBlocked);

    map->EnableMapperMode();
    map->InstantScrollTo(mpos {200, 200});
    map->Resize(msize {600, 600});
    CHECK(map->GetField(mpos {200, 200}).IsView);
    map->Resize(msize {1200, 1200});
    CHECK_FALSE(map->GetField(mpos {1100, 1100}).IsView);
    CHECK(map->GetField(mpos {1100, 1100}).Items.empty());
    map->InstantScrollTo(mpos {1100, 1100});
    CHECK(map->GetField(mpos {1100, 1100}).IsView);
    map->DestroySelf();
    CHECK_FALSE(client->GetEntity(map->GetId()));
}

TEST_CASE("ClientLargeMapScrollBoundsStayWithinMemoryBudget")
{
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto proto = safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("LargeScrollBoundedMap"), client->GetPropertyRegistrar("Map"));
    proto->SetSize(msize {1200, 1200});
    proto->SetWorkHex(mpos {301, 183});
    proto->SetScrollAxialArea(irect32 {-380, 230, 356, 243});

    size_t initial_memory = platform::get_process_private_memory_usage();
    auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {9001}, proto, isize32 {320, 200});
    auto destroy_map = scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); });
    size_t map_memory = platform::get_process_private_memory_usage();

    if (initial_memory == 0 || map_memory == 0) {
        SKIP("Process-private-memory measurement is unavailable on this platform");
    }

    // Scroll-block lines must not materialize the rest of a large, empty map
    INFO("Scroll-bounded map private memory: " << map_memory << "; initial: " << initial_memory);
    CHECK(map_memory < initial_memory + 64 * 1024 * 1024);
}

TEST_CASE("ClientMapConstructionFailureReleasesRenderTargets")
{
    auto settings = MakeClientLifetimeSettings();
    BakerTests::OverrideSetting(settings.View.MapDirectDraw, false);
    BakerTests::OverrideSetting(settings.View.DisableIndoorMask, false);
    BakerTests::OverrideSetting(settings.View.DisableLighting, false);
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto proto = safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("InvalidLifetimeMap"), client->GetPropertyRegistrar("Map").as_ptr());
    proto->SetSize(msize {0, 0});
    size_t initial_targets = client->SprMngr.GetRtMngr().GetRenderTargetCount();

    CHECK_THROWS_AS(safe_alloc::make_refcounted<MapView>(client.as_ptr(), ident_t {9001}, proto.as_ptr(), isize32 {320, 200}), VerificationException);
    CHECK_FALSE(client->GetEntity(ident_t {9001}));
    CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() == initial_targets);
}

TEST_CASE("ClientMapUnloadDropsPendingItemOwners")
{
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto map_proto = safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("PendingLifetimeMap"), client->GetPropertyRegistrar("Map").as_ptr());
    map_proto->SetSize(msize {32, 32});
    auto item_proto = safe_alloc::make_refcounted<ProtoItem>(client->Hashes.to_hashed_string("PendingLifetimeItem"), client->GetPropertyRegistrar("Item").as_ptr());
    auto map = safe_alloc::make_refcounted<MapView>(client.as_ptr(), ident_t {9001}, map_proto.as_ptr(), isize32 {320, 200});
    auto destroy_map = scope_exit([&map]() noexcept {
        safe_call([&map] {
            if (!map->IsDestroyed()) {
                map->DestroySelf();
            }
        });
    });
    auto item = safe_alloc::make_refcounted<ItemHexView>(map.as_ptr(), ident_t {}, item_proto.as_ptr());
    map->RefreshItem(item, true);
    REQUIRE(item->GetRefCount() == 2);
    item->DestroySelf();
    CHECK(item->GetRefCount() == 2);
    map->DestroySelf();
    CHECK(item->GetRefCount() == 1);
}

TEST_CASE("ExpiredOneImageAtlasReleasesRenderTarget")
{
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto atlas_mngr = client->SprMngr.GetAtlasMngr();
    auto& rt_mngr = client->SprMngr.GetRtMngr();
    size_t initial_targets = rt_mngr.GetRenderTargetCount();

    for (uint32_t cycle = 0; cycle < 12; cycle++) {
        auto [atlas, allocation, pos] = atlas_mngr->FindAtlasPlace(AtlasType::OneImage, isize32 {16, 16});
        ignore_unused(atlas, pos);
        CHECK(rt_mngr.GetRenderTargetCount() == initial_targets + 1);
    }
}

TEST_CASE("AtlasCleanupReleasesOnlyEmptyPages")
{
    auto settings = MakeClientLifetimeSettings();
    RenderTargetManager rt_mngr(GetApp()->MainWindow.GetRender(), [] { });
    TextureAtlasManager atlas_mngr(&settings, &rt_mngr);
    size_t initial_targets = rt_mngr.GetRenderTargetCount();
    auto [atlas1, allocation1, pos1] = atlas_mngr.FindAtlasPlace(AtlasType::MapSprites, isize32 {16, 16});
    auto [atlas2, allocation2, pos2] = atlas_mngr.FindAtlasPlace(AtlasType::MapSprites, isize32 {16, 16});
    ignore_unused(pos1, pos2);
    unique_del_nptr<TextureAtlasLayout::Allocation> owner1 = std::move(allocation1);
    unique_del_nptr<TextureAtlasLayout::Allocation> owner2 = std::move(allocation2);
    REQUIRE(atlas1 == atlas2);
    CHECK(rt_mngr.GetRenderTargetCount() == initial_targets + 1);

    owner1 = nullptr;
    atlas_mngr.CleanupAtlases();
    CHECK(rt_mngr.GetRenderTargetCount() == initial_targets + 1);
    CHECK_FALSE(atlas2->GetLayout()->IsEmpty());

    owner2 = nullptr;
    atlas_mngr.CleanupAtlases();
    CHECK(rt_mngr.GetRenderTargetCount() == initial_targets);
    atlas_mngr.CleanupAtlases();
    CHECK(rt_mngr.GetRenderTargetCount() == initial_targets);
}

// Hidden: compare actual client-field access; timings are evidence, not a flaky pass/fail threshold
TEST_CASE("ClientMapFieldAccessCost", "[.]")
{
    vector<mpos> positions;

    for (int16_t y = 160; y < 224; y++) {
        for (int16_t x = 256; x < 352; x++) {
            positions.emplace_back(x, y);
        }
    }

    auto compare = [&positions](size_t populated_stride, bool scattered, bool writing) {
        if (scattered) {
            for (size_t index = 0; index < positions.size(); index++) {
                positions[index] = mpos {numeric_cast<int16_t>(index * 137 % 1200), numeric_cast<int16_t>(index * 71 % 1200)};
            }
        }

        auto measure = [&](auto& grid, string_view name) {
            if (populated_stride != 0) {
                for (size_t index = 0; index < positions.size(); index += populated_stride) {
                    grid.GetCellForWriting(positions[index])->MoveBlocked = true;
                }
            }

            array<double, 7> samples {};
            uint64_t checksum = 0;

            for (double& sample : samples) {
                auto started = std::chrono::steady_clock::now();

                for (size_t iteration = 0; iteration < 128; iteration++) {
                    for (mpos pos : positions) {
                        if (writing) {
                            ptr<MapView::Field> field = grid.GetCellForWriting(pos);
                            field->IsView = !field->IsView;
                            checksum += field->IsView;
                        }
                        else {
                            checksum += grid.GetCellForReading(pos).MoveBlocked;
                        }
                    }
                }

                sample = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            }

            std::sort(samples.begin(), samples.end());
            WARN(strex("field access {} stride={} scattered={} writing={} median={:.3f} ms checksum={}", name, populated_stride, scattered, writing, samples[3], checksum).str());
            return checksum;
        };

        uint64_t dense_checksum = 0;
        {
            StaticTwoDimensionalGrid<MapView::Field, mpos, msize> dense {{1200, 1200}};
            dense_checksum = measure(dense, "dense");
        }
        {
            ChunkedTwoDimensionalGrid<MapView::Field, mpos, msize, GameSettings::CLIENT_MAP_CHUNK_SIDE> chunked {{1200, 1200}};
            CHECK(measure(chunked, "chunked") == dense_checksum);
        }
        {
            DynamicTwoDimensionalGrid<MapView::Field, mpos, msize> dynamic {{1200, 1200}};
            CHECK(measure(dynamic, "hash") == dense_checksum);
        }
    };

    compare(1, false, false);
    compare(8, false, false);
    compare(0, false, false);
    compare(1, false, true);
    compare(8, true, false);
}

FO_END_NAMESPACE
