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
#include "StringUtils.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param pid ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Mapper_Game_AddItem(FOMapper* mapper, hstring pid, ushort hx, ushort hy)
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
[[maybe_unused]] CritterView* Mapper_Game_AddCritter(FOMapper* mapper, hstring pid, ushort hx, ushort hy)
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
[[maybe_unused]] ItemView* Mapper_Game_GetItem(FOMapper* mapper, ushort hx, ushort hy)
{
    return mapper->HexMngr.GetItem(hx, hy, hstring());
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Mapper_Game_GetItems(FOMapper* mapper, ushort hx, ushort hy)
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
[[maybe_unused]] CritterView* Mapper_Game_GetCritter(FOMapper* mapper, ushort hx, ushort hy, CritterFindType findType)
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
[[maybe_unused]] vector<CritterView*> Mapper_Game_GetCritters(FOMapper* mapper, ushort hx, ushort hy, CritterFindType findType)
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
[[maybe_unused]] void Mapper_Game_MoveEntity(FOMapper* mapper, ClientEntity* entity, ushort hx, ushort hy)
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
[[maybe_unused]] void Mapper_Game_DeleteEntity(FOMapper* mapper, ClientEntity* entity)
{
    mapper->DeleteEntity(entity);
}

///# ...
///# param entities ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Game_DeleteEntities(FOMapper* mapper, const vector<ClientEntity*>& entities)
{
    for (auto* entity : entities) {
        if (entity != nullptr && !entity->IsDestroyed()) {
            mapper->DeleteEntity(entity);
        }
    }
}

///# ...
///# param entity ...
///# param set ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Game_SelectEntity(FOMapper* mapper, ClientEntity* entity, bool set)
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
[[maybe_unused]] void Mapper_Game_SelectEntities(FOMapper* mapper, const vector<ClientEntity*>& entities, bool set)
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
[[maybe_unused]] ClientEntity* Mapper_Game_GetSelectedEntity(FOMapper* mapper)
{
    return !mapper->SelectedEntities.empty() ? mapper->SelectedEntities[0] : nullptr;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ClientEntity*> Mapper_Game_GetSelectedEntities(FOMapper* mapper)
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
[[maybe_unused]] uint Mapper_Game_GetTilesCount(FOMapper* mapper, ushort hx, ushort hy, bool roof)
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
[[maybe_unused]] void Mapper_Game_DeleteTile(FOMapper* mapper, ushort hx, ushort hy, bool roof, int layer)
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
///# param ox ...
///# param oy ...
///# param layer ...
///# param roof ...
///# param picHash ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Game_AddTileHash(FOMapper* mapper, ushort hx, ushort hy, int ox, int oy, int layer, bool roof, hstring picHash)
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

    if (!picHash) {
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
[[maybe_unused]] hstring Mapper_Game_GetTileNameEx(FOMapper* mapper, ushort hx, ushort hy, bool roof, int layer)
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
            return mapper->ResolveHash(tiles[i].NameHash);
        }
    }

    return hstring();
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
[[maybe_unused]] void Mapper_Game_AddTileName(FOMapper* mapper, ushort hx, ushort hy, int ox, int oy, int layer, bool roof, string_view picName)
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

    const auto pic_hash = mapper->ToHashedString(picName);
    mapper->HexMngr.SetTile(pic_hash, hx, hy, static_cast<short>(ox), static_cast<short>(oy), static_cast<uchar>(layer_), roof, false);
}

///# ...
///# param fileName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] MapView* Mapper_Game_LoadMap(FOMapper* mapper, string_view fileName)
{
    // Todo: need attention!
    // auto* pmap = new ProtoMap(mapper->ToHashedString(fileName), mapper->GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME));
    // if (!pmap->EditorLoad(mapper->ServerFileSys, mapper->ProtoMngr, mapper->SprMngr, mapper->ResMngr))
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
[[maybe_unused]] void Mapper_Game_UnloadMap(FOMapper* mapper, MapView* map)
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

    map->GetProto()->Release();
    map->Release();
}

///# ...
///# param map ...
///# param customName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Mapper_Game_SaveMap(FOMapper* mapper, MapView* map, string_view customName)
{
    if (map == nullptr) {
        throw ScriptException("Proto map arg nullptr");
    }

    // Todo: need attention!
    //((ProtoMap*)map->Proto)->EditorSave(mapper->ServerFileSys, customName);
    mapper->RunMapSaveScript(map);
    return true;
}

///# ...
///# param map ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Mapper_Game_ShowMap(FOMapper* mapper, MapView* map)
{
    if (map == nullptr) {
        throw ScriptException("Proto map arg nullptr");
    }

    if (mapper->ActiveMap == map) {
        return true;
    }

    mapper->SelectClear();
    // Todo: need attention!
    // if (!mapper->HexMngr.SetProtoMap(*static_cast<const ProtoMap*>(map->GetProto()))) {
    //  return false;
    //}

    mapper->HexMngr.FindSetCenter(map->GetWorkHexX(), map->GetWorkHexY());
    mapper->ActiveMap = map;

    return true;
}

///# ...
///# param index ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<MapView*> Mapper_Game_GetLoadedMaps(FOMapper* mapper, int& index)
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
[[maybe_unused]] vector<string> Mapper_Game_GetMapFileNames(FOMapper* mapper, string_view dir)
{
    vector<string> names;

    // Todo: Settings.MapsDir
    // auto map_files = mapper->ServerFileSys.FilterFiles("fomap", dir, false);
    // while (map_files.MoveNext()) {
    //    auto file_header = map_files.GetCurFileHeader();
    //    names.emplace_back(file_header.GetName());
    // }

    return names;
}

///# ...
///# param width ...
///# param height ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Game_ResizeMap(FOMapper* mapper, ushort width, ushort height)
{
    if (!mapper->HexMngr.IsMapLoaded()) {
        throw ScriptException("Map not loaded");
    }

    RUNTIME_ASSERT(mapper->ActiveMap);
    auto* pmap = const_cast<ProtoMap*>(dynamic_cast<const ProtoMap*>(mapper->ActiveMap->GetProto()));

    // Unload current
    // Todo: need attention!
    // mapper->HexMngr.GetProtoMap(*pmap);
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
        pmap->SetWorkHexX(maxhx - 1);
    }
    if (pmap->GetWorkHexY() >= maxhy) {
        pmap->SetWorkHexY(maxhy - 1);
    }

    pmap->SetWidth(maxhx);
    pmap->SetHeight(maxhy);

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
    // mapper->HexMngr.SetProtoMap(*pmap);
    mapper->HexMngr.FindSetCenter(pmap->GetWorkHexX(), pmap->GetWorkHexY());
}

///# ...
///# param tab ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<string> Mapper_Game_TabGetTileDirs(FOMapper* mapper, int tab)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    auto& ttab = mapper->TabsTiles[tab];
    return ttab.TileDirs;
}

///# ...
///# param tab ...
///# param subTab ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<hstring> Mapper_Game_TabGetItemPids(FOMapper* mapper, int tab, string_view subTab)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && !mapper->Tabs[tab].count(string(subTab))) {
        return vector<hstring>();
    }

    vector<hstring> pids;
    const auto& stab = mapper->Tabs[tab][!subTab.empty() ? string(subTab) : FOMapper::DEFAULT_SUB_TAB];
    for (const auto* proto : stab.ItemProtos) {
        pids.push_back(proto->GetProtoId());
    }
    return pids;
}

///# ...
///# param tab ...
///# param subTab ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<hstring> Mapper_Game_TabGetCritterPids(FOMapper* mapper, int tab, string_view subTab)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && !mapper->Tabs[tab].count(string(subTab))) {
        return vector<hstring>();
    }

    vector<hstring> pids;
    const auto& stab = mapper->Tabs[tab][!subTab.empty() ? string(subTab) : FOMapper::DEFAULT_SUB_TAB];
    for (const auto* proto : stab.NpcProtos) {
        pids.push_back(proto->GetProtoId());
    }
    return pids;
}

///# ...
///# param tab ...
///# param dirNames ...
///# param includeSubdirs ...
///@ ExportMethod
[[maybe_unused]] void Mapper_Game_TabSetTileDirs(FOMapper* mapper, int tab, const vector<string>& dirNames, const vector<bool>& includeSubdirs)
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
[[maybe_unused]] void Mapper_Game_TabSetItemPids(FOMapper* mapper, int tab, string_view subTab, const vector<hstring>& itemPids)
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
[[maybe_unused]] void Mapper_Game_TabSetCritterPids(FOMapper* mapper, int tab, string_view subTab, const vector<hstring>& critterPids)
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
[[maybe_unused]] void Mapper_Game_TabDelete(FOMapper* mapper, int tab)
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
[[maybe_unused]] void Mapper_Game_TabSelect(FOMapper* mapper, int tab, string_view subTab, bool show)
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
[[maybe_unused]] void Mapper_Game_TabSetName(FOMapper* mapper, int tab, string_view tabName)
{
    if (tab < 0 || tab >= FOMapper::INT_MODE_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    mapper->TabsName[tab] = tabName;
}

///# ...
///# param key ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Mapper_Game_GetIfaceIniStr(FOMapper* mapper, string_view key)
{
    return mapper->IfaceIni.GetStr("", key, "");
}
