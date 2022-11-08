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

void MapLoader::Load(string_view name, const string& buf, ProtoManager& proto_mngr, NameResolver& name_resolver, const CrLoadFunc& cr_load, const ItemLoadFunc& item_load, const TileLoadFunc& tile_load)
{
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
    unordered_set<uint> busy_ids;
    const auto process_id = [&busy_ids](uint id) {
        if (!busy_ids.insert(id).second) {
            uint new_id = std::numeric_limits<uint>::max();
            while (!busy_ids.insert(new_id).second) {
                new_id--;
            }
            return new_id;
        }
        return id;
    };

    // Critters
    vector<string> errors;
    for (const auto& pkv : map_data.GetSections("Critter")) {
        auto& kv = *pkv;
        if (kv.count("$Id") == 0u || kv.count("$Proto") == 0u) {
            errors.emplace_back("Proto critter invalid data");
            continue;
        }

        const auto id = process_id(_str(kv["$Id"]).toUInt());
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
        if (kv.count("$Id") == 0u || kv.count("$Proto") == 0u) {
            errors.emplace_back("Proto item invalid data");
            continue;
        }

        const auto id = process_id(_str(kv["$Id"]).toUInt());
        const auto& proto_name = kv["$Proto"];
        const auto* proto = proto_mngr.GetProtoItem(name_resolver.ToHashedString(proto_name));
        if (proto == nullptr) {
            errors.emplace_back(_str("Proto item '{}' not found", proto_name));
        }
        else if (!item_load(id, proto, kv)) {
            errors.emplace_back(_str("Unable to load item '{}' properties", proto_name));
        }
    }

    // Tiles
    for (const auto& pkv : map_data.GetSections("Tile")) {
        auto& kv = *pkv;
        if (kv.count("PicMap") == 0u || kv.count("HexX") == 0u || kv.count("HexY") == 0u) {
            errors.emplace_back("Tile invalid data");
            continue;
        }

        const auto tname = kv["PicMap"];
        const auto hx = static_cast<int>(_str(kv["HexX"]).toUInt());
        const auto hy = static_cast<int>(_str(kv["HexY"]).toUInt());
        const auto ox = static_cast<int>(kv.count("OffsetX") != 0u ? _str(kv["OffsetX"]).toUInt() : 0);
        const auto oy = static_cast<int>(kv.count("OffsetY") != 0u ? _str(kv["OffsetY"]).toUInt() : 0);
        const auto layer = static_cast<int>(kv.count("Layer") != 0u ? _str(kv["Layer"]).toUInt() : 0);
        const auto is_roof = kv.count("IsRoof") != 0u ? _str(kv["IsRoof"]).toBool() : false;

        // if (hx < 0 || hx >= width || hy < 0 || hy >= height) {
        //     errors.emplace_back(_str("Tile '{}' have wrong hex position {} {}", tname, hx, hy));
        //     continue;
        // }
        if (layer < 0 || layer > std::numeric_limits<uchar>::max()) {
            errors.emplace_back(_str("Tile '{}' have wrong layer value {}", tname, layer));
            continue;
        }

        const auto htname = name_resolver.ToHashedString(tname);

        if (!tile_load({htname.as_hash(), static_cast<ushort>(hx), static_cast<ushort>(hy), static_cast<short>(ox), static_cast<short>(oy), static_cast<uchar>(layer), is_roof})) {
            errors.emplace_back("Unable to load tile");
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
