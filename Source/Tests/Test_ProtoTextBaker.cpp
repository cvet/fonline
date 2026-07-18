//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "ProtoTextBaker.h"
#include "Test_BakerHelpers.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE

static auto MakeDynamicMetadataBlob(const vector<pair<string_view, vector<vector<string_view>>>>& sections) -> vector<uint8_t>
{
    vector<uint8_t> metadata;
    auto writer = DataWriter(metadata);

    writer.Write<uint16_t>(numeric_cast<uint16_t>(sections.size()));

    for (const auto& [section_name, entries] : sections) {
        writer.Write<uint16_t>(numeric_cast<uint16_t>(section_name.length()));
        writer.WriteStringBytes(section_name);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(entries.size()));

        for (const auto& tokens : entries) {
            writer.Write<uint32_t>(numeric_cast<uint32_t>(tokens.size()));

            for (string_view token : tokens) {
                writer.Write<uint16_t>(numeric_cast<uint16_t>(token.length()));
                writer.WriteStringBytes(token);
            }
        }
    }

    return metadata;
}

static auto LoadOutputTextPack(const BakerTests::TestRig& rig, string_view path, HashStorage& hashes) -> TextPack
{
    auto it = rig.Outputs.find(string(path));
    REQUIRE(it != rig.Outputs.end());

    TextPack pack(&hashes);
    REQUIRE(pack.LoadFromBinaryData(it->second));
    return pack;
}

static auto MakeTextKey(HashStorage& hashes, string_view pack_name, string_view proto_name, string_view key2 = {}, string_view key3 = {}) -> TextPackKey
{
    return TextPackKey::FromPack(hashes, pack_name, proto_name, key2, key3);
}

TEST_CASE("ProtoTextBaker")
{
    using namespace BakerTests;

    TestRig rig;
    auto bakers = MakeRequestedBakers({string(ProtoTextBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == ProtoTextBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 6);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));

    SECTION("IgnoresNonProtoSourceFiles")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Data/Readme.txt", "not a proto");

        ProtoTextBaker baker(local_rig.MakeContext("ProtoTextIgnored"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.empty());
    }

    SECTION("SkipsUnsupportedTargetPath")
    {
        TestRig local_rig;
        local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
        local_rig.AddSourceFile("Items/Ignored.fopro", R"([ProtoItem]
$Name = IgnoredItem
$Text engl Name = Ignored
)");

        ProtoTextBaker baker(local_rig.MakeContext("ProtoTextPack"));
        baker.BakeFiles(local_rig.GetAllSourceFiles(), "skip.bin");

        CHECK(local_rig.Outputs.empty());
    }

    SECTION("BakesProtoTextPacksByTypeLanguageAndInheritedParents")
    {
        TestRig local_rig;
        OverrideSetting(local_rig.Settings.BakeLanguages, vector<string> {"engl", "russ"});
        local_rig.AddBakedFile("Metadata.fometa-server", MakeDynamicMetadataBlob({{"Entity", {{"Gizmo", "HasProtos"}}}}));
        local_rig.AddSourceFile("Items/TextItems.fopro", R"([ProtoItem]
$Name = BaseItem
$Text engl Name = Base item name
$Text engl Desc = Base item description

[ProtoItem]
$Name = ChildItem
$Parent = BaseItem
$Text engl Desc = Child item description
$Text engl Long = Line\nBreak
$Text russ Desc = Child item ru
$Text span Name = Nombre ignorado
)");
        local_rig.AddSourceFile("Critters/TextCritter.fopro", R"([ProtoCritter]
$Name = TextCritter
$Text engl Name = Critter name
)");
        local_rig.AddSourceFile("Maps/TextMap.fomap", R"([Header]
$Name = TextMap
$Text engl Name = Map name
)");
        local_rig.AddSourceFile("Locations/TextLocation.fopro", R"([ProtoLocation]
$Name = TextLocation
$Text engl Name = Location name
)");
        local_rig.AddSourceFile("Custom/TextGizmo.fopro", R"([ProtoGizmo]
$Name = TextGizmo
$Text engl Name = Custom gizmo name
)");

        ProtoTextBaker baker(local_rig.MakeContext("ProtoTextPack"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.size() == 10);
        CHECK_FALSE(local_rig.Outputs.contains("ProtoTextPack.Items.span.fotxt-bin"));

        HashStorage hashes;
        auto items_engl = LoadOutputTextPack(local_rig, "ProtoTextPack.Items.engl.fotxt-bin", hashes);
        auto items_russ = LoadOutputTextPack(local_rig, "ProtoTextPack.Items.russ.fotxt-bin", hashes);
        auto critters_engl = LoadOutputTextPack(local_rig, "ProtoTextPack.Critters.engl.fotxt-bin", hashes);
        auto maps_engl = LoadOutputTextPack(local_rig, "ProtoTextPack.Maps.engl.fotxt-bin", hashes);
        auto locations_engl = LoadOutputTextPack(local_rig, "ProtoTextPack.Locations.engl.fotxt-bin", hashes);
        auto protos_engl = LoadOutputTextPack(local_rig, "ProtoTextPack.Protos.engl.fotxt-bin", hashes);
        auto protos_russ = LoadOutputTextPack(local_rig, "ProtoTextPack.Protos.russ.fotxt-bin", hashes);

        CHECK(items_engl.GetStr(MakeTextKey(hashes, "Items", "BaseItem", "Name"), 0) == "Base item name");
        CHECK(items_engl.GetStr(MakeTextKey(hashes, "Items", "ChildItem", "Name"), 0) == "Base item name");
        CHECK(items_engl.GetStr(MakeTextKey(hashes, "Items", "ChildItem", "Desc"), 0) == "Child item description");
        CHECK(items_engl.GetStr(MakeTextKey(hashes, "Items", "ChildItem", "Long"), 0) == "Line\nBreak");
        CHECK(items_russ.GetStr(MakeTextKey(hashes, "Items", "ChildItem", "Name"), 0) == "Base item name");
        CHECK(items_russ.GetStr(MakeTextKey(hashes, "Items", "ChildItem", "Desc"), 0) == "Child item ru");
        CHECK(critters_engl.GetStr(MakeTextKey(hashes, "Critters", "TextCritter", "Name"), 0) == "Critter name");
        CHECK(maps_engl.GetStr(MakeTextKey(hashes, "Maps", "TextMap", "Name"), 0) == "Map name");
        CHECK(locations_engl.GetStr(MakeTextKey(hashes, "Locations", "TextLocation", "Name"), 0) == "Location name");
        CHECK(protos_engl.GetStr(MakeTextKey(hashes, "Protos", "TextGizmo", "Name"), 0) == "Custom gizmo name");
        CHECK(protos_russ.GetStr(MakeTextKey(hashes, "Protos", "TextGizmo", "Name"), 0) == "Custom gizmo name");
    }

    SECTION("AcceptsFixedTypeSections")
    {
        TestRig local_rig;
        OverrideSetting(local_rig.Settings.BakeLanguages, vector<string> {"engl"});
        local_rig.AddBakedFile("Metadata.fometa-server", MakeDynamicMetadataBlob({{"FixedType", {{"Blueprint"}}}}));
        local_rig.AddSourceFile("Protos/Blueprint.fopro", R"([Blueprint]
$Name = VaultDoorBlueprint
$Text engl Name = Blueprint name
)");

        ProtoTextBaker baker(local_rig.MakeContext("ProtoTextFixed"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.size() == 5);
        CHECK(local_rig.Outputs.contains("ProtoTextFixed.Protos.engl.fotxt-bin"));
    }

    SECTION("BakeCheckerCanSkipAllProtoTextTargets")
    {
        TestRig local_rig;
        OverrideSetting(local_rig.Settings.BakeLanguages, vector<string> {"engl", "russ"});
        local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
        local_rig.AddSourceFile("Items/Checked.fopro", R"([ProtoItem]
$Name = CheckedItem
$Text engl Name = Checked
)",
            77);

        vector<pair<string, uint64_t>> checks;
        ProtoTextBaker baker(local_rig.MakeContext("ProtoTextSkip", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        }));
        baker.BakeFiles(local_rig.GetAllSourceFiles(), "");

        CHECK(local_rig.Outputs.empty());
        REQUIRE(checks.size() == 10);
        CHECK(checks.front().second == 77);
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check.first == "ProtoTextSkip.Items.engl.fotxt-bin"; }));
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check.first == "ProtoTextSkip.Protos.russ.fotxt-bin"; }));
    }

    SECTION("RejectsInvalidProtoTextInputs")
    {
        {
            TestRig local_rig;
            local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
            local_rig.AddSourceFile("Items/BrokenSection.fopro", R"([BrokenSection]
$Name = Broken
$Text engl Name = Broken
)");

            ProtoTextBaker baker(local_rig.MakeContext("ProtoTextBrokenSection"));
            CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoTextBakerException);
        }
        {
            TestRig local_rig;
            local_rig.AddBakedFile("Metadata.fometa-server", MakeDynamicMetadataBlob({{"Entity", {{"AuditLog"}}}}));
            local_rig.AddSourceFile("Custom/AuditLog.fopro", R"([ProtoAuditLog]
$Name = AuditLogEntry
$Text engl Name = Audit
)");

            ProtoTextBaker baker(local_rig.MakeContext("ProtoTextNoProtos"));
            CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoTextBakerException);
        }
        {
            TestRig local_rig;
            local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
            local_rig.AddSourceFile("Custom/Unknown.fopro", R"([ProtoUnknownEntity]
$Name = UnknownProto
$Text engl Name = Unknown
)");

            ProtoTextBaker baker(local_rig.MakeContext("ProtoTextUnknownType"));
            CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoTextBakerException);
        }
        {
            TestRig local_rig;
            local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
            local_rig.AddSourceFile("Items/DuplicateOne.fopro", R"([ProtoItem]
$Name = DuplicateItem
$Text engl Name = One
)");
            local_rig.AddSourceFile("Items/DuplicateTwo.fopro", R"([ProtoItem]
$Name = DuplicateItem
$Text engl Name = Two
)");

            ProtoTextBaker baker(local_rig.MakeContext("ProtoTextDuplicate"));
            CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoTextBakerException);
        }
        {
            TestRig local_rig;
            local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
            local_rig.AddSourceFile("Items/MissingParent.fopro", R"([ProtoItem]
$Name = OrphanItem
$Parent = MissingItem
$Text engl Name = Orphan
)");

            ProtoTextBaker baker(local_rig.MakeContext("ProtoTextMissingParent"));
            CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoTextBakerException);
        }
        {
            TestRig local_rig;
            local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
            local_rig.AddSourceFile("Items/TooManyTextTokens.fopro", R"([ProtoItem]
$Name = VerboseItem
$Text engl Name Extra Overflow = Too many
)");

            ProtoTextBaker baker(local_rig.MakeContext("ProtoTextTooManyTokens"));
            CHECK_THROWS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        }
        {
            TestRig local_rig;
            OverrideSetting(local_rig.Settings.BakeLanguages, vector<string> {});
            local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
            local_rig.AddSourceFile("Items/NoLanguages.fopro", R"([ProtoItem]
$Name = NoLanguagesItem
$Text = No default language
)");

            CHECK_THROWS_AS(ProtoTextBaker(local_rig.MakeContext("ProtoTextNoLanguages")), ProtoTextBakerException);
        }
        {
            TestRig local_rig;
            local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());
            local_rig.AddSourceFile("Items/NestedMissingParent.fopro", R"([ProtoItem]
$Name = ParentWithMissingBase
$Parent = MissingGrandParent
$Text engl Name = Parent

[ProtoItem]
$Name = ChildWithBrokenParent
$Parent = ParentWithMissingBase
$Text engl Name = Child
)");

            ProtoTextBaker baker(local_rig.MakeContext("ProtoTextNestedMissingParent"));
            CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoTextBakerException);
        }
        {
            TestRig local_rig;
            OverrideSetting(local_rig.Settings.BakeLanguages, vector<string> {"engl"});
            local_rig.AddBakedFile("Metadata.fometa-server", MakeDynamicMetadataBlob({{"Entity", {{"Gizmo", "HasProtos"}, {"Widget", "HasProtos"}}}}));
            local_rig.AddSourceFile("Custom/Intersections.fopro", R"([ProtoGizmo]
$Name = SharedCustomProto
$Text engl Name = Gizmo name

[ProtoWidget]
$Name = SharedCustomProto
$Text engl Name = Widget name
)");

            ProtoTextBaker baker(local_rig.MakeContext("ProtoTextIntersection"));
            CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), ProtoTextBakerException);
        }
    }
}

FO_END_NAMESPACE
