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

#include "Test_BakerHelpers.h"

#include "DataSerialization.h"

#if FO_ANGELSCRIPT_SCRIPTING
#include "MetadataBaker.h"
#include "MetadataRegistration.h"
#endif

FO_BEGIN_NAMESPACE

#if FO_ANGELSCRIPT_SCRIPTING
static void ExpectMetadataBakerError(string_view script, string_view message)
{
    FO_STACK_TRACE_ENTRY();

    BakerTests::TestRig local_rig;
    local_rig.AddSourceFile("Scripts/BrokenMetadata.fos", string(script));

    MetadataBaker baker(local_rig.MakeContext());
    REQUIRE_THROWS_WITH(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring(std::string(message)));
}
#endif

TEST_CASE("MetadataBaker")
{
#if FO_ANGELSCRIPT_SCRIPTING
    using namespace BakerTests;

    TestRig rig;
    auto bakers = MakeRequestedBakers({string(MetadataBaker::NAME)}, rig);
    auto skip_baked_header = [](data_reader& reader) {
        ignore_unused(reader.read<uint32_t>());
        ignore_unused(reader.read<uint16_t>());
        auto version_size = reader.read<uint16_t>();
        ignore_unused(reader.read_string_view(version_size));
    };
    auto read_baked_header = [](const vector<uint8_t>& output) {
        data_reader reader(output);
        CHECK(reader.read<uint32_t>() == METADATA_FILE_MAGIC);
        CHECK(reader.read<uint16_t>() == METADATA_FILE_VERSION);
        auto version_size = reader.read<uint16_t>();
        string version;
        version.resize(version_size);
        reader.read_string_bytes(version);
        return version;
    };
    auto read_baked_tags = [&skip_baked_header](const vector<uint8_t>& output) {
        map<string, vector<vector<string>>> tags;
        data_reader reader(output);
        skip_baked_header(reader);
        auto tag_count = reader.read<uint16_t>();
        auto read_string = [&reader](uint16_t size) -> string {
            string value;
            value.resize(size);
            reader.read_string_bytes(value);
            return value;
        };

        for (uint16_t i = 0; i < tag_count; i++) {
            auto tag_name_len = reader.read<uint16_t>();
            string tag_name = read_string(tag_name_len);
            auto tag_value_count = reader.read<uint32_t>();

            for (uint32_t j = 0; j < tag_value_count; j++) {
                auto value_parts_count = reader.read<uint32_t>();
                vector<string> value_parts;
                value_parts.reserve(value_parts_count);

                for (uint32_t k = 0; k < value_parts_count; k++) {
                    auto part_len = reader.read<uint16_t>();
                    value_parts.emplace_back(read_string(part_len));
                }

                tags[tag_name].emplace_back(std::move(value_parts));
            }
        }

        reader.verify_end();
        return tags;
    };

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == MetadataBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 1);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));

    SECTION("skips non metadata targets before parsing scripts")
    {
        rig.AddSourceFile("Scripts/Broken.fos", "///@ Unknown Tag");

        MetadataBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "metadata.bin"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("ignores non script inputs")
    {
        rig.AddSourceFile("Docs/Notes.txt", "///@ Enum Ignored Value = 1");

        MetadataBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK(rig.Outputs.empty());
    }

    SECTION("uses bake checker per side with newest script write time")
    {
        rig.AddSourceFile("Scripts/01_Enum.fos", "///@ Enum CoverageSide First = 1", 11);
        rig.AddSourceFile("Scripts/02_Enum.fos", "///@ Enum CoverageSide Second = 2", 42);

        vector<pair<string, uint64_t>> checks;
        MetadataBaker baker(rig.MakeContext("MetaPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return path.ends_with(".fometa-client");
        }));

        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        REQUIRE(checks.size() == 3);
        CHECK(checks[0] == pair<string, uint64_t> {"MetaPack.fometa-server", 42});
        CHECK(checks[1] == pair<string, uint64_t> {"MetaPack.fometa-client", 42});
        CHECK(checks[2] == pair<string, uint64_t> {"MetaPack.fometa-mapper", 42});
        REQUIRE(rig.Outputs.size() == 1);
        REQUIRE(rig.Outputs.contains("MetaPack.fometa-client"));

        auto tags = read_baked_tags(rig.Outputs.at("MetaPack.fometa-client"));
        auto enum_it = tags.find("Enum");

        REQUIRE(enum_it != tags.end());
        CHECK(std::ranges::count(enum_it->second, vector<string> {"CoverageSide", "uint8", "First", "1", "Second", "2"}) == 1);
    }

    SECTION("uses the applied config write time for setting values")
    {
        string temp_dir = fs::path_to_string(std::filesystem::temp_directory_path() / std::format("metadata_setting_config_{}", std::chrono::steady_clock::now().time_since_epoch().count()));
        REQUIRE(fs::create_directories(temp_dir));
        string config_path = strex(temp_dir).combine_path("Test.fomain").str();
        REQUIRE(fs::write_file(config_path, "Coverage.Enabled = true\n"));

        rig.Settings.ApplyConfigAtPath("Test.fomain", temp_dir);
        rig.AddSourceFile("Scripts/TestSetting.fos", "///@ Setting Client bool Coverage.Enabled", 1);

        vector<uint64_t> checked_write_times;
        MetadataBaker baker(rig.MakeContext("MetaPack", [&checked_write_times](string_view, uint64_t write_time) {
            checked_write_times.emplace_back(write_time);
            return false;
        }));

        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(checked_write_times.size() == 3);
        CHECK(std::ranges::all_of(checked_write_times, [&](uint64_t write_time) { return write_time == fs::last_write_time(config_path); }));
        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("returns without output when bake checker rejects every side")
    {
        rig.AddSourceFile("Scripts/TestEnum.fos", "///@ Enum CoverageDisabled Value = 1", 7);

        vector<pair<string, uint64_t>> checks;
        MetadataBaker baker(rig.MakeContext("MetaPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        }));

        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        REQUIRE(checks.size() == 3);
        CHECK(checks[0] == pair<string, uint64_t> {"MetaPack.fometa-server", 7});
        CHECK(checks[1] == pair<string, uint64_t> {"MetaPack.fometa-client", 7});
        CHECK(checks[2] == pair<string, uint64_t> {"MetaPack.fometa-mapper", 7});
        CHECK(rig.Outputs.empty());
    }

    SECTION("writes one metadata version shared by every target")
    {
        rig.AddSourceFile("Scripts/TestMetadataVersion.fos", R"(
namespace TestMetadataVersion
{
///@ Property Critter Server int32 LayoutValue Mutable
///@ Property Critter Common bool LayoutFlag Mutable PublicSync
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        auto read_metadata_version = [&](string_view target) -> string {
            string output_name = strex("TestPack.fometa-{}", target).str();
            REQUIRE(rig.Outputs.contains(output_name));

            return read_baked_header(rig.Outputs.at(output_name));
        };

        // Client and server resolve wire property indices through their own metadata, so a target-dependent
        // version would leave the mismatch it exists to catch undetectable
        string client_version = read_metadata_version("client");
        CHECK_FALSE(client_version.empty());
        CHECK(read_metadata_version("server") == client_version);
        CHECK(read_metadata_version("mapper") == client_version);

        auto bake_version_of = [&](string_view script) -> string {
            TestRig changed_rig;
            changed_rig.AddSourceFile("Scripts/TestMetadataVersion.fos", string(script));

            MetadataBaker changed_baker(changed_rig.MakeContext());
            REQUIRE_NOTHROW(changed_baker.BakeFiles(changed_rig.GetAllSourceFiles(), ""));
            REQUIRE(changed_rig.Outputs.contains("TestPack.fometa-client"));

            return read_baked_header(changed_rig.Outputs.at("TestPack.fometa-client"));
        };

        // Inserting a property shifts every reg index below it, which is exactly the desync being versioned
        CHECK(bake_version_of(R"(
namespace TestMetadataVersion
{
///@ Property Critter Server int32 LayoutValue Mutable
///@ Property Critter Server int32 LayoutExtra Mutable
///@ Property Critter Common bool LayoutFlag Mutable PublicSync
}
)") != client_version);

        // Every tag kind takes part, not only the ones that shape the property table: metadata that differs in
        // any way came from another bake, and that is what the version has to catch
        CHECK(bake_version_of(R"(
namespace TestMetadataVersion
{
///@ Property Critter Server int32 LayoutValue Mutable
///@ Property Critter Common bool LayoutFlag Mutable PublicSync
///@ Enum CoverageMood Calm = 1
}
)") != client_version);
    }

    SECTION("keeps the metadata version apart when a token repeats the tag name")
    {
        auto bake_version_of = [&](string_view script) -> string {
            TestRig local_rig;
            local_rig.AddSourceFile("Scripts/TestMetadataVersionCollision.fos", string(script));

            MetadataBaker baker(local_rig.MakeContext());
            REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
            REQUIRE(local_rig.Outputs.contains("TestPack.fometa-client"));

            return read_baked_header(local_rig.Outputs.at("TestPack.fometa-client"));
        };

        // Two tags versus one tag whose trailing tokens spell out the second one. ParseEnum reads the first entry
        // and ignores the tail, so these are genuinely different metadata, and the version has to say so
        string two_tags = bake_version_of(R"(
namespace TestMetadataVersionCollision
{
///@ Enum CoverageAlpha Key = 1
///@ Enum CoverageBeta Key = 2
}
)");
        string one_tag = bake_version_of(R"(
namespace TestMetadataVersionCollision
{
///@ Enum CoverageAlpha Key = 1 Enum CoverageBeta Key = 2
}
)");

        CHECK_FALSE(two_tags.empty());
        CHECK(two_tags != one_tag);
    }

    SECTION("parses continued tags and strips trailing comments")
    {
        rig.AddSourceFile("Scripts/TestContinuation.fos", R"(
namespace TestContinuation
{
///@ Enum ContinuedCoverage \
Value = 5 // trailing comment
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        auto tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-client"));
        auto enum_it = tags.find("Enum");

        REQUIRE(enum_it != tags.end());
        CHECK(std::ranges::count(enum_it->second, vector<string> {"ContinuedCoverage", "uint8", "Value", "5"}) == 1);
    }

    SECTION("serializes enum value edges")
    {
        rig.AddSourceFile("Scripts/TestEnumEdges.fos", R"(
namespace TestEnumEdges
{
///@ Enum CoverageSigned Negative = - 2
///@ Enum CoverageSigned Auto
///@ Enum CoverageUint8 High = 255
///@ Enum CoverageUint16 Wide = 256
///@ Enum CoverageInt32 Wide = 65536
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        auto tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-client"));
        auto enum_it = tags.find("Enum");

        REQUIRE(enum_it != tags.end());
        CHECK(std::ranges::count(enum_it->second, vector<string> {"CoverageSigned", "int32", "Negative", "-2", "Auto", "0"}) == 1);
        CHECK(std::ranges::count(enum_it->second, vector<string> {"CoverageUint8", "uint8", "High", "255"}) == 1);
        CHECK(std::ranges::count(enum_it->second, vector<string> {"CoverageUint16", "uint16", "Wide", "256"}) == 1);
        CHECK(std::ranges::count(enum_it->second, vector<string> {"CoverageInt32", "int32", "Wide", "65536"}) == 1);
    }

    SECTION("extends engine enums and rejects engine enum collisions")
    {
        rig.AddSourceFile("Scripts/TestEngineEnumEdges.fos", R"(
namespace TestEngineEnumEdges
{
///@ Enum CritterProperty MetadataBakerCoverageExtra
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-server"));

        auto tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-server"));
        auto enum_it = tags.find("Enum");

        REQUIRE(enum_it != tags.end());
        CHECK(std::ranges::any_of(enum_it->second, [](const vector<string>& entry) { return entry.size() == 4 && entry[0] == "CritterProperty" && entry[1].empty() && entry[2] == "MetadataBakerCoverageExtra"; }));

        vector<pair<string_view, string_view>> cases = {
            {"///@ Enum CritterProperty None", "cannot override engine enum value"},
            {"///@ Enum CritterProperty MetadataBakerCoverageCollision = 0", "duplicate enum value in engine enum"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("rejects invalid enum declarations")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ Enum BadOnly", "insufficient parameters"},
            {"///@ Enum Bad Entry\n///@ Enum Bad Entry", "duplicate enum entry"},
            {"///@ Enum Bad Entry =", "expected value after '='"},
            {"///@ Enum Bad Entry = word", "expected number after '='"},
            {"///@ Enum Bad Entry = - word", "expected number after '-'"},
            {"///@ Enum Bad Entry = 2147483648", "enum value out of int32 range"},
            {"///@ Enum Bad First = 1\n///@ Enum Bad Second = 1", "duplicate enum value"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("rejects invalid entity declarations")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ Entity Server", "insufficient parameters"},
            {"///@ Entity Server ProtoBad", "entity name cannot start with 'Proto'"},
            {"///@ Entity Server AbstractBad", "entity name cannot start with 'Abstract'"},
            {"///@ Entity Server StaticBad", "entity name cannot start with 'Static'"},
            {"///@ Entity Server CoverageEntity\n///@ Entity Server CoverageEntity", "duplicate entity type"},
            {"///@ Entity Server int32", "entity name conflict with another type"},
            {"///@ Enum CoverageEntityProperty Entry\n///@ Entity Server CoverageEntity", "property enum conflict with another type"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("serializes value type layouts")
    {
        rig.AddSourceFile("Scripts/TestValueType.fos", R"(
namespace TestValueType
{
///@ ValueType Common CoverageValue Layout = hstring - Label + int32 - Count + bool - Enabled
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        auto tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-client"));
        auto value_type_it = tags.find("ValueType");

        REQUIRE(value_type_it != tags.end());
        CHECK(std::ranges::count(value_type_it->second, vector<string> {"CoverageValue", "Label", "hstring", "Count", "int32", "Enabled", "bool"}) == 1);

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ClientSide);
        REQUIRE_NOTHROW(RegisterDynamicMetadata(&meta, rig.Outputs.at("TestPack.fometa-client")));
        CHECK(meta.GetBaseType("CoverageValue").IsComplexStruct);
    }

    SECTION("rejects invalid value type declarations")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ ValueType Common Bad Layout =", "insufficient parameters"},
            {"///@ ValueType Mapper Bad Layout = int32 - Value", "invalid target"},
            {"///@ ValueType Common Bad Wrong = int32 - Value", "expected 'Layout ='"},
            {"///@ ValueType Common Bad Layout = int32 - Value\n///@ ValueType Common Bad Layout = bool - Flag", "duplicate type name"},
            {"///@ ValueType Common Bad Layout = - Value + bool - Flag", "expected 'type-field' layout entry"},
            {"///@ ValueType Common Bad Layout = MissingType - Value", "cannot resolve field type"},
            {"///@ ValueType Common Bad Layout = int32 Extra - Value", "invalid field type"},
            {"///@ ValueType Common Bad Layout = int32[] - Values", "field type must be a plain value"},
            {"///@ Entity Common TargetEntity\n///@ ValueType Common Bad Layout = TargetEntity - Value", "unsupported field type"},
            {"///@ ValueType Common Bad Layout = int32 -", "insufficient parameters"},
            {"///@ ValueType Common Bad Layout = int32 - ,", "invalid field name"},
            {"///@ ValueType Common Bad Layout = int32 - Value + bool - Value", "duplicate field name"},
            {"///@ ValueType Common Bad Layout = int32 - Count bool - Enabled", "expected '+' between layout entries"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("serializes entity flags and holder entries")
    {
        rig.AddSourceFile("Scripts/TestEntityFlagsAndHolders.fos", R"(
namespace TestEntityFlagsAndHolders
{
///@ Entity Common CoverageHolder Global HasProtos HasStatics HasAbstract
///@ Entity Server CoverageTarget
///@ EntityHolder Server CoverageHolder CoverageTarget Items Persistent
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-server"));

        auto tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-server"));
        auto entity_it = tags.find("Entity");
        auto holder_it = tags.find("EntityHolder");

        REQUIRE(entity_it != tags.end());
        CHECK(std::ranges::count(entity_it->second, vector<string> {"CoverageHolder", "Global", "HasProtos", "HasStatics", "HasAbstract"}) == 1);
        CHECK(std::ranges::count(entity_it->second, vector<string> {"CoverageTarget"}) == 1);

        REQUIRE(holder_it != tags.end());
        CHECK(std::ranges::count(holder_it->second, vector<string> {"Server", "CoverageHolder", "CoverageTarget", "Items", "Persistent"}) == 1);
    }

    SECTION("serializes off-target entity holder stubs")
    {
        rig.AddSourceFile("Scripts/TestOffTargetMetadataStubs.fos", R"(
namespace TestOffTargetMetadataStubs
{
///@ Entity Client ClientHeld
///@ FixedType Client ClientFixed
///@ ValueType Client ClientSnapshot Layout = int32 - Value
///@ EntityHolder Client Critter ClientHeld ClientEntries PublicSync
///@ Property ClientHeld Client int32 Ignored
///@ Property ClientFixed Client int32 IgnoredFixed
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-server"));

        auto tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-server"));
        auto holder_it = tags.find("EntityHolder");

        REQUIRE(holder_it != tags.end());
        CHECK(std::ranges::count(holder_it->second, vector<string> {"Stub", "Critter", "", "ClientEntries", "PublicSync"}) == 1);

        auto entity_it = tags.find("Entity");
        if (entity_it != tags.end()) {
            CHECK(std::ranges::none_of(entity_it->second, [](const vector<string>& entry) { return !entry.empty() && entry.front() == "ClientHeld"; }));
        }

        auto fixed_type_it = tags.find("FixedType");
        if (fixed_type_it != tags.end()) {
            CHECK(std::ranges::none_of(fixed_type_it->second, [](const vector<string>& entry) { return !entry.empty() && entry.front() == "ClientFixed"; }));
        }
    }

    SECTION("rejects invalid entity holder declarations")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ EntityHolder Server Holder Target", "insufficient parameters"},
            {"///@ EntityHolder Server MissingHolder MissingTarget Entry", "unknown holder entity type"},
            {R"(
///@ Entity Server Holder
///@ Entity Server Target
///@ EntityHolder Server Holder Target Entry OwnerSync
)",
                "sync flags cannot be used with Server target"},
            {R"(
///@ Entity Server Holder
///@ EntityHolder Server Holder MissingTarget Entry
)",
                "unknown target entity type"},
            {R"(
///@ Entity Common Holder
///@ Entity Common Target
///@ EntityHolder Common Holder Target Entry OwnerSync PublicSync
)",
                "sync flags invalid mixture"},
            {R"(
///@ Entity Common Holder
///@ Entity Common Target
///@ EntityHolder Common Holder Target Entry
)",
                "Common target requires sync flag"},
            {"///@ EntityHolder Server Critter Item Entries", "cannot hold exported entity type"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("rejects invalid fixed type declarations")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ FixedType Server", "insufficient parameters"},
            {"///@ FixedType Server CoverageFixed ExtraFlag", "flags are not supported"},
            {"///@ FixedType Server CoverageFixed\n///@ FixedType Server CoverageFixed", "duplicate fixed type"},
            {"///@ FixedType Server int32", "type name conflict with another type"},
            {"///@ Enum CoverageFixedProperty Entry\n///@ FixedType Server CoverageFixed", "property enum conflict with another type"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("reports empty and unknown codegen tags")
    {
        {
            TestRig local_rig;
            local_rig.AddSourceFile("Scripts/EmptyTag.fos", "///@ // comment only");

            MetadataBaker baker(local_rig.MakeContext());
            REQUIRE_THROWS_WITH(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("empty tag"));
        }
        {
            TestRig local_rig;
            local_rig.AddSourceFile("Scripts/UnknownTag.fos", "///@ Unknown Tag");

            MetadataBaker baker(local_rig.MakeContext());
            REQUIRE_THROWS_WITH(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("unknown tag name"));
        }
    }

    SECTION("aggregates async metadata errors when sync mode is disabled")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Scripts/BrokenAsync.fos", "///@ Unknown Tag");

        auto context = local_rig.MakeContext();
        context->ForceSyncMode = false;

        MetadataBaker baker(context);
        REQUIRE_THROWS_WITH(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("Errors during preparing of metadata"));
    }

    SECTION("resolves setting groups and serializes their configured values")
    {
        ConfigFile config {"Common.DebugBuild = true\nDebugFlag = false\n"};
        rig.Settings.ApplyConfigFile(config, "");
        rig.AddSourceFile("Scripts/TestSettings.fos", R"(
namespace TestSettings
{
#if CLIENT
///@ Setting Client bool DebugBuild
///@ Setting Client bool Common.DebugBuild
///@ Setting Client bool Common . DebugBuild
///@ Setting Client bool DebugFlag
#endif
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        const auto& output = rig.Outputs.at("TestPack.fometa-client");
        data_reader reader(output);
        skip_baked_header(reader);
        auto tag_count = reader.read<uint16_t>();
        auto read_string = [&reader](uint16_t size) -> string {
            string value;
            value.resize(size);
            reader.read_string_bytes(value);
            return value;
        };

        vector<vector<string>> settings_entries;

        for (uint16_t i = 0; i < tag_count; i++) {
            auto tag_name_len = reader.read<uint16_t>();
            string tag_name = read_string(tag_name_len);
            auto tag_value_count = reader.read<uint32_t>();

            for (uint32_t j = 0; j < tag_value_count; j++) {
                auto value_parts_count = reader.read<uint32_t>();
                vector<string> value_parts;
                value_parts.reserve(value_parts_count);

                for (uint32_t k = 0; k < value_parts_count; k++) {
                    auto part_len = reader.read<uint16_t>();
                    value_parts.emplace_back(read_string(part_len));
                }

                if (tag_name == "Setting") {
                    settings_entries.emplace_back(std::move(value_parts));
                }
            }
        }

        reader.verify_end();

        auto debug_build_value = rig.Settings.FindSettingValue("Common.DebugBuild");
        auto debug_flag_value = rig.Settings.FindSettingValue("DebugFlag");
        REQUIRE(debug_build_value);
        REQUIRE(debug_flag_value);
        CHECK(std::ranges::count(settings_entries, vector<string> {"Common.DebugBuild", "bool", *debug_build_value}) == 3);
        CHECK(std::ranges::count(settings_entries, vector<string> {"DebugFlag", "bool", *debug_flag_value}) == 1);
    }

    SECTION("rejects setting declarations without a configured value")
    {
        ExpectMetadataBakerError("///@ Setting Client bool Missing.Value", "setting has no configured value");
    }

    SECTION("serializes and applies initial game setting values")
    {
        ConfigFile bake_config {"Common.GameName = MetadataGame\n"};
        rig.Settings.ApplyConfigFile(bake_config, "");
        rig.Settings.SetCustomSetting("Daylight.SunriseMinute", any_t(string("540")));
        rig.AddSourceFile("Scripts/TestSettingValue.fos", "///@ Setting Common string Common.GameName\n///@ Setting Client int32 Daylight.SunriseMinute");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        const auto& output = rig.Outputs.at("TestPack.fometa-client");
        auto tags = read_baked_tags(output);
        auto setting_it = tags.find("Setting");

        REQUIRE(setting_it != tags.end());
        CHECK(std::ranges::count(setting_it->second, vector<string> {"Common.GameName", "string", "MetadataGame"}) == 1);
        CHECK(std::ranges::count(setting_it->second, vector<string> {"Daylight.SunriseMinute", "int32", "540"}) == 1);

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ClientSide);
        REQUIRE_NOTHROW(RegisterDynamicMetadata(&meta, output));
        // Mirrors what BaseEngine does with the registered values
        auto apply_metadata_settings = [&](GlobalSettings& settings) {
            for (const auto& [name, value] : meta.GetGameSettingsInitialValues()) {
                if (!settings.FindSettingValue(name)) {
                    settings.SetSettingValue(name, value);
                }
            }
        };

        GlobalSettings unconfigured_runtime_settings {false};
        unconfigured_runtime_settings.ApplyDefaultSettings();
        apply_metadata_settings(unconfigured_runtime_settings);
        CHECK(unconfigured_runtime_settings.GameName == "MetadataGame");
        CHECK_FALSE(unconfigured_runtime_settings.FindCustomSetting("Common.GameName"));
        REQUIRE(unconfigured_runtime_settings.FindCustomSetting("Daylight.SunriseMinute"));
        CHECK(unconfigured_runtime_settings.GetCustomSetting("Daylight.SunriseMinute") == "540");

        // The applied configuration is the operator's explicit override, so a sub-config, local config or
        // command-line value survives the metadata baseline
        GlobalSettings overridden_runtime_settings {false};
        overridden_runtime_settings.ApplyDefaultSettings();
        ConfigFile runtime_config {"Common.GameName = LocalGame\n"};
        overridden_runtime_settings.ApplyConfigFile(runtime_config, "");
        overridden_runtime_settings.SetCustomSetting("Daylight.SunriseMinute", any_t(string("600")));
        apply_metadata_settings(overridden_runtime_settings);
        CHECK(overridden_runtime_settings.GameName == "LocalGame");
        CHECK(overridden_runtime_settings.GetCustomSetting("Daylight.SunriseMinute") == "600");

        auto missing_value_output = MakeMetadataBlob({{"Setting", {{"Invalid.WithoutValue", "bool"}}}});
        EngineMetadata invalid_meta {[] { }};
        invalid_meta.RegisterSide(EngineSideKind::ClientSide);
        REQUIRE_THROWS_WITH(RegisterDynamicMetadata(&invalid_meta, missing_value_output), Catch::Matchers::ContainsSubstring("Setting metadata record must contain setting name, value type, and initial value"));
    }

    SECTION("refuses a pack written in an older file layout")
    {
        // Layout 1 wrote the Setting record without its configured value, and a token layout change leaves the
        // metadata version untouched, so only the file layout version can stop such a pack from being read
        constexpr uint16_t settings_without_value_layout = 1;
        STATIC_REQUIRE(METADATA_FILE_VERSION > settings_without_value_layout);

        vector<uint8_t> outdated_metadata;
        auto writer = data_writer(outdated_metadata);
        writer.write<uint32_t>(METADATA_FILE_MAGIC);
        writer.write<uint16_t>(settings_without_value_layout);
        writer.write<uint16_t>(numeric_cast<uint16_t>(TEST_METADATA_VERSION.length()));
        writer.write_string_bytes(TEST_METADATA_VERSION);
        writer.write<uint16_t>(const_numeric_cast<uint16_t>(1));
        writer.write<uint16_t>(numeric_cast<uint16_t>(METADATA_SETTING_SECTION.length()));
        writer.write_string_bytes(METADATA_SETTING_SECTION);
        writer.write<uint32_t>(const_numeric_cast<uint32_t>(1));
        writer.write<uint32_t>(const_numeric_cast<uint32_t>(2));

        for (string_view token : {string_view("Common.GameName"), string_view("string")}) {
            writer.write<uint16_t>(numeric_cast<uint16_t>(token.length()));
            writer.write_string_bytes(token);
        }

        CHECK_THROWS_WITH(ReadMetadataVersion(outdated_metadata), Catch::Matchers::ContainsSubstring("resources must be rebaked"));

        EngineMetadata outdated_meta {[] { }};
        outdated_meta.RegisterSide(EngineSideKind::ClientSide);
        CHECK_THROWS_AS(RegisterDynamicMetadata(&outdated_meta, outdated_metadata), MetadataOutdatedException);
    }

    SECTION("serializes events and remote calls")
    {
        rig.AddSourceFile("Scripts/TestEventsAndRemoteCalls.fos", R"(
namespace TestEventsAndRemoteCalls
{
///@ Event Server Critter CoverageEvent ( )
///@ Event Server Critter CoverageEventArgs ( int32 amount , string? note )
///@ RemoteCall Server CoverageCall ( int32 amount , string? note ) MaxBytes 4096 MaxCollectionSize 32
///@ RemoteCall Client ClientCoverage ( )
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-server"));

        auto tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-server"));
        auto event_it = tags.find("Event");
        auto remote_call_it = tags.find("RemoteCall");

        REQUIRE(event_it != tags.end());
        CHECK(std::ranges::count(event_it->second, vector<string> {"Critter", "CoverageEvent"}) == 1);
        CHECK(std::ranges::count(event_it->second, vector<string> {"Critter", "CoverageEventArgs", "int32", "", "amount", "string", "?", "note"}) == 1);

        REQUIRE(remote_call_it != tags.end());
        CHECK(std::ranges::count(remote_call_it->second, vector<string> {"CoverageCall", "TestEventsAndRemoteCalls.fos", "In", "int32", "", "amount", "string", "?", "note", "Limits", "4096", "32"}) == 1);
        CHECK(std::ranges::count(remote_call_it->second, vector<string> {"ClientCoverage", "TestEventsAndRemoteCalls.fos", "Out", "Limits", "0", "0"}) == 1);
    }

    SECTION("rejects invalid event declarations")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ Event Server Critter CoverageEvent", "insufficient parameters"},
            {"///@ Event Server MissingEntity CoverageEvent ( )", "invalid entity type"},
            {"///@ Event Server Critter CoverageEvent ) trailing", "expected '(' after event name"},
            {"///@ Event Server Critter CoverageEvent ( MissingType value )", "cannot resolve arg type"},
            {"///@ Event Server Critter CoverageEvent ( int32", "expected arg name after it's type"},
            {"///@ Event Server Critter CoverageEvent ( int32 value bool next )", "expected ')' or ',' after arg"},
            {"///@ Event Server Critter CoverageEvent ( ) trailing", "unexpected tokens after ')'"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("rejects invalid remote call declarations")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ RemoteCall Server CoverageCall", "insufficient parameters"},
            {"///@ RemoteCall Mapper CoverageCall ( )", "expected 'Server' or 'Client' as target"},
            {"///@ RemoteCall Server CoverageCall ) trailing", "expected '(' after remote call name"},
            {"///@ RemoteCall Server CoverageCall ( MissingType value )", "cannot resolve arg type"},
            {"///@ RemoteCall Server CoverageCall ( int32", "expected arg name after it's type"},
            {"///@ RemoteCall Server CoverageCall ( int32 value bool next )", "expected ')' or ',' after arg"},
            {"///@ RemoteCall Server CoverageCall ( ) MaxBytes", "requires a non-negative integer value"},
            {"///@ RemoteCall Server CoverageCall ( ) MaxBytes -1", "must not be negative"},
            {"///@ RemoteCall Server CoverageCall ( ) UnknownLimit 1", "unknown structural limit"},
            {"///@ RemoteCall Server CoverageCall ( ) MaxBytes 1 MaxBytes 2", "duplicate MaxBytes"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("rejects invalid setting declarations")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ Setting Server bool", "insufficient parameters"},
            {"///@ Setting Mapper bool Coverage.Setting", "expected 'Common', 'Server' or 'Client' as target"},
            {"///@ Setting Server MissingType Coverage.Setting", "invalid type"},
            {"///@ Setting Server Critter Coverage.Setting", "type must be primitive or enum or string or any"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("rejects malformed migration rules")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ MigrationRule Property Item OnlyOneArg", "insufficient parameters"},
            {"///@ MigrationRule Property Item . Remove", "insufficient parameters"},
            {"///@ MigrationRule Property Item . Bad Remove", "malformed dotted name"},
            {"///@ MigrationRule Property Item Bad Name Remove", "too many rule arguments"},
            {"///@ MigrationRule Property Item Bad .", "malformed dotted name"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("serializes migration rules with dotted names")
    {
        rig.AddSourceFile("Scripts/TestMigration.fos", R"(
namespace TestMigration
{
///@ MigrationRule Property Item Weapon.AmmoPid Weapon.Ammo
///@ MigrationRule Proto Modifier LegacyAchvO9tCm0 Remove
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        const auto& output = rig.Outputs.at("TestPack.fometa-client");
        data_reader reader(output);
        skip_baked_header(reader);
        auto tag_count = reader.read<uint16_t>();
        auto read_string = [&reader](uint16_t size) -> string {
            string value;
            value.resize(size);
            reader.read_string_bytes(value);
            return value;
        };

        vector<vector<string>> migration_entries;
        vector<vector<string>> settings_entries;

        for (uint16_t i = 0; i < tag_count; i++) {
            auto tag_name_len = reader.read<uint16_t>();
            string tag_name = read_string(tag_name_len);
            auto tag_value_count = reader.read<uint32_t>();

            for (uint32_t j = 0; j < tag_value_count; j++) {
                auto value_parts_count = reader.read<uint32_t>();
                vector<string> value_parts;
                value_parts.reserve(value_parts_count);

                for (uint32_t k = 0; k < value_parts_count; k++) {
                    auto part_len = reader.read<uint16_t>();
                    value_parts.emplace_back(read_string(part_len));
                }

                if (tag_name == "MigrationRule") {
                    migration_entries.emplace_back(std::move(value_parts));
                }
                else if (tag_name == "Setting") {
                    settings_entries.emplace_back(std::move(value_parts));
                }
            }
        }

        reader.verify_end();

        CHECK(std::ranges::count(migration_entries, vector<string> {"Property", "Item", "Weapon.AmmoPid", "Weapon.Ammo"}) == 1);
        CHECK(std::ranges::count(migration_entries, vector<string> {"Proto", "Modifier", "LegacyAchvO9tCm0", "Remove"}) == 1);
        CHECK(settings_entries.empty());

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ClientSide);
        REQUIRE_NOTHROW(RegisterDynamicMetadata(&meta, output));

        auto property_rule = meta.CheckMigrationRule(meta.Hashes.to_hashed_string("Property"), meta.Hashes.to_hashed_string("Item"), meta.Hashes.to_hashed_string("Weapon.AmmoPid"));
        REQUIRE(property_rule.has_value());
        CHECK(property_rule.value() == meta.Hashes.to_hashed_string("Weapon.Ammo"));

        auto proto_rule = meta.CheckMigrationRule(meta.Hashes.to_hashed_string("Proto"), meta.Hashes.to_hashed_string("Modifier"), meta.Hashes.to_hashed_string("LegacyAchvO9tCm0"));
        REQUIRE(proto_rule.has_value());
        CHECK(proto_rule.value() == meta.Hashes.to_hashed_string("Remove"));
    }

    SECTION("serializes ref type fields declared via property tags")
    {
        rig.AddSourceFile("Scripts/TestRefTypeProps.fos", R"(
namespace TestRefTypeProps
{
///@ RefType Common RouteSnapshot
///@ Property RouteSnapshot Common int32[] Steps
///@ Property RouteSnapshot Common hstring[] Tags
///@ Property RouteSnapshot Common string Note
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        const auto& output = rig.Outputs.at("TestPack.fometa-client");
        auto tags = read_baked_tags(output);
        auto ref_type_it = tags.find("RefType");

        REQUIRE(ref_type_it != tags.end());
        CHECK(std::ranges::count(ref_type_it->second, vector<string> {"RouteSnapshot", "Steps", "int32[]", "0", "Tags", "hstring[]", "0", "Note", "string", "0"}) == 1);

        auto property_it = tags.find("Property");
        if (property_it != tags.end()) {
            CHECK(std::ranges::none_of(property_it->second, [](const auto& entry) { return !entry.empty() && entry.front() == "RouteSnapshot"; }));
        }

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ClientSide);
        REQUIRE_NOTHROW(RegisterDynamicMetadata(&meta, output));

        const auto& route_snapshot_type = meta.GetBaseType("RouteSnapshot");
        REQUIRE(route_snapshot_type.IsRefType);
        REQUIRE(route_snapshot_type.RefType != nullptr);
        REQUIRE(route_snapshot_type.RefType->FieldsRegistrar != nullptr);
        auto steps_prop = route_snapshot_type.RefType->FieldsRegistrar->FindProperty("Steps");
        auto tags_prop = route_snapshot_type.RefType->FieldsRegistrar->FindProperty("Tags");
        auto note_prop = route_snapshot_type.RefType->FieldsRegistrar->FindProperty("Note");
        REQUIRE(static_cast<bool>(steps_prop));
        REQUIRE(static_cast<bool>(tags_prop));
        REQUIRE(static_cast<bool>(note_prop));
        CHECK(steps_prop->GetViewTypeName() == "int32[]");
        CHECK(tags_prop->GetViewTypeName() == "hstring[]");
        CHECK(note_prop->GetViewTypeName() == "string");
    }

    SECTION("registers entity properties that use baked ref types")
    {
        rig.AddSourceFile("Scripts/TestRefTypeEntityProps.fos", R"(
namespace TestRefTypeEntityProps
{
///@ RefType Common RouteSnapshot
///@ Property RouteSnapshot Common int32[] Steps
///@ Property RouteSnapshot Common string Note
///@ Property Critter Server RouteSnapshot Snapshot Mutable Persistent
///@ Property Critter Server RouteSnapshot[] Snapshots Mutable Persistent
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-server"));

        const auto& output = rig.Outputs.at("TestPack.fometa-server");
        auto tags = read_baked_tags(output);
        auto property_it = tags.find("Property");

        REQUIRE(property_it != tags.end());
        CHECK(std::ranges::count(property_it->second, vector<string> {"Critter", "Server", "RouteSnapshot", "Snapshot", "Mutable", "Persistent"}) == 1);
        CHECK(std::ranges::count(property_it->second, vector<string> {"Critter", "Server", "RouteSnapshot[]", "Snapshots", "Mutable", "Persistent"}) == 1);

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ServerSide);
        meta.RegisterEntityType("Critter", true, false, true, true, true);
        meta.RegisterEnumGroup("CritterProperty", "uint16", {{"None", 0}});
        REQUIRE_NOTHROW(RegisterDynamicMetadata(&meta, output));

        auto critter_registrar = meta.GetPropertyRegistrar("Critter");
        REQUIRE(static_cast<bool>(critter_registrar));

        auto snapshot_prop = critter_registrar->FindProperty("Snapshot");
        auto snapshots_prop = critter_registrar->FindProperty("Snapshots");

        REQUIRE(static_cast<bool>(snapshot_prop));
        REQUIRE(static_cast<bool>(snapshots_prop));
        CHECK(snapshot_prop->GetViewTypeName() == "RouteSnapshot");
        CHECK(snapshot_prop->IsBaseTypeRefType());
        CHECK_FALSE(snapshot_prop->IsArray());
        CHECK(snapshots_prop->GetViewTypeName() == "RouteSnapshot[]");
        CHECK(snapshots_prop->IsBaseTypeRefType());
        CHECK(snapshots_prop->IsArray());
        CHECK(static_cast<bool>(meta.GetBaseType("RouteSnapshot").RefType->FieldsRegistrar->FindProperty("Steps")));
        CHECK(static_cast<bool>(meta.GetBaseType("RouteSnapshot").RefType->FieldsRegistrar->FindProperty("Note")));
    }

    SECTION("serializes entity component properties")
    {
        rig.AddSourceFile("Scripts/TestEntityComponentProps.fos", R"(
namespace TestEntityComponentProps
{
///@ Property Critter Server bool Marker Component
///@ Property Critter Server int32 Marker.Step
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-server"));

        auto tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-server"));
        auto property_it = tags.find("Property");

        REQUIRE(property_it != tags.end());
        CHECK(std::ranges::count(property_it->second, vector<string> {"Critter", "Server", "bool", "Marker", "Component"}) == 1);
        CHECK(std::ranges::count(property_it->second, vector<string> {"Critter", "Server", "int32", "Marker.Step"}) == 1);
    }

    SECTION("rejects invalid entity properties")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ Property Critter Server int32", "insufficient parameters"},
            {"///@ Property MissingEntity Server int32 Value", "unknown entity type"},
            {"///@ Property Critter Mapper int32 Value", "invalid target"},
            {"///@ Property Critter Server MissingType Value", "cannot resolve property type"},
            {"///@ Property Critter Server int32 &", "property type can't be ref"},
            {"///@ Property Critter Server int32 & Value", "property type can't be ref"},
            {"///@ Property Critter Server int32 Marker Component", "component property must be plain bool"},
            {"///@ Property Critter Server bool Marker Component\n///@ Property Critter Server bool Marker Component", "duplicate component"},
            {"///@ Property Critter Server int32 Missing.Step", "unknown component for property"},
            {"///@ Property Critter Server bool Marker Component\n///@ Property Critter Server int32 Other.Step", "unknown component for property"},
            {"///@ Property Critter Server int32 Marker.Step Component", "component property cannot be nested"},
            {"///@ Property Critter Server bool Marker Component\n///@ Property Critter Client int32 Marker.Step", "property target is incompatible with component target"},
            {"///@ Property Critter Server int32 Value\n///@ Property Critter Server int32 Value", "duplicate property"},
            {"///@ Property Critter Common int32 Value Mutable", "common mutable property must have sync type"},
            {"///@ Property Critter Common int32 Value Mutable OwnerSync PublicSync", "sync flags invalid mixture"},
            {"///@ Property Critter Common int32 Value Mutable NoSync ModifiableByClient", "modifiable property must be synced"},
            {"///@ Property Critter Common int32 Value Mutable OwnerSync ModifiableByAnyClient", "modifiable by any property must be public synced"},
            {"///@ Property Critter Server int32 Value Persistent", "persistent property must be mutable"},
            {"///@ Property Critter Common int32 Value Mutable Virtual OwnerSync", "synced property can't be virtual"},
            {"///@ Property Critter Server int32 Value Mutable Virtual Persistent", "virtual property can't be persistent"},
            {"///@ Property Critter Server int32 Value NullGetterForProto", "null getter can only be on virtual property"},
            {"///@ Property Critter Server int32 Value Nullable", "Nullable can only be used on FixedType or Proto entity properties"},
            {"///@ Property Critter Client int32 Value Mutable Persistent", "persistent property can't be client only"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("registers ref types in declaration order")
    {
        rig.AddSourceFile("Scripts/TestNestedRefTypeProps.fos", R"(
namespace TestNestedRefTypeProps
{
///@ RefType Common Alpha
///@ Property Alpha Common Beta Dependency
///@ RefType Common Beta
///@ Property Beta Common int32 Value
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        const auto& output = rig.Outputs.at("TestPack.fometa-client");
        auto tags = read_baked_tags(output);
        auto ref_type_it = tags.find("RefType");

        REQUIRE(ref_type_it != tags.end());
        REQUIRE(ref_type_it->second.size() == 2);
        CHECK(ref_type_it->second[0].front() == "Alpha");
        CHECK(ref_type_it->second[1].front() == "Beta");

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ClientSide);
        REQUIRE_NOTHROW(RegisterDynamicMetadata(&meta, output));

        const auto& alpha_type = meta.GetBaseType("Alpha");
        const auto& beta_type = meta.GetBaseType("Beta");
        REQUIRE(alpha_type.IsRefType);
        REQUIRE(beta_type.IsRefType);
        REQUIRE(alpha_type.RefType != nullptr);
        REQUIRE(beta_type.RefType != nullptr);
        REQUIRE(alpha_type.RefType->FieldsRegistrar != nullptr);
        REQUIRE(beta_type.RefType->FieldsRegistrar != nullptr);

        auto dependency_prop = alpha_type.RefType->FieldsRegistrar->FindProperty("Dependency");
        REQUIRE(static_cast<bool>(dependency_prop));
        CHECK(dependency_prop->GetViewTypeName() == "Beta");
        CHECK(dependency_prop->IsBaseTypeRefType());
        CHECK(static_cast<bool>(beta_type.RefType->FieldsRegistrar->FindProperty("Value")));
    }

    SECTION("rejects legacy ref type layout syntax")
    {
        rig.AddSourceFile("Scripts/TestLegacyRefType.fos", R"(
namespace TestLegacyRefType
{
///@ RefType Common LegacyRefType Layout = int32-Value
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("Layout is no longer supported"));
    }

    SECTION("rejects invalid ref type declarations")
    {
        vector<pair<string_view, string_view>> cases = {
            {"///@ RefType Server", "insufficient parameters"},
            {"///@ RefType Mapper BadRef", "invalid target"},
            {"///@ RefType Server BadRef Extra", "unexpected parameters"},
            {R"(
///@ RefType Server BadRef
///@ Property BadRef Server int32 Value
///@ RefType Server BadRef
)",
                "duplicate type name"},
            {"///@ RefType Server EmptyRef", "no fields declared via Property"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("rejects invalid ref type field declarations")
    {
        vector<pair<string_view, string_view>> cases = {
            {R"(
///@ RefType Server BadRef
///@ Property BadRef Client int32 Value
)",
                "RefType field target must match RefType target"},
            {R"(
///@ RefType Server BadRef
///@ Property BadRef Server MissingType Value
)",
                "cannot resolve property type"},
            {R"(
///@ RefType Server BadRef
///@ Property BadRef Server int32 & Value
)",
                "RefType field type can't be ref"},
            {R"(
///@ RefType Server BadRef
///@ Property BadRef Server callback ( void ) Value
)",
                "RefType callback fields are unsupported"},
            {R"(
///@ RefType Server BadRef
///@ Property BadRef Server Critter Target
)",
                "RefType entity fields must be fixed or proto references"},
            {R"(
///@ RefType Server BadRef
///@ Property BadRef Server Critter = > int32 Mapping
)",
                "RefType entity dict keys are unsupported"},
            {R"(
///@ RefType Server BadRef
///@ Property BadRef Server bool Marker Component
///@ Property BadRef Server int32 Marker.Step Component
)",
                "component property cannot be nested"},
            {R"(
///@ RefType Server BadRef
///@ Property BadRef Server int32 Value
///@ Property BadRef Server bool Value
)",
                "duplicate RefType field name"},
        };

        for (const auto& [script, message] : cases) {
            ExpectMetadataBakerError(script, message);
        }
    }

    SECTION("rejects ref type property flags")
    {
        rig.AddSourceFile("Scripts/TestRefTypeFlags.fos", R"(
namespace TestRefTypeFlags
{
///@ RefType Common RefTypeWithFlags
///@ Property RefTypeWithFlags Common int32 Value Persistent
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("RefType fields only allow the 'Component' flag"));
    }

    SECTION("accepts ref type component marker and component sub-properties")
    {
        rig.AddSourceFile("Scripts/TestRefTypeComponent.fos", R"(
namespace TestRefTypeComponent
{
///@ RefType Server RouteSnapshot
///@ Property RouteSnapshot Server bool Marker Component
///@ Property RouteSnapshot Server int32 Marker.Steps
///@ Property RouteSnapshot Server string Marker.Note
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        REQUIRE(rig.Outputs.contains("TestPack.fometa-server"));

        const auto& output = rig.Outputs.at("TestPack.fometa-server");
        auto tags = read_baked_tags(output);
        auto ref_type_it = tags.find("RefType");

        REQUIRE(ref_type_it != tags.end());
        // Encoded as: name, "<field> <type> <flag-count> <flag*>" repeating
        CHECK(std::ranges::count(ref_type_it->second, vector<string> {"RouteSnapshot", "Marker", "bool", "1", "Component", "Marker.Steps", "int32", "0", "Marker.Note", "string", "0"}) == 1);
    }

    SECTION("rejects ref type component sub-property without declared component")
    {
        rig.AddSourceFile("Scripts/TestRefTypeComponentMissing.fos", R"(
namespace TestRefTypeComponentMissing
{
///@ RefType Server RouteSnapshot
///@ Property RouteSnapshot Server int32 Missing.Steps
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("RefType component is not declared"));
    }

    SECTION("rejects property owners that are not ref types")
    {
        rig.AddSourceFile("Scripts/TestValueTypePropertyOwner.fos", R"(
namespace TestValueTypePropertyOwner
{
///@ ValueType Common PlainSnapshot Layout = int32-Value
///@ Property PlainSnapshot Common int32 AnotherValue
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("only RefType supports script metadata properties"));
    }
#endif
}

FO_END_NAMESPACE
