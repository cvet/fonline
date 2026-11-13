// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "Application.h"
#include "CacheStorage.h"
#include "ConfigFile.h"
#include "DiskFileSystem.h"
#include "ResourcePack.h"
#include "Settings.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static auto MakeTempSettingsDir(string_view name) -> string
{
    auto base {std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count())};
    return fs::path_to_string(base);
}

static auto ReadTestResourceConfig(const GlobalSettings& settings, bool required) -> optional<string>
{
    FileSystem metadata_source {};

    if (settings.Common.Packaged) {
        AddClientPackSource(metadata_source, settings, METADATA_PACK_NAME, !required);
    }
    else {
        metadata_source.AddPackSource(false, settings.Baking.BakeOutput, METADATA_PACK_NAME, !required);
    }

    return ReadResourceConfig(metadata_source, required);
}

TEST_CASE("SettingPathParsing")
{
    CHECK(SplitSettingPath("Android.GradleDependency.Sample") == (vector<string> {"Android", "GradleDependency", "Sample"}));
    CHECK(SplitSettingPath("A_B.A.B") == (vector<string> {"A_B", "A", "B"}));
    CHECK(SplitSettingPath("Ungrouped") == (vector<string> {"Ungrouped"}));

    for (string_view name : {"", ".Name", "Name.", "Domain..Name", ".", "Domain...Name"}) {
        CAPTURE(name);
        CHECK_THROWS_AS(SplitSettingPath(name), SettingsException);
    }
}

TEST_CASE("CommandLineArgsSettingLookup")
{
    SECTION("SettingNameNormalizationMatchesTheSettingsParser")
    {
        vector<string> values {"lf_tests", "--Render.Sleep ", "-1"};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        CommandLineArgs args {argv};
        GlobalSettings settings {false};
        settings.SetupDefaultSettings();
        settings.ApplyCommandLine(args);

        CHECK(settings.Render.Sleep == -1);
        CHECK(args.GetSettingValue(SettingNames::Render_Sleep) == "-1");
    }

    SECTION("LastOccurrenceAndExplicitText")
    {
        vector<string> values {"lf_tests", "--Probe.Mode", "False", "-Probe.Mode", "True", "--Probe.Number", "-1", "--Probe.Empty", "", "--Probe.Text", " C:\\path\\\"quoted\" "};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        CommandLineArgs args {argv};

        CHECK(args.GetSettingValue("Probe.Mode") == "True");
        CHECK(args.GetSettingValue("Probe.Number") == "-1");
        CHECK(args.GetSettingValue("Probe.Empty") == "");
        CHECK(args.GetSettingValue("Probe.Text") == " C:\\path\\\"quoted\" ");
        CHECK_FALSE(args.GetSettingValue("Probe"));
        CHECK_FALSE(args.GetSettingValue("probe.Mode"));
        CHECK_FALSE(args.GetSettingValue("--Probe.Mode"));
    }

    SECTION("ImplicitValuesBeforeAnotherOptionAndAtEnd")
    {
        vector<string> values {"lf_tests", "--Probe.Flag", "--Probe.Other", "value", "-Probe.Last"};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        CommandLineArgs args {argv};

        CHECK(args.GetSettingValue("Probe.Flag") == "1");
        CHECK(args.GetSettingValue("Probe.Other") == "value");
        CHECK(args.GetSettingValue("Probe.Last") == "1");
        CHECK_FALSE(args.GetSettingValue("Probe.Absent"));
        CHECK_FALSE(CommandLineArgs {}.GetSettingValue("Probe.Flag"));
    }

    SECTION("EarlyBooleanMatchesFinalCommandLineOverride")
    {
        vector<string> values {"lf_tests", "--Common.NonInteractive", "False", "-Common.NonInteractive"};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        CommandLineArgs args {argv};
        GlobalSettings settings {false};
        settings.SetupDefaultSettings();
        settings.ApplyCommandLine(args);

        CHECK(args.GetSettingValue(SettingNames::Common_NonInteractive) == "1");
        CHECK(settings.Common.NonInteractive);
        CHECK(strvex(args.GetSettingValue(SettingNames::Common_NonInteractive).value_or("0")).to_bool() == settings.Common.NonInteractive);
    }

    SECTION("ExplicitOnlyPolicySkipsValuelessOccurrences")
    {
        // A bare flag after an explicit path must not answer the explicit-only getter: the earlier
        // value is the one the caller asked for, while the ordinary policy still takes the last entry
        vector<string> values {"lf_tests", "--ClientLibPath", "old.dll", "--ClientLibPath"};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        CommandLineArgs args {argv};

        CHECK(args.GetSettingValue(CommandLineSettings::ClientLibPath, std::nullopt) == "old.dll");
        CHECK(args.GetSettingValue(CommandLineSettings::ClientLibPath) == "1");

        vector<string> empty_values {"lf_tests", "--ClientLibPath", ""};
        vector<CommandLineArg> empty_argv {};

        for (string& value : empty_values) {
            empty_argv.emplace_back(value.data());
        }

        CommandLineArgs empty_args {empty_argv};

        CHECK(empty_args.GetSettingValue(CommandLineSettings::ClientLibPath, std::nullopt) == "");
        CHECK(empty_args.GetSettingValue(CommandLineSettings::ClientLibPath) == "");
    }

    SECTION("GetSettingsKeepsOrderDuplicatesAndValueKinds")
    {
        vector<string> values {"lf_tests", "--ApplySubConfig", "First", "--ApplySubConfig", "Second", "--Probe.Flag", "--Probe.Next", "", "--Probe.Raw", " C:\\path\\\"quoted\" "};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        CommandLineArgs args {argv};

        vector<CommandLineSetting> sub_configs {args.GetSettings(CommandLineSettings::ApplySubConfig)};

        REQUIRE(sub_configs.size() == 2);
        CHECK(sub_configs[0].Name == CommandLineSettings::ApplySubConfig);
        CHECK(sub_configs[0].Value == "First");
        CHECK(sub_configs[1].Name == CommandLineSettings::ApplySubConfig);
        CHECK(sub_configs[1].Value == "Second");

        // The whole enumeration keeps every distinct entry in command-line order, with raw borrowed
        // values and implicit flags told apart from explicit empty strings
        vector<CommandLineSetting> all {args.GetSettings()};

        REQUIRE(all.size() == 5);
        CHECK(all[0].Name == CommandLineSettings::ApplySubConfig);
        CHECK(all[0].Value == "First");
        CHECK(all[1].Name == CommandLineSettings::ApplySubConfig);
        CHECK(all[1].Value == "Second");
        CHECK(all[2].Name == "Probe.Flag");
        CHECK_FALSE(all[2].Value.has_value());
        CHECK(all[3].Name == "Probe.Next");
        REQUIRE(all[3].Value.has_value());
        CHECK(all[3].Value->empty());
        CHECK(all[4].Name == "Probe.Raw");
        CHECK(all[4].Value == " C:\\path\\\"quoted\" ");

        CHECK(args.GetSettings("probe").empty());
        CHECK(args.GetSettings("--ApplySubConfig").empty());
    }

    SECTION("FlagPresenceMatchesWholeOptionNames")
    {
        vector<string> values {"lf_tests", "--Probe.Path", "C:\\tmp\\--server-service-start", "--server-service-delete-extra", "--fork", "False"};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        CommandLineArgs args {argv};

        CHECK_FALSE(args.GetSettingValue(CommandLineSettings::ServerServiceStart).has_value());
        CHECK_FALSE(args.GetSettingValue(CommandLineSettings::ServerServiceDelete).has_value());
        CHECK(args.GetSettingValue(CommandLineSettings::Fork).has_value());
        CHECK(args.GetSettingValue(CommandLineSettings::Fork) == "False");
    }
}

TEST_CASE("Settings")
{
    SECTION("ProjectDayCurvesAreNotEngineSettings")
    {
        GlobalSettings settings {false};

        for (string name : {"View.GlobalDayColorTime", "View.GlobalDayColor"}) {
            CAPTURE(name);
            CHECK_NOTHROW(settings.SetRuntimeSetting(name, "project-owned"));
            CHECK(settings.GetCustomSetting(name) == "project-owned");
        }
    }

    SECTION("GetResourcePacksThrowsWhenNothingApplied")
    {
        GlobalSettings settings {false};

        CHECK_THROWS_AS(settings.GetResourcePacks(), SettingsException);
    }

    SECTION("SubConfigMultiParentMergesLeftToRight")
    {
        GlobalSettings settings {false};
        ConfigFile config {"[SubConfig]\n"
                           "Name = BaseA\n"
                           "OnlyA = fromA\n"
                           "Shared = fromA\n"
                           "[SubConfig]\n"
                           "Name = BaseB\n"
                           "OnlyB = fromB\n"
                           "Shared = fromB\n"
                           "[SubConfig]\n"
                           "Name = Mixed\n"
                           "Parent = BaseA BaseB\n"
                           "Own = child\n"};

        settings.ApplyConfigFile(config, "cfg");
        settings.ApplySubConfigSection("Mixed");

        // Multiple parents merge (not replace): both parents' unique keys survive, later parents
        // override earlier ones on shared keys, and the child's own settings sit on top
        CHECK(settings.GetCustomSetting("OnlyA") == "fromA");
        CHECK(settings.GetCustomSetting("OnlyB") == "fromB");
        CHECK(settings.GetCustomSetting("Shared") == "fromB");
        CHECK(settings.GetCustomSetting("Own") == "child");
    }

    SECTION("ApplyConfigFileParsesSubConfigsAndResourcePacks")
    {
        GlobalSettings settings {false};
        ConfigFile config {"UnknownSetting = root\n"
                           "[SubConfig]\n"
                           "Name = Base\n"
                           "Shared = parent\n"
                           "Mode = base\n"
                           "[SubConfig]\n"
                           "Name = Child\n"
                           "Parent = Base\n"
                           "Mode = child\n"
                           "Leaf = extra\n"
                           "[ResourcePack]\n"
                           "Name = CommonPack\n"
                           "InputDirs = dirA dirB\n"
                           "InputFiles = fileA fileB\n"
                           "IncludePatterns = **/*.fos *.fos\n"
                           "ExcludePatterns = **/Generated/** **/_*\n"
                           "Bakers = BakerA BakerB\n"
                           "[ResourcePack]\n"
                           "Name = ServerPack\n"
                           "InputDirs = server_dir\n"
                           "ServerOnly = true\n"};

        settings.ApplyConfigFile(config, "cfg");

        CHECK(settings.GetCustomSetting("UnknownSetting") == "root");
        REQUIRE(settings.GetSubConfigs().size() == 2);
        CHECK(settings.GetSubConfigs()[0].Name == "Base");
        CHECK(settings.GetSubConfigs()[1].Name == "Child");

        settings.ApplySubConfigSection("Child");
        CHECK(settings.GetCustomSetting("Shared") == "parent");
        CHECK(settings.GetCustomSetting("Mode") == "child");
        CHECK(settings.GetCustomSetting("Leaf") == "extra");

        auto packs {settings.GetResourcePacks()};
        REQUIRE(packs.size() == 2);
        CHECK(packs[0].Name == "CommonPack");
        REQUIRE(packs[0].InputDirs.size() == 2);
        CHECK(packs[0].InputDirs[0] == strex("cfg").combine_path("dirA").str());
        CHECK(packs[0].InputDirs[1] == strex("cfg").combine_path("dirB").str());
        REQUIRE(packs[0].InputFiles.size() == 2);
        CHECK(packs[0].InputFiles[0] == strex("cfg").combine_path("fileA").str());
        CHECK(packs[0].InputFiles[1] == strex("cfg").combine_path("fileB").str());
        REQUIRE(packs[0].IncludePatterns.size() == 2);
        CHECK(packs[0].IncludePatterns[0] == "**/*.fos");
        CHECK(packs[0].IncludePatterns[1] == "*.fos");
        REQUIRE(packs[0].ExcludePatterns.size() == 2);
        CHECK(packs[0].ExcludePatterns[0] == "**/Generated/**");
        CHECK(packs[0].ExcludePatterns[1] == "**/_*");
        REQUIRE(packs[0].Bakers.size() == 2);
        CHECK(packs[0].Bakers[0] == "BakerA");
        CHECK(packs[0].Bakers[1] == "BakerB");
        CHECK_FALSE(packs[0].ServerOnly);
        CHECK(packs[1].ServerOnly);
        CHECK_FALSE(packs[1].ClientOnly);
    }

    SECTION("IgnoreInputDirsFromASubConfigLeaveTheDirectoryOutOfEveryPack")
    {
        ConfigFile config {"[SubConfig]\n"
                           "Name = Shipping\n"
                           "Baking.IgnoreInputDirs = scripts/tests\n"
                           "[ResourcePack]\n"
                           "Name = ScriptPack\n"
                           "InputDirs = scripts scripts/tests\n"
                           "[ResourcePack]\n"
                           "Name = MetadataPack\n"
                           "InputDirs = scripts/tests scripts\n"
                           "[ResourcePack]\n"
                           "Name = ArtPack\n"
                           "InputDirs = art\n"};
        string scripts_dir {strex("cfg").combine_path("scripts").str()};
        string tests_dir {strex("cfg").combine_path("scripts/tests").str()};
        string art_dir {strex("cfg").combine_path("art").str()};

        GlobalSettings development {false};
        development.SetupDefaultSettings();
        development.ApplyConfigFile(config, "cfg");
        development.ApplyAutoSettings({});

        CHECK(development.GetResourcePacks()[0].InputDirs == vector<string> {scripts_dir, tests_dir});
        CHECK(development.GetResourcePacks()[1].InputDirs == vector<string> {tests_dir, scripts_dir});

        GlobalSettings shipping {false};
        shipping.SetupDefaultSettings();
        shipping.ApplyConfigFile(config, "cfg");
        shipping.ApplySubConfigSection("Shipping");

        // Applying twice matters: the second pass must not report the already left out directory as unknown
        shipping.ApplyAutoSettings({});
        shipping.ApplyAutoSettings({});

        REQUIRE(shipping.GetResourcePacks().size() == 3);
        CHECK(shipping.GetResourcePacks()[0].InputDirs == vector<string> {scripts_dir});
        CHECK(shipping.GetResourcePacks()[1].InputDirs == vector<string> {scripts_dir});
        CHECK(shipping.GetResourcePacks()[2].InputDirs == vector<string> {art_dir});
    }

    SECTION("PackDeclarationsWrittenForAPackagedConfigRestoreEveryPackList")
    {
        ConfigFile authored {"[SubConfig]\n"
                             "Name = Shipping\n"
                             "Baking.IgnoreInputDirs = scripts/tests\n"
                             "[ResourcePack]\n"
                             "Name = Scripts\n"
                             "InputDirs = scripts scripts/tests\n"
                             "[ResourcePack]\n"
                             "Name = ServerData\n"
                             "InputDirs = server\n"
                             "ServerOnly = True\n"
                             "[ResourcePack]\n"
                             "Name = Art\n"
                             "InputDirs = art\n"
                             "ClientOnly = True\n"
                             "[ResourcePack]\n"
                             "Name = MapperData\n"
                             "InputDirs = mapper\n"
                             "MapperOnly = True\n"};

        GlobalSettings baking {false};
        baking.SetupDefaultSettings();
        baking.ApplyConfigFile(authored, "cfg");
        baking.ApplySubConfigSection("Shipping");
        baking.ApplyAutoSettings({});

        // What a packaged application sees: the baked settings, the ignored inputs among them, then the pack sections
        ConfigFile baked {strex("Baking.IgnoreInputDirs=scripts/tests\n{}", baking.GetResourcePackDeclarations()).str()};
        GlobalSettings packaged {false};
        packaged.SetupDefaultSettings();
        packaged.ApplyConfigFile(baked, "");

        REQUIRE_NOTHROW(packaged.ApplyAutoSettings({}));
        CHECK(packaged.GetServerResourcePacks() == baking.GetServerResourcePacks());
        CHECK(packaged.GetClientResourcePacks() == baking.GetClientResourcePacks());
        CHECK(packaged.GetMapperResourcePacks() == baking.GetMapperResourcePacks());
        CHECK(packaged.GetServerResourcePacks() == vector<string> {"Scripts", "ServerData"});
        CHECK(packaged.GetClientResourcePacks() == vector<string> {"Scripts", "Art"});
        CHECK(packaged.GetMapperResourcePacks() == vector<string> {"MapperData"});
        CHECK(std::ranges::all_of(packaged.GetResourcePacks(), [](const ResourcePackInfo& pack) { return pack.InputDirs.empty(); }));
    }

    SECTION("IgnoredInputDirThatNoPackReadsIsRejected")
    {
        ConfigFile config {"[SubConfig]\n"
                           "Name = Misspelled\n"
                           "Baking.IgnoreInputDirs = scripts/test\n"
                           "[ResourcePack]\n"
                           "Name = ScriptPack\n"
                           "InputDirs = scripts scripts/tests\n"};

        GlobalSettings settings {false};
        settings.SetupDefaultSettings();
        settings.ApplyConfigFile(config, "cfg");
        settings.ApplySubConfigSection("Misspelled");

        CHECK_THROWS_AS(settings.ApplyAutoSettings({}), SettingsException);
        CHECK(settings.GetResourcePacks()[0].InputDirs.size() == 2);
    }

    SECTION("ApplyCommandLineSetsCustomValuesAndImplicitFlags")
    {
        GlobalSettings settings {false};
        char arg0[] {"lf_tests"};
        char arg1[] {"--CustomCli"};
        char arg2[] {"123"};
        char arg3[] {"--FlagOnly"};
        vector<CommandLineArg> argv {arg0, arg1, arg2, arg3};

        settings.ApplyCommandLine(CommandLineArgs {argv});

        CHECK(settings.GetCustomSetting("CustomCli") == "123");
        CHECK(settings.GetCustomSetting("FlagOnly") == "1");
    }

    SECTION("CommandLineOptionIsADashBeforeASettingName")
    {
        CHECK(CommandLineArgs::IsOption("--Render.Sleep"));
        CHECK(CommandLineArgs::IsOption("-ApplySubConfig"));
        CHECK(CommandLineArgs::IsOption("--custom"));

        CHECK_FALSE(CommandLineArgs::IsOption("-1"));
        CHECK_FALSE(CommandLineArgs::IsOption("-0.5"));
        CHECK_FALSE(CommandLineArgs::IsOption("-.5"));
        CHECK_FALSE(CommandLineArgs::IsOption("-5 5 10 10"));
        CHECK_FALSE(CommandLineArgs::IsOption("-"));
        CHECK_FALSE(CommandLineArgs::IsOption("--"));
        CHECK_FALSE(CommandLineArgs::IsOption(""));
        CHECK_FALSE(CommandLineArgs::IsOption("+Tag"));
        CHECK_FALSE(CommandLineArgs::IsOption("Value"));
    }

    SECTION("ApplyCommandLineKeepsNegativeValues")
    {
        // Read as the next option, a negative value would leave its setting at the implicit flag value 1 and
        // add a stray custom setting named after its digits
        GlobalSettings settings {false};
        settings.SetupDefaultSettings();

        char arg0[] {"lf_tests"};
        char arg1[] {"--Render.FixedFPS"};
        char arg2[] {"60"};
        char arg3[] {"--Render.Sleep"};
        char arg4[] {"-1"};
        char arg5[] {"--Probe.Offset"};
        char arg6[] {"-0.5"};
        char arg7[] {"--Probe.Area"};
        char arg8[] {"-5 5 10 10"};
        char arg9[] {"--Probe.Flag"};
        vector<CommandLineArg> argv {arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9};

        settings.ApplyCommandLine(CommandLineArgs {argv});

        CHECK(settings.Render.FixedFPS == 60);
        CHECK(settings.Render.Sleep == -1);
        CHECK(settings.GetCustomSetting("Probe.Offset") == "-0.5");
        CHECK(settings.GetCustomSetting("Probe.Area") == "-5 5 10 10");
        CHECK(settings.GetCustomSetting("Probe.Flag") == "1");
        CHECK_FALSE(settings.FindCustomSetting("1"));
        CHECK_FALSE(settings.FindCustomSetting("0.5"));
        CHECK_FALSE(settings.FindCustomSetting("5 5 10 10"));
    }

    SECTION("CommandLineArgsAcceptEmptyNativeArgv")
    {
        CommandLineArgs args {0, nullptr};

        CHECK(args.empty());
    }

    SECTION("ProgramArgsKeepTheArgumentsInUtf8")
    {
        char arg0[] {"lf_tests"};
        char arg1[] {"--Common.UserWritablePath"};
        char arg2[] {"\xD0\xA2\xD0\xB5\xD1\x81\xD1\x82"};
        char* argv[] {arg0, arg1, arg2};

        ProgramArgs program_args {3, argv};
        CommandLineArgs args {program_args.GetArgs()};

#if FO_WINDOWS
        // The console argv is not trusted there: the arguments come from the wide command line of this process
        REQUIRE_FALSE(args.empty());
        auto exe_path {platform::get_exe_path()};
        REQUIRE(exe_path.has_value());
        CHECK(strex(args.Get(0)).extract_file_name().erase_file_extension().lower().str() == strex(exe_path.value()).extract_file_name().erase_file_extension().lower().str());
#else
        REQUIRE(args.size() == 3);
        CHECK(args.Get(0) == "lf_tests");
        CHECK(args.Get(2) == "\xD0\xA2\xD0\xB5\xD1\x81\xD1\x82");
#endif
    }

    SECTION("ApplyCommandLineMasksSecretValuesInLog")
    {
        // Capture the "Set <name> to <value>" lines emitted by the logging pass
        string captured {};
        logging::set_callback("settings_secret_redaction_test", [&captured](logging::type, string_view message, nptr<const stack_trace::catched_data>) { captured += message; });
        auto remove_callback {scope_exit([]() noexcept { logging::set_callback("settings_secret_redaction_test", nullptr); })};

        GlobalSettings settings {false};
        // Real flow logs command-line overrides only after defaults (and the config) are applied, so the
        // Common.SecretSettingTokens list (default includes "token") is populated by then
        settings.SetupDefaultSettings();

        char arg0[] {"lf_tests"};
        char arg1[] {"--Probe.AccessToken"};
        char arg2[] {"super-secret-value"};
        char arg3[] {"--Common.GameName"};
        char arg4[] {"RedactionProbe"};
        char* argv[] {arg0, arg1, arg2, arg3, arg4};

        settings.ApplyCommandLine(CommandLineArgs {5, argv});

        // A name matching a secret token (Common.SecretSettingTokens, default includes "token") is masked;
        // the credential value itself must never reach the log
        CHECK(captured.find("Set Probe.AccessToken to ***") != string::npos);
        CHECK(captured.find("super-secret-value") == string::npos);

        // A non-secret name is logged verbatim, and both values are still applied
        CHECK(captured.find("Set Common.GameName to RedactionProbe") != string::npos);
        CHECK(settings.GetCustomSetting("Probe.AccessToken") == "super-secret-value");
        CHECK(settings.Common.GameName == "RedactionProbe");
    }

    SECTION("ApplyCommandLineAppendAccumulatesPerCall")
    {
        // '+'-prefixed overrides append, so applying one command line twice doubles the result — the hazard the
        // single-application flow exists to avoid
        GlobalSettings settings {false};
        char arg0[] {"lf_tests"};
        char arg1[] {"--Common.GameName"};
        char arg2[] {"+Tag"};
        char* argv[] {arg0, arg1, arg2};

        settings.ApplyCommandLine(CommandLineArgs {3, argv});
        CHECK(settings.Common.GameName == "Tag");

        // A second pass over the same object appends again — what the two-pass flow used to do
        settings.ApplyCommandLine(CommandLineArgs {3, argv});
        CHECK(settings.Common.GameName == "Tag Tag");
    }

    SECTION("StringSettingsKeepBackslashesAsWritten")
    {
        // Each path holds a sequence the AnyData coded-string grammar rewrites: \n, \r and a doubled backslash
        string unc_path {R"(\\server\share\none\runs\Baking)"};
        string drive_path {R"(C:\work\none\Baking)"};

        GlobalSettings from_command_line {false};
        from_command_line.SetupDefaultSettings();
        string path_arg {unc_path};
        std::array<char*, 3> argv {const_cast<char*>("app"), const_cast<char*>("--Baking.BakeOutput"), path_arg.data()};
        from_command_line.ApplyCommandLine(CommandLineArgs {numeric_cast<int32_t>(argv.size()), argv.data()});

        CHECK(from_command_line.Baking.BakeOutput == unc_path);

        GlobalSettings from_config {false};
        from_config.SetupDefaultSettings();
        ConfigFile config {strex("Baking.BakeOutput = {}\nManagedScript.Dirs = {}\t{}\n", drive_path, drive_path, unc_path).str()};
        from_config.ApplyConfigFile(config, "");

        CHECK(from_config.Baking.BakeOutput == drive_path);
        CHECK(from_config.ManagedScript.Dirs == vector<string> {drive_path, unc_path});
    }

    SECTION("StringSettingsSurviveTheBakedConfigRoundTrip")
    {
        // The config baker writes Save() as key=value lines that a packaged application parses back, so a value
        // rewritten on read would drift further with every bake
        string unc_path {R"(\\server\share\none\runs\Baking)"};

        GlobalSettings baking {true};
        baking.SetupDefaultSettings();
        ConfigFile authored {strex("Baking.BakeOutput = {}\nManagedScript.Dirs = {} Scripts\n", unc_path, unc_path).str()};
        baking.ApplyConfigFile(authored, "");
        auto saved {baking.Save()};

        GlobalSettings packaged {false};
        packaged.SetupDefaultSettings();
        ConfigFile baked {strex("Baking.BakeOutput={}\nManagedScript.Dirs={}\n", saved.at(string(SettingNames::Baking_BakeOutput)), saved.at(string(SettingNames::ManagedScript_Dirs))).str()};
        packaged.ApplyConfigFile(baked, "");

        CHECK(packaged.Baking.BakeOutput == unc_path);
        CHECK(packaged.ManagedScript.Dirs == vector<string> {unc_path, "Scripts"});
    }

    SECTION("SetupConfigAtPathResolvesFileVariables")
    {
        string temp_dir {MakeTempSettingsDir("settings_config")};
        bool removed_before {fs::remove_dir_tree(temp_dir)};
        ignore_unused(removed_before);

        REQUIRE(fs::write_file(strex(temp_dir).combine_path("payload.txt").str(), string_view {"  loaded value  "}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("main.fomain").str(), string_view {"ExternalValue = $FILE{payload.txt}\n"}));

        GlobalSettings settings {false};
        settings.SetupConfigAtPath("main.fomain", temp_dir);

        CHECK(settings.GetAppliedConfigs().size() == 1);
        CHECK(settings.GetCustomSetting("ExternalValue") == "loaded value");

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("SetupConfigAtPathThrowsForMissingConfig")
    {
        GlobalSettings settings {false};

        CHECK_THROWS_AS(settings.SetupConfigAtPath("missing.fomain", "/tmp/not_there"), SettingsException);
    }

    SECTION("FindCustomSettingReturnsNullableLookup")
    {
        GlobalSettings settings {false};

        auto missing {settings.FindCustomSetting("Missing")};
        CHECK_FALSE(static_cast<bool>(missing));
        CHECK(settings.GetCustomSetting("Missing").empty());

        settings.SetCustomSetting("Present", any_t(string("value")));
        auto present {settings.FindCustomSetting("Present")};

        REQUIRE(static_cast<bool>(present));
        CHECK(*present == "value");
        CHECK(settings.GetCustomSetting("Present") == "value");
    }

    SECTION("RuntimeSettingRejectsEngineSettingsAndKeepsCustomValues")
    {
        GlobalSettings settings {false};
        settings.SetupDefaultSettings();

        // Every engine setting is read-only at runtime: a configured value is a knob someone turned, and the
        // system that owns a value which changes while the game runs keeps that value itself
        bool original_scroll {settings.Hex.WindowedMouseScroll};
        CHECK_THROWS_AS(settings.SetRuntimeSetting(string(SettingNames::Hex_WindowedMouseScroll), "True"), SettingsException);
        CHECK(settings.Hex.WindowedMouseScroll == original_scroll);
        CHECK_FALSE(static_cast<bool>(settings.FindCustomSetting(SettingNames::Hex_WindowedMouseScroll)));

        // The group is part of the name: a bare short name reaches no engine setting and is kept as a custom
        // one, which is what lets two groups declare the same short name without one answering for the other
        settings.SetRuntimeSetting("WindowedMouseScroll", "False");
        CHECK(settings.Hex.WindowedMouseScroll == original_scroll);
        CHECK(settings.GetCustomSetting("WindowedMouseScroll") == "False");
        CHECK(settings.GetRuntimeSetting("WindowedMouseScroll") == "False");

        // A custom write moves the generation, a rejected engine write does not: the managed typed-setting cell
        // re-parses a custom value only when this counter moved
        uint64_t generation_before {settings.GetCustomSettingsGeneration()};
        settings.SetRuntimeSetting("Project.RuntimeValue", "value");
        CHECK(settings.GetCustomSetting("Project.RuntimeValue") == "value");
        CHECK(settings.GetCustomSettingsGeneration() == generation_before + 1);
        CHECK_THROWS_AS(settings.SetRuntimeSetting(string(SettingNames::Hex_WindowedMouseScroll), "False"), SettingsException);
        CHECK(settings.GetCustomSettingsGeneration() == generation_before + 1);
        settings.SetCustomSetting("Project.RuntimeValue", any_t("other"));
        CHECK(settings.GetCustomSettingsGeneration() == generation_before + 2);

        string original_game_name {settings.Common.GameName};
        CHECK_THROWS_AS(settings.SetRuntimeSetting(string(SettingNames::Common_GameName), "Changed"), SettingsException);
        CHECK(settings.Common.GameName == original_game_name);
        CHECK_FALSE(static_cast<bool>(settings.FindCustomSetting(SettingNames::Common_GameName)));

        string name_collision {"Common.GameName"};
        name_collision.push_back('\0');
        name_collision += "Custom";
        REQUIRE(const_hash(name_collision.c_str()) == const_hash(SettingNames::Common_GameName.data()));
        settings.SetRuntimeSetting(name_collision, "custom");
        CHECK(settings.Common.GameName == original_game_name);
        CHECK(settings.GetCustomSetting(name_collision) == "custom");
        CHECK(settings.GetRuntimeSetting(name_collision) == "custom");
        CHECK(settings.GetRuntimeSetting(string(SettingNames::Common_GameName)) == settings.Common.GameName);

        // A bare engine-setting name is not the setting either: it writes a custom value instead of throwing
        settings.SetRuntimeSetting("GameName", "Bare");
        CHECK(settings.Common.GameName == original_game_name);
        CHECK(settings.GetRuntimeSetting("GameName") == "Bare");
    }

    SECTION("ConfigKeyWithoutGroupDoesNotReachTheSetting")
    {
        // The dotted key is the whole name of a setting, so an unqualified key is an unknown setting and is
        // kept as a custom value; that is what leaves the short name free for another group to declare
        GlobalSettings settings {false};
        settings.SetupDefaultSettings();

        ConfigFile config {"ServerPort = 5555\nNetwork.ServerPort = 4444\n"};
        settings.ApplyConfigFile(config, "");

        CHECK(settings.Network.ServerPort == 4444);
        CHECK(settings.GetCustomSetting("ServerPort") == "5555");
    }

    SECTION("BakingModeSaveReturnsAppliedSettings")
    {
        GlobalSettings settings {true};
        ConfigFile config {"CustomSaved = value\n"};

        settings.ApplyConfigFile(config, "");

        auto saved {settings.Save()};
        auto it {saved.find("CustomSaved")};
        REQUIRE(it != saved.end());
        CHECK(it->second == "value");
    }

    SECTION("UpdateFilesInMemoryCanBeOverriddenBySubConfigs")
    {
        GlobalSettings settings {false};
        ConfigFile config {"ServerNetwork.UpdateFilesInMemory = False\n"
                           "[SubConfig]\n"
                           "Name = PublicGame\n"
                           "ServerNetwork.UpdateFilesInMemory = True\n"
                           "[SubConfig]\n"
                           "Name = Staging\n"
                           "Parent = PublicGame\n"
                           "ServerNetwork.UpdateFilesInMemory = False\n"};

        settings.ApplyConfigFile(config, "cfg");
        settings.ApplySubConfigSection("PublicGame");

        CHECK(settings.ServerNetwork.UpdateFilesInMemory);

        settings.ApplySubConfigSection("Staging");

        CHECK_FALSE(settings.ServerNetwork.UpdateFilesInMemory);
    }

    SECTION("WritableRootFromCommandLineCreatesTheDirectory")
    {
        // An explicit path on the command line is the one source that outranks everything, because it is
        // the only one available before any file has been read
        string root {MakeTempSettingsDir("settings_writable_root")};
        ignore_unused(fs::remove_dir_tree(root));

        string root_arg {root};
        std::array<char*, 3> argv {const_cast<char*>("app"), const_cast<char*>("--Common.UserWritablePath"), root_arg.data()};
        string resolved {ResolveWritableRoot(CommandLineArgs {numeric_cast<int32_t>(argv.size()), argv.data()})};

        CHECK(resolved == fs::resolve_path(root));
        CHECK(fs::is_dir(resolved));

        ignore_unused(fs::remove_dir_tree(root));
    }

    SECTION("CommandLinePassKeepsTheResolvedWritableRoot")
    {
        // The startup resolves "*" to the per-user data directory, and read again as a setting value it became "*"
        GlobalSettings settings {false};
        settings.SetupDefaultSettings();
        settings.ApplyWritableRoot("/home/player/.local/share/Game");

        char arg0[] {"app"};
        char arg1[] {"--Common.UserWritablePath"};
        char arg2[] {"*"};
        char* argv[] {arg0, arg1, arg2};
        settings.ApplyCommandLine(CommandLineArgs {3, argv});

        CHECK(settings.Common.UserWritablePath == "/home/player/.local/share/Game");
    }

    SECTION("WritableRootWithoutMarkerStaysInTheWorkingDirectory")
    {
        // No path argument and no installer marker next to the test exe: everything stays relative
        std::array<char*, 1> argv {const_cast<char*>("app")};

        CHECK(ResolveWritableRoot(CommandLineArgs {numeric_cast<int32_t>(argv.size()), argv.data()}).empty());
    }

    SECTION("WritableRootFailsafeReturnsToTheWorkingDirectory")
    {
        // A root whose parent is a regular file can't be created: the resolver must fail safe rather than
        // brick startup
        string temp_dir {MakeTempSettingsDir("settings_writable_blocker")};
        ignore_unused(fs::remove_dir_tree(temp_dir));
        string blocker {strex(temp_dir).combine_path("blocker").str()};
        REQUIRE(fs::write_file(blocker, string_view {"x"}));

        string blocked_root {strex(blocker).combine_path("sub").str()};
        std::array<char*, 3> argv {const_cast<char*>("app"), const_cast<char*>("--Common.UserWritablePath"), blocked_root.data()};

        CHECK(ResolveWritableRoot(CommandLineArgs {numeric_cast<int32_t>(argv.size()), argv.data()}).empty());

        ignore_unused(fs::remove_dir_tree(temp_dir));
    }

    SECTION("WritableRootTakesFirstValidExplicitPath")
    {
        // Valueless and empty occurrences are skipped, and the first explicit path that survives wins,
        // even when a later occurrence is valid too
        string temp_dir {MakeTempSettingsDir("settings_writable_first")};
        auto cleanup {scope_exit([temp_dir]() noexcept { ignore_unused(fs::remove_dir_tree(temp_dir)); })};
        REQUIRE(fs::create_directories(temp_dir));

        string root_a {strex(temp_dir).combine_path("root_a").str()};
        string root_b {strex(temp_dir).combine_path("root_b").str()};
        vector<string> values {"app", "--Common.UserWritablePath", "--Common.UserWritablePath", "", "--Common.UserWritablePath", root_a, "--Common.UserWritablePath", root_b};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        string resolved {ResolveWritableRoot(CommandLineArgs {argv})};

        CHECK(resolved == fs::resolve_path(root_a));
        CHECK(fs::is_dir(root_a));
        CHECK_FALSE(fs::is_dir(root_b));
    }

    SECTION("LoadAppSettingsAppliesSubConfigsInCommandLineOrder")
    {
        string temp_dir {MakeTempSettingsDir("settings_load_app_order")};
        auto cleanup {scope_exit([temp_dir]() noexcept { ignore_unused(fs::remove_dir_tree(temp_dir)); })};
        REQUIRE(fs::create_directories(temp_dir));

        string config_path {strex(temp_dir).combine_path("main.fomain").str()};
        REQUIRE(fs::write_file(config_path,
            string_view {"Common.GameName = Base\n"
                         "[SubConfig]\n"
                         "Name = First\n"
                         "Common.GameName = First\n"
                         "[SubConfig]\n"
                         "Name = Second\n"
                         "Common.GameName = Second\n"}));

        string writable_ordered {strex(temp_dir).combine_path("writable_ordered").str()};
        vector<string> ordered_values {"app", "--ApplyConfig", config_path, "--ApplySubConfig", "First", "--ApplySubConfig", "Second", "--Common.UserWritablePath", writable_ordered};
        vector<CommandLineArg> ordered_argv {};

        for (string& value : ordered_values) {
            ordered_argv.emplace_back(value.data());
        }

        GlobalSettings ordered {LoadAppSettings(CommandLineArgs {ordered_argv})};

        CHECK(ordered.Common.GameName == "Second");

        string writable_reversed {strex(temp_dir).combine_path("writable_reversed").str()};
        vector<string> reversed_values {"app", "--ApplyConfig", config_path, "--ApplySubConfig", "Second", "--ApplySubConfig", "First", "--Common.UserWritablePath", writable_reversed};
        vector<CommandLineArg> reversed_argv {};

        for (string& value : reversed_values) {
            reversed_argv.emplace_back(value.data());
        }

        GlobalSettings reversed {LoadAppSettings(CommandLineArgs {reversed_argv})};

        CHECK(reversed.Common.GameName == "First");
    }

    SECTION("LoadAppSettingsThrowsForBareConfigNameBeforeLaterValidOccurrence")
    {
        string temp_dir {MakeTempSettingsDir("settings_load_app_bare_config")};
        auto cleanup {scope_exit([temp_dir]() noexcept { ignore_unused(fs::remove_dir_tree(temp_dir)); })};
        REQUIRE(fs::create_directories(temp_dir));

        string config_path {strex(temp_dir).combine_path("main.fomain").str()};
        REQUIRE(fs::write_file(config_path,
            string_view {"Common.GameName = Base\n"
                         "[SubConfig]\n"
                         "Name = First\n"
                         "Common.GameName = First\n"}));

        string writable {strex(temp_dir).combine_path("writable").str()};
        vector<string> values {"app", "--ApplyConfig", "--ApplyConfig", config_path, "--Common.UserWritablePath", writable};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        CHECK_THROWS_AS(LoadAppSettings(CommandLineArgs {argv}), AppInitException);
    }

    SECTION("LoadAppSettingsThrowsForBareSubConfigNameBeforeLaterValidOccurrence")
    {
        string temp_dir {MakeTempSettingsDir("settings_load_app_bare_subconfig")};
        auto cleanup {scope_exit([temp_dir]() noexcept { ignore_unused(fs::remove_dir_tree(temp_dir)); })};
        REQUIRE(fs::create_directories(temp_dir));

        string config_path {strex(temp_dir).combine_path("main.fomain").str()};
        REQUIRE(fs::write_file(config_path,
            string_view {"Common.GameName = Base\n"
                         "[SubConfig]\n"
                         "Name = First\n"
                         "Common.GameName = First\n"}));

        string writable {strex(temp_dir).combine_path("writable").str()};
        vector<string> values {"app", "--ApplyConfig", config_path, "--ApplySubConfig", "--ApplySubConfig", "First", "--Common.UserWritablePath", writable};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        CHECK_THROWS_AS(LoadAppSettings(CommandLineArgs {argv}), AppInitException);
    }

    SECTION("ApplyCommandLineKeepsSequentialReplaceAndAppendOperations")
    {
        // The scalar getter answers the last occurrence, but the command-line pass applies every entry
        // in order: replacing the token list and appending twice cannot collapse into the final entry
        GlobalSettings settings {false};
        settings.SetupDefaultSettings();

        char arg0[] {"lf_tests"};
        char arg1[] {"--Common.SecretSettingTokens"};
        char arg2[] {"base"};
        char arg3[] {"-Common.SecretSettingTokens"};
        char arg4[] {"+tail"};
        char arg5[] {"--Common.SecretSettingTokens"};
        char arg6[] {"+last"};
        char* argv[] {arg0, arg1, arg2, arg3, arg4, arg5, arg6};

        settings.ApplyCommandLine(CommandLineArgs {7, argv});

        CHECK(settings.Common.SecretSettingTokens == vector<string> {"base", "tail", "last"});

        char arg7[] {"--Probe.Scalar"};
        char arg8[] {"first"};
        char arg9[] {"-Probe.Scalar"};
        char arg10[] {"second"};
        char* last_argv[] {arg0, arg7, arg8, arg9, arg10};

        settings.ApplyCommandLine(CommandLineArgs {5, last_argv});

        CHECK(settings.GetCustomSetting("Probe.Scalar") == "second");
    }
}

TEST_CASE("WritableRootConfigCannotReplaceStartupResolution")
{
    for (bool baking_mode : {false, true}) {
        GlobalSettings settings {baking_mode};
        settings.SetupDefaultSettings();
        settings.ApplyWritableRoot("resolved/startup/root");

        for (string_view value : {"other/root", "+other/root", "*", "$ENV{FONLINE_UNUSED_WRITABLE_ROOT}"}) {
            CAPTURE(baking_mode, value);
            ConfigFile config {strex("Common.UserWritablePath = {}\n", value)};

            CHECK_THROWS_AS(settings.ApplyConfigFile(config, ""), SettingsException);
            CHECK(settings.Common.UserWritablePath == "resolved/startup/root");
            CHECK(settings.GetRuntimeSetting(string(SettingNames::Common_UserWritablePath)) == "resolved/startup/root");
        }

        ConfigFile empty_config {"Common.UserWritablePath =\n"};

        CHECK_NOTHROW(settings.ApplyConfigFile(empty_config, ""));
        CHECK(settings.Common.UserWritablePath == "resolved/startup/root");
        CHECK(settings.GetRuntimeSetting(string(SettingNames::Common_UserWritablePath)) == "resolved/startup/root");
    }
}

TEST_CASE("ApplicationWritableRootRejectsLateConfigSources")
{
    string root {MakeTempSettingsDir("application_writable_root")};
    auto cleanup {scope_exit([&root]() noexcept { (void)fs::remove_dir_tree(root); })};
    string config_path {strex(root).combine_path("Startup.fomain").str()};
    string config {"Render.Sleep = 7\nCommon.CachePath = Cache\n"};
    char program[] {"settings_test"};
    char apply_config[] {"--ApplyConfig"};
    char writable_path[] {"--Common.UserWritablePath"};
    char apply_subconfig[] {"--ApplySubConfig"};
    char subconfig_name[] {"Late"};
    vector<CommandLineArg> args {program, apply_config, config_path.data(), writable_path, root.data()};

    SECTION("MainConfig")
    {
        config += "Common.UserWritablePath = other/root\n";
    }

    SECTION("SubConfig")
    {
        config += "[SubConfig]\nName = Late\nCommon.UserWritablePath = other/root\n";
        args.emplace_back(apply_subconfig);
        args.emplace_back(subconfig_name);
    }

    SECTION("LocalConfig")
    {
        CacheStorage cache {strex(root).combine_path("Cache").str()};
        cache.SetString(LOCAL_CONFIG_NAME, "Common.UserWritablePath = other/root\n");
    }

    REQUIRE(fs::write_file(config_path, config));
    CHECK_THROWS_AS(LoadAppSettings(CommandLineArgs {args}), SettingsException);
}

TEST_CASE("ApplicationSettings")
{
    string root {MakeTempSettingsDir("application_settings")};
    auto cleanup {scope_exit([&root]() noexcept { (void)fs::remove_dir_tree(root); })};
    string config_path {strex(root).combine_path("Startup.fomain").str()};
    REQUIRE(fs::write_file(config_path,
        string_view {"Common.CachePath = Cache\n"
                     "Render.Sleep = 7\n"
                     "Network.NetBufferSize = 8192\n"
                     "[SubConfig]\n"
                     "Name = Override\n"
                     "Render.Sleep = 11\n"}));

    char program[] {"settings_test"};
    char apply_config[] {"--ApplyConfig"};
    char writable_path[] {"--Common.UserWritablePath"};
    vector<CommandLineArg> args {program, apply_config, config_path.data(), writable_path, root.data()};

    auto configured {LoadAppSettings(CommandLineArgs {args})};
    CHECK(configured.SpriteMesh.MaxTriangles == 0);
    CHECK(configured.ServerNetwork.ClientPingTime == 0);
    CHECK(configured.Common.CachePath == "Cache");
    CHECK(configured.Common.SecretSettingTokens.empty());
    CHECK(configured.Network.NetBufferSize == 8192);
    CHECK(configured.Render.Sleep == 7);
    CHECK(configured.Common.UserWritablePath == fs::resolve_path(root));

    char apply_subconfig[] {"--ApplySubConfig"};
    char subconfig_name[] {"Override"};
    args.emplace_back(apply_subconfig);
    args.emplace_back(subconfig_name);
    auto overlaid {LoadAppSettings(CommandLineArgs {args})};
    CHECK(overlaid.Render.Sleep == 11);
    CHECK(overlaid.SpriteMesh.MaxTriangles == 0);

    {
        CacheStorage cache {strex(root).combine_path("Cache").str()};
        cache.SetString(LOCAL_CONFIG_NAME, "Render.Sleep = 13\nCommon.UserWritablePath =\n");
    }

    auto cached {LoadAppSettings(CommandLineArgs {args})};
    CHECK(cached.Render.Sleep == 13);
    CHECK(cached.Common.UserWritablePath == fs::resolve_path(root));
    CHECK(cached.ServerNetwork.ClientPingTime == 0);

    char sleep_setting[] {"--Render.Sleep"};
    char zero[] {"0"};
    args.emplace_back(sleep_setting);
    args.emplace_back(zero);
    auto command_line {LoadAppSettings(CommandLineArgs {args})};
    CHECK(command_line.Render.Sleep == 0);
    CHECK(command_line.Network.NetBufferSize == 8192);
    CHECK(command_line.SpriteMesh.MaxTriangles == 0);
}

TEST_CASE("InternalConfigBootstrap", "[.][settings]")
{
    auto expected {make_nptr(std::getenv("FO_TEST_RESOURCE_DIRECTORY"))};
    REQUIRE(expected);
    CHECK(ReadInternalConfigValue(SettingNames::Common_ResourcesPath) == expected.get());
    CHECK(ReadInternalConfigValue("Common.ResourcesPathPrefix").empty());
}

TEST_CASE("PackagedApplicationSettings", "[.][settings]")
{
    if (!GetApp()->Settings.Common.Packaged) {
        SKIP("Requires a packaged unit-test binary with a Metadata bootstrap pack");
    }

    string root {MakeTempSettingsDir("packaged_application_settings")};
    auto cleanup {scope_exit([&root]() noexcept { safe_call([&root] { (void)fs::remove_dir_tree(root); }); })};
    char program[] {"settings_test"};
    char writable_path[] {"--Common.UserWritablePath"};
    char append_setting[] {"--Common.SecretSettingTokens"};
    char append_value[] {"+startup-probe"};
    char direct_draw[] {"--View.MapDirectDraw"};
    char enabled[] {"1"};
    char compatibility[] {"--Network.ForceCompatibilityVersion"};
    char compatibility_value[] {"startup-probe-version"};
    vector<CommandLineArg> args {program, writable_path, root.data(), append_setting, append_value, direct_draw, enabled, compatibility, compatibility_value};
    size_t append_logs {0};
    logging::set_callback("packaged_settings_once_test", [&append_logs](logging::type, string_view message, nptr<const stack_trace::catched_data>) {
        if (message.find("Set Common.SecretSettingTokens to ") != string_view::npos) {
            append_logs++;
        }
    });
    auto remove_callback {scope_exit([]() noexcept { logging::set_callback("packaged_settings_once_test", nullptr); })};

    SECTION("MissingResourceAppliesCommandLineOnce")
    {
        auto settings {LoadAppSettings(CommandLineArgs {args}, AppInitFlags::StandaloneClient)};
        CHECK_FALSE(settings.GetResourceConfigContent());
        CHECK(std::ranges::count(settings.Common.SecretSettingTokens, "startup-probe") == 1);
        CHECK(append_logs == 1);
        CHECK(settings.Common.Packaged);
        CHECK(settings.View.MapDirectDraw);
        CHECK_FALSE(settings.View.MapZoomEnabled);
        CHECK(settings.Network.CompatibilityVersion == compatibility_value);
    }

    SECTION("WritableClientProfileHasPriority")
    {
        string installed {strex(root).combine_path("InstalledResources").str()};
        string overlay {strex(root).combine_path("Resources").str()};
        REQUIRE(fs::create_directories(installed));
        REQUIRE(fs::create_directories(overlay));
        string old_content {"Common.GameVersion=installed-client\n"};
        string new_content {"#" FO_COMPATIBILITY_VERSION "\nCommon.GameVersion=writable-client\n"};

        {
            ResourcePackWriter writer {strex(installed).combine_path("Metadata.fores").str()};
            writer.AddFile("Selected.fomain-client", make_const_span(old_content));
            writer.Finish();
        }
        {
            ResourcePackWriter writer {strex(overlay).combine_path("Metadata.fores").str()};
            writer.AddFile("Selected.fomain-client", make_const_span(new_content));
            writer.Finish();
        }

        char resource_option[] {"--Common.ResourcesPath"};
        args.emplace_back(resource_option);
        args.emplace_back(installed.data());
        auto settings {LoadAppSettings(CommandLineArgs {args}, AppInitFlags::StandaloneClient)};
        CHECK(settings.Common.GameVersion == "writable-client");
        CHECK(settings.GetResourceConfigContent() == new_content);
    }

    SECTION("OlderResourceProfileDefersToUpdatedBinary")
    {
        string resources {strex(root).combine_path("Resources").str()};
        REQUIRE(fs::create_directories(resources));
        string legacy {"Baking.ClientResources=Resources\nCommon.GameVersion=older-resources\n"};

        {
            ResourcePackWriter writer {strex(resources).combine_path("Metadata.fores").str()};
            writer.AddFile("Selected.fomain-client", make_const_span(legacy));
            writer.Finish();
        }

        auto settings {LoadAppSettings(CommandLineArgs {args}, AppInitFlags::StandaloneClient)};
        CHECK_FALSE(settings.GetResourceConfigContent());
        CHECK(settings.Common.GameVersion == "bootstrap-version");
    }

    SECTION("SubConfigRequestContinuesWithMainProfile")
    {
        char apply_subconfig[] {"--ApplySubConfig"};
        char subconfig[] {"Ignored"};
        args.emplace_back(apply_subconfig);
        args.emplace_back(subconfig);
        CHECK_NOTHROW(LoadAppSettings(CommandLineArgs {args}, AppInitFlags::StandaloneClient));
    }

    SECTION("ResourceLocationOverrideAndFinalPrecedence")
    {
        string resources {strex(root).combine_path("Resources").str()};
        REQUIRE(fs::create_directories(resources));
        string content {"#" FO_COMPATIBILITY_VERSION "\n"
                        "Common.ResourcesPath=Resources\nCommon.GameVersion=resource-version\n"
                        "Client.Language=russ\nCommon.SecretSettingTokens=resource-secret\n"
                        "Common.CachePath=ResourceCache\n"
                        "[ResourcePack]\nName=Metadata\n"};
        {
            ResourcePackWriter writer {strex(resources).combine_path("Metadata.fores").str()};
            writer.AddFile("Selected.fomain-client", make_const_span(content));
            // The reader filters by config suffix and never parses the metadata payload
            string metadata_content {"metadata-bytes"};
            writer.AddFile("Metadata.fometa-client", make_const_span(metadata_content));
            writer.Finish();
        }
        string cache_path {strex(root).combine_path("SelectedCache").str()};
        {
            CacheStorage cache {cache_path};
            cache.SetString(LOCAL_CONFIG_NAME, "Client.Language=ru18\nCommon.SecretSettingTokens=local-secret\n");
        }
        char cache_setting[] {"--Common.CachePath"};
        char language[] {"--Client.Language"};
        char language_value[] {"en18"};
        char resources_setting[] {"--Common.ResourcesPath"};
        args.emplace_back(resources_setting);
        args.emplace_back(resources.data());
        args.emplace_back(cache_setting);
        args.emplace_back(cache_path.data());
        args.emplace_back(language);
        args.emplace_back(language_value);
        auto settings {LoadAppSettings(CommandLineArgs {args}, AppInitFlags::StandaloneClient)};
        REQUIRE(settings.GetResourceConfigContent());
        CHECK(*settings.GetResourceConfigContent() == content);
        CHECK(settings.Common.GameVersion == "resource-version");
        CHECK(GetClientWritableResourceDir(settings) == resources);
        CHECK(settings.Common.CachePath == cache_path);
        CHECK(settings.Client.Language == "en18");
        CHECK(std::ranges::count(settings.Common.SecretSettingTokens, "local-secret") == 1);
        CHECK(std::ranges::count(settings.Common.SecretSettingTokens, "startup-probe") == 1);
        CHECK(append_logs == 1);
        CHECK(settings.Common.Packaged);
        CHECK_FALSE(settings.View.MapZoomEnabled);
        CHECK(settings.Network.CompatibilityVersion == compatibility_value);
    }
}

TEST_CASE("PackagedServerApplicationSettings", "[.][settings]")
{
    if (!GetApp()->Settings.Common.Packaged) {
        SKIP("Requires a packaged unit-test binary with a Metadata bootstrap pack");
    }

    string root {MakeTempSettingsDir("packaged_server_settings")};
    auto cleanup {scope_exit([&root]() noexcept { safe_call([&root] { (void)fs::remove_dir_tree(root); }); })};
    REQUIRE(fs::create_directories(root));
    auto previous_path {std::filesystem::current_path()};
    auto restore_path {scope_exit([&previous_path]() noexcept { std::filesystem::current_path(previous_path); })};
    std::filesystem::current_path(fs::make_path(root));
    string server_resources {strex(root).combine_path("Resources").str()};
    char program[] {"settings_test"};
    char writable_path[] {"--Common.UserWritablePath"};
    char append_setting[] {"--Common.SecretSettingTokens"};
    char append_value[] {"+server-startup-probe"};
    vector<CommandLineArg> args {program, writable_path, root.data(), append_setting, append_value};

    SECTION("ReadsServerResourceAndKeepsServerSettings")
    {
        REQUIRE(fs::create_directories(server_resources));
        {
            string content {"Common.ResourcesPath=Resources\nCommon.CachePath=Cache\nCommon.GameVersion=server-resource-version\nServer.DbStorage=JSON\nServer.AutoStartClientOnServer=2\n"
                            "[ResourcePack]\nName=Metadata\n"};
            ResourcePackWriter writer {strex(server_resources).combine_path("Metadata.fores").str()};
            writer.AddFile("Selected.fomain-server", make_const_span(content));
            string client_content {"Common.GameVersion=client-resource-version\n"};
            writer.AddFile("Selected.fomain-client", make_const_span(client_content));
            // The reader filters by config suffix and never parses the metadata payload
            string metadata_content {"metadata-bytes"};
            writer.AddFile("Metadata.fometa-server", make_const_span(metadata_content));
            writer.Finish();
        }
        {
            CacheStorage cache {strex(root).combine_path("Cache").str()};
            cache.SetString(LOCAL_CONFIG_NAME, "Server.ServerHeight=600\n");
        }
        char height[] {"--Server.ServerHeight"};
        char height_value[] {"720"};
        args.emplace_back(height);
        args.emplace_back(height_value);
        auto settings {LoadAppSettings(CommandLineArgs {args})};
        CHECK(settings.Common.GameVersion == "server-resource-version");
        CHECK(settings.Server.DbStorage == "JSON");
        CHECK(settings.Server.AutoStartClientOnServer == 2);
        CHECK(settings.Server.ServerHeight == 720);
        CHECK(std::ranges::count(settings.Common.SecretSettingTokens, "server-startup-probe") == 1);
    }

    SECTION("MissingServerConfigurationFallsBackToClient")
    {
        REQUIRE(fs::create_directories(server_resources));
        string content {"Common.GameVersion=client-fallback-version\n[ResourcePack]\nName=Metadata\n"};
        {
            ResourcePackWriter writer {strex(server_resources).combine_path("Metadata.fores").str()};
            writer.AddFile("Selected.fomain-client", make_const_span(content));
            string metadata_content {"metadata-bytes"};
            writer.AddFile("Metadata.fometa-client", make_const_span(metadata_content));
            writer.Finish();
        }
        auto settings {LoadAppSettings(CommandLineArgs {args})};
        CHECK(settings.Common.GameVersion == "client-fallback-version");
        CHECK(settings.GetResourceConfigContent() == content);
        CHECK(std::ranges::count(settings.Common.SecretSettingTokens, "server-startup-probe") == 1);
    }

    SECTION("ServerBaseIgnoresWritableClientMetadata")
    {
        string writable {strex(root).combine_path("Writable").str()};
        string overlay {strex(writable).combine_path("Resources").str()};
        REQUIRE(fs::create_directories(server_resources));
        REQUIRE(fs::create_directories(overlay));
        string server_content {"#" FO_COMPATIBILITY_VERSION "\nCommon.ResourcesPath=Resources\nServer.DbStorage=JSON\nCommon.GameVersion=server-base\n"};
        string client_content {"#" FO_COMPATIBILITY_VERSION "\nCommon.ResourcesPath=Resources\nCommon.GameVersion=client-overlay\n"};

        {
            ResourcePackWriter writer {strex(server_resources).combine_path("Metadata.fores").str()};
            writer.AddFile("Selected.fomain-server", make_const_span(server_content));
            writer.Finish();
        }
        {
            ResourcePackWriter writer {strex(overlay).combine_path("Metadata.fores").str()};
            writer.AddFile("Selected.fomain-client", make_const_span(client_content));
            writer.Finish();
        }

        args[2] = writable.data();
        auto settings {LoadAppSettings(CommandLineArgs {args})};
        CHECK(settings.Server.DbStorage == "JSON");
        CHECK(settings.Common.GameVersion == "server-base");
        CHECK(settings.GetResourceConfigContent() == server_content);
        REQUIRE(fs::write_file(strex(overlay).combine_path("Metadata.patch.fores").str(), string_view {"client-only-invalid-patch"}));
        auto unpatched_server {LoadAppSettings(CommandLineArgs {args})};
        CHECK(unpatched_server.GetResourceConfigContent() == server_content);
        auto client {LoadAppSettings(CommandLineArgs {args}, AppInitFlags::StandaloneClient)};
        CHECK(client.GetResourceConfigContent() == client_content);
    }

    SECTION("MissingResourcesKeepBootstrap")
    {
        auto settings {LoadAppSettings(CommandLineArgs {args})};
        CHECK_FALSE(settings.GetResourceConfigContent());
        CHECK(settings.Common.GameVersion == "bootstrap-version");
        CHECK(std::ranges::count(settings.Common.SecretSettingTokens, "server-startup-probe") == 1);
    }
}

TEST_CASE("SettingsCopyFromZeroSnapshot", "[settings]")
{
    auto settings {GlobalSettings(true)};

    settings.SetupDefaultSettings();
    ConfigFile config {"AAA.Value=1\n[ResourcePack]\nName=Metadata\n[SubConfig]\nName=Child\n"};
    settings.ApplyConfigFile(config, "");
    settings.SetRuntimeSetting("Runtime.Value", "old");
    uint64_t changed_generation {settings.GetCustomSettingsGeneration()};
    settings.CopyFrom(GlobalSettings {true});
    CHECK(settings.Network.NetBufferSize == 0);
    CHECK_FALSE(settings.Client.ClientPropertiesPackData);
    CHECK_FALSE(settings.FindCustomSetting("AAA.Value"));
    CHECK_FALSE(settings.FindCustomSetting("Runtime.Value"));
    CHECK_THROWS_AS(settings.GetResourcePacks(), SettingsException);
    CHECK(settings.GetSubConfigs().empty());
    CHECK_FALSE(settings.HasCompleteConfig());
    CHECK(settings.GetCustomSettingsGeneration() > changed_generation);
    settings.SetSettingValue(SettingNames::Common_GameName, "StillBaking");
    CHECK(settings.Save().at(string(SettingNames::Common_GameName)) == "StillBaking");
}

TEST_CASE("SettingsSetupRequiresFreshBase")
{
    string directory {MakeTempSettingsDir("setup_fresh_base")};
    auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(directory); })};
    REQUIRE(fs::create_directories(directory));
    REQUIRE(fs::write_file(strex(directory).combine_path("Root.fomain").str(), "Render.Sleep=7\nGame.Keep=base\n"));
    GlobalSettings settings {false};

    SECTION("Defaults")
    {
        settings.SetupDefaultSettings();
    }

    SECTION("RootFile")
    {
        settings.SetupConfigAtPath("Root.fomain", directory);
    }

    SECTION("ResourceProfile")
    {
        settings.SetupResourceConfig("Render.Sleep=7\nGame.Keep=base\n");
    }

    SECTION("NativeOverlayIncludingZero")
    {
        settings.SetSettingValue("Render.Sleep", "0");
    }

    SECTION("GameOverlay")
    {
        settings.SetCustomSetting("Game.Keep", any_t {string {"custom"}});
    }

    SECTION("WritableRootIncludingEmpty")
    {
        settings.ApplyWritableRoot("");
    }

    SECTION("AutoWithEmptyIdentity")
    {
        settings.ApplyAutoSettings({});
    }

    SECTION("DefaultsAfterEmptyGameRefresh")
    {
        settings.SetupDefaultSettings();
        GlobalSettings empty {false};
        settings.ApplyGameSettings(empty);
    }

    GlobalSettings before {false};
    before.CopyFrom(settings);
    string game_before {settings.GetRuntimeSetting("Game.Keep")};
    auto source_before {settings.GetResourceConfigContent()};

    CHECK_THROWS_WITH(settings.SetupDefaultSettings(), Catch::Matchers::ContainsSubstring("Settings setup requires fresh settings"));
    CHECK_THROWS_WITH(settings.SetupConfigAtPath("Root.fomain", directory), Catch::Matchers::ContainsSubstring("Settings setup requires fresh settings"));
    CHECK_THROWS_WITH(settings.SetupInternalConfig(), Catch::Matchers::ContainsSubstring("Settings setup requires fresh settings"));
    CHECK_THROWS_WITH(settings.SetupResourceConfig("Game.Keep=replaced\n"), Catch::Matchers::ContainsSubstring("Settings setup requires fresh settings"));
    CHECK_FALSE(settings.HasEngineSettingsChanges(before));
    CHECK(settings.GetRuntimeSetting("Game.Keep") == game_before);
    CHECK(settings.GetResourceConfigContent() == source_before);
}

TEST_CASE("SettingsSetupPreservesPartialApplyAndRetry")
{
    SECTION("DefaultsThenPartialConfig")
    {
        GlobalSettings settings {false};
        settings.SetupDefaultSettings();
        ConfigFile overlay {"Render.Sleep=11\nGame.Value=overlay\n"};
        settings.ApplyConfigFile(overlay, "");
        CHECK(settings.Render.Sleep == 11);
        CHECK(settings.Client.ClientPropertiesPackData);
        CHECK(settings.GetCustomSetting("Game.Value") == "overlay");
        CHECK_FALSE(settings.HasCompleteConfig());
    }

    SECTION("EmptyFileNameRejectsWithoutMutation")
    {
        GlobalSettings settings {false};
        CHECK_THROWS_AS(settings.SetupConfigAtPath("", ""), SettingsException);
        REQUIRE_NOTHROW(settings.SetupDefaultSettings());
        CHECK(settings.Client.ClientPropertiesPackData);
    }

    SECTION("UnpatchedInternalConfigAllowsAnotherSetup")
    {
        if (!GetApp()->Settings.Common.Packaged) {
            GlobalSettings settings {false};
            CHECK_THROWS_AS(settings.SetupInternalConfig(), SettingsException);
            REQUIRE_NOTHROW(settings.SetupDefaultSettings());
            CHECK(settings.Client.ClientPropertiesPackData);
        }
    }
}

TEST_CASE("SettingsSetupFileFailureAllowsRetry")
{
    string directory {MakeTempSettingsDir("setup_file_retry")};
    auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(directory); })};
    REQUIRE(fs::create_directories(directory));
    string path {strex(directory).combine_path("Root.fomain").str()};
    REQUIRE(fs::write_file(path, "AAA.Partial=old\nClient.ClientPropertiesPackData=invalid-bool\n"));
    GlobalSettings settings {true};

    CHECK_THROWS(settings.SetupConfigAtPath("Root.fomain", directory));
    CHECK_FALSE(settings.FindCustomSetting("AAA.Partial"));
    CHECK(settings.GetAppliedConfigs().empty());
    CHECK(settings.Render.Sleep == 0);
    REQUIRE(fs::write_file(path, "Render.Sleep=9\nGame.Current=retry\n"));
    REQUIRE_NOTHROW(settings.SetupConfigAtPath("Root.fomain", directory));
    CHECK(settings.Render.Sleep == 9);
    CHECK(settings.GetCustomSetting("Game.Current") == "retry");
    CHECK(settings.GetAppliedConfigs().size() == 1);
    CHECK_FALSE(settings.HasCompleteConfig());
}

TEST_CASE("ResourceConfigurationFailurePreservesSnapshot", "[settings]")
{
    auto settings {GlobalSettings(false)};
    CHECK_THROWS(settings.SetupResourceConfig("AAA.Partial=1\nClient.ClientPropertiesPackData=invalid-bool\n"));
    CHECK_FALSE(settings.FindCustomSetting("AAA.Partial"));
    CHECK_FALSE(settings.Client.ClientPropertiesPackData);
    CHECK_FALSE(settings.HasCompleteConfig());

    REQUIRE_NOTHROW(settings.SetupResourceConfig("Game.Current=original\n"));
    CHECK(settings.HasCompleteConfig());
    CHECK_THROWS(settings.SetupResourceConfig("AAA.Partial=2\nClient.ClientPropertiesPackData=invalid-bool\n"));
    CHECK(settings.GetCustomSetting("Game.Current") == "original");
    CHECK_FALSE(settings.FindCustomSetting("AAA.Partial"));
    CHECK(settings.HasCompleteConfig());
    auto copied {GlobalSettings(false)};
    copied.CopyFrom(settings);
    CHECK(copied.HasCompleteConfig());
    copied.CopyFrom(GlobalSettings {false});
    CHECK_FALSE(copied.HasCompleteConfig());
}

TEST_CASE("ResourceConfigurationStartsFromZero", "[settings]")
{
    auto settings {GlobalSettings(false)};
    string content {"Render.Sleep=7\nGame.Caption=0\nGame.Word=false\n"};
    REQUIRE_NOTHROW(settings.SetupResourceConfig(content));
    CHECK(settings.Render.Sleep == 7);
    CHECK_FALSE(settings.Client.ClientPropertiesPackData);
    CHECK(settings.Network.NetBufferSize == 0);
    CHECK(settings.GetCustomSetting("Game.Caption") == "0");
    CHECK(settings.GetCustomSetting("Game.Word") == "false");
}

TEST_CASE("ResourcesDirectoryReplacesRoleSpecificDirectories")
{
    auto settings {GlobalSettings(false)};
    settings.SetSettingValue("Common.ResourcesPath", "Content/Nested");
    CHECK(settings.Common.ResourcesPath == "Content/Nested");
    settings.SetSettingValue("Common.CachePath", "UserCache");
    settings.SetSettingValue("Common.PlatformBinariesPath", "Runtime");
    CHECK(settings.Common.CachePath == "UserCache");
    CHECK(settings.Common.PlatformBinariesPath == "Runtime");

    FO_TEMPORARY_COMPAT(RetiredBakingDirectorySettings, "2026-12-31");
    for (string_view old_name : {"Baking.ResourcesDir", "Baking.CacheResources", "Baking.PlatformBinaries"}) {
        CHECK_THROWS_AS(settings.SetSettingValue(old_name, "OldDirectory"), SettingsException);
    }

    CHECK_THROWS_AS(settings.SetSettingValue("Baking.ClientResources", "OtherResources"), SettingsException);
    CHECK_THROWS_AS(settings.SetSettingValue("Baking.ServerResources", "OtherResources"), SettingsException);
}

TEST_CASE("ResourceBootstrapDoesNotReadClientDistribution")
{
    string root {MakeTempSettingsDir("resource_bootstrap")};
    auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(root); })};
    string resources_dir {strex(root).combine_path("Resources").str()};
    string client_dir {strex(resources_dir).combine_path(CLIENT_RESOURCE_PACKS_SUBDIR).str()};
    REQUIRE(fs::create_directories(client_dir));
    string client_content {"Common.GameVersion=distributed-client\n"};

    {
        ResourcePackWriter writer {strex(client_dir).combine_path("Metadata.fores").str()};
        writer.AddFile("Selected.fomain-client", make_const_span(client_content));
        writer.Finish();
    }

    FileSystem empty_server_source {};
    empty_server_source.AddPackSource(true, resources_dir, METADATA_PACK_NAME, true);
    CHECK_FALSE(ReadResourceConfig(empty_server_source, false));

    FileSystem client_source {};
    client_source.AddPackSource(true, client_dir, METADATA_PACK_NAME);
    CHECK(ReadResourceConfig(client_source, true) == client_content);
    string server_content {"Common.GameVersion=server\n"};

    {
        ResourcePackWriter writer {strex(resources_dir).combine_path("Metadata.fores").str()};
        writer.AddFile("Selected.fomain-server", make_const_span(server_content));
        writer.Finish();
    }

    FileSystem server_source {};
    server_source.AddPackSource(true, resources_dir, METADATA_PACK_NAME);
    CHECK(ReadResourceConfig(server_source, true) == server_content);
}

TEST_CASE("ResourceConfigurationPriority")
{
    string root {MakeTempSettingsDir("resource_config_priority")};
    auto cleanup {scope_exit([&root]() noexcept { safe_call([&root] { (void)fs::remove_dir_tree(root); }); })};
    auto settings {GlobalSettings(false)};
    settings.SetupDefaultSettings();
    ConfigFile declarations {"[ResourcePack]\nName=Metadata\n"};
    settings.ApplyConfigFile(declarations, "");
    string server_content {"Common.GameVersion=server-version\nServer.DbStorage=JSON\n"};
    string client_content {"Common.GameVersion=client-version\n"};

    SECTION("ServerConfigurationPrecedesClientInTheSamePack")
    {
        settings.SetSettingValue("Baking.BakeOutput", root);
        string directory {strex(root).combine_path("Metadata").str()};
        REQUIRE(fs::create_directories(directory));
        string server_path {strex(directory).combine_path("Selected.fomain-server").str()};
        REQUIRE(fs::write_file(server_path, string_view {server_content}));
        REQUIRE(fs::write_file(strex(directory).combine_path("Selected.fomain-client").str(), string_view {client_content}));
        CHECK(ReadTestResourceConfig(settings, true) == server_content);
        REQUIRE(fs::remove_file(server_path));
        CHECK(ReadTestResourceConfig(settings, true) == client_content);
    }

    SECTION("MetadataPackWithoutConfigurationIsAbsent")
    {
        // The pack carries its metadata payload but no baked configuration of either role:
        // the reader ignores the metadata files and reports the configuration as absent
        settings.SetSettingValue("Baking.BakeOutput", root);
        string directory {strex(root).combine_path("Metadata").str()};
        REQUIRE(fs::create_directories(directory));
        REQUIRE(fs::write_file(strex(directory).combine_path("Metadata.fometa-server").str(), string_view {"metadata-bytes"}));
        REQUIRE(fs::write_file(strex(directory).combine_path("Metadata.fometa-client").str(), string_view {"metadata-bytes"}));
        CHECK_FALSE(ReadTestResourceConfig(settings, false));
        CHECK_THROWS_AS(ReadTestResourceConfig(settings, true), BaseEngineException);
    }
}

// Requires a test-binary copy with its internal config patched and its unpackaged marker retained
TEST_CASE("ServerConfiguration", "[.][settings]")
{
    string content {"Common.ResourcesPath=Resources\nCommon.CachePath=Cache\nCommon.GameVersion=server-resource-version\nServer.DbStorage=JSON\nServer.AutoStartClientOnServer=2\n"
                    "[ResourcePack]\nName=Metadata\n"};
    auto settings {GlobalSettings(false)};
    settings.SetupResourceConfig(content);

    SECTION("ReplacesServerSnapshot")
    {
        CHECK(settings.Common.GameVersion == "server-resource-version");
        CHECK(settings.Server.DbStorage == "JSON");
        CHECK(settings.Server.AutoStartClientOnServer == 2);
        CHECK_FALSE(settings.Render.ForceOpenGL);
        REQUIRE(settings.GetResourceConfigContent());
        CHECK(*settings.GetResourceConfigContent() == content);
    }

    SECTION("PrefersServerAndRejectsMultipleServerConfigurations")
    {
        string root {MakeTempSettingsDir("server_resource_config")};
        auto cleanup {scope_exit([&root]() noexcept { safe_call([&root] { (void)fs::remove_dir_tree(root); }); })};
        settings.SetSettingValue("Baking.BakeOutput", root);
        CHECK_FALSE(ReadTestResourceConfig(settings, false));
        CHECK_THROWS_AS(ReadTestResourceConfig(settings, true), BaseEngineException);
        string directory {strex(root).combine_path("Metadata").str()};
        REQUIRE(fs::create_directories(directory));
        REQUIRE(fs::write_file(strex(directory).combine_path("Selected.fomain-server").str(), string_view {content}));
        CHECK(ReadTestResourceConfig(settings, true) == content);
        REQUIRE(fs::write_file(strex(directory).combine_path("Other.fomain-server").str(), string_view {content}));
        CHECK_THROWS_AS(ReadTestResourceConfig(settings, true), BaseEngineException);
        bool removed {fs::remove_file(strex(directory).combine_path("Other.fomain-server").str())};
        REQUIRE(removed);
        REQUIRE(fs::write_file(strex(directory).combine_path("Selected.fomain-client").str(), string_view {content}));
        CHECK(ReadTestResourceConfig(settings, true) == content);
    }
}

TEST_CASE("ClientConfiguration", "[.][settings]")
{
    string content {"Common.GameVersion=0.3.1482\n"
                    "Client.Language=russ\nRender.ForceOpenGL=0\nClient.DirectMoveRetargetMinMs=0\nClient.DirectMoveSlide=0\nGame.Enabled=false\nGame.Caption=\n"
                    "[ResourcePack]\nName=Metadata\n"};
    auto build_settings {[](string_view text) {
        auto result {GlobalSettings(false)};
        result.SetupResourceConfig(text);
        return result;
    }};

    SECTION("ResourceProfileStartsFromZero")
    {
        auto updated {build_settings(content)};
        CHECK(updated.Common.GameVersion == "0.3.1482");
        CHECK(updated.Client.Language == "russ");
        CHECK_FALSE(updated.Render.ForceOpenGL);
        CHECK(updated.Client.DirectMoveRetargetMinMs == 0);
        CHECK_FALSE(updated.Client.DirectMoveSlide);
        CHECK(updated.GetCustomSetting("Game.Enabled") == "false");
        CHECK(updated.GetCustomSetting("Game.Caption").empty());
        CHECK_FALSE(updated.FindCustomSetting("Game.Removed"));
        CHECK_THROWS_AS(updated.SetRuntimeSetting("Common.GameVersion", "changed"), SettingsException);
    }

    SECTION("UserConfigurationAndCommandLineRemainHigherPriority")
    {
        auto updated {build_settings(content)};
        ConfigFile preferences {"Client.Language=engl\nRender.ForceOpenGL=0\n"};
        updated.ApplyConfigFile(preferences, "");
        CHECK(updated.Client.Language == "engl");
        CHECK_FALSE(updated.Render.ForceOpenGL);
        char program[] {"lf_tests"};
        char option[] {"--Client.Language"};
        char value[] {"ru18"};
        vector<CommandLineArg> argv {program, option, value};
        updated.ApplyCommandLine(CommandLineArgs {argv});
        CHECK(updated.Client.Language == "ru18");
    }

    SECTION("ChangingCacheDirectoryPreservesPreferencesAndExistingDestinationWins")
    {
        string directory {MakeTempSettingsDir("client_preferences")};
        auto cleanup {scope_exit([&directory]() noexcept { safe_call([&directory] { (void)fs::remove_dir_tree(directory); }); })};
        auto previous {build_settings(content)};
        previous.ApplyWritableRoot(directory);
        previous.SetSettingValue("Common.CachePath", "old-cache");
        auto updated {build_settings(content)};
        updated.ApplyWritableRoot(directory);
        updated.SetSettingValue("Common.CachePath", "new-cache");
        auto old_cache {CacheStorage(fs::make_writable_path(directory, previous.Common.CachePath))};
        old_cache.SetString(LOCAL_CONFIG_NAME, "Client.Language=ru18\n");
        PreserveLocalConfig(previous, updated);
        auto new_cache {CacheStorage(fs::make_writable_path(directory, updated.Common.CachePath))};
        CHECK(new_cache.GetString(LOCAL_CONFIG_NAME) == "Client.Language=ru18\n");
        new_cache.SetString(LOCAL_CONFIG_NAME, "Client.Language=en18\n");
        PreserveLocalConfig(previous, updated);
        CHECK(new_cache.GetString(LOCAL_CONFIG_NAME) == "Client.Language=en18\n");
    }

    SECTION("ResourceProfileAndSecondStartup")
    {
        string directory {MakeTempSettingsDir("client_config")};
        auto cleanup {scope_exit([&directory]() noexcept { safe_call([&directory] { (void)fs::remove_dir_tree(directory); }); })};
        auto settings {build_settings(content)};
        settings.SetSettingValue("Baking.BakeOutput", directory);
        CHECK_FALSE(ReadTestResourceConfig(settings, false));
        CHECK_THROWS_AS(ReadTestResourceConfig(settings, true), BaseEngineException);
        string pack_directory {strex(directory).combine_path("Metadata").str()};
        REQUIRE(fs::create_directories(pack_directory));
        string path {strex(pack_directory).combine_path("Test.fomain-client").str()};
        REQUIRE(fs::write_file(path, string_view {content}));
        auto applied {ReadTestResourceConfig(settings, true)};
        REQUIRE(applied);
        auto running {build_settings(*applied)};
        CHECK(running.GetResourceConfigContent() == applied);

        string next {content};
        next.replace(next.find("0.3.1482"), 8, "0.3.1483");
        REQUIRE(fs::write_file(path, string_view {next}));
        auto downloaded {ReadTestResourceConfig(settings, true)};
        REQUIRE(downloaded);
        CHECK(*downloaded == next);
        CHECK(running.GetResourceConfigContent() != downloaded);
        CHECK(running.Common.GameVersion == "0.3.1482");
        auto relaunched {build_settings(*downloaded)};
        CHECK(relaunched.Common.GameVersion == "0.3.1483");
        CHECK(relaunched.GetResourceConfigContent() == ReadTestResourceConfig(settings, true));
        CHECK_FALSE(fs::exists(strex(directory).combine_path(".client-config").str()));
        REQUIRE(fs::write_file(strex(pack_directory).combine_path("Other.fomain-client"), string_view {content}));
        CHECK_THROWS_AS(ReadTestResourceConfig(settings, true), BaseEngineException);
    }

    SECTION("ProfileMemorySurvivesCopyAndPreferenceOverrides")
    {
        auto applied {build_settings(content)};
        auto copied {GlobalSettings(false)};
        CHECK_FALSE(copied.GetResourceConfigContent());
        copied.CopyFrom(applied);
        copied.SetSettingValue("Client.Language", "en18");
        REQUIRE(copied.GetResourceConfigContent());
        CHECK(*copied.GetResourceConfigContent() == content);
        CHECK(copied.Client.Language == "en18");
    }
}

TEST_CASE("PackagedResourceConfiguration", "[.][settings]")
{
    if (!GetApp()->Settings.Common.Packaged) {
        SKIP("Requires a packaged test binary with a patched internal config");
    }

    auto bootstrap {GlobalSettings(false)};
    bootstrap.SetupInternalConfig();
    CHECK(bootstrap.Common.GameVersion == "bootstrap-version");
    CHECK(bootstrap.GetCustomSetting("Game.Removed") == "old");
    CHECK(bootstrap.Render.ForceOpenGL);

    string content {"Common.GameVersion=resource-version\nRender.ForceOpenGL=0\n"
                    "[ResourcePack]\nName=Metadata\n"};
    auto updated {GlobalSettings(false)};
    updated.SetupResourceConfig(content);
    CHECK(updated.Common.GameVersion == "resource-version");
    CHECK_FALSE(updated.Render.ForceOpenGL);
    CHECK_FALSE(updated.FindCustomSetting("Game.Removed"));
    REQUIRE(updated.GetResourceConfigContent());
    CHECK(*updated.GetResourceConfigContent() == content);
    CHECK(bootstrap.Common.GameVersion == "bootstrap-version");

    string next {"Common.GameVersion=next-version\nRender.ForceOpenGL=0\n"
                 "[ResourcePack]\nName=Metadata\n"};
    auto relaunched {GlobalSettings(false)};
    relaunched.SetupResourceConfig(next);
    CHECK(relaunched.Common.GameVersion == "next-version");
    CHECK_FALSE(relaunched.Render.ForceOpenGL);
    CHECK(updated.Common.GameVersion == "resource-version");
    CHECK(updated.GetResourceConfigContent() != relaunched.GetResourceConfigContent());

    CHECK(updated.Common.GameVersion == "resource-version");
    CHECK(*updated.GetResourceConfigContent() == content);
}

TEST_CASE("EngineSettingsChanges")
{
    SECTION("PristineAndEquivalentNativeValuesCompareEqual")
    {
        GlobalSettings left {false};
        GlobalSettings right {false};

        CHECK_FALSE(left.HasEngineSettingsChanges(right));
        CHECK_FALSE(right.HasEngineSettingsChanges(left));

        left.SetupDefaultSettings();
        right.SetupDefaultSettings();

        CHECK_FALSE(left.HasEngineSettingsChanges(right));

        left.SetSettingValue("Render.WindowResizable", "0");
        left.SetSettingValue("Render.Sleep", "0");
        left.SetSettingValue("Common.SecretSettingTokens", "alpha\tbeta");
        left.SetSettingValue("Render.MapRenderTargetScale", "1");
        right.SetSettingValue("Render.WindowResizable", "false");
        right.SetSettingValue("Render.Sleep", "0");
        right.SetSettingValue("Common.SecretSettingTokens", "alpha beta");
        right.SetSettingValue("Render.MapRenderTargetScale", "1.0");

        CHECK_FALSE(left.HasEngineSettingsChanges(right));
        CHECK_FALSE(right.HasEngineSettingsChanges(left));
    }

    SECTION("ScalarVectorAndPathChangesCount")
    {
        GlobalSettings left {false};
        GlobalSettings right {false};

        left.SetupDefaultSettings();
        right.SetupDefaultSettings();

        CHECK_FALSE(left.HasEngineSettingsChanges(right));

        right.SetSettingValue("Render.Sleep", "1");
        CHECK(left.HasEngineSettingsChanges(right));
        CHECK(right.HasEngineSettingsChanges(left));

        right.SetSettingValue("Render.Sleep", "-1");
        CHECK_FALSE(left.HasEngineSettingsChanges(right));

        right.SetSettingValue("Common.SecretSettingTokens", "custom tokens");
        CHECK(left.HasEngineSettingsChanges(right));

        right.SetSettingValue("Common.SecretSettingTokens", "secret token password apikey");
        CHECK_FALSE(left.HasEngineSettingsChanges(right));

        right.SetSettingValue("Common.GameName", "OtherGame");
        CHECK(left.HasEngineSettingsChanges(right));

        right.SetSettingValue("Common.GameName", "FOnline");
        CHECK_FALSE(left.HasEngineSettingsChanges(right));

        right.SetSettingValue("Common.ResourcesPath", "Content/Other");
        CHECK(left.HasEngineSettingsChanges(right));

        right.SetSettingValue("Common.ResourcesPath", "Resources");
        CHECK_FALSE(left.HasEngineSettingsChanges(right));
    }

    SECTION("ResourcePackDeclarationsFollowNamesFlagsAndOrder")
    {
        GlobalSettings declared {false};
        ConfigFile declared_config {"Game.Value = one\n[ResourcePack]\nName = Alpha\n[ResourcePack]\nName = Beta\nServerOnly = true\n"};
        declared.ApplyConfigFile(declared_config, "");

        GlobalSettings same {false};
        ConfigFile same_config {"Game.Value = two\nGame.Caption =\n[ResourcePack]\nName = Alpha\n[ResourcePack]\nName = Beta\nServerOnly = true\n"};
        same.ApplyConfigFile(same_config, "");

        CHECK_FALSE(declared.HasEngineSettingsChanges(same));
        CHECK_FALSE(same.HasEngineSettingsChanges(declared));

        GlobalSettings renamed {false};
        ConfigFile renamed_config {"Game.Value = one\n[ResourcePack]\nName = Alpha\n[ResourcePack]\nName = Other\nServerOnly = true\n"};
        renamed.ApplyConfigFile(renamed_config, "");
        CHECK(declared.HasEngineSettingsChanges(renamed));
        CHECK(renamed.HasEngineSettingsChanges(declared));

        GlobalSettings reordered {false};
        ConfigFile reordered_config {"Game.Value = one\n[ResourcePack]\nName = Beta\nServerOnly = true\n[ResourcePack]\nName = Alpha\n"};
        reordered.ApplyConfigFile(reordered_config, "");
        CHECK(declared.HasEngineSettingsChanges(reordered));

        GlobalSettings role_changed {false};
        ConfigFile role_changed_config {"Game.Value = one\n[ResourcePack]\nName = Alpha\nClientOnly = true\n[ResourcePack]\nName = Beta\n"};
        role_changed.ApplyConfigFile(role_changed_config, "");
        CHECK(declared.HasEngineSettingsChanges(role_changed));
    }

    SECTION("RawProfileTextAndCommentsAreIgnored")
    {
        GlobalSettings left {false};
        left.SetupResourceConfig("Render.Sleep=7\nGame.Mode=one\n");

        GlobalSettings right {false};
        right.SetupResourceConfig("# comment\nRender.Sleep = 7 # trailing comment\nGame.Mode = two\nGame.Caption =\n");

        CHECK_FALSE(left.HasEngineSettingsChanges(right));
        CHECK_FALSE(right.HasEngineSettingsChanges(left));
    }

    SECTION("GameOnlyValuesCommandLineAndGenerationAreIgnored")
    {
        GlobalSettings base {false};
        ConfigFile base_config {"Game.Mode = one\n"};
        base.ApplyConfigFile(base_config, "");

        GlobalSettings updated {false};
        ConfigFile updated_config {"Game.Mode = two\nGame.Caption =\n"};
        updated.ApplyConfigFile(updated_config, "");

        char program[] {"lf_tests"};
        char option[] {"--Game.CliMode"};
        char value[] {"live"};
        char* argv[] {program, option, value};
        updated.ApplyCommandLine(CommandLineArgs {3, argv});

        CHECK(base.GetCustomSetting("Game.Mode") == "one");
        CHECK(updated.GetCustomSetting("Game.Mode") == "two");
        CHECK(updated.GetCustomSetting("Game.Caption").empty());
        CHECK(updated.GetCustomSetting("Game.CliMode") == "live");
        CHECK(updated.GetCustomSettingsGeneration() != base.GetCustomSettingsGeneration());
        CHECK_FALSE(updated.GetCommandLineArgs().empty());

        CHECK_FALSE(base.HasEngineSettingsChanges(updated));
        CHECK_FALSE(updated.HasEngineSettingsChanges(base));
    }
}

TEST_CASE("GameSettingsApply")
{
    SECTION("ReplacesOnlyCustomSettingsAndSelectedProfile")
    {
        string running_content {"Render.Sleep=7\n"
                                "Common.GameName=RunningGame\n"
                                "Common.SecretSettingTokens=old token\n"
                                "Game.Present=before\n"
                                "Game.Enabled=true\n"
                                "Game.Caption=old-caption\n"
                                "Game.Removed=old-nonzero\n"
                                "Game.Empty=\n"};
        string updated_content {"Render.Sleep=99\n"
                                "Common.GameName=UpdatedGame\n"
                                "Common.SecretSettingTokens=fresh token\n"
                                "Game.Present=after\n"
                                "Game.Enabled=false\n"
                                "Game.Caption=\n"
                                "Game.New=new-value\n"
                                "[ResourcePack]\n"
                                "Name=UpdatedPack\n"};

        GlobalSettings running {false};
        running.SetupResourceConfig(running_content);
        running.ApplyWritableRoot("running/root");

        GlobalSettings updated {false};
        updated.SetupResourceConfig(updated_content);
        updated.ApplyWritableRoot("updated/root");

        GlobalSettings native_snapshot {false};
        native_snapshot.CopyFrom(running);

        string_view game_name_view {running.Common.GameName.data(), running.Common.GameName.size()};
        const_span<string> secret_tokens_view {running.Common.SecretSettingTokens};
        uint64_t generation_before {running.GetCustomSettingsGeneration()};

        running.ApplyGameSettings(updated);

        CHECK(running.Common.GameName == "RunningGame");
        CHECK(running.Render.Sleep == 7);
        CHECK(running.Common.UserWritablePath == "running/root");
        CHECK(running.Common.SecretSettingTokens == vector<string> {"old", "token"});
        CHECK(running.Common.GameName.data() == game_name_view.data());
        CHECK(running.Common.SecretSettingTokens.data() == secret_tokens_view.data());
        CHECK(running.Common.SecretSettingTokens.size() == secret_tokens_view.size());
        CHECK_FALSE(running.HasEngineSettingsChanges(native_snapshot));
        CHECK(running.GetCustomSettingsGeneration() == generation_before + 1);

        REQUIRE(running.GetResourceConfigContent());
        CHECK(*running.GetResourceConfigContent() == updated_content);
        CHECK(running.HasCompleteConfig());

        CHECK(running.GetCustomSetting("Game.Present") == "after");
        CHECK(running.GetCustomSetting("Game.New") == "new-value");
        CHECK(running.GetCustomSetting("Game.Enabled") == "false");
        CHECK(running.GetCustomSetting("Game.Caption").empty());
        CHECK(static_cast<bool>(running.FindCustomSetting("Game.Enabled")));
        CHECK(static_cast<bool>(running.FindCustomSetting("Game.Caption")));
        CHECK_FALSE(static_cast<bool>(running.FindCustomSetting("Game.Removed")));
        CHECK_FALSE(static_cast<bool>(running.FindCustomSetting("Game.Empty")));
        CHECK(running.GetCustomSetting("Game.Removed").empty());

        CHECK_FALSE(static_cast<bool>(running.FindSettingValue("Game.Removed")));
        CHECK_FALSE(static_cast<bool>(running.FindSettingValue("Game.Empty")));

        auto new_value {running.FindSettingValue("Game.New")};
        REQUIRE(static_cast<bool>(new_value));
        CHECK(*new_value == "new-value");

        auto enabled_value {running.FindSettingValue("Game.Enabled")};
        REQUIRE(static_cast<bool>(enabled_value));
        CHECK(*enabled_value == "false");

        auto caption_value {running.FindSettingValue("Game.Caption")};
        REQUIRE(static_cast<bool>(caption_value));
        CHECK(caption_value->empty());

        auto native_value {running.FindSettingValue("Render.Sleep")};
        REQUIRE(static_cast<bool>(native_value));
        CHECK(*native_value == "7");

        auto game_name_value {running.FindSettingValue("Common.GameName")};
        REQUIRE(static_cast<bool>(game_name_value));
        CHECK(*game_name_value == "RunningGame");

        auto writable_value {running.FindSettingValue(SettingNames::Common_UserWritablePath)};
        REQUIRE(static_cast<bool>(writable_value));
        CHECK(*writable_value == "running/root");
    }

    SECTION("RuntimeCustomValuesWithoutSourceEntriesDoNotBringOne")
    {
        GlobalSettings running {false};
        running.SetupResourceConfig("Game.Source=old\n");

        GlobalSettings updated {false};
        updated.SetRuntimeSetting("Game.Source", "runtime");
        updated.SetRuntimeSetting("Game.OnlyRuntime", "value");

        running.ApplyGameSettings(updated);

        CHECK(running.GetCustomSetting("Game.Source") == "runtime");
        CHECK(running.GetCustomSetting("Game.OnlyRuntime") == "value");
        CHECK_FALSE(static_cast<bool>(running.FindSettingValue("Game.Source")));
        CHECK_FALSE(static_cast<bool>(running.FindSettingValue("Game.OnlyRuntime")));
    }

    SECTION("EmptyReplacementKeepsNativeStateUntilZeroCopy")
    {
        GlobalSettings running {false};
        running.SetupDefaultSettings();
        CHECK(running.Common.GameName == "FOnline");

        GlobalSettings empty {false};
        uint64_t generation_before {running.GetCustomSettingsGeneration()};
        running.ApplyGameSettings(empty);

        CHECK(running.Common.GameName == "FOnline");
        CHECK_FALSE(running.GetResourceConfigContent());
        CHECK_FALSE(running.HasCompleteConfig());
        CHECK(running.GetCustomSettingsGeneration() == generation_before + 1);

        running.CopyFrom(GlobalSettings {false});

        CHECK(running.Common.GameName.empty());
        CHECK(running.GetCommandLineArgs().empty());
    }

    SECTION("CopyFromKeepsReplacementSemantics")
    {
        GlobalSettings source {false};
        source.SetupResourceConfig("Render.Sleep=7\nGame.New=new\n[ResourcePack]\nName=Pack\n");

        GlobalSettings copied {false};
        copied.CopyFrom(source);

        CHECK(copied.Render.Sleep == 7);
        CHECK(copied.GetCustomSetting("Game.New") == "new");
        REQUIRE(copied.GetResourceConfigContent());
        CHECK(*copied.GetResourceConfigContent() == "Render.Sleep=7\nGame.New=new\n[ResourcePack]\nName=Pack\n");
        CHECK(copied.HasCompleteConfig());

        uint64_t generation {copied.GetCustomSettingsGeneration()};
        copied.CopyFrom(GlobalSettings {false});

        CHECK(copied.Render.Sleep == 0);
        CHECK_FALSE(static_cast<bool>(copied.FindCustomSetting("Game.New")));
        CHECK_FALSE(copied.GetResourceConfigContent());
        CHECK_FALSE(copied.HasCompleteConfig());
        CHECK(copied.GetCustomSettingsGeneration() > generation);
    }
}

TEST_CASE("CommandLineSettingsReplay")
{
    SECTION("OwnedOperationsSurviveSourceBuffersAndReplay")
    {
        vector<string> values {"lf_tests", "--Common.SecretSettingTokens", "base", "-Common.SecretSettingTokens", "+tail", "--Probe.Flag", "--Probe.Empty", "", "--Render.Sleep", "-2", "--Probe.Raw", " C:\\path\\\"quoted\" ", "--ApplySubConfig", "Ignored", "--Common.UserWritablePath", "ignored-root"};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        GlobalSettings settings {false};
        settings.SetupDefaultSettings();
        settings.ApplyCommandLine(CommandLineArgs {argv});

        CHECK(settings.Common.SecretSettingTokens == vector<string> {"base", "tail"});
        CHECK(settings.Render.Sleep == -2);
        CHECK(settings.GetCustomSetting("Probe.Flag") == "1");
        CHECK(settings.GetCustomSetting("Probe.Empty").empty());
        CHECK(static_cast<bool>(settings.FindCustomSetting("Probe.Empty")));
        CHECK(settings.GetCustomSetting("Probe.Raw") == "C:\\path\\\"quoted\"");
        CHECK(settings.Common.UserWritablePath.empty());

        values.clear();
        argv.clear();

        auto replay {settings.GetCommandLineArgs()};
        auto replay_settings {replay.GetSettings()};

        REQUIRE(replay_settings.size() == 6);
        CHECK(replay_settings[0].Name == SettingNames::Common_SecretSettingTokens);
        CHECK(replay_settings[0].Value == "base");
        CHECK(replay_settings[1].Name == SettingNames::Common_SecretSettingTokens);
        CHECK(replay_settings[1].Value == "+tail");
        CHECK(replay_settings[2].Name == "Probe.Flag");
        CHECK(replay_settings[2].Value == "1");
        CHECK(replay_settings[3].Name == "Probe.Empty");
        REQUIRE(replay_settings[3].Value.has_value());
        CHECK(replay_settings[3].Value->empty());
        CHECK(replay_settings[4].Name == "Render.Sleep");
        CHECK(replay_settings[4].Value == "-2");
        CHECK(replay_settings[5].Name == "Probe.Raw");
        CHECK(replay_settings[5].Value == "C:\\path\\\"quoted\"");
        CHECK_FALSE(replay.GetSettingValue(CommandLineSettings::ApplySubConfig).has_value());
        CHECK_FALSE(replay.GetSettingValue(SettingNames::Common_UserWritablePath).has_value());

        GlobalSettings replayed {false};
        replayed.SetupDefaultSettings();
        replayed.ApplyCommandLine(replay);

        CHECK(replayed.Common.SecretSettingTokens == settings.Common.SecretSettingTokens);
        CHECK(replayed.Render.Sleep == -2);
        CHECK(replayed.GetCustomSetting("Probe.Flag") == "1");
        CHECK(replayed.GetCustomSetting("Probe.Empty").empty());
        CHECK(replayed.GetCustomSetting("Probe.Raw") == "C:\\path\\\"quoted\"");
    }

    SECTION("MultipleCallsKeepOrderRepeatedKeysAndAppend")
    {
        GlobalSettings settings {false};
        settings.SetupDefaultSettings();

        char program[] {"lf_tests"};
        char token_option[] {"--Common.SecretSettingTokens"};
        char token_value[] {"base"};
        char append_option[] {"-Common.SecretSettingTokens"};
        char append_value[] {"+tail"};
        char probe_option[] {"--Probe.Value"};
        char first_value[] {"first"};
        char second_value[] {"second"};
        vector<CommandLineArg> first_argv {program, token_option, token_value};

        settings.ApplyCommandLine(CommandLineArgs {first_argv});

        vector<CommandLineArg> second_argv {program, append_option, append_value, probe_option, first_value, probe_option, second_value};

        settings.ApplyCommandLine(CommandLineArgs {second_argv});

        CHECK(settings.Common.SecretSettingTokens == vector<string> {"base", "tail"});
        CHECK(settings.GetCustomSetting("Probe.Value") == "second");

        auto history_args {settings.GetCommandLineArgs()};
        auto history {history_args.GetSettings()};

        REQUIRE(history.size() == 4);
        CHECK(history[0].Name == SettingNames::Common_SecretSettingTokens);
        CHECK(history[0].Value == "base");
        CHECK(history[1].Name == SettingNames::Common_SecretSettingTokens);
        CHECK(history[1].Value == "+tail");
        CHECK(history[2].Name == "Probe.Value");
        CHECK(history[2].Value == "first");
        CHECK(history[3].Name == "Probe.Value");
        CHECK(history[3].Value == "second");

        GlobalSettings replayed {false};
        replayed.SetupDefaultSettings();
        replayed.ApplyCommandLine(history_args);

        CHECK(replayed.Common.SecretSettingTokens == vector<string> {"base", "tail"});
        CHECK(replayed.GetCustomSetting("Probe.Value") == "second");
    }

    SECTION("SelfReplayKeepsInputViewsValid")
    {
        vector<string> values {"lf_tests", "--Common.SecretSettingTokens", "base", "-Common.SecretSettingTokens", "+tail", "--Probe.Value", "first", "-Probe.Value", "second"};
        vector<CommandLineArg> argv {};

        for (string& value : values) {
            argv.emplace_back(value.data());
        }

        GlobalSettings settings {false};
        settings.SetupDefaultSettings();
        settings.ApplyCommandLine(CommandLineArgs {argv});

        size_t operation_count {settings.GetCommandLineArgs().GetSettings().size()};
        REQUIRE(operation_count == 4);

        values.clear();
        argv.clear();

        settings.ApplyCommandLine(settings.GetCommandLineArgs());

        CHECK(settings.Common.SecretSettingTokens == vector<string> {"base", "tail"});
        CHECK(settings.GetCustomSetting("Probe.Value") == "second");
        CHECK(settings.GetCommandLineArgs().GetSettings().size() == operation_count * 2);
    }

    SECTION("CopyClearAndGameSettingsRetainOrResetOwnedReplay")
    {
        GlobalSettings running {false};
        running.SetupDefaultSettings();

        char program[] {"lf_tests"};
        char cli_mode[] {"--Game.CliMode"};
        char live[] {"live"};
        char* argv[] {program, cli_mode, live};
        running.ApplyCommandLine(CommandLineArgs {3, argv});

        GlobalSettings updated {false};
        updated.SetupResourceConfig("Game.Profile=new\nGame.CliMode=live\n");

        char updated_option[] {"--Game.UpdatedCli"};
        char updated_value[] {"other"};
        char* updated_argv[] {program, updated_option, updated_value};
        updated.ApplyCommandLine(CommandLineArgs {3, updated_argv});

        size_t history_size {running.GetCommandLineArgs().GetSettings().size()};
        running.ApplyGameSettings(updated);

        CHECK(running.GetCustomSetting("Game.Profile") == "new");
        CHECK(running.GetCustomSetting("Game.CliMode") == "live");
        CHECK(running.GetCustomSetting("Game.UpdatedCli") == "other");
        CHECK(running.GetCommandLineArgs().GetSettings().size() == history_size);
        CHECK(running.GetCommandLineArgs().GetSettingValue("Game.CliMode") == "live");
        CHECK_FALSE(running.GetCommandLineArgs().GetSettingValue("Game.UpdatedCli").has_value());

        GlobalSettings candidate {false};
        candidate.SetupResourceConfig("Game.Profile=new\nGame.CliMode=live\n");
        candidate.ApplyCommandLine(running.GetCommandLineArgs());

        CHECK(candidate.GetCustomSetting("Game.Profile") == "new");
        CHECK(candidate.GetCustomSetting("Game.CliMode") == "live");

        GlobalSettings copied {false};
        copied.CopyFrom(running);

        CHECK(copied.GetCustomSetting("Game.Profile") == "new");
        CHECK(copied.GetCommandLineArgs().GetSettings().size() == history_size);
        CHECK(copied.GetCommandLineArgs().GetSettingValue("Game.CliMode") == "live");

        uint64_t generation {copied.GetCustomSettingsGeneration()};
        copied.CopyFrom(GlobalSettings {false});

        CHECK(copied.GetCommandLineArgs().empty());
        CHECK_FALSE(static_cast<bool>(copied.FindCustomSetting("Game.Profile")));
        CHECK_FALSE(copied.GetResourceConfigContent());
        CHECK(copied.GetCustomSettingsGeneration() > generation);
    }
}

TEST_CASE("CommandLineReplayPreservesOptionLikeValues")
{
    char program[] {"lf_tests"};
    char option[] {"--Game.Caption"};
    char value[] {" --Render.Sleep "};
    vector<CommandLineArg> args {program, option, value};
    GlobalSettings running {false};
    running.ApplyCommandLine(CommandLineArgs {args});
    REQUIRE(running.GetCustomSetting("Game.Caption") == "--Render.Sleep");
    REQUIRE(running.Render.Sleep == 0);

    GlobalSettings replayed {false};
    replayed.ApplyCommandLine(running.GetCommandLineArgs());
    CHECK(replayed.GetCustomSetting("Game.Caption") == "--Render.Sleep");
    CHECK(replayed.Render.Sleep == 0);
}

TEST_CASE("GameSettingsApplyPreservesNativeSaveMembership")
{
    GlobalSettings running {true};
    running.SetupResourceConfig("Render.Sleep=7\nGame.Removed=old\n");
    GlobalSettings updated {true};
    updated.SetupResourceConfig("Client.Language=ru18\nGame.New=new\n");
    running.ApplyGameSettings(updated);
    auto values {running.Save()};
    CHECK(values.contains("Render.Sleep"));
    if (values.contains("Render.Sleep")) {
        CHECK(values.at("Render.Sleep") == "7");
    }
    CHECK_FALSE(values.contains("Client.Language"));
    CHECK_FALSE(values.contains("Game.Removed"));
    CHECK(values.at("Game.New") == "new");
    CHECK(running.Render.Sleep == 7);
    CHECK(running.Client.Language.empty());
}

TEST_CASE("AppSettingsRefreshRetainsPreferencesCliAndStartupState")
{
    string directory {MakeTempSettingsDir("settings_refresh_overrides")};
    auto cleanup {scope_exit([&directory]() noexcept { (void)fs::remove_dir_tree(directory); })};
    string cache_dir {fs::make_writable_path(directory, "selected-cache")};
    CacheStorage cache {cache_dir};
    cache.SetString(LOCAL_CONFIG_NAME, "Client.Language=engl\nGame.Local=preference\nGame.CliMode=preference\n");

    GlobalSettings running {false};
    running.SetupResourceConfig("Common.GameName=StableGame\nCommon.CachePath=profile-cache\n"
                                "Common.SecretSettingTokens=base\nClient.Language=russ\nRender.Sleep=7\n"
                                "Game.Value=before\nGame.Removed=stale\nGame.Explicit=profile\n");
    running.ApplyWritableRoot(directory);
    vector<string> values {"lf_tests", "--Common.CachePath", "selected-cache", "--Client.Language", "ru18", "--Render.Sleep", "-2", "--Game.CliMode", "live"};
    vector<CommandLineArg> args {};

    for (string& value : values) {
        args.emplace_back(value.data());
    }

    ApplyAppSettingsOverrides(running, CommandLineArgs {args});
    running.SetSettingValue("Common.NonInteractive", "True");
    running.SetSettingValue("Common.SecretSettingTokens", "+tail");
    running.SetSettingValue("Game.Explicit", "  keep + exact  ");
    BakerTests::OverrideSetting(running.Common.Packaged, true);
    BakerTests::OverrideSetting(running.View.MonitorWidth, 1920);
    BakerTests::OverrideSetting(running.View.MonitorHeight, 1080);

    values.clear();
    args.clear();

    string next_profile {"Common.GameName=StableGame\nCommon.CachePath=profile-cache\n"
                         "Common.SecretSettingTokens=base\nClient.Language=russ\nRender.Sleep=9\n"
                         "Game.Value=after\nGame.Explicit=profile\n"};
    GlobalSettings candidate {false};
    candidate.SetupResourceConfig(next_profile);
    candidate.ApplyWritableRoot(directory);
    ApplyAppSettingsOverrides(candidate, running.GetCommandLineArgs());
    CHECK(running.HasEngineSettingsChanges(candidate));
    candidate.ApplyStartupOverrides(running);
    CHECK_FALSE(running.HasEngineSettingsChanges(candidate));
    CHECK(candidate.Common.NonInteractive);
    CHECK(candidate.Common.Packaged);
    CHECK(candidate.View.MonitorWidth == 1920);
    CHECK(candidate.View.MonitorHeight == 1080);
    CHECK(candidate.Common.SecretSettingTokens == vector<string> {"base", "tail"});
    CHECK(candidate.Client.Language == "ru18");
    CHECK(candidate.Render.Sleep == -2);
    CHECK(candidate.GetCustomSetting("Game.Explicit") == "  keep + exact  ");

    string_view game_name_view {running.Common.GameName};
    uint64_t generation {running.GetCustomSettingsGeneration()};
    running.ApplyGameSettings(candidate);
    CHECK(running.Common.GameName.data() == game_name_view.data());
    CHECK(running.GetCustomSettingsGeneration() == generation + 1);
    CHECK(running.GetResourceConfigContent() == next_profile);
    CHECK(running.GetCustomSetting("Game.Value") == "after");
    CHECK(running.GetCustomSetting("Game.Local") == "preference");
    CHECK(running.GetCustomSetting("Game.CliMode") == "live");
    CHECK(running.GetCustomSetting("Game.Explicit") == "  keep + exact  ");
    CHECK_FALSE(running.FindCustomSetting("Game.Removed"));

    GlobalSettings copied {false};
    copied.CopyFrom(running);
    GlobalSettings copied_candidate {false};
    copied_candidate.SetupResourceConfig(next_profile);
    copied_candidate.ApplyStartupOverrides(copied);
    CHECK(copied_candidate.Common.NonInteractive);
    CHECK(copied_candidate.GetCustomSetting("Game.Explicit") == "  keep + exact  ");
    copied.CopyFrom(GlobalSettings {false});
    GlobalSettings cleared_candidate {false};
    cleared_candidate.SetupResourceConfig(next_profile);
    cleared_candidate.ApplyStartupOverrides(copied);
    CHECK_FALSE(cleared_candidate.Common.NonInteractive);
    CHECK(cleared_candidate.GetCustomSetting("Game.Explicit") == "profile");
}

TEST_CASE("ResourceConfigurationRequiresFreshSettings", "[settings]")
{
    GlobalSettings settings {false};

    SECTION("ResourceProfileCannotBeAppliedTwice")
    {
        settings.SetupResourceConfig("Render.Sleep=7\n");
    }

    SECTION("EarlierConfigCannotBeReplaced")
    {
        ConfigFile config {"Render.Sleep=7\n"};
        settings.ApplyConfigFile(config, "");
    }

    SECTION("EarlierRuntimeGameValuesCannotBeReplaced")
    {
        settings.SetRuntimeSetting("Game.Keep", "value");
    }

    SECTION("EarlierWritableRootCannotBeReplaced")
    {
        settings.ApplyWritableRoot("keep-root");
    }

    SECTION("EarlierResourceDeclarationsCannotBeReplaced")
    {
        ConfigFile config {"[ResourcePack]\nName=KeepPack\n"};
        settings.ApplyConfigFile(config, "");
    }

    SECTION("PackagedIdentityCannotPrecedeResourceProfile")
    {
        settings.ApplyAutoSettings("Prior_Runtime");
    }

    SECTION("ReferenceDefaultsCannotPrecedeResourceProfile")
    {
        settings.SetupDefaultSettings();
    }

    SECTION("AutoSettingsCannotPrecedeResourceProfile")
    {
        settings.ApplyAutoSettings({});
    }

    auto raw_before {settings.GetResourceConfigContent()};
    int32_t sleep_before {settings.Render.Sleep};
    CHECK_THROWS_AS(settings.SetupResourceConfig("Game.New=next\n"), VerificationException);
    CHECK(settings.GetResourceConfigContent() == raw_before);
    CHECK(settings.Render.Sleep == sleep_before);
    CHECK_FALSE(settings.FindCustomSetting("Game.New"));
}

TEST_CASE("PackagedRuntimeIdentityFollowsSettingsSnapshot")
{
    GlobalSettings settings {false};
    ConfigFile config {"Common.Packaged=0\n"};
    settings.ApplyConfigFile(config, "");
    string runtime_name {"Published_Headless_Variant"};
    settings.ApplyAutoSettings(runtime_name);
    runtime_name = "changed-source";

    CHECK(settings.Common.Packaged);
    CHECK(settings.GetPackagedRuntimeName() == "Published_Headless_Variant");
    settings.ApplyAutoSettings(settings.GetPackagedRuntimeName());
    CHECK(settings.GetPackagedRuntimeName() == "Published_Headless_Variant");

    GlobalSettings copied {false};
    copied.CopyFrom(settings);
    CHECK(copied.GetPackagedRuntimeName() == "Published_Headless_Variant");
    CHECK_FALSE(copied.HasEngineSettingsChanges(settings));

    GlobalSettings candidate {false};
    candidate.ApplyAutoSettings("Another_Runtime");
    CHECK(candidate.Common.Packaged);
    CHECK(settings.HasEngineSettingsChanges(candidate));
    candidate.ApplyStartupOverrides(settings);
    CHECK(candidate.GetPackagedRuntimeName() == "Published_Headless_Variant");
    CHECK_FALSE(settings.HasEngineSettingsChanges(candidate));

    GlobalSettings empty_game_settings {false};
    string_view name_before {settings.GetPackagedRuntimeName()};
    settings.ApplyGameSettings(empty_game_settings);
    CHECK(settings.Common.Packaged);
    CHECK(settings.GetPackagedRuntimeName() == "Published_Headless_Variant");
    CHECK(settings.GetPackagedRuntimeName().data() == name_before.data());

    copied.CopyFrom(GlobalSettings {false});
    CHECK_FALSE(copied.Common.Packaged);
    CHECK(copied.GetPackagedRuntimeName().empty());
}

TEST_CASE("ResourceConfigReaderLeavesCompatibilityToBootstrap")
{
    string directory {MakeTempSettingsDir("metadata_reader_policy")};
    auto cleanup {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(directory); })};
    REQUIRE(fs::create_directories(directory));
    string content {"#different-runtime\nCommon.GameVersion=reader-content\n"};
    REQUIRE(fs::write_file(strex(directory).combine_path("Selected.fomain-client").str(), content));
    FileSystem metadata_source {};
    metadata_source.AddDirSource(directory, true);

    CHECK(ReadResourceConfig(metadata_source, false) == content);
    CHECK(ReadResourceConfig(metadata_source, true) == content);
}

FO_END_NAMESPACE
