#include "catch_amalgamated.hpp"

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
            "ServerOnly = true\n",
            nullptr};

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
        char* argv[] = {arg0, arg1, arg2, arg3};

        settings.ApplyCommandLine(4, argv);

        CHECK(settings.GetCustomSetting("CustomCli") == "123");
        CHECK(settings.GetCustomSetting("FlagOnly") == "1");
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

    SECTION("BakingModeSaveReturnsAppliedSettings")
    {
        GlobalSettings settings {true};
        ConfigFile config {"Bake.fomain", "CustomSaved = value\n", nullptr};

        settings.ApplyConfigFile(config, "");

        const auto saved = settings.Save();
        const auto it = saved.find("CustomSaved");
        REQUIRE(it != saved.end());
        CHECK(it->second == "value");
    }
}

FO_END_NAMESPACE
