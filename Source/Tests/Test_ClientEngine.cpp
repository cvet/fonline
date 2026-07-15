//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#include "catch_amalgamated.hpp"

#include "AngelScriptScripting.h"
#include "Application.h"
#include "Baker.h"
#include "Client.h"
#include "CritterView.h"
#include "DataSerialization.h"
#include "DefaultSprites.h"
#include "PlayerView.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    static auto MakeClientTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);

        return settings;
    }

    static auto MakeClientScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerClientEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ClientEngineScripts",
            {
                {"Scripts/ClientEngineTest.fos", R"(
namespace ClientEngineTest
{
    int StartCalls = 0;
    int LoopCalls = 0;
    int ManualCalls = 0;

    [[ModuleInit]]
    void InitClientEngineTest()
    {
        Game.OnStart.Subscribe(OnStart);
        Game.OnLoop.Subscribe(OnLoop);
    }

    [[Event]]
    void OnStart()
    {
        StartCalls++;
    }

    [[Event]]
    void OnLoop()
    {
        LoopCalls++;
    }

    void UnitTestNoop() {}

    void UnitTestMarkManualCall()
    {
        ManualCalls++;
    }

    int UnitTestGetStartCalls()
    {
        return StartCalls;
    }

    int UnitTestGetLoopCalls()
    {
        return LoopCalls;
    }

    int UnitTestGetManualCalls()
    {
        return ManualCalls;
    }

    int UnitTestMapSpriteHolderRefType()
    {
        MapSpriteHolder holder = MapSpriteHolder();
        if (holder is null) return -1;
        if (holder.Valid) return -2;

        holder.SprId = 42;
        if (holder.SprId != 42) return -3;

        holder.NoLight = true;
        if (!holder.NoLight) return -4;

        holder.Angle = 90;
        if (holder.Angle != 90) return -5;

        holder.TweakAlpha = 123;
        if (holder.TweakAlpha != 123) return -6;

        holder.MapProjected = true;
        if (!holder.MapProjected) return -7;

        MapSpriteHolder same = holder;
        if (!(holder == same)) return -8;

        holder.StopDraw();
        return 0;
    }
}
)"},
            },
            [](string_view message) {
                const auto message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }

    static auto MakeClientTestResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientEngineCompilerResources");
        compiler_source->AddFile("Metadata.fometa-client", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerClientEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestClientCritter");
        const auto script_blob = MakeClientScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientEngineRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-client", metadata_blob);
        runtime_source->AddFile("ClientEngineTest.fopro-bin-client", proto_blob);
        runtime_source->AddFile("ClientEngineTest.fos-bin-client", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    static auto MakeClientEngine(GlobalSettings& settings) -> refcount_ptr<ClientEngine>
    {
        return SafeAlloc::MakeRefCounted<ClientEngine>(&settings, MakeClientTestResources(), &GetApp()->MainWindow);
    }
}

TEST_CASE("ClientEngineStartsAndRegistersEntities")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));
    CHECK_FALSE(static_cast<bool>(client->GetCurLocation()));
    CHECK_FALSE(static_cast<bool>(client->GetCurMap()));

    const auto critter_pid = client->Hashes.ToHashedString("UnitTestClientCritter");
    auto critter_proto = client->GetProtoCritter(critter_pid);
    REQUIRE(static_cast<bool>(critter_proto));

    auto player = SafeAlloc::MakeRefCounted<PlayerView>(client, ident_t {1001});
    auto critter = SafeAlloc::MakeRefCounted<CritterView>(client, ident_t {1002}, critter_proto);

    REQUIRE(client->GetEntity(player->GetId()) == player);
    REQUIRE(client->GetEntity(critter->GetId()) == critter);
    CHECK(critter->GetProtoId() == critter_pid);
    CHECK(critter->GetName() == "UnitTestClientCritter_1002");

    critter->DestroySelf();
    player->DestroySelf();

    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {1002})));
    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {1001})));
}

TEST_CASE("ClientEngineScriptModuleInitAndLoopAreCallable")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    const auto get_func_name = [&client](string_view name) { return client->Hashes.ToHashedString(name); };

    int start_calls = 0;
    int loop_calls = 0;
    int manual_calls = 0;

    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetStartCalls"), start_calls));
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetLoopCalls"), loop_calls));
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetManualCalls"), manual_calls));

    CHECK(start_calls == 1);
    CHECK(loop_calls == 0);
    CHECK(manual_calls == 0);

    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestMarkManualCall")));
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetManualCalls"), manual_calls));
    CHECK(manual_calls == 1);

    client->MainLoop();
    client->MainLoop();

    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetLoopCalls"), loop_calls));
    CHECK(loop_calls >= 2);
}

TEST_CASE("ClientEngineScheduledCallbacksDoNotRunNestedZeroDelayInSamePass")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    int32_t callback_count = 0;

    client->ScheduleDelayedCallback(timespan::zero, [&client, &callback_count] {
        callback_count++;

        client->ScheduleDelayedCallback(timespan::zero, [&callback_count] { callback_count++; });
    });

    client->ProcessScheduledCallbacks();
    CHECK(callback_count == 1);

    client->ProcessScheduledCallbacks();
    CHECK(callback_count == 2);
}

TEST_CASE("ClientEngineMethodRefTypeOps")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    const auto get_func_name = [&client](string_view name) { return client->Hashes.ToHashedString(name); };

    int32_t result = 0;
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestMapSpriteHolderRefType"), result));
    CHECK(result == 0);
}

TEST_CASE("AtlasSpriteFillDataSupportsBakedMeshes")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    const frect32 atlas_rect = {0.25f, 0.5f, 0.5f, 0.25f};
    const frect32 draw_rect = {100.0f, 200.0f, 20.0f, 40.0f};
    const ucolor color_left = {10, 20, 30, 40};
    const ucolor color_right = {110, 120, 130, 140};

    SECTION("Absent mesh keeps the legacy quad")
    {
        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

        const size_t index_count = sprite->FillData(draw_buf, draw_rect, {color_left, color_right});

        REQUIRE(index_count == 6);
        REQUIRE(draw_buf->VertCount == 4);
        REQUIRE(draw_buf->IndCount == 6);
        CHECK(draw_buf->Indices[0] == 0);
        CHECK(draw_buf->Indices[1] == 1);
        CHECK(draw_buf->Indices[2] == 3);
        CHECK(draw_buf->Indices[3] == 1);
        CHECK(draw_buf->Indices[4] == 2);
        CHECK(draw_buf->Indices[5] == 3);
        CHECK(draw_buf->Vertices[0].Color == color_left);
        CHECK(draw_buf->Vertices[1].Color == color_left);
        CHECK(draw_buf->Vertices[2].Color == color_right);
        CHECK(draw_buf->Vertices[3].Color == color_right);
    }

    SECTION("Explicit empty mesh emits no draw data")
    {
        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, SpriteMeshData {});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

        const size_t index_count = sprite->FillData(draw_buf, draw_rect, {color_left, color_right});

        CHECK(index_count == 0);
        CHECK(draw_buf->VertCount == 0);
        CHECK(draw_buf->IndCount == 0);
    }

    SECTION("Mesh maps positions UVs indices and horizontal light colors")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 5}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
        draw_buf->Vertices.resize(2);
        draw_buf->VertCount = 2;
        draw_buf->Indices.resize(1);
        draw_buf->Indices[0] = 0;
        draw_buf->IndCount = 1;

        const size_t index_count = sprite->FillData(draw_buf, draw_rect, {color_left, color_right});

        REQUIRE(index_count == 3);
        REQUIRE(draw_buf->VertCount == 5);
        REQUIRE(draw_buf->IndCount == 4);
        CHECK(draw_buf->Indices[1] == 2);
        CHECK(draw_buf->Indices[2] == 3);
        CHECK(draw_buf->Indices[3] == 4);

        const Vertex2D& left = draw_buf->Vertices[2];
        const Vertex2D& center = draw_buf->Vertices[3];
        const Vertex2D& right = draw_buf->Vertices[4];
        CHECK(left.PosX == Catch::Approx(100.0f));
        CHECK(left.PosY == Catch::Approx(200.0f));
        CHECK(left.TexU == Catch::Approx(0.25f));
        CHECK(left.TexV == Catch::Approx(0.5f));
        CHECK(left.Color == color_left);
        CHECK(center.PosX == Catch::Approx(110.0f));
        CHECK(center.PosY == Catch::Approx(220.0f));
        CHECK(center.TexU == Catch::Approx(0.5f));
        CHECK(center.TexV == Catch::Approx(0.625f));
        CHECK(center.Color == (ucolor {60, 70, 80, 90}));
        CHECK(right.PosX == Catch::Approx(120.0f));
        CHECK(right.PosY == Catch::Approx(200.0f));
        CHECK(right.TexU == Catch::Approx(0.75f));
        CHECK(right.TexV == Catch::Approx(0.5f));
        CHECK(right.Color == color_right);
    }

    SECTION("Cropped mesh preserves source-relative horizontal light colors")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 0};
        mesh.Vertices = {{0, 0}, {3, 10}, {6, 0}};
        mesh.Indices = {0, 1, 2};

        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {6, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

        REQUIRE(sprite->FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        REQUIRE(draw_buf->VertCount == 3);
        CHECK(draw_buf->Vertices[0].Color == (ucolor {30, 40, 50, 60}));
        CHECK(draw_buf->Vertices[1].Color == (ucolor {60, 70, 80, 90}));
        CHECK(draw_buf->Vertices[2].Color == (ucolor {90, 100, 110, 120}));
    }

    SECTION("Live atlas allocation observes sprite mesh metadata for dump lifetime")
    {
        TextureAtlasLayout layout {{12, 12}};
        auto atlas_allocation = layout.Allocate({12, 12});
        REQUIRE(atlas_allocation);
        const nptr<TextureAtlasLayout::Allocation> allocation_observer = atlas_allocation.as_nptr();
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 5}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        {
            auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, std::move(atlas_allocation), atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
            auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

            REQUIRE(allocation_observer->GetSpriteMesh());
            CHECK(allocation_observer->GetSpriteMesh()->Vertices.size() == 3);
            CHECK(sprite->FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        }

        CHECK_FALSE(allocation_observer->IsActive());
        CHECK(allocation_observer->GetSpriteMesh() == nullptr);
    }

    SECTION("Moving an atlas sprite rebinds the allocation mesh observer")
    {
        TextureAtlasLayout layout {{12, 12}};
        auto atlas_allocation = layout.Allocate({12, 12});
        REQUIRE(atlas_allocation);
        const nptr<TextureAtlasLayout::Allocation> allocation_observer = atlas_allocation.as_nptr();
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 5}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        {
            AtlasSprite source {&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, std::move(atlas_allocation), atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)}};
            const nptr<const SpriteMeshData> source_mesh = allocation_observer->GetSpriteMesh();
            AtlasSprite moved {std::move(source)};

            REQUIRE(allocation_observer->GetSpriteMesh());
            CHECK(allocation_observer->GetSpriteMesh() != source_mesh);
            auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
            CHECK(moved.FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        }

        CHECK_FALSE(allocation_observer->IsActive());
        CHECK(allocation_observer->GetSpriteMesh() == nullptr);
    }
}

TEST_CASE("DefaultSpriteFactoryValidatesBakedMeshPayload")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    SpriteMeshData mesh;
    mesh.SourceSize = {2, 2};
    mesh.Vertices = {{0, 0}, {2, 0}, {0, 2}};
    mesh.Indices = {0, 1, 2};

    const vector<uint8_t> valid_blob = BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Mesh, mesh);
    constexpr size_t mesh_kind_offset = 20 + 2 * 2 * sizeof(ucolor);
    constexpr size_t mesh_vertex_count_offset = mesh_kind_offset + 1;
    constexpr size_t mesh_index_count_offset = mesh_vertex_count_offset + sizeof(uint16_t);
    constexpr size_t mesh_source_size_offset = mesh_index_count_offset + sizeof(uint32_t);
    constexpr size_t mesh_source_offset_offset = mesh_source_size_offset + sizeof(uint16_t) * 2;
    constexpr size_t mesh_vertices_offset = mesh_source_offset_offset + sizeof(int32_t) * 2;
    constexpr size_t mesh_indices_offset = mesh_vertices_offset + 3 * sizeof(uint16_t) * 2;

    const auto write_u16 = [](vector<uint8_t>& data, size_t offset, uint16_t value) {
        data[offset] = numeric_cast<uint8_t>(value & 0xFF);
        data[offset + 1] = numeric_cast<uint8_t>(value >> 8);
    };
    const auto write_u32 = [](vector<uint8_t>& data, size_t offset, uint32_t value) {
        data[offset] = numeric_cast<uint8_t>(value & 0xFF);
        data[offset + 1] = numeric_cast<uint8_t>((value >> 8) & 0xFF);
        data[offset + 2] = numeric_cast<uint8_t>((value >> 16) & 0xFF);
        data[offset + 3] = numeric_cast<uint8_t>(value >> 24);
    };

    auto source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("PolygonSpriteResources");
    source->AddFile("Quad.png", BakerTests::MakeMinimalBakedSprite(2, 2));
    source->AddFile("Empty.png", BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Empty));
    source->AddFile("ValidMesh.png", valid_blob);

    vector<uint8_t> bad_version = valid_blob;
    bad_version[1]++;
    source->AddFile("BadVersion.png", std::move(bad_version));

    vector<uint8_t> bad_kind = valid_blob;
    bad_kind[mesh_kind_offset] = 0xFF;
    source->AddFile("BadKind.png", std::move(bad_kind));

    vector<uint8_t> bad_vertex_count = valid_blob;
    write_u16(bad_vertex_count, mesh_vertex_count_offset, uint16_t {2});
    source->AddFile("BadVertexCount.png", std::move(bad_vertex_count));

    vector<uint8_t> bad_index_count = valid_blob;
    write_u32(bad_index_count, mesh_index_count_offset, uint32_t {4});
    source->AddFile("BadIndexCount.png", std::move(bad_index_count));

    vector<uint8_t> implausible_index_count = valid_blob;
    write_u32(implausible_index_count, mesh_index_count_offset, uint32_t {21});
    source->AddFile("ImplausibleIndexCount.png", std::move(implausible_index_count));

    vector<uint8_t> bad_source_size = valid_blob;
    write_u16(bad_source_size, mesh_source_size_offset, uint16_t {0});
    source->AddFile("BadSourceSize.png", std::move(bad_source_size));

    vector<uint8_t> bad_source_offset = valid_blob;
    write_u32(bad_source_offset, mesh_source_offset_offset, uint32_t {2});
    source->AddFile("BadSourceOffset.png", std::move(bad_source_offset));

    vector<uint8_t> bad_coordinate = valid_blob;
    write_u16(bad_coordinate, mesh_vertices_offset, uint16_t {3});
    source->AddFile("BadCoordinate.png", std::move(bad_coordinate));

    vector<uint8_t> bad_index = valid_blob;
    write_u16(bad_index, mesh_indices_offset, uint16_t {3});
    source->AddFile("BadIndex.png", std::move(bad_index));

    vector<uint8_t> degenerate_triangle = valid_blob;
    write_u16(degenerate_triangle, mesh_vertices_offset + 2 * sizeof(uint16_t) * 2, uint16_t {1});
    write_u16(degenerate_triangle, mesh_vertices_offset + 2 * sizeof(uint16_t) * 2 + sizeof(uint16_t), uint16_t {0});
    source->AddFile("DegenerateTriangle.png", std::move(degenerate_triangle));

    SpriteMeshData inconsistent_winding_mesh;
    inconsistent_winding_mesh.Vertices = {{0, 0}, {2, 0}, {0, 2}, {2, 2}};
    inconsistent_winding_mesh.Indices = {0, 1, 2, 1, 2, 3};
    source->AddFile("InconsistentWinding.png", BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Mesh, inconsistent_winding_mesh));

    vector<uint8_t> trailing_data = valid_blob;
    trailing_data.emplace_back(uint8_t {0});
    source->AddFile("TrailingData.png", std::move(trailing_data));

    vector<uint8_t> truncated_payload = valid_blob;
    truncated_payload.resize(mesh_indices_offset);
    source->AddFile("TruncatedPayload.png", std::move(truncated_payload));

    client->SprMngr.GetResources()->AddCustomSource(std::move(source));
    DefaultSpriteFactory factory {&client->SprMngr};
    const auto load = [&client, &factory](string_view path) { return factory.LoadSprite(client->Hashes.ToHashedString(path), AtlasType::MapSprites); };

    auto valid_sprite = load("ValidMesh.png");
    REQUIRE(static_cast<bool>(valid_sprite));
    auto valid_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(valid_sprite->FillData(valid_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 3);

    auto quad_sprite = load("Quad.png");
    REQUIRE(static_cast<bool>(quad_sprite));
    auto quad_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(quad_sprite->FillData(quad_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 6);

    auto empty_sprite = load("Empty.png");
    REQUIRE(static_cast<bool>(empty_sprite));
    auto empty_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(empty_sprite->FillData(empty_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 0);

    CHECK_THROWS(load("BadVersion.png"));
    CHECK_THROWS(load("BadKind.png"));
    CHECK_THROWS(load("BadVertexCount.png"));
    CHECK_THROWS(load("BadIndexCount.png"));
    CHECK_THROWS(load("ImplausibleIndexCount.png"));
    CHECK_THROWS(load("BadSourceSize.png"));
    CHECK_THROWS(load("BadSourceOffset.png"));
    CHECK_THROWS(load("BadCoordinate.png"));
    CHECK_THROWS(load("BadIndex.png"));
    CHECK_THROWS(load("DegenerateTriangle.png"));
    CHECK_THROWS(load("InconsistentWinding.png"));
    CHECK_THROWS(load("TrailingData.png"));
    CHECK_THROWS(load("TruncatedPayload.png"));
}

TEST_CASE("SpriteWireframeRendersThroughPrimitiveOverlay")
{
    auto settings = MakeClientTestSettings();
    settings.DrawWireframe = true;
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    SpriteMeshData mesh;
    mesh.SourceSize = {10, 10};
    mesh.Vertices = {{0, 0}, {5, 10}, {10, 0}};
    mesh.Indices = {0, 1, 2};

    auto [atlas, atlas_allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {10, 10});
    CHECK(atlas->GetSize() == isize32 {12, 12});
    CHECK(atlas_pos == ipos32 {1, 1});
    CHECK(atlas_allocation->GetPosition() == ipos32 {0, 0});
    CHECK(atlas_allocation->GetSize() == isize32 {12, 12});
    const frect32 sprite_atlas_rect {
        numeric_cast<float32_t>(atlas_pos.x) / numeric_cast<float32_t>(atlas->GetSize().width),
        numeric_cast<float32_t>(atlas_pos.y) / numeric_cast<float32_t>(atlas->GetSize().height),
        10.0f / numeric_cast<float32_t>(atlas->GetSize().width),
        10.0f / numeric_cast<float32_t>(atlas->GetSize().height),
    };
    auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, atlas, std::move(atlas_allocation), sprite_atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});

    client->SprMngr.DrawSprite(sprite, {2, 3}, ucolor {255, 255, 255});
    CHECK_NOTHROW(client->SprMngr.Flush());
}

FO_END_NAMESPACE
