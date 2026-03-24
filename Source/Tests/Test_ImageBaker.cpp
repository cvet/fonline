//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "ImageBaker.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

TEST_CASE("ImageBaker")
{
    using namespace BakerTests;

    TestRig rig;
    const auto bakers = MakeRequestedBakers({string(ImageBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == ImageBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 4);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
}

FO_END_NAMESPACE
