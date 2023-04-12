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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "StringUtils.h"

// Todo: restore supporting of the map old text format

void MapLoader::Load(string_view name, const string& buf, ProtoManager& proto_mngr, NameResolver& name_resolver, const CrLoadFunc& cr_load, const ItemLoadFunc& item_load)
{
    STACK_TRACE_ENTRY();

    // Load from file
    const auto is_old_format = buf.find("[Header]") != string::npos && buf.find("[Tiles]") != string::npos && buf.find("[Objects]") != string::npos;
    if (is_old_format) {
        throw MapLoaderException("Unable to load map from old map format", name);
    }

    // Header
    ConfigFile map_data(_str("{}.fomap", name), buf, &name_resolver);
    if (!map_data.HasSection("ProtoMap")) {
        throw MapLoaderException("Invalid map format", name);
    }

    // Automatic id fixier
    unordered_set<ident_t::underlying_type> busy_ids;
    ident_t::underlying_type last_lowest_id = std::numeric_limits<ident_t::underlying_type>::max();
    const auto process_id = [&busy_ids, &last_lowest_id](ident_t::underlying_type id) -> ident_t {
        if (id < std::numeric_limits<ident_t::underlying_type>::max() / 2 || !busy_ids.emplace(id).second) {
            auto new_id = last_lowest_id - 1;
            while (!busy_ids.emplace(new_id).second) {
                new_id--;
            }
            last_lowest_id = new_id;
            return ident_t {new_id};
        }
        if (id < last_lowest_id) {
            last_lowest_id = id;
        }
        return ident_t {id};
    };

    // Critters
    vector<string> errors;
    for (const auto& pkv : map_data.GetSections("Critter")) {
        auto& kv = *pkv;
        if (kv.count("$Proto") == 0) {
            errors.emplace_back("Proto critter invalid data");
            continue;
        }

        const auto id = process_id(kv.count("$Id") != 0 ? _str(kv["$Id"]).toUInt() : 0);
        const auto& proto_name = kv["$Proto"];
        const auto* proto = proto_mngr.GetProtoCritter(name_resolver.ToHashedString(proto_name));
        if (proto == nullptr) {
            errors.emplace_back(_str("Proto critter '{}' not found", proto_name));
        }
        else if (!cr_load(id, proto, kv)) {
            errors.emplace_back(_str("Unable to load critter '{}' properties", proto_name));
        }
    }

    // Items
    for (const auto& pkv : map_data.GetSections("Item")) {
        auto& kv = *pkv;
        if (kv.count("$Proto") == 0) {
            errors.emplace_back("Proto item invalid data");
            continue;
        }

        const auto id = process_id(kv.count("$Id") != 0 ? _str(kv["$Id"]).toUInt() : 0);
        const auto& proto_name = kv["$Proto"];
        const auto* proto = proto_mngr.GetProtoItem(name_resolver.ToHashedString(proto_name));
        if (proto == nullptr) {
            errors.emplace_back(_str("Proto item '{}' not found", proto_name));
        }
        else if (!item_load(id, proto, kv)) {
            errors.emplace_back(_str("Unable to load item '{}' properties", proto_name));
        }
    }

    if (!errors.empty()) {
        string errors_cat;
        for (const auto& error : errors) {
            errors_cat += error + "\n";
        }
        throw MapLoaderException("Map load error", errors_cat);
    }
}
