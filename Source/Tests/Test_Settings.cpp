#include "catch_amalgamated.hpp"

#include "Application.h"
#include "ConfigFile.h"
#include "DiskFileSystem.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static auto MakeTempSettingsDir(string_view name) -> string
{
    const auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_string(base);
}

TEST_CASE("Settings")
{
    SECTION("GetResourcePacksThrowsWhenNothingApplied")
    {
        GlobalSettings settings {false};

        CHECK_THROWS_AS(settings.GetResourcePacks(), SettingsException);
    }

    SECTION("ApplyConfigFileParsesSubConfigsAndResourcePacks")
    {
        GlobalSettings settings {false};
        ConfigFile config {"Test.fomain",
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
            "RecursiveInput = true\n"
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

        const auto packs = settings.GetResourcePacks();
        REQUIRE(packs.size() == 2);
        CHECK(packs[0].Name == "CommonPack");
        REQUIRE(packs[0].InputDirs.size() == 2);
        CHECK(packs[0].InputDirs[0] == strex("cfg").combine_path("dirA").str());
        CHECK(packs[0].InputDirs[1] == strex("cfg").combine_path("dirB").str());
        REQUIRE(packs[0].InputFiles.size() == 2);
        CHECK(packs[0].InputFiles[0] == strex("cfg").combine_path("fileA").str());
        CHECK(packs[0].InputFiles[1] == strex("cfg").combine_path("fileB").str());
        CHECK(packs[0].RecursiveInput);
        REQUIRE(packs[0].Bakers.size() == 2);
        CHECK(packs[0].Bakers[0] == "BakerA");
        CHECK(packs[0].Bakers[1] == "BakerB");
        CHECK_FALSE(packs[0].ServerOnly);
        CHECK(packs[1].ServerOnly);
        CHECK_FALSE(packs[1].ClientOnly);
    }

    SECTION("ApplyCommandLineSetsCustomValuesAndImplicitFlags")
    {
        GlobalSettings settings {false};
        char arg0[] = "lf_tests";
        char arg1[] = "--CustomCli";
        char arg2[] = "123";
        char arg3[] = "--FlagOnly";
        const vector<CommandLineArg> argv = {arg0, arg1, arg2, arg3};

        settings.ApplyCommandLine(CommandLineArgs {argv});

        CHECK(settings.GetCustomSetting("CustomCli") == "123");
        CHECK(settings.GetCustomSetting("FlagOnly") == "1");
    }

    SECTION("CommandLineArgsAcceptEmptyNativeArgv")
    {
        const CommandLineArgs args {0, nullptr};

        CHECK(args.empty());
    }

    SECTION("ApplyCommandLineMasksSecretValuesInLog")
    {
        // Capture the "Set <name> to <value>" lines emitted by the logging pass.
        string captured;
        SetLogCallback("settings_secret_redaction_test", [&captured](LogType, string_view message, nptr<const CatchedStackTraceData>) { captured += message; });
        const auto remove_callback = scope_exit([]() noexcept { SetLogCallback("settings_secret_redaction_test", nullptr); });

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
        CHECK(captured.find("Set Probe.AccessToken to ***") != string::npos);
        CHECK(captured.find("super-secret-value") == string::npos);

        // A non-secret name is logged verbatim, and both values are still applied.
        CHECK(captured.find("Set Common.GameName to RedactionProbe") != string::npos);
        CHECK(settings.GetCustomSetting("Probe.AccessToken") == "super-secret-value");
        CHECK(settings.GameName == "RedactionProbe");
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
        CHECK(settings.GameName == "Tag");

        // A second pass over the same object appends again — what the two-pass flow used to do.
        settings.ApplyCommandLine(CommandLineArgs {3, argv});
        CHECK(settings.GameName == "Tag Tag");
    }

    SECTION("ApplyConfigAtPathResolvesFileVariables")
    {
        const string temp_dir = MakeTempSettingsDir("settings_config");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        REQUIRE(fs_write_file(strex(temp_dir).combine_path("payload.txt").str(), string_view {"  loaded value  "}));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("main.fomain").str(), string_view {"ExternalValue = $FILE{payload.txt}\n"}));

        GlobalSettings settings {false};
        settings.ApplyConfigAtPath("main.fomain", temp_dir);

        CHECK(settings.GetAppliedConfigs().size() == 1);
        CHECK(settings.GetCustomSetting("ExternalValue") == "loaded value");

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("ApplyConfigAtPathThrowsForMissingConfig")
    {
        GlobalSettings settings {false};

        CHECK_THROWS_AS(settings.ApplyConfigAtPath("missing.fomain", "/tmp/not_there"), SettingsException);
    }

    SECTION("FindCustomSettingReturnsNullableLookup")
    {
        GlobalSettings settings {false};

        const auto missing = settings.FindCustomSetting("Missing");
        CHECK_FALSE(static_cast<bool>(missing));
        CHECK(settings.GetCustomSetting("Missing").empty());

        settings.SetCustomSetting("Present", any_t(string("value")));
        const auto present = settings.FindCustomSetting("Present");

        REQUIRE(static_cast<bool>(present));
        CHECK(*present == "value");
        CHECK(settings.GetCustomSetting("Present") == "value");
    }

    SECTION("BakingModeSaveReturnsAppliedSettings")
    {
        GlobalSettings settings {true};
        ConfigFile config {"Bake.fomain", "CustomSaved = value\n"};

        settings.ApplyConfigFile(config, "");

        const auto saved = settings.Save();
        const auto it = saved.find("CustomSaved");
        REQUIRE(it != saved.end());
        CHECK(it->second == "value");
    }

    SECTION("UpdateFilesInMemoryCanBeOverriddenBySubConfigs")
    {
        GlobalSettings settings {false};
        ConfigFile config {"Test.fomain",
            "ServerNetwork.UpdateFilesInMemory = False\n"
            "[SubConfig]\n"
            "Name = PublicGame\n"
            "ServerNetwork.UpdateFilesInMemory = True\n"
            "[SubConfig]\n"
            "Name = Staging\n"
            "Parent = PublicGame\n"
            "ServerNetwork.UpdateFilesInMemory = False\n"};

        settings.ApplyConfigFile(config, "cfg");
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
        const auto root = MakeTempSettingsDir("settings_writable_root");
        ignore_unused(fs_remove_dir_tree(root));

        settings.UserWritablePath = root;
        ResolveUserWritablePath(settings);

        CHECK(settings.UserWritablePath == fs_resolve_path(root));
        CHECK(fs_is_dir(settings.UserWritablePath));
        CHECK(fs_is_dir(fs_make_writable_path(settings.UserWritablePath, settings.CacheResources)));
        CHECK(fs_is_dir(fs_make_writable_path(settings.UserWritablePath, settings.ClientResources)));

        ignore_unused(fs_remove_dir_tree(root));
    }

    SECTION("ResolveUserWritablePathPortableStaysEmpty")
    {
        GlobalSettings settings {false};

        // No explicit path and no installer marker next to the test exe: stay portable (empty).
        settings.UserWritablePath = "";
        ResolveUserWritablePath(settings);

        CHECK(settings.UserWritablePath.empty());
    }

    SECTION("ResolveUserWritablePathFailsafeRevertsToPortable")
    {
        GlobalSettings settings {false};

        // A root whose parent is a regular file can't be created: the resolver must fail safe to portable
        // rather than brick startup.
        const auto temp_dir = MakeTempSettingsDir("settings_writable_blocker");
        ignore_unused(fs_remove_dir_tree(temp_dir));
        const auto blocker = strex(temp_dir).combine_path("blocker").str();
        REQUIRE(fs_write_file(blocker, string_view {"x"}));

        settings.UserWritablePath = strex(blocker).combine_path("sub").str();
        ResolveUserWritablePath(settings);

        CHECK(settings.UserWritablePath.empty());

        ignore_unused(fs_remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
