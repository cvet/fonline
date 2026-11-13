// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "Application.h"
#include "Client.h"
#include "CritterView.h"
#include "ItemHexView.h"
#include "MapLoader.h"
#include "MapView.h"
#include "ModelInfoBaker.h"
#include "ModelMeshData.h"
#include "ModelSprites.h"
#include "NetBuffer.h"
#include "PlayerView.h"
#include "Server.h"
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
    GlobalSettings settings {false};
    settings.SetupDefaultSettings();
    settings.ApplyAutoSettings({});
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    return settings;
}

static auto MakeClientLifetimeEngine(GlobalSettings& settings, vector<pair<string, vector<uint8_t>>> extra_resources = {}) -> refcount_ptr<ClientEngine>
{
    vector<uint8_t> metadata {BakerTests::MakeEmptyMetadataBlob()};
    auto source {safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientEntityLifetime")};
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
    auto compiler_source {safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientEntityLifetimeCompiler")};
    compiler_source->AddFile("Metadata.fometa-client", metadata);
    FileSystem compiler_resources {};
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerClientEngine compiler {compiler_resources};
    source->AddFile("ClientEntityLifetime.fos-bin-client", BakerTests::CompileInlineScripts(&compiler, "ClientEntityLifetimeScripts", {{"Scripts/Lifetime.fos", "void LifetimeFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    for (auto& [path, data] : extra_resources) {
        source->AddFile(path, std::move(data));
    }

    FileSystem resources {};
    resources.AddCustomSource(std::move(source));
    return safe_alloc::make_refcounted<ClientEngine>(&settings, std::move(resources), &GetApp()->MainWindow);
}

TEST_CASE("ClientRemoteCallsRequireConnectedSession")
{
    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto call {client->Hashes.to_hashed_string("DisconnectedProbe")};

    CHECK_FALSE(client->IsConnected());
    CHECK_THROWS_AS(client->SendRemoteCall(call, client, {}), VerificationException);
    CHECK(client->GetConnection()->OutBuf->IsEmpty());
}

TEST_CASE("ClientBinaryCompatibilityPrecedesPlayerSession")
{
    bool compatible {GENERATE(true, false)};
    CAPTURE(compatible);
    static std::atomic_uint16_t next_port {48200};
    auto port {next_port.fetch_add(1)};
    GlobalSettings server_settings {false};
    server_settings.SetupDefaultSettings();
    server_settings.ApplyAutoSettings({});
    BakerTests::ApplySelfContainedServerSettings(server_settings);
    BakerTests::OverrideSetting(server_settings.Network.ServerPort, port);
    auto client_settings {MakeClientLifetimeSettings()};
    BakerTests::OverrideSetting(client_settings.Network.ServerPort, port);
    if (!compatible) {
        string different_version {server_settings.Network.CompatibilityVersion};
        REQUIRE(!different_version.empty());
        different_version.front() = different_version.front() == '0' ? '1' : '0';
        BakerTests::OverrideSetting(client_settings.Network.CompatibilityVersion, different_version);
    }

    auto server_source {safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientCompatibilityServer")};
    server_source->AddFile("Metadata.fometa-server", BakerTests::MakeEmptyMetadataBlob());
#if FO_ANGELSCRIPT_SCRIPTING
    auto compiler_source {safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientCompatibilityCompiler")};
    compiler_source->AddFile("Metadata.fometa-server", BakerTests::MakeEmptyMetadataBlob());
    FileSystem compiler_resources {};
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerServerEngine compiler {compiler_resources};
    server_source->AddFile("ClientCompatibility.fos-bin-server", BakerTests::CompileInlineScripts(&compiler, "ClientCompatibilityScripts", {{"Scripts/Compatibility.fos", "void CompatibilityFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    FileSystem server_resources {};
    server_resources.AddCustomSource(std::move(server_source));
    auto server {safe_alloc::make_refcounted<ServerEngine>(&server_settings, std::move(server_resources))};
    auto client {MakeClientLifetimeEngine(client_settings)};
    auto shutdown {scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    for (int32_t i = 0; i < 3000 && !server->IsStarted() && !server->IsStartingError(); i++) {
        coarse_sleep(std::chrono::milliseconds {10});
    }
    REQUIRE(server->IsStarted());
    REQUIRE(client->GetMetadataVersion() == server->GetMetadataVersion());
    client->Connect();
    bool refused {false};
    for (int32_t i = 0; i < 2000 && !refused && !client->IsConnected(); i++) {
        try {
            client->MainLoop();
        }
        catch (const ResourcesOutdatedException&) {
            refused = true;
        }
        if (!refused && !client->IsConnected()) {
            coarse_sleep(std::chrono::milliseconds {2});
        }
    }
    CHECK(refused == !compatible);
    CHECK(client->IsConnected() == compatible);
    CHECK(static_cast<bool>(client->GetCurPlayer()) == compatible);
    client->Disconnect();

    if (compatible) {
        auto call {client->Hashes.to_hashed_string("DisconnectedProbe")};
        CHECK_THROWS_AS(client->SendRemoteCall(call, client, {}), VerificationException);
        CHECK(client->GetConnection()->OutBuf->IsEmpty());

        client->Connect();

        for (int32_t i = 0; i < 2000 && !client->IsConnected(); i++) {
            client->MainLoop();

            if (!client->IsConnected()) {
                coarse_sleep(std::chrono::milliseconds {2});
            }
        }

        CHECK(client->IsConnected());
        CHECK(static_cast<bool>(client->GetCurPlayer()));
        client->Disconnect();
    }
}

#if FO_ENABLE_3D
static auto MakeScratchModelResources() -> vector<pair<string, vector<uint8_t>>>
{
    constexpr string_view model_path {"Models/ScratchLifetime.fo3d"};
    constexpr string_view mesh_path {"Models/ScratchLifetime.fbx"};
    ModelMeshData mesh {};
    mesh.RootBone = safe_alloc::make_unique<ModelMeshBoneData>();
    mesh.RootBone->Name = "Root";
    mesh.RootBone->TransformationMatrix = mat44 {1.0f};
    mesh.RootBone->GlobalTransformationMatrix = mat44 {1.0f};
    ModelMeshGeometryData geometry {};
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
    vector<uint8_t> mesh_blob {};
    data_writer writer {mesh_blob};
    WriteModelMeshData(writer, mesh, "ScratchLifetime");
    BakerTests::TestRig rig {};
    rig.AddSourceFile(model_path, "Model ScratchLifetime.fbx\n", 1);
    rig.AddSourceFile(mesh_path, "scratch mesh fixture", 1);
    rig.AddBakedFile(mesh_path, mesh_blob, 1);
    rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeEmptyMetadataBlob());
    ModelInfoBaker baker {rig.MakeContext(), [](string_view path, const File& file) -> ModelSourceAsset {
                              ModelSourceAsset asset {};
                              asset.FileName = path;
                              asset.WriteTime = file.GetWriteTime();
                              asset.Skeleton.FileName = path;
                              asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Root", .Hierarchy = {"Root"}, .RestLocalTransform = mat44 {1.0f}});
                              return asset;
                          }};

    baker.BakeFiles(rig.GetAllSourceFiles(), "");
    REQUIRE(rig.Outputs.count(string {model_path}) == 1);
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back(string {mesh_path}, std::move(mesh_blob));

    for (auto& [path, data] : rig.Outputs) {
        resources.emplace_back(path, std::move(data));
    }

    return resources;
}

TEST_CASE("ModelSpriteScratchTargetsAreReleasedByCacheCleanup")
{
    constexpr string_view model_path {"Models/ScratchLifetime.fo3d"};
    auto settings {MakeClientLifetimeSettings()};
    BakerTests::OverrideSetting(settings.Render.ModelSpriteMaxTextureWidth, int32_t {4096});
    BakerTests::OverrideSetting(settings.Render.ModelSpriteMaxTextureHeight, int32_t {4096});
    auto client {MakeClientLifetimeEngine(settings, MakeScratchModelResources())};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto factory {client->ResMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()};
    REQUIRE(factory);
    auto sprite {factory->LoadSprite(client->Hashes.to_hashed_string(model_path), AtlasType::MapSprites).dyn_cast<ModelSprite>()};
    REQUIRE(sprite);
    size_t initial_targets {client->SprMngr.GetRtMngr().GetRenderTargetCount()};

    SECTION("Changing frame sizes does not retain every temporary target")
    {
        for (int32_t size {512}; size < 1152; size += 32) {
            sprite->SetSize({size, size});
            sprite->DrawToAtlas();
        }

        CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() <= initial_targets + 12);
    }

    SECTION("Cache cleanup releases scratch targets while preserving a live sprite")
    {
        for (int32_t size {256}; size < 768; size += 32) {
            sprite->SetSize({size, size});
            sprite->DrawToAtlas();
        }

        client->ResMngr.CleanupSpriteCache();
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
        int32_t previous_max_width {AppRender::MAX_ATLAS_WIDTH};
        int32_t previous_max_height {AppRender::MAX_ATLAS_HEIGHT};
        auto restore_texture_caps {scope_exit([&]() noexcept {
            AppRender::MAX_ATLAS_WIDTH = previous_max_width;
            AppRender::MAX_ATLAS_HEIGHT = previous_max_height;
        })};

        AppRender::MAX_ATLAS_WIDTH = 4096;
        AppRender::MAX_ATLAS_HEIGHT = 4096;
        sprite->SetSize({512, 512});
        sprite->DrawToAtlas();
        auto large_sprite {factory->LoadSprite(client->Hashes.to_hashed_string(model_path), AtlasType::MapSprites).dyn_cast<ModelSprite>()};
        REQUIRE(large_sprite);
        large_sprite->SetSize({1536, 1536});
        large_sprite->DrawToAtlas();
        isize32 large_size {client->EffectMngr.Effects.FlushRenderTarget->MainTex->Size};
        CAPTURE(large_size.width, large_size.height);
        REQUIRE(numeric_cast<uint64_t>(large_size.width) * numeric_cast<uint64_t>(large_size.height) > 8 * 1024 * 1024);
        CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() <= initial_targets + 2);
        REQUIRE_NOTHROW(sprite->DrawToAtlas());
        isize32 returned_size {client->EffectMngr.Effects.FlushRenderTarget->MainTex->Size};
        CAPTURE(returned_size.width, returned_size.height);
        CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() <= initial_targets + 2);
    }

    SECTION("Recently used fitting sizes reuse their existing targets")
    {
        sprite->SetSize({512, 512});
        sprite->DrawToAtlas();
        nptr<const RenderTexture> first_texture {client->EffectMngr.Effects.FlushRenderTarget->MainTex};
        isize32 first_size {first_texture->Size};
        auto second_sprite {factory->LoadSprite(client->Hashes.to_hashed_string(model_path), AtlasType::MapSprites).dyn_cast<ModelSprite>()};
        REQUIRE(second_sprite);
        second_sprite->SetSize({768, 768});
        second_sprite->DrawToAtlas();
        size_t targets_after_two_sizes {client->SprMngr.GetRtMngr().GetRenderTargetCount()};
        // Redraw the settled frame; a new size request can require a different root-relative placement
        sprite->DrawToAtlas();
        isize32 returned_size {client->EffectMngr.Effects.FlushRenderTarget->MainTex->Size};
        CAPTURE(first_size.width, first_size.height, returned_size.width, returned_size.height);
        CHECK(client->EffectMngr.Effects.FlushRenderTarget->MainTex == first_texture);
        CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() == targets_after_two_sizes);
    }
}

#endif

TEST_CASE("MapViewRenderTargetsAreReleasedOnDestroy")
{
    GlobalSettings settings {MakeClientLifetimeSettings()};
    bool direct_draw {false};
    bool disable_mask {false};

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
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    ProtoMap proto {client->Hashes.to_hashed_string("LifetimeMap"), client->GetPropertyRegistrar(MapView::ENTITY_TYPE_NAME)};
    proto.SetSize(msize {16, 16});
    auto& targets {client->SprMngr.GetRtMngr()};
    size_t baseline {targets.GetRenderTargetCount()};
    size_t map_targets {direct_draw ? 1u : (disable_mask ? 2u : 3u)};

    for (int32_t cycle {0}; cycle < 5; cycle++) {
        auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, make_ptr(&proto), isize32 {320, 200}, nullptr)};
        CHECK(targets.GetRenderTargetCount() == baseline + map_targets);
        map->DestroySelf();
        CHECK(targets.GetRenderTargetCount() == baseline);
    }
}

TEST_CASE("MapViewDestroyClearsOnlyItsCachedIndoorMaskReferences")
{
    GlobalSettings settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    ProtoMap proto {client->Hashes.to_hashed_string("LifetimeMap"), client->GetPropertyRegistrar(MapView::ENTITY_TYPE_NAME)};
    proto.SetSize(msize {16, 16});
    auto sprite {client->ResMngr.LoadSprite("LifetimeSprite.png", AtlasType::IfaceSprites)};
    REQUIRE(sprite);
    auto& targets {client->SprMngr.GetRtMngr()};
    size_t baseline {targets.GetRenderTargetCount()};
    auto outer_target {targets.CreateRenderTarget(false, {320, 200}, false)};
    targets.PushRenderTarget(outer_target);
    auto outer_cleanup {scope_exit([&]() noexcept {
        safe_call([&] {
            targets.PopRenderTarget();
            targets.DeleteRenderTarget(outer_target);
        });
    })};

    auto mask_a {client->EffectMngr.LoadEffect(EffectUsage::QuadSprite, "Effects/LifetimeMaskA.fofx")};
    auto mask_b {client->EffectMngr.LoadEffect(EffectUsage::QuadSprite, "Effects/LifetimeMaskB.fofx")};
    auto mask_c {client->EffectMngr.LoadEffect(EffectUsage::QuadSprite, "Effects/LifetimeMaskC.fofx")};
    REQUIRE(mask_a);
    REQUIRE(mask_b);
    REQUIRE(mask_c);
    client->EffectMngr.Effects.FlushMap = mask_a;
    auto first {safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, make_ptr(&proto), isize32 {320, 200}, nullptr)};
    auto first_cleanup {scope_exit([&]() noexcept {
        safe_call([&] {
            if (!first->IsDestroyed()) {
                first->DestroySelf();
            }
        });
    })};

    first->DrawMap();
    REQUIRE(mask_a->IndoorMaskTex);
    auto retiring_texture {mask_a->IndoorMaskTex};
    mask_b->IndoorMaskTex = mask_a->IndoorMaskTex;
    client->EffectMngr.Effects.FlushMap = mask_c;
    auto second {safe_alloc::make_refcounted<MapView>(client, ident_t {1002}, make_ptr(&proto), isize32 {320, 200}, nullptr)};
    auto second_cleanup {scope_exit([&]() noexcept {
        safe_call([&] {
            if (!second->IsDestroyed()) {
                second->DestroySelf();
            }
        });
    })};

    second->DrawMap();
    REQUIRE(mask_c->IndoorMaskTex);
    auto second_texture {mask_c->IndoorMaskTex};
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
    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    CHECK_FALSE(client->IsStartingUp());

    // There is no way back into the state, so a second finish is a start path running twice
    CHECK_THROWS_AS(client->FinishStartingUp(), VerificationException);
}

TEST_CASE("ClientEntityLookupRetainsUntilCallerFinishes")
{
    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto player {safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001})};

    {
        auto retained {client->GetEntity(ident_t {1001})};
        REQUIRE(retained);
        REQUIRE(player->GetRefCount() == 2);

        std::thread release_thread {[owner = std::move(player)] { ignore_unused(owner); }};
        release_thread.join();

        CHECK(retained->GetId() == ident_t {1001});
        CHECK(retained->GetRefCount() == 1);
    }

    CHECK_FALSE(client->GetEntity(ident_t {1001}));
}

TEST_CASE("ClientCritterDestroyClearsAttachmentsBeforeDelayedFinalRelease")
{
    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto proto {safe_alloc::make_refcounted<ProtoCritter>(client->Hashes.to_hashed_string("AttachedCritter"), client->GetPropertyRegistrar("Critter").as_ptr())};
    auto passenger {safe_alloc::make_refcounted<CritterView>(client, ident_t {1003}, proto)};
    auto old_hull {safe_alloc::make_refcounted<CritterView>(client, ident_t {1002}, proto)};
    passenger->SetIsAttached(true);
    passenger->SetAttachMaster(old_hull->GetId());
    old_hull->SetAttachedCritters({passenger->GetId()});
    REQUIRE(old_hull->HasAttachedCritters());

    old_hull->DestroySelf();
    CHECK(old_hull->IsDestroyed());
    CHECK_FALSE(old_hull->HasAttachedCritters());
    CHECK_FALSE(old_hull->IsAttachedCritter(passenger->GetId()));
    CHECK_FALSE(passenger->IsDestroyed());
    CHECK(client->GetEntity(passenger->GetId()) == passenger);

    auto new_hull {safe_alloc::make_refcounted<CritterView>(client, ident_t {1002}, proto)};
    new_hull->SetAttachedCritters({passenger->GetId()});
    std::thread release_thread {[owner = std::move(old_hull)] { ignore_unused(owner); }};
    release_thread.join();
    CHECK(client->GetEntity(ident_t {1002}) == new_hull);
    CHECK(new_hull->IsAttachedCritter(passenger->GetId()));
    CHECK(passenger->GetAttachMaster() == new_hull->GetId());

    new_hull->DestroySelf();
    CHECK_FALSE(new_hull->HasAttachedCritters());
    passenger->DestroySelf();
}

TEST_CASE("ClientAuthoritativeInventorySlotMoveSwapsExclusivePeers")
{
    auto settings {MakeClientLifetimeSettings()};
    BakerTests::OverrideSetting(settings.Critter.CritterSlotMultiItem, vector<bool> {true, false, true});
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto critter_proto {safe_alloc::make_refcounted<ProtoCritter>(client->Hashes.to_hashed_string("SlotCritter"), client->GetPropertyRegistrar("Critter").as_ptr())};
    auto item_proto {safe_alloc::make_refcounted<ProtoItem>(client->Hashes.to_hashed_string("SlotItem"), client->GetPropertyRegistrar("Item").as_ptr())};
    auto critter {safe_alloc::make_refcounted<CritterView>(client, ident_t {1002}, critter_proto)};
    auto first {critter->AddMapperInvItem(ident_t {1003}, item_proto, CritterItemSlot::Main, nullptr)};
    auto second {critter->AddMapperInvItem(ident_t {1004}, item_proto, CritterItemSlot::Inventory, nullptr)};
    auto third {critter->AddMapperInvItem(ident_t {1005}, item_proto, CritterItemSlot::Inventory, nullptr)};
    auto cleanup {scope_exit([&critter]() noexcept { safe_call([&critter] { critter->DestroySelf(); }); })};

    critter->ApplyAuthoritativeItemSlotMove(second->GetId(), CritterItemSlot::Inventory, CritterItemSlot::Main);
    CHECK(first->GetCritterSlot() == CritterItemSlot::Inventory);
    CHECK(second->GetCritterSlot() == CritterItemSlot::Main);
    CHECK(third->GetCritterSlot() == CritterItemSlot::Inventory);

    critter->ApplyAuthoritativeItemSlotMove(first->GetId(), CritterItemSlot::Inventory, CritterItemSlot::Main);
    CHECK(first->GetCritterSlot() == CritterItemSlot::Main);
    CHECK(second->GetCritterSlot() == CritterItemSlot::Inventory);

    // The local predictor already made this swap before its authoritative echo
    first->SetCritterSlot(CritterItemSlot::Inventory);
    second->SetCritterSlot(CritterItemSlot::Main);
    critter->ApplyAuthoritativeItemSlotMove(second->GetId(), CritterItemSlot::Inventory, CritterItemSlot::Main);
    CHECK(first->GetCritterSlot() == CritterItemSlot::Inventory);
    CHECK(second->GetCritterSlot() == CritterItemSlot::Main);

    critter->ApplyAuthoritativeItemSlotMove(second->GetId(), CritterItemSlot::Main, CritterItemSlot::Inventory);
    CHECK(first->GetCritterSlot() == CritterItemSlot::Inventory);
    CHECK(second->GetCritterSlot() == CritterItemSlot::Inventory);
    CHECK(third->GetCritterSlot() == CritterItemSlot::Inventory);

    // Authored multi-item equipment slots have the same peer-preservation rule
    const auto multi_slot {static_cast<CritterItemSlot>(2)};
    first->SetCritterSlot(multi_slot);
    critter->ApplyAuthoritativeItemSlotMove(second->GetId(), CritterItemSlot::Inventory, multi_slot);
    CHECK(first->GetCritterSlot() == multi_slot);
    CHECK(second->GetCritterSlot() == multi_slot);

    critter->ApplyAuthoritativeItemSlotMove(ident_t {9999}, CritterItemSlot::Inventory, CritterItemSlot::Main);
    CHECK(first->GetCritterSlot() == multi_slot);
    CHECK(second->GetCritterSlot() == multi_slot);
}

TEST_CASE("ClientAuthoritativeInventorySlotMoveStopsAfterReentrantDestruction")
{
    auto settings {MakeClientLifetimeSettings()};
    BakerTests::OverrideSetting(settings.Critter.CritterSlotMultiItem, vector<bool> {true, false});
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto critter_proto {safe_alloc::make_refcounted<ProtoCritter>(client->Hashes.to_hashed_string("ReentrantSlotCritter"), client->GetPropertyRegistrar("Critter"))};
    auto item_proto {safe_alloc::make_refcounted<ProtoItem>(client->Hashes.to_hashed_string("ReentrantSlotItem"), client->GetPropertyRegistrar("Item"))};
    auto critter {safe_alloc::make_refcounted<CritterView>(client, ident_t {1101}, critter_proto)};
    auto callback_active {safe_alloc::make_shared<bool>(true)};
    auto cleanup {scope_exit([&critter, callback_active]() mutable noexcept {
        *callback_active = false;

        if (!critter->IsDestroyed()) {
            safe_call([&critter] { critter->DestroySelf(); });
        }
    })};
    auto displaced {critter->AddMapperInvItem(ident_t {1102}, item_proto, CritterItemSlot::Main, nullptr)};
    auto moved {critter->AddMapperInvItem(ident_t {1103}, item_proto, CritterItemSlot::Inventory, nullptr).hold_ref()};
    auto peer {critter->AddMapperInvItem(ident_t {1104}, item_proto, CritterItemSlot::Main, nullptr).hold_ref()};
    int32_t mutation {GENERATE(0, 1, 2)};
    CAPTURE(mutation);
    int32_t callback_count {0};

    displaced->GetPropertyCritterSlot()->AddPostSetter([&, callback_active](nptr<Entity> entity, ptr<const Property>) {
        if (*callback_active && entity == displaced) {
            callback_count++;

            if (mutation == 0) {
                critter->DeleteInvItem(moved);
            }
            else if (mutation == 1) {
                critter->DeleteInvItem(peer);
            }
            else {
                critter->DestroySelf();
            }
        }
    });

    critter->ApplyAuthoritativeItemSlotMove(moved->GetId(), CritterItemSlot::Inventory, CritterItemSlot::Main);

    CHECK(callback_count == 1);

    if (mutation == 1) {
        CHECK_FALSE(moved->IsDestroyed());
        CHECK(moved->GetCritterSlot() == CritterItemSlot::Main);
        CHECK(peer->IsDestroyed());
        CHECK(displaced->GetCritterSlot() == CritterItemSlot::Inventory);
    }
    else {
        CHECK(moved->IsDestroyed());
        CHECK(moved->GetCritterSlot() == CritterItemSlot::Inventory);
    }
}

TEST_CASE("ClientEntityFinalReleasePreservesSuccessorRegistration")
{
    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto old_player {safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001})};
    auto new_player {safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001})};
    std::thread release_thread {[owner = std::move(old_player)] { ignore_unused(owner); }};
    release_thread.join();

    CHECK(client->GetEntity(ident_t {1001}) == new_player);
    new_player->DestroySelf();
    CHECK_FALSE(client->GetEntity(ident_t {1001}));
}

TEST_CASE("ClientMapUnloadReleasesRenderTargetsWithRetainedHandles")
{
    auto settings {MakeClientLifetimeSettings()};
    BakerTests::OverrideSetting(settings.View.MapDirectDraw, false);
    BakerTests::OverrideSetting(settings.View.DisableIndoorMask, false);
    BakerTests::OverrideSetting(settings.View.DisableLighting, false);
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto proto {safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("LifetimeMap"), client->GetPropertyRegistrar("Map").as_ptr())};
    proto->SetSize(msize {500, 500});
    auto& rt_mngr {client->SprMngr.GetRtMngr()};
    size_t initial_targets {rt_mngr.GetRenderTargetCount()};

    // Warm the committed pages the allocator keeps for reuse before measuring repeated unloads
    {
        auto map {safe_alloc::make_refcounted<MapView>(client.as_ptr(), ident_t {9000}, proto.as_ptr(), isize32 {320, 200})};
        auto destroy_map {scope_exit([&map]() noexcept {
            safe_call([&map] {
                if (!map->IsDestroyed()) {
                    map->DestroySelf();
                }
            });
        })};

        CHECK(rt_mngr.GetRenderTargetCount() == initial_targets + 3);
        map->DestroySelf();
        CHECK(map->IsDestroyed());
        CHECK_FALSE(client->GetEntity(map->GetId()));
        CHECK(rt_mngr.GetRenderTargetCount() == initial_targets);
    }

    size_t initial_memory {memory::get_in_use_bytes()};
    vector<refcount_ptr<MapView>> retired_maps {};

    for (uint32_t cycle {0}; cycle < 12; cycle++) {
        auto map {safe_alloc::make_refcounted<MapView>(client.as_ptr(), ident_t {9001 + cycle}, proto.as_ptr(), isize32 {320, 200})};
        auto destroy_map {scope_exit([&map]() noexcept {
            safe_call([&map] {
                if (!map->IsDestroyed()) {
                    map->DestroySelf();
                }
            });
        })};

        CHECK(rt_mngr.GetRenderTargetCount() == initial_targets + 3);
        map->DestroySelf();
        CHECK(map->IsDestroyed());
        CHECK_FALSE(client->GetEntity(map->GetId()));
        CHECK(rt_mngr.GetRenderTargetCount() == initial_targets);
        retired_maps.emplace_back(map);
    }

    // Managed wrappers can retain every retired map; native storage must be released before their finalizers run
    if (initial_memory != 0) {
        size_t retained_memory {memory::get_in_use_bytes()};
        INFO("Retained map memory: " << retained_memory << "; initial: " << initial_memory);
        CHECK(retained_memory < initial_memory + 8 * 1024 * 1024);
    }
}

TEST_CASE("ClientLargeMapFieldsStayWithinMemoryBudget")
{
    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto proto {safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("LargeSparseMap"), client->GetPropertyRegistrar("Map"))};
    proto->SetSize(msize {1200, 1200});
    proto->SetWorkHex(mpos {200, 200});

    size_t initial_memory {platform::get_process_private_memory_usage()};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {9001}, proto, isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept {
        safe_call([&map] {
            if (!map->IsDestroyed()) {
                map->DestroySelf();
            }
        });
    })};

    size_t map_memory {platform::get_process_private_memory_usage()};

    // Measure the real constructor, including view and light buffers, without optional allocator diagnostics
    if (initial_memory != 0 && map_memory != 0) {
        INFO("Large map private memory: " << map_memory << "; initial: " << initial_memory);
        CHECK(map_memory < initial_memory + 64 * 1024 * 1024);
    }

    map->SetScrollCheck(false);
    map->InstantScrollTo(mpos {300, 300});
    map->InstantScrollTo(mpos {200, 200});
    REQUIRE(map->GetField(mpos {200, 200}).IsView);
    ptr<const MapView::Field> first_field {&map->GetField(mpos {200, 200})};

    for (mpos center : {mpos {500, 500}, mpos {900, 900}, mpos {1100, 1100}, mpos {900, 1100}, mpos {1100, 900}, mpos {900, 700}}) {
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
    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto proto {safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("LargeScrollBoundedMap"), client->GetPropertyRegistrar("Map"))};
    proto->SetSize(msize {1200, 1200});
    proto->SetWorkHex(mpos {301, 183});
    proto->SetScrollAxialArea(irect32 {-380, 230, 356, 243});

    size_t initial_memory {platform::get_process_private_memory_usage()};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {9001}, proto, isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
    size_t map_memory {platform::get_process_private_memory_usage()};

    if (initial_memory == 0 || map_memory == 0) {
        SKIP("Process-private-memory measurement is unavailable on this platform");
    }

    // Scroll-block lines must not materialize the rest of a large, empty map
    INFO("Scroll-bounded map private memory: " << map_memory << "; initial: " << initial_memory);
    CHECK(map_memory < initial_memory + 64 * 1024 * 1024);
}

TEST_CASE("ClientMaximumSizeMapLoadsWithoutMapSizedWork")
{
    // A map at the configured maximum (up to a 128,000-cell continent with wide coordinates) must construct, scroll,
    // light and unload with storage and time that follow the touched area rather than the map size
    auto settings {MakeClientLifetimeSettings()};
    BakerTests::OverrideSetting(settings.View.DisableLighting, false);
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto proto {safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("MaximumSizeMap"), client->GetPropertyRegistrar("Map"))};
    auto side {numeric_cast<mcoord_t>(std::min<int32_t>(GameSettings::MAX_MAP_SIZE, 128000))};
    auto far_coord {numeric_cast<mcoord_t>(side - 200)};
    proto->SetSize(msize {side, side});
    proto->SetWorkHex(mpos {far_coord, far_coord});

    size_t initial_memory {platform::get_process_private_memory_usage()};
    auto start_time {nanotime::now()};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {9001}, proto, isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept {
        safe_call([&map] {
            if (!map->IsDestroyed()) {
                map->DestroySelf();
            }
        });
    })};
    map->SetScrollCheck(false);
    map->InstantScrollTo(mpos {200, 200});
    map->InstantScrollTo(mpos {far_coord, far_coord});
    auto elapsed {(nanotime::now() - start_time).to_ms<int64_t>()};
    size_t map_memory {platform::get_process_private_memory_usage()};

    INFO("Map side " << side << ", construction and two far scrolls took " << elapsed << " ms");
    CHECK(elapsed < 5000);
    CHECK(map->GetField(mpos {far_coord, far_coord}).IsView);
    CHECK_FALSE(map->GetField(mpos {200, 200}).IsView);
    CHECK(map->GetField(mpos {numeric_cast<mcoord_t>(side - 1), numeric_cast<mcoord_t>(side - 1)}).Items.empty());

    if (initial_memory != 0 && map_memory != 0) {
        INFO("Maximum-size map private memory: " << map_memory << "; initial: " << initial_memory);
        CHECK(map_memory < initial_memory + 64 * 1024 * 1024);
    }

    map->DestroySelf();
    CHECK_FALSE(client->GetEntity(map->GetId()));
}

TEST_CASE("ClientMapLoadsAndUnloadsStaticItemsOfOtherMaps")
{
    // Prototypes and the baked map come from a baker engine over the same empty metadata the client uses
    vector<uint8_t> metadata {BakerTests::MakeEmptyMetadataBlob()};
    auto compiler_source {safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientContentProtos")};
    compiler_source->AddFile("Metadata.fometa-client", metadata);
    FileSystem compiler_resources {};
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerClientEngine proto_engine {compiler_resources};

    hstring item_type {proto_engine.Hashes.to_hashed_string("Item")};
    hstring map_type {proto_engine.Hashes.to_hashed_string("Map")};
    hstring blocker_pid {proto_engine.Hashes.to_hashed_string("ClientContentBlocker")};

    vector<pair<string, function<void(ProtoItem&)>>> item_protos;
    item_protos.emplace_back("ClientContentBlocker", [](ProtoItem& proto) {
        proto.SetStatic(true);
        proto.SetNoBlock(false);
    });
    vector<pair<string, function<void(ProtoMap&)>>> map_protos;
    map_protos.emplace_back("ClientContentMap", [](ProtoMap& proto) { proto.SetSize(msize {32, 32}); });

    auto item_registrar {proto_engine.GetPropertyRegistrar(item_type)};
    REQUIRE(static_cast<bool>(item_registrar));
    vector<uint8_t> content_map {};
    {
        auto writer {data_writer(content_map)};
        writer.write<uint32_t>(BAKED_MAP_FILE_MAGIC);
        writer.write<uint32_t>(BAKED_MAP_FILE_VERSION);
        writer.write<uint32_t>(uint32_t {1});
        writer.write<uint32_t>(numeric_cast<uint32_t>(blocker_pid.as_str().length()));
        writer.write_string_bytes(blocker_pid.as_str());
        writer.write<uint32_t>(uint32_t {2});

        for (auto&& [baked_id, hex] : {pair {int64_t {31}, mpos {2, 3}}, pair {int64_t {32}, mpos {20, 20}}}) {
            Properties props {item_registrar};
            props.SetValue<bool>(item_registrar->FindProperty("Static"), true);
            props.SetValue<ItemOwnership>(item_registrar->FindProperty("Ownership"), ItemOwnership::MapHex);
            props.SetValue<mpos>(item_registrar->FindProperty("Hex"), hex);
            vector<uint8_t> props_data {};
            set<hstring> str_hashes {};
            props.StoreAllData(props_data, str_hashes);
            writer.write<ident_t::underlying_type>(baked_id);
            writer.write<hstring::hash_t>(blocker_pid.as_hash());
            writer.write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
            writer.write_bytes({props_data.data(), props_data.size()});
        }
    }

    vector<pair<string, vector<uint8_t>>> extra {};
    extra.emplace_back("ClientContentItems.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(proto_engine, item_type, item_protos));
    extra.emplace_back("ClientContentMap.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoMap>(proto_engine, map_type, map_protos));
    extra.emplace_back("ClientContentMap.fomap-bin-client", std::move(content_map));

    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings, std::move(extra))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto proto {safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("ClientContentHost"), client->GetPropertyRegistrar("Map"))};
    proto->SetSize(msize {600, 600});
    proto->SetWorkHex(mpos {50, 50});
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {9001}, proto, isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept {
        safe_call([&map] {
            if (!map->IsDestroyed()) {
                map->DestroySelf();
            }
        });
    })};

    // The same id offsets as on the server side, derived from the placement
    auto id_offset {[](mpos dest_hex) -> int64_t { return (int64_t {1} << 50) + int64_t {dest_hex.x} * 1000000 + int64_t {dest_hex.y} * 1000; }};

    hstring content_pid {client->Hashes.to_hashed_string("ClientContentMap")};
    const mpos dest_hex {320, 256};
    ident_t blocker_id {ident_t {(int64_t {1} << 50) + 320256031}};
    ident_t far_id {ident_t {(int64_t {1} << 50) + 320256032}};

    vector<ident_t> ids {map->LoadRuntimeStaticItems(content_pid, dest_hex, id_offset(dest_hex))};
    REQUIRE(ids.size() == 2);
    REQUIRE(map->GetItem(blocker_id));
    CHECK(map->GetItem(blocker_id)->GetHex() == mpos {322, 259});
    CHECK(map->GetItem(far_id)->GetHex() == mpos {340, 276});
    CHECK(map->GetItem(blocker_id)->GetStatic());
    CHECK(map->IsRuntimeStaticItem(blocker_id));
    CHECK(map->GetField(mpos {322, 259}).MoveBlocked);
    CHECK_FALSE(map->GetField(mpos {2, 3}).MoveBlocked);

    // Ids must be nonzero and unused, and content must fit the map
    CHECK_THROWS(map->LoadRuntimeStaticItems(content_pid, dest_hex, id_offset(dest_hex)));
    CHECK_THROWS(map->LoadRuntimeStaticItems(content_pid, mpos {100, 100}, id_offset(dest_hex)));
    CHECK_THROWS(map->LoadRuntimeStaticItems(content_pid, mpos {100, 100}, -31));
    CHECK_THROWS(map->LoadRuntimeStaticItems(content_pid, mpos {100, 100}, std::numeric_limits<int64_t>::max()));
    CHECK_THROWS(map->LoadRuntimeStaticItems(content_pid, mpos {590, 590}, id_offset(mpos {590, 590})));
    CHECK_THROWS(map->LoadRuntimeStaticItems(client->Hashes.to_hashed_string("ClientMissingMap"), dest_hex, id_offset(dest_hex)));
    CHECK(map->GetItems().size() == 2);

    // Unloading destroys the views and releases the empty field blocks they covered
    map->UnloadRuntimeStaticItems(ids);
    CHECK_FALSE(map->GetItem(blocker_id));
    CHECK_FALSE(map->IsRuntimeStaticItem(blocker_id));
    CHECK_FALSE(map->GetField(mpos {322, 259}).MoveBlocked);
    CHECK(map->GetField(mpos {322, 259}).Items.empty());
    CHECK_THROWS(map->UnloadRuntimeStaticItems(ids));

    // Loading again gives the same ids
    CHECK(map->LoadRuntimeStaticItems(content_pid, dest_hex, id_offset(dest_hex)) == ids);
    CHECK(map->GetItem(blocker_id));

    // An area of the source map lands with its first hex at dest_hex and must lie inside the source map
    vector<ident_t> area_ids {map->LoadRuntimeStaticItems(content_pid, mpos {100, 100}, mpos {20, 20}, msize {12, 12}, id_offset(mpos {100, 100}))};
    REQUIRE(area_ids.size() == 1);
    REQUIRE(map->GetItem(area_ids.front()));
    CHECK(area_ids.front() == ident_t {(int64_t {1} << 50) + 100100032});
    CHECK(map->GetItem(area_ids.front())->GetHex() == mpos {100, 100});
    CHECK_THROWS(map->LoadRuntimeStaticItems(content_pid, mpos {200, 200}, mpos {20, 20}, msize {13, 1}, id_offset(mpos {200, 200})));
    CHECK_THROWS(map->LoadRuntimeStaticItems(content_pid, mpos {200, 200}, mpos {32, 0}, msize {1, 1}, id_offset(mpos {200, 200})));
    map->UnloadRuntimeStaticItems(area_ids);
    CHECK_FALSE(map->GetItem(area_ids.front()));

    map->DestroySelf();
}

TEST_CASE("ClientMapConstructionFailureReleasesRenderTargets")
{
    auto settings {MakeClientLifetimeSettings()};
    BakerTests::OverrideSetting(settings.View.MapDirectDraw, false);
    BakerTests::OverrideSetting(settings.View.DisableIndoorMask, false);
    BakerTests::OverrideSetting(settings.View.DisableLighting, false);
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto proto {safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("InvalidLifetimeMap"), client->GetPropertyRegistrar("Map").as_ptr())};
    proto->SetSize(msize {0, 0});
    size_t initial_targets {client->SprMngr.GetRtMngr().GetRenderTargetCount()};

    CHECK_THROWS_AS(safe_alloc::make_refcounted<MapView>(client.as_ptr(), ident_t {9001}, proto.as_ptr(), isize32 {320, 200}), VerificationException);
    CHECK_FALSE(client->GetEntity(ident_t {9001}));
    CHECK(client->SprMngr.GetRtMngr().GetRenderTargetCount() == initial_targets);
}

TEST_CASE("ClientMapUnloadDropsPendingItemOwners")
{
    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto map_proto {safe_alloc::make_refcounted<ProtoMap>(client->Hashes.to_hashed_string("PendingLifetimeMap"), client->GetPropertyRegistrar("Map").as_ptr())};
    map_proto->SetSize(msize {32, 32});
    auto item_proto {safe_alloc::make_refcounted<ProtoItem>(client->Hashes.to_hashed_string("PendingLifetimeItem"), client->GetPropertyRegistrar("Item").as_ptr())};
    auto map {safe_alloc::make_refcounted<MapView>(client.as_ptr(), ident_t {9001}, map_proto.as_ptr(), isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept {
        safe_call([&map] {
            if (!map->IsDestroyed()) {
                map->DestroySelf();
            }
        });
    })};

    auto item {safe_alloc::make_refcounted<ItemHexView>(map.as_ptr(), ident_t {}, item_proto.as_ptr())};
    map->RefreshItem(item, true);
    REQUIRE(item->GetRefCount() == 2);
    item->DestroySelf();
    CHECK(item->GetRefCount() == 2);
    map->DestroySelf();
    CHECK(item->GetRefCount() == 1);
}

TEST_CASE("ExpiredOneImageAtlasReleasesRenderTarget")
{
    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto atlas_mngr {client->SprMngr.GetAtlasMngr()};
    auto& rt_mngr {client->SprMngr.GetRtMngr()};
    size_t initial_targets {rt_mngr.GetRenderTargetCount()};

    for (uint32_t cycle {0}; cycle < 12; cycle++) {
        auto [atlas, allocation, pos] {atlas_mngr->FindAtlasPlace(AtlasType::OneImage, isize32 {16, 16})};
        ignore_unused(atlas, pos);
        CHECK(rt_mngr.GetRenderTargetCount() == initial_targets + 1);
    }
}

TEST_CASE("AtlasCleanupReleasesOnlyEmptyPages")
{
    auto settings {MakeClientLifetimeSettings()};
    RenderTargetManager rt_mngr {GetApp()->MainWindow.GetRender(), [] { }};
    TextureAtlasManager atlas_mngr {&settings, &rt_mngr};
    size_t initial_targets {rt_mngr.GetRenderTargetCount()};
    auto [atlas1, allocation1, pos1] {atlas_mngr.FindAtlasPlace(AtlasType::MapSprites, isize32 {16, 16})};
    auto [atlas2, allocation2, pos2] {atlas_mngr.FindAtlasPlace(AtlasType::MapSprites, isize32 {16, 16})};
    ignore_unused(pos1, pos2);
    unique_del_nptr<TextureAtlasLayout::Allocation> owner1 {static_cast<unique_del_nptr<TextureAtlasLayout::Allocation>>(std::move(allocation1))};
    unique_del_nptr<TextureAtlasLayout::Allocation> owner2 {static_cast<unique_del_nptr<TextureAtlasLayout::Allocation>>(std::move(allocation2))};
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
    vector<mpos> positions {};

    for (int16_t y {160}; y < 224; y++) {
        for (int16_t x {256}; x < 352; x++) {
            positions.emplace_back(x, y);
        }
    }

    auto compare {[&positions](size_t populated_stride, bool scattered, bool writing) {
        if (scattered) {
            for (size_t index {0}; index < positions.size(); index++) {
                positions[index] = mpos {numeric_cast<int16_t>(index * 137 % 1200), numeric_cast<int16_t>(index * 71 % 1200)};
            }
        }

        auto measure {[&](auto& grid, string_view name) {
            if (populated_stride != 0) {
                for (size_t index {0}; index < positions.size(); index += populated_stride) {
                    grid.GetCellForWriting(positions[index])->MoveBlocked = true;
                }
            }

            array<double, 7> samples {};
            uint64_t checksum {0};

            for (double& sample : samples) {
                auto started {std::chrono::steady_clock::now()};

                for (size_t iteration {0}; iteration < 128; iteration++) {
                    for (mpos pos : positions) {
                        if (writing) {
                            ptr<MapView::Field> field {grid.GetCellForWriting(pos)};
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
        }};

        uint64_t dense_checksum {0};
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
    }};

    compare(1, false, false);
    compare(8, false, false);
    compare(0, false, false);
    compare(1, false, true);
    compare(8, true, false);
}

TEST_CASE("ClientCritterFinalizationDropsAttachmentIds", "[client][entity][attachments]")
{
    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    ProtoCritter proto {client->Hashes.to_hashed_string("LifetimeCritter"), client->GetPropertyRegistrar(CritterView::ENTITY_TYPE_NAME)};
    refcount_nptr<CritterView> master {safe_alloc::make_refcounted<CritterView>(client, ident_t {1001}, &proto)};
    auto peer {safe_alloc::make_refcounted<CritterView>(client, ident_t {1002}, &proto)};
    master->SetAttachedCritters(vector<ident_t> {peer->GetId()});

    std::atomic<size_t> native_reports {};
    auto previous_exception_callback {exceptions::get_callback()};
    exceptions::set_callback([&native_reports, previous_exception_callback](string_view message, const stack_trace::catched_data& trace, bool fatal) {
        native_reports.fetch_add(1, std::memory_order_relaxed);

        if (previous_exception_callback) {
            previous_exception_callback(message, trace, fatal);
        }
    });

    auto restore_exception_callback {scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { exceptions::set_callback(std::move(previous)); })};

    master->DestroySelf();
    CHECK(master->IsDestroyed());
    CHECK_FALSE(master->HasAttachedCritters());
    CHECK_FALSE(peer->IsDestroyed());
    master.reset();
    CHECK(native_reports.load(std::memory_order_relaxed) == 0);
    peer->DestroySelf();
}

TEST_CASE("ClientItemPrototypeFootprintRecachesOnBlockingChange")
{
    BakerTests::TestRig rig {};
    vector<uint8_t> metadata {BakerTests::MakeEmptyMetadataBlob()};
    rig.AddBakedFile("Metadata.fometa-server", metadata);
    rig.AddBakedFile("Metadata.fometa-client", metadata);

    BakerServerEngine server_engine {rig.BakedFiles};
    BakerClientEngine client_engine {rig.BakedFiles};

    auto server_registrar {server_engine.GetPropertyRegistrar(server_engine.Hashes.to_hashed_string("Item"))};
    REQUIRE(static_cast<bool>(server_registrar));
    ProtoItem server_proto {server_engine.Hashes.to_hashed_string("FootprintLifetimeItem"), server_registrar};
    server_proto.SetPicMap(server_engine.Hashes.to_hashed_string("LifetimeSprite.png"));
    server_proto.SetShootThru(true);
    server_proto.SetLightThru(true);
    server_proto.SetMultihexLines(vector<uint8_t> {5, 1});
    server_proto.SetDrawMultihexLines(false);

    // Instances inherit the prototype contour, so no local unsynced geometry overrides are authored
    Properties server_item {server_registrar, server_proto.GetProperties()};
    Properties server_control {server_registrar, server_proto.GetProperties()};

    auto send_snapshot {[](const Properties& source) {
        auto stored_data {source.StoreData(false)};

        NetOutBuffer out {8};
        out.WritePropsData(*stored_data.Data, *stored_data.Sizes);

        NetInBuffer in {8};
        in.AddData(out.GetData());

        vector<vector<uint8_t>> chunks {};
        in.ReadPropsData(chunks);
        return chunks;
    }};

    auto footprint_chunks {send_snapshot(server_item)};
    auto control_chunks {send_snapshot(server_control)};

    vector<pair<string, function<void(ProtoItem&)>>> client_protos {};
    client_protos.emplace_back(string {"FootprintLifetimeItem"}, [&client_engine](ProtoItem& proto) {
        proto.SetPicMap(client_engine.Hashes.to_hashed_string("LifetimeSprite.png"));
        proto.SetShootThru(true);
        proto.SetLightThru(true);
        proto.SetMultihexLines(vector<uint8_t> {5, 1});
        proto.SetDrawMultihexLines(false);
    });

    auto proto_blob {BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(client_engine, client_engine.Hashes.to_hashed_string("Item"), client_protos)};
    vector<pair<string, vector<uint8_t>>> extra_resources {};
    extra_resources.emplace_back("FootprintItems.fopro-bin-client", std::move(proto_blob));

    auto settings {MakeClientLifetimeSettings()};
    auto client {MakeClientLifetimeEngine(settings, std::move(extra_resources))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    ProtoMap map_proto {client->Hashes.to_hashed_string("FootprintLifetimeMap"), client->GetPropertyRegistrar("Map")};
    map_proto.SetSize(msize {32, 32});
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {9001}, make_ptr(&map_proto), isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept {
        safe_call([&map] {
            if (!map->IsDestroyed()) {
                map->DestroySelf();
            }
        });
    })};

    auto item_a {map->AddReceivedItem(ident_t {5001}, client->Hashes.to_hashed_string("FootprintLifetimeItem"), mpos {16, 16}, footprint_chunks, false)};
    auto item_b {map->AddReceivedItem(ident_t {5002}, client->Hashes.to_hashed_string("FootprintLifetimeItem"), mpos {24, 24}, control_chunks, false)};

    // Direction 5 traces one NorthWest step from each anchor: A {(16,16),(16,15)}, B {(24,24),(24,23)}
    array<mpos, 2> footprint_a {mpos {16, 16}, mpos {16, 15}};
    array<mpos, 2> footprint_b {mpos {24, 24}, mpos {24, 23}};
    vector<uint8_t> expected_lines {5, 1};

    CHECK(item_a->GetMultihexLines() == expected_lines);
    CHECK(item_b->GetMultihexLines() == expected_lines);
    CHECK(item_a->GetMultihexMesh().empty());
    CHECK(item_b->GetMultihexMesh().empty());

    // Closed: every contour cell blocks movement while shoot and light pass through the transparent item
    for (auto hex : footprint_a) {
        CHECK(map->GetField(hex).MoveBlocked);
        CHECK_FALSE(map->GetField(hex).ShootBlocked);
        CHECK_FALSE(map->GetField(hex).LightBlocked);
    }

    for (auto hex : footprint_b) {
        CHECK(map->GetField(hex).MoveBlocked);
        CHECK_FALSE(map->GetField(hex).ShootBlocked);
        CHECK_FALSE(map->GetField(hex).LightBlocked);
    }

    CHECK_FALSE(map->GetField(mpos {16, 17}).MoveBlocked);
    CHECK_FALSE(map->GetField(mpos {24, 25}).MoveBlocked);

    const auto& extra_entries_a {map->GetField(mpos {16, 15}).MultihexItems};
    REQUIRE(extra_entries_a.size() == 1);
    CHECK(extra_entries_a.front().first == item_a);
    CHECK_FALSE(extra_entries_a.front().second);

    const auto& extra_entries_b {map->GetField(mpos {24, 23}).MultihexItems};
    REQUIRE(extra_entries_b.size() == 1);
    CHECK(extra_entries_b.front().first == item_b);
    CHECK_FALSE(extra_entries_b.front().second);

    // Opening runs the real NoBlock post-setter, which must refresh the whole contour and not just the anchor
    item_a->SetNoBlock(true);

    for (auto hex : footprint_a) {
        CHECK_FALSE(map->GetField(hex).MoveBlocked);
        CHECK_FALSE(map->GetField(hex).ShootBlocked);
        CHECK_FALSE(map->GetField(hex).LightBlocked);
    }

    for (auto hex : footprint_b) {
        CHECK(map->GetField(hex).MoveBlocked);
    }

    // Closing restores both A cells while the independent B control stays blocked
    item_a->SetNoBlock(false);

    for (auto hex : footprint_a) {
        CHECK(map->GetField(hex).MoveBlocked);
        CHECK_FALSE(map->GetField(hex).ShootBlocked);
        CHECK_FALSE(map->GetField(hex).LightBlocked);
    }

    for (auto hex : footprint_b) {
        CHECK(map->GetField(hex).MoveBlocked);
    }

    map->DestroySelf();
    CHECK(map->IsDestroyed());
    CHECK_FALSE(client->GetEntity(map->GetId()));
}

FO_END_NAMESPACE
