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
#include "ModelAnimationData.h"
#include "ModelManager.h"
#include "ModelMeshBaker.h"
#include "ModelMeshData.h"
#include "ModelSprites.h"
#include "PlayerView.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    struct RecordedQuadDraw
    {
        vector<Vertex2D> Vertices {};
        vector<vindex_t> Indices {};
        size_t StartIndex {};
        optional<size_t> IndicesToDraw {};
        nptr<const RenderTexture> CustomTexture {};
    };

    static auto MakeRecordingQuadEffectLoader() -> RenderEffectLoader
    {
        FO_STACK_TRACE_ENTRY();

        return [](string_view name) -> string {
            if (name == "Effects/Test_Recording.fofx") {
                return "[Effect]\nPasses = 1\n";
            }

            if (name == "Effects/Test_Recording.fofx-1-info") {
                return "[EffectInfo]\nMainTex = 0\nSpriteBorderBuf = 1\n";
            }

            throw GenericException("Unexpected recording effect request", name);
        };
    }

    class RecordingQuadEffect final : public RenderEffect
    {
    public:
        RecordingQuadEffect() :
            RenderEffect(EffectUsage::QuadSprite, "Effects/Test_Recording.fofx", MakeRecordingQuadEffectLoader())
        {
            FO_STACK_TRACE_ENTRY();
        }

        void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override
        {
            FO_STACK_TRACE_ENTRY();

            RecordedQuadDraw draw;
            draw.Vertices.assign(dbuf->Vertices.begin(), dbuf->Vertices.begin() + numeric_cast<ptrdiff_t>(dbuf->VertCount));
            draw.Indices.assign(dbuf->Indices.begin(), dbuf->Indices.begin() + numeric_cast<ptrdiff_t>(dbuf->IndCount));
            draw.StartIndex = start_index;
            draw.IndicesToDraw = indices_to_draw;
            draw.CustomTexture = custom_tex;
            Draws.emplace_back(std::move(draw));
        }

        vector<RecordedQuadDraw> Draws {};
    };

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

    static auto MakeClientTestResources(vector<pair<string, vector<uint8_t>>> extra_resources = {}) -> FileSystem
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

        for (auto& [resource_path, resource_data] : extra_resources) {
            runtime_source->AddFile(resource_path, std::move(resource_data));
        }

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    static auto MakeClientEngine(GlobalSettings& settings, FileSystem resources) -> refcount_ptr<ClientEngine>
    {
        return SafeAlloc::MakeRefCounted<ClientEngine>(&settings, std::move(resources), &GetApp()->MainWindow);
    }

    static auto MakeClientEngine(GlobalSettings& settings) -> refcount_ptr<ClientEngine>
    {
        return MakeClientEngine(settings, MakeClientTestResources());
    }

#if FO_ENABLE_3D
    static void WriteRuntimeModelBoneHeader(DataWriter& writer, string_view name, bool attached_mesh)
    {
        FO_STACK_TRACE_ENTRY();

        writer.WriteString(name);
        writer.Write<mat44>(mat44 {1.0f});
        writer.Write<mat44>(mat44 {1.0f});
        writer.Write<uint8_t>(attached_mesh ? uint8_t {1} : uint8_t {0});
    }

    static auto MakeRuntimeModelMesh(const function<void(DataWriter&)>& write_root) -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        vector<uint8_t> data;
        DataWriter writer {data};
        WriteModelMeshHeader(writer);
        write_root(writer);
        return data;
    }

    static auto MakeRuntimeModelMeshWithVertex(const Vertex3D& vertex, uint32_t skin_bones_count = 1) -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        return MakeRuntimeModelMesh([&](DataWriter& writer) {
            WriteRuntimeModelBoneHeader(writer, "Root", true);
            const array<Vertex3D, 1> vertices {vertex};
            writer.Write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
            writer.WriteObjectArray(const_span<Vertex3D> {vertices});
            writer.Write<uint32_t>(uint32_t {0});
            writer.WriteString({});
            writer.Write<uint32_t>(skin_bones_count);

            for (uint32_t i = 0; i < skin_bones_count; i++) {
                writer.WriteString({});
            }

            writer.Write<uint32_t>(skin_bones_count);

            for (uint32_t i = 0; i < skin_bones_count; i++) {
                writer.Write<mat44>(mat44 {1.0f});
            }

            writer.Write<uint32_t>(uint32_t {0});
        });
    }

    static void WriteRuntimeModelDescriptionPrefix(DataWriter& writer)
    {
        FO_STACK_TRACE_ENTRY();

        writer.WriteBytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
        writer.Write<uint16_t>(MODEL_DESCRIPTION_SCHEMA_VERSION);
        writer.Write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
        writer.WriteString("Models/UnusedBase.fbx");
        writer.Write<uint8_t>(uint8_t {0});
        writer.Write<uint8_t>(uint8_t {0});
        writer.Write<uint8_t>(uint8_t {0});
        writer.Write<int32_t>(0);
        writer.Write<int32_t>(0);
        writer.Write<int32_t>(0);
        writer.Write<int32_t>(0);
        writer.WriteString({});
    }

    static void WriteRuntimeModelDescriptionLinkPrefix(DataWriter& writer)
    {
        FO_STACK_TRACE_ENTRY();

        writer.Write<int32_t>(0);
        writer.Write<int32_t>(0);
        writer.WriteString({});
        writer.WriteString({});
        writer.Write<uint8_t>(uint8_t {0});

        for (size_t i = 0; i < 10; i++) {
            writer.Write<float32_t>(0.0f);
        }

        writer.Write<uint32_t>(uint32_t {0});
    }

    static void WriteRuntimeModelDescriptionLink(DataWriter& writer)
    {
        FO_STACK_TRACE_ENTRY();

        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
    }
#endif
}

#if FO_ENABLE_3D
TEST_CASE("ClientEngineLoadsModelMeshBakerOutputThroughRuntimeParser")
{
    constexpr string_view model_path = "Models/RuntimeParserTriangle.obj";

    BakerTests::TestRig rig;
    rig.AddSourceFile(model_path, R"(o RuntimeParserTriangle
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)");

    ModelMeshBaker baker(rig.MakeContext());
    baker.BakeFiles(rig.GetAllSourceFiles(), model_path);

    const auto output_it = rig.Outputs.find(string(model_path));
    REQUIRE(output_it != rig.Outputs.end());

    auto resources = MakeClientTestResources({{string {model_path}, output_it->second}});
    REQUIRE(resources.IsFileExists(model_path));

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, std::move(resources));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    CHECK_NOTHROW(factory->GetModelMngr()->PreloadModel(model_path));
}

TEST_CASE("ClientEngineRejectsMalformedBakedModelCountsAndBounds")
{
    vector<pair<string, vector<uint8_t>>> malformed_resources;

    malformed_resources.emplace_back("Models/VertexCountBomb.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/IndexCountBomb.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/IndexOutOfBounds.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        const array<Vertex3D, 1> vertices {};
        writer.Write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
        writer.WriteObjectArray(const_span<Vertex3D> {vertices});
        const array<vindex_t, 1> indices {vindex_t {1}};
        writer.Write<uint32_t>(numeric_cast<uint32_t>(indices.size()));
        writer.WriteObjectArray(const_span<vindex_t> {indices});
        writer.WriteString({});
        writer.Write<uint32_t>(uint32_t {1});
        writer.WriteString({});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<mat44>(mat44 {1.0f});
        writer.Write<uint32_t>(uint32_t {0});
    }));

    malformed_resources.emplace_back("Models/SkinCountBomb.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        writer.WriteString({});
        writer.Write<uint32_t>(numeric_cast<uint32_t>(MODEL_MAX_BONES + 1));
    }));

    malformed_resources.emplace_back("Models/SkinOffsetMismatch.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        writer.WriteString({});
        writer.Write<uint32_t>(uint32_t {1});
        writer.WriteString({});
        writer.Write<uint32_t>(uint32_t {0});
    }));

    Vertex3D valid_skin_vertex {};
    valid_skin_vertex.BlendWeights[0] = 1.0f;

    Vertex3D non_finite_skin_weight = valid_skin_vertex;
    non_finite_skin_weight.BlendWeights[0] = std::numeric_limits<float32_t>::quiet_NaN();
    malformed_resources.emplace_back("Models/NonFiniteSkinWeight.fbx", MakeRuntimeModelMeshWithVertex(non_finite_skin_weight));

    Vertex3D out_of_range_skin_weight = valid_skin_vertex;
    out_of_range_skin_weight.BlendWeights[0] = -0.25f;
    malformed_resources.emplace_back("Models/OutOfRangeSkinWeight.fbx", MakeRuntimeModelMeshWithVertex(out_of_range_skin_weight));

    Vertex3D non_finite_skin_index = valid_skin_vertex;
    non_finite_skin_index.BlendIndices[0] = std::numeric_limits<float32_t>::infinity();
    malformed_resources.emplace_back("Models/NonFiniteSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(non_finite_skin_index));

    Vertex3D non_integral_skin_index = valid_skin_vertex;
    non_integral_skin_index.BlendIndices[0] = 0.5f;
    malformed_resources.emplace_back("Models/NonIntegralSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(non_integral_skin_index));

    Vertex3D out_of_range_skin_index = valid_skin_vertex;
    out_of_range_skin_index.BlendIndices[0] = 1.0f;
    malformed_resources.emplace_back("Models/OutOfRangeSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(out_of_range_skin_index));

    Vertex3D invalid_skin_weight_sum = valid_skin_vertex;
    invalid_skin_weight_sum.BlendWeights[0] = 0.5f;
    malformed_resources.emplace_back("Models/InvalidSkinWeightSum.fbx", MakeRuntimeModelMeshWithVertex(invalid_skin_weight_sum));

    malformed_resources.emplace_back("Models/ChildCountBomb.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", false);
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/HierarchyDepthBomb.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        for (uint32_t depth = 0; depth <= MODEL_MESH_MAX_HIERARCHY_DEPTH; depth++) {
            WriteRuntimeModelBoneHeader(writer, "Bone", false);
            writer.Write<uint32_t>(depth < MODEL_MESH_MAX_HIERARCHY_DEPTH ? uint32_t {1} : uint32_t {0});
        }
    }));

    {
        vector<uint8_t> data;
        DataWriter writer {data};
        writer.WriteBytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
        writer.Write<uint16_t>(MODEL_DESCRIPTION_SCHEMA_VERSION);
        writer.Write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionStringBomb.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data;
        DataWriter writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLink(writer);
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionLinksBomb.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data;
        DataWriter writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionNestedCountBomb.fo3d", std::move(data));
    }

    const array<pair<string_view, string_view>, 16> expected_failures {{
        {"Models/VertexCountBomb.fbx", "vertex count exceeds maximum addressable count"},
        {"Models/IndexCountBomb.fbx", "mesh indices"},
        {"Models/IndexOutOfBounds.fbx", "outside vertex count"},
        {"Models/SkinCountBomb.fbx", "skin bone count exceeds maximum"},
        {"Models/SkinOffsetMismatch.fbx", "skin bone offset count mismatch"},
        {"Models/NonFiniteSkinWeight.fbx", "non-finite skin weight"},
        {"Models/OutOfRangeSkinWeight.fbx", "skin weight outside [0, 1]"},
        {"Models/NonFiniteSkinIndex.fbx", "non-finite skin index"},
        {"Models/NonIntegralSkinIndex.fbx", "non-integral skin index"},
        {"Models/OutOfRangeSkinIndex.fbx", "skin index outside valid range"},
        {"Models/InvalidSkinWeightSum.fbx", "skin-weight sum"},
        {"Models/ChildCountBomb.fbx", "child count exceeds maximum"},
        {"Models/HierarchyDepthBomb.fbx", "hierarchy depth"},
        {"Models/DescriptionStringBomb.fo3d", "String length exceeds remaining buffer"},
        {"Models/DescriptionLinksBomb.fo3d", "links"},
        {"Models/DescriptionNestedCountBomb.fo3d", "disabled meshes"},
    }};

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(malformed_resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    auto model_mngr = factory->GetModelMngr();

    for (const auto& [model_path, expected_failure] : expected_failures) {
        INFO(model_path);
        CHECK_THROWS_WITH(model_mngr->PreloadModel(model_path), Catch::Matchers::ContainsSubstring(std::string {expected_failure}));
    }
}
#endif

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
        CHECK_FALSE(sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect).has_value());
        CHECK(draw_buf->VertCount == 0);
        CHECK(draw_buf->IndCount == 0);
    }

    SECTION("Mesh maps positions UVs indices and horizontal light colors")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 10}, {10, 0}};
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
        CHECK(center.PosY == Catch::Approx(240.0f));
        CHECK(center.TexU == Catch::Approx(0.5f));
        CHECK(center.TexV == Catch::Approx(0.75f));
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

        CHECK(sprite->GetSize() == isize32 {10, 10});
        CHECK(sprite->GetOffset() == ipos32 {});
        REQUIRE(sprite->FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        REQUIRE(draw_buf->VertCount == 3);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[1].PosX == Catch::Approx(110.0f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(116.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[1].TexU == Catch::Approx(0.5f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[0].Color == (ucolor {30, 40, 50, 60}));
        CHECK(draw_buf->Vertices[1].Color == (ucolor {60, 70, 80, 90}));
        CHECK(draw_buf->Vertices[2].Color == (ucolor {90, 100, 110, 120}));
    }

    SECTION("Cropped mesh region preserves logical source coordinates")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 3};
        mesh.Vertices = {{0, 0}, {6, 0}, {0, 5}};
        mesh.Indices = {0, 1, 2};

        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {6, 5}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
        const optional<AtlasSpriteRegion> region = sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect);

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(104.0f));
        CHECK(region->DrawRect.y == Catch::Approx(212.0f));
        CHECK(region->DrawRect.width == Catch::Approx(12.0f));
        CHECK(region->DrawRect.height == Catch::Approx(20.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f));
        CHECK(region->TextureRect.width == Catch::Approx(0.5f));
        CHECK(region->TextureRect.height == Catch::Approx(0.25f));
        REQUIRE(sprite->FillRegionData(draw_buf, {0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect, color_left) == 6);
        REQUIRE(draw_buf->VertCount == 4);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[0].PosY == Catch::Approx(232.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[0].TexV == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[1].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[1].PosY == Catch::Approx(212.0f));
        CHECK(draw_buf->Vertices[1].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[1].TexV == Catch::Approx(0.5f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(116.0f));
        CHECK(draw_buf->Vertices[2].PosY == Catch::Approx(212.0f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[2].TexV == Catch::Approx(0.5f));
    }

    SECTION("Expanded mesh region clips atlas padding outside the logical source")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {-2, -1};
        mesh.Vertices = {{0, 0}, {14, 0}, {0, 13}};
        mesh.Indices = {0, 1, 2};

        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {14, 13}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
        const optional<AtlasSpriteRegion> region = sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect);

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(100.0f));
        CHECK(region->DrawRect.y == Catch::Approx(200.0f));
        CHECK(region->DrawRect.width == Catch::Approx(20.0f));
        CHECK(region->DrawRect.height == Catch::Approx(40.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f + 0.5f * 2.0f / 14.0f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f + 0.25f / 13.0f));
        CHECK(region->TextureRect.width == Catch::Approx(0.5f * 10.0f / 14.0f));
        CHECK(region->TextureRect.height == Catch::Approx(0.25f * 10.0f / 13.0f));
        REQUIRE(sprite->FillRegionData(draw_buf, {0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect, color_left) == 6);
        REQUIRE(draw_buf->VertCount == 4);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(100.0f));
        CHECK(draw_buf->Vertices[0].PosY == Catch::Approx(240.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f + 0.5f * 2.0f / 14.0f));
        CHECK(draw_buf->Vertices[0].TexV == Catch::Approx(0.5f + 0.25f * 11.0f / 13.0f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(120.0f));
        CHECK(draw_buf->Vertices[2].PosY == Catch::Approx(200.0f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.25f + 0.5f * 12.0f / 14.0f));
        CHECK(draw_buf->Vertices[2].TexV == Catch::Approx(0.5f + 0.25f / 13.0f));
    }

    SECTION("Partial logical region intersects the cropped atlas frame")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 3};
        mesh.Vertices = {{0, 0}, {6, 0}, {0, 5}};
        mesh.Indices = {0, 1, 2};

        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {6, 5}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        const optional<AtlasSpriteRegion> region = sprite->ResolveRegion({0.1f, 0.2f}, {0.5f, 0.6f}, draw_rect);

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(105.0f));
        CHECK(region->DrawRect.y == Catch::Approx(210.0f));
        CHECK(region->DrawRect.width == Catch::Approx(15.0f));
        CHECK(region->DrawRect.height == Catch::Approx(30.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f));
        CHECK(region->TextureRect.width == Catch::Approx(0.25f));
        CHECK(region->TextureRect.height == Catch::Approx(0.15f));
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
    SpriteMeshData cropped_mesh;
    cropped_mesh.SourceSize = {4, 3};
    cropped_mesh.SourceOffset = {1, -1};
    cropped_mesh.Vertices = {{0, 0}, {3, 0}, {0, 3}};
    cropped_mesh.Indices = {0, 1, 2};
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
    source->AddFile("CroppedMesh.png", BakerTests::MakeMinimalBakedSprite(3, 3, SpriteMeshKind::Mesh, cropped_mesh));

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

    auto cropped_sprite = load("CroppedMesh.png");
    REQUIRE(cropped_sprite);
    CHECK(cropped_sprite->GetSize() == cropped_mesh.SourceSize);
    CHECK(cropped_sprite->GetOffset() == ipos32 {0, 1});
    CHECK_FALSE(cropped_sprite->IsHitTest({0, 0}));
    CHECK(cropped_sprite->IsHitTest({1, 0}));
    CHECK(cropped_sprite->IsHitTest({3, 1}));
    CHECK_FALSE(cropped_sprite->IsHitTest({3, 2}));
    auto cropped_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    REQUIRE(cropped_sprite->FillData(cropped_draw_buf, frect32 {0.0f, 0.0f, 4.0f, 3.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 3);
    REQUIRE(cropped_draw_buf->VertCount == 3);
    CHECK(cropped_draw_buf->Vertices[0].PosX == Catch::Approx(1.0f));
    CHECK(cropped_draw_buf->Vertices[0].PosY == Catch::Approx(-1.0f));
    CHECK(cropped_draw_buf->Vertices[1].PosX == Catch::Approx(4.0f));
    CHECK(cropped_draw_buf->Vertices[1].PosY == Catch::Approx(-1.0f));
    CHECK(cropped_draw_buf->Vertices[2].PosX == Catch::Approx(1.0f));
    CHECK(cropped_draw_buf->Vertices[2].PosY == Catch::Approx(2.0f));

    auto restored_image = client->SprMngr.LoadSpriteAsQuad(client->Hashes.ToHashedString("CroppedMesh.png"), AtlasType::IfaceSprites);
    REQUIRE(restored_image);
    CHECK(restored_image->GetSize() == cropped_mesh.SourceSize);
    CHECK(restored_image->GetAtlasRect().width * restored_image->GetAtlas()->GetTexture()->SizeData[0] == Catch::Approx(4.0f));
    CHECK(restored_image->GetAtlasRect().height * restored_image->GetAtlas()->GetTexture()->SizeData[1] == Catch::Approx(3.0f));
    const optional<AtlasSpriteRegion> restored_region = restored_image->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 4.0f, 3.0f});
    REQUIRE(restored_region.has_value());
    CHECK(restored_region->DrawRect == frect32 {0.0f, 0.0f, 4.0f, 3.0f});
    CHECK(restored_region->TextureRect == restored_image->GetAtlasRect());
    auto restored_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(restored_image->FillData(restored_draw_buf, frect32 {0.0f, 0.0f, 4.0f, 3.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 6);

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

TEST_CASE("SpriteManagerMapsPolygonAtlasPatternsAndPaddedEffects")
{
    SpriteMeshData mesh;
    mesh.SourceSize = {4, 3};
    mesh.SourceOffset = {1, 0};
    mesh.Vertices = {{0, 0}, {3, 0}, {0, 3}};
    mesh.Indices = {0, 1, 2};

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources({{"PatternMesh.png", BakerTests::MakeMinimalBakedSprite(3, 3, SpriteMeshKind::Mesh, mesh)}}));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    RecordingQuadEffect effect;

    auto sprite = client->SprMngr.LoadSprite("PatternMesh.png", AtlasType::IfaceSprites);
    REQUIRE(sprite);
    auto atlas_sprite = sprite.dyn_cast<AtlasSprite>();
    REQUIRE(atlas_sprite);
    sprite->SetDrawEffect(make_nptr(&effect));

    client->SprMngr.DrawSpritePattern(sprite, {10, 20}, {7, 5}, {4, 3}, ucolor {255, 255, 255, 255});
    client->SprMngr.Flush();

    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& pattern_draw = effect.Draws.front();
    REQUIRE(pattern_draw.Vertices.size() == 16);
    REQUIRE(pattern_draw.Indices.size() == 24);
    CHECK(pattern_draw.StartIndex == 0);
    REQUIRE(pattern_draw.IndicesToDraw.has_value());
    CHECK(*pattern_draw.IndicesToDraw == 24);
    CHECK(pattern_draw.CustomTexture == atlas_sprite->GetBatchTexture());

    const frect32 atlas_rect = atlas_sprite->GetAtlasRect();
    const auto check_quad = [](const RecordedQuadDraw& draw, size_t vertex, frect32 draw_rect, frect32 texture_rect) {
        REQUIRE(vertex + 3 < draw.Vertices.size());
        CHECK(draw.Vertices[vertex + 0].PosX == Catch::Approx(draw_rect.x));
        CHECK(draw.Vertices[vertex + 0].PosY == Catch::Approx(draw_rect.y + draw_rect.height));
        CHECK(draw.Vertices[vertex + 0].TexU == Catch::Approx(texture_rect.x));
        CHECK(draw.Vertices[vertex + 0].TexV == Catch::Approx(texture_rect.y + texture_rect.height));
        CHECK(draw.Vertices[vertex + 1].PosX == Catch::Approx(draw_rect.x));
        CHECK(draw.Vertices[vertex + 1].PosY == Catch::Approx(draw_rect.y));
        CHECK(draw.Vertices[vertex + 1].TexU == Catch::Approx(texture_rect.x));
        CHECK(draw.Vertices[vertex + 1].TexV == Catch::Approx(texture_rect.y));
        CHECK(draw.Vertices[vertex + 2].PosX == Catch::Approx(draw_rect.x + draw_rect.width));
        CHECK(draw.Vertices[vertex + 2].PosY == Catch::Approx(draw_rect.y));
        CHECK(draw.Vertices[vertex + 2].TexU == Catch::Approx(texture_rect.x + texture_rect.width));
        CHECK(draw.Vertices[vertex + 2].TexV == Catch::Approx(texture_rect.y));
        CHECK(draw.Vertices[vertex + 3].PosX == Catch::Approx(draw_rect.x + draw_rect.width));
        CHECK(draw.Vertices[vertex + 3].PosY == Catch::Approx(draw_rect.y + draw_rect.height));
        CHECK(draw.Vertices[vertex + 3].TexU == Catch::Approx(texture_rect.x + texture_rect.width));
        CHECK(draw.Vertices[vertex + 3].TexV == Catch::Approx(texture_rect.y + texture_rect.height));
    };

    check_quad(pattern_draw, 0, {11.0f, 20.0f, 3.0f, 3.0f}, atlas_rect);
    check_quad(pattern_draw, 4, {15.0f, 20.0f, 2.0f, 3.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width * 2.0f / 3.0f, atlas_rect.height});
    check_quad(pattern_draw, 8, {11.0f, 23.0f, 3.0f, 2.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width, atlas_rect.height * 2.0f / 3.0f});
    check_quad(pattern_draw, 12, {15.0f, 23.0f, 2.0f, 2.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width * 2.0f / 3.0f, atlas_rect.height * 2.0f / 3.0f});

    effect.Draws.clear();
    REQUIRE(client->SprMngr.DrawSpriteRegion(sprite, {0.0f, 0.0f}, {1.0f, 1.0f}, {50.0f, 60.0f}, {4.0f, 3.0f}, ucolor {255, 255, 255, 255}));
    client->SprMngr.Flush();
    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& region_draw = effect.Draws.front();
    REQUIRE(region_draw.Vertices.size() == 4);
    REQUIRE(region_draw.Indices.size() == 6);
    check_quad(region_draw, 0, {51.0f, 60.0f, 3.0f, 3.0f}, atlas_rect);

    effect.Draws.clear();
    effect.SpriteBorderBuf.reset();
    constexpr int32_t padding = 2;
    client->SprMngr.DrawSpriteWithEffect(sprite, {100, 200}, ucolor {255, 255, 255, 255}, make_ptr(&effect), padding);

    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& effect_draw = effect.Draws.front();
    REQUIRE(effect_draw.Vertices.size() == 4);
    REQUIRE(effect_draw.Indices.size() == 6);
    CHECK(effect_draw.StartIndex == 0);
    CHECK_FALSE(effect_draw.IndicesToDraw.has_value());
    CHECK(effect_draw.CustomTexture == atlas_sprite->GetBatchTexture());

    const float32_t texture_padding_x = atlas_sprite->GetAtlas()->GetTexture()->SizeData[2] * numeric_cast<float32_t>(padding);
    const float32_t texture_padding_y = atlas_sprite->GetAtlas()->GetTexture()->SizeData[3] * numeric_cast<float32_t>(padding);
    const frect32 effect_draw_rect {99.0f, 198.0f, 7.0f, 7.0f};
    const frect32 effect_texture_rect {atlas_rect.x - texture_padding_x, atlas_rect.y - texture_padding_y, atlas_rect.width + texture_padding_x * 2.0f, atlas_rect.height + texture_padding_y * 2.0f};

    CHECK(effect_draw.Vertices[0].PosX == Catch::Approx(effect_draw_rect.x));
    CHECK(effect_draw.Vertices[0].PosY == Catch::Approx(effect_draw_rect.y + effect_draw_rect.height));
    CHECK(effect_draw.Vertices[0].TexU == Catch::Approx(effect_texture_rect.x));
    CHECK(effect_draw.Vertices[0].TexV == Catch::Approx(effect_texture_rect.y + effect_texture_rect.height));
    CHECK(effect_draw.Vertices[1].PosX == Catch::Approx(effect_draw_rect.x));
    CHECK(effect_draw.Vertices[1].PosY == Catch::Approx(effect_draw_rect.y));
    CHECK(effect_draw.Vertices[1].TexU == Catch::Approx(effect_texture_rect.x));
    CHECK(effect_draw.Vertices[1].TexV == Catch::Approx(effect_texture_rect.y));
    CHECK(effect_draw.Vertices[2].PosX == Catch::Approx(effect_draw_rect.x + effect_draw_rect.width));
    CHECK(effect_draw.Vertices[2].PosY == Catch::Approx(effect_draw_rect.y));
    CHECK(effect_draw.Vertices[2].TexU == Catch::Approx(effect_texture_rect.x + effect_texture_rect.width));
    CHECK(effect_draw.Vertices[2].TexV == Catch::Approx(effect_texture_rect.y));
    CHECK(effect_draw.Vertices[3].PosX == Catch::Approx(effect_draw_rect.x + effect_draw_rect.width));
    CHECK(effect_draw.Vertices[3].PosY == Catch::Approx(effect_draw_rect.y + effect_draw_rect.height));
    CHECK(effect_draw.Vertices[3].TexU == Catch::Approx(effect_texture_rect.x + effect_texture_rect.width));
    CHECK(effect_draw.Vertices[3].TexV == Catch::Approx(effect_texture_rect.y + effect_texture_rect.height));

    REQUIRE(effect.SpriteBorderBuf.has_value());
    CHECK(effect.SpriteBorderBuf->SpriteBorder[0] == Catch::Approx(atlas_rect.x));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[1] == Catch::Approx(atlas_rect.y));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[2] == Catch::Approx(atlas_rect.x + atlas_rect.width));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[3] == Catch::Approx(atlas_rect.y + atlas_rect.height));
    sprite->SetDrawEffect(nullptr);
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
