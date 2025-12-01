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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ProtoBaker.h"
#include "AnyData.h"
#include "ConfigFile.h"
#include "EngineBase.h"
#include "EntityProtos.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE();

ProtoBaker::ProtoBaker(BakerData& data) :
    BaseBaker(data)
{
    FO_STACK_TRACE_ENTRY();
}

ProtoBaker::~ProtoBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void ProtoBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    if (!target_path.empty() && !strex(target_path).get_file_extension().starts_with("fopro-")) {
        return;
    }

    vector<File> filtered_files;
    uint64 max_write_time = 0;

    for (const auto& file_header : files) {
        const string ext = strex(file_header.GetPath()).get_file_extension();
        const auto it = std::ranges::find(_settings->ProtoFileExtensions, ext);

        if (it == _settings->ProtoFileExtensions.end()) {
            continue;
        }

        max_write_time = std::max(max_write_time, file_header.GetWriteTime());
        filtered_files.emplace_back(File::Load(file_header));
    }

    if (filtered_files.empty()) {
        return;
    }

    vector<std::future<void>> file_bakings;

    if (!_bakeChecker || _bakeChecker(_resPackName + ".fopro-bin-server", max_write_time)) {
        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] {
            auto engine = BakerEngine(PropertiesRelationType::ServerRelative);
            const auto script_sys = BakerScriptSystem(engine, *_bakedFiles);
            auto data = BakeProtoFiles(&engine, &script_sys, filtered_files);
            _writeData(_resPackName + ".fopro-bin-server", data);
        }));
    }

    if (!_bakeChecker || _bakeChecker(_resPackName + ".fopro-bin-client", max_write_time)) {
        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] {
            const auto engine = BakerEngine(PropertiesRelationType::ClientRelative);
            auto data = BakeProtoFiles(&engine, nullptr, filtered_files);
            _writeData(_resPackName + ".fopro-bin-client", data);
        }));
    }

    if (!_bakeChecker || _bakeChecker(_resPackName + ".fopro-bin-mapper", max_write_time)) {
        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] {
            const auto engine = BakerEngine(PropertiesRelationType::BothRelative);
            auto data = BakeProtoFiles(&engine, nullptr, filtered_files);
            _writeData(_resPackName + ".fopro-bin-mapper", data);
        }));
    }

    size_t errors = 0;

    for (auto& file_baking : file_bakings) {
        try {
            file_baking.get();
        }
        catch (const std::exception& ex) {
            WriteLog("Proto baking error: {}", ex.what());
            errors++;
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    if (errors != 0) {
        throw ProtoBakerException("Errors during protos parsing");
    }
}

auto ProtoBaker::BakeProtoFiles(const EngineData* engine, const ScriptSystem* script_sys, const vector<File>& files) const -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    const auto proto_rule_name = engine->Hashes.ToHashedString("Proto");
    const auto component_rule_name = engine->Hashes.ToHashedString("Component");

    // Collect data
    unordered_map<hstring, unordered_map<hstring, map<string, string>>> all_file_protos;

    for (const auto& file : files) {
        const bool is_fomap = strex(file.GetPath()).get_file_extension() == "fomap";
        const auto fopro_options = is_fomap ? ConfigFileOption::ReadFirstSection : ConfigFileOption::None;
        auto fopro = ConfigFile(file.GetPath(), file.GetStr(), &engine->Hashes, fopro_options);

        for (auto& section : fopro.GetSections()) {
            const auto& section_name = section.first;
            auto& section_kv = section.second;

            // Skip default section
            if (section_name.empty()) {
                continue;
            }

            hstring type_name;

            if (is_fomap && section_name == "Header") {
                type_name = engine->Hashes.ToHashedString("Map");
            }
            else if (strex(section_name).starts_with("Proto") && section_name.length() > "Proto"_len) {
                type_name = engine->Hashes.ToHashedString(section_name.substr("Proto"_len));
            }
            else {
                throw ProtoBakerException("Invalid proto section name", section_name, file.GetPath());
            }

            if (!engine->IsValidEntityType(type_name) || !engine->GetEntityTypeInfo(type_name).HasProtos) {
                throw ProtoBakerException("Invalid proto type", section_name, file.GetPath());
            }

            const auto name = section_kv.count("$Name") != 0 ? section_kv.at("$Name") : file.GetNameNoExt();
            auto pid = engine->Hashes.ToHashedString(name);
            pid = engine->CheckMigrationRule(proto_rule_name, type_name, pid).value_or(pid);

            auto& file_protos = all_file_protos[type_name];

            if (file_protos.count(pid) != 0) {
                throw ProtoBakerException("Proto already loaded", type_name, pid, file.GetPath());
            }

            file_protos.emplace(pid, section_kv);
        }
    }

    // Processing
    unordered_map<hstring, unordered_map<hstring, refcount_ptr<ProtoEntity>>> all_protos;

    const auto insert_map_values = [](const map<string, string>& from_kv, map<string, string>& to_kv) {
        for (auto&& [key, value] : from_kv) {
            FO_RUNTIME_ASSERT(!key.empty());

            if (key.front() != '$') {
                to_kv[key] = value;
            }
        }
    };

    for (const auto& file_protos : all_file_protos) {
        const auto& type_name = file_protos.first;
        const auto& file_proto_pids = file_protos.second;

        for (auto&& [pid, file_kv] : file_proto_pids) {
            const auto base_name = pid.as_str();
            FO_RUNTIME_ASSERT(all_protos[type_name].count(pid) == 0);

            // Fill content from parents
            map<string, string> proto_kv;

            function<void(string_view, const map<string, string>&)> fill_parent_recursive = [&](string_view name, const map<string, string>& cur_kv) {
                const auto parent_name_line = cur_kv.count("$Parent") != 0 ? cur_kv.at("$Parent") : string();

                for (auto& parent_name : strex(parent_name_line).split(' ')) {
                    auto parent_pid = engine->Hashes.ToHashedString(parent_name);
                    parent_pid = engine->CheckMigrationRule(proto_rule_name, type_name, parent_pid).value_or(parent_pid);

                    const auto it_parent = file_proto_pids.find(parent_pid);

                    if (it_parent == file_proto_pids.end()) {
                        if (base_name == name) {
                            throw ProtoBakerException("Proto fail to load parent", base_name, parent_name);
                        }

                        throw ProtoBakerException("Proto fail to load parent for another proto", base_name, parent_name, name);
                    }

                    fill_parent_recursive(parent_name, it_parent->second);
                    insert_map_values(it_parent->second, proto_kv);
                }
            };

            fill_parent_recursive(base_name, file_kv);

            // Actual content
            insert_map_values(file_kv, proto_kv);

            // Create proto
            const auto* property_registrator = engine->GetPropertyRegistrator(type_name);
            auto props = Properties(property_registrator);
            props.ApplyFromText(proto_kv);

            auto proto = SafeAlloc::MakeRefCounted<ProtoCustomEntity>(pid, property_registrator, &props);
            const auto inserted = all_protos[type_name].emplace(pid, proto).second;
            FO_RUNTIME_ASSERT(inserted);

            // Components
            for (auto&& [key, value] : proto_kv) {
                if (key.front() == '$') {
                    continue;
                }

                bool is_component;
                const auto* prop = property_registrator->FindProperty(key, &is_component);
                ignore_unused(prop);

                if (!is_component) {
                    continue;
                }

                if (value != "Enabled" && value != "Disabled") {
                    throw ProtoBakerException("Proto item has invalid component value (expected Enabled or Disabled)", base_name, key, value);
                }

                auto component_name = engine->Hashes.ToHashedString(key);
                component_name = engine->CheckMigrationRule(component_rule_name, type_name, component_name).value_or(component_name);

                if (!property_registrator->IsComponentRegistered(component_name)) {
                    throw ProtoBakerException("Proto item has invalid component", base_name, component_name);
                }

                if (value == "Enabled") {
                    proto->EnableComponent(component_name);
                }
            }
        }
    }

    // Validation
    size_t errors = 0;

    for (auto&& [type_name, protos] : all_protos) {
        for (auto& proto : protos | std::views::values) {
            errors += ValidateProperties(proto->GetProperties(), strex("proto {} {}", type_name, proto->GetName()), script_sys);
        }
    }

    if (errors != 0) {
        throw ProtoBakerException("Errors during proto validation");
    }

    // Binary representation
    vector<uint8> protos_data;
    set<hstring> str_hashes;

    {
        auto writer = DataWriter(protos_data);

        vector<uint8> props_data;

        writer.Write<uint32>(numeric_cast<uint32>(all_protos.size()));

        for (auto&& [type_name, protos] : all_protos) {
            writer.Write<uint32>(numeric_cast<uint32>(protos.size()));

            writer.Write<uint16>(numeric_cast<uint16>(type_name.as_str().length()));
            writer.WritePtr(type_name.as_str().data(), type_name.as_str().length());

            for (auto& proto : protos | std::views::values) {
                const auto proto_name = proto->GetName();
                writer.Write<uint16>(numeric_cast<uint16>(proto_name.length()));
                writer.WritePtr(proto_name.data(), proto_name.length());

                writer.Write<uint16>(numeric_cast<uint16>(proto->GetComponents().size()));

                for (const auto& component : proto->GetComponents()) {
                    const auto& component_str = component.as_str();
                    writer.Write<uint16>(numeric_cast<uint16>(component_str.length()));
                    writer.WritePtr(component_str.data(), component_str.length());
                }

                proto->GetProperties().StoreAllData(props_data, str_hashes);
                writer.Write<uint32>(numeric_cast<uint32>(props_data.size()));
                writer.WritePtr(props_data.data(), props_data.size());
            }
        }
    }

    vector<uint8> final_data;

    {
        auto final_writer = DataWriter(final_data);

        final_writer.Write<uint32>(numeric_cast<uint32>(str_hashes.size()));

        for (const auto& hstr : str_hashes) {
            const auto& str = hstr.as_str();
            final_writer.Write<uint32>(numeric_cast<uint32>(str.length()));
            final_writer.WritePtr(str.c_str(), str.length());
        }

        final_writer.WritePtr(protos_data.data(), protos_data.size());
    }

    return final_data;
}

FO_END_NAMESPACE();
