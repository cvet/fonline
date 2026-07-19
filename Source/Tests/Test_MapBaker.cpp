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

    ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), registrator};
    proto.SetSize(msize {50, 50});
    proto.GetProperties()->StoreAllData(props_data, str_hashes);

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

static void AddMapBakerMetadataAndEntityProtos(BakerTests::TestRig& rig, string_view map_proto_name, string_view critter_proto_name, string_view visible_item_proto_name, string_view hidden_item_proto_name)
{
    FO_STACK_TRACE_ENTRY();

    AddMapBakerMetadataAndProto(rig, map_proto_name);

    BakerServerEngine server_proto_engine {rig.BakedFiles};
    BakerClientEngine client_proto_engine {rig.BakedFiles};

    const hstring server_critter_type = server_proto_engine.Hashes.ToHashedString("Critter");
    const hstring server_item_type = server_proto_engine.Hashes.ToHashedString("Item");
    const hstring client_item_type = client_proto_engine.Hashes.ToHashedString("Item");

    rig.AddBakedFile("MapBakerCritters.fopro-bin-server",
        BakerTests::MakeMultiProtoResourceBlob<ProtoCritter>(server_proto_engine, server_critter_type,
            {
                {string(critter_proto_name), [&server_proto_engine](ProtoCritter& proto) { proto.SetModelName(server_proto_engine.Hashes.ToHashedString("MapBakerCritterModel")); }},
            }));

    const auto make_item_protos = [](auto& proto_engine, hstring item_type, string_view visible_proto, string_view hidden_proto, bool set_hidden) {
        return BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(proto_engine, item_type,
            {
                {string(visible_proto),
                    [&proto_engine, set_hidden](ProtoItem& proto) {
                        proto.SetStatic(true);
                        if (set_hidden) {
                            proto.SetHidden(false);
                        }
                        proto.SetPicMap(proto_engine.Hashes.ToHashedString("MapBakerVisibleItemPic"));
                    }},
                {string(hidden_proto),
                    [&proto_engine, set_hidden](ProtoItem& proto) {
                        proto.SetStatic(true);
                        if (set_hidden) {
                            proto.SetHidden(true);
                        }
                        proto.SetPicMap(proto_engine.Hashes.ToHashedString("MapBakerHiddenItemPic"));
                    }},
            });
    };

    rig.AddBakedFile("MapBakerItems.fopro-bin-server", make_item_protos(server_proto_engine, server_item_type, visible_item_proto_name, hidden_item_proto_name, true));
    rig.AddBakedFile("MapBakerItems.fopro-bin-client", make_item_protos(client_proto_engine, client_item_type, visible_item_proto_name, hidden_item_proto_name, false));

    rig.AddBakedFile("MapBakerCritterModel", "");
    rig.AddBakedFile("MapBakerVisibleItemPic", "");
    rig.AddBakedFile("MapBakerHiddenItemPic", "");
    rig.AddBakedFile("MapBakerCritterOverride", "");
    rig.AddBakedFile("MapBakerVisibleItemOverride", "");
    rig.AddBakedFile("MapBakerHiddenItemOverride", "");
}

struct BakedMapServerSummary
{
    uint32_t Hashes {};
    uint32_t Critters {};
    uint32_t Items {};
};

struct BakedMapClientSummary
{
    uint32_t Hashes {};
    uint32_t Items {};
};

static void SkipBakedMapStrings(DataReader& reader, uint32_t count)
{
    FO_STACK_TRACE_ENTRY();

    for (uint32_t i = 0; i < count; i++) {
        const uint32_t len = reader.Read<uint32_t>();
        (void)reader.ReadBytes(len);
    }
}

static void SkipBakedMapEntities(DataReader& reader, uint32_t count)
{
    FO_STACK_TRACE_ENTRY();

    for (uint32_t i = 0; i < count; i++) {
        (void)reader.Read<ident_t::underlying_type>();
        (void)reader.Read<hstring::hash_t>();
        const uint32_t props_size = reader.Read<uint32_t>();
        (void)reader.ReadBytes(props_size);
    }
}

static auto ReadBakedMapServerSummary(const vector<uint8_t>& data) -> BakedMapServerSummary
{
    FO_STACK_TRACE_ENTRY();

    auto reader = DataReader {data};
    auto summary = BakedMapServerSummary {};

    summary.Hashes = reader.Read<uint32_t>();
    SkipBakedMapStrings(reader, summary.Hashes);
    summary.Critters = reader.Read<uint32_t>();
    SkipBakedMapEntities(reader, summary.Critters);
    summary.Items = reader.Read<uint32_t>();
    SkipBakedMapEntities(reader, summary.Items);
    reader.VerifyEnd();

    return summary;
}

static auto ReadBakedMapClientSummary(const vector<uint8_t>& data) -> BakedMapClientSummary
{
    FO_STACK_TRACE_ENTRY();

    auto reader = DataReader {data};
    auto summary = BakedMapClientSummary {};

    summary.Hashes = reader.Read<uint32_t>();
    SkipBakedMapStrings(reader, summary.Hashes);
    summary.Items = reader.Read<uint32_t>();
    SkipBakedMapEntities(reader, summary.Items);
    reader.VerifyEnd();

    return summary;
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

    SECTION("SkipsNonMapSourcesAndCheckerRejectedMaps")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Nested/Readme.txt", "not a map");
        local_rig.AddSourceFile("Nested/SkippedMap.fomap",
            "[ProtoMap]\n"
            "$Name = SkippedMap\n");

        vector<pair<string, uint64_t>> checks;
        MapBaker baker(local_rig.MakeContext("Maps", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        }));
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.empty());
        REQUIRE(checks.size() == 2);
        CHECK(checks[0].first == "SkippedMap.fomap-bin-server");
        CHECK(checks[1].first == "SkippedMap.fomap-bin-client");
    }

    SECTION("RechecksSkippedServerSideWhenClientSideNeedsBake")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndProto(local_rig, "UnitTestMap");
        local_rig.AddSourceFile("Nested/UnitTestMap.fomap",
            "[ProtoMap]\n"
            "$Name = UnitTestMap\n");

        vector<string> checked_paths;
        MapBaker baker(local_rig.MakeContext("Maps", [&checked_paths](string_view path, uint64_t) {
            checked_paths.emplace_back(path);
            return path.ends_with("-client");
        }));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.contains("UnitTestMap.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("UnitTestMap.fomap-bin-client"));
        CHECK(std::ranges::count(checked_paths, string {"UnitTestMap.fomap-bin-server"}) == 2);
        CHECK(std::ranges::count(checked_paths, string {"UnitTestMap.fomap-bin-client"}) == 1);
    }

    SECTION("RechecksSkippedClientSideWhenServerSideNeedsBake")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndProto(local_rig, "UnitTestMap");
        local_rig.AddSourceFile("Nested/UnitTestMap.fomap",
            "[ProtoMap]\n"
            "$Name = UnitTestMap\n");

        vector<string> checked_paths;
        MapBaker baker(local_rig.MakeContext("Maps", [&checked_paths](string_view path, uint64_t) {
            checked_paths.emplace_back(path);
            return path.ends_with("-server");
        }));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.contains("UnitTestMap.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("UnitTestMap.fomap-bin-client"));
        CHECK(std::ranges::count(checked_paths, string {"UnitTestMap.fomap-bin-server"}) == 1);
        CHECK(std::ranges::count(checked_paths, string {"UnitTestMap.fomap-bin-client"}) == 2);
    }

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

    SECTION("FindsExactSourceMapForTargetedRuntimeBake")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndProto(local_rig, "ExactMap");
        local_rig.AddSourceFile("ExactMap.fomap",
            "[ProtoMap]\n"
            "$Name = ExactMap\n");

        MapBaker baker(local_rig.MakeContext("Maps"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "ExactMap.fomap-bin-client"));
        CHECK(local_rig.Outputs.contains("ExactMap.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("ExactMap.fomap-bin-client"));
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

    SECTION("BakesCrittersAndStaticItems")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndEntityProtos(local_rig, "RichMap", "MapBakerCritter", "MapBakerVisibleItem", "MapBakerHiddenItem");
        local_rig.AddSourceFile("RichMap.fomap",
            "[ProtoMap]\n"
            "$Name = RichMap\n"
            "[$Name/Critter]\n"
            "$Id = 11\n"
            "$Proto = MapBakerCritter\n"
            "Hex = 10 11\n"
            "ModelName = MapBakerCritterOverride\n"
            "[$Name/Item]\n"
            "$Id = 21\n"
            "$Proto = MapBakerVisibleItem\n"
            "Hex = 12 13\n"
            "PicMap = MapBakerVisibleItemOverride\n"
            "[$Name/Item]\n"
            "$Id = 22\n"
            "$Proto = MapBakerHiddenItem\n"
            "Hex = 14 15\n"
            "PicMap = MapBakerHiddenItemOverride\n");

        MapBaker baker(local_rig.MakeContext("Maps"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        REQUIRE(local_rig.Outputs.contains("RichMap.fomap-bin-server"));
        REQUIRE(local_rig.Outputs.contains("RichMap.fomap-bin-client"));

        const auto server_summary = ReadBakedMapServerSummary(local_rig.Outputs.at("RichMap.fomap-bin-server"));
        const auto client_summary = ReadBakedMapClientSummary(local_rig.Outputs.at("RichMap.fomap-bin-client"));

        CHECK(server_summary.Hashes >= 3);
        CHECK(server_summary.Critters == 1);
        CHECK(server_summary.Items == 2);
        CHECK(client_summary.Hashes >= 2);
        CHECK(client_summary.Items == 1);
    }

    SECTION("RejectsValidationErrors")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndEntityProtos(local_rig, "InvalidResourceMap", "MapBakerCritter", "MapBakerVisibleItem", "MapBakerHiddenItem");
        local_rig.AddSourceFile("InvalidResourceMap.fomap",
            "[ProtoMap]\n"
            "$Name = InvalidResourceMap\n"
            "[$Name/Item]\n"
            "$Id = 21\n"
            "$Proto = MapBakerVisibleItem\n"
            "Hex = 12 13\n"
            "PicMap = MissingMapBakerPic\n");

        MapBaker baker(local_rig.MakeContext("Maps"));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), MapBakerException);
    }

    SECTION("SkipsMissingTargetedRuntimeBake")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Nested/Readme.txt", "not a map");
        local_rig.AddSourceFile("Nested/UnitTestMap.fomap",
            "[ProtoMap]\n"
            "$Name = UnitTestMap\n");

        MapBaker baker(local_rig.MakeContext("Maps"));
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "MissingMap.fomap-bin-client"));
        CHECK(local_rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipTargetedRuntimeBake")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("UnitTestMap.fomap",
            "[ProtoMap]\n"
            "$Name = UnitTestMap\n",
            123);

        vector<pair<string, uint64_t>> checks;
        MapBaker baker(local_rig.MakeContext("Maps", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        }));
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "UnitTestMap.fomap-bin-client"));

        CHECK(local_rig.Outputs.empty());
        REQUIRE(checks.size() == 2);
        CHECK(checks[0] == pair<string, uint64_t> {"UnitTestMap.fomap-bin-server", 123});
        CHECK(checks[1] == pair<string, uint64_t> {"UnitTestMap.fomap-bin-client", 123});
    }

    SECTION("BakesEveryMapFromMultiMapFile")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndProto(local_rig, "MultiMapOne");
        local_rig.AddSourceFile("Nested/MultiMaps.fomap",
            "[ProtoMap]\n"
            "$Name = MultiMapOne\n"
            "[ProtoMap]\n"
            "$Name = MultiMapTwo\n");

        MapBaker baker(local_rig.MakeContext("Maps"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.contains("MultiMapOne.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("MultiMapOne.fomap-bin-client"));
        CHECK(local_rig.Outputs.contains("MultiMapTwo.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("MultiMapTwo.fomap-bin-client"));
    }

    SECTION("FindsMapInsideMultiMapFileForTargetedRuntimeBake")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndProto(local_rig, "MultiMapOne");
        local_rig.AddSourceFile("Nested/MultiMaps.fomap",
            "[ProtoMap]\n"
            "$Name = MultiMapOne\n"
            "[ProtoMap]\n"
            "$Name = MultiMapTwo\n");

        MapBaker baker(local_rig.MakeContext("Maps"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "MultiMapTwo.fomap-bin-server"));

        CHECK(local_rig.Outputs.contains("MultiMapTwo.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("MultiMapTwo.fomap-bin-client"));
        CHECK_FALSE(local_rig.Outputs.contains("MultiMapOne.fomap-bin-server"));
        CHECK_FALSE(local_rig.Outputs.contains("MultiMapOne.fomap-bin-client"));
    }

    SECTION("SkipsProtoFilesWithoutMapAnchors")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/Plain.fopro",
            "[ProtoItem]\n"
            "$Name = PlainItem\n");

        MapBaker baker(local_rig.MakeContext("Maps"));
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.empty());
    }

    SECTION("BakesMapsFromAnyProtoExtensionContainer")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndProto(local_rig, "FoproDeclaredMap");
        local_rig.AddSourceFile("Protos/MixedContainer.fopro",
            "[ProtoMap]\n"
            "$Name = FoproDeclaredMap\n");

        MapBaker baker(local_rig.MakeContext("Maps"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.contains("FoproDeclaredMap.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("FoproDeclaredMap.fomap-bin-client"));
    }

    SECTION("AnonymousAnchorBakesUnderFileStemAlongsideNamedOnes")
    {
        TestRig local_rig;
        AddMapBakerMetadataAndProto(local_rig, "MixedMaps");
        local_rig.AddSourceFile("Nested/MixedMaps.fomap",
            "[ProtoMap]\n"
            "Outside = True\n"
            "[ProtoMap]\n"
            "$Name = MixedMapTwo\n");

        MapBaker baker(local_rig.MakeContext("Maps"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.contains("MixedMaps.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("MixedMaps.fomap-bin-client"));
        CHECK(local_rig.Outputs.contains("MixedMapTwo.fomap-bin-server"));
        CHECK(local_rig.Outputs.contains("MixedMapTwo.fomap-bin-client"));
    }
}

FO_END_NAMESPACE
