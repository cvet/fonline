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
        const auto bakers = MakeRequestedBakers({string(TextBaker::NAME)}, rig);

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

        TextBaker baker(rig.MakeContext("CorePack", [](string_view, uint64) { return false; }));
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        CHECK(rig.Outputs.empty());
    }
}

FO_END_NAMESPACE
