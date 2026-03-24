//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "ProtoBaker.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

TEST_CASE("ProtoBaker")
{
    using namespace BakerTests;

    TestRig rig;
    const auto bakers = MakeRequestedBakers({string(ProtoBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == ProtoBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 5);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), ""));
}

FO_END_NAMESPACE
