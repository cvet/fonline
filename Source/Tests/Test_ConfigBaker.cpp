// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "Test_BakerHelpers.h"

#if FO_ANGELSCRIPT_SCRIPTING || FO_MANAGED_SCRIPTING
#include "ConfigBaker.h"
#include "ConfigFile.h"
#include "MetadataBaker.h"
#include "MetadataRegistration.h"
#endif

FO_BEGIN_NAMESPACE

#if FO_ANGELSCRIPT_SCRIPTING || FO_MANAGED_SCRIPTING
static auto MakeConfigBakerTempDir() -> string
{
    return fs::path_to_string(std::filesystem::temp_directory_path() / std::format("lf_config_baker_{}", std::chrono::steady_clock::now().time_since_epoch().count()));
}

static void AddConfigBakerMetadata(BakerTests::TestRig& rig)
{
    auto metadata_blob {BakerTests::MakeEmptyMetadataBlob()};
    rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
    rig.AddBakedFile("Metadata.fometa-client", metadata_blob);
}

static auto MakeConfigBakerConfigWithGameSetting(string_view name, string_view value) -> string
{
    string config {BakerTests::MakeCompleteConfigBakerConfig()};
    config += strex("{} = {}\n", name, value);

    return config;
}
#endif

TEST_CASE("BakerRootConfigUsesZeroBase")
{
#if FO_ANGELSCRIPT_SCRIPTING || FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    string directory {MakeConfigBakerTempDir()};
    auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(directory); })};
    REQUIRE(fs::create_directories(directory));
    REQUIRE(fs::write_file(strex(directory).combine_path("Root.fomain").str(), "Render.Sleep=7\n"));

    SECTION("FreshSettings")
    {
        GlobalSettings settings {true};
        settings.SetupConfigAtPath("Root.fomain", directory);
        CHECK(settings.Render.Sleep == 7);
        CHECK_FALSE(settings.Client.ClientPropertiesPackData);
    }

    SECTION("BakerFixture")
    {
        TestRig rig {"Root.fomain", directory};
        CHECK(rig.Settings.Render.Sleep == 7);
        CHECK_FALSE(rig.Settings.Client.ClientPropertiesPackData);
    }
#endif
}

TEST_CASE("ConfigBaker")
{
#if FO_ANGELSCRIPT_SCRIPTING || FO_MANAGED_SCRIPTING
    using namespace BakerTests;

    SECTION("RejectsOutOfRangeGameInteger")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); })};
        REQUIRE(fs::create_directories(temp_dir));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("Test.fomain").str(), MakeCompleteConfigBakerConfig() + "Game.Small=256\n"));
        TestRig rig {"Test.fomain", temp_dir};
        auto metadata {MakeMetadataBlob({{"Setting", {{"Game.Small", "uint8"}}}})};
        rig.AddBakedFile("Metadata.fometa-server", metadata);
        rig.AddBakedFile("Metadata.fometa-client", metadata);
        ConfigBaker baker {rig.MakeContext()};

        CHECK_THROWS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
    }

    SECTION("RejectsUnresolvedGameEnum")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); })};
        REQUIRE(fs::create_directories(temp_dir));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("Test.fomain").str(), MakeCompleteConfigBakerConfig() + "Game.Mode=Missing\n"));
        TestRig rig {"Test.fomain", temp_dir};
        auto metadata {MakeMetadataBlob({{"Enum", {{"ProbeMode", "uint8", "Zero", "0", "One", "1"}}}, {"Setting", {{"Game.Mode", "ProbeMode"}}}})};
        rig.AddBakedFile("Metadata.fometa-server", metadata);
        rig.AddBakedFile("Metadata.fometa-client", metadata);
        ConfigBaker baker {rig.MakeContext()};

        CHECK_THROWS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
    }

    SECTION("RejectsInvalidGameIntegerLiterals")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); })};
        REQUIRE(fs::create_directories(temp_dir));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};

        vector<pair<string_view, string_view>> invalid_values {
            {"uint8", "-1"},
            {"uint8", "-0x1"},
            {"uint8", "0x100"},
            {"int8", "128"},
            {"int8", "0x80"},
            {"int8", "-129"},
            {"int8", "-0x81"},
            {"int64", "9223372036854775808"},
            {"int64", "-9223372036854775809"},
            {"uint64", "18446744073709551616"},
            {"uint64", "0x10000000000000000"},
            {"int32", "1.0"},
            {"int32", "0.0"},
            {"int32", "1e2"},
            {"int32", "0e0"},
            {"int32", "12x"},
            {"int32", "0x"},
        };

        for (const auto& [type_name, literal] : invalid_values) {
            REQUIRE(fs::write_file(config_path, MakeConfigBakerConfigWithGameSetting("Game.Small", literal)));
            TestRig rig {"Test.fomain", temp_dir};
            auto metadata {MakeMetadataBlob({{"Setting", {{"Game.Small", type_name}}}})};
            rig.AddBakedFile("Metadata.fometa-server", metadata);
            rig.AddBakedFile("Metadata.fometa-client", metadata);
            ConfigBaker baker {rig.MakeContext("ConfigPack")};

            CHECK_THROWS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
        }
    }

    SECTION("WritesValidGameIntegerBoundariesAndHex")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); })};
        REQUIRE(fs::create_directories(temp_dir));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Game.MinInt8 = -128\n"
                "Game.MaxInt8 = 127\n"
                "Game.MaxUInt8 = 255\n"
                "Game.MinInt64 = -9223372036854775808\n"
                "Game.MaxInt64 = 9223372036854775807\n"
                "Game.MaxUInt64 = 18446744073709551615\n"
                "Game.HexUInt8 = 0xFF\n"
                "Game.HexUInt64 = 0xFFFFFFFFFFFFFFFF\n"
                "Game.HexInt8Min = -0x80\n"
                "Game.HexInt16 = 0x7FFF\n"
                "Game.ZeroDecimal = 0\n"
                "Game.ZeroHex = 0X0\n"
                "Game.ZeroNegative = -0\n"));

        TestRig rig {"Test.fomain", temp_dir};
        auto metadata {MakeMetadataBlob({{"Setting", {{"Game.MinInt8", "int8"}, {"Game.MaxInt8", "int8"}, {"Game.MaxUInt8", "uint8"}, {"Game.MinInt64", "int64"}, {"Game.MaxInt64", "int64"}, {"Game.MaxUInt64", "uint64"}, {"Game.HexUInt8", "uint8"}, {"Game.HexUInt64", "uint64"}, {"Game.HexInt8Min", "int8"}, {"Game.HexInt16", "int16"}, {"Game.ZeroDecimal", "int32"}, {"Game.ZeroHex", "uint64"}, {"Game.ZeroNegative", "int32"}}}})};
        rig.AddBakedFile("Metadata.fometa-server", metadata);
        rig.AddBakedFile("Metadata.fometa-client", metadata);

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string server_config {rig.GetOutputText("(Root).fomain-server")};
        string client_config {rig.GetOutputText("(Root).fomain-client")};

        CHECK(server_config.find("Game.MinInt8=-128\n") != string::npos);
        CHECK(server_config.find("Game.MaxInt8=127\n") != string::npos);
        CHECK(server_config.find("Game.MaxUInt8=255\n") != string::npos);
        CHECK(server_config.find("Game.MinInt64=-9223372036854775808\n") != string::npos);
        CHECK(server_config.find("Game.MaxInt64=9223372036854775807\n") != string::npos);
        CHECK(server_config.find("Game.MaxUInt64=18446744073709551615\n") != string::npos);
        CHECK(server_config.find("Game.HexUInt8=0xFF\n") != string::npos);
        CHECK(server_config.find("Game.HexUInt64=0xFFFFFFFFFFFFFFFF\n") != string::npos);
        CHECK(server_config.find("Game.HexInt8Min=-0x80\n") != string::npos);
        CHECK(server_config.find("Game.HexInt16=0x7FFF\n") != string::npos);
        CHECK(client_config.find("Game.MaxUInt64=18446744073709551615\n") != string::npos);
        CHECK(client_config.find("Game.HexUInt8=0xFF\n") != string::npos);

        CHECK(server_config.find("Game.ZeroDecimal=") == string::npos);
        CHECK(server_config.find("Game.ZeroHex=") == string::npos);
        CHECK(server_config.find("Game.ZeroNegative=") == string::npos);
        CHECK(client_config.find("Game.ZeroDecimal=") == string::npos);
        CHECK(client_config.find("Game.ZeroHex=") == string::npos);
    }

    SECTION("WritesValidGameEnumNamesAndNumericValuesAndOmitsTypedZeros")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); })};
        REQUIRE(fs::create_directories(temp_dir));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Game.BareZero = Zero\n"
                "Game.QualifiedZero = ProbeMode::Zero\n"
                "Game.BareOne = One\n"
                "Game.QualifiedMax = ProbeMode::Max\n"
                "Game.NumericUndeclared = 200\n"
                "Game.NumericHex = 0x10\n"
                "Game.NumericDecimalZero = 0\n"
                "Game.NumericZero = 0x0\n"
                "Game.SignedMin = Min\n"
                "Game.SignedQualifiedMax = ProbeSigned::Max\n"
                "Game.SignedNumeric = -1\n"));

        TestRig rig {"Test.fomain", temp_dir};
        auto metadata {MakeMetadataBlob({
            {"Enum", {{"ProbeMode", "uint8", "Zero", "0", "One", "1", "Max", "255"}, {"ProbeSigned", "int8", "Zero", "0", "Min", "-128", "Max", "127"}}},
            {"Setting", {{"Game.BareZero", "ProbeMode"}, {"Game.QualifiedZero", "ProbeMode"}, {"Game.BareOne", "ProbeMode"}, {"Game.QualifiedMax", "ProbeMode"}, {"Game.NumericUndeclared", "ProbeMode"}, {"Game.NumericHex", "ProbeMode"}, {"Game.NumericDecimalZero", "ProbeMode"}, {"Game.NumericZero", "ProbeMode"}, {"Game.SignedMin", "ProbeSigned"}, {"Game.SignedQualifiedMax", "ProbeSigned"}, {"Game.SignedNumeric", "ProbeSigned"}}},
        })};
        rig.AddBakedFile("Metadata.fometa-server", metadata);
        rig.AddBakedFile("Metadata.fometa-client", metadata);

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string server_config {rig.GetOutputText("(Root).fomain-server")};
        string client_config {rig.GetOutputText("(Root).fomain-client")};

        CHECK(server_config.find("Game.BareOne=One\n") != string::npos);
        CHECK(server_config.find("Game.QualifiedMax=ProbeMode::Max\n") != string::npos);
        CHECK(server_config.find("Game.NumericUndeclared=200\n") != string::npos);
        CHECK(server_config.find("Game.NumericHex=0x10\n") != string::npos);
        CHECK(server_config.find("Game.SignedMin=Min\n") != string::npos);
        CHECK(server_config.find("Game.SignedQualifiedMax=ProbeSigned::Max\n") != string::npos);
        CHECK(server_config.find("Game.SignedNumeric=-1\n") != string::npos);
        CHECK(client_config.find("Game.NumericUndeclared=200\n") != string::npos);

        CHECK(server_config.find("Game.BareZero=") == string::npos);
        CHECK(server_config.find("Game.QualifiedZero=") == string::npos);
        CHECK(server_config.find("Game.NumericDecimalZero=") == string::npos);
        CHECK(server_config.find("Game.NumericZero=") == string::npos);
        CHECK(client_config.find("Game.QualifiedZero=") == string::npos);
        CHECK(client_config.find("Game.NumericDecimalZero=") == string::npos);
        CHECK(client_config.find("Game.NumericZero=") == string::npos);
    }

    SECTION("RejectsUnresolvedGameEnumNames")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); })};
        REQUIRE(fs::create_directories(temp_dir));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};

        vector<string_view> unresolved_values {"Missing", "ProbeMode::Missing", "OtherMode::One"};

        for (string_view literal : unresolved_values) {
            REQUIRE(fs::write_file(config_path, MakeConfigBakerConfigWithGameSetting("Game.Mode", literal)));
            TestRig rig {"Test.fomain", temp_dir};
            auto metadata {MakeMetadataBlob({{"Enum", {{"ProbeMode", "uint8", "Zero", "0", "One", "1"}}}, {"Setting", {{"Game.Mode", "ProbeMode"}}}})};
            rig.AddBakedFile("Metadata.fometa-server", metadata);
            rig.AddBakedFile("Metadata.fometa-client", metadata);
            ConfigBaker baker {rig.MakeContext("ConfigPack")};

            CHECK_THROWS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
        }
    }

    SECTION("RejectsOutOfRangeGameEnumValues")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); })};
        REQUIRE(fs::create_directories(temp_dir));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};

        auto metadata {MakeMetadataBlob({
            {"Enum", {{"ProbeMode", "uint8", "Zero", "0", "One", "1", "Negative", "-1"}, {"ProbeSigned", "int8", "Zero", "0", "Min", "-128", "Max", "127"}, {"ProbeOversized", "uint8", "Zero", "0", "TooBig", "256"}, {"ProbeOversizedSigned", "int8", "Zero", "0", "TooBig", "128"}}},
            {"Setting", {{"Game.Mode", "ProbeMode"}, {"Game.Signed", "ProbeSigned"}, {"Game.Oversized", "ProbeOversized"}, {"Game.OversizedSigned", "ProbeOversizedSigned"}}},
        })};

        vector<pair<string_view, string_view>> invalid_values {
            {"Game.Mode", "256"},
            {"Game.Mode", "0x100"},
            {"Game.Mode", "-1"},
            {"Game.Mode", "Negative"},
            {"Game.Mode", "1.5"},
            {"Game.Mode", "0.0"},
            {"Game.Mode", "0e0"},
            {"Game.Signed", "128"},
            {"Game.Signed", "-129"},
            {"Game.Oversized", "TooBig"},
            {"Game.Oversized", "ProbeOversized::TooBig"},
            {"Game.OversizedSigned", "TooBig"},
        };

        for (const auto& [setting_name, literal] : invalid_values) {
            REQUIRE(fs::write_file(config_path, MakeConfigBakerConfigWithGameSetting(setting_name, literal)));
            TestRig rig {"Test.fomain", temp_dir};
            rig.AddBakedFile("Metadata.fometa-server", metadata);
            rig.AddBakedFile("Metadata.fometa-client", metadata);
            ConfigBaker baker {rig.MakeContext("ConfigPack")};

            CHECK_THROWS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
        }
    }

    SECTION("SkipsUnsupportedTargetWithoutTouchingMetadata")
    {
        TestRig rig {};
        ConfigBaker baker {rig.MakeContext()};

        CHECK_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipAllConfigs")
    {
        TestRig rig {};
        ConfigFile config {"[SubConfig]\n"
                           "Name = Child\n"
                           "Common.GameName = ChildGame\n"};
        rig.Settings.ApplyConfigFile(config, "");

        vector<pair<string, uint64_t>> checks {};
        ConfigBaker baker {rig.MakeContext("ConfigPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        })};
        CHECK_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        CHECK(rig.Outputs.empty());
        REQUIRE(checks.size() == 8);
        CHECK(checks[0].first == "(Root).fomain-server");
        CHECK(checks[1].first == "(Root).fomain-embedded-server");
        CHECK(checks[2].first == "(Root).fomain-client");
        CHECK(checks[3].first == "(Root).fomain-embedded-client");
        CHECK(checks[4].first == "Child.fomain-server");
        CHECK(checks[5].first == "Child.fomain-embedded-server");
        CHECK(checks[6].first == "Child.fomain-client");
        CHECK(checks[7].first == "Child.fomain-embedded-client");
        CHECK(std::ranges::all_of(checks, [](const auto& check) { return check.second == std::numeric_limits<uint64_t>::max(); }));
    }

    SECTION("BakesCompleteRootConfig")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "[ResourcePack]\nName = Core\nInputDirs = core\n"
                "[ResourcePack]\nName = Art\nInputDirs = art\nClientOnly = True\n"));

        TestRig rig {"Test.fomain", temp_dir};
        AddConfigBakerMetadata(rig);

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        REQUIRE(rig.Outputs.contains("(Root).fomain-server"));
        REQUIRE(rig.Outputs.contains("(Root).fomain-client"));
        REQUIRE(rig.Outputs.contains("(Root).fomain-embedded-server"));
        REQUIRE(rig.Outputs.contains("(Root).fomain-embedded-client"));
        string server_config {rig.GetOutputText("(Root).fomain-server")};
        string client_config {rig.GetOutputText("(Root).fomain-client")};
        string embedded_server_config {rig.GetOutputText("(Root).fomain-embedded-server")};
        string embedded_client_config {rig.GetOutputText("(Root).fomain-embedded-client")};

        // A packaged application has no other source for its pack list, and the sections close the config
        string pack_declarations {"[ResourcePack]\nName=Core\n[ResourcePack]\nName=Art\nClientOnly=1\n"};
        CHECK(server_config.ends_with(pack_declarations));
        CHECK(client_config.ends_with(pack_declarations));
        CHECK(embedded_server_config.ends_with(pack_declarations));
        CHECK(embedded_client_config.ends_with(pack_declarations));
        CHECK(server_config.find("InputDirs") == string::npos);
        CHECK(embedded_server_config.find("InputDirs") == string::npos);
        CHECK(embedded_client_config.find("InputDirs") == string::npos);

        string compatibility_comment {"#" FO_COMPATIBILITY_VERSION "\n"};
        CHECK(embedded_server_config.starts_with(compatibility_comment));
        CHECK(embedded_client_config.starts_with(compatibility_comment));

        CHECK(server_config.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(client_config.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(server_config.find("ServerNetwork.ClientPingTime=10000\n") != string::npos);
        CHECK(client_config.find("ServerNetwork.ClientPingTime=") == string::npos);
        CHECK(client_config.find("ClientNetwork.ServerHost=127.0.0.1\n") != string::npos);
        CHECK(server_config.find("Common.AsyncLogWrite=") == string::npos);

        // Embedded server carries the native server and client halves; embedded client carries its own half
        CHECK(embedded_server_config.find("ServerNetwork.ClientPingTime=10000\n") != string::npos);
        CHECK(embedded_server_config.find("ClientNetwork.ServerHost=127.0.0.1\n") != string::npos);
        CHECK(embedded_client_config.find("ClientNetwork.ServerHost=127.0.0.1\n") != string::npos);
        CHECK(embedded_client_config.find("ServerNetwork.ClientPingTime=") == string::npos);

        // Nonzero reference defaults have to travel in full, while typed zero reference defaults stay absent
        CHECK(server_config.find("Render.WindowCentered=1\n") != string::npos);
        CHECK(server_config.find("Geometry.MapSmoothPath=1\n") != string::npos);
        CHECK(client_config.find("Audio.MixRate=44100\n") != string::npos);
        CHECK(client_config.find("View.ScreenWidth=1024\n") != string::npos);
        CHECK(client_config.find("Client.DirectMoveTraceSteps=24\n") != string::npos);
        CHECK(server_config.find("Network.UdpPortOffset=") == string::npos);
        CHECK(server_config.find("Render.WindowResizable=") == string::npos);
        CHECK(server_config.find("Render.HeadBone=") == string::npos);
        CHECK(client_config.find("ClientNetwork.ChannelServerKeys=") == string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("WritesCompleteGameSettingsIncludingDeclaredCustomSettings")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Server.CustomEnabled = true\n"
                "Client.CustomTitle = Frontend\n"
                "Unknown.CustomSetting = Visible\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeMetadataBlob({{"Setting", {{"Server.CustomEnabled", "bool"}, {"Common.GameName", "string"}}}}));
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.CustomTitle", "string"}, {"Common.GameName", "string"}}}}));

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string server_config {rig.GetOutputText("(Root).fomain-server")};
        string client_config {rig.GetOutputText("(Root).fomain-client")};
        string embedded_server_config {rig.GetOutputText("(Root).fomain-embedded-server")};
        string embedded_client_config {rig.GetOutputText("(Root).fomain-embedded-client")};
        CHECK(server_config.find("Server.CustomEnabled=1\n") != string::npos);
        CHECK(server_config.find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(server_config.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(server_config.find("Unknown.CustomSetting=Visible\n") != string::npos);
        CHECK(client_config.find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(client_config.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(client_config.find("Server.CustomEnabled=") == string::npos);
        CHECK(client_config.find("Unknown.CustomSetting=") == string::npos);

        // Embedded profiles keep native engine settings and drop nonbootstrap game and unknown values
        CHECK(embedded_server_config.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(embedded_server_config.find("Client.DirectMoveTraceSteps=24\n") != string::npos);
        CHECK(embedded_client_config.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(embedded_server_config.find("Server.CustomEnabled=") == string::npos);
        CHECK(embedded_server_config.find("Client.CustomTitle=") == string::npos);
        CHECK(embedded_client_config.find("Client.CustomTitle=") == string::npos);
        CHECK(embedded_server_config.find("Unknown.CustomSetting=") == string::npos);
        CHECK(embedded_client_config.find("Unknown.CustomSetting=") == string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("WritesFullGameProfilesWithTypedZeroOmission")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Server.CustomEnabled = true\n"
                "Client.CustomTitle = Frontend\n"
                "[SubConfig]\n"
                "Name = Override\n"
                "Server.CustomEnabled = false\n"
                "Client.CustomTitle = Overridden\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeMetadataBlob({{"Setting", {{"Server.CustomEnabled", "bool"}}}}));
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.CustomTitle", "string"}}}}));

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string root_server_config {rig.GetOutputText("(Root).fomain-server")};
        string root_client_config {rig.GetOutputText("(Root).fomain-client")};
        string sub_server_config {rig.GetOutputText("Override.fomain-server")};
        string sub_client_config {rig.GetOutputText("Override.fomain-client")};
        string sub_embedded_server_config {rig.GetOutputText("Override.fomain-embedded-server")};
        string sub_embedded_client_config {rig.GetOutputText("Override.fomain-embedded-client")};
        CHECK(root_server_config.find("Server.CustomEnabled=1\n") != string::npos);
        CHECK(root_client_config.find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(sub_server_config.find("Server.CustomEnabled=") == string::npos);
        CHECK(sub_client_config.find("Client.CustomTitle=Overridden\n") != string::npos);
        CHECK(sub_server_config.find("Client.CustomTitle=Overridden\n") != string::npos);
        CHECK(sub_client_config.find("Server.CustomEnabled=") == string::npos);

        // The embedded pair follows the same subconfig resolution but leaves nonbootstrap game values out
        CHECK(sub_embedded_server_config.find("Server.CustomEnabled=") == string::npos);
        CHECK(sub_embedded_server_config.find("Client.CustomTitle=") == string::npos);
        CHECK(sub_embedded_client_config.find("Client.CustomTitle=") == string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("WritesFullGameProfilesForSelectedSubConfigs")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Server.CustomEnabled = true\n"
                "Client.CustomTitle = Root\n"
                "[SubConfig]\n"
                "Name = Public\n"
                "Server.CustomEnabled = false\n"
                "Client.CustomTitle = Public\n"
                "[SubConfig]\n"
                "Name = Staging\n"
                "Parent = Public\n"
                "Server.CustomEnabled = true\n"
                "Client.CustomTitle = Root\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeMetadataBlob({{"Setting", {{"Server.CustomEnabled", "bool"}}}}));
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.CustomTitle", "string"}}}}));
        rig.Settings.ApplySubConfigSection("Public");

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string root_server_config {rig.GetOutputText("(Root).fomain-server")};
        string root_client_config {rig.GetOutputText("(Root).fomain-client")};
        string public_server_config {rig.GetOutputText("Public.fomain-server")};
        string public_client_config {rig.GetOutputText("Public.fomain-client")};
        string staging_server_config {rig.GetOutputText("Staging.fomain-server")};
        string staging_client_config {rig.GetOutputText("Staging.fomain-client")};
        CHECK(root_server_config.find("Server.CustomEnabled=1\n") != string::npos);
        CHECK(root_client_config.find("Client.CustomTitle=Root\n") != string::npos);
        CHECK(public_server_config.find("Server.CustomEnabled=") == string::npos);
        CHECK(public_server_config.find("Client.CustomTitle=Public\n") != string::npos);
        CHECK(public_client_config.find("Client.CustomTitle=Public\n") != string::npos);
        CHECK(public_client_config.find("Server.CustomEnabled=") == string::npos);
        CHECK(staging_server_config.find("Server.CustomEnabled=1\n") != string::npos);
        CHECK(staging_server_config.find("Client.CustomTitle=Root\n") != string::npos);
        CHECK(staging_client_config.find("Client.CustomTitle=Root\n") != string::npos);
        CHECK(staging_client_config.find("Server.CustomEnabled=") == string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("WritesBootstrapGameSettingInFullAndEmbeddedProfiles")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Baking.BootstrapGameSettings = Client.CustomTitle\n"
                "Server.CustomEnabled = true\n"
                "Client.CustomTitle = Frontend\n"
                "[SubConfig]\n"
                "Name = Override\n"
                "Server.CustomEnabled = false\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeMetadataBlob({{"Setting", {{"Server.CustomEnabled", "bool"}}}}));
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.CustomTitle", "string"}}}}));

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        // A complete profile carries the listed setting and every other nonzero game setting without a special case
        CHECK(rig.GetOutputText("(Root).fomain-server").find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(rig.GetOutputText("(Root).fomain-client").find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(rig.GetOutputText("Override.fomain-server").find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(rig.GetOutputText("Override.fomain-client").find("Client.CustomTitle=Frontend\n") != string::npos);

        CHECK(rig.GetOutputText("(Root).fomain-server").find("Server.CustomEnabled=1\n") != string::npos);
        CHECK(rig.GetOutputText("Override.fomain-server").find("Server.CustomEnabled=") == string::npos);

        // The server aggregate also retains listed client values for embedded-client clones
        CHECK(rig.GetOutputText("(Root).fomain-embedded-client").find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(rig.GetOutputText("Override.fomain-embedded-client").find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(rig.GetOutputText("(Root).fomain-embedded-server").find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(rig.GetOutputText("Override.fomain-embedded-server").find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(rig.GetOutputText("(Root).fomain-embedded-server").find("Server.CustomEnabled=") == string::npos);
        CHECK(rig.GetOutputText("Override.fomain-embedded-server").find("Server.CustomEnabled=") == string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("WritesEmbeddedProfilesWithNativeEngineAndBootstrapSettings")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Baking.BootstrapGameSettings = Server.BootstrapValue Client.BootstrapValue\n"
                "Server.PrivateValue = private-server\n"
                "Client.PrivateValue = private-client\n"
                "Server.BootstrapValue = bootstrap-server\n"
                "Client.BootstrapValue = bootstrap-client\n"
                "Unknown.CustomSetting = ignored\n"
                "[ResourcePack]\n"
                "Name = Core\n"
                "InputDirs = core\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeMetadataBlob({{"Setting", {{"Server.PrivateValue", "string"}, {"Server.BootstrapValue", "string"}}}}));
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.PrivateValue", "string"}, {"Client.BootstrapValue", "string"}}}}));

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string server_config {rig.GetOutputText("(Root).fomain-server")};
        string client_config {rig.GetOutputText("(Root).fomain-client")};
        string embedded_server_config {rig.GetOutputText("(Root).fomain-embedded-server")};
        string embedded_client_config {rig.GetOutputText("(Root).fomain-embedded-client")};

        // Full sibling profiles keep every routed game value and arbitrary unknown settings
        CHECK(server_config.find("Server.PrivateValue=private-server\n") != string::npos);
        CHECK(server_config.find("Client.PrivateValue=private-client\n") != string::npos);
        CHECK(client_config.find("Client.PrivateValue=private-client\n") != string::npos);
        CHECK(client_config.find("Server.PrivateValue=") == string::npos);
        CHECK(server_config.find("Unknown.CustomSetting=ignored\n") != string::npos);

        // Embedded server keeps both native halves; embedded client keeps only its own
        CHECK(embedded_server_config.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(embedded_server_config.find("ServerNetwork.ClientPingTime=10000\n") != string::npos);
        CHECK(embedded_server_config.find("ClientNetwork.ServerHost=127.0.0.1\n") != string::npos);
        CHECK(embedded_server_config.find("Client.DirectMoveTraceSteps=24\n") != string::npos);
        CHECK(embedded_client_config.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(embedded_client_config.find("ClientNetwork.ServerHost=127.0.0.1\n") != string::npos);
        CHECK(embedded_client_config.find("Client.DirectMoveTraceSteps=24\n") != string::npos);
        CHECK(embedded_client_config.find("ServerNetwork.ClientPingTime=") == string::npos);

        // Embedded profiles keep the bootstrap values present in their full sibling
        CHECK(embedded_server_config.find("Server.BootstrapValue=bootstrap-server\n") != string::npos);
        CHECK(embedded_server_config.find("Client.BootstrapValue=bootstrap-client\n") != string::npos);
        CHECK(embedded_client_config.find("Client.BootstrapValue=bootstrap-client\n") != string::npos);
        CHECK(embedded_client_config.find("Server.BootstrapValue=") == string::npos);

        // Nonbootstrap game values and unknown keys never enter the embedded profiles
        CHECK(embedded_server_config.find("Server.PrivateValue=") == string::npos);
        CHECK(embedded_server_config.find("Client.PrivateValue=") == string::npos);
        CHECK(embedded_client_config.find("Server.PrivateValue=") == string::npos);
        CHECK(embedded_client_config.find("Client.PrivateValue=") == string::npos);
        CHECK(embedded_server_config.find("Unknown.CustomSetting=") == string::npos);
        CHECK(embedded_client_config.find("Unknown.CustomSetting=") == string::npos);

        string compatibility_comment {"#" FO_COMPATIBILITY_VERSION "\n"};
        string pack_declarations {"[ResourcePack]\nName=Core\n"};
        CHECK(embedded_server_config.starts_with(compatibility_comment));
        CHECK(embedded_client_config.starts_with(compatibility_comment));
        CHECK(embedded_server_config.ends_with(pack_declarations));
        CHECK(embedded_client_config.ends_with(pack_declarations));
        CHECK(server_config.ends_with(pack_declarations));
        CHECK(client_config.ends_with(pack_declarations));

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("EmbeddedProfilesOmitTypedZerosAndNormalizeBootstrapValues")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Baking.BootstrapGameSettings = Server.BootstrapFlag Server.BootstrapZero Server.BootstrapEnv Client.BootstrapText\n"
                "Server.BootstrapFlag = true\n"
                "Server.BootstrapZero = 0\n"
                "Server.BootstrapEnv = $TARGET_ENV{CONFIG_BAKER_EMBEDDED}\n"
                "Client.BootstrapText = false\n"
                "Client.UpdaterInfoDelay = 0\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeMetadataBlob({{"Setting", {{"Server.BootstrapFlag", "bool"}, {"Server.BootstrapZero", "int32"}, {"Server.BootstrapEnv", "string"}}}}));
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.BootstrapText", "string"}}}}));

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string server_config {rig.GetOutputText("(Root).fomain-server")};
        string embedded_server_config {rig.GetOutputText("(Root).fomain-embedded-server")};
        string embedded_client_config {rig.GetOutputText("(Root).fomain-embedded-client")};
        CHECK(server_config.find("Server.BootstrapFlag=1\n") != string::npos);
        CHECK(server_config.find("Server.BootstrapZero=") == string::npos);
        CHECK(server_config.find("Server.BootstrapEnv=$TARGET_ENV{CONFIG_BAKER_EMBEDDED}\n") != string::npos);
        CHECK(embedded_server_config.find("Server.BootstrapFlag=1\n") != string::npos);
        CHECK(embedded_server_config.find("Server.BootstrapZero=") == string::npos);
        CHECK(embedded_server_config.find("Server.BootstrapEnv=$TARGET_ENV{CONFIG_BAKER_EMBEDDED}\n") != string::npos);
        CHECK(embedded_server_config.find("Client.BootstrapText=false\n") != string::npos);
        CHECK(embedded_client_config.find("Client.BootstrapText=false\n") != string::npos);
        CHECK(embedded_client_config.find("Server.BootstrapFlag=") == string::npos);
        CHECK(embedded_client_config.find("Client.UpdaterInfoDelay=") == string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("EmbeddedProfilesInheritSubConfigOverrides")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Baking.BootstrapGameSettings = Client.BootstrapTitle\n"
                "Client.BootstrapTitle = RootTitle\n"
                "Client.Language = RootLang\n"
                "[SubConfig]\n"
                "Name = Override\n"
                "Client.BootstrapTitle = OverrideTitle\n"
                "Client.Language = OverrideLang\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeEmptyMetadataBlob());
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.BootstrapTitle", "string"}}}}));

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string root_server_config {rig.GetOutputText("(Root).fomain-server")};
        string root_embedded_server_config {rig.GetOutputText("(Root).fomain-embedded-server")};
        string root_embedded_client_config {rig.GetOutputText("(Root).fomain-embedded-client")};
        string override_server_config {rig.GetOutputText("Override.fomain-server")};
        string override_embedded_server_config {rig.GetOutputText("Override.fomain-embedded-server")};
        string override_embedded_client_config {rig.GetOutputText("Override.fomain-embedded-client")};
        CHECK(root_server_config.find("Client.BootstrapTitle=RootTitle\n") != string::npos);
        CHECK(override_server_config.find("Client.BootstrapTitle=OverrideTitle\n") != string::npos);
        CHECK(root_embedded_client_config.find("Client.BootstrapTitle=RootTitle\n") != string::npos);
        CHECK(root_embedded_client_config.find("Client.Language=RootLang\n") != string::npos);
        CHECK(override_embedded_client_config.find("Client.BootstrapTitle=OverrideTitle\n") != string::npos);
        CHECK(override_embedded_client_config.find("Client.Language=OverrideLang\n") != string::npos);
        CHECK(root_embedded_server_config.find("Client.BootstrapTitle=RootTitle\n") != string::npos);
        CHECK(override_embedded_server_config.find("Client.BootstrapTitle=OverrideTitle\n") != string::npos);
        CHECK(root_embedded_server_config.find("Client.Language=RootLang\n") != string::npos);
        CHECK(override_embedded_server_config.find("Client.Language=OverrideLang\n") != string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("BakesWhenEmbeddedOutputIsMissing")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path, MakeCompleteConfigBakerConfig()));

        TestRig rig {"Test.fomain", temp_dir};
        AddConfigBakerMetadata(rig);

        vector<string> checks {};
        ConfigBaker baker {rig.MakeContext("ConfigPack", [&checks](string_view path, uint64_t) {
            checks.emplace_back(string {path});
            return path == "(Root).fomain-embedded-server";
        })};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        REQUIRE(checks.size() == 4);
        CHECK(checks[0] == "(Root).fomain-server");
        CHECK(checks[1] == "(Root).fomain-embedded-server");
        CHECK(checks[2] == "(Root).fomain-client");
        CHECK(checks[3] == "(Root).fomain-embedded-client");
        REQUIRE(rig.Outputs.size() == 4);
        CHECK(rig.Outputs.contains("(Root).fomain-server"));
        CHECK(rig.Outputs.contains("(Root).fomain-embedded-server"));
        CHECK(rig.Outputs.contains("(Root).fomain-client"));
        CHECK(rig.Outputs.contains("(Root).fomain-embedded-client"));

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("ClientConfigBakesWhenEmbeddedOutputIsMissing")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path, MakeCompleteConfigBakerConfig() + "[ResourcePack]\nName=TestPack\n"));

        TestRig rig {"Test.fomain", temp_dir};
        AddConfigBakerMetadata(rig);

        vector<string> checks {};
        ClientConfigBaker baker {rig.MakeContext("TestPack", [&checks](string_view path, uint64_t) {
            checks.emplace_back(string {path});
            return path == "(Root).fomain-embedded-client";
        })};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        REQUIRE(checks.size() == 2);
        CHECK(checks[0] == "(Root).fomain-client");
        CHECK(checks[1] == "(Root).fomain-embedded-client");
        REQUIRE(rig.Outputs.size() == 2);
        CHECK(rig.Outputs.contains("(Root).fomain-client"));
        CHECK(rig.Outputs.contains("(Root).fomain-embedded-client"));
        CHECK_FALSE(rig.Outputs.contains("(Root).fomain-server"));
        CHECK_FALSE(rig.Outputs.contains("(Root).fomain-embedded-server"));

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("RejectsBootstrapGameSettingThatIsNotDeclared")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Baking.BootstrapGameSettings = Client.CustomTytle\n"
                "Client.CustomTitle = Frontend\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeEmptyMetadataBlob());
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.CustomTitle", "string"}}}}));

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_THROWS_WITH(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""), Catch::Matchers::ContainsSubstring("Bootstrap game setting is not a declared game setting"));

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("BakingRequiresSingleAppliedRootConfig")
    {
        TestRig rig {};
        AddConfigBakerMetadata(rig);

        ConfigBaker baker {rig.MakeContext()};
        CHECK_THROWS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
    }

    SECTION("BakingRequiresBakedMetadata")
    {
        auto temp_dir {std::filesystem::temp_directory_path() / "lf-configbaker-test"};
        std::filesystem::create_directories(temp_dir);
        auto fomain_path {temp_dir / "Test.fomain"};
        REQUIRE(fs::write_file(fs::path_to_string(fomain_path), "GameName = Test\n"));

        TestRig rig {"Test.fomain", fs::path_to_string(temp_dir)};

        ConfigBaker baker {rig.MakeContext()};

        CHECK_THROWS_AS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""), MetadataNotFoundException);

        std::error_code ec {};
        std::filesystem::remove_all(temp_dir, ec);
    }

    SECTION("BakingReportsIncompleteConfigSettings")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            "Common.GameName = RootGame\n"
            "[SubConfig]\n"
            "Name = Child\n"
            "Common.GameName = ChildGame\n"));

        TestRig rig {"Test.fomain", temp_dir};
        AddConfigBakerMetadata(rig);

        ConfigBaker baker {rig.MakeContext("ConfigPack", [](string_view path, uint64_t) { return path == "Child.fomain-server"; })};
        CHECK_THROWS_AS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""), ConfigBakerException);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("ClientOnlyPackContainsCompleteProfilesWithoutServerData")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        auto cleanup {scope_exit([&temp_dir]() noexcept { safe_call([&temp_dir] { (void)fs::remove_dir_tree(temp_dir); }); })};
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        string config {MakeCompleteConfigBakerConfig() +
            "Client.Language=\nClient.DirectMoveSlide=false\n"
            "Client.DirectMoveRetargetMinMs=0\nServerNetwork.ChannelSecretKey=private-server-value\n"
            "[ResourcePack]\nName=Metadata\nBakers=Config ClientConfig\n"
            "[SubConfig]\nName=Child\nCommon.GameVersion=1.2.3\n"};
        bool written {fs::write_file(config_path, config)};
        REQUIRE(written);
        TestRig rig {"Test.fomain", temp_dir};
        AddConfigBakerMetadata(rig);
        auto context {rig.MakeContext("Metadata")};
        ClientConfigBaker baker {context};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
        REQUIRE(rig.Outputs.size() == 4);
        CHECK_FALSE(rig.Outputs.contains("(Root).fomain-server"));
        CHECK_FALSE(rig.Outputs.contains("Child.fomain-server"));
        CHECK_FALSE(rig.Outputs.contains("(Root).fomain-embedded-server"));
        CHECK_FALSE(rig.Outputs.contains("Child.fomain-embedded-server"));
        string root {rig.GetOutputText("(Root).fomain-client")};
        string root_embedded {rig.GetOutputText("(Root).fomain-embedded-client")};
        string child {rig.GetOutputText("Child.fomain-client")};
        string child_embedded {rig.GetOutputText("Child.fomain-embedded-client")};
        CHECK(root.find("Client.Language=") == string::npos);
        CHECK(root.find("Client.DirectMoveSlide=") == string::npos);
        CHECK(root.find("Client.DirectMoveRetargetMinMs=") == string::npos);
        CHECK(root.find("Client.DirectMoveTraceSteps=24\n") != string::npos);
        CHECK(root.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(root.find("ServerNetwork.") == string::npos);
        CHECK(root.find("private-server-value") == string::npos);
        CHECK(child.find("Common.GameVersion=1.2.3\n") != string::npos);
        ConfigFile profile {child};
        CHECK(profile.GetAsStr("", "Common.GameVersion") == "1.2.3");
        CHECK(root_embedded.find("Client.DirectMoveTraceSteps=24\n") != string::npos);
        CHECK(root_embedded.find("Common.GameName=FOnline\n") != string::npos);
        CHECK(root_embedded.find("ServerNetwork.") == string::npos);
        CHECK(root_embedded.find("private-server-value") == string::npos);
        CHECK(child_embedded.find("Common.GameVersion=1.2.3\n") != string::npos);
        ConfigBaker server_baker {context};
        REQUIRE_NOTHROW(server_baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
        CHECK(rig.Outputs.size() == 8);
        CHECK(rig.Outputs.contains("(Root).fomain-server"));
        CHECK(rig.Outputs.contains("(Root).fomain-embedded-server"));
        CHECK(rig.Outputs.contains("Child.fomain-server"));
        CHECK(rig.Outputs.contains("Child.fomain-embedded-server"));
        CHECK(rig.GetOutputText("(Root).fomain-client") == root);
        CHECK(rig.GetOutputText("(Root).fomain-embedded-client") == root_embedded);
        CHECK(rig.GetOutputText("Child.fomain-client") == child);
        CHECK(rig.GetOutputText("Child.fomain-embedded-client") == child_embedded);
    }

    SECTION("ClientProfilesAgreeBetweenConfigAndClientConfigBakers")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Client.CustomTitle = Frontend\n"
                "Server.CustomGone = hidden\n"
                "[ResourcePack]\nName=TestPack\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeMetadataBlob({{"Setting", {{"Server.CustomGone", "string"}}}}));
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.CustomTitle", "string"}}}}));

        ClientConfigBaker client_baker {rig.MakeContext("TestPack")};
        REQUIRE_NOTHROW(client_baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
        REQUIRE(rig.Outputs.size() == 2);
        string client_only_profile {rig.GetOutputText("(Root).fomain-client")};
        string client_only_embedded_profile {rig.GetOutputText("(Root).fomain-embedded-client")};

        ConfigBaker config_baker {rig.MakeContext("TestPack")};
        REQUIRE_NOTHROW(config_baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));
        REQUIRE(rig.Outputs.size() == 4);
        string server_profile {rig.GetOutputText("(Root).fomain-server")};
        string client_profile {rig.GetOutputText("(Root).fomain-client")};
        string server_embedded_profile {rig.GetOutputText("(Root).fomain-embedded-server")};
        string client_embedded_profile {rig.GetOutputText("(Root).fomain-embedded-client")};
        CHECK(client_profile == client_only_profile);
        CHECK(client_embedded_profile == client_only_embedded_profile);
        CHECK(server_profile.find("Server.CustomGone=hidden\n") != string::npos);
        CHECK(server_profile.find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(client_profile.find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(client_profile.find("Server.CustomGone=") == string::npos);
        CHECK(server_embedded_profile.find("Server.CustomGone=") == string::npos);
        CHECK(server_embedded_profile.find("Client.CustomTitle=") == string::npos);
        CHECK(client_embedded_profile.find("Client.CustomTitle=") == string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("OmitsNativeTypedZerosAndPreservesAuthoredLiterals")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Client.Language = 0\n"
                "Client.DefaultSplash = false\n"
                "Client.UpdaterInfoDelay = 0\n"
                "View.MapCameraAngle = 0\n"
                "Client.DirectMoveSlide = false\n"
                "View.MapRenderTargetScale = 0.0001\n"
                "Audio.SoundFileExtensions =\n"
                "Common.SecretSettingTokens = 0\n"
                "Baking.ProtoFileExtensions = 0\n"
                "Critter.CritterSlotEnabled = 0 false\n"
                "Unknown.CustomZero = 0\n"
                "Unknown.CustomFalse = false\n"));

        TestRig rig {"Test.fomain", temp_dir};
        AddConfigBakerMetadata(rig);

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string server_config {rig.GetOutputText("(Root).fomain-server")};
        string client_config {rig.GetOutputText("(Root).fomain-client")};
        string embedded_server_config {rig.GetOutputText("(Root).fomain-embedded-server")};
        string embedded_client_config {rig.GetOutputText("(Root).fomain-embedded-client")};
        ConfigFile client_profile {client_config};

        // A native string is zero only when empty, so literal 0 and false values survive as strings
        CHECK(client_config.find("Client.Language=0\n") != string::npos);
        CHECK(client_config.find("Client.DefaultSplash=false\n") != string::npos);

        // Native typed zeros are omitted, including false, integer zero, empty strings and empty arrays
        CHECK(client_config.find("Client.UpdaterInfoDelay=") == string::npos);
        CHECK(client_config.find("View.MapCameraAngle=") == string::npos);
        CHECK(client_config.find("Client.DirectMoveSlide=") == string::npos);
        CHECK(client_config.find("Render.WindowResizable=") == string::npos);
        CHECK(client_config.find("Render.HeadBone=") == string::npos);
        CHECK(client_config.find("Common.AsyncLogWrite=") == string::npos);
        CHECK(client_config.find("Audio.SoundFileExtensions=") == string::npos);
        CHECK(client_config.find("Animation.PreviewActionAnim=") == string::npos);
        CHECK(client_config.find("Animation.PreviewStateAnim=") == string::npos);

        // Nonzero reference defaults are written in full
        CHECK(client_config.find("Render.WindowCentered=1\n") != string::npos);
        CHECK(client_config.find("Audio.MixRate=44100\n") != string::npos);
        CHECK(server_config.find("Geometry.MapSmoothPath=1\n") != string::npos);

        // A tiny nonzero native float is not rounded down to zero in the profile
        string_view render_scale {client_profile.GetAsStr("", "View.MapRenderTargetScale")};
        CHECK_FALSE(render_scale.empty());
        CHECK(strvex(render_scale).to_float32() > 0.0f);
        CHECK(strvex(render_scale).to_float32() < 0.001f);

        // Arrays are zero only when empty, even when every element is zero
        CHECK(client_config.find("Common.SecretSettingTokens=0\n") != string::npos);
        CHECK(client_config.find("Baking.ProtoFileExtensions=0\n") != string::npos);
        CHECK(server_config.find("Critter.CritterSlotEnabled=") != string::npos);
        CHECK(client_config.find("Critter.CritterSlotEnabled=") == string::npos);

        // Unknown settings have no declared type, so their literals are never interpreted as zero or false
        CHECK(server_config.find("Unknown.CustomZero=0\n") != string::npos);
        CHECK(server_config.find("Unknown.CustomFalse=false\n") != string::npos);
        CHECK(client_config.find("Unknown.CustomZero=") == string::npos);
        CHECK(client_config.find("Unknown.CustomFalse=") == string::npos);

        // Embedded profiles use the same typed-zero rules and omit unknown settings entirely
        CHECK(embedded_client_config.find("Client.Language=0\n") != string::npos);
        CHECK(embedded_client_config.find("Client.DefaultSplash=false\n") != string::npos);
        CHECK(embedded_client_config.find("Client.UpdaterInfoDelay=") == string::npos);
        CHECK(embedded_client_config.find("View.MapCameraAngle=") == string::npos);
        CHECK(embedded_client_config.find("Client.DirectMoveSlide=") == string::npos);
        CHECK(embedded_client_config.find("Common.SecretSettingTokens=0\n") != string::npos);
        CHECK(embedded_client_config.find("Baking.ProtoFileExtensions=0\n") != string::npos);
        CHECK(embedded_server_config.find("Critter.CritterSlotEnabled=") != string::npos);
        CHECK(embedded_client_config.find("Critter.CritterSlotEnabled=") == string::npos);
        CHECK(embedded_server_config.find("Unknown.CustomZero=") == string::npos);
        CHECK(embedded_server_config.find("Unknown.CustomFalse=") == string::npos);
        CHECK(embedded_client_config.find("Unknown.CustomZero=") == string::npos);
        CHECK(embedded_client_config.find("Unknown.CustomFalse=") == string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("WritesNonZeroGameSettingsAndOmitsTypedZeros")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Server.CustomEnabled = true\n"
                "Server.CustomCount = 0\n"
                "Server.CustomPort = 18446744073709551615\n"
                "Server.CustomLabel = 0\n"
                "Server.CustomRatio = 0.0001\n"
                "Client.CustomFlag = false\n"
                "Client.CustomText = false\n"
                "Client.CustomEmpty =\n"
                "Client.CustomDelta = 12\n"
                "Client.CustomTitle = Frontend\n"
                "Client.CustomScale = 0\n"
                "Unknown.CustomZero = 0\n"
                "Unknown.CustomFalse = false\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeMetadataBlob({{"Setting", {{"Server.CustomEnabled", "bool"}, {"Server.CustomCount", "int32"}, {"Server.CustomPort", "uint64"}, {"Server.CustomLabel", "string"}, {"Server.CustomRatio", "float32"}, {"Common.GameName", "string"}}}}));
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.CustomFlag", "bool"}, {"Client.CustomText", "string"}, {"Client.CustomEmpty", "string"}, {"Client.CustomDelta", "int32"}, {"Client.CustomTitle", "string"}, {"Client.CustomScale", "float32"}, {"Common.GameName", "string"}}}}));

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string server_config {rig.GetOutputText("(Root).fomain-server")};
        string client_config {rig.GetOutputText("(Root).fomain-client")};

        // Nonzero authored values are written into the full profile
        CHECK(server_config.find("Server.CustomEnabled=1\n") != string::npos);
        CHECK(server_config.find("Server.CustomPort=18446744073709551615\n") != string::npos);
        CHECK(server_config.find("Server.CustomLabel=0\n") != string::npos);
        CHECK(server_config.find("Server.CustomRatio=0.0001\n") != string::npos);
        CHECK(server_config.find("Client.CustomText=false\n") != string::npos);
        CHECK(server_config.find("Client.CustomDelta=12\n") != string::npos);
        CHECK(server_config.find("Client.CustomTitle=Frontend\n") != string::npos);
        CHECK(client_config.find("Client.CustomText=false\n") != string::npos);
        CHECK(client_config.find("Client.CustomDelta=12\n") != string::npos);
        CHECK(client_config.find("Client.CustomTitle=Frontend\n") != string::npos);

        // Typed zero game values are omitted from the full profile
        CHECK(server_config.find("Server.CustomCount=") == string::npos);
        CHECK(server_config.find("Client.CustomFlag=") == string::npos);
        CHECK(server_config.find("Client.CustomEmpty=") == string::npos);
        CHECK(server_config.find("Client.CustomScale=") == string::npos);
        CHECK(client_config.find("Client.CustomFlag=") == string::npos);
        CHECK(client_config.find("Client.CustomEmpty=") == string::npos);
        CHECK(client_config.find("Client.CustomScale=") == string::npos);
        CHECK(client_config.find("Server.CustomEnabled=") == string::npos);
        CHECK(client_config.find("Server.CustomPort=") == string::npos);
        CHECK(client_config.find("Unknown.CustomZero=") == string::npos);
        CHECK(client_config.find("Unknown.CustomFalse=") == string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("PreservesRuntimeTargetDirectivesVerbatim")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        REQUIRE(std::filesystem::create_directories(fs::make_path(temp_dir)));
        string config_path {static_cast<string&&>(strex(temp_dir).combine_path("Test.fomain"))};
        REQUIRE(fs::write_file(config_path,
            MakeCompleteConfigBakerConfig() +
                "Baking.BootstrapGameSettings = Server.BootstrapEnvTarget Client.BootstrapFileTarget\n"
                "Server.CustomEnvTarget = $TARGET_ENV{CONFIG_BAKER_BOOL}\n"
                "Client.CustomFileTarget = $TARGET_FILE{config_baker_int.txt}\n"
                "Server.BootstrapEnvTarget = $TARGET_ENV{CONFIG_BAKER_EMBEDDED_BOOL}\n"
                "Client.BootstrapFileTarget = $TARGET_FILE{config_baker_embedded_int.txt}\n"));

        TestRig rig {"Test.fomain", temp_dir};
        rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeMetadataBlob({{"Setting", {{"Server.CustomEnvTarget", "bool"}, {"Server.BootstrapEnvTarget", "bool"}}}}));
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Setting", {{"Client.CustomFileTarget", "int32"}, {"Client.BootstrapFileTarget", "int32"}}}}));

        ConfigBaker baker {rig.MakeContext("ConfigPack")};
        REQUIRE_NOTHROW(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""));

        string server_config {rig.GetOutputText("(Root).fomain-server")};
        string client_config {rig.GetOutputText("(Root).fomain-client")};
        string embedded_server_config {rig.GetOutputText("(Root).fomain-embedded-server")};
        string embedded_client_config {rig.GetOutputText("(Root).fomain-embedded-client")};
        CHECK(server_config.find("Server.CustomEnvTarget=$TARGET_ENV{CONFIG_BAKER_BOOL}\n") != string::npos);
        CHECK(client_config.find("Client.CustomFileTarget=$TARGET_FILE{config_baker_int.txt}\n") != string::npos);
        CHECK(server_config.find("Client.CustomFileTarget=$TARGET_FILE{config_baker_int.txt}\n") != string::npos);
        CHECK(embedded_server_config.find("Server.BootstrapEnvTarget=$TARGET_ENV{CONFIG_BAKER_EMBEDDED_BOOL}\n") != string::npos);
        CHECK(embedded_client_config.find("Client.BootstrapFileTarget=$TARGET_FILE{config_baker_embedded_int.txt}\n") != string::npos);
        CHECK(embedded_server_config.find("Client.BootstrapFileTarget=$TARGET_FILE{config_baker_embedded_int.txt}\n") != string::npos);
        CHECK(embedded_client_config.find("Server.BootstrapEnvTarget=") == string::npos);
        CHECK(embedded_server_config.find("Server.CustomEnvTarget=") == string::npos);
        CHECK(embedded_client_config.find("Client.CustomFileTarget=") == string::npos);

        std::error_code ec {};
        std::filesystem::remove_all(fs::make_path(temp_dir), ec);
    }

    SECTION("MasterBakerWritesMetadataAndConfigurationsToTheSamePack")
    {
        string temp_dir {MakeConfigBakerTempDir()};
        auto cleanup {scope_exit([&temp_dir]() noexcept { safe_call([&temp_dir] { (void)fs::remove_dir_tree(temp_dir); }); })};
        string output_dir {strex(temp_dir).combine_path("output").str()};
        string config_path {strex(temp_dir).combine_path("Test.fomain").str()};
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("input/Fixture.cs").str(), string_view {"// Metadata input\n"}));
        string config {MakeCompleteConfigBakerConfig() + strex("Baking.BakeOutput={}\nBaking.SingleThreadBaking=true\n", output_dir).str() +
            "[ResourcePack]\nName=Metadata\nInputDirs=input\nIncludePatterns=*\nBakers=Metadata Config ClientConfig\n"
            "[SubConfig]\nName=Child\nCommon.GameVersion=1.2.3\n"};
        REQUIRE(fs::write_file(config_path, string_view {config}));
        auto settings {GlobalSettings(true)};
        settings.SetupConfigAtPath("Test.fomain", temp_dir);

        MasterBaker baker {&settings};
        REQUIRE(baker.BakeAll());

        string pack_dir {strex(output_dir).combine_path("Metadata").str()};
        for (string_view role : {"client", "server", "mapper"}) {
            CHECK(fs::exists(strex(pack_dir).combine_path(strex("Metadata.fometa-{}", role)).str()));
        }
        for (string_view role : {"client", "server"}) {
            CHECK(fs::exists(strex(pack_dir).combine_path(strex("(Root).fomain-{}", role)).str()));
            CHECK(fs::exists(strex(pack_dir).combine_path(strex("(Root).fomain-embedded-{}", role)).str()));
            auto content {fs::read_file(strex(pack_dir).combine_path(strex("Child.fomain-{}", role)).str())};
            REQUIRE(content);
            ConfigFile profile {*content};
            CHECK(profile.GetAsStr("", "Common.GameVersion") == "1.2.3");
            CHECK(fs::exists(strex(pack_dir).combine_path(strex("Child.fomain-embedded-{}", role)).str()));
            auto embedded_content {fs::read_file(strex(pack_dir).combine_path(strex("Child.fomain-embedded-{}", role)).str())};
            REQUIRE(embedded_content);
            ConfigFile embedded_profile {*embedded_content};
            CHECK(embedded_profile.GetAsStr("", "Common.GameVersion") == "1.2.3");
        }
        CHECK_FALSE(fs::exists(strex(output_dir).combine_path("Configs").str()));
    }

    SECTION("SetupBakersReturnsClientConfig")
    {
        TestRig rig {};
        ConfigFile config {"[ResourcePack]\nName=Metadata\nBakers=Config ClientConfig\n"};
        rig.Settings.ApplyConfigFile(config, "");
        auto bakers {MakeRequestedBakers({string(ClientConfigBaker::NAME)}, rig, "Metadata")};
        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == ClientConfigBaker::NAME);
        CHECK(bakers.front()->GetOrder() == 2);
    }

    SECTION("SetupBakersReturnsRequestedBaker")
    {
        TestRig rig {};
        auto bakers {MakeRequestedBakers({string(ConfigBaker::NAME)}, rig)};

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == ConfigBaker::NAME);
        CHECK(bakers.front()->GetOrder() == 2);
    }
#endif
}

#if FO_ANGELSCRIPT_SCRIPTING
TEST_CASE("BakerDataSourceResolvesConfigsBakedOnDemand")
{
    string temp_dir = MakeConfigBakerTempDir();
    string metadata_input_path = strex(temp_dir).combine_path("metadata_input/Metadata.fos").str();
    string output_dir = strex(temp_dir).combine_path("output").str();
    string config_output_path = strex(output_dir).combine_path("Configs/(Root).fomain-server").str();

    REQUIRE(fs::write_file(metadata_input_path, string_view {"void Placeholder() { }"}));

    auto source_time = std::filesystem::file_time_type::clock::now() - std::chrono::minutes {2};
    std::filesystem::last_write_time(fs::make_path(metadata_input_path), source_time);

    for (string_view target : {"server", "client", "mapper"}) {
        string metadata_output_path = strex(output_dir).combine_path(strex("Metadata/Metadata.fometa-{}", target).str()).str();
        REQUIRE(fs::write_file(metadata_output_path, BakerTests::MakeEmptyMetadataBlob()));
        std::filesystem::last_write_time(fs::make_path(metadata_output_path), source_time + std::chrono::minutes {1});
    }

    REQUIRE(fs::write_file(strex(temp_dir).combine_path("Test.fomain").str(), MakeCompleteConfigBakerConfig() + strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Metadata
InputDirs = metadata_input
IncludePatterns = **/*.fos
Bakers = {}
[ResourcePack]
Name = Configs
Bakers = {}
ServerOnly = True
)",
                                                                                                               output_dir, MetadataBaker::NAME, ConfigBaker::NAME)
                                                                                                               .str()));

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();
    settings.ApplyConfigAtPath("Test.fomain", temp_dir);

    BakerDataSource data_source {&settings};
    REQUIRE(data_source.IsFileExists("(Root).fomain-server"));

    size_t size = 0;
    uint64_t write_time = 0;

    REQUIRE(data_source.GetFileInfo("(Root).fomain-server", size, write_time));
    REQUIRE(fs::exists(config_output_path));
    CHECK(size != 0);
    CHECK(write_time == fs::last_write_time(config_output_path));

    auto later_time = std::filesystem::file_time_type::clock::now() + std::chrono::minutes {1};
    std::filesystem::last_write_time(fs::make_path(config_output_path), later_time);
    uint64_t pinned_output_time = fs::last_write_time(config_output_path);

    REQUIRE(data_source.GetFileInfo("(Root).fomain-server", size, write_time));
    CHECK(fs::last_write_time(config_output_path) == pinned_output_time);

    std::error_code ec;
    std::filesystem::remove_all(fs::make_path(temp_dir), ec);
}
#endif

FO_END_NAMESPACE
