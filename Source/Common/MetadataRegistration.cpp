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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "MetadataRegistration.h"

FO_BEGIN_NAMESPACE

static void RegisterDynamicMetadataEnums(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
static void RegisterDynamicMetadataEntities(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
static void RegisterDynamicMetadataEntityHolders(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
static void RegisterDynamicMetadataFixedTypes(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
static void RegisterDynamicMetadataValueTypes(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
static void RegisterDynamicMetadataRefTypes(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
static void RegisterDynamicMetadataProperties(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
static void RegisterDynamicMetadataEvents(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
static void RegisterDynamicMetadataRemoteCalls(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
static void RegisterDynamicMetadataSettings(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
static void RegisterDynamicMetadataMigrationRules(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data);
[[nodiscard]] static auto ReadMetadataStringView(DataReader& reader, size_t size) -> string_view;

void RegisterDynamicMetadata(ptr<EngineMetadata> meta, const_span<uint8_t> metadata_bin)
{
    FO_STACK_TRACE_ENTRY();

    // Read data
    map<string_view, vector<vector<string_view>>> engine_data;
    auto reader = DataReader(metadata_bin);
    const auto sections_count = reader.Read<uint16_t>();

    for (const auto i : iterate_range(sections_count)) {
        ignore_unused(i);

        const auto section_name_size = reader.Read<uint16_t>();
        const string_view section_name = ReadMetadataStringView(reader, section_name_size);

        const auto entries_count = reader.Read<uint32_t>();
        vector<vector<string_view>> entries;
        entries.reserve(entries_count);

        for (const auto j : iterate_range(entries_count)) {
            ignore_unused(j);

            auto& cur_entry = entries.emplace_back();
            const auto tokens_count = reader.Read<uint32_t>();

            for (const auto k : iterate_range(tokens_count)) {
                ignore_unused(k);

                const auto token_size = reader.Read<uint16_t>();
                const string_view token = ReadMetadataStringView(reader, token_size);

                cur_entry.emplace_back(token);
            }
        }

        engine_data.emplace(section_name, std::move(entries));
    }

    reader.VerifyEnd();

    RegisterDynamicMetadataEnums(meta, engine_data["Enum"]);
    RegisterDynamicMetadataEntities(meta, engine_data["Entity"]);
    RegisterDynamicMetadataEntityHolders(meta, engine_data["EntityHolder"]);
    RegisterDynamicMetadataFixedTypes(meta, engine_data["FixedType"]);
    RegisterDynamicMetadataValueTypes(meta, engine_data["ValueType"]);
    RegisterDynamicMetadataRefTypes(meta, engine_data["RefType"]);
    RegisterDynamicMetadataProperties(meta, engine_data["Property"]);
    RegisterDynamicMetadataEvents(meta, engine_data["Event"]);
    RegisterDynamicMetadataRemoteCalls(meta, engine_data["RemoteCall"]);
    RegisterDynamicMetadataSettings(meta, engine_data["Setting"]);
    RegisterDynamicMetadataMigrationRules(meta, engine_data["MigrationRule"]);
}

static auto ReadMetadataStringView(DataReader& reader, size_t size) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const_span<uint8_t> bytes = reader.ReadBytes(size);

    if (bytes.empty()) {
        return {};
    }

    ptr<const uint8_t> data = bytes.data();
    ptr<const char> chars = data.reinterpret_as<const char>();
    return {chars.get(), bytes.size()};
}

static void RegisterDynamicMetadataEnums(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(tokens.size() >= 2, "Enum metadata record is missing enum name or underlying type", tokens.size());
        FO_VERIFY_AND_THROW(tokens.size() % 2 == 0, "Enum metadata record must contain key/value token pairs after the header", tokens[0], tokens.size());
        const auto& enum_name = tokens[0];
        const auto& enum_underlying_type = tokens[1];

        if (!enum_underlying_type.empty()) {
            unordered_map<string, int32_t> key_values;

            for (size_t i = 2; i < tokens.size(); i += 2) {
                const auto key = tokens[i];
                const auto value = strvex(tokens[i + 1]).to_int32();
                key_values.emplace(key, value);
            }

            meta->RegisterEnumGroup(enum_name, enum_underlying_type, std::move(key_values));
        }
        else {
            for (size_t i = 2; i < tokens.size(); i += 2) {
                const auto key = tokens[i];
                const auto value = strvex(tokens[i + 1]).to_int32();

                meta->RegisterEnumEntry(enum_name, key, value);
            }
        }
    }
}

static void RegisterDynamicMetadataEntities(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(!tokens.empty(), "Entity metadata record is empty and cannot provide an entity type name", engine_data.size());
        const auto name = tokens[0];
        const auto flags = span(tokens).subspan(1);
        const auto is_global = std::ranges::any_of(flags, [](auto&& f) { return f == "Global"; });
        const auto has_protos = std::ranges::any_of(flags, [](auto&& f) { return f == "HasProtos"; });
        const auto has_statics = std::ranges::any_of(flags, [](auto&& f) { return f == "HasStatics"; });
        const auto has_abstract = std::ranges::any_of(flags, [](auto&& f) { return f == "HasAbstract"; });

        meta->RegisterEntityType(name, false, is_global, has_protos, has_statics, has_abstract);
    }
}

static void RegisterDynamicMetadataEntityHolders(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(tokens.size() >= 4, "EntityHolder metadata record is missing target, holder entity, target entity or entry name", tokens.size());
        const auto target = tokens[0];
        const auto holder_entity = tokens[1];
        const auto target_entity = tokens[2];
        const auto entry = tokens[3];
        const auto flags = span(tokens).subspan(4);
        const auto has_owner_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "OwnerSync"; });
        const auto has_public_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "PublicSync"; });
        const auto has_persistent = std::ranges::any_of(flags, [](auto&& f) { return f == "Persistent"; });
        const auto sync = has_public_sync ? EntityHolderEntrySync::PublicSync : (has_owner_sync ? EntityHolderEntrySync::OwnerSync : EntityHolderEntrySync::NoSync);

        if (target != "Stub") {
            meta->RegsiterEntityHolderEntry(holder_entity, target_entity, entry, sync, has_persistent);
        }
        else {
            auto prop_registrator = meta->GetPropertyRegistratorForEdit(holder_entity);
            ptr<const Property> prop = has_persistent ? //
                prop_registrator->RegisterProperty({"Server", "ident[]", strex("{}Ids", entry), "Persistent", "CoreProperty"}) : //
                prop_registrator->RegisterProperty({"Server", "ident[]", strex("{}Ids", entry), "CoreProperty"});
            meta->RegisterEnumEntry(strex("{}Property", holder_entity), strex("{}Ids", entry), numeric_cast<int32_t>(prop->GetRegIndex()));
        }
    }
}

static void RegisterDynamicMetadataFixedTypes(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(!tokens.empty(), "FixedType metadata record is empty and cannot provide a type name", engine_data.size());

        meta->RegisterFixedType(tokens[0], false);
    }
}

static void RegisterDynamicMetadataValueTypes(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(tokens.size() >= 3, "ValueType metadata record is missing type name or at least one field/type pair", tokens.size());
        FO_VERIFY_AND_THROW(tokens.size() % 2 == 1, "Dynamic value type metadata must contain a type name followed by field/type pairs", tokens[0], tokens.size());

        const auto& name = tokens[0];
        vector<pair<string_view, string_view>> layout;
        layout.reserve((tokens.size() - 1) / 2);

        for (size_t i = 1; i < tokens.size(); i += 2) {
            FO_VERIFY_AND_THROW(meta->IsValidBaseType(tokens[i + 1]), "ValueType metadata field has an invalid base type", tokens[i + 1], tokens[i], name);
            layout.emplace_back(tokens[i], tokens[i + 1]);
        }

        meta->RegisterValueType(name);
        meta->RegisterValueTypeLayout(name, layout);
    }
}

static void RegisterDynamicMetadataRefTypes(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(!tokens.empty(), "RefType metadata record is empty and cannot provide a type name", engine_data.size());

        meta->RegisterRefType(tokens[0]);
    }

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(!tokens.empty(), "RefType layout metadata record is empty and cannot provide a type name", engine_data.size());

        const auto& name = tokens[0];
        vector<vector<string_view>> layout;

        for (size_t i = 1; i < tokens.size();) {
            FO_VERIFY_AND_THROW(i + 2 < tokens.size(), "RefType metadata field record is truncated", name);

            const auto field_name = tokens[i];
            const auto field_type = tokens[i + 1];
            const auto flag_count_signed = strex(tokens[i + 2]).to_int32();
            FO_VERIFY_AND_THROW(flag_count_signed >= 0, "RefType metadata field has a negative flag count", field_name, name);
            const auto flag_count = numeric_cast<size_t>(flag_count_signed);

            FO_VERIFY_AND_THROW(i + 3 + flag_count <= tokens.size(), "RefType metadata field flags are truncated", field_name, name);

            const auto type = meta->ResolveComplexType(field_type);
            FO_VERIFY_AND_THROW(!type.IsMutable, "RefType metadata field has a mutable type", field_type, field_name, name);
            FO_VERIFY_AND_THROW(type.Kind != ComplexTypeKind::Callback, "RefType metadata field cannot use a callback type", field_name, name);
            FO_VERIFY_AND_THROW(!type.BaseType.IsEntity || type.BaseType.IsFixedType || type.BaseType.IsEntityProto, "RefType metadata field cannot reference a runtime entity type", field_name, name);
            FO_VERIFY_AND_THROW(!type.KeyType.has_value() || !type.KeyType->IsEntity, "RefType metadata dictionary key cannot reference a runtime entity type", field_type, field_name, name);

            vector<string_view> field_tokens;
            field_tokens.reserve(2 + flag_count);
            field_tokens.emplace_back(field_name);
            field_tokens.emplace_back(field_type);

            for (size_t k = 0; k < flag_count; k++) {
                field_tokens.emplace_back(tokens[i + 3 + k]);
            }

            layout.emplace_back(std::move(field_tokens));
            i += 3 + flag_count;
        }

        meta->RegisterRefTypeLayout(name, layout);
    }
}

static void RegisterDynamicMetadataProperties(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(tokens.size() >= 4, "Property metadata record is missing entity name or property declaration tokens", tokens.size());
        const auto entity_name = tokens[0];
        auto prop_registrator = meta->GetPropertyRegistratorForEdit(entity_name);
        const auto prop_tokens = span(tokens).subspan(1);

        auto prop = prop_registrator->RegisterProperty(prop_tokens);
        const string prop_enum_name = prop->IsInComponent() ? strex("{}_{}", prop->GetComponentName(), prop->GetNameWithoutComponent()).str() : string {prop->GetName()};
        meta->RegisterEnumEntry(strex("{}Property", entity_name), prop_enum_name, numeric_cast<int32_t>(prop->GetRegIndex()));
    }
}

static void RegisterDynamicMetadataEvents(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(tokens.size() >= 2, "Event metadata record is missing entity name or event name", tokens.size());
        FO_VERIFY_AND_THROW((tokens.size() - 2) % 3 == 0, "Event metadata arguments must be encoded as type/nullability/name triples", tokens[0], tokens[1], tokens.size());
        const auto entity_name = tokens[0];
        EntityEventDesc event;
        event.Name = tokens[1];

        for (size_t i = 2; i + 3 <= tokens.size(); i += 3) {
            auto arg_type = meta->ResolveComplexType(tokens[i]);
            const bool arg_nullable = tokens[i + 1] == "?";
            auto arg_name = string(tokens[i + 2]);
            event.Args.emplace_back(std::move(arg_name), std::move(arg_type), arg_nullable);
        }

        meta->RegisterEntityEvent(entity_name, std::move(event));
    }
}

static void RegisterDynamicMetadataRemoteCalls(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(tokens.size() >= 3, "RemoteCall metadata record is missing call name, subsystem hint or direction", tokens.size());
        FO_VERIFY_AND_THROW((tokens.size() - 3) % 3 == 0, "RemoteCall metadata arguments must be encoded as type/nullability/name triples", tokens[0], tokens[1], tokens[2], tokens.size());
        RemoteCallDesc remote_call;
        remote_call.Name = meta->Hashes.ToHashedString(tokens[0]);
        remote_call.SubsystemHint = tokens[1];
        const auto inbound = tokens[2] == "In";

        for (size_t i = 3; i + 3 <= tokens.size(); i += 3) {
            auto arg_type = meta->ResolveComplexType(tokens[i]);
            const bool arg_nullable = tokens[i + 1] == "?";
            auto arg_name = string(tokens[i + 2]);
            remote_call.Args.emplace_back(std::move(arg_name), std::move(arg_type), arg_nullable);
        }

        if (inbound) {
            meta->RegisterInboundRemoteCall(std::move(remote_call));
        }
        else {
            meta->RegisterOutboundRemoteCall(std::move(remote_call));
        }
    }
}

static void RegisterDynamicMetadataSettings(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(tokens.size() >= 2, "Setting metadata record is missing setting name or value type", tokens.size());
        const auto name = tokens[0];
        const auto& type = meta->GetBaseType(tokens[1]);

        meta->RegisterGameSetting(name, type);
    }
}

static void RegisterDynamicMetadataMigrationRules(ptr<EngineMetadata> meta, const vector<vector<string_view>>& engine_data)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tokens : engine_data) {
        FO_VERIFY_AND_THROW(tokens.size() >= 4, "MigrationRule metadata record is missing rule scope, version or target fields", tokens.size());

        meta->RegisterMigrationRule(tokens[0], tokens[1], tokens[2], tokens[3]);
    }
}

auto ReadMetadataBin(ptr<const FileSystem> resources, string_view target) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    const string target_lower = strex(target).lower();

    if (const auto restore_info = resources->ReadFile(strex("Metadata.fometa-{}", target_lower))) {
        return restore_info.GetData();
    }
    else {
        throw MetadataNotFoundException(FO_LINE_STR);
    }
}

FO_END_NAMESPACE
