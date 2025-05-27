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
#include "Application.h"
#include "ConfigFile.h"
#include "EngineBase.h"
#include "EntityProtos.h"
#include "Log.h"
#include "ScriptSystem.h"
#include "StringUtils.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE();

ProtoBaker::ProtoBaker(const BakerSettings& settings, string pack_name, BakeCheckerCallback bake_checker, AsyncWriteDataCallback write_data, const FileSystem* baked_files) :
    BaseBaker(settings, std::move(pack_name), std::move(bake_checker), std::move(write_data), baked_files)
{
    FO_STACK_TRACE_ENTRY();
}

ProtoBaker::~ProtoBaker()
{
    FO_STACK_TRACE_ENTRY();
}

auto ProtoBaker::IsExtSupported(string_view ext) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = std::find(_settings->ProtoFileExtensions.begin(), _settings->ProtoFileExtensions.end(), ext);

    if (it != _settings->ProtoFileExtensions.end()) {
        return true;
    }

    return false;
}

void ProtoBaker::BakeFiles(FileCollection files)
{
    FO_STACK_TRACE_ENTRY();

    vector<File> filtered_files;
    uint64 max_write_time = 0;

    while (files.MoveNext()) {
        auto file_header = files.GetCurFileHeader();
        const string ext = strex(file_header.GetPath()).getFileExtension();

        if (!IsExtSupported(ext)) {
            continue;
        }

        max_write_time = std::max(max_write_time, file_header.GetWriteTime());
        filtered_files.emplace_back(files.GetCurFile());
    }

    if (filtered_files.empty()) {
        return;
    }

    vector<std::future<void>> file_bakings;

    if (!_bakeChecker || _bakeChecker(_packName + ".foprob-server", max_write_time)) {
        if (_bakeChecker) {
            for (const auto& lang_name : _settings->BakeLanguages) {
                (void)_bakeChecker(strex("Protos.{}.fotxtb", lang_name), max_write_time);
            }
        }

        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] {
            auto engine = BakerEngine(PropertiesRelationType::ServerRelative);
            const auto script_sys = BakerScriptSystem(engine, *_bakedFiles);
            auto data = BakeProtoFiles(&engine, &script_sys, filtered_files, true);
            _writeData(_packName + ".foprob-server", data);
        }));

        if (_bakeChecker) {
            (void)_bakeChecker(_packName + ".foprob-client", max_write_time);
        }

        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] {
            const auto engine = BakerEngine(PropertiesRelationType::ClientRelative);
            auto data = BakeProtoFiles(&engine, nullptr, filtered_files, false);
            _writeData(_packName + ".foprob-client", data);
        }));

        if (_bakeChecker) {
            (void)_bakeChecker(_packName + ".foprob-mapper", max_write_time);
        }

        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] {
            const auto engine = BakerEngine(PropertiesRelationType::BothRelative);
            auto data = BakeProtoFiles(&engine, nullptr, filtered_files, false);
            _writeData(_packName + ".foprob-mapper", data);
        }));
    }

    int32 errors = 0;

    for (auto& file_baking : file_bakings) {
        try {
            file_baking.get();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    if (errors != 0) {
        throw ProtoBakerException("Errors during scripts compilation");
    }
}

auto ProtoBaker::BakeProtoFiles(const EngineData* engine, const ScriptSystem* script_sys, const vector<File>& files, bool write_texts) const -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    const auto proto_rule_name = engine->Hashes.ToHashedString("Proto");
    const auto component_rule_name = engine->Hashes.ToHashedString("Component");

    // Collect data
    unordered_map<hstring, unordered_map<hstring, map<string, string>>> all_file_protos;

    for (const auto& file : files) {
        const string ext = strex(file.GetPath()).getFileExtension();
        const bool is_fomap = string_view(ext) == "fomap";
        auto fopro_options = ConfigFileOption::None;

        if (is_fomap) {
            fopro_options = ConfigFileOption::ReadFirstSection;
        }

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
            else if (strex(section_name).startsWith("Proto") && section_name.length() > "Proto"_len) {
                type_name = engine->Hashes.ToHashedString(section_name.substr("Proto"_len));
            }
            else {
                throw ProtoBakerException("Invalid proto section name", section_name, file.GetName());
            }

            if (!engine->IsValidEntityType(type_name) || !engine->GetEntityTypeInfo(type_name).HasProtos) {
                throw ProtoBakerException("Invalid proto type", section_name, file.GetName());
            }

            const auto name = section_kv.count("$Name") != 0 ? section_kv.at("$Name") : file.GetName();
            auto pid = engine->Hashes.ToHashedString(name);
            pid = engine->CheckMigrationRule(proto_rule_name, type_name, pid).value_or(pid);

            auto& file_protos = all_file_protos[type_name];

            if (file_protos.count(pid) != 0) {
                throw ProtoBakerException("Proto already loaded", type_name, pid, file.GetName());
            }

            file_protos.emplace(pid, section_kv);
        }
    }

    // Processing
    unordered_map<hstring, unordered_map<hstring, refcount_ptr<ProtoEntity>>> all_protos;
    unordered_map<hstring, unordered_map<hstring, map<string, TextPack>>> all_proto_texts;

    const auto insert_map_values = [](const map<string, string>& from_kv, map<string, string>& to_kv) {
        for (auto&& [key, value] : from_kv) {
            FO_RUNTIME_ASSERT(!key.empty());

            if (key.front() != '$' || strex(key).startsWith("$Text")) {
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

            std::function<void(string_view, const map<string, string>&)> fill_parent_recursive = [&](string_view name, const map<string, string>& cur_kv) {
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

            // Texts
            if (write_texts) {
                FO_RUNTIME_ASSERT(!_settings->BakeLanguages.empty());
                const string& default_lang = _settings->BakeLanguages.front();

                all_proto_texts[type_name][pid] = {};

                for (auto&& [key, value] : proto_kv) {
                    if (strex(key).startsWith("$Text")) {
                        const auto key_tok = strex(key).split(' ');
                        const string lang = key_tok.size() >= 2 ? key_tok[1] : default_lang;

                        TextPackKey text_key = pid.as_uint();

                        for (size_t i = 2; i < key_tok.size(); i++) {
                            const string& num = key_tok[i];

                            if (!num.empty()) {
                                if (strex(num).isNumber()) {
                                    text_key += strex(num).toUInt();
                                }
                                else {
                                    text_key += engine->Hashes.ToHashedString(num).as_uint();
                                }
                            }
                        }

                        all_proto_texts[type_name][pid][lang].AddStr(text_key, value);
                    }
                }
            }
        }
    }

    // Validation
    int32 errors = 0;

    for (auto&& [type_name, protos] : all_protos) {
        for (auto&& [pid, proto] : protos) {
            errors += ValidateProperties(proto->GetProperties(), strex("proto {} {}", type_name, proto->GetName()), script_sys);
        }
    }

    // Texts
    if (write_texts) {
        vector<pair<string, map<string, TextPack>>> lang_packs;

        for (const auto& lang : _settings->BakeLanguages) {
            lang_packs.emplace_back(lang, map<string, TextPack>());
        }

        const auto fill_proto_texts = [&](const auto& protos, TextPackName pack_name) {
            for (auto&& [pid, proto] : protos) {
                for (const auto& proto_text : all_proto_texts.at(proto->GetTypeName()).at(pid)) {
                    const auto it = std::find_if(lang_packs.begin(), lang_packs.end(), [&](auto&& l) { return l.first == proto_text.first; });

                    if (it != lang_packs.end()) {
                        const auto& pack_name_str = engine->ResolveEnumValueName("TextPackName", static_cast<int32>(pack_name));
                        auto& text_pack = it->second[pack_name_str];

                        if (text_pack.CheckIntersections(proto_text.second)) {
                            WriteLog("Proto text intersection detected for proto {} and pack {}", proto->GetName(), pack_name);
                            errors++;
                        }

                        text_pack.Merge(proto_text.second);
                    }
                    else {
                        WriteLog(LogType::Warning, "Unsupported language {} in proto {}", proto_text.first, proto->GetName());
                    }
                }
            }
        };

        fill_proto_texts(all_protos[engine->Hashes.ToHashedString("Critter")], TextPackName::Dialogs);
        fill_proto_texts(all_protos[engine->Hashes.ToHashedString("Item")], TextPackName::Items);
        fill_proto_texts(all_protos[engine->Hashes.ToHashedString("Map")], TextPackName::Maps);
        fill_proto_texts(all_protos[engine->Hashes.ToHashedString("Location")], TextPackName::Locations);

        for (auto&& [type_name, entity_info] : engine->GetEntityTypesInfo()) {
            if (!entity_info.Exported && entity_info.HasProtos) {
                fill_proto_texts(all_protos[type_name], TextPackName::Protos);
            }
        }

        TextPack::FixPacks(_settings->BakeLanguages, lang_packs);
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

            for (auto&& [pid, proto] : protos) {
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
