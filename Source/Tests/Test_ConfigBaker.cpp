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
#include "MetadataRegistration.h"
#endif

FO_BEGIN_NAMESPACE

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

    SECTION("BakingRequiresBakedMetadata")
    {
        const auto temp_dir = std::filesystem::temp_directory_path() / "lf-configbaker-test";
        std::filesystem::create_directories(temp_dir);
        const auto fomain_path = temp_dir / "Test.fomain";
        (void)fs_write_file(fs_path_to_string(fomain_path), "GameName = Test\n");

        TestRig rig;
        rig.Settings.ApplyConfigAtPath("Test.fomain", fs_path_to_string(temp_dir));

        ConfigBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(TestRig::MakeEmptyFiles(), ""), MetadataNotFoundException);

        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    SECTION("SetupBakersReturnsRequestedBaker")
    {
        TestRig rig;
        const auto bakers = MakeRequestedBakers({string(ConfigBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == ConfigBaker::NAME);
        CHECK(bakers.front()->GetOrder() == 2);
    }
#endif
}

FO_END_NAMESPACE
