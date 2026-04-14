//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#if FO_ANGELSCRIPT_SCRIPTING
#include "ScriptSystem.h"
#endif
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

#if FO_ANGELSCRIPT_SCRIPTING
    const auto make_script_blob = [](string_view script_source) {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ProtoBakerCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine compiler_engine {compiler_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ProtoBakerScripts", {{"Scripts/TestItemCallbacks.fos", string(script_source)}}, [](string_view message) {
            const auto message_str = string(message);

            if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                throw ScriptSystemException(message_str);
            }
        });
    };

    static constexpr string_view ProtoFile = R"([ProtoItem]
$Name = TestItem
StaticScript = TestItemCallbacks::OnStatic
TriggerScript = TestItemCallbacks::OnTrigger
)";

    static constexpr string_view ValidScript = R"(namespace TestItemCallbacks
{
[[ItemStatic]]
bool OnStatic(Critter cr, StaticItem staticItem, Item usedItem, any param)
{
    return true;
}

[[ItemTrigger]]
void OnTrigger(Critter cr, StaticItem trigger, bool entered, uint8 dir)
{
}
})";

    static constexpr string_view LegacyAttributeScript = R"(namespace TestItemCallbacks
{
[[StaticItemCallback]]
bool OnStatic(Critter cr, StaticItem staticItem, Item usedItem, any param)
{
    return true;
}

[[Trigger]]
void OnTrigger(Critter cr, StaticItem trigger, bool entered, uint8 dir)
{
}
})";

    static constexpr string_view WrongSignatureScript = R"(namespace TestItemCallbacks
{
[[ItemStatic]]
void OnStatic(Critter cr, StaticItem staticItem, Item usedItem, any param)
{
}

[[ItemTrigger]]
bool OnTrigger(Critter cr, StaticItem trigger, bool entered, uint8 dir)
{
    return true;
}
})";

    const auto server_only_bake = [](string_view path, uint64) { return path.ends_with(".fopro-bin-server"); };

    SECTION("BakesItemScriptPropertiesWhenCallbacksHaveItemAttributes")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/TestItem.fopro", ProtoFile);

        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
        local_rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
        local_rig.AddBakedFile("TestItemCallbacks.fos-bin-server", make_script_blob(ValidScript));

        ProtoBaker baker(local_rig.MakeContext("ProtoPackTest", server_only_bake));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("ProtoPackTest.fopro-bin-server"));
    }

    SECTION("RejectsLegacyTriggerAndStaticItemAttributeNames")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/TestItem.fopro", ProtoFile);

        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
        local_rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
        local_rig.AddBakedFile("TestItemCallbacks.fos-bin-server", make_script_blob(LegacyAttributeScript));

        ProtoBaker baker(local_rig.MakeContext("ProtoPackTest", server_only_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
    }

    SECTION("RejectsWrongItemScriptCallbackSignaturesEvenWhenAttributed")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/TestItem.fopro", ProtoFile);

        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
        local_rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
        local_rig.AddBakedFile("TestItemCallbacks.fos-bin-server", make_script_blob(WrongSignatureScript));

        ProtoBaker baker(local_rig.MakeContext("ProtoPackTest", server_only_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
    }
#endif
}

FO_END_NAMESPACE
