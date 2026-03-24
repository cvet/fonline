//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#if FO_ANGELSCRIPT_SCRIPTING
#include "AngelScriptBaker.h"
#include "Test_BakerHelpers.h"
#endif

FO_BEGIN_NAMESPACE

TEST_CASE("AngelScriptBaker")
{
#if FO_ANGELSCRIPT_SCRIPTING
    using namespace BakerTests;

    TestRig rig;
    const auto bakers = MakeRequestedBakers({string(AngelScriptBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == AngelScriptBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 4);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
#endif
}

FO_END_NAMESPACE
