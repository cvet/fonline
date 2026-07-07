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
    CHECK(bakers.front()->GetOrder() == 6);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), ""));

    const auto add_server_metadata = [](TestRig& local_rig) { local_rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeEmptyMetadataBlob()); };
    const auto add_client_mapper_metadata = [](TestRig& local_rig) {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
        local_rig.AddBakedFile("Metadata.fometa-client", metadata_blob);
        local_rig.AddBakedFile("Metadata.fometa-mapper", metadata_blob);
    };
    const auto make_dynamic_metadata_blob = [](const vector<pair<string_view, vector<vector<string_view>>>>& sections) {
        vector<uint8_t> metadata;
        auto writer = DataWriter(metadata);

        writer.Write<uint16_t>(numeric_cast<uint16_t>(sections.size()));

        for (const auto& [section_name, entries] : sections) {
            writer.Write<uint16_t>(numeric_cast<uint16_t>(section_name.length()));
            writer.WriteStringBytes(section_name);
            writer.Write<uint32_t>(numeric_cast<uint32_t>(entries.size()));

            for (const auto& tokens : entries) {
                writer.Write<uint32_t>(numeric_cast<uint32_t>(tokens.size()));

                for (const string_view token : tokens) {
                    writer.Write<uint16_t>(numeric_cast<uint16_t>(token.length()));
                    writer.WriteStringBytes(token);
                }
            }
        }

        return metadata;
    };
    const auto add_client_mapper_metadata_blob = [](TestRig& local_rig, const vector<uint8_t>& metadata_blob) {
        local_rig.AddBakedFile("Metadata.fometa-client", metadata_blob);
        local_rig.AddBakedFile("Metadata.fometa-mapper", metadata_blob);
    };
    const auto server_only_bake = [](string_view path, uint64_t) { return path.ends_with(".fopro-bin-server"); };
    const auto client_mapper_bake = [](string_view path, uint64_t) { return path.ends_with(".fopro-bin-client") || path.ends_with(".fopro-bin-mapper"); };

    SECTION("IgnoresNonProtoSourceFiles")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Data/Readme.txt", "not a proto");

        ProtoBaker baker(local_rig.MakeContext("ProtoPackIgnored"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.empty());
    }

    SECTION("BakesClientAndMapperProtoTargets")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/PlainItem.fopro", R"([ProtoItem]
$Name = PlainItem
)");
        add_client_mapper_metadata(local_rig);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackAll", client_mapper_bake));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("ProtoPackAll.fopro-bin-client"));
        CHECK(local_rig.Outputs.contains("ProtoPackAll.fopro-bin-mapper"));
        CHECK(local_rig.Outputs.size() == 2);
    }

    SECTION("BakesFomapHeaderWithDefaultProtoName")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Maps/HeaderOnly.fomap", R"([Header]
)");
        add_client_mapper_metadata(local_rig);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackFomap", client_mapper_bake));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("ProtoPackFomap.fopro-bin-client"));
        CHECK(local_rig.Outputs.contains("ProtoPackFomap.fopro-bin-mapper"));
        CHECK(local_rig.Outputs.size() == 2);
    }

    SECTION("BakesInheritedProtoParents")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/Inherit.fopro", R"([ProtoItem]
$Name = ParentItem
Hidden = True

[ProtoItem]
$Name = ChildItem
$Parent = ParentItem
)");
        add_client_mapper_metadata(local_rig);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackInherit", client_mapper_bake));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("ProtoPackInherit.fopro-bin-client"));
        CHECK(local_rig.Outputs.contains("ProtoPackInherit.fopro-bin-mapper"));
        CHECK(local_rig.Outputs.size() == 2);
    }

    SECTION("BakesDynamicCustomEntityProtos")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Custom/Gizmo.fopro", R"([ProtoGizmo]
$Name = CustomGizmo
)");
        add_client_mapper_metadata_blob(local_rig, make_dynamic_metadata_blob({{"Entity", {{"Gizmo", "HasProtos"}}}}));

        ProtoBaker baker(local_rig.MakeContext("ProtoPackCustom", client_mapper_bake));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("ProtoPackCustom.fopro-bin-client"));
        CHECK(local_rig.Outputs.contains("ProtoPackCustom.fopro-bin-mapper"));
        CHECK(local_rig.Outputs.size() == 2);
    }

    SECTION("BakesDynamicFixedTypeProtos")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Protos/Blueprint.fopro", R"([Blueprint]
$Name = VaultDoorBlueprint
)");
        add_client_mapper_metadata_blob(local_rig, make_dynamic_metadata_blob({{"FixedType", {{"Blueprint"}}}}));

        ProtoBaker baker(local_rig.MakeContext("ProtoPackFixed", client_mapper_bake));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("ProtoPackFixed.fopro-bin-client"));
        CHECK(local_rig.Outputs.contains("ProtoPackFixed.fopro-bin-mapper"));
        CHECK(local_rig.Outputs.size() == 2);
    }

    SECTION("RejectsInvalidProtoSectionNames")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/Broken.fopro", R"([BrokenSection]
$Name = Broken
)");
        add_client_mapper_metadata(local_rig);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackBroken", client_mapper_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
    }

    SECTION("RejectsEntityTypeWithoutProtos")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Custom/AuditLog.fopro", R"([ProtoAuditLog]
$Name = AuditLogEntry
)");
        add_client_mapper_metadata_blob(local_rig, make_dynamic_metadata_blob({{"Entity", {{"AuditLog"}}}}));

        ProtoBaker baker(local_rig.MakeContext("ProtoPackNoProtos", client_mapper_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
    }

    SECTION("RejectsUnknownProtoTypeNames")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Custom/Unknown.fopro", R"([ProtoUnknownEntity]
$Name = UnknownProto
)");
        add_client_mapper_metadata(local_rig);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackUnknownType", client_mapper_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
    }

    SECTION("RejectsDuplicateProtoIdsAcrossFiles")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/DuplicateOne.fopro", R"([ProtoItem]
$Name = DuplicateItem
)");
        local_rig.AddSourceFile("Items/DuplicateTwo.fopro", R"([ProtoItem]
$Name = DuplicateItem
)");
        add_client_mapper_metadata(local_rig);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackDuplicate", client_mapper_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
    }

    SECTION("RejectsMissingDirectProtoParent")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/MissingParent.fopro", R"([ProtoItem]
$Name = OrphanItem
$Parent = MissingParentItem
)");
        add_client_mapper_metadata(local_rig);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackMissingParent", client_mapper_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
    }

    SECTION("RejectsProtoParentFromAnotherType")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Protos/CrossTypeParent.fopro", R"([ProtoItem]
$Name = SharedBase

[ProtoCritter]
$Name = WrongChild
$Parent = SharedBase
)");
        add_client_mapper_metadata(local_rig);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackCrossParent", client_mapper_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
    }

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

    static constexpr string_view InitProtoFile = R"([ProtoItem]
$Name = InitItem
InitScript = TestItemCallbacks::OnItemInit

[ProtoCritter]
$Name = InitCritter
InitScript = TestItemCallbacks::OnCritterInit

[ProtoMap]
$Name = InitMap
InitScript = TestItemCallbacks::OnMapInit

[ProtoLocation]
$Name = InitLocation
InitScript = TestItemCallbacks::OnLocationInit
)";

    static constexpr string_view ValidScript = R"(namespace TestItemCallbacks
{
[[ItemStatic]]
bool OnStatic(Critter cr, StaticItem staticItem, Item usedItem, any param)
{
    return true;
}

[[ItemTrigger]]
void OnTrigger(Critter cr, StaticItem trigger, bool entered, mdir dir)
{
}
})";

    static constexpr string_view ValidInitScript = R"(namespace TestItemCallbacks
{
void OnItemInit(Item item, bool firstTime)
{
}

void OnCritterInit(Critter cr, bool firstTime)
{
}

void OnMapInit(Map map, bool firstTime)
{
}

void OnLocationInit(Location loc, bool firstTime)
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
void OnTrigger(Critter cr, StaticItem trigger, bool entered, mdir dir)
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
bool OnTrigger(Critter cr, StaticItem trigger, bool entered, mdir dir)
{
    return true;
}
})";

    SECTION("BakesItemScriptPropertiesWhenCallbacksHaveItemAttributes")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/TestItem.fopro", ProtoFile);

        add_server_metadata(local_rig);
        local_rig.AddBakedFile("TestItemCallbacks.fos-bin-server", make_script_blob(ValidScript));

        ProtoBaker baker(local_rig.MakeContext("ProtoPackTest", server_only_bake));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("ProtoPackTest.fopro-bin-server"));
    }

    SECTION("BakesInitScriptPropertiesForServerProtoTypes")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Protos/InitScripts.fopro", InitProtoFile);

        add_server_metadata(local_rig);
        local_rig.AddBakedFile("TestItemCallbacks.fos-bin-server", make_script_blob(ValidInitScript));

        ProtoBaker baker(local_rig.MakeContext("ProtoPackInit", server_only_bake));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("ProtoPackInit.fopro-bin-server"));
    }

    SECTION("RejectsLegacyTriggerAndStaticItemAttributeNames")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/TestItem.fopro", ProtoFile);

        add_server_metadata(local_rig);
        local_rig.AddBakedFile("TestItemCallbacks.fos-bin-server", make_script_blob(LegacyAttributeScript));

        ProtoBaker baker(local_rig.MakeContext("ProtoPackTest", server_only_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
    }

    SECTION("RejectsWrongItemScriptCallbackSignaturesEvenWhenAttributed")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Items/TestItem.fopro", ProtoFile);

        add_server_metadata(local_rig);
        local_rig.AddBakedFile("TestItemCallbacks.fos-bin-server", make_script_blob(WrongSignatureScript));

        ProtoBaker baker(local_rig.MakeContext("ProtoPackTest", server_only_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
    }
#endif
}

FO_END_NAMESPACE
