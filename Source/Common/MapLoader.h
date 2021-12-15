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

#pragma once

#include "Common.h"

#include "Entity.h"
#include "ProtoManager.h"

DECLARE_EXCEPTION(MapLoaderException);

struct MapTile
{
    hash Name {};
    ushort HexX {};
    ushort HexY {};
    short OffsX {};
    short OffsY {};
    uchar Layer {};
    bool IsRoof {};
    bool IsSelected {}; // Todo: remove mapper specific IsSelected from MapTile
};

class MapLoader final
{
public:
    using CrLoadFunc = std::function<bool(uint id, const ProtoCritter* proto, const map<string, string>& kv)>;
    using ItemLoadFunc = std::function<bool(uint id, const ProtoItem* proto, const map<string, string>& kv)>;
    using TileLoadFunc = std::function<void(MapTile&& tile)>;

    MapLoader() = delete;

    static void Load(string_view name, FileManager& file_mngr, ProtoManager& proto_mngr, const PropertyRegistrator* map_property_registrator, const CrLoadFunc& cr_load, const ItemLoadFunc& item_load, const TileLoadFunc& tile_load);
};
