//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "RawCopyBaker.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

TEST_CASE("RawCopyBaker")
{
    using namespace BakerTests;

    SECTION("CopiesMatchingExtensions")
    {
        TestRig rig;
        rig.AddSourceFile("Data/config.json", "{\"enabled\":true}\n", 10);
        rig.AddSourceFile("Data/readme.txt", "skip\n", 11);

        RawCopyBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("Data/config.json"));
        CHECK(rig.GetOutputText("Data/config.json") == "{\"enabled\":true}\n");
    }

    SECTION("SetupBakersReturnsRequestedBaker")
    {
        TestRig rig;
        auto bakers = MakeRequestedBakers({string(RawCopyBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == RawCopyBaker::NAME);
        CHECK(bakers.front()->GetOrder() == 4);
    }

    SECTION("CopiesOnlyExplicitTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Data/config.json", "{\"enabled\":true}\n", 10);
        rig.AddSourceFile("Data/other.json", "{\"enabled\":false}\n", 11);

        RawCopyBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Data/other.json");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("Data/other.json"));
        CHECK_FALSE(rig.Outputs.contains("Data/config.json"));
    }

    SECTION("SkipsExplicitTargetWithUnsupportedExtension")
    {
        TestRig rig;
        rig.AddSourceFile("Data/config.json", "{\"enabled\":true}\n", 10);
        rig.AddSourceFile("Data/readme.txt", "skip\n", 11);

        RawCopyBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Data/readme.txt");

        CHECK(rig.Outputs.empty());
    }

    SECTION("SkipsMissingExplicitTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Data/config.json", "{\"enabled\":true}\n", 10);

        RawCopyBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Data/missing.json");

        CHECK(rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipCopy")
    {
        TestRig rig;
        rig.AddSourceFile("Data/config.json", "{\"enabled\":true}\n", 10);

        RawCopyBaker baker(rig.MakeContext("TestPack", [](string_view, uint64_t) { return false; }));
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        CHECK(rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipExplicitTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Data/config.json", "{\"enabled\":true}\n", 10);

        vector<pair<string, uint64_t>> checks;
        RawCopyBaker baker(rig.MakeContext("TestPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        }));
        baker.BakeFiles(rig.GetAllSourceFiles(), "Data/config.json");

        CHECK(rig.Outputs.empty());
        REQUIRE(checks.size() == 1);
        CHECK(checks.front() == pair<string, uint64_t> {"Data/config.json", 10});
    }
}

FO_END_NAMESPACE
