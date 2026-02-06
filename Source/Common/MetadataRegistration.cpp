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

void RegisterDynamicMetadata(EngineMetadata* meta, const_span<uint8> metadata_bin)
{
    FO_STACK_TRACE_ENTRY();

    // Read data
    map<string_view, vector<vector<string_view>>> engine_data;
    auto reader = DataReader(metadata_bin);
    const auto sections_count = reader.Read<uint16>();

    for (const auto i : iterate_range(sections_count)) {
        ignore_unused(i);

        const auto section_name_size = reader.Read<uint16>();
        string_view section_name = {reader.ReadPtr<char>(section_name_size), section_name_size};

        const auto entries_count = reader.Read<uint32>();
        vector<vector<string_view>> entries;
        entries.reserve(entries_count);

        for (const auto j : iterate_range(entries_count)) {
            ignore_unused(j);

            auto& cur_entry = entries.emplace_back();
            const auto tokens_count = reader.Read<uint32>();

            for (const auto k : iterate_range(tokens_count)) {
                ignore_unused(k);

                const auto token_size = reader.Read<uint16>();
                string_view token = {reader.ReadPtr<char>(token_size), token_size};

                cur_entry.emplace_back(token);
            }
        }

        engine_data.emplace(section_name, std::move(entries));
    }

    reader.VerifyEnd();

    // Enums
    for (const auto& tokens : engine_data["Enum"]) {
        FO_RUNTIME_ASSERT(tokens.size() >= 2);
        FO_RUNTIME_ASSERT(tokens.size() % 2 == 0);
        const auto& enum_name = tokens[0];
        const auto& enum_underlying_type = tokens[1];

        if (!enum_underlying_type.empty()) {
            unordered_map<string, int32> key_values;

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

    // Entities
    for (const auto& tokens : engine_data["Entity"]) {
        FO_RUNTIME_ASSERT(!tokens.empty());
        const auto name = tokens[0];
        const auto flags = span(tokens).subspan(1);
        const auto is_global = std::ranges::any_of(flags, [](auto&& f) { return f == "Global"; });
        const auto has_protos = std::ranges::any_of(flags, [](auto&& f) { return f == "HasProtos"; });
        const auto has_statics = std::ranges::any_of(flags, [](auto&& f) { return f == "HasStatics"; });
        const auto has_abstract = std::ranges::any_of(flags, [](auto&& f) { return f == "HasAbstract"; });

        meta->RegisterEntityType(name, false, is_global, has_protos, has_statics, has_abstract);
    }

    // Entity holders
    for (const auto& tokens : engine_data["EntityHolder"]) {
        FO_RUNTIME_ASSERT(tokens.size() >= 4);
        const auto target = tokens[0];
        const auto holder_entity = tokens[1];
        const auto target_entity = tokens[2];
        const auto entry = tokens[3];
        const auto flags = span(tokens).subspan(4);
        const auto has_owner_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "OwnerSync"; });
        const auto has_public_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "PublicSync"; });
        const auto sync = has_public_sync ? EntityHolderEntrySync::PublicSync : (has_owner_sync ? EntityHolderEntrySync::OwnerSync : EntityHolderEntrySync::NoSync);

        if (target != "Stub") {
            meta->RegsiterEntityHolderEntry(holder_entity, target_entity, entry, sync);
        }
        else {
            auto* prop_registrator = meta->GetPropertyRegistratorForEdit(holder_entity);
            const auto* prop = prop_registrator->RegisterProperty({"Server", "ident[]", strex("{}Ids", entry), "Persistent", "CoreProperty"});
            meta->RegisterEnumEntry(strex("{}Property", holder_entity), strex("{}Ids", entry), numeric_cast<int32>(prop->GetRegIndex()));
        }
    }

    // Property components
    for (const auto& tokens : engine_data["PropertyComponent"]) {
        FO_RUNTIME_ASSERT(tokens.size() >= 2);
        const auto entity_name = tokens[0];
        const auto component_name = tokens[1];
        const auto component_hname = meta->Hashes.ToHashedString(component_name);
        auto* prop_registrator = meta->GetPropertyRegistratorForEdit(entity_name);

        prop_registrator->RegisterComponent(component_name);
        meta->RegisterEnumEntry(strex("{}Component", entity_name), component_name, component_hname.as_int32());
    }

    // Properties
    for (const auto& tokens : engine_data["Property"]) {
        FO_RUNTIME_ASSERT(tokens.size() >= 4);
        const auto entity_name = tokens[0];
        auto* prop_registrator = meta->GetPropertyRegistratorForEdit(entity_name);
        const auto prop_tokens = span(tokens).subspan(1);

        const auto* prop = prop_registrator->RegisterProperty(prop_tokens);
        const auto prop_enum_name = prop->GetComponent() ? strex("{}_{}", prop->GetComponent(), prop->GetNameWithoutComponent()) : prop->GetName();
        meta->RegisterEnumEntry(strex("{}Property", entity_name), prop_enum_name, numeric_cast<int32>(prop->GetRegIndex()));
    }

    // Events
    for (const auto& tokens : engine_data["Event"]) {
        FO_RUNTIME_ASSERT(tokens.size() >= 3);
        FO_RUNTIME_ASSERT((tokens.size() - 3) % 2 == 0);
        const auto entity_name = tokens[0];
        EntityEventDesc event;
        event.Name = tokens[1];
        event.Deferred = tokens[2] == "Deferred";

        for (size_t i = 3; i < tokens.size() - 1; i += 2) {
            auto arg_type = meta->ResolveComplexType(tokens[i]);
            auto arg_name = string(tokens[i + 1]);
            event.Args.emplace_back(std::move(arg_name), std::move(arg_type));
        }

        meta->RegisterEntityEvent(entity_name, std::move(event));
    }

    // Remote calls
    for (const auto& tokens : engine_data["RemoteCall"]) {
        FO_RUNTIME_ASSERT(tokens.size() >= 3);
        FO_RUNTIME_ASSERT((tokens.size() - 3) % 2 == 0);
        RemoteCallDesc remote_call;
        remote_call.Name = meta->Hashes.ToHashedString(tokens[0]);
        remote_call.SubsystemHint = tokens[1];
        const auto inbound = tokens[2] == "In";

        for (size_t i = 3; i < tokens.size() - 1; i += 2) {
            auto arg_type = meta->ResolveComplexType(tokens[i]);
            auto arg_name = string(tokens[i + 1]);
            remote_call.Args.emplace_back(std::move(arg_name), std::move(arg_type));
        }

        if (inbound) {
            meta->RegisterInboundRemoteCall(std::move(remote_call));
        }
        else {
            meta->RegisterOutboundRemoteCall(std::move(remote_call));
        }
    }

    // Settings
    for (const auto& tokens : engine_data["Setting"]) {
        FO_RUNTIME_ASSERT(tokens.size() >= 2);
        const auto name = tokens[0];
        const auto& type = meta->GetBaseType(tokens[1]);

        meta->RegisterGameSetting(name, type);
    }

    // Migration rules
    for (const auto& tokens : engine_data["MigrationRule"]) {
        FO_RUNTIME_ASSERT(tokens.size() >= 4);

        meta->RegisterMigrationRule(tokens[0], tokens[1], tokens[2], tokens[3]);
    }
}

auto ReadMetadataBin(const FileSystem* resources, string_view target) -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(resources);

    const string target_lower = strex(target).lower();

    if (const auto restore_info = resources->ReadFile(strex("Metadata.fometa-{}", target_lower))) {
        return restore_info.GetData();
    }
    else {
        throw MetadataNotFoundException(FO_LINE_STR);
    }
}

FO_END_NAMESPACE
