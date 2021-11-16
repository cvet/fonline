//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GenericUtils.h"
#include "StringUtils.h"

// Todo: restore supporting of the map old text format

void MapLoader::Load(string_view name, FileManager& file_mngr, ProtoManager& proto_mngr, const CrLoadFunc& cr_load, const ItemLoadFunc& item_load, const TileLoadFunc& tile_load)
{
    // Find file
    auto maps = file_mngr.FilterFiles("fomap");
    auto map_file = maps.FindFile(name);
    if (!map_file) {
        throw MapLoaderException("Map not found", name);
    }

    // Load from file
    const auto* buf = map_file.GetCStr();
    const auto is_old_format = strstr(buf, "[Header]") != nullptr && strstr(buf, "[Tiles]") != nullptr && strstr(buf, "[Objects]") != nullptr;
    if (is_old_format) {
        throw MapLoaderException("Unable to load map from old map format", name);
    }

    // Header
    ConfigFile map_data(buf);
    if (!map_data.IsApp("ProtoMap")) {
        throw MapLoaderException("Invalid map format", name);
    }

    Properties props(ProtoMap::PropertiesRegistrator);
    if (!props.LoadFromText(map_data.GetApp("ProtoMap"))) {
        throw MapLoaderException("Unable to load map properties", name);
    }

    auto width = props.GetValue<ushort>(ProtoMap::PropertyWidth);
    auto height = props.GetValue<ushort>(ProtoMap::PropertyHeight);

    // Critters
    vector<string> errors;
    for (auto& pkv : map_data.GetApps("Critter")) {
        auto& kv = *pkv;
        if (kv.count("$Id") == 0u || kv.count("$Proto") == 0u) {
            errors.emplace_back("Proto critter invalid data");
            continue;
        }

        auto id = _str(kv["$Id"]).toUInt();
        auto proto_id = _str(kv["$Proto"]).toHash();
        const auto* proto = proto_mngr.GetProtoCritter(proto_id);
        if (proto == nullptr) {
            errors.emplace_back(_str("Proto critter '{}' not found", _str().parseHash(proto_id)).strv());
        }
        else if (!cr_load(id, proto, kv)) {
            errors.emplace_back(_str("Unable to load critter '{}' properties", _str().parseHash(proto_id)).strv());
        }
    }

    // Items
    for (auto& pkv : map_data.GetApps("Item")) {
        auto& kv = *pkv;
        if (kv.count("$Id") == 0u || kv.count("$Proto") == 0u) {
            errors.emplace_back("Proto item invalid data");
            continue;
        }

        auto id = _str(kv["$Id"]).toUInt();
        auto proto_id = _str(kv["$Proto"]).toHash();
        const auto* proto = proto_mngr.GetProtoItem(proto_id);
        if (proto == nullptr) {
            errors.emplace_back(_str("Proto item '{}' not found", _str().parseHash(proto_id)).strv());
        }
        else if (!item_load(id, proto, kv)) {
            errors.emplace_back(_str("Unable to load item '{}' properties", _str().parseHash(proto_id)).strv());
        }
    }

    // Tiles
    for (auto& pkv : map_data.GetApps("Tile")) {
        auto& kv = *pkv;
        if (kv.count("PicMap") == 0u || kv.count("HexX") == 0u || kv.count("HexY") == 0u) {
            errors.emplace_back("Tile invalid data");
            continue;
        }

        const auto tname = _str(kv["PicMap"]).toHash();
        const auto hx = static_cast<int>(_str(kv["HexX"]).toUInt());
        const auto hy = static_cast<int>(_str(kv["HexY"]).toUInt());
        const auto ox = static_cast<int>(kv.count("OffsetX") != 0u ? _str(kv["OffsetX"]).toUInt() : 0);
        const auto oy = static_cast<int>(kv.count("OffsetY") != 0u ? _str(kv["OffsetY"]).toUInt() : 0);
        const auto layer = static_cast<int>(kv.count("Layer") != 0u ? _str(kv["Layer"]).toUInt() : 0);
        const auto is_roof = kv.count("IsRoof") != 0u ? _str(kv["IsRoof"]).toBool() : false;

        if (hx < 0 || hx >= width || hy < 0 || hy >= height) {
            errors.emplace_back(_str("Tile '{}' have wrong hex position {} {}", _str().parseHash(tname), hx, hy).strv());
            continue;
        }
        if (layer < 0 || layer > 255) {
            errors.emplace_back(_str("Tile '{}' have wrong layer value {}", _str().parseHash(tname), layer).strv());
            continue;
        }

        tile_load({tname, static_cast<ushort>(hx), static_cast<ushort>(hy), static_cast<short>(ox), static_cast<short>(oy), static_cast<uchar>(layer), is_roof});
    }

    if (!errors.empty()) {
        throw MapLoaderException("Map load error"); // Todo: pass errors vector to MapLoaderException
    }
}
