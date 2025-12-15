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

#include "Common.h"

#include "FileSystem.h"
#include "Geometry.h"
#include "Mapper.h"

FO_BEGIN_NAMESPACE();

///@ ExportMethod
FO_SCRIPT_API ItemView* Mapper_Game_AddItem(FOMapper* mapper, hstring pid, mpos hex)
{
    if (!mapper->GetCurMap()->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return mapper->CreateItem(pid, hex, nullptr);
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Mapper_Game_AddCritter(FOMapper* mapper, hstring pid, mpos hex)
{
    if (!mapper->GetCurMap()->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return mapper->CreateCritter(pid, hex);
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Mapper_Game_GetItem(FOMapper* mapper, mpos hex)
{
    return mapper->GetCurMap()->GetItemOnHex(hex, hstring());
}

///@ ExportMethod
FO_SCRIPT_API vector<ItemView*> Mapper_Game_GetItems(FOMapper* mapper, mpos hex)
{
    const auto hex_items = mapper->GetCurMap()->GetItemsOnHex(hex);
    return vec_transform(hex_items, [](auto&& item) -> ItemView* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Mapper_Game_GetCritter(FOMapper* mapper, mpos hex, CritterFindType findType)
{
    const auto critters = mapper->GetCurMap()->GetCrittersOnHex(hex, findType);
    return !critters.empty() ? critters.front() : nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Mapper_Game_GetCritters(FOMapper* mapper, mpos hex, CritterFindType findType)
{
    return vec_transform(mapper->GetCurMap()->GetCrittersOnHex(hex, findType), [](auto&& cr) -> CritterView* { return cr; });
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_MoveEntity(FOMapper* mapper, ClientEntity* entity, mpos hex)
{
    if (!mapper->GetCurMap()->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    mapper->MoveEntity(entity, hex);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_DeleteEntity(FOMapper* mapper, ClientEntity* entity)
{
    mapper->DeleteEntity(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_DeleteEntities(FOMapper* mapper, const vector<ClientEntity*>& entities)
{
    for (auto* entity : entities) {
        if (entity != nullptr && !entity->IsDestroyed()) {
            mapper->DeleteEntity(entity);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SelectEntity(FOMapper* mapper, ClientEntity* entity, bool set)
{
    if (entity == nullptr) {
        throw ScriptException("Entity arg is null");
    }

    if (set) {
        mapper->SelectAdd(entity);
    }
    else {
        mapper->SelectRemove(entity);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SelectEntities(FOMapper* mapper, const vector<ClientEntity*>& entities, bool set)
{
    for (auto* entity : entities) {
        if (entity != nullptr) {
            if (set) {
                mapper->SelectAdd(entity);
            }
            else {
                mapper->SelectRemove(entity);
            }
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API ClientEntity* Mapper_Game_GetSelectedEntity(FOMapper* mapper)
{
    return !mapper->SelectedEntities.empty() ? mapper->SelectedEntities[0].get() : nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<ClientEntity*> Mapper_Game_GetSelectedEntities(FOMapper* mapper)
{
    vector<ClientEntity*> entities;
    entities.reserve(mapper->SelectedEntities.size());

    for (auto& entity : mapper->SelectedEntities) {
        entities.emplace_back(entity.get());
    }

    return entities;
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Mapper_Game_AddTile(FOMapper* mapper, hstring pid, mpos hex, int32 layer, bool roof)
{
    if (mapper->GetCurMap() == nullptr) {
        throw ScriptException("Map not loaded");
    }
    if (!mapper->GetCurMap()->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto corrected_layer = numeric_cast<uint8>(std::clamp(layer, 0, 4));

    return mapper->GetCurMap()->AddMapperTile(pid, hex, corrected_layer, roof);
}

///@ ExportMethod
FO_SCRIPT_API MapView* Mapper_Game_LoadMap(FOMapper* mapper, string_view fileName)
{
    return mapper->LoadMap(fileName);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_UnloadMap(FOMapper* mapper, MapView* map)
{
    mapper->UnloadMap(map);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SaveMap(FOMapper* mapper, MapView* map, string_view customName)
{
    mapper->SaveMap(map, customName);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_ShowMap(FOMapper* mapper, MapView* map)
{
    mapper->ShowMap(map);
}

///@ ExportMethod
FO_SCRIPT_API vector<MapView*> Mapper_Game_GetLoadedMaps(FOMapper* mapper, int32& index)
{
    index = -1;

    for (int32 i = 0, j = numeric_cast<int32>(mapper->LoadedMaps.size()); i < j; i++) {
        const auto& map = mapper->LoadedMaps[i];
        if (map.get() == mapper->GetCurMap()) {
            index = i;
        }
    }

    vector<MapView*> result;

    for (auto& map : mapper->LoadedMaps) {
        result.emplace_back(map.get());
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<string> Mapper_Game_GetMapFileNames(FOMapper* mapper, string_view dir)
{
    vector<string> names;

    auto map_files = mapper->MapsFileSys.FilterFiles("fomap", dir, false);

    for (const auto& map_file_header : map_files) {
        names.emplace_back(map_file_header.GetNameNoExt());
    }

    return names;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_ResizeMap(FOMapper* mapper, int32 width, int32 height)
{
    if (mapper->GetCurMap() == nullptr) {
        throw ScriptException("Map not loaded");
    }

    mapper->ResizeMap(mapper->GetCurMap(), width, height);
}

///@ ExportMethod
FO_SCRIPT_API vector<hstring> Mapper_Game_TabGetItemPids(FOMapper* mapper, int32 tab, string_view subTab)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && mapper->Tabs[tab].count(string(subTab)) == 0) {
        return {};
    }

    vector<hstring> pids;
    const auto& stab = mapper->Tabs[tab][!subTab.empty() ? string(subTab) : FOMapper::DEFAULT_SUB_TAB];

    for (const auto& proto : stab.ItemProtos) {
        pids.emplace_back(proto->GetProtoId());
    }

    return pids;
}

///@ ExportMethod
FO_SCRIPT_API vector<hstring> Mapper_Game_TabGetCritterPids(FOMapper* mapper, int32 tab, string_view subTab)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && mapper->Tabs[tab].count(string(subTab)) == 0) {
        return {};
    }

    vector<hstring> pids;
    const auto& stab = mapper->Tabs[tab][!subTab.empty() ? string(subTab) : FOMapper::DEFAULT_SUB_TAB];

    for (const auto& proto : stab.CritterProtos) {
        pids.emplace_back(proto->GetProtoId());
    }

    return pids;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSetItemPids(FOMapper* mapper, int32 tab, string_view subTab, const vector<hstring>& itemPids)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    if (subTab.empty() || subTab == FOMapper::DEFAULT_SUB_TAB) {
        return;
    }

    // Add protos to sub tab
    if (!itemPids.empty()) {
        vector<raw_ptr<const ProtoItem>> protos;

        for (const auto item_pid : itemPids) {
            const auto* proto = mapper->ProtoMngr.GetProtoItemSafe(item_pid);

            if (proto != nullptr) {
                protos.emplace_back(proto);
            }
        }

        auto& stab = mapper->Tabs[tab][string(subTab)];
        stab.ItemProtos = protos;
    }
    else {
        // Delete sub tab
        const auto it = mapper->Tabs[tab].find(string(subTab));

        if (it != mapper->Tabs[tab].end()) {
            if (mapper->TabsActive[tab] == &it->second) {
                mapper->TabsActive[tab] = nullptr;
            }

            mapper->Tabs[tab].erase(it);
        }
    }

    // Recalculate whole pids
    auto& stab_default = mapper->Tabs[tab][FOMapper::DEFAULT_SUB_TAB];
    stab_default.ItemProtos.clear();

    for (auto& stab : mapper->Tabs[tab] | std::views::values) {
        if (&stab == &stab_default) {
            continue;
        }

        for (auto& proto : stab.ItemProtos) {
            stab_default.ItemProtos.emplace_back(proto);
        }
    }

    if (mapper->TabsActive[tab] == nullptr) {
        mapper->TabsActive[tab] = &stab_default;
    }

    mapper->RefreshCurProtos();
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSetCritterPids(FOMapper* mapper, int32 tab, string_view subTab, const vector<hstring>& critterPids)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    if (subTab.empty() || subTab == FOMapper::DEFAULT_SUB_TAB) {
        return;
    }

    // Add protos to sub tab
    if (!critterPids.empty()) {
        vector<raw_ptr<const ProtoCritter>> protos;

        for (const auto pid : critterPids) {
            const auto* proto = mapper->ProtoMngr.GetProtoCritter(pid);
            protos.emplace_back(proto);
        }

        if (!protos.empty()) {
            auto& stab = mapper->Tabs[tab][string(subTab)];
            stab.CritterProtos = protos;
        }
    }
    else {
        // Delete sub tab
        const auto it = mapper->Tabs[tab].find(string(subTab));
        if (it != mapper->Tabs[tab].end()) {
            if (mapper->TabsActive[tab] == &it->second) {
                mapper->TabsActive[tab] = nullptr;
            }

            mapper->Tabs[tab].erase(it);
        }
    }

    // Recalculate whole pids
    auto& stab_default = mapper->Tabs[tab][FOMapper::DEFAULT_SUB_TAB];
    stab_default.CritterProtos.clear();

    for (auto it = mapper->Tabs[tab].begin(), end = mapper->Tabs[tab].end(); it != end; ++it) {
        auto& stab = it->second;
        if (&stab == &stab_default) {
            continue;
        }
        for (size_t i = 0; i < stab.CritterProtos.size(); i++) {
            stab_default.CritterProtos.emplace_back(stab.CritterProtos[i]);
        }
    }

    if (mapper->TabsActive[tab] == nullptr) {
        mapper->TabsActive[tab] = &stab_default;
    }

    // Refresh
    mapper->RefreshCurProtos();
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabDelete(FOMapper* mapper, int32 tab)
{
    if (tab < 0 || tab >= FOMapper::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    mapper->Tabs[tab].clear();
    auto& stab_default = mapper->Tabs[tab][FOMapper::DEFAULT_SUB_TAB];
    mapper->TabsActive[tab] = &stab_default;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSelect(FOMapper* mapper, int32 tab, string_view subTab, bool show)
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

    const auto it = mapper->Tabs[tab].find(!subTab.empty() ? string(subTab) : FOMapper::DEFAULT_SUB_TAB);
    if (it != mapper->Tabs[tab].end()) {
        mapper->TabsActive[tab] = &it->second;
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSetName(FOMapper* mapper, int32 tab, string_view tabName)
{
    if (tab < 0 || tab >= FOMapper::INT_MODE_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    mapper->TabsName[tab] = tabName;
}

///@ ExportMethod
FO_SCRIPT_API string Mapper_Game_GetIfaceIniStr(FOMapper* mapper, string_view key)
{
    return string(mapper->IfaceIni->GetAsStr("", key));
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_AddMessage(FOMapper* mapper, string_view message)
{
    mapper->AddMess(message);
}

FO_END_NAMESPACE();
