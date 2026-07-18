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
#if FO_ENABLE_3D
#include "ModelAnimationData.h"
#include "ModelManager.h"
#include "ModelMeshBaker.h"
#include "ModelMeshData.h"
#include "ModelSprites.h"
#endif
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

FO_END_NAMESPACE
