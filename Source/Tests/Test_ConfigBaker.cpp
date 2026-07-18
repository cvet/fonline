//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "Test_BakerHelpers.h"

#if FO_ANGELSCRIPT_SCRIPTING
#include "ConfigBaker.h"
#include "ConfigFile.h"
#include "MetadataRegistration.h"
#endif

FO_BEGIN_NAMESPACE

#if FO_ANGELSCRIPT_SCRIPTING
static auto MakeConfigBakerTempDir() -> string
{
    return fs_path_to_string(std::filesystem::temp_directory_path() / std::format("lf_config_baker_{}", std::chrono::steady_clock::now().time_since_epoch().count()));
}

static void AddConfigBakerMetadata(BakerTests::TestRig& rig)
{
    auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
    rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
    rig.AddBakedFile("Metadata.fometa-client", metadata_blob);
}

static auto MakeConfigBakerMetadataBlob(const vector<pair<string_view, vector<vector<string_view>>>>& sections) -> vector<uint8_t>
{
    vector<uint8_t> metadata;
    auto writer = DataWriter(metadata);

    writer.Write<uint16_t>(numeric_cast<uint16_t>(sections.size()));

    for (const auto& [section_name, entries] : sections) {
        writer.Write<uint16_t>(numeric_cast<uint16_t>(section_name.length()));
        writer.WriteStringBytes(section_name);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(entries.size()));

        for (const auto& tokens : entries) {
            writer.Write<uint32_t>(numeric_cast<uint32_t>(tokens.size()));

            for (string_view token : tokens) {
                writer.Write<uint16_t>(numeric_cast<uint16_t>(token.length()));
                writer.WriteStringBytes(token);
            }
        }
    }

    return metadata;
}

template<typename T>
static auto FormatConfigBakerScalarValue(const T& value) -> string
{
    return strex("{}", value);
}

template<typename T>
static auto FormatConfigBakerSettingValue(const vector<T>& value) -> string
{
    string result;

    for (T entry : value) {
        if (!result.empty()) {
            result += " ";
        }

        result += FormatConfigBakerScalarValue(entry);
    }

    return result;
}

template<typename T>
static auto FormatConfigBakerSettingValue(const T& value) -> string
{
    return FormatConfigBakerScalarValue(value);
}

template<typename T>
static void AppendConfigBakerSetting(string& config, string_view name, const T& value)
{
    config += strex("{} = {}\n", name, FormatConfigBakerSettingValue(value));
}

static auto MakeCompleteConfigBakerConfig() -> string
{
    string config;

#define FIXED_SETTING(type, group, name, ...) AppendConfigBakerSetting(config, #group "." #name, type {__VA_ARGS__})
#define VARIABLE_SETTING(type, group, name, ...) AppendConfigBakerSetting(config, #group "." #name, type {__VA_ARGS__})
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings.inc"
#undef FIXED_SETTING
#undef VARIABLE_SETTING
#undef SETTING_GROUP
#undef SETTING_GROUP_END

    return config;
}
#endif

TEST_CASE("ConfigBaker")
{
#if FO_ANGELSCRIPT_SCRIPTING
    using namespace BakerTests;

    SECTION("SkipsUnsupportedTargetWithoutTouchingMetadata")
    {
        TestRig rig;
        ConfigBaker baker(rig.MakeContext());

        CHECK_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipAllConfigs")
    {
        TestRig rig;
        ConfigFile config {"Skip.fomain",
            "[SubConfig]\n"
            "Name = Child\n"
            "Common.GameName = ChildGame\n"};
        rig.Settings.ApplyConfigFile(config, "");

        vector<pair<string, uint64_t>> checks;
        ConfigBaker baker(rig.MakeContext("ConfigPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        }));
        CHECK_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        CHECK(rig.Outputs.empty());
        REQUIRE(checks.size() == 4);
        CHECK(checks[0].first == "(Root).fomain-server");
        CHECK(checks[1].first == "(Root).fomain-client");
        CHECK(checks[2].first == "Child.fomain-server");
        CHECK(checks[3].first == "Child.fomain-client");
        CHECK(std::ranges::all_of(checks, [](const auto& check) { return check.second == std::numeric_limits<uint64_t>::max(); }));
    }

    SECTION("BakesCompleteRootConfig")
    {
        string temp_dir = MakeConfigBakerTempDir();
        REQUIRE(std::filesystem::create_directories(temp_dir));
        string config_path = strex(temp_dir).combine_path("Test.fomain");
        REQUIRE(fs_write_file(config_path, MakeCompleteConfigBakerConfig()));

        TestRig rig;
        AddConfigBakerMetadata(rig);
        rig.Settings.ApplyConfigAtPath("Test.fomain", temp_dir);

        ConfigBaker baker(rig.MakeContext("ConfigPack"));
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        REQUIRE(rig.Outputs.contains("(Root).fomain-server"));
        REQUIRE(rig.Outputs.contains("(Root).fomain-client"));
        string server_config = rig.GetOutputText("(Root).fomain-server");
        string client_config = rig.GetOutputText("(Root).fomain-client");
        CHECK(server_config.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(client_config.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(server_config.find("ServerNetwork.ClientPingTime=10000\n") != string::npos);
        CHECK(client_config.find("ServerNetwork.ClientPingTime=") == string::npos);
        CHECK(client_config.find("ClientNetwork.ServerHost=127.0.0.1\n") != string::npos);
        CHECK(server_config.find("Common.AsyncLogWrite=") == string::npos);

        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    SECTION("BakesDynamicMetadataSettings")
    {
        string temp_dir = MakeConfigBakerTempDir();
        REQUIRE(std::filesystem::create_directories(temp_dir));
        string config_path = strex(temp_dir).combine_path("Test.fomain");
        REQUIRE(fs_write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Server.CustomEnabled = true\n"
                "Client.CustomTitle = Frontend\n"
                "Unknown.CustomSetting = Visible\n"));

        TestRig rig;
        rig.AddBakedFile("Metadata.fometa-server", MakeConfigBakerMetadataBlob({{"Setting", {{"Server.CustomEnabled", "bool"}}}}));
        rig.AddBakedFile("Metadata.fometa-client", MakeConfigBakerMetadataBlob({{"Setting", {{"Client.CustomTitle", "string"}}}}));
        rig.Settings.ApplyConfigAtPath("Test.fomain", temp_dir);

        ConfigBaker baker(rig.MakeContext("ConfigPack"));
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string server_config = rig.GetOutputText("(Root).fomain-server");
        string client_config = rig.GetOutputText("(Root).fomain-client");
        CHECK(server_config.find("Server.CustomEnabled=1\n") != string::npos);
        CHECK(server_config.find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(server_config.find("Unknown.CustomSetting=Visible\n") != string::npos);
        CHECK(client_config.find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(client_config.find("Server.CustomEnabled=") == string::npos);
        CHECK(client_config.find("Unknown.CustomSetting=") == string::npos);

        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    SECTION("BakingRequiresSingleAppliedRootConfig")
    {
        TestRig rig;
        AddConfigBakerMetadata(rig);

        ConfigBaker baker(rig.MakeContext());
        CHECK_THROWS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
    }

    SECTION("BakingRequiresBakedMetadata")
    {
        auto temp_dir = std::filesystem::temp_directory_path() / "lf-configbaker-test";
        std::filesystem::create_directories(temp_dir);
        auto fomain_path = temp_dir / "Test.fomain";
        fs_write_file(fs_path_to_string(fomain_path), "GameName = Test\n");

        TestRig rig;
        rig.Settings.ApplyConfigAtPath("Test.fomain", fs_path_to_string(temp_dir));

        ConfigBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""), MetadataNotFoundException);

        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    SECTION("BakingReportsIncompleteConfigSettings")
    {
        string temp_dir = MakeConfigBakerTempDir();
        REQUIRE(std::filesystem::create_directories(temp_dir));
        string config_path = strex(temp_dir).combine_path("Test.fomain");
        REQUIRE(fs_write_file(config_path,
            "Common.GameName = RootGame\n"
            "[SubConfig]\n"
            "Name = Child\n"
            "Common.GameName = ChildGame\n"));

        TestRig rig;
        AddConfigBakerMetadata(rig);
        rig.Settings.ApplyConfigAtPath("Test.fomain", temp_dir);

        ConfigBaker baker(rig.MakeContext("ConfigPack", [](string_view path, uint64_t) { return path == "Child.fomain-server"; }));
        CHECK_THROWS_AS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""), ConfigBakerException);

        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    SECTION("SetupBakersReturnsRequestedBaker")
    {
        TestRig rig;
        auto bakers = MakeRequestedBakers({string(ConfigBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == ConfigBaker::NAME);
        CHECK(bakers.front()->GetOrder() == 2);
    }
#endif
}

FO_END_NAMESPACE
