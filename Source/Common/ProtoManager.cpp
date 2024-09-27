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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ProtoManager.h"
#include "ConfigFile.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "StringUtils.h"

ProtoManager::ProtoManager(FOEngineBase* engine) :
    _engine {engine},
    _migrationRuleName {_engine->ToHashedString("Proto")},
    _itemTypeName {_engine->ToHashedString(ProtoItem::ENTITY_TYPE_NAME)},
    _crTypeName {_engine->ToHashedString(ProtoCritter::ENTITY_TYPE_NAME)},
    _mapTypeName {_engine->ToHashedString(ProtoMap::ENTITY_TYPE_NAME)},
    _locTypeName {_engine->ToHashedString(ProtoLocation::ENTITY_TYPE_NAME)}
{
    STACK_TRACE_ENTRY();
}

void ProtoManager::ParseProtos(FileSystem& resources)
{
    STACK_TRACE_ENTRY();

    const auto proto_rule_name = _engine->ToHashedString("Proto");
    const auto component_rule_name = _engine->ToHashedString("Component");

    // Collect data
    unordered_map<hstring, unordered_map<hstring, map<string, string>>> all_file_protos;

    for (const auto* ext : {"fopro", "fomap", "foitem", "focr", "foloc"}) {
        const bool is_fomap = string_view(ext) == "fomap";
        auto files = resources.FilterFiles(ext);

        while (files.MoveNext()) {
            auto file = files.GetCurFile();

            auto fopro_options = ConfigFileOption::None;

            if (is_fomap) {
                fopro_options = ConfigFileOption::ReadFirstSection;
            }

            auto fopro = ConfigFile(file.GetPath(), file.GetStr(), _engine, fopro_options);

            for (auto& section : fopro.GetSections()) {
                const auto& section_name = section.first;
                auto& section_kv = section.second;

                // Skip default section
                if (section_name.empty()) {
                    continue;
                }

                hstring type_name;

                if (is_fomap && section_name == "Header") {
                    type_name = _engine->ToHashedString("Map");
                }
                else if (strex(section_name).startsWith("Proto") && section_name.length() > "Proto"_len) {
                    type_name = _engine->ToHashedString(section_name.substr("Proto"_len));
                }
                else {
                    throw ProtoManagerException("Invalid proto section name", section_name, file.GetName());
                }

                if (!_engine->IsValidEntityType(type_name) || !_engine->GetEntityTypeInfo(type_name).HasProtos) {
                    throw ProtoManagerException("Invalid proto type", section_name, file.GetName());
                }

                const auto name = section_kv.count("$Name") != 0 ? section_kv.at("$Name") : file.GetName();
                auto pid = _engine->ToHashedString(name);

                pid = _engine->CheckMigrationRule(proto_rule_name, type_name, pid).value_or(pid);

                auto& file_protos = all_file_protos[type_name];

                if (file_protos.count(pid) != 0) {
                    throw ProtoManagerException("Proto already loaded", type_name, pid, file.GetName());
                }

                file_protos.emplace(pid, section_kv);
            }
        }
    }

    // Processing
    const auto insert_map_values = [](const map<string, string>& from_kv, map<string, string>& to_kv, bool overwrite) {
        for (auto&& [key, value] : from_kv) {
            RUNTIME_ASSERT(!key.empty());

            if (key[0] != '$' || strex(key).startsWith("$Text")) {
                if (overwrite) {
                    to_kv[key] = value;
                }
                else {
                    to_kv.emplace(key, value);
                }
            }
            else if (key == "$Components" && !value.empty()) {
                if (to_kv.count(key) == 0) {
                    to_kv[key] = value;
                }
                else {
                    to_kv[key] += " " + value;
                }
            }
        }
    };

    for (auto&& file_protos : all_file_protos) {
        const auto& type_name = file_protos.first;
        auto& file_proto_pids = file_protos.second;

        // Injection
        auto injection = [&](const string& key_name, bool overwrite) {
            for (auto&& [pid, kv] : file_proto_pids) {
                if (kv.count(key_name) != 0) {
                    for (const auto& inject_name : strex(kv[key_name]).split(' ')) {
                        if (inject_name == "All") {
                            for (auto&& [other_pid, other_kv] : file_proto_pids) {
                                if (other_pid != pid) {
                                    insert_map_values(kv, other_kv, overwrite);
                                }
                            }
                        }
                        else {
                            auto inject_pid = _engine->ToHashedString(inject_name);

                            inject_pid = _engine->CheckMigrationRule(proto_rule_name, type_name, inject_pid).value_or(inject_pid);

                            if (file_proto_pids.count(inject_pid) == 0) {
                                throw ProtoManagerException("Proto not found for injection from another proto", inject_name, pid);
                            }

                            insert_map_values(kv, file_proto_pids[inject_pid], overwrite);
                        }
                    }
                }
            }
        };

        injection("$Inject", false);

        // Protos
        for (auto&& [pid, file_kv] : file_proto_pids) {
            const auto base_name = pid.as_str();
            RUNTIME_ASSERT(_protos[type_name].count(pid) == 0);

            // Fill content from parents
            map<string, string> final_kv;

            std::function<void(string_view, map<string, string>&)> fill_parent = [&](string_view name, map<string, string>& cur_kv) {
                const auto parent_name_line = cur_kv.count("$Parent") != 0 ? cur_kv["$Parent"] : string();

                for (auto& parent_name : strex(parent_name_line).split(' ')) {
                    auto parent_pid = _engine->ToHashedString(parent_name);

                    parent_pid = _engine->CheckMigrationRule(proto_rule_name, type_name, parent_pid).value_or(parent_pid);

                    auto it_parent = file_proto_pids.find(parent_pid);

                    if (it_parent == file_proto_pids.end()) {
                        if (base_name == name) {
                            throw ProtoManagerException("Proto fail to load parent", base_name, parent_name);
                        }

                        throw ProtoManagerException("Proto fail to load parent for another proto", base_name, parent_name, name);
                    }

                    fill_parent(parent_name, it_parent->second);
                    insert_map_values(it_parent->second, final_kv, true);
                }
            };

            fill_parent(base_name, file_kv);

            // Actual content
            insert_map_values(file_kv, final_kv, true);

            // Final injection
            injection("$InjectOverride", true);

            // Create proto
            const auto* property_registrator = _engine->GetPropertyRegistrator(type_name);
            auto props = Properties(property_registrator);

            if (!props.ApplyFromText(final_kv)) {
                throw ProtoManagerException("Proto item fail to load properties", base_name);
            }

            auto* proto = CreateProto(type_name, pid, &props);

            // Components
            if (final_kv.count("$Components") != 0) {
                for (const auto& component_name : strex(final_kv["$Components"]).split(' ')) {
                    auto component_name_hashed = _engine->ToHashedString(component_name);
                    component_name_hashed = _engine->CheckMigrationRule(component_rule_name, type_name, component_name_hashed).value_or(component_name_hashed);

                    if (!proto->GetProperties().GetRegistrator()->IsComponentRegistered(component_name_hashed)) {
                        throw ProtoManagerException("Proto item has invalid component", base_name, component_name);
                    }

                    proto->EnableComponent(component_name_hashed);
                }
            }

            // Texts
            const string default_lang = !_engine->Settings.Languages.empty() ? _engine->Settings.Languages.front() : "";

            _parsedTexts[type_name][pid] = {};

            for (auto& kv : final_kv) {
                if (strex(kv.first).startsWith("$Text")) {
                    const auto key_tok = strex(kv.first).split(' ');
                    const string lang = key_tok.size() >= 2 ? key_tok[1] : default_lang;

                    TextPackKey text_key = pid.as_uint();

                    for (size_t i = 2; i < key_tok.size(); i++) {
                        const string& num = key_tok[i];

                        if (!num.empty()) {
                            if (strex(num).isNumber()) {
                                text_key += strex(num).toUInt();
                            }
                            else {
                                text_key += _engine->ToHashedString(num).as_uint();
                            }
                        }
                    }

                    _parsedTexts[type_name][pid][lang].AddStr(text_key, kv.second);
                }
            }
        }
    }

    // Mapper collections
    for (auto&& [pid, proto] : _itemProtos) {
        if (!proto->GetComponents().empty()) {
            const_cast<ProtoItem*>(proto)->CollectionName = strex(*proto->GetComponents().begin()).lower();
        }
        else {
            const_cast<ProtoItem*>(proto)->CollectionName = "other";
        }
    }

    for (auto&& [pid, proto] : _crProtos) {
        const_cast<ProtoCritter*>(proto)->CollectionName = "all";
    }
}

auto ProtoManager::CreateProto(hstring type_name, hstring pid, const Properties* props) -> ProtoEntity*
{
    STACK_TRACE_ENTRY();

    const auto create_proto = [&, this]() -> ProtoEntity* {
        const auto* registrator = _engine->GetPropertyRegistrator(type_name);
        RUNTIME_ASSERT(registrator);

        if (type_name == ProtoLocation::ENTITY_TYPE_NAME) {
            auto* proto = new ProtoLocation(pid, registrator, props);
            _locProtos.emplace(pid, proto);

            return proto;
        }
        else if (type_name == ProtoMap::ENTITY_TYPE_NAME) {
            auto* proto = new ProtoMap(pid, registrator, props);
            _mapProtos.emplace(pid, proto);

            return proto;
        }
        else if (type_name == ProtoCritter::ENTITY_TYPE_NAME) {
            auto* proto = new ProtoCritter(pid, registrator, props);
            _crProtos.emplace(pid, proto);

            return proto;
        }
        else if (type_name == ProtoItem::ENTITY_TYPE_NAME) {
            auto* proto = new ProtoItem(pid, registrator, props);
            _itemProtos.emplace(pid, proto);

            return proto;
        }
        else {
            return new ProtoCustomEntity(pid, registrator, props);
        }
    };

    ProtoEntity* proto = create_proto();

    const auto inserted = _protos[type_name].emplace(pid, proto).second;
    RUNTIME_ASSERT(inserted);

    return proto;
}

void ProtoManager::LoadFromResources()
{
    STACK_TRACE_ENTRY();

    string protos_fname = "Protos.foprob";

#if !FO_SINGLEPLAYER
    switch (_engine->GetPropertiesRelation()) {
    case PropertiesRelationType::BothRelative:
        protos_fname = "FullProtos.foprob";
        break;
    case PropertiesRelationType::ServerRelative:
        protos_fname = "ServerProtos.foprob";
        break;
    case PropertiesRelationType::ClientRelative:
        protos_fname = "ClientProtos.foprob";
        break;
    }
#endif

    const auto protos_file = _engine->Resources.ReadFile(protos_fname);

    if (!protos_file) {
        throw ProtoManagerException("Protos binary file not found", protos_fname);
    }

    auto reader = DataReader({protos_file.GetBuf(), protos_file.GetSize()});

    // Hashes
    {
        const auto hashes_count = reader.Read<uint>();

        string str;

        for (uint i = 0; i < hashes_count; i++) {
            const auto str_len = reader.Read<uint>();
            str.resize(str_len);
            reader.ReadPtr(str.data(), str.length());
            const auto hstr = _engine->ToHashedString(str);
            UNUSED_VARIABLE(hstr);
        }
    }

    // Protos
    {
        vector<uint8> props_data;

        const auto types_count = reader.Read<uint>();

        for (uint i = 0; i < types_count; i++) {
            const auto protos_count = reader.Read<uint>();

            const auto type_name_len = reader.Read<uint16>();
            const auto type_name_str = string(reader.ReadPtr<char>(type_name_len), type_name_len);
            const auto type_name = _engine->ToHashedString(type_name_str);

            RUNTIME_ASSERT(_engine->IsValidEntityType(type_name));

            for (uint j = 0; j < protos_count; j++) {
                const auto proto_name_len = reader.Read<uint16>();
                const auto proto_name = string(reader.ReadPtr<char>(proto_name_len), proto_name_len);
                const auto proto_id = _engine->ToHashedString(proto_name);

                auto* proto = CreateProto(type_name, proto_id, nullptr);

                const auto components_count = reader.Read<uint16>();

                for (uint16 k = 0; k < components_count; k++) {
                    const auto component_name_len = reader.Read<uint16>();
                    const auto component_name = string(reader.ReadPtr<char>(component_name_len), component_name_len);
                    const auto component_name_hashed = _engine->ToHashedString(component_name);

                    proto->EnableComponent(component_name_hashed);
                }

                const uint data_size = reader.Read<uint>();
                props_data.resize(data_size);
                reader.ReadPtr<uint8>(props_data.data(), data_size);
                proto->GetPropertiesForEdit().RestoreAllData(props_data);
            }
        }
    }

    reader.VerifyEnd();
}

auto ProtoManager::GetProtosBinaryData() const -> vector<uint8>
{
    STACK_TRACE_ENTRY();

    vector<uint8> protos_data;
    set<hstring> str_hashes;

    {
        auto writer = DataWriter(protos_data);

        vector<uint8> props_data;

        writer.Write<uint>(static_cast<uint>(_protos.size()));

        for (auto&& [type_name, protos] : _protos) {
            writer.Write<uint>(static_cast<uint>(protos.size()));

            writer.Write<uint16>(static_cast<uint16>(type_name.as_str().length()));
            writer.WritePtr(type_name.as_str().data(), type_name.as_str().length());

            for (auto&& [pid, proto] : protos) {
                const auto proto_name = proto->GetName();
                writer.Write<uint16>(static_cast<uint16>(proto_name.length()));
                writer.WritePtr(proto_name.data(), proto_name.length());

                writer.Write<uint16>(static_cast<uint16>(proto->GetComponents().size()));

                for (const auto& component : proto->GetComponents()) {
                    const auto& component_str = component.as_str();
                    writer.Write<uint16>(static_cast<uint16>(component_str.length()));
                    writer.WritePtr(component_str.data(), component_str.length());
                }

                proto->GetProperties().StoreAllData(props_data, str_hashes);
                writer.Write<uint>(static_cast<uint>(props_data.size()));
                writer.WritePtr(props_data.data(), props_data.size());
            }
        }
    }

    vector<uint8> data;

    {
        auto final_writer = DataWriter(data);

        final_writer.Write<uint>(static_cast<uint>(str_hashes.size()));

        for (const auto& hstr : str_hashes) {
            const auto& str = hstr.as_str();
            final_writer.Write<uint>(static_cast<uint>(str.length()));
            final_writer.WritePtr(str.c_str(), str.length());
        }

        final_writer.WritePtr(protos_data.data(), protos_data.size());
    }

    return data;
}

auto ProtoManager::GetProtoItem(hstring proto_id) noexcept(false) -> const ProtoItem*
{
    STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _itemTypeName, proto_id).value_or(proto_id);

    if (const auto it = _itemProtos.find(proto_id); it != _itemProtos.end()) {
        return it->second;
    }

    throw ProtoManagerException("Item proto not exists", proto_id);
}

auto ProtoManager::GetProtoCritter(hstring proto_id) noexcept(false) -> const ProtoCritter*
{
    STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _crTypeName, proto_id).value_or(proto_id);

    if (const auto it = _crProtos.find(proto_id); it != _crProtos.end()) {
        return it->second;
    }

    throw ProtoManagerException("Critter proto not exists", proto_id);
}

auto ProtoManager::GetProtoMap(hstring proto_id) noexcept(false) -> const ProtoMap*
{
    STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _mapTypeName, proto_id).value_or(proto_id);

    if (const auto it = _mapProtos.find(proto_id); it != _mapProtos.end()) {
        return it->second;
    }

    throw ProtoManagerException("Map proto not exists", proto_id);
}

auto ProtoManager::GetProtoLocation(hstring proto_id) noexcept(false) -> const ProtoLocation*
{
    STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _locTypeName, proto_id).value_or(proto_id);

    if (const auto it = _locProtos.find(proto_id); it != _locProtos.end()) {
        return it->second;
    }

    throw ProtoManagerException("Location proto not exists", proto_id);
}

auto ProtoManager::GetProtoEntity(hstring type_name, hstring proto_id) noexcept(false) -> const ProtoEntity*
{
    STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, type_name, proto_id).value_or(proto_id);

    const auto it_type = _protos.find(type_name);

    if (it_type == _protos.end()) {
        throw ProtoManagerException("Entity type protos not exist", type_name);
    }

    if (const auto it = it_type->second.find(proto_id); it != it_type->second.end()) {
        return it->second;
    }

    throw ProtoManagerException("Entity proto not exists", type_name, proto_id);
}

auto ProtoManager::GetProtoItemSafe(hstring proto_id) noexcept -> const ProtoItem*
{
    STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _itemTypeName, proto_id).value_or(proto_id);

    if (const auto it = _itemProtos.find(proto_id); it != _itemProtos.end()) {
        return it->second;
    }

    return nullptr;
}

auto ProtoManager::GetProtoCritterSafe(hstring proto_id) noexcept -> const ProtoCritter*
{
    STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _crTypeName, proto_id).value_or(proto_id);

    if (const auto it = _crProtos.find(proto_id); it != _crProtos.end()) {
        return it->second;
    }

    return nullptr;
}

auto ProtoManager::GetProtoMapSafe(hstring proto_id) noexcept -> const ProtoMap*
{
    STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _mapTypeName, proto_id).value_or(proto_id);

    if (const auto it = _mapProtos.find(proto_id); it != _mapProtos.end()) {
        return it->second;
    }

    return nullptr;
}

auto ProtoManager::GetProtoLocationSafe(hstring proto_id) noexcept -> const ProtoLocation*
{
    STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, _locTypeName, proto_id).value_or(proto_id);

    if (const auto it = _locProtos.find(proto_id); it != _locProtos.end()) {
        return it->second;
    }

    return nullptr;
}

auto ProtoManager::GetProtoEntitySafe(hstring type_name, hstring proto_id) noexcept -> const ProtoEntity*
{
    STACK_TRACE_ENTRY();

    proto_id = _engine->CheckMigrationRule(_migrationRuleName, type_name, proto_id).value_or(proto_id);

    const auto it_type = _protos.find(type_name);

    if (it_type == _protos.end()) {
        return nullptr;
    }

    if (const auto it = it_type->second.find(proto_id); it != it_type->second.end()) {
        return it->second;
    }

    return nullptr;
}

auto ProtoManager::GetProtoEntities(hstring type_name) const noexcept -> const unordered_map<hstring, const ProtoEntity*>&
{
    STACK_TRACE_ENTRY();

    const auto it_type = _protos.find(type_name);

    if (it_type == _protos.end()) {
        return _emptyProtos;
    }

    return it_type->second;
}
