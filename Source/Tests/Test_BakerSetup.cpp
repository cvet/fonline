//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "AngelScriptBaker.h"
#include "ConfigBaker.h"
#include "EffectBaker.h"
#include "ImageBaker.h"
#include "MapBaker.h"
#include "MetadataBaker.h"
#include "ModelInfoBaker.h"
#include "ModelMeshBaker.h"
#include "ProtoBaker.h"
#include "ProtoTextBaker.h"
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
        vector<string> requested_bakers {
            string(TextBaker::NAME),
            string(MapBaker::NAME),
            string(RawCopyBaker::NAME),
            string(ProtoBaker::NAME),
            string(ImageBaker::NAME),
            string(EffectBaker::NAME),
            string(ProtoTextBaker::NAME),
        };
        vector<string> expected_names {};

#if FO_ANGELSCRIPT_SCRIPTING
        requested_bakers.emplace_back(string(AngelScriptBaker::NAME));
        requested_bakers.emplace_back(string(ConfigBaker::NAME));
        requested_bakers.emplace_back(string(MetadataBaker::NAME));

        expected_names.emplace_back(string(MetadataBaker::NAME));
        expected_names.emplace_back(string(ConfigBaker::NAME));
#endif

        expected_names.emplace_back(string(RawCopyBaker::NAME));
        expected_names.emplace_back(string(ImageBaker::NAME));
        expected_names.emplace_back(string(EffectBaker::NAME));
        expected_names.emplace_back(string(ProtoBaker::NAME));
        expected_names.emplace_back(string(MapBaker::NAME));
        expected_names.emplace_back(string(TextBaker::NAME));
        expected_names.emplace_back(string(ProtoTextBaker::NAME));

#if FO_ENABLE_3D
        requested_bakers.emplace_back(string(ModelInfoBaker::NAME));
        requested_bakers.emplace_back(string(ModelMeshBaker::NAME));

        expected_names.emplace_back(string(ModelMeshBaker::NAME));
        expected_names.emplace_back(string(ModelInfoBaker::NAME));
#endif

#if FO_ANGELSCRIPT_SCRIPTING
        expected_names.emplace_back(string(AngelScriptBaker::NAME));
#endif

        const auto bakers = MakeRequestedBakers(requested_bakers, rig);

        REQUIRE(bakers.size() == expected_names.size());

        for (size_t i = 0; i < expected_names.size(); i++) {
            CHECK(bakers[i]->GetName() == expected_names[i]);
        }
    }

    SECTION("ReturnsNoBakersWhenRequestIsEmpty")
    {
        TestRig rig;
        const auto bakers = MakeRequestedBakers({}, rig);

        CHECK(bakers.empty());
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
