//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "Test_BakerHelpers.h"

#if FO_ANGELSCRIPT_SCRIPTING
#include "MetadataBaker.h"
#endif

FO_BEGIN_NAMESPACE

TEST_CASE("MetadataBaker")
{
#if FO_ANGELSCRIPT_SCRIPTING
    using namespace BakerTests;

    TestRig rig;
    const auto bakers = MakeRequestedBakers({string(MetadataBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == MetadataBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 1);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
#endif
}

FO_END_NAMESPACE
