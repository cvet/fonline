//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "EffectBaker.h"
#include "ImageBaker.h"
#include "MapBaker.h"
#include "ProtoBaker.h"
#include "RawCopyBaker.h"
#include "Test_BakerHelpers.h"
#include "TextBaker.h"

FO_BEGIN_NAMESPACE

TEST_CASE("BakerSetup")
{
    using namespace BakerTests;

    SECTION("ReturnsBakersInCanonicalSetupOrder")
    {
        TestRig rig;
        const auto bakers = MakeRequestedBakers({string(TextBaker::NAME), string(MapBaker::NAME), string(RawCopyBaker::NAME), string(ProtoBaker::NAME), string(ImageBaker::NAME), string(EffectBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 6);
        CHECK(bakers[0]->GetName() == RawCopyBaker::NAME);
        CHECK(bakers[1]->GetName() == ImageBaker::NAME);
        CHECK(bakers[2]->GetName() == EffectBaker::NAME);
        CHECK(bakers[3]->GetName() == ProtoBaker::NAME);
        CHECK(bakers[4]->GetName() == MapBaker::NAME);
        CHECK(bakers[5]->GetName() == TextBaker::NAME);
    }

    SECTION("IgnoresUnknownBakerNames")
    {
        TestRig rig;
        const auto bakers = MakeRequestedBakers({"UnknownBaker", string(RawCopyBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == RawCopyBaker::NAME);
    }
}

FO_END_NAMESPACE
