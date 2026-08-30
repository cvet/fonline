//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

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
    auto bakers = MakeRequestedBakers({string(ProtoBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == ProtoBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 7);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), ""));

    auto add_server_metadata = [](TestRig& local_rig) { local_rig.AddBakedFile("Metadata.fometa-server", BakerTests::MakeEmptyMetadataBlob()); };
    auto add_client_mapper_metadata = [](TestRig& local_rig) {
        auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
        local_rig.AddBakedFile("Metadata.fometa-client", metadata_blob);
        local_rig.AddBakedFile("Metadata.fometa-mapper", metadata_blob);
    };
    auto add_client_mapper_metadata_blob = [](TestRig& local_rig, const vector<uint8_t>& metadata_blob) {
        local_rig.AddBakedFile("Metadata.fometa-client", metadata_blob);
        local_rig.AddBakedFile("Metadata.fometa-mapper", metadata_blob);
    };
    auto server_only_bake = [](string_view path, uint64_t) { return path.ends_with(".fopro-bin-server"); };
    auto client_mapper_bake = [](string_view path, uint64_t) { return path.ends_with(".fopro-bin-client") || path.ends_with(".fopro-bin-mapper"); };

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
        OverrideSetting(local_rig.Settings.ProtoFileExtensions, vector<string> {"fopro", "fomap"});
        local_rig.AddSourceFile("Maps/HeaderOnly.fomap", R"([ProtoMap]
)");
        add_client_mapper_metadata(local_rig);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackFomap", client_mapper_bake));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("ProtoPackFomap.fopro-bin-client"));
        CHECK(local_rig.Outputs.contains("ProtoPackFomap.fopro-bin-mapper"));
        CHECK(local_rig.Outputs.size() == 2);
    }

    SECTION("RejectsCollidingAnonymousMapAnchors")
    {
        TestRig local_rig;
        OverrideSetting(local_rig.Settings.ProtoFileExtensions, vector<string> {"fopro", "fomap"});
        local_rig.AddSourceFile("Maps/Collide.fomap", R"([ProtoMap]
Outside = True
[ProtoMap]
Outside = False
)");
        add_client_mapper_metadata(local_rig);

        // Both anchors resolve to the file stem and collide in proto registration
        ProtoBaker baker(local_rig.MakeContext("ProtoPackFomap", client_mapper_bake));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoBakerException);
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

    // Variant overrides Base and Other reaches Base again, the shape Baking.AllowRepeatedProtoParents
    // governs. Baking the same set with a single-parent Child makes blob equality pin which parent won
    auto make_diamond_source = [](string_view child_parents) {
        return string(R"([ProtoItem]
$Name = DiamondBase
Count = 1

[ProtoItem]
$Name = DiamondVariant
$Parent = DiamondBase
Count = 2

[ProtoItem]
$Name = DiamondOther
$Parent = DiamondBase

[ProtoItem]
$Name = DiamondChild
$Parent = )")
            .append(child_parents);
    };

    auto bake_diamond = [&](string_view child_parents, bool allow_repeated) {
        TestRig local_rig;
        OverrideSetting(local_rig.Settings.AllowRepeatedProtoParents, allow_repeated);
        local_rig.AddSourceFile("Items/Diamond.fopro", make_diamond_source(child_parents));

        auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
        local_rig.AddBakedFile("Metadata.fometa-client", metadata_blob);
        local_rig.AddBakedFile("Metadata.fometa-mapper", metadata_blob);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackDiamond", client_mapper_bake));
        baker.BakeFiles(local_rig.GetAllSourceFiles(), "");
        return local_rig.Outputs.at("ProtoPackDiamond.fopro-bin-client");
    };

    SECTION("RepeatedProtoParentDefaultIsPermissive")
    {
        TestRig local_rig;
        CHECK(local_rig.Settings.AllowRepeatedProtoParents);
    }

    SECTION("RejectsRepeatedProtoParentWhenNotAllowed")
    {
        CHECK_THROWS_AS(bake_diamond("DiamondVariant DiamondOther", false), ProtoBakerException);
    }

    SECTION("AcceptsUnrelatedParentsAndPlainChainsWhenRepetitionIsForbidden")
    {
        // The rule must fire on a repeated ancestor, not on inheritance depth
        CHECK_NOTHROW(bake_diamond("DiamondVariant", false));
        CHECK_NOTHROW(bake_diamond("DiamondOther", false));
    }

    SECTION("AppliesRepeatedProtoParentOnce")
    {
        auto diamond = bake_diamond("DiamondVariant DiamondOther", true);

        // First reach keeps its position, so Variant's override of Base survives Other's walk
        CHECK(diamond == bake_diamond("DiamondVariant", true));
        CHECK(diamond != bake_diamond("DiamondOther", true));
    }

    SECTION("RejectsProtoParentCycleUnderEverySetting")
    {
        auto bake_cycle = [&](string_view content, bool allow_repeated) {
            TestRig local_rig;
            OverrideSetting(local_rig.Settings.AllowRepeatedProtoParents, allow_repeated);
            local_rig.AddSourceFile("Items/Cycle.fopro", content);

            auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
            local_rig.AddBakedFile("Metadata.fometa-client", metadata_blob);
            local_rig.AddBakedFile("Metadata.fometa-mapper", metadata_blob);

            ProtoBaker baker(local_rig.MakeContext("ProtoPackCycle", client_mapper_bake));
            baker.BakeFiles(local_rig.GetAllSourceFiles(), "");
        };

        string_view self_cycle = R"([ProtoItem]
$Name = SelfCycle
$Parent = SelfCycle
)";
        string_view pair_cycle = R"([ProtoItem]
$Name = CycleA
$Parent = CycleB

[ProtoItem]
$Name = CycleB
$Parent = CycleA
)";
        string_view long_cycle = R"([ProtoItem]
$Name = LongA
$Parent = LongC

[ProtoItem]
$Name = LongB
$Parent = LongA

[ProtoItem]
$Name = LongC
$Parent = LongB
)";

        // A cycle is never valid input, so the setting may not make it bakeable either way
        for (bool allow_repeated : {true, false}) {
            CHECK_THROWS_AS(bake_cycle(self_cycle, allow_repeated), ProtoBakerException);
            CHECK_THROWS_AS(bake_cycle(pair_cycle, allow_repeated), ProtoBakerException);
            CHECK_THROWS_AS(bake_cycle(long_cycle, allow_repeated), ProtoBakerException);
        }
    }

    // Baked bytes must follow the prototype set alone. Iterating an unordered map would chain
    // same-bucket entries in insertion order, so moving a proto between files rewrote output that did not change
    auto bake_arrangement = [&](const vector<pair<string_view, string_view>>& files) {
        TestRig local_rig;

        for (const auto& [path, content] : files) {
            local_rig.AddSourceFile(path, content);
        }

        add_client_mapper_metadata(local_rig);

        ProtoBaker baker(local_rig.MakeContext("ProtoPackOrder", client_mapper_bake));
        baker.BakeFiles(local_rig.GetAllSourceFiles(), "");
        return local_rig.Outputs.at("ProtoPackOrder.fopro-bin-client");
    };

    SECTION("BakesIdenticalBytesWhateverFileCarriesEachProto")
    {
        constexpr string_view items_abc = R"([ProtoItem]
$Name = OrderItemA
Count = 1

[ProtoItem]
$Name = OrderItemB
Count = 2

[ProtoItem]
$Name = OrderItemC
Count = 3
)";
        constexpr string_view items_de_critter_p = R"([ProtoItem]
$Name = OrderItemD
Count = 4

[ProtoItem]
$Name = OrderItemE
Count = 5

[ProtoCritter]
$Name = OrderCritterP
)";
        constexpr string_view items_fgh_critter_q = R"([ProtoItem]
$Name = OrderItemF
Count = 6

[ProtoItem]
$Name = OrderItemG
Count = 7

[ProtoItem]
$Name = OrderItemH
Count = 8

[ProtoCritter]
$Name = OrderCritterQ
)";

        // The same protos, redistributed and renamed so neither the file order nor the order within a
        // file survives, while the resolved set stays identical
        constexpr string_view mixed_hg = R"([ProtoItem]
$Name = OrderItemH
Count = 8

[ProtoItem]
$Name = OrderItemG
Count = 7
)";
        constexpr string_view mixed_fed_critter_q = R"([ProtoCritter]
$Name = OrderCritterQ

[ProtoItem]
$Name = OrderItemF
Count = 6

[ProtoItem]
$Name = OrderItemE
Count = 5

[ProtoItem]
$Name = OrderItemD
Count = 4
)";
        constexpr string_view mixed_cba_critter_p = R"([ProtoItem]
$Name = OrderItemC
Count = 3

[ProtoCritter]
$Name = OrderCritterP

[ProtoItem]
$Name = OrderItemB
Count = 2

[ProtoItem]
$Name = OrderItemA
Count = 1
)";

        auto grouped = bake_arrangement({{"Items/Alpha.fopro", items_abc}, {"Items/Beta.fopro", items_de_critter_p}, {"Items/Gamma.fopro", items_fgh_critter_q}});
        auto shuffled = bake_arrangement({{"Protos/Zulu.fopro", mixed_hg}, {"Protos/Yankee.fopro", mixed_fed_critter_q}, {"Protos/Xray.fopro", mixed_cba_critter_p}});

        CHECK_FALSE(grouped.empty());
        CHECK(grouped == shuffled);

        // Guard against the comparison passing because the payload never reached the blob
        auto changed = bake_arrangement({{"Items/Alpha.fopro", items_abc}, {"Items/Beta.fopro", items_de_critter_p}, {"Items/Gamma.fopro", R"([ProtoItem]
$Name = OrderItemF
Count = 6

[ProtoItem]
$Name = OrderItemG
Count = 7

[ProtoItem]
$Name = OrderItemH
Count = 9

[ProtoCritter]
$Name = OrderCritterQ
)"}});

        CHECK(grouped != changed);
    }

    SECTION("BakesDynamicCustomEntityProtos")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Custom/Gizmo.fopro", R"([ProtoGizmo]
$Name = CustomGizmo
)");
        add_client_mapper_metadata_blob(local_rig, BakerTests::MakeMetadataBlob({{"Entity", {{"Gizmo", "HasProtos"}}}}));

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
        add_client_mapper_metadata_blob(local_rig, BakerTests::MakeMetadataBlob({{"FixedType", {{"Blueprint"}}}}));

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
        add_client_mapper_metadata_blob(local_rig, BakerTests::MakeMetadataBlob({{"Entity", {{"AuditLog"}}}}));

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
    auto make_script_blob = [](string_view script_source) {
        auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ProtoBakerCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine compiler_engine {compiler_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ProtoBakerScripts", {{"Scripts/TestItemCallbacks.fos", string(script_source)}}, [](string_view message) {
            string message_str = string(message);

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
bool OnStatic(Critter cr, StaticItem staticItem, Item? usedItem, any param)
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
bool OnStatic(Critter cr, StaticItem staticItem, Item? usedItem, any param)
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
void OnStatic(Critter cr, StaticItem staticItem, Item? usedItem, any param)
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
