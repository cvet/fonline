#include "catch_amalgamated.hpp"

#include "Application.h"
#include "ConfigFile.h"
#include "DiskFileSystem.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static_assert(std::same_as<CommandLineArg, u8string_view>);
static_assert(std::same_as<decltype(std::declval<const CommandLineArgs&>().Get(size_t {})), u8string_view>);

static auto MakeTempSettingsDir(string_view name) -> u8string
{
    auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_u8string(base);
}

static auto MakeSettingsConfig(string_view name, string_view text) -> ConfigFile
{
    (void)name;
    u8string strict_text = text;
    return ConfigFile {std::move(strict_text)};
}

TEST_CASE("Settings")
{
    SECTION("GetResourcePacksThrowsWhenNothingApplied")
    {
        GlobalSettings settings {false};

        CHECK_THROWS_AS(settings.GetResourcePacks(), SettingsException);
    }

    SECTION("SubConfigMultiParentMergesLeftToRight")
    {
        GlobalSettings settings {false};
        ConfigFile config = MakeSettingsConfig("Test.fomain",
            "[SubConfig]\n"
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
            "Own = child\n");

        settings.ApplyConfigFile(config, u8"cfg");
        settings.ApplySubConfigSection("Mixed");

        // Multiple parents merge (not replace): both parents' unique keys survive, later parents
        // override earlier ones on shared keys, and the child's own settings sit on top.
        CHECK(settings.GetCustomSetting("OnlyA") == "fromA");
        CHECK(settings.GetCustomSetting("OnlyB") == "fromB");
        CHECK(settings.GetCustomSetting("Shared") == "fromB");
        CHECK(settings.GetCustomSetting("Own") == "child");
    }

    SECTION("ApplyConfigFileParsesSubConfigsAndResourcePacks")
    {
        GlobalSettings settings {false};
        ConfigFile config = MakeSettingsConfig("Test.fomain",
            "UnknownSetting = root\n"
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
            "ServerOnly = true\n");

        settings.ApplyConfigFile(config, u8"cfg");

        CHECK(settings.GetCustomSetting("UnknownSetting") == "root");
        REQUIRE(settings.GetSubConfigs().size() == 2);
        CHECK(settings.GetSubConfigs()[0].Name == "Base");
        CHECK(settings.GetSubConfigs()[1].Name == "Child");

        settings.ApplySubConfigSection("Child");
        CHECK(settings.GetCustomSetting("Shared") == "parent");
        CHECK(settings.GetCustomSetting("Mode") == "child");
        CHECK(settings.GetCustomSetting("Leaf") == "extra");

        auto packs = settings.GetResourcePacks();
        REQUIRE(packs.size() == 2);
        CHECK(packs[0].Name == "CommonPack");
        REQUIRE(packs[0].InputDirs.size() == 2);
        CHECK(packs[0].InputDirs[0] == fs_combine_path(u8"cfg", "dirA"));
        CHECK(packs[0].InputDirs[1] == fs_combine_path(u8"cfg", "dirB"));
        REQUIRE(packs[0].InputFiles.size() == 2);
        CHECK(packs[0].InputFiles[0] == fs_combine_path(u8"cfg", "fileA"));
        CHECK(packs[0].InputFiles[1] == fs_combine_path(u8"cfg", "fileB"));
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

    SECTION("Utf8PathSettingsAndResourceInputsPreserveUnicode")
    {
        GlobalSettings settings {false};
        ConfigFile config {u8string {u8"Common.GameName = Последний рубеж 🌍\n"
                                     u8"Baking.BakeOutput = данные/мир-🌍\n"
                                     u8"[ResourcePack]\n"
                                     u8"Name = UnicodePack\n"
                                     u8"InputDirs = ресурсы/карты\n"
                                     u8"InputFiles = архивы/текстуры.zip\n"
                                     u8"Bakers = RawCopy\n"}};
        u8string config_dir {u8"корень/конфигурация"};

        settings.ApplyConfigFile(config, config_dir.view());

        CHECK(settings.BakeOutput.view() == u8"данные/мир-🌍");
        CHECK(settings.GameName.view() == u8"Последний рубеж 🌍");
        auto packs = settings.GetResourcePacks();
        REQUIRE(packs.size() == 1);
        REQUIRE(packs[0].InputDirs.size() == 1);
        REQUIRE(packs[0].InputFiles.size() == 1);

        u8string expected_dir = fs_path_to_u8string(std::filesystem::path {fs_make_path(config_dir.view())} / std::filesystem::path {u8"ресурсы/карты"});
        u8string expected_file = fs_path_to_u8string(std::filesystem::path {fs_make_path(config_dir.view())} / std::filesystem::path {u8"архивы/текстуры.zip"});
        CHECK(packs[0].InputDirs[0] == expected_dir);
        CHECK(packs[0].InputFiles[0] == expected_file);
    }

    SECTION("AsciiSettingRejectsUnicode")
    {
        GlobalSettings settings {false};
        ConfigFile config {u8string {u8"Client.Language = русский\n"}};

        try {
            settings.ApplyConfigFile(config, u8string_view {});
            FAIL("A Unicode value was accepted by an ASCII setting");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Ascii);
            CHECK(ex.error() == TextValidationError::NonAsciiByte);
            CHECK(ex.offset() == 0);
        }
    }

#if !FO_WINDOWS
    SECTION("EnvironmentExpansionPreservesUtf8AndRejectsMalformedBytes")
    {
        constexpr const char* variable_name = "FO_SETTINGS_UTF8_ENV_TEST";
        nptr<const char> original_value {std::getenv(variable_name)};
        optional<string> saved_value = original_value ? optional<string> {string {original_value.get()}} : optional<string> {};
        auto restore_env = scope_exit([&]() noexcept {
            if (saved_value) {
                (void)setenv(variable_name, saved_value->c_str(), 1);
            }
            else {
                (void)unsetenv(variable_name);
            }
        });

        u8string unicode_value {u8"Последний рубеж 🌍"};
        ptr<const char> unicode_value_cstr = utf8_to_c_str(unicode_value.view_nt());
        REQUIRE(setenv(variable_name, unicode_value_cstr.get(), 1) == 0);

        GlobalSettings valid_settings {false};
        ConfigFile valid_config = MakeSettingsConfig("Environment.fomain", "Common.GameName = $ENV{FO_SETTINGS_UTF8_ENV_TEST}\n");
        valid_settings.ApplyConfigFile(valid_config, u8string_view {});
        CHECK(valid_settings.GameName == unicode_value);

        array<char, 2> invalid_value = {std::bit_cast<char>(uint8_t {0xFF}), char {}};
        REQUIRE(setenv(variable_name, invalid_value.data(), 1) == 0);

        GlobalSettings invalid_settings {false};
        ConfigFile invalid_config = MakeSettingsConfig("InvalidEnvironment.fomain", "Common.GameName = $ENV{FO_SETTINGS_UTF8_ENV_TEST}\n");
        CHECK_THROWS_AS(invalid_settings.ApplyConfigFile(invalid_config, u8string_view {}), TextValidationException);
    }
#endif

    SECTION("ApplyCommandLineSetsCustomValuesAndImplicitFlags")
    {
        GlobalSettings settings {false};
        array<CommandLineArg, 4> argv = {
            u8"lf_tests",
            u8"--CustomCli",
            u8"123",
            u8"--FlagOnly",
        };

        settings.ApplyCommandLine(CommandLineArgs {argv});

        CHECK(settings.GetCustomSetting("CustomCli") == "123");
        CHECK(settings.GetCustomSetting("FlagOnly") == "1");
    }

    SECTION("CommandLineArgsAcceptEmptyNativeArgv")
    {
        CommandLineArgs args {0, nullptr};

        CHECK(args.empty());
    }

    SECTION("CommandLineArgsOwnValidatedUtf8")
    {
        u8string executable {u8"lf_тесты"};
        u8string option {u8"--CustomCli"};
        u8string value {u8"значение-𐍈"};
        array<CommandLineArg, 3> argv = {executable.view(), option.view(), value.view()};
        CommandLineArgs args {argv};

        executable.assign(u8"changed");
        option.assign(u8"changed");
        value.assign(u8"changed");

        CHECK(args.Get(0) == u8"lf_тесты");
        CHECK(args.Get(1) == u8"--CustomCli");
        CHECK(args.Get(2) == u8"значение-𐍈");
        CHECK(args.Get(3).empty());
        CHECK(CommandLineArgs::IsOption(args.Get(1)));
        CHECK_FALSE(CommandLineArgs::IsOption(args.Get(2)));
    }

    SECTION("CommandLineArgsRejectMalformedNativeUtf8")
    {
        char malformed[] = {'x', std::bit_cast<char>(uint8_t {0xFF}), char {}};
        char* argv[] = {malformed};

        try {
            (void)CommandLineArgs {1, argv};
            FAIL("Malformed native command-line UTF-8 was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::ScalarOutOfRange);
            CHECK(ex.offset() == 1);
        }
    }

    SECTION("CommandLineArgsRejectEmbeddedNullInStrictViews")
    {
        array<CommandLineArg, 1> argv = {u8"a\0b"};

        try {
            (void)CommandLineArgs {argv};
            FAIL("An embedded null was accepted in a strict command-line argument");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::EmbeddedNull);
            CHECK(ex.offset() == 1);
        }
    }

    SECTION("ApplyCommandLineMasksSecretValuesInLog")
    {
        // Capture the "Set <name> to <value>" lines emitted by the logging pass.
        u8string captured;
        SetLogCallback("settings_secret_redaction_test", [&captured](LogType, u8string_view message, nptr<const CatchedStackTraceData>) { captured.append(message); });
        auto remove_callback = scope_exit([]() noexcept { SetLogCallback("settings_secret_redaction_test", nullptr); });

        GlobalSettings settings {false};
        // Real flow logs command-line overrides only after defaults (and the config) are applied, so the
        // Common.SecretSettingTokens list (default includes "token") is populated by then.
        settings.ApplyDefaultSettings();

        char arg0[] = "lf_tests";
        char arg1[] = "--Probe.AccessToken";
        char arg2[] = "super-secret-value";
        char arg3[] = "--Common.GameName";
        char arg4[] = "RedactionProbe";
        char* argv[] = {arg0, arg1, arg2, arg3, arg4};

        settings.ApplyCommandLine(CommandLineArgs {5, argv});

        // A name matching a secret token (Common.SecretSettingTokens, default includes "token") is masked;
        // the credential value itself must never reach the log.
        CHECK(captured.view().native_view().find(u8"Set Probe.AccessToken to ***") != std::u8string_view::npos);
        CHECK(captured.view().native_view().find(u8"super-secret-value") == std::u8string_view::npos);

        // A non-secret name is logged verbatim, and both values are still applied.
        CHECK(captured.view().native_view().find(u8"Set Common.GameName to RedactionProbe") != std::u8string_view::npos);
        CHECK(settings.GetCustomSetting("Probe.AccessToken") == "super-secret-value");
        CHECK(settings.GameName.view() == u8"RedactionProbe");
    }

    SECTION("ApplyCommandLineAppendAccumulatesPerCall")
    {
        // '+'-prefixed overrides append to the current value, so applying the same command line twice to
        // one settings object doubles the result. LoadAppSettings() therefore applies the command line to
        // the live settings exactly once — this test pins the hazard that the single-application flow
        // must avoid (it was a real bug while the command line was applied in two passes).
        GlobalSettings settings {false};
        char arg0[] = "lf_tests";
        char arg1[] = "--Common.GameName";
        char arg2[] = "+Tag";
        char* argv[] = {arg0, arg1, arg2};

        settings.ApplyCommandLine(CommandLineArgs {3, argv});
        CHECK(settings.GameName.view() == u8"Tag");

        // A second pass over the same object appends again — what the two-pass flow used to do.
        settings.ApplyCommandLine(CommandLineArgs {3, argv});
        CHECK(settings.GameName.view() == u8"Tag Tag");
    }

    SECTION("ApplyConfigAtPathResolvesFileVariables")
    {
        u8string temp_dir = MakeTempSettingsDir("settings_config");
        bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        u8string payload_path = fs_combine_path(temp_dir.view(), "payload.txt");
        u8string config_path = fs_combine_path(temp_dir.view(), "main.fomain");
        REQUIRE(fs_write_file_text(payload_path.view(), u8"  loaded value  "));
        REQUIRE(fs_write_file_text(config_path.view(), u8"ExternalValue = $FILE{payload.txt}\n"));

        GlobalSettings settings {false};
        settings.ApplyConfigAtPath(u8"main.fomain", temp_dir.view());

        CHECK(settings.GetAppliedConfigs().size() == 1);
        CHECK(settings.GetCustomSetting("ExternalValue") == "loaded value");

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("ApplyConfigAtPathThrowsForMissingConfig")
    {
        GlobalSettings settings {false};

        CHECK_THROWS_AS(settings.ApplyConfigAtPath(u8"missing.fomain", u8"/tmp/not_there"), SettingsException);
    }

    SECTION("FindCustomSettingReturnsNullableLookup")
    {
        GlobalSettings settings {false};

        auto missing = settings.FindCustomSetting("Missing");
        CHECK_FALSE(static_cast<bool>(missing));
        CHECK(settings.GetCustomSetting("Missing").empty());

        settings.SetCustomSetting("Present", any_t(string("value")));
        auto present = settings.FindCustomSetting("Present");

        REQUIRE(static_cast<bool>(present));
        CHECK(*present == "value");
        CHECK(settings.GetCustomSetting("Present") == "value");
    }

    SECTION("BakingModeSaveReturnsAppliedSettings")
    {
        GlobalSettings settings {true};
        ConfigFile config = MakeSettingsConfig("Bake.fomain", "CustomSaved = value\n");

        settings.ApplyConfigFile(config, u8string_view {});

        auto saved = settings.Save();
        auto it = saved.find("CustomSaved");
        REQUIRE(it != saved.end());
        CHECK(it->second.view() == u8"value");
    }

    SECTION("UpdateFilesInMemoryCanBeOverriddenBySubConfigs")
    {
        GlobalSettings settings {false};
        ConfigFile config = MakeSettingsConfig("Test.fomain",
            "ServerNetwork.UpdateFilesInMemory = False\n"
            "[SubConfig]\n"
            "Name = PublicGame\n"
            "ServerNetwork.UpdateFilesInMemory = True\n"
            "[SubConfig]\n"
            "Name = Staging\n"
            "Parent = PublicGame\n"
            "ServerNetwork.UpdateFilesInMemory = False\n");

        settings.ApplyConfigFile(config, u8"cfg");
        settings.ApplySubConfigSection("PublicGame");

        CHECK(settings.UpdateFilesInMemory);

        settings.ApplySubConfigSection("Staging");

        CHECK_FALSE(settings.UpdateFilesInMemory);
    }

    SECTION("ResolveUserWritablePathInstalledExplicitPathCreatesTree")
    {
        GlobalSettings settings {false};

        // An explicit absolute path is the installed layout: resolve it, create it, and pre-create the
        // cache + resource-overlay subdirs under it.
        auto root = MakeTempSettingsDir("settings_writable_root");
        (void)fs_remove_dir_tree(root.view());

        settings.UserWritablePath = root;
        ResolveUserWritablePath(settings);

        u8string resolved_root = settings.UserWritablePath;
        u8string cache_resources = settings.CacheResources;
        u8string client_resources = settings.ClientResources;
        u8string expected_root = fs_resolve_path(root.view());
        u8string cache_path = fs_make_writable_path(resolved_root.view(), cache_resources.view());
        u8string client_resources_path = fs_make_writable_path(resolved_root.view(), client_resources.view());
        CHECK(resolved_root == expected_root);
        CHECK(fs_is_dir(resolved_root.view()));
        CHECK(fs_is_dir(cache_path.view()));
        CHECK(fs_is_dir(client_resources_path.view()));

        (void)fs_remove_dir_tree(root.view());
    }

    SECTION("ResolveUserWritablePathPortableStaysEmpty")
    {
        GlobalSettings settings {false};

        // No explicit path and no installer marker next to the test exe: stay portable (empty).
        settings.UserWritablePath = u8string {};
        ResolveUserWritablePath(settings);

        CHECK(settings.UserWritablePath.empty());
    }

    SECTION("ResolveUserWritablePathFailsafeRevertsToPortable")
    {
        GlobalSettings settings {false};

        // A root whose parent is a regular file can't be created: the resolver must fail safe to portable
        // rather than brick startup.
        auto temp_dir = MakeTempSettingsDir("settings_writable_blocker");
        (void)fs_remove_dir_tree(temp_dir.view());
        auto blocker = fs_combine_path(temp_dir.view(), "blocker");
        auto blocked_root = fs_combine_path(blocker.view(), "sub");
        REQUIRE(fs_write_file_text(blocker.view(), u8"x"));

        settings.UserWritablePath = blocked_root;
        ResolveUserWritablePath(settings);

        CHECK(settings.UserWritablePath.empty());

        (void)fs_remove_dir_tree(temp_dir.view());
    }
}

FO_END_NAMESPACE
