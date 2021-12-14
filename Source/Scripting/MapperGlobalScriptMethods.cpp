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

#include "Common.h"

#include "FileSystem.h"
#include "GenericUtils.h"
#include "GeometryHelper.h"
#include "Log.h"
#include "Mapper.h"
#include "MapperScripting.h"
#include "StringUtils.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param hx1 ...
///# param hy1 ...
///# param hx2 ...
///# param hy2 ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Mapper_Global_GetHexDistance(FOMapper* mapper, ushort hx1, ushort hy1, ushort hx2, ushort hy2)
{
    return mapper->GeomHelper.DistGame(hx1, hy1, hx2, hy2);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uchar Mapper_Global_GetHexDir(FOMapper* mapper, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy)
{
    return mapper->GeomHelper.GetFarDir(fromHx, fromHy, toHx, toHy);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param offset ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uchar Mapper_Global_GetHexDirWithOffset(FOMapper* mapper, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, float offset)
{
    return mapper->GeomHelper.GetFarDir(fromHx, fromHy, toHx, toHy, offset);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_GetTick(FOMapper* mapper)
{
    return mapper->GameTime.FrameTick();
}

///# ...
///# param pid ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Mapper_Global_AddItem(FOMapper* mapper, hash pid, ushort hx, ushort hy)
{
    if (hx >= mapper->HexMngr.GetWidth() || hy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    const auto* proto = mapper->ProtoMngr.GetProtoItem(pid);
    if (!proto) {
        throw ScriptException("Invalid item prototype");
    }

    return mapper->AddItem(pid, hx, hy, nullptr);
}

///# ...
///# param pid ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Mapper_Global_AddCritter(FOMapper* mapper, hash pid, ushort hx, ushort hy)
{
    if (hx >= mapper->HexMngr.GetWidth() || hy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    const auto* proto = mapper->ProtoMngr.GetProtoCritter(pid);
    if (!proto) {
        throw ScriptException("Invalid critter prototype");
    }

    return mapper->AddCritter(pid, hx, hy);
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Mapper_Global_GetItemByHex(FOMapper* mapper, ushort hx, ushort hy)
{
    return mapper->HexMngr.GetItem(hx, hy, 0);
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Mapper_Global_GetItemsByHex(FOMapper* mapper, ushort hx, ushort hy)
{
    vector<ItemHexView*> hex_items;
    mapper->HexMngr.GetItems(hx, hy, hex_items);

    vector<ItemView*> items;
    items.reserve(hex_items.size());
    for (auto* item : hex_items) {
        items.push_back(item);
    }

    return items;
}

///# ...
///# param hx ...
///# param hy ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Mapper_Global_GetCritterByHex(FOMapper* mapper, ushort hx, ushort hy, int findType)
{
    vector<CritterView*> critters_;
    mapper->HexMngr.GetCritters(hx, hy, critters_, findType);
    return !critters_.empty() ? critters_[0] : nullptr;
}

///# ...
///# param hx ...
///# param hy ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<CritterView*> Mapper_Global_GetCrittersByHex(FOMapper* mapper, ushort hx, ushort hy, int findType)
{
    vector<CritterView*> critters;
    mapper->HexMngr.GetCritters(hx, hy, critters, findType);
    return critters;
}

///# ...
///# param entity ...
///# param hx ...
///# param hy ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_MoveEntity(FOMapper* mapper, ClientEntity* entity, ushort hx, ushort hy)
{
    auto hx_ = hx;
    auto hy_ = hy;

    if (hx_ >= mapper->HexMngr.GetWidth()) {
        hx_ = mapper->HexMngr.GetWidth() - 1;
    }
    if (hy_ >= mapper->HexMngr.GetHeight()) {
        hy_ = mapper->HexMngr.GetHeight() - 1;
    }

    mapper->MoveEntity(entity, hx_, hy_);
}

///# ...
///# param entity ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DeleteEntity(FOMapper* mapper, ClientEntity* entity)
{
    mapper->DeleteEntity(entity);
}

///# ...
///# param entities ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DeleteEntities(FOMapper* mapper, const vector<ClientEntity*>& entities)
{
    for (auto* entity : entities) {
        if (entity != nullptr && !entity->IsDestroyed) {
            mapper->DeleteEntity(entity);
        }
    }
}

///# ...
///# param entity ...
///# param set ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_SelectEntity(FOMapper* mapper, ClientEntity* entity, bool set)
{
    if (entity == nullptr) {
        throw ScriptException("Entity arg is null");
    }

    if (set) {
        mapper->SelectAdd(entity);
    }
    else {
        mapper->SelectErase(entity);
    }
}

///# ...
///# param entities ...
///# param set ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_SelectEntities(FOMapper* mapper, const vector<ClientEntity*>& entities, bool set)
{
    for (auto* entity : entities) {
        if (entity != nullptr) {
            if (set) {
                mapper->SelectAdd(entity);
            }
            else {
                mapper->SelectErase(entity);
            }
        }
    }
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ClientEntity* Mapper_Global_GetSelectedEntity(FOMapper* mapper)
{
    return !mapper->SelectedEntities.empty() ? mapper->SelectedEntities[0] : nullptr;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ClientEntity*> Mapper_Global_GetSelectedEntities(FOMapper* mapper)
{
    vector<ClientEntity*> entities;
    entities.reserve(mapper->SelectedEntities.size());

    for (auto* entity : mapper->SelectedEntities) {
        entities.push_back(entity);
    }

    return entities;
}

///# ...
///# param hx ...
///# param hy ...
///# param roof ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_GetTilesCount(FOMapper* mapper, ushort hx, ushort hy, bool roof)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    const auto& tiles = mapper->HexMngr.GetTiles(hx, hy, roof);
    return static_cast<uint>(tiles.size());
}

///# ...
///# param hx ...
///# param hy ...
///# param roof ...
///# param layer ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DeleteTile(FOMapper* mapper, ushort hx, ushort hy, bool roof, int layer)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    auto deleted = false;
    auto& tiles = mapper->HexMngr.GetTiles(hx, hy, roof);
    auto& f = mapper->HexMngr.GetField(hx, hy);
    if (layer < 0) {
        deleted = !tiles.empty();
        tiles.clear();
        while (f.GetTilesCount(roof)) {
            f.EraseTile(0, roof);
        }
    }
    else {
        for (size_t i = 0, j = tiles.size(); i < j; i++) {
            if (tiles[i].Layer == layer) {
                tiles.erase(tiles.begin() + i);
                f.EraseTile(static_cast<uint>(i), roof);
                deleted = true;
                break;
            }
        }
    }

    if (deleted) {
        if (roof) {
            mapper->HexMngr.RebuildRoof();
        }
        else {
            mapper->HexMngr.RebuildTiles();
        }
    }
}

///# ...
///# param hx ...
///# param hy ...
///# param roof ...
///# param layer ...
///# return ...
///@ ExportMethod
[[maybe_unused]] hash Mapper_Global_GetTileHash(FOMapper* mapper, ushort hx, ushort hy, bool roof, int layer)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    const auto& tiles = mapper->HexMngr.GetTiles(hx, hy, roof);
    for (const auto i : xrange(tiles)) {
        if (tiles[i].Layer == layer) {
            return tiles[i].Name;
        }
    }

    return 0;
}

///# ...
///# param hx ...
///# param hy ...
///# param ox ...
///# param oy ...
///# param layer ...
///# param roof ...
///# param picHash ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_AddTileHash(FOMapper* mapper, ushort hx, ushort hy, int ox, int oy, int layer, bool roof, hash picHash)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    if (picHash == 0u) {
        return;
    }

    auto layer_ = layer;
    layer_ = std::clamp(layer_, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END);

    mapper->HexMngr.SetTile(picHash, hx, hy, static_cast<short>(ox), static_cast<short>(oy), static_cast<uchar>(layer_), roof, false);
}

///# ...
///# param hx ...
///# param hy ...
///# param roof ...
///# param layer ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Mapper_Global_GetTileName(FOMapper* mapper, ushort hx, ushort hy, bool roof, int layer)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    const auto& tiles = mapper->HexMngr.GetTiles(hx, hy, roof);
    for (const auto i : xrange(tiles)) {
        if (tiles[i].Layer == layer) {
            return _str().parseHash(tiles[i].Name).str();
        }
    }

    return string("");
}

///# ...
///# param hx ...
///# param hy ...
///# param ox ...
///# param oy ...
///# param layer ...
///# param roof ...
///# param picName ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_AddTileName(FOMapper* mapper, ushort hx, ushort hy, int ox, int oy, int layer, bool roof, string_view picName)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (hx >= mapper->HexMngr.GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    if (picName.empty()) {
        return;
    }

    auto layer_ = layer;
    layer_ = std::clamp(layer_, DRAW_ORDER_TILE, DRAW_ORDER_TILE_END);

    const auto pic_hash = _str(picName).toHash();
    mapper->HexMngr.SetTile(pic_hash, hx, hy, static_cast<short>(ox), static_cast<short>(oy), static_cast<uchar>(layer_), roof, false);
}

///# ...
///# param fileName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] MapView* Mapper_Global_LoadMap(FOMapper* mapper, string_view fileName)
{
    auto* pmap = new ProtoMap(_str(fileName).toHash());
    // Todo: need attention!
    // if (!pmap->EditorLoad(mapper->ServerFileMngr, mapper->ProtoMngr, mapper->SprMngr, mapper->ResMngr))
    //     return nullptr;

    // auto* map = new MapView(0, pmap);
    // mapper->LoadedMaps.push_back(map);
    // mapper->RunMapLoadScript(map);

    // return map;
    return nullptr;
}

///# ...
///# param map ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_UnloadMap(FOMapper* mapper, MapView* map)
{
    if (map == nullptr) {
        throw ScriptException("Proto map arg nullptr");
    }

    const auto it = std::find(mapper->LoadedMaps.begin(), mapper->LoadedMaps.end(), map);
    if (it != mapper->LoadedMaps.end()) {
        mapper->LoadedMaps.erase(it);
    }

    if (map == mapper->ActiveMap) {
        mapper->HexMngr.UnloadMap();
        mapper->SelectedEntities.clear();
        mapper->ActiveMap = nullptr;
    }

    map->Proto->Release();
    map->Release();
}

///# ...
///# param map ...
///# param customName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Mapper_Global_SaveMap(FOMapper* mapper, MapView* map, string_view customName)
{
    if (map == nullptr) {
        throw ScriptException("Proto map arg nullptr");
    }

    // Todo: need attention!
    //((ProtoMap*)map->Proto)->EditorSave(mapper->ServerFileMngr, customName);
    mapper->RunMapSaveScript(map);
    return true;
}

///# ...
///# param map ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Mapper_Global_ShowMap(FOMapper* mapper, MapView* map)
{
    if (map == nullptr) {
        throw ScriptException("Proto map arg nullptr");
    }

    if (mapper->ActiveMap == map) {
        return true;
    }

    mapper->SelectClear();
    if (!mapper->HexMngr.SetProtoMap(*(ProtoMap*)map->Proto)) {
        return false;
    }

    mapper->HexMngr.FindSetCenter(map->GetWorkHexX(), map->GetWorkHexY());
    mapper->ActiveMap = map;

    return true;
}

///# ...
///# param index ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<MapView*> Mapper_Global_GetLoadedMaps(FOMapper* mapper, int& index)
{
    index = -1;
    for (auto i = 0, j = static_cast<int>(mapper->LoadedMaps.size()); i < j; i++) {
        const auto* map = mapper->LoadedMaps[i];
        if (map == mapper->ActiveMap) {
            index = i;
        }
    }
    return mapper->LoadedMaps;
}

///# ...
///# param dir ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<string> Mapper_Global_GetMapFileNames(FOMapper* mapper, string_view dir)
{
    vector<string> names;

    auto map_files = mapper->ServerFileMngr.FilterFiles("fomap", dir, false);
    while (map_files.MoveNext()) {
        auto file_header = map_files.GetCurFileHeader();
        names.emplace_back(file_header.GetName());
    }

    return names;
}

///# ...
///# param width ...
///# param height ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_ResizeMap(FOMapper* mapper, ushort width, ushort height)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }

    RUNTIME_ASSERT(mapper->ActiveMap);
    auto* pmap = (ProtoMap*)mapper->ActiveMap->Proto;

    // Unload current
    mapper->HexMngr.GetProtoMap(*pmap);
    mapper->SelectClear();
    mapper->HexMngr.UnloadMap();

    // Check size
    auto maxhx = std::clamp(width, MAXHEX_MIN, MAXHEX_MAX);
    auto maxhy = std::clamp(height, MAXHEX_MIN, MAXHEX_MAX);
    const auto old_maxhx = pmap->GetWidth();
    const auto old_maxhy = pmap->GetHeight();

    maxhx = std::clamp(maxhx, MAXHEX_MIN, MAXHEX_MAX);
    maxhy = std::clamp(maxhy, MAXHEX_MIN, MAXHEX_MAX);

    if (pmap->GetWorkHexX() >= maxhx) {
        pmap->SetWorkHexX_ReadOnlyWorkaround(maxhx - 1);
    }
    if (pmap->GetWorkHexY() >= maxhy) {
        pmap->SetWorkHexY_ReadOnlyWorkaround(maxhy - 1);
    }

    pmap->SetWidth_ReadOnlyWorkaround(maxhx);
    pmap->SetHeight_ReadOnlyWorkaround(maxhy);

    // Delete truncated entities
    if (maxhx < old_maxhx || maxhy < old_maxhy) {
        // Todo: need attention!
        /*for (auto it = pmap->AllEntities.begin(); it != pmap->AllEntities.end();)
        {
            ClientEntity* entity = *it;
            int hx = (entity->Type == EntityType::CritterView ? ((CritterView*)entity)->GetHexX() :
                                                                ((ItemHexView*)entity)->GetHexX());
            int hy = (entity->Type == EntityType::CritterView ? ((CritterView*)entity)->GetHexY() :
                                                                ((ItemHexView*)entity)->GetHexY());
            if (hx >= maxhx || hy >= maxhy)
            {
                entity->Release();
                it = pmap->AllEntities.erase(it);
            }
            else
            {
                ++it;
            }
        }*/
    }

    // Delete truncated tiles
    if (maxhx < old_maxhx || maxhy < old_maxhy) {
        // Todo: need attention!
        /*for (auto it = pmap->Tiles.begin(); it != pmap->Tiles.end();)
        {
            MapTile& tile = *it;
            if (tile.HexX >= maxhx || tile.HexY >= maxhy)
                it = pmap->Tiles.erase(it);
            else
                ++it;
        }*/
    }

    // Update visibility
    mapper->HexMngr.SetProtoMap(*pmap);
    mapper->HexMngr.FindSetCenter(pmap->GetWorkHexX(), pmap->GetWorkHexY());
}

///# ...
///# param tab ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<string> Mapper_Global_TabGetTileDirs(FOMapper* mapper, int tab)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    auto& ttab = mapper->TabsTiles[tab];
    // return ttab.TileSubDirs;
    return vector<string>();
}

///# ...
///# param tab ...
///# param subTab ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<hash> Mapper_Global_TabGetItemPids(FOMapper* mapper, int tab, string_view subTab)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && !mapper->Tabs[tab].count(string(subTab))) {
        return vector<hash>();
    }

    auto& stab = mapper->Tabs[tab][!subTab.empty() ? string(subTab) : FOMapper::DEFAULT_SUB_TAB];
    // return stab.ItemProtos;
    return vector<hash>();
}

///# ...
///# param tab ...
///# param subTab ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<hash> Mapper_Global_TabGetCritterPids(FOMapper* mapper, int tab, string_view subTab)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && !mapper->Tabs[tab].count(string(subTab))) {
        return vector<hash>();
    }

    auto& stab = mapper->Tabs[tab][!subTab.empty() ? string(subTab) : FOMapper::DEFAULT_SUB_TAB];
    // return stab.NpcProtos;
    return vector<hash>();
}

///# ...
///# param tab ...
///# param dirNames ...
///# param includeSubdirs ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_TabSetTileDirs(FOMapper* mapper, int tab, const vector<string>& dirNames, const vector<bool>& includeSubdirs)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    /*if (dirNames && includeSubdirs && dirNames->GetSize() != includeSubdirs->GetSize())
        return;

    TileTab& ttab = mapper->TabsTiles[tab];
    ttab.TileDirs.clear();
    ttab.TileSubDirs.clear();

    if (dirNames)
    {
        for (uint i = 0, j = dirNames->GetSize(); i < j; i++)
        {
            string& name = *(string*)dirNames->At(i);
            if (!name.empty())
            {
                ttab.TileDirs.push_back(name);
                ttab.TileSubDirs.push_back(includeSubdirs ? *(bool*)includeSubdirs->At(i) : false);
            }
        }
    }

    mapper->RefreshTiles(tab);*/
}

///# ...
///# param tab ...
///# param subTab ...
///# param itemPids ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_TabSetItemPids(FOMapper* mapper, int tab, string_view subTab, const vector<hash>& itemPids)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    /*if (subTab.empty() || subTab == DEFAULT_SUB_TAB)
        return;

    // Add protos to sub tab
    if (itemPids && itemPids->GetSize())
    {
        ProtoItemVec proto_items;
        for (int i = 0, j = itemPids->GetSize(); i < j; i++)
        {
            hash pid = *(hash*)itemPids->At(i);
            ProtoItem* proto_item = mapper->ProtoMngr.GetProtoItem(pid);
            if (proto_item)
                proto_items.push_back(proto_item);
        }

        if (proto_items.size())
        {
            SubTab& stab = mapper->Tabs[tab][subTab];
            stab.ItemProtos = proto_items;
        }
    }
    // Delete sub tab
    else
    {
        auto it = mapper->Tabs[tab].find(subTab);
        if (it != mapper->Tabs[tab].end())
        {
            if (mapper->TabsActive[tab] == &it->second)
                mapper->TabsActive[tab] = nullptr;
            mapper->Tabs[tab].erase(it);
        }
    }

    // Recalculate whole pids
    SubTab& stab_default = mapper->Tabs[tab][DEFAULT_SUB_TAB];
    stab_default.ItemProtos.clear();
    for (auto it = mapper->Tabs[tab].begin(), end = mapper->Tabs[tab].end(); it != end; ++it)
    {
        SubTab& stab = it->second;
        if (&stab == &stab_default)
            continue;
        for (uint i = 0, j = (uint)stab.ItemProtos.size(); i < j; i++)
            stab_default.ItemProtos.push_back(stab.ItemProtos[i]);
    }
    if (!mapper->TabsActive[tab])
        mapper->TabsActive[tab] = &stab_default;

    // Refresh
    mapper->RefreshCurProtos();*/
}

///# ...
///# param tab ...
///# param subTab ...
///# param critterPids ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_TabSetCritterPids(FOMapper* mapper, int tab, string_view subTab, const vector<hash>& critterPids)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    /*if (subTab.empty() || subTab == DEFAULT_SUB_TAB)
        return;

    // Add protos to sub tab
    if (critterPids && critterPids->GetSize())
    {
        ProtoCritterVec cr_protos;
        for (int i = 0, j = critterPids->GetSize(); i < j; i++)
        {
            hash pid = *(hash*)critterPids->At(i);
            ProtoCritter* cr_data = mapper->ProtoMngr.GetProtoCritter(pid);
            if (cr_data)
                cr_protos.push_back(cr_data);
        }

        if (cr_protos.size())
        {
            SubTab& stab = mapper->Tabs[tab][subTab];
            stab.NpcProtos = cr_protos;
        }
    }
    // Delete sub tab
    else
    {
        auto it = mapper->Tabs[tab].find(subTab);
        if (it != mapper->Tabs[tab].end())
        {
            if (mapper->TabsActive[tab] == &it->second)
                mapper->TabsActive[tab] = nullptr;
            mapper->Tabs[tab].erase(it);
        }
    }

    // Recalculate whole pids
    SubTab& stab_default = mapper->Tabs[tab][DEFAULT_SUB_TAB];
    stab_default.NpcProtos.clear();
    for (auto it = mapper->Tabs[tab].begin(), end = mapper->Tabs[tab].end(); it != end; ++it)
    {
        SubTab& stab = it->second;
        if (&stab == &stab_default)
            continue;
        for (uint i = 0, j = (uint)stab.NpcProtos.size(); i < j; i++)
            stab_default.NpcProtos.push_back(stab.NpcProtos[i]);
    }
    if (!mapper->TabsActive[tab])
        mapper->TabsActive[tab] = &stab_default;

    // Refresh
    mapper->RefreshCurProtos();*/
}

///# ...
///# param tab ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_TabDelete(FOMapper* mapper, int tab)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    /*mapper->Tabs[tab].clear();
    SubTab& stab_default = mapper->Tabs[tab][DEFAULT_SUB_TAB];
    mapper->TabsActive[tab] = &stab_default;*/
}

///# ...
///# param tab ...
///# param subTab ...
///# param show ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_TabSelect(FOMapper* mapper, int tab, string_view subTab, bool show)
{
    if (tab < 0 || tab >= FOMapper::INT_MODE_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    if (show) {
        mapper->IntSetMode(tab);
    }

    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        return;
    }

    auto it = mapper->Tabs[tab].find(!subTab.empty() ? string(subTab) : FOMapper::DEFAULT_SUB_TAB);
    if (it != mapper->Tabs[tab].end()) {
        mapper->TabsActive[tab] = &it->second;
    }
}

///# ...
///# param tab ...
///# param tabName ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_TabSetName(FOMapper* mapper, int tab, string_view tabName)
{
    if (tab < 0 || tab >= FOMapper::INT_MODE_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    mapper->TabsName[tab] = tabName;
}

///# ...
///# param hx ...
///# param hy ...
///# param speed ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_MoveScreenToHex(FOMapper* mapper, ushort hx, ushort hy, uint speed, bool canStop)
{
    if (hx >= mapper->HexMngr.GetWidth() || hy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid hex args");
    }
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    if (speed == 0u) {
        mapper->HexMngr.FindSetCenter(hx, hy);
    }
    else {
        mapper->HexMngr.ScrollToHex(hx, hy, static_cast<float>(speed) / 1000.0f, canStop);
    }
}

///# ...
///# param ox ...
///# param oy ...
///# param speed ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_MoveScreenOffset(FOMapper* mapper, int ox, int oy, uint speed, bool canStop)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map is not loaded");
    }

    mapper->HexMngr.ScrollOffset(ox, oy, static_cast<float>(speed) / 1000.0f, canStop);
}

///# ...
///# param hx ...
///# param hy ...
///# param dir ...
///# param steps ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Mapper_Global_MoveHexByDir(FOMapper* mapper, ushort& hx, ushort& hy, uchar dir, uint steps)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }
    if (dir >= mapper->Settings.MapDirCount) {
        throw ScriptException("Invalid dir arg");
    }
    if (steps == 0u) {
        throw ScriptException("Steps arg is zero");
    }

    auto result = false;
    auto hx_ = hx;
    auto hy_ = hy;

    if (steps > 1) {
        for (uint i = 0; i < steps; i++) {
            result |= mapper->GeomHelper.MoveHexByDir(hx_, hy_, dir, mapper->HexMngr.GetWidth(), mapper->HexMngr.GetHeight());
        }
    }
    else {
        result |= mapper->GeomHelper.MoveHexByDir(hx_, hy_, dir, mapper->HexMngr.GetWidth(), mapper->HexMngr.GetHeight());
    }

    hx = static_cast<ushort>(hx_);
    hy = static_cast<ushort>(hy_);
    return result;
}

///# ...
///# param key ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Mapper_Global_GetIfaceIniStr(FOMapper* mapper, string_view key)
{
    return mapper->IfaceIni.GetStr("", key, "");
}

///# ...
///# param msg ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_Message(FOMapper* mapper, string_view msg)
{
    mapper->AddMess(msg);
}

///# ...
///# param textMsg ...
///# param strNum ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_MessageMsg(FOMapper* mapper, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    mapper->AddMess(mapper->CurLang.Msg[textMsg].GetStr(strNum));
}

///# ...
///# param text ...
///# param hx ...
///# param hy ...
///# param ms ...
///# param color ...
///# param fade ...
///# param ox ...
///# param oy ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_MapMessage(FOMapper* mapper, string_view text, ushort hx, ushort hy, uint ms, uint color, bool fade, int ox, int oy)
{
    FOMapper::MapText map_text;
    map_text.HexX = hx;
    map_text.HexY = hy;
    map_text.Color = (color != 0u ? color : COLOR_TEXT);
    map_text.Fade = fade;
    map_text.StartTick = mapper->GameTime.FrameTick();
    map_text.Tick = ms;
    map_text.Text = text;
    map_text.Pos = mapper->HexMngr.GetRectForText(hx, hy);
    map_text.EndPos = IRect(map_text.Pos, ox, oy);

    const auto it = std::find_if(mapper->GameMapTexts.begin(), mapper->GameMapTexts.end(), [&map_text](const FOMapper::MapText& t) { return t.HexX == map_text.HexX && t.HexY == map_text.HexY; });
    if (it != mapper->GameMapTexts.end()) {
        mapper->GameMapTexts.erase(it);
    }

    mapper->GameMapTexts.push_back(map_text);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Mapper_Global_GetMsgStr(FOMapper* mapper, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    return mapper->CurLang.Msg[textMsg].GetStr(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# param skipCount ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Mapper_Global_GetMsgStrSkip(FOMapper* mapper, int textMsg, uint strNum, uint skipCount)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    return mapper->CurLang.Msg[textMsg].GetStr(strNum, skipCount);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_GetMsgStrNumUpper(FOMapper* mapper, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    return mapper->CurLang.Msg[textMsg].GetStrNumUpper(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_GetMsgStrNumLower(FOMapper* mapper, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    return mapper->CurLang.Msg[textMsg].GetStrNumLower(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_GetMsgStrCount(FOMapper* mapper, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    return mapper->CurLang.Msg[textMsg].Count(strNum);
}

///# ...
///# param textMsg ...
///# param strNum ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Mapper_Global_IsMsgStr(FOMapper* mapper, int textMsg, uint strNum)
{
    if (textMsg >= TEXTMSG_COUNT) {
        throw ScriptException("Invalid text msg arg");
    }

    return mapper->CurLang.Msg[textMsg].Count(strNum) > 0;
}

///# ...
///# param text ...
///# param replace ...
///# param str ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Mapper_Global_ReplaceTextStr(FOMapper* mapper, string_view text, string_view replace, string_view str)
{
    const auto pos = text.find(replace, 0);
    if (pos == std::string::npos) {
        return string(text);
    }

    return string(text).replace(pos, replace.length(), text);
}

///# ...
///# param text ...
///# param replace ...
///# param i ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Mapper_Global_ReplaceTextInt(FOMapper* mapper, string_view text, string_view replace, int i)
{
    const auto pos = text.find(replace, 0);
    if (pos == std::string::npos) {
        return string(text);
    }

    return string(text).replace(pos, replace.length(), _str("{}", i).strv());
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_GetHexInPath(FOMapper* mapper, ushort fromHx, ushort fromHy, ushort& toHx, ushort& toHy, float angle, uint dist)
{
    pair<ushort, ushort> pre_block;
    pair<ushort, ushort> block;
    mapper->HexMngr.TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, false, nullptr, 0, &block, &pre_block, nullptr, true);
    toHx = pre_block.first;
    toHy = pre_block.second;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_GetPathLengthHex(FOMapper* mapper, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, uint cut)
{
    if (fromHx >= mapper->HexMngr.GetWidth() || fromHy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= mapper->HexMngr.GetWidth() || toHy >= mapper->HexMngr.GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (cut > 0 && !mapper->HexMngr.CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, cut)) {
        return 0;
    }

    vector<uchar> steps;
    if (!mapper->HexMngr.FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, steps, -1)) {
        return 0;
    }

    return static_cast<uint>(steps.size());
}

///# ...
///# param hx ...
///# param hy ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Mapper_Global_GetHexPos(FOMapper* mapper, ushort hx, ushort hy, int& x, int& y)
{
    x = y = 0;

    if (mapper->HexMngr.IsMapLoaded() && hx < mapper->HexMngr.GetWidth() && hy < mapper->HexMngr.GetHeight()) {
        mapper->HexMngr.GetHexCurrentPosition(hx, hy, x, y);

        x += mapper->Settings.ScrOx + (mapper->Settings.MapHexWidth / 2);
        y += mapper->Settings.ScrOy + (mapper->Settings.MapHexHeight / 2);

        x = static_cast<int>(static_cast<float>(x) / mapper->Settings.SpritesZoom);
        y = static_cast<int>(static_cast<float>(y) / mapper->Settings.SpritesZoom);

        return true;
    }

    return false;
}

///# ...
///# param x ...
///# param y ...
///# param hx ...
///# param hy ...
///# param ignoreInterface ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Mapper_Global_GetMonitorHex(FOMapper* mapper, int x, int y, ushort& hx, ushort& hy, bool ignoreInterface)
{
    const auto old_x = mapper->Settings.MouseX;
    const auto old_y = mapper->Settings.MouseY;

    mapper->Settings.MouseX = x;
    mapper->Settings.MouseY = y;

    ushort hx_;
    ushort hy_;
    const auto result = mapper->GetCurHex(hx_, hy_, ignoreInterface);

    mapper->Settings.MouseX = old_x;
    mapper->Settings.MouseY = old_y;

    if (result) {
        hx = hx_;
        hy = hy_;
        return true;
    }

    return false;
}

///# ...
///# param x ...
///# param y ...
///# param ignoreInterface ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ClientEntity* Mapper_Global_GetMonitorObject(FOMapper* mapper, int x, int y, bool ignoreInterface)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }

    if (!ignoreInterface && mapper->IsCurInInterface()) {
        return nullptr;
    }

    ItemHexView* item = nullptr;
    CritterView* cr = nullptr;
    mapper->HexMngr.GetSmthPixel(mapper->Settings.MouseX, mapper->Settings.MouseY, item, cr);

    ClientEntity* mobj = nullptr;
    if (item != nullptr) {
        mobj = item;
    }
    else if (cr != nullptr) {
        mobj = cr;
    }

    return mobj;
}

///# ...
///# param datName ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_AddDataSource(FOMapper* mapper, string_view datName)
{
    mapper->FileMngr.AddDataSource(datName, false);

    for (auto tab = 0; tab < FOMapper::TAB_COUNT; tab++) {
        mapper->RefreshTiles(tab);
    }
}

///# ...
///# param fontIndex ...
///# param fontFname ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_LoadFont(FOMapper* mapper, int fontIndex, string_view fontFname)
{
    bool result;
    if (fontFname.length() > 0 && fontFname[0] == '*') {
        result = mapper->SprMngr.LoadFontFO(fontIndex, fontFname.substr(1), false, false);
    }
    else {
        result = mapper->SprMngr.LoadFontBmf(fontIndex, fontFname);
    }

    if (result) {
        mapper->SprMngr.BuildFonts();
    }
    else {
        throw ScriptException("Unable to load font", fontIndex, fontFname);
    }
}

///# ...
///# param font ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_SetDefaultFont(FOMapper* mapper, int font, uint color)
{
    mapper->SprMngr.SetDefaultFont(font, color);
}

///# ...
///# param x ...
///# param y ...
///# param button ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_MouseClick(FOMapper* mapper, int x, int y, int button)
{
    /*IntVec prev_events = mapper->Settings.MainWindowMouseEvents;
    mapper->Settings.MainWindowMouseEvents.clear();
    int prev_x = mapper->Settings.MouseX;
    int prev_y = mapper->Settings.MouseY;
    int last_prev_x = mapper->Settings.LastMouseX;
    int last_prev_y = mapper->Settings.LastMouseY;
    int prev_cursor = mapper->CurMode;
    mapper->Settings.MouseX = mapper->Settings.LastMouseX = x;
    mapper->Settings.MouseY = mapper->Settings.LastMouseY = y;
    mapper->Settings.MainWindowMouseEvents.push_back(SDL_MOUSEBUTTONDOWN);
    mapper->Settings.MainWindowMouseEvents.push_back(MouseButtonToSdlButton(button));
    mapper->Settings.MainWindowMouseEvents.push_back(SDL_MOUSEBUTTONUP);
    mapper->Settings.MainWindowMouseEvents.push_back(MouseButtonToSdlButton(button));
    mapper->ParseMouse();
    mapper->Settings.MainWindowMouseEvents = prev_events;
    mapper->Settings.MouseX = prev_x;
    mapper->Settings.MouseY = prev_y;
    mapper->Settings.LastMouseX = last_prev_x;
    mapper->Settings.LastMouseY = last_prev_y;*/
}

///# ...
///# param key1 ...
///# param key2 ...
///# param key1Text ...
///# param key2Text ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_KeyboardPress(FOMapper* mapper, KeyCode key1, KeyCode key2, string_view key1Text, string_view key2Text)
{
    if (key1 == KeyCode::None && key2 == KeyCode::None) {
        return;
    }

    if (key1 != KeyCode::None) {
        mapper->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {key1, string(key1Text)}});
    }

    if (key2 != KeyCode::None) {
        mapper->ProcessInputEvent(InputEvent {InputEvent::KeyDownEvent {key2, string(key2Text)}});
        mapper->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {key2}});
    }

    if (key1 != KeyCode::None) {
        mapper->ProcessInputEvent(InputEvent {InputEvent::KeyUpEvent {key1}});
    }
}

///# ...
///# param fallAnimName ...
///# param dropAnimName ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_SetRainAnimation(FOMapper* mapper, string_view fallAnimName, string_view dropAnimName)
{
    mapper->HexMngr.SetRainAnimation(fallAnimName, dropAnimName);
}

///# ...
///# param targetZoom ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_ChangeZoom(FOMapper* mapper, float targetZoom)
{
    if (Math::FloatCompare(targetZoom, mapper->Settings.SpritesZoom)) {
        return;
    }

    if (targetZoom == 1.0f) {
        mapper->HexMngr.ChangeZoom(0);
    }
    else if (targetZoom > mapper->Settings.SpritesZoom) {
        while (targetZoom > mapper->Settings.SpritesZoom) {
            const auto old_zoom = mapper->Settings.SpritesZoom;

            mapper->HexMngr.ChangeZoom(1);

            if (Math::FloatCompare(mapper->Settings.SpritesZoom, old_zoom)) {
                break;
            }
        }
    }
    else if (targetZoom < mapper->Settings.SpritesZoom) {
        while (targetZoom < mapper->Settings.SpritesZoom) {
            const auto old_zoom = mapper->Settings.SpritesZoom;

            mapper->HexMngr.ChangeZoom(-1);

            if (Math::FloatCompare(mapper->Settings.SpritesZoom, old_zoom)) {
                break;
            }
        }
    }
}

///# ...
///# param sprName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_LoadSprite(FOMapper* mapper, string_view sprName)
{
    return mapper->AnimLoad(sprName, AtlasType::Static);
}

///# ...
///# param nameHash ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_LoadSpriteHash(FOMapper* mapper, uint nameHash)
{
    return mapper->AnimLoad(nameHash, AtlasType::Static);
}

///# ...
///# param sprId ...
///# param sprIndex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Mapper_Global_GetSpriteWidth(FOMapper* mapper, uint sprId, int sprIndex)
{
    auto* anim = mapper->AnimGetFrames(sprId);
    if (!anim || sprIndex >= static_cast<int>(anim->CntFrm)) {
        return 0;
    }

    const auto* si = mapper->SprMngr.GetSpriteInfo(sprIndex < 0 ? anim->GetCurSprId(mapper->GameTime.GameTick()) : anim->GetSprId(sprIndex));
    if (!si) {
        return 0;
    }

    return si->Width;
}

///# ...
///# param sprId ...
///# param sprIndex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Mapper_Global_GetSpriteHeight(FOMapper* mapper, uint sprId, int sprIndex)
{
    auto* anim = mapper->AnimGetFrames(sprId);
    if (!anim || sprIndex >= static_cast<int>(anim->CntFrm)) {
        return 0;
    }

    const auto* si = mapper->SprMngr.GetSpriteInfo(sprIndex < 0 ? anim->GetCurSprId(mapper->GameTime.GameTick()) : anim->GetSprId(sprIndex));
    if (!si) {
        return 0;
    }

    return si->Height;
}

///# ...
///# param sprId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_GetSpriteCount(FOMapper* mapper, uint sprId)
{
    auto* anim = mapper->AnimGetFrames(sprId);
    return anim ? anim->CntFrm : 0;
}

///# ...
///# param sprId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_GetSpriteTicks(FOMapper* mapper, uint sprId)
{
    auto* anim = mapper->AnimGetFrames(sprId);
    return anim ? anim->Ticks : 0;
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Mapper_Global_GetPixelColor(FOMapper* mapper, uint sprId, int frameIndex, int x, int y)
{
    if (sprId == 0u) {
        return 0;
    }

    auto* anim = mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return 0;
    }

    const auto spr_id = (frameIndex < 0 ? anim->GetCurSprId(mapper->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    return mapper->SprMngr.GetPixColor(spr_id, x, y, false);
}

///# ...
///# param text ...
///# param w ...
///# param h ...
///# param font ...
///# param flags ...
///# param tw ...
///# param th ...
///# param lines ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_GetTextInfo(FOMapper* mapper, string_view text, int w, int h, int font, int flags, int& tw, int& th, int& lines)
{
    if (!mapper->SprMngr.GetTextInfo(w, h, text, font, flags, tw, th, lines)) {
        throw ScriptException("Can't evaluate text information", font);
    }
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# param color ...
///# param offs ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DrawSprite(FOMapper* mapper, uint sprId, int frameIndex, int x, int y, uint color, bool offs)
{
    if (!mapper->SpritesCanDraw || !sprId) {
        return;
    }

    auto* anim = mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    auto x_ = x;
    auto y_ = y;

    const auto spr_id = (frameIndex < 0 ? anim->GetCurSprId(mapper->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (offs) {
        const auto* si = mapper->SprMngr.GetSpriteInfo(spr_id);
        if (!si) {
            return;
        }

        x_ += -si->Width / 2 + si->OffsX;
        y_ += -si->Height + si->OffsY;
    }

    mapper->SprMngr.DrawSprite(spr_id, x_, y_, COLOR_SCRIPT_SPRITE(color));
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///# param zoom ...
///# param color ...
///# param offs ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DrawSpriteSize(FOMapper* mapper, uint sprId, int frameIndex, int x, int y, int w, int h, bool zoom, uint color, bool offs)
{
    if (!mapper->SpritesCanDraw || !sprId) {
        return;
    }

    auto* anim = mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    auto x_ = x;
    auto y_ = y;

    const auto spr_id = (frameIndex < 0 ? anim->GetCurSprId(mapper->GameTime.GameTick()) : anim->GetSprId(frameIndex));
    if (offs) {
        const auto* si = mapper->SprMngr.GetSpriteInfo(spr_id);
        if (!si) {
            return;
        }

        x_ += si->OffsX;
        y_ += si->OffsY;
    }

    mapper->SprMngr.DrawSpriteSizeExt(spr_id, x_, y_, w, h, zoom, true, true, COLOR_SCRIPT_SPRITE(color));
}

///# ...
///# param sprId ...
///# param frameIndex ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///# param sprWidth ...
///# param sprHeight ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DrawSpritePattern(FOMapper* mapper, uint sprId, int frameIndex, int x, int y, int w, int h, int sprWidth, int sprHeight, uint color)
{
    if (!mapper->SpritesCanDraw || !sprId) {
        return;
    }

    auto* anim = mapper->AnimGetFrames(sprId);
    if (!anim || frameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    mapper->SprMngr.DrawSpritePattern(frameIndex < 0 ? anim->GetCurSprId(mapper->GameTime.GameTick()) : anim->GetSprId(frameIndex), x, y, w, h, sprWidth, sprHeight, COLOR_SCRIPT_SPRITE(color));
}

///# ...
///# param text ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///# param color ...
///# param font ...
///# param flags ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DrawText(FOMapper* mapper, string_view text, int x, int y, int w, int h, uint color, int font, int flags)
{
    if (!mapper->SpritesCanDraw) {
        return;
    }
    if (text.length() == 0) {
        return;
    }

    auto x_ = x;
    auto y_ = y;
    auto w_ = w;
    auto h_ = h;

    if (w_ < 0) {
        w_ = -w_;
        x_ -= w_;
    }
    if (h_ < 0) {
        h_ = -h_;
        y_ -= h_;
    }

    mapper->SprMngr.DrawStr(IRect(x_, y_, x_ + w_, y_ + h_), text, flags, COLOR_SCRIPT_TEXT(color), font);
}

///# ...
///# param primitiveType ...
///# param data ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DrawPrimitive(FOMapper* mapper, int primitiveType, const vector<int>& data)
{
    if (!mapper->SpritesCanDraw || data.empty()) {
        return;
    }

    RenderPrimitiveType prim;
    switch (primitiveType) {
    case 0:
        prim = RenderPrimitiveType::PointList;
        break;
    case 1:
        prim = RenderPrimitiveType::LineList;
        break;
    case 2:
        prim = RenderPrimitiveType::LineStrip;
        break;
    case 3:
        prim = RenderPrimitiveType::TriangleList;
        break;
    case 4:
        prim = RenderPrimitiveType::TriangleStrip;
        break;
    case 5:
        prim = RenderPrimitiveType::TriangleFan;
        break;
    default:
        return;
    }

    const auto size = data.size() / 3;
    PrimitivePoints points;
    points.resize(size);

    for (const auto i : xrange(size)) {
        auto& pp = points[i];
        pp.PointX = data[i * 3];
        pp.PointY = data[i * 3 + 1];
        pp.PointColor = data[i * 3 + 2];
        pp.PointOffsX = nullptr;
        pp.PointOffsY = nullptr;
    }

    mapper->SprMngr.DrawPoints(points, prim, nullptr, nullptr, nullptr);
}

///# ...
///# param mapSpr ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DrawMapSprite(FOMapper* mapper, MapSprite* mapSpr)
{
    if (mapSpr == nullptr) {
        throw ScriptException("Map sprite arg is null");
    }

    if (!mapper->HexMngr.IsMapLoaded()) {
        return;
    }
    if (mapSpr->HexX >= mapper->HexMngr.GetWidth() || mapSpr->HexY >= mapper->HexMngr.GetHeight()) {
        return;
    }
    if (!mapper->HexMngr.IsHexToDraw(mapSpr->HexX, mapSpr->HexY)) {
        return;
    }

    auto* anim = mapper->AnimGetFrames(mapSpr->SprId);
    if (!anim || mapSpr->FrameIndex >= static_cast<int>(anim->CntFrm)) {
        return;
    }

    auto color = mapSpr->Color;
    auto is_flat = mapSpr->IsFlat;
    auto no_light = mapSpr->NoLight;
    auto draw_order = mapSpr->DrawOrder;
    auto draw_order_hy_offset = mapSpr->DrawOrderHyOffset;
    auto corner = mapSpr->Corner;
    auto disable_egg = mapSpr->DisableEgg;
    auto contour_color = mapSpr->ContourColor;

    if (mapSpr->ProtoId != 0u) {
        const auto* proto_item = mapper->ProtoMngr.GetProtoItem(mapSpr->ProtoId);
        if (!proto_item) {
            return;
        }

        color = (proto_item->GetIsColorize() ? proto_item->GetLightColor() : 0);
        is_flat = proto_item->GetIsFlat();
        const auto is_item = !proto_item->IsAnyScenery();
        no_light = (is_flat && !is_item);
        draw_order = (is_flat ? (is_item ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY) : (is_item ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY));
        draw_order_hy_offset = static_cast<int>(proto_item->GetDrawOrderOffsetHexY());
        corner = proto_item->GetCorner();
        disable_egg = proto_item->GetDisableEgg();
        contour_color = (proto_item->GetIsBadItem() ? COLOR_RGB(255, 0, 0) : 0);
    }

    auto& f = mapper->HexMngr.GetField(mapSpr->HexX, mapSpr->HexY);
    auto& tree = mapper->HexMngr.GetDrawTree();
    auto& spr = tree.InsertSprite(draw_order, mapSpr->HexX, mapSpr->HexY + draw_order_hy_offset, (mapper->Settings.MapHexWidth / 2) + mapSpr->OffsX, (mapper->Settings.MapHexHeight / 2) + mapSpr->OffsY, &f.ScrX, &f.ScrY, mapSpr->FrameIndex < 0 ? anim->GetCurSprId(mapper->GameTime.GameTick()) : anim->GetSprId(mapSpr->FrameIndex), nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsX : nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsY : nullptr, mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, nullptr, nullptr);

    spr.MapSpr = mapSpr;
    mapSpr->AddRef();

    if (!no_light) {
        spr.SetLight(corner, mapper->HexMngr.GetLightHex(0, 0), mapper->HexMngr.GetWidth(), mapper->HexMngr.GetHeight());
    }

    if (!is_flat && !disable_egg) {
        int egg_type = 0;
        switch (corner) {
        case CORNER_SOUTH:
            egg_type = EGG_X_OR_Y;
            break;
        case CORNER_NORTH:
            egg_type = EGG_X_AND_Y;
            break;
        case CORNER_EAST_WEST:
        case CORNER_WEST:
            egg_type = EGG_Y;
            break;
        default:
            egg_type = EGG_X;
            break;
        }
        spr.SetEgg(egg_type);
    }

    if (color != 0u) {
        spr.SetColor(color & 0xFFFFFF);
        spr.SetFixedAlpha(color >> 24);
    }

    if (contour_color != 0u) {
        spr.SetContour(CONTOUR_CUSTOM, contour_color);
    }
}

///# ...
///# param modelName ...
///# param anim1 ...
///# param anim2 ...
///# param dir ...
///# param l ...
///# param t ...
///# param r ...
///# param b ...
///# param scratch ...
///# param center ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DrawCritter2d(FOMapper* mapper, hash modelName, uint anim1, uint anim2, uchar dir, int l, int t, int r, int b, bool scratch, bool center, uint color)
{
    auto* anim = mapper->ResMngr.GetCritterAnim(modelName, anim1, anim2, dir);
    if (anim) {
        mapper->SprMngr.DrawSpriteSize(anim->Ind[0], l, t, r - l, b - t, scratch, center, COLOR_SCRIPT_SPRITE(color));
    }
}

///# ...
///# param instance ...
///# param modelName ...
///# param anim1 ...
///# param anim2 ...
///# param layers ...
///# param position ...
///# param color ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_DrawCritter3d(FOMapper* mapper, uint instance, hash modelName, uint anim1, uint anim2, const vector<int>& layers, const vector<float>& position, uint color)
{
    // x y
    // rx ry rz
    // sx sy sz
    // speed
    // scissor l t r b

    if (instance >= mapper->DrawCritterModel.size()) {
        mapper->DrawCritterModel.resize(instance + 1u);
        mapper->DrawCritterModelCrType.resize(instance + 1u);
        mapper->DrawCritterModelFailedToLoad.resize(instance + 1u);
    }

    if (mapper->DrawCritterModelFailedToLoad[instance] && mapper->DrawCritterModelCrType[instance] == modelName) {
        return;
    }

    auto*& model = mapper->DrawCritterModel[instance];
    if (model == nullptr || mapper->DrawCritterModelCrType[instance] != modelName) {
        if (model != nullptr) {
            mapper->SprMngr.FreeModel(model);
        }

        model = mapper->SprMngr.LoadModel(_str().parseHash(modelName), false);

        mapper->DrawCritterModelCrType[instance] = modelName;
        mapper->DrawCritterModelFailedToLoad[instance] = false;

        if (model == nullptr) {
            mapper->DrawCritterModelFailedToLoad[instance] = true;
            return;
        }

        model->EnableShadow(false);
        model->SetTimer(false);
    }

    const auto count = static_cast<uint>(position.size());
    const auto x = (count > 0 ? position[0] : 0.0f);
    const auto y = (count > 1 ? position[1] : 0.0f);
    const auto rx = (count > 2 ? position[2] : 0.0f);
    const auto ry = (count > 3 ? position[3] : 0.0f);
    const auto rz = (count > 4 ? position[4] : 0.0f);
    const auto sx = (count > 5 ? position[5] : 1.0f);
    const auto sy = (count > 6 ? position[6] : 1.0f);
    const auto sz = (count > 7 ? position[7] : 1.0f);
    const auto speed = (count > 8 ? position[8] : 1.0f);
    const auto period = (count > 9 ? position[9] : 0.0f);
    const auto stl = (count > 10 ? position[10] : 0.0f);
    const auto stt = (count > 11 ? position[11] : 0.0f);
    const auto str = (count > 12 ? position[12] : 0.0f);
    const auto stb = (count > 13 ? position[13] : 0.0f);

    if (count > 13) {
        mapper->SprMngr.PushScissor(static_cast<int>(stl), static_cast<int>(stt), static_cast<int>(str), static_cast<int>(stb));
    }

    std::memset(mapper->DrawCritterModelLayers, 0, sizeof(mapper->DrawCritterModelLayers));
    for (uint i = 0, j = static_cast<uint>(layers.size()); i < j && i < LAYERS3D_COUNT; i++) {
        mapper->DrawCritterModelLayers[i] = layers[i];
    }

    model->SetDirAngle(0);
    model->SetRotation(rx * PI_FLOAT / 180.0f, ry * PI_FLOAT / 180.0f, rz * PI_FLOAT / 180.0f);
    model->SetScale(sx, sy, sz);
    model->SetSpeed(speed);
    model->SetAnimation(anim1, anim2, mapper->DrawCritterModelLayers, ANIMATION_PERIOD(static_cast<int>(period * 100.0f)) | ANIMATION_NO_SMOOTH);

    mapper->SprMngr.Draw3d(static_cast<int>(x), static_cast<int>(y), model, COLOR_SCRIPT_SPRITE(color));

    if (count > 13) {
        mapper->SprMngr.PopScissor();
    }
}

///# ...
///# param x ...
///# param y ...
///# param w ...
///# param h ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_PushDrawScissor(FOMapper* mapper, int x, int y, int w, int h)
{
    mapper->SprMngr.PushScissor(x, y, x + w, y + h);
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Global_PopDrawScissor(FOMapper* mapper)
{
    mapper->SprMngr.PopScissor();
}
