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

#include "MapLoader.h"
#include "ConfigFile.h"
#include "EngineBase.h"

FO_BEGIN_NAMESPACE

static constexpr string_view MAP_ANCHOR_SECTION = "ProtoMap";
static constexpr string_view CONTEXT_PREFIX = "$Name";

void MapLoader::Load(string_view name, string_view file_name, const string& buf, const EngineMetadata& meta, HashResolver& hash_resolver, const CrLoadFunc& cr_load, const ItemLoadFunc& item_load)
{
    FO_STACK_TRACE_ENTRY();

    // Load from file
    ConfigFile map_data(buf);

    // Walk the file in order: a [ProtoMap] anchor declares a map (named by its $Name, or by the
    // file when it carries none) and owns the nested sections that follow it. A nested prefix is
    // either the CONTEXT_PREFIX token, meaning the anchor above, or an explicit map name
    string file_stem = strex(file_name).extract_file_name().erase_file_extension().str();

    struct NestedMapSection
    {
        string_view Owner {};
        string_view Type {};
        ptr<map<string_view, string_view>> KeyValues;
    };

    vector<string_view> anchor_names;
    vector<NestedMapSection> nested_sections;
    string_view cur_anchor_name;
    bool has_anchor = false;

    for (const auto& [section_name, section_kv] : map_data.GetOrderedSections()) {
        if (section_name.empty()) {
            continue;
        }

        size_t slash_pos = section_name.find('/');

        if (slash_pos == string_view::npos) {
            if (section_name != MAP_ANCHOR_SECTION) {
                throw MapLoaderException("Invalid map file section, expected a ProtoMap anchor or nested map content", section_name, name, file_name);
            }

            auto anchor_name_it = section_kv->find("$Name");
            cur_anchor_name = anchor_name_it != section_kv->end() ? anchor_name_it->second : string_view {file_stem};
            has_anchor = true;
            anchor_names.emplace_back(cur_anchor_name);
            continue;
        }

        string_view prefix = section_name.substr(0, slash_pos);
        string_view nested_type = section_name.substr(slash_pos + 1);

        if (nested_type != "Critter" && nested_type != "Item") {
            throw MapLoaderException("Unknown nested map section type", section_name, name, file_name);
        }

        if (prefix == CONTEXT_PREFIX) {
            if (!has_anchor) {
                throw MapLoaderException("Nested map section appears before any ProtoMap anchor", section_name, name, file_name);
            }

            nested_sections.emplace_back(NestedMapSection {cur_anchor_name, nested_type, section_kv});
        }
        else {
            nested_sections.emplace_back(NestedMapSection {prefix, nested_type, section_kv});
        }
    }

    if (!has_anchor) {
        throw MapLoaderException("Invalid map format, no ProtoMap anchor declared", name, file_name);
    }
    if (std::ranges::find(anchor_names, name) == anchor_names.end()) {
        throw MapLoaderException("Map is not declared in the file", name, file_name);
    }

    for (const auto& nested_section : nested_sections) {
        if (std::ranges::find(anchor_names, nested_section.Owner) == anchor_names.end()) {
            throw MapLoaderException("Nested map section is addressed to an undeclared map", nested_section.Owner, name, file_name);
        }
    }

    auto collect_map_sections = [&](string_view nested_type) -> vector<ptr<map<string_view, string_view>>> {
        vector<ptr<map<string_view, string_view>>> sections;

        for (const auto& nested_section : nested_sections) {
            if (nested_section.Owner == name && nested_section.Type == nested_type) {
                sections.emplace_back(nested_section.KeyValues);
            }
        }

        return sections;
    };

    size_t errors = 0;

    // Automatic id fixier
    unordered_set<ident_t::underlying_type> busy_ids;
    ident_t::underlying_type last_lowest_id = 1;

    auto process_id = [&busy_ids, &last_lowest_id](ident_t::underlying_type id) -> ident_t {
        if (id <= 0 || !busy_ids.emplace(id).second) {
            auto new_id = last_lowest_id;

            while (!busy_ids.emplace(new_id).second) {
                new_id++;
            }

            last_lowest_id = new_id;
            return ident_t {new_id};
        }

        last_lowest_id = std::max(id, last_lowest_id);
        return ident_t {id};
    };

    // Critters
    for (const auto& pkv : collect_map_sections("Critter")) {
        auto kv = pkv;
        auto proto_it = kv->find("$Proto");

        if (proto_it == kv->end()) {
            WriteLog(LogType::Warning, "Proto critter invalid data");
            errors++;
            continue;
        }

        auto id_it = kv->find("$Id");
        ident_t id = process_id(id_it != kv->end() ? strex(id_it->second).to_int64() : 0);
        const auto& proto_name = proto_it->second;
        hstring hashed_proto_name = hash_resolver.ToHashedString(proto_name);
        auto proto = meta.GetProtoCritter(hashed_proto_name);

        if (!proto) {
            WriteLog(LogType::Warning, "Proto critter '{}' not found", proto_name);
            errors++;
        }
        else {
            try {
                cr_load(id, proto, kv);
            }
            catch (const std::exception& ex) {
                WriteLog(LogType::Warning, "Unable to load critter '{}'", proto_name);
                ReportExceptionAndContinue(ex);
                errors++;
            }
        }
    }

    // Items
    for (const auto& pkv : collect_map_sections("Item")) {
        auto kv = pkv;
        auto proto_it = kv->find("$Proto");

        if (proto_it == kv->end()) {
            WriteLog(LogType::Warning, "Proto item invalid data");
            errors++;
            continue;
        }

        auto id_it = kv->find("$Id");
        ident_t id = process_id(id_it != kv->end() ? strex(id_it->second).to_int64() : 0);
        const auto& proto_name = proto_it->second;
        hstring hashed_proto_name = hash_resolver.ToHashedString(proto_name);
        auto proto = meta.GetProtoItem(hashed_proto_name);

        if (!proto) {
            WriteLog(LogType::Warning, "Proto item '{}' not found", proto_name);
            errors++;
        }
        else {
            try {
                item_load(id, proto, kv);
            }
            catch (const std::exception& ex) {
                WriteLog(LogType::Warning, "Unable to load item '{}'", proto_name);
                ReportExceptionAndContinue(ex);
                errors++;
            }
        }
    }

    if (errors != 0) {
        throw MapLoaderException("Map load error", name);
    }
}

// Enumerates the maps a file declares; an empty result means the file is not a map container.
// This doubles as the map-file detector: map files are recognized by their [ProtoMap] anchors,
// not by a dedicated extension.
auto MapLoader::EnumerateMaps(string_view file_name, const string& buf) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    auto map_data = ConfigFile(buf, ConfigFileOption::SkipNestedSections);
    auto anchor_sections = map_data.GetSections(MAP_ANCHOR_SECTION);

    vector<string> map_names;
    map_names.reserve(anchor_sections.size());

    for (const auto& anchor_kv : anchor_sections) {
        auto anchor_name_it = anchor_kv->find("$Name");
        string map_name = anchor_name_it != anchor_kv->end() ? string(anchor_name_it->second) : strex(file_name).extract_file_name().erase_file_extension().str();

        // Several anchors without $Name all resolve to the file stem; enumerating the id once is
        // enough — the duplicate itself is reported by the generic proto collision check on bake
        if (std::ranges::find(map_names, map_name) == map_names.end()) {
            map_names.emplace_back(std::move(map_name));
        }
    }

    return map_names;
}

FO_END_NAMESPACE
