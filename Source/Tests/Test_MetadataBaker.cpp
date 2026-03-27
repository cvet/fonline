//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "Test_BakerHelpers.h"

#include "DataSerialization.h"

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

    SECTION("resolves optional setting groups")
    {
        rig.AddSourceFile("Scripts/TestSettings.fos", R"(
namespace TestSettings
{
#if CLIENT
///@ Setting Client bool DebugBuild
///@ Setting Client bool Common.DebugBuild
///@ Setting Client bool DebugFlag
#endif
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        const auto& output = rig.Outputs.at("TestPack.fometa-client");
        DataReader reader(output);
        const auto tag_count = reader.Read<uint16>();

        vector<vector<string>> settings_entries;

        for (uint16 i = 0; i < tag_count; i++) {
            const auto tag_name_len = reader.Read<uint16>();
            const auto* tag_name_ptr = reader.ReadPtr<char>(tag_name_len);
            const string tag_name {tag_name_ptr, tag_name_len};
            const auto tag_value_count = reader.Read<uint32>();

            for (uint32 j = 0; j < tag_value_count; j++) {
                const auto value_parts_count = reader.Read<uint32>();
                vector<string> value_parts;
                value_parts.reserve(value_parts_count);

                for (uint32 k = 0; k < value_parts_count; k++) {
                    const auto part_len = reader.Read<uint16>();
                    const auto* part_ptr = reader.ReadPtr<char>(part_len);
                    value_parts.emplace_back(part_ptr, part_len);
                }

                if (tag_name == "Setting") {
                    settings_entries.emplace_back(std::move(value_parts));
                }
            }
        }

        reader.VerifyEnd();

        CHECK(std::ranges::count(settings_entries, vector<string> {"Common.DebugBuild", "bool"}) == 2);
        CHECK(std::ranges::count(settings_entries, vector<string> {"DebugFlag", "bool"}) == 1);
    }
#endif
}

FO_END_NAMESPACE
