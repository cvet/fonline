//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "MapBaker.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static auto MakeMapProtoBlob(EngineMetadata& proto_engine, hstring type_name, string_view proto_name) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> props_data;
    set<hstring> str_hashes;

    auto registrator = proto_engine.GetPropertyRegistrator(type_name);
    REQUIRE(static_cast<bool>(registrator));

    ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), registrator.as_ptr()};
    proto.SetSize(msize {50, 50});
    proto.GetProperties().StoreAllData(props_data, str_hashes);

    vector<uint8_t> protos_data;
    auto writer = DataWriter(protos_data);

    writer.Write<uint32_t>(uint32_t {0});
    ignore_unused(str_hashes);
    writer.Write<uint32_t>(uint32_t {1});
    writer.Write<uint32_t>(uint32_t {1});
    writer.Write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
    writer.WriteStringBytes(type_name.as_str());
    writer.Write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
    writer.WriteStringBytes(proto_name);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
    if (!props_data.empty()) {
        writer.WriteBytes({props_data.data(), props_data.size()});
    }

    return protos_data;
}

static void AddMapBakerMetadataAndProto(BakerTests::TestRig& rig, string_view proto_name)
{
    FO_STACK_TRACE_ENTRY();

    const vector<uint8_t> metadata_blob = BakerTests::MakeEmptyMetadataBlob();
    rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
    rig.AddBakedFile("Metadata.fometa-client", metadata_blob);

    BakerServerEngine server_proto_engine {rig.BakedFiles};
    BakerClientEngine client_proto_engine {rig.BakedFiles};
    const hstring server_map_type = server_proto_engine.Hashes.ToHashedString("Map");
    const hstring client_map_type = client_proto_engine.Hashes.ToHashedString("Map");

    rig.AddBakedFile("MapBakerTest.fopro-bin-server", MakeMapProtoBlob(server_proto_engine, server_map_type, proto_name));
    rig.AddBakedFile("MapBakerTest.fopro-bin-client", MakeMapProtoBlob(client_proto_engine, client_map_type, proto_name));

#if FO_ANGELSCRIPT_SCRIPTING
    const vector<uint8_t> script_blob = BakerTests::CompileInlineScripts(&server_proto_engine, "MapBakerScripts", {{"Scripts/MapBakerScripts.fos", "namespace MapBakerScripts\n{\nvoid Dummy()\n{\n}\n}\n"}}, [](string_view message) {
        const string message_str = string(message);

        if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
            throw ScriptSystemException(message_str);
        }
    });

    rig.AddBakedFile("MapBakerScripts.fos-bin-server", script_blob);
#endif
}

TEST_CASE("MapBaker")
{
    using namespace BakerTests;

    TestRig rig;
    const auto bakers = MakeRequestedBakers({string(MapBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == MapBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 7);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), ""));

    SECTION("UsesMapNameForBakedOutputAndIncrementalTracking")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndProto(local_rig, "UnitTestMap");
        local_rig.AddSourceFile("Nested/UnitTestMap.fomap",
            "[ProtoMap]\n"
            "$Name = UnitTestMap\n");

        vector<string> checked_paths;
        const auto bake_checker = [&](string_view path, uint64_t) {
            checked_paths.emplace_back(path);
            return true;
        };

        MapBaker baker(local_rig.MakeContext("Maps", bake_checker));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("UnitTestMap.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("UnitTestMap.fomap-bin-client"));
        CHECK(std::ranges::find(checked_paths, string {"UnitTestMap.fomap-bin-server"}) != checked_paths.end());
        CHECK(std::ranges::find(checked_paths, string {"UnitTestMap.fomap-bin-client"}) != checked_paths.end());
        CHECK(std::ranges::find(checked_paths, string {"Nested/UnitTestMap.fomap-bin-server"}) == checked_paths.end());
        CHECK(std::ranges::find(checked_paths, string {"Nested/UnitTestMap.fomap-bin-client"}) == checked_paths.end());
    }

    SECTION("FindsSourceMapInSubdirectoryForTargetedRuntimeBake")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndProto(local_rig, "UnitTestMap");
        local_rig.AddSourceFile("Nested/UnitTestMap.fomap",
            "[ProtoMap]\n"
            "$Name = UnitTestMap\n");

        MapBaker baker(local_rig.MakeContext("Maps"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "UnitTestMap.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("UnitTestMap.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("UnitTestMap.fomap-bin-client"));
    }
}

FO_END_NAMESPACE
