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

#include "ProtoTextBaker.h"
#include "AnyData.h"
#include "ConfigFile.h"
#include "EngineBase.h"
#include "EntityProtos.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE();

ProtoTextBaker::ProtoTextBaker(BakerData& data) :
    BaseBaker(data)
{
    FO_STACK_TRACE_ENTRY();
}

ProtoTextBaker::~ProtoTextBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void ProtoTextBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    if (!target_path.empty() && !strex(target_path).get_file_extension().starts_with("fotxt-")) {
        return;
    }

    // Collect files
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

    // Process files
    if (_bakeChecker) {
        bool check_result = false;

        for (const auto& lang_name : _settings->BakeLanguages) {
            check_result |= _bakeChecker(strex("{}.Protos.{}.fotxt-bin", _resPackName, lang_name), max_write_time);
            check_result |= _bakeChecker(strex("{}.Items.{}.fotxt-bin", _resPackName, lang_name), max_write_time);
            check_result |= _bakeChecker(strex("{}.Critters.{}.fotxt-bin", _resPackName, lang_name), max_write_time);
            check_result |= _bakeChecker(strex("{}.Maps.{}.fotxt-bin", _resPackName, lang_name), max_write_time);
            check_result |= _bakeChecker(strex("{}.Locations.{}.fotxt-bin", _resPackName, lang_name), max_write_time);
        }

        if (!check_result) {
            return;
        }
    }

    const auto engine = BakerEngine(PropertiesRelationType::ServerRelative);
    const auto proto_rule_name = engine.Hashes.ToHashedString("Proto");

    // Collect data
    unordered_map<hstring, unordered_map<hstring, map<string, string>>> all_file_protos;

    for (const auto& file : filtered_files) {
        const bool is_fomap = strex(file.GetPath()).get_file_extension() == "fomap";
        const auto fopro_options = is_fomap ? ConfigFileOption::ReadFirstSection : ConfigFileOption::None;
        auto fopro = ConfigFile(file.GetPath(), file.GetStr(), &engine.Hashes, fopro_options);

        for (auto& section : fopro.GetSections()) {
            const auto& section_name = section.first;
            auto& section_kv = section.second;

            // Skip default section
            if (section_name.empty()) {
                continue;
            }

            hstring type_name;

            if (is_fomap && section_name == "Header") {
                type_name = engine.Hashes.ToHashedString("Map");
            }
            else if (strex(section_name).starts_with("Proto") && section_name.length() > "Proto"_len) {
                type_name = engine.Hashes.ToHashedString(section_name.substr("Proto"_len));
            }
            else {
                throw ProtoTextBakerException("Invalid proto section name", section_name, file.GetPath());
            }

            if (!engine.IsValidEntityType(type_name) || !engine.GetEntityTypeInfo(type_name).HasProtos) {
                throw ProtoTextBakerException("Invalid proto type", section_name, file.GetPath());
            }

            const auto name = section_kv.count("$Name") != 0 ? section_kv.at("$Name") : file.GetNameNoExt();
            auto pid = engine.Hashes.ToHashedString(name);
            pid = engine.CheckMigrationRule(proto_rule_name, type_name, pid).value_or(pid);

            auto& file_protos = all_file_protos[type_name];

            if (file_protos.count(pid) != 0) {
                throw ProtoTextBakerException("Proto already loaded", type_name, pid, file.GetPath());
            }

            file_protos.emplace(pid, section_kv);
        }
    }

    // Process data
    unordered_map<hstring, unordered_map<hstring, map<string, TextPack>>> all_proto_texts;

    const auto insert_map_values = [](const map<string, string>& from_kv, map<string, string>& to_kv) {
        for (auto&& [key, value] : from_kv) {
            FO_RUNTIME_ASSERT(!key.empty());

            if (strex(key).starts_with("$Text")) {
                to_kv[key] = value;
            }
        }
    };

    for (const auto& file_protos : all_file_protos) {
        const auto& type_name = file_protos.first;
        const auto& file_proto_pids = file_protos.second;

        for (auto&& [pid, file_kv] : file_proto_pids) {
            const auto base_name = pid.as_str();
            FO_RUNTIME_ASSERT(all_proto_texts[type_name].count(pid) == 0);

            map<string, string> proto_kv;

            function<void(string_view, const map<string, string>&)> fill_parent_recursive = [&](string_view name, const map<string, string>& cur_kv) {
                const auto parent_name_line = cur_kv.count("$Parent") != 0 ? cur_kv.at("$Parent") : string();

                for (auto& parent_name : strex(parent_name_line).split(' ')) {
                    auto parent_pid = engine.Hashes.ToHashedString(parent_name);
                    parent_pid = engine.CheckMigrationRule(proto_rule_name, type_name, parent_pid).value_or(parent_pid);

                    const auto it_parent = file_proto_pids.find(parent_pid);

                    if (it_parent == file_proto_pids.end()) {
                        if (base_name == name) {
                            throw ProtoTextBakerException("Proto fail to load parent", base_name, parent_name);
                        }

                        throw ProtoTextBakerException("Proto fail to load parent for another proto", base_name, parent_name, name);
                    }

                    fill_parent_recursive(parent_name, it_parent->second);
                    insert_map_values(it_parent->second, proto_kv);
                }
            };

            fill_parent_recursive(base_name, file_kv);
            insert_map_values(file_kv, proto_kv);

            FO_RUNTIME_ASSERT(!_settings->BakeLanguages.empty());
            const string& default_lang = _settings->BakeLanguages.front();

            all_proto_texts[type_name][pid] = {};

            for (auto&& [key, value] : proto_kv) {
                FO_RUNTIME_ASSERT(strex(key).starts_with("$Text"));

                const auto key_tok = strex(key).split(' ');
                const string lang = key_tok.size() >= 2 ? key_tok[1] : default_lang;

                TextPackKey text_key = pid.as_uint32();

                for (size_t i = 2; i < key_tok.size(); i++) {
                    const string& num = key_tok[i];

                    if (!num.empty()) {
                        if (strex(num).is_number()) {
                            text_key += strex(num).to_uint32();
                        }
                        else {
                            text_key += engine.Hashes.ToHashedString(num).as_uint32();
                        }
                    }
                }

                all_proto_texts[type_name][pid][lang].AddStr(text_key, StringEscaping::DecodeString(value));
            }
        }
    }

    // Combine to single packs
    size_t errors = 0;
    vector<pair<string, map<string, TextPack>>> lang_packs;

    for (const auto& lang : _settings->BakeLanguages) {
        auto empty_lang_pack = map<string, TextPack>();
        empty_lang_pack["Items"] = {};
        empty_lang_pack["Critters"] = {};
        empty_lang_pack["Maps"] = {};
        empty_lang_pack["Locations"] = {};
        empty_lang_pack["Protos"] = {};
        lang_packs.emplace_back(lang, std::move(empty_lang_pack));
    }

    const auto fill_proto_texts = [&](hstring type_name, TextPackName pack_name) {
        for (auto&& [pid, proto_texts] : all_proto_texts[type_name]) {
            for (const auto& proto_text : proto_texts) {
                const auto it = std::ranges::find_if(lang_packs, [&](auto&& l) { return l.first == proto_text.first; });

                if (it != lang_packs.end()) {
                    const auto& pack_name_str = engine.ResolveEnumValueName("TextPackName", static_cast<int32>(pack_name));
                    auto& text_pack = it->second[pack_name_str];

                    if (text_pack.CheckIntersections(proto_text.second)) {
                        WriteLog("Proto text intersection detected for proto {} and pack {}", pid, pack_name);
                        errors++;
                    }

                    text_pack.Merge(proto_text.second);
                }
                else {
                    WriteLog(LogType::Warning, "Unsupported language {} in proto {}", proto_text.first, pid);
                }
            }
        }
    };

    fill_proto_texts(engine.Hashes.ToHashedString("Item"), TextPackName::Items);
    fill_proto_texts(engine.Hashes.ToHashedString("Critter"), TextPackName::Critters);
    fill_proto_texts(engine.Hashes.ToHashedString("Map"), TextPackName::Maps);
    fill_proto_texts(engine.Hashes.ToHashedString("Location"), TextPackName::Locations);

    for (auto&& [type_name, entity_info] : engine.GetEntityTypesInfo()) {
        if (!entity_info.Exported && entity_info.HasProtos) {
            fill_proto_texts(type_name, TextPackName::Protos);
        }
    }

    TextPack::FixPacks(_settings->BakeLanguages, lang_packs);

    if (errors != 0) {
        throw ProtoTextBakerException("Errors during proto texts parsing");
    }

    // Save packs
    for (auto&& [lang_name, lang_pack] : lang_packs) {
        for (auto&& [pack_name, text_pack] : lang_pack) {
            _writeData(strex("{}.{}.{}.fotxt-bin", _resPackName, pack_name, lang_name), text_pack.GetBinaryData());
        }
    }
}

FO_END_NAMESPACE();
