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
#include "Log.h"
#include "StringUtils.h"
#include "Testing.h"

// Todo: restore supporting of the map old text format

void MapLoader::Load(const string& name, FileManager& file_mngr, ProtoManager& proto_mngr, CrLoadFunc cr_load,
    ItemLoadFunc item_load, TileLoadFunc tile_load)
{
    // Find file
    FileCollection maps = file_mngr.FilterFiles("fomap");
    File map_file = maps.FindFile(name);
    if (!map_file)
        throw MapLoaderException("Map not found", name);

    // Load from file
    const char* buf = map_file.GetCStr();
    bool is_old_format = (strstr(buf, "[Header]") && strstr(buf, "[Tiles]") && strstr(buf, "[Objects]"));
    if (is_old_format)
        throw MapLoaderException("Unable to load map from old map format", name);

    // Header
    ConfigFile map_data(buf);
    if (!map_data.IsApp("ProtoMap"))
        throw MapLoaderException("Invalid map format", name);

    Properties props(ProtoMap::PropertiesRegistrator);
    props.LoadFromText(map_data.GetApp("ProtoMap"));

    ushort width = props.GetValue<ushort>(ProtoMap::PropertyWidth);
    ushort height = props.GetValue<ushort>(ProtoMap::PropertyHeight);

    // Critters
    PStrMapVec npc_data;
    map_data.GetApps("Critter", npc_data);
    StrVec errors {};
    for (auto& pkv : npc_data)
    {
        auto& kv = *pkv;
        if (!kv.count("$Id") || !kv.count("$Proto"))
        {
            errors.emplace_back("Proto critter invalid data");
            continue;
        }

        uint id = _str(kv["$Id"]).toUInt();
        hash proto_id = _str(kv["$Proto"]).toHash();
        ProtoCritter* proto = proto_mngr.GetProtoCritter(proto_id);
        if (!proto)
            errors.emplace_back(_str("Proto critter '{}' not found", _str().parseHash(proto_id)));
        else if (!cr_load(id, proto, kv))
            errors.emplace_back(_str("Unable to load critter '{}' properties", _str().parseHash(proto_id)));
    }

    // Items
    PStrMapVec items_data;
    map_data.GetApps("Item", items_data);
    for (auto& pkv : items_data)
    {
        auto& kv = *pkv;
        if (!kv.count("$Id") || !kv.count("$Proto"))
        {
            errors.emplace_back("Proto item invalid data");
            continue;
        }

        uint id = _str(kv["$Id"]).toUInt();
        hash proto_id = _str(kv["$Proto"]).toHash();
        ProtoItem* proto = proto_mngr.GetProtoItem(proto_id);
        if (!proto)
            errors.emplace_back(_str("Proto item '{}' not found", _str().parseHash(proto_id)));
        else if (!item_load(id, proto, kv))
            errors.emplace_back(_str("Unable to load item '{}' properties", _str().parseHash(proto_id)));
    }

    // Tiles
    PStrMapVec tiles_data;
    map_data.GetApps("Tile", tiles_data);
    for (auto& pkv : tiles_data)
    {
        auto& kv = *pkv;
        if (!kv.count("PicMap") || !kv.count("HexX") || !kv.count("HexY"))
        {
            errors.emplace_back("Tile invalid data");
            continue;
        }

        hash name = _str(kv["PicMap"]).toHash();
        ushort hx = _str(kv["HexX"]).toUInt();
        ushort hy = _str(kv["HexY"]).toUInt();
        short ox = (kv.count("OffsetX") ? _str(kv["OffsetX"]).toUInt() : 0);
        short oy = (kv.count("OffsetY") ? _str(kv["OffsetY"]).toUInt() : 0);
        uchar layer = (kv.count("Layer") ? _str(kv["Layer"]).toUInt() : 0);
        bool is_roof = (kv.count("IsRoof") ? _str(kv["IsRoof"]).toBool() : false);
        if (hx < 0 || hx >= width || hy < 0 || hy > height)
        {
            errors.emplace_back(_str("Tile '{}' have wrong hex position {} {}", _str().parseHash(name), hx, hy));
            continue;
        }
        if (layer < 0 || layer > 255)
        {
            errors.emplace_back(_str("Tile '{}' have wrong layer value {}", _str().parseHash(name), layer));
            continue;
        }

        tile_load({name, hx, hy, ox, oy, layer, is_roof});
    }

    if (!errors.empty())
        throw MapLoaderException("Map load error", errors); // Todo: pass errors vector to MapLoaderException
}
