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

#include "ProtoTextBaker.h"
#include "AnyData.h"
#include "ConfigFile.h"
#include "EngineBase.h"
#include "EntityProtos.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE

ProtoTextBaker::ProtoTextBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx))
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
    uint64_t max_write_time = 0;

    for (const auto& file_header : files) {
        const string ext = strex(file_header.GetPath()).get_file_extension();
        const auto it = std::ranges::find(_context->Settings->ProtoFileExtensions, ext);

        if (it == _context->Settings->ProtoFileExtensions.end()) {
            continue;
        }

        max_write_time = std::max(max_write_time, file_header.GetWriteTime());
        filtered_files.emplace_back(File::Load(file_header));
    }

    if (filtered_files.empty()) {
        return;
    }

    // Process files
    if (_context->BakeChecker) {
        bool check_result = false;

        for (const auto& lang_name : _context->Settings->BakeLanguages) {
            check_result |= _context->BakeChecker(strex("{}.Protos.{}.fotxt-bin", _context->PackName, lang_name), max_write_time);
            check_result |= _context->BakeChecker(strex("{}.Items.{}.fotxt-bin", _context->PackName, lang_name), max_write_time);
            check_result |= _context->BakeChecker(strex("{}.Critters.{}.fotxt-bin", _context->PackName, lang_name), max_write_time);
            check_result |= _context->BakeChecker(strex("{}.Maps.{}.fotxt-bin", _context->PackName, lang_name), max_write_time);
            check_result |= _context->BakeChecker(strex("{}.Locations.{}.fotxt-bin", _context->PackName, lang_name), max_write_time);
        }

        if (!check_result) {
            return;
        }
    }

    const auto engine = BakerServerEngine(*_context->BakedFiles);
    const auto proto_rule_name = engine.Hashes.ToHashedString("Proto");
    const auto item_type_name = engine.Hashes.ToHashedString("Item");
    const auto critter_type_name = engine.Hashes.ToHashedString("Critter");
    const auto map_type_name = engine.Hashes.ToHashedString("Map");
    const auto location_type_name = engine.Hashes.ToHashedString("Location");

    // Collect data
    unordered_map<hstring, unordered_map<hstring, map<string, string>>> all_file_protos;

    for (const auto& file : filtered_files) {
        const bool is_fomap = strex(file.GetPath()).get_file_extension() == "fomap";
        const auto fopro_options = is_fomap ? ConfigFileOption::ReadFirstSection : ConfigFileOption::None;
        auto fopro = ConfigFile(file.GetPath(), file.GetStr(), fopro_options);

        for (const auto& [section_name, section_kv_view] : fopro.GetSections()) {
            // Skip default section
            if (section_name.empty()) {
                continue;
            }

            hstring type_name;

            if (is_fomap && section_name == "Header") {
                type_name = engine.Hashes.ToHashedString("Map");
            }
            else if (strvex(section_name).starts_with("Proto") && section_name.length() > "Proto"_len) {
                type_name = engine.Hashes.ToHashedString(section_name.substr("Proto"_len));
            }
            else if (const auto section_type = engine.Hashes.ToHashedString(section_name); engine.IsFixedType(section_type)) {
                type_name = section_type;
            }
            else {
                throw ProtoTextBakerException("Invalid proto section name", section_name, file.GetPath());
            }

            if (engine.IsValidEntityType(type_name)) {
                if (!engine.GetEntityType(type_name).HasProtos) {
                    throw ProtoTextBakerException("Invalid proto type", section_name, file.GetPath());
                }
            }
            else if (engine.IsFixedType(type_name)) {
                if (!engine.GetFixedType(type_name).HasProtos) {
                    throw ProtoTextBakerException("Invalid proto type", section_name, file.GetPath());
                }
            }
            else {
                throw ProtoTextBakerException("Invalid proto type", section_name, file.GetPath());
            }

            map<string, string> section_kv;

            for (const auto& [key, value] : section_kv_view) {
                section_kv.emplace(string(key), string(value));
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

            if (strvex(key).starts_with("$Text")) {
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

            FO_RUNTIME_ASSERT(!_context->Settings->BakeLanguages.empty());
            const string& default_lang = _context->Settings->BakeLanguages.front();

            all_proto_texts[type_name][pid] = {};

            const auto text_pack_name = [&]() -> string_view {
                if (type_name == item_type_name) {
                    return "Items";
                }
                if (type_name == critter_type_name) {
                    return "Critters";
                }
                if (type_name == map_type_name) {
                    return "Maps";
                }
                if (type_name == location_type_name) {
                    return "Locations";
                }
                return "Protos";
            }();

            for (auto&& [key, value] : proto_kv) {
                FO_RUNTIME_ASSERT(strvex(key).starts_with("$Text"));

                const auto key_tok = strex(key).split(' ');
                const string lang = key_tok.size() >= 2 ? key_tok[1] : default_lang;

                FO_RUNTIME_ASSERT(key_tok.size() <= 4);

                const string_view key2 = key_tok.size() >= 3 ? string_view {key_tok[2]} : string_view {};
                const string_view key3 = key_tok.size() >= 4 ? string_view {key_tok[3]} : string_view {};
                const auto text_key = TextPackKey::FromPack(engine.Hashes, text_pack_name, pid.as_str(), key2, key3);

                auto& lang_packs_map = all_proto_texts[type_name][pid];
                lang_packs_map.try_emplace(lang, engine.Hashes);
                lang_packs_map.at(lang).AddStr(text_key, StringEscaping::DecodeString(value));
            }
        }
    }

    // Combine to single packs
    size_t errors = 0;
    vector<pair<string, map<string, TextPack>>> lang_packs;

    for (const auto& lang : _context->Settings->BakeLanguages) {
        auto empty_lang_pack = map<string, TextPack>();
        empty_lang_pack.try_emplace("Items", engine.Hashes);
        empty_lang_pack.try_emplace("Critters", engine.Hashes);
        empty_lang_pack.try_emplace("Maps", engine.Hashes);
        empty_lang_pack.try_emplace("Locations", engine.Hashes);
        empty_lang_pack.try_emplace("Protos", engine.Hashes);
        lang_packs.emplace_back(lang, std::move(empty_lang_pack));
    }

    const auto fill_proto_texts = [&](hstring entity_name, string_view pack_name) {
        for (auto&& [pid, proto_texts] : all_proto_texts[entity_name]) {
            for (const auto& proto_text : proto_texts) {
                const auto it = std::ranges::find_if(lang_packs, [&](auto&& l) { return l.first == proto_text.first; });

                if (it != lang_packs.end()) {
                    auto& text_pack = it->second.at(string(pack_name));

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

    fill_proto_texts(item_type_name, "Items");
    fill_proto_texts(critter_type_name, "Critters");
    fill_proto_texts(map_type_name, "Maps");
    fill_proto_texts(location_type_name, "Locations");

    for (auto&& [type_name, type_desc] : engine.GetEntityTypes()) {
        if (!type_desc.Exported && type_desc.HasProtos) {
            fill_proto_texts(type_name, "Protos");
        }
    }

    TextPack::FixPacks(_context->Settings->BakeLanguages, lang_packs);

    if (errors != 0) {
        throw ProtoTextBakerException("Errors during proto texts parsing");
    }

    // Save packs
    for (auto&& [lang_name, lang_pack] : lang_packs) {
        for (auto&& [pack_name, text_pack] : lang_pack) {
            _context->WriteData(strex("{}.{}.{}.fotxt-bin", _context->PackName, pack_name, lang_name), text_pack.GetBinaryData());
        }
    }
}

FO_END_NAMESPACE
