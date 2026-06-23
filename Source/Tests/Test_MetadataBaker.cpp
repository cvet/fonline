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
#include "MetadataRegistration.h"
#endif

FO_BEGIN_NAMESPACE

TEST_CASE("MetadataBaker")
{
#if FO_ANGELSCRIPT_SCRIPTING
    using namespace BakerTests;

    TestRig rig;
    const auto bakers = MakeRequestedBakers({string(MetadataBaker::NAME)}, rig);
    const auto read_baked_tags = [](const vector<uint8_t>& output) {
        map<string, vector<vector<string>>> tags;
        DataReader reader(output);
        const auto tag_count = reader.Read<uint16_t>();

        for (uint16_t i = 0; i < tag_count; i++) {
            const auto tag_name_len = reader.Read<uint16_t>();
            const auto* tag_name_ptr = reader.ReadPtr<char>(tag_name_len);
            const string tag_name {tag_name_ptr, tag_name_len};
            const auto tag_value_count = reader.Read<uint32_t>();

            for (uint32_t j = 0; j < tag_value_count; j++) {
                const auto value_parts_count = reader.Read<uint32_t>();
                vector<string> value_parts;
                value_parts.reserve(value_parts_count);

                for (uint32_t k = 0; k < value_parts_count; k++) {
                    const auto part_len = reader.Read<uint16_t>();
                    const auto* part_ptr = reader.ReadPtr<char>(part_len);
                    value_parts.emplace_back(part_ptr, part_len);
                }

                tags[tag_name].emplace_back(std::move(value_parts));
            }
        }

        reader.VerifyEnd();
        return tags;
    };

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == MetadataBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 1);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));

    SECTION("serializes metadata tags from managed scripts")
    {
        rig.AddSourceFile("Scripts/Managed/TestManagedMetadata.cs", R"(
namespace TestManagedMetadata
{
///@ Setting Server bool ManagedMetadata.ServerFlag
///@ Setting Client bool ManagedMetadata.ClientFlag
///@ Enum ManagedMetadataKind ServerEntry
///@ Enum ManagedMetadataKind MapperEntry
///@ RefType Common ManagedMetadataSnapshot
///@ Property ManagedMetadataSnapshot Common int32 Value
///@ FixedType Mapper ManagedMetadataMarker
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-server"));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-mapper"));

        const auto server_tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-server"));
        const auto client_tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-client"));
        const auto mapper_tags = read_baked_tags(rig.Outputs.at("TestPack.fometa-mapper"));

        REQUIRE(server_tags.contains("Setting"));
        REQUIRE(client_tags.contains("Setting"));
        CHECK(std::ranges::count(server_tags.at("Setting"), vector<string> {"ManagedMetadata.ServerFlag", "bool"}) == 1);
        CHECK(std::ranges::count(client_tags.at("Setting"), vector<string> {"ManagedMetadata.ClientFlag", "bool"}) == 1);
        CHECK((!mapper_tags.contains("Setting") || mapper_tags.at("Setting").empty()));

        REQUIRE(server_tags.contains("Enum"));
        CHECK(std::ranges::count(server_tags.at("Enum"), vector<string> {"ManagedMetadataKind", "uint8", "ServerEntry", "0", "MapperEntry", "1"}) == 1);

        REQUIRE(client_tags.contains("RefType"));
        CHECK(std::ranges::count(client_tags.at("RefType"), vector<string> {"ManagedMetadataSnapshot", "Value", "int32", "0"}) == 1);

        REQUIRE(mapper_tags.contains("FixedType"));
        CHECK(std::ranges::count(mapper_tags.at("FixedType"), vector<string> {"ManagedMetadataMarker"}) == 1);
    }

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
        const auto tag_count = reader.Read<uint16_t>();

        vector<vector<string>> settings_entries;

        for (uint16_t i = 0; i < tag_count; i++) {
            const auto tag_name_len = reader.Read<uint16_t>();
            const auto* tag_name_ptr = reader.ReadPtr<char>(tag_name_len);
            const string tag_name {tag_name_ptr, tag_name_len};
            const auto tag_value_count = reader.Read<uint32_t>();

            for (uint32_t j = 0; j < tag_value_count; j++) {
                const auto value_parts_count = reader.Read<uint32_t>();
                vector<string> value_parts;
                value_parts.reserve(value_parts_count);

                for (uint32_t k = 0; k < value_parts_count; k++) {
                    const auto part_len = reader.Read<uint16_t>();
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

    SECTION("serializes migration rules with dotted names")
    {
        rig.AddSourceFile("Scripts/TestMigration.fos", R"(
namespace TestMigration
{
///@ MigrationRule Property Item Weapon.AmmoPid Weapon.Ammo
///@ MigrationRule Proto Modifier LegacyAchvO9tCm0 Remove
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        const auto& output = rig.Outputs.at("TestPack.fometa-client");
        DataReader reader(output);
        const auto tag_count = reader.Read<uint16_t>();

        vector<vector<string>> migration_entries;
        vector<vector<string>> settings_entries;

        for (uint16_t i = 0; i < tag_count; i++) {
            const auto tag_name_len = reader.Read<uint16_t>();
            const auto* tag_name_ptr = reader.ReadPtr<char>(tag_name_len);
            const string tag_name {tag_name_ptr, tag_name_len};
            const auto tag_value_count = reader.Read<uint32_t>();

            for (uint32_t j = 0; j < tag_value_count; j++) {
                const auto value_parts_count = reader.Read<uint32_t>();
                vector<string> value_parts;
                value_parts.reserve(value_parts_count);

                for (uint32_t k = 0; k < value_parts_count; k++) {
                    const auto part_len = reader.Read<uint16_t>();
                    const auto* part_ptr = reader.ReadPtr<char>(part_len);
                    value_parts.emplace_back(part_ptr, part_len);
                }

                if (tag_name == "MigrationRule") {
                    migration_entries.emplace_back(std::move(value_parts));
                }
                else if (tag_name == "Setting") {
                    settings_entries.emplace_back(std::move(value_parts));
                }
            }
        }

        reader.VerifyEnd();

        CHECK(std::ranges::count(migration_entries, vector<string> {"Property", "Item", "Weapon.AmmoPid", "Weapon.Ammo"}) == 1);
        CHECK(std::ranges::count(migration_entries, vector<string> {"Proto", "Modifier", "LegacyAchvO9tCm0", "Remove"}) == 1);
        CHECK(settings_entries.empty());

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ClientSide);
        REQUIRE_NOTHROW(RegisterDynamicMetadata(&meta, output));

        const auto property_rule = meta.CheckMigrationRule(meta.Hashes.ToHashedString("Property"), meta.Hashes.ToHashedString("Item"), meta.Hashes.ToHashedString("Weapon.AmmoPid"));
        REQUIRE(property_rule.has_value());
        CHECK(property_rule.value() == meta.Hashes.ToHashedString("Weapon.Ammo"));

        const auto proto_rule = meta.CheckMigrationRule(meta.Hashes.ToHashedString("Proto"), meta.Hashes.ToHashedString("Modifier"), meta.Hashes.ToHashedString("LegacyAchvO9tCm0"));
        REQUIRE(proto_rule.has_value());
        CHECK(proto_rule.value() == meta.Hashes.ToHashedString("Remove"));
    }

    SECTION("serializes ref type fields declared via property tags")
    {
        rig.AddSourceFile("Scripts/TestRefTypeProps.fos", R"(
namespace TestRefTypeProps
{
///@ RefType Common RouteSnapshot
///@ Property RouteSnapshot Common int32[] Steps
///@ Property RouteSnapshot Common hstring[] Tags
///@ Property RouteSnapshot Common string Note
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        const auto& output = rig.Outputs.at("TestPack.fometa-client");
        const auto tags = read_baked_tags(output);
        const auto ref_type_it = tags.find("RefType");

        REQUIRE(ref_type_it != tags.end());
        CHECK(std::ranges::count(ref_type_it->second, vector<string> {"RouteSnapshot", "Steps", "int32[]", "0", "Tags", "hstring[]", "0", "Note", "string", "0"}) == 1);

        const auto property_it = tags.find("Property");
        if (property_it != tags.end()) {
            CHECK(std::ranges::none_of(property_it->second, [](const auto& entry) { return !entry.empty() && entry.front() == "RouteSnapshot"; }));
        }

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ClientSide);
        REQUIRE_NOTHROW(RegisterDynamicMetadata(&meta, output));

        const auto& route_snapshot_type = meta.GetBaseType("RouteSnapshot");
        REQUIRE(route_snapshot_type.IsRefType);
        REQUIRE(route_snapshot_type.RefType != nullptr);
        REQUIRE(route_snapshot_type.RefType->FieldsRegistrator != nullptr);
        REQUIRE(route_snapshot_type.RefType->FieldsRegistrator->FindProperty("Steps") != nullptr);
        REQUIRE(route_snapshot_type.RefType->FieldsRegistrator->FindProperty("Tags") != nullptr);
        REQUIRE(route_snapshot_type.RefType->FieldsRegistrator->FindProperty("Note") != nullptr);
        CHECK(route_snapshot_type.RefType->FieldsRegistrator->FindProperty("Steps")->GetViewTypeName() == "int32[]");
        CHECK(route_snapshot_type.RefType->FieldsRegistrator->FindProperty("Tags")->GetViewTypeName() == "hstring[]");
        CHECK(route_snapshot_type.RefType->FieldsRegistrator->FindProperty("Note")->GetViewTypeName() == "string");
    }

    SECTION("registers entity properties that use baked ref types")
    {
        rig.AddSourceFile("Scripts/TestRefTypeEntityProps.fos", R"(
namespace TestRefTypeEntityProps
{
///@ RefType Common RouteSnapshot
///@ Property RouteSnapshot Common int32[] Steps
///@ Property RouteSnapshot Common string Note
///@ Property Critter Server RouteSnapshot Snapshot Mutable Persistent
///@ Property Critter Server RouteSnapshot[] Snapshots Mutable Persistent
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-server"));

        const auto& output = rig.Outputs.at("TestPack.fometa-server");
        const auto tags = read_baked_tags(output);
        const auto property_it = tags.find("Property");

        REQUIRE(property_it != tags.end());
        CHECK(std::ranges::count(property_it->second, vector<string> {"Critter", "Server", "RouteSnapshot", "Snapshot", "Mutable", "Persistent"}) == 1);
        CHECK(std::ranges::count(property_it->second, vector<string> {"Critter", "Server", "RouteSnapshot[]", "Snapshots", "Mutable", "Persistent"}) == 1);

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ServerSide);
        meta.RegisterEntityType("Critter", true, false, true, true, true);
        meta.RegisterEnumGroup("CritterProperty", "uint16", {{"None", 0}});
        REQUIRE_NOTHROW(RegisterDynamicMetadata(&meta, output));

        const auto* critter_registrator = meta.GetPropertyRegistrator("Critter");
        REQUIRE(critter_registrator != nullptr);

        const auto* snapshot_prop = critter_registrator->FindProperty("Snapshot");
        const auto* snapshots_prop = critter_registrator->FindProperty("Snapshots");

        REQUIRE(snapshot_prop != nullptr);
        REQUIRE(snapshots_prop != nullptr);
        CHECK(snapshot_prop->GetViewTypeName() == "RouteSnapshot");
        CHECK(snapshot_prop->IsBaseTypeRefType());
        CHECK_FALSE(snapshot_prop->IsArray());
        CHECK(snapshots_prop->GetViewTypeName() == "RouteSnapshot[]");
        CHECK(snapshots_prop->IsBaseTypeRefType());
        CHECK(snapshots_prop->IsArray());
        CHECK(meta.GetBaseType("RouteSnapshot").RefType->FieldsRegistrator->FindProperty("Steps") != nullptr);
        CHECK(meta.GetBaseType("RouteSnapshot").RefType->FieldsRegistrator->FindProperty("Note") != nullptr);
    }

    SECTION("registers ref types in declaration order")
    {
        rig.AddSourceFile("Scripts/TestNestedRefTypeProps.fos", R"(
namespace TestNestedRefTypeProps
{
///@ RefType Common Alpha
///@ Property Alpha Common Beta Dependency
///@ RefType Common Beta
///@ Property Beta Common int32 Value
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(rig.Outputs.contains("TestPack.fometa-client"));

        const auto& output = rig.Outputs.at("TestPack.fometa-client");
        const auto tags = read_baked_tags(output);
        const auto ref_type_it = tags.find("RefType");

        REQUIRE(ref_type_it != tags.end());
        REQUIRE(ref_type_it->second.size() == 2);
        CHECK(ref_type_it->second[0].front() == "Alpha");
        CHECK(ref_type_it->second[1].front() == "Beta");

        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ClientSide);
        REQUIRE_NOTHROW(RegisterDynamicMetadata(&meta, output));

        const auto& alpha_type = meta.GetBaseType("Alpha");
        const auto& beta_type = meta.GetBaseType("Beta");
        REQUIRE(alpha_type.IsRefType);
        REQUIRE(beta_type.IsRefType);
        REQUIRE(alpha_type.RefType != nullptr);
        REQUIRE(beta_type.RefType != nullptr);
        REQUIRE(alpha_type.RefType->FieldsRegistrator != nullptr);
        REQUIRE(beta_type.RefType->FieldsRegistrator != nullptr);

        const auto* dependency_prop = alpha_type.RefType->FieldsRegistrator->FindProperty("Dependency");
        REQUIRE(dependency_prop != nullptr);
        CHECK(dependency_prop->GetViewTypeName() == "Beta");
        CHECK(dependency_prop->IsBaseTypeRefType());
        CHECK(beta_type.RefType->FieldsRegistrator->FindProperty("Value") != nullptr);
    }

    SECTION("rejects legacy ref type layout syntax")
    {
        rig.AddSourceFile("Scripts/TestLegacyRefType.fos", R"(
namespace TestLegacyRefType
{
///@ RefType Common LegacyRefType Layout = int32-Value
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("Layout is no longer supported"));
    }

    SECTION("rejects ref type property flags")
    {
        rig.AddSourceFile("Scripts/TestRefTypeFlags.fos", R"(
namespace TestRefTypeFlags
{
///@ RefType Common RefTypeWithFlags
///@ Property RefTypeWithFlags Common int32 Value Persistent
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("RefType fields only allow the 'Component' flag"));
    }

    SECTION("accepts ref type component marker and component sub-properties")
    {
        rig.AddSourceFile("Scripts/TestRefTypeComponent.fos", R"(
namespace TestRefTypeComponent
{
///@ RefType Server RouteSnapshot
///@ Property RouteSnapshot Server bool Marker Component
///@ Property RouteSnapshot Server int32 Marker.Steps
///@ Property RouteSnapshot Server string Marker.Note
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        REQUIRE(rig.Outputs.contains("TestPack.fometa-server"));

        const auto& output = rig.Outputs.at("TestPack.fometa-server");
        const auto tags = read_baked_tags(output);
        const auto ref_type_it = tags.find("RefType");

        REQUIRE(ref_type_it != tags.end());
        // Encoded as: name, "<field> <type> <flag-count> <flag*>" repeating.
        CHECK(std::ranges::count(ref_type_it->second, vector<string> {"RouteSnapshot", "Marker", "bool", "1", "Component", "Marker.Steps", "int32", "0", "Marker.Note", "string", "0"}) == 1);
    }

    SECTION("rejects ref type component sub-property without declared component")
    {
        rig.AddSourceFile("Scripts/TestRefTypeComponentMissing.fos", R"(
namespace TestRefTypeComponentMissing
{
///@ RefType Server RouteSnapshot
///@ Property RouteSnapshot Server int32 Missing.Steps
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("RefType component is not declared"));
    }

    SECTION("rejects property owners that are not ref types")
    {
        rig.AddSourceFile("Scripts/TestValueTypePropertyOwner.fos", R"(
namespace TestValueTypePropertyOwner
{
///@ ValueType Common PlainSnapshot Layout = int32-Value
///@ Property PlainSnapshot Common int32 AnotherValue
}
)");

        MetadataBaker baker(rig.MakeContext());
        REQUIRE_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("only RefType supports script metadata properties"));
    }
#endif
}

FO_END_NAMESPACE
