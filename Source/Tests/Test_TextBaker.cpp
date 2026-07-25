//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "Test_BakerHelpers.h"
#include "TextBaker.h"

FO_BEGIN_NAMESPACE

TEST_CASE("TextBaker")
{
    using namespace BakerTests;

    SECTION("RejectsEmptyBakeLanguages")
    {
        TestRig rig;
        OverrideSetting(rig.Settings.BakeLanguages, vector<string> {});

        CHECK_THROWS_AS(TextBaker(rig.MakeContext("CorePack")), TextBakerException);
    }

    SECTION("IgnoresNonTextSourceFiles")
    {
        TestRig rig;
        rig.AddSourceFile("Readme.txt", "not a text pack", 7);

        TextBaker baker(rig.MakeContext("CorePack"));
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        CHECK(rig.Outputs.empty());
    }

    SECTION("BakesTextPackToBinary")
    {
        TestRig rig;
        rig.AddSourceFile("Game.engl.fotxt", "# Test pack\n\n{1}{}{Hello wasteland}\n", 7);

        TextBaker baker(rig.MakeContext("CorePack"));
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("CorePack.Game.engl.fotxt-bin"));
        CHECK_FALSE(rig.Outputs.at("CorePack.Game.engl.fotxt-bin").empty());
    }

    SECTION("SetupBakersReturnsRequestedBaker")
    {
        TestRig rig;
        auto bakers = MakeRequestedBakers({string(TextBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == TextBaker::NAME);
        CHECK(bakers.front()->GetOrder() == 4);
    }

    SECTION("SkipsUnsupportedTargetPath")
    {
        TestRig rig;
        rig.AddSourceFile("Game.engl.fotxt", "# Test pack\n\n{1}{}{Hello wasteland}\n", 7);

        TextBaker baker(rig.MakeContext("CorePack"));
        baker.BakeFiles(rig.GetAllSourceFiles(), "skip.bin");

        CHECK(rig.Outputs.empty());
    }

    SECTION("ThrowsWhenDefaultLanguageMissing")
    {
        TestRig rig;
        rig.AddSourceFile("Game.russ.fotxt", "# Test pack\n\n{1}{}{Privet}\n", 7);

        TextBaker baker(rig.MakeContext("CorePack"));

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), TextBakerException);
    }

    SECTION("BakeCheckerCanSkipTextBake")
    {
        TestRig rig;
        rig.AddSourceFile("Game.engl.fotxt", "# Test pack\n\n{1}{}{Hello wasteland}\n", 7);

        TextBaker baker(rig.MakeContext("CorePack", [](string_view, uint64_t) { return false; }));
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        CHECK(rig.Outputs.empty());
    }

    SECTION("BakeCheckerCompletesChangedPackLanguages")
    {
        TestRig rig;
        OverrideSetting(rig.Settings.BakeLanguages, vector<string> {"engl", "russ"});
        rig.AddSourceFile("Readme.txt", "not a text pack", 7);
        rig.AddSourceFile("Game.engl.fotxt", "# Test pack\n\n{1}{}{Hello wasteland}\n", 7);
        rig.AddSourceFile("Game.russ.fotxt", "# Test pack\n\n{1}{}{Privet}\n", 9);

        vector<pair<string, uint64_t>> checks;
        TextBaker baker(rig.MakeContext("CorePack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return path == "CorePack.Game.engl.fotxt-bin";
        }));
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(checks.size() == 2);
        CHECK(checks[0] == pair<string, uint64_t> {"CorePack.Game.engl.fotxt-bin", 7});
        CHECK(checks[1] == pair<string, uint64_t> {"CorePack.Game.russ.fotxt-bin", 9});
        CHECK(rig.Outputs.contains("CorePack.Game.engl.fotxt-bin"));
        CHECK(rig.Outputs.contains("CorePack.Game.russ.fotxt-bin"));
        CHECK(rig.Outputs.size() == 2);
    }

    SECTION("RejectsInvalidTextPack")
    {
        TestRig rig;
        rig.AddSourceFile("Game.engl.fotxt", "{10}{}{Valid}\n{20}{Broken\n", 7);

        TextBaker baker(rig.MakeContext("CorePack"));
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), TextPackException);
    }
}

FO_END_NAMESPACE
