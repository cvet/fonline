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

#include "Common.h"

#include "Application.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "Mapper.h"
#include "Rendering.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

static void CenterMapperViewOnHex(ptr<MapView> map, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    map->InstantScrollTo(hex);

    constexpr ipos32 hex_center {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    ipos32 hex_screen_pos = map->MapToScreenPos(map->GetHexMapPos(hex) + hex_center);
    isize32 screen_size = map->GetScreenSize();
    ipos32 screen_center {screen_size.width / 2, screen_size.height / 2};
    fpos32 correction = fpos32(hex_screen_pos - screen_center) / std::max(map->GetSpritesZoom(), 0.001f);

    map->InstantScroll(correction);
    map->RebuildMap();
}

static void CenterMapperViewOnRawHex(ptr<MapView> map, ipos32 rawHex)
{
    FO_STACK_TRACE_ENTRY();

    ipos32 new_screen_hex = map->ConvertToScreenRawHex(rawHex);
    ipos32 offset_to_new_pos = GeometryHelper::GetHexOffset(map->GetScreenRawHex(), new_screen_hex);
    map->InstantScroll(fpos32(offset_to_new_pos));

    constexpr ipos32 hex_center {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    ipos32 raw_hex_pos = GeometryHelper::GetHexOffset(map->GetScreenRawHex(), rawHex);
    ipos32 center_screen_pos = map->MapToScreenPos(raw_hex_pos + hex_center);
    isize32 screen_size = map->GetScreenSize();
    ipos32 screen_center {screen_size.width / 2, screen_size.height / 2};
    fpos32 correction = fpos32(center_screen_pos - screen_center) / std::max(map->GetSpritesZoom(), 0.001f);

    map->InstantScroll(correction);
    map->RebuildMap();
}

static auto RequireCurMapperMap(ptr<MapperEngine> mapper_ptr) -> ptr<MapView>
{
    FO_STACK_TRACE_ENTRY();

    auto map = mapper_ptr->GetCurMap();

    if (!map) {
        throw ScriptException("No current map shown in the mapper");
    }

    return map;
}

///@ ExportMethod
FO_SCRIPT_API ptr<ItemView> Mapper_Game_AddItem(ptr<MapperEngine> mapper, hstring pid, mpos hex)
{
    auto map = RequireCurMapperMap(mapper);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    auto item = mapper->CreateItem(pid, hex, nullptr);
    return item;
}

///@ ExportMethod
FO_SCRIPT_API ptr<ItemView> Mapper_Game_AddItem(ptr<MapperEngine> mapper, ptr<ProtoItem> proto, mpos hex)
{
    auto map = RequireCurMapperMap(mapper);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    auto item = mapper->CreateItem(proto->GetProtoId(), hex, nullptr);
    return item;
}

///@ ExportMethod
FO_SCRIPT_API ptr<CritterView> Mapper_Game_AddCritter(ptr<MapperEngine> mapper, hstring pid, mpos hex)
{
    auto map = RequireCurMapperMap(mapper);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    auto cr = mapper->CreateCritter(pid, hex);
    return cr;
}

///@ ExportMethod
FO_SCRIPT_API ptr<CritterView> Mapper_Game_AddCritter(ptr<MapperEngine> mapper, ptr<ProtoCritter> proto, mpos hex)
{
    auto map = RequireCurMapperMap(mapper);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    auto cr = mapper->CreateCritter(proto->GetProtoId(), hex);
    return cr;
}

///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Mapper_Game_GetItemOnHex(ptr<MapperEngine> mapper, mpos hex)
{
    auto map = RequireCurMapperMap(mapper);
    auto item = map->GetItemOnHex(hex, hstring());
    return item;
}

///@ ExportMethod
FO_SCRIPT_API vector<ptr<ItemView>> Mapper_Game_GetItemsOnHex(ptr<MapperEngine> mapper, mpos hex)
{
    auto map = RequireCurMapperMap(mapper);
    span<ptr<ItemHexView>> hex_items = map->GetItemsOnHex(hex);
    return vector<ptr<ItemView>>(hex_items.begin(), hex_items.end());
}

///@ ExportMethod
FO_SCRIPT_API nptr<CritterView> Mapper_Game_GetCritterOnHex(ptr<MapperEngine> mapper, mpos hex, CritterFindType findType)
{
    auto map = RequireCurMapperMap(mapper);
    vector<ptr<CritterHexView>> critters = map->GetCrittersOnHex(hex, findType);

    if (critters.empty()) {
        return nullptr;
    }

    auto cr = critters.front();
    return cr;
}

///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Mapper_Game_GetCrittersOnHex(ptr<MapperEngine> mapper, mpos hex, CritterFindType findType)
{
    auto map = RequireCurMapperMap(mapper);
    vector<ptr<CritterHexView>> critters = map->GetCrittersOnHex(hex, findType);
    return vector<ptr<CritterView>>(critters.begin(), critters.end());
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_MoveEntity(ptr<MapperEngine> mapper, nptr<ClientEntity> entity, mpos hex)
{
    if (!entity) {
        return;
    }

    auto map = RequireCurMapperMap(mapper);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    mapper->MoveEntity(entity, hex);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_DeleteEntity(ptr<MapperEngine> mapper, ptr<ClientEntity> entity)
{
    mapper->DeleteEntity(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_DeleteEntities(ptr<MapperEngine> mapper, readonly_vector<nptr<ClientEntity>> entities)
{
    for (nptr<ClientEntity> entity : entities) {
        if (!entity) {
            continue;
        }

        if (!entity->IsDestroyed()) {
            mapper->DeleteEntity(entity);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SelectEntity(ptr<MapperEngine> mapper, ptr<ClientEntity> entity, bool set)
{
    if (set) {
        mapper->SelectAdd(entity);
    }
    else {
        mapper->SelectRemove(entity);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SelectEntities(ptr<MapperEngine> mapper, readonly_vector<nptr<ClientEntity>> entities, bool set)
{
    for (nptr<ClientEntity> entity : entities) {
        if (entity) {
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
FO_SCRIPT_API nptr<ClientEntity> Mapper_Game_GetSelectedEntity(ptr<MapperEngine> mapper)
{
    return !mapper->SelectedEntities.empty() ? mapper->SelectedEntities[0].get() : nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<ptr<ClientEntity>> Mapper_Game_GetSelectedEntities(ptr<MapperEngine> mapper)
{
    vector<ptr<ClientEntity>> entities;
    entities.reserve(mapper->SelectedEntities.size());

    for (size_t i = 0; i < mapper->SelectedEntities.size(); i++) {
        entities.emplace_back(mapper->SelectedEntities[i]);
    }

    return entities;
}

///@ ExportMethod
FO_SCRIPT_API nptr<ClientEntity> Mapper_Game_FindEntityById(ptr<MapperEngine> mapper, ident_t id)
{
    auto map = RequireCurMapperMap(mapper);

    return mapper->FindEntityById(map, id);
}

///@ ExportMethod
FO_SCRIPT_API bool Mapper_Game_SetEntityProperty(ptr<MapperEngine> mapper, ptr<ClientEntity> entity, string_view propName, string_view valueText)
{
    auto prop = entity->GetProperties()->GetRegistrator()->FindProperty(propName);

    if (!prop) {
        throw ScriptException("Unknown property", propName);
    }

    return mapper->ApplyEntityPropertyText(entity, prop, valueText);
}

///@ ExportMethod
FO_SCRIPT_API ptr<ItemView> Mapper_Game_AddTile(ptr<MapperEngine> mapper, hstring pid, mpos hex, int32_t layer, bool roof)
{
    auto map = RequireCurMapperMap(mapper);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }
    if (!mapper->GetProtoItem(pid)) {
        throw ScriptException("Invalid item proto", pid);
    }

    auto corrected_layer = numeric_cast<uint8_t>(std::clamp(layer, 0, 4));

    auto tile = map->AddMapperTile(pid, hex, corrected_layer, roof);
    return tile;
}

///@ ExportMethod
FO_SCRIPT_API nptr<MapView> Mapper_Game_NewMap(ptr<MapperEngine> mapper, string_view name, int32_t width, int32_t height)
{
    if (name.empty()) {
        throw ScriptException("Map name is empty");
    }

    int32_t corrected_width = std::clamp(width, GameSettings::MIN_MAP_SIZE, GameSettings::MAX_MAP_SIZE);
    int32_t corrected_height = std::clamp(height, GameSettings::MIN_MAP_SIZE, GameSettings::MAX_MAP_SIZE);

    string map_text = strex("[ProtoMap]\nSize = {} {}\nWorkHex = {} {}\n", //
        corrected_width, corrected_height, corrected_width / 2, corrected_height / 2)
                          .str();

    return mapper->LoadMapFromText(name, name, map_text);
}

///@ ExportMethod
FO_SCRIPT_API nptr<MapView> Mapper_Game_NewMapFromText(ptr<MapperEngine> mapper, string_view name, string_view text)
{
    if (name.empty()) {
        throw ScriptException("Map name is empty");
    }
    if (text.find("[ProtoMap]") == string_view::npos) {
        throw ScriptException("Map text has no [ProtoMap] section");
    }

    return mapper->LoadMapFromText(name, name, string(text));
}

///@ ExportMethod
FO_SCRIPT_API nptr<MapView> Mapper_Game_LoadMap(ptr<MapperEngine> mapper, string_view fileName)
{
    auto map = mapper->LoadMap(fileName);
    return map;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_UnloadMap(ptr<MapperEngine> mapper, ptr<MapView> map)
{
    mapper->UnloadMap(map);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SaveMap(ptr<MapperEngine> mapper, ptr<MapView> map, string_view customName)
{
    mapper->SaveMap(map, customName);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SaveMapToPath(ptr<MapperEngine> mapper, ptr<MapView> map, string_view subDir, string_view name)
{
    // Sandbox-disciplined save into <MapsRoot>/<subDir>/<name>.<ext> (subDir defaults to the
    // AI authoring area "Generated" at the caller). Refuse path separators in the name and any
    // ".." traversal so an authoring agent cannot escape the Maps tree.
    if (name.empty()) {
        throw ScriptException("Map name is empty");
    }
    if (name.find('/') != string_view::npos || name.find('\\') != string_view::npos) {
        throw ScriptException("Map name must not contain path separators", name);
    }
    if (subDir.find("..") != string_view::npos || name.find("..") != string_view::npos) {
        throw ScriptException("Path traversal is not allowed");
    }

    mapper->SaveMapToDir(map, subDir, name);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_ShowMap(ptr<MapperEngine> mapper, ptr<MapView> map)
{
    mapper->ShowMap(map);
}

///@ ExportMethod
FO_SCRIPT_API vector<ptr<MapView>> Mapper_Game_GetLoadedMaps(ptr<MapperEngine> mapper, int32_t& index)
{
    index = -1;

    auto cur_map = mapper->GetCurMap();

    for (int32_t i = 0, j = numeric_cast<int32_t>(mapper->LoadedMaps.size()); i < j; i++) {
        if (cur_map && mapper->LoadedMaps[i] == cur_map) {
            index = i;
        }
    }

    vector<ptr<MapView>> result;
    result.reserve(mapper->LoadedMaps.size());

    for (size_t i = 0; i < mapper->LoadedMaps.size(); i++) {
        result.emplace_back(mapper->LoadedMaps[i]);
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<string> Mapper_Game_GetMapFileNames(ptr<MapperEngine> mapper, string_view dir, bool recursive)
{
    vector<string> names;

    auto map_files = mapper->MapsFileSys.FilterFiles("", dir, recursive);

    for (const auto& map_file_header : map_files) {
        string ext = strex(map_file_header.GetPath()).get_file_extension();

        if (std::ranges::find(mapper->Settings->ProtoFileExtensions, ext) == mapper->Settings->ProtoFileExtensions.end()) {
            continue;
        }

        File map_file = File::Load(map_file_header);
        auto declared_maps = MapLoader::EnumerateMaps(map_file.GetPath(), map_file.GetStr());
        names.insert(names.end(), std::make_move_iterator(declared_maps.begin()), std::make_move_iterator(declared_maps.end()));
    }

    return names;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_ResizeMap(ptr<MapperEngine> mapper, int32_t width, int32_t height)
{
    auto map = RequireCurMapperMap(mapper);
    mapper->ResizeMap(map.get(), width, height);
}

///@ ExportMethod
FO_SCRIPT_API vector<hstring> Mapper_Game_TabGetItemPids(ptr<MapperEngine> mapper, int32_t tab, string_view subTab)
{
    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && mapper->Tabs[tab].count(string(subTab)) == 0) {
        return {};
    }

    vector<hstring> pids;
    const auto& stab = mapper->Tabs[tab][!subTab.empty() ? string(subTab) : MapperEngine::DEFAULT_SUB_TAB];

    for (ptr<const ProtoItem> proto : stab.ItemProtos) {
        pids.emplace_back(proto->GetProtoId());
    }

    return pids;
}

///@ ExportMethod
FO_SCRIPT_API vector<hstring> Mapper_Game_TabGetCritterPids(ptr<MapperEngine> mapper, int32_t tab, string_view subTab)
{
    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && mapper->Tabs[tab].count(string(subTab)) == 0) {
        return {};
    }

    vector<hstring> pids;
    const auto& stab = mapper->Tabs[tab][!subTab.empty() ? string(subTab) : MapperEngine::DEFAULT_SUB_TAB];

    for (ptr<const ProtoCritter> proto : stab.CritterProtos) {
        pids.emplace_back(proto->GetProtoId());
    }

    return pids;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSetItemPids(ptr<MapperEngine> mapper, int32_t tab, string_view subTab, readonly_vector<hstring> itemPids)
{
    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    if (subTab.empty() || subTab == MapperEngine::DEFAULT_SUB_TAB) {
        return;
    }

    // Add protos to sub tab
    if (!itemPids.empty()) {
        vector<ptr<const ProtoItem>> protos;

        for (auto item_pid : itemPids) {
            auto proto = mapper->GetProtoItem(item_pid);

            if (proto) {
                protos.emplace_back(proto);
            }
        }

        auto& stab = mapper->Tabs[tab][string(subTab)];
        stab.ItemProtos = protos;
    }
    else {
        // Delete sub tab
        auto it = mapper->Tabs[tab].find(string(subTab));

        if (it != mapper->Tabs[tab].end()) {
            if (mapper->ActiveSubTabs[tab] == &it->second) {
                mapper->ActiveSubTabs[tab] = nullptr;
            }

            mapper->Tabs[tab].erase(it);
        }
    }

    // Recalculate whole pids
    auto& stab_default = mapper->Tabs[tab][MapperEngine::DEFAULT_SUB_TAB];
    stab_default.ItemProtos.clear();

    for (auto& stab : mapper->Tabs[tab] | std::views::values) {
        if (&stab == &stab_default) {
            continue;
        }

        for (ptr<const ProtoItem> proto : stab.ItemProtos) {
            stab_default.ItemProtos.emplace_back(proto);
        }
    }

    if (!mapper->ActiveSubTabs[tab]) {
        mapper->ActiveSubTabs[tab] = &stab_default;
    }

    mapper->RefreshActiveProtoLists();
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSetCritterPids(ptr<MapperEngine> mapper, int32_t tab, string_view subTab, readonly_vector<hstring> critterPids)
{
    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    if (subTab.empty() || subTab == MapperEngine::DEFAULT_SUB_TAB) {
        return;
    }

    // Add protos to sub tab
    if (!critterPids.empty()) {
        vector<ptr<const ProtoCritter>> protos;

        for (auto pid : critterPids) {
            auto proto = mapper->GetProtoCritter(pid);
            FO_VERIFY_AND_THROW(proto, "Unknown critter proto id for sub tab");
            protos.emplace_back(proto);
        }

        if (!protos.empty()) {
            auto& stab = mapper->Tabs[tab][string(subTab)];
            stab.CritterProtos = protos;
        }
    }
    else {
        // Delete sub tab
        auto it = mapper->Tabs[tab].find(string(subTab));
        if (it != mapper->Tabs[tab].end()) {
            if (mapper->ActiveSubTabs[tab] == &it->second) {
                mapper->ActiveSubTabs[tab] = nullptr;
            }

            mapper->Tabs[tab].erase(it);
        }
    }

    // Recalculate whole pids
    auto& stab_default = mapper->Tabs[tab][MapperEngine::DEFAULT_SUB_TAB];
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

    if (!mapper->ActiveSubTabs[tab]) {
        mapper->ActiveSubTabs[tab] = &stab_default;
    }

    // Refresh
    mapper->RefreshActiveProtoLists();
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabDelete(ptr<MapperEngine> mapper, int32_t tab)
{
    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    mapper->Tabs[tab].clear();
    auto& stab_default = mapper->Tabs[tab][MapperEngine::DEFAULT_SUB_TAB];
    mapper->ActiveSubTabs[tab] = &stab_default;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSelect(ptr<MapperEngine> mapper, int32_t tab, string_view subTab, bool show)
{
    if (tab < 0 || tab >= MapperEngine::INT_MODE_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    if (show) {
        mapper->SetActivePanelMode(tab);
    }

    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        return;
    }

    auto it = mapper->Tabs[tab].find(!subTab.empty() ? string(subTab) : MapperEngine::DEFAULT_SUB_TAB);
    if (it != mapper->Tabs[tab].end()) {
        mapper->ActiveSubTabs[tab] = &it->second;
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSetName(ptr<MapperEngine> mapper, int32_t tab, string_view tabName)
{
    if (tab < 0 || tab >= MapperEngine::INT_MODE_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    mapper->PanelModeNames[tab] = tabName;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_AddMessage(ptr<MapperEngine> mapper, string_view message)
{
    mapper->AddMess(message);
}

///@ ExportMethod
FO_SCRIPT_API msize Mapper_Game_GetCurMapHexSize(ptr<MapperEngine> mapper)
{
    auto map = RequireCurMapperMap(mapper);

    return map->GetSize();
}

///@ ExportMethod
FO_SCRIPT_API isize32 Mapper_Game_GetCurMapPixelSize(ptr<MapperEngine> mapper)
{
    auto map = RequireCurMapperMap(mapper);

    auto hex_size = map->GetSize();
    int32_t width = numeric_cast<int32_t>(hex_size.width) * GameSettings::MAP_HEX_WIDTH;
    int32_t height = numeric_cast<int32_t>(hex_size.height) * GameSettings::MAP_HEX_LINE_HEIGHT + (GameSettings::MAP_HEX_HEIGHT - GameSettings::MAP_HEX_LINE_HEIGHT);
    return {width, height};
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperViewSize(ptr<MapperEngine> mapper, isize32 size)
{
    if (size.width <= 0 || size.height <= 0) {
        throw ScriptException("View size must be positive", size.width, size.height);
    }

    auto map = RequireCurMapperMap(mapper);

    map->SetScreenSize(size);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_CenterMapperOnPlayableArea(ptr<MapperEngine> mapper)
{
    auto map = RequireCurMapperMap(mapper);

    if (irect32 area = map->GetScrollAxialArea(); !area.is_zero()) {
        // Axial -> pixel center, then resolve back to a raw hex without clamping to the
        // authored map rectangle. ScrollAxialArea may intentionally extend into negative
        // axial space, and preview captures need to align to that editor boundary.
        int32_t axial_cx = area.x + area.width / 2;
        int32_t axial_cy = area.y + area.height / 2;
        ipos32 pixel_center {axial_cx * (GameSettings::MAP_HEX_WIDTH / 2), axial_cy * GameSettings::MAP_HEX_LINE_HEIGHT};
        ipos32 raw_hex = GeometryHelper::GetHexPosCoord(pixel_center);
        CenterMapperViewOnRawHex(map, raw_hex);
    }
    else {
        auto map_size = map->GetSize();
        mpos center_hex {numeric_cast<int16_t>(map_size.width / 2), numeric_cast<int16_t>(map_size.height / 2)};
        CenterMapperViewOnHex(map, center_hex);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_CenterMapperOnHex(ptr<MapperEngine> mapper, mpos hex)
{
    auto map = RequireCurMapperMap(mapper);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Hex is outside the map", hex.x, hex.y);
    }

    CenterMapperViewOnHex(map, hex);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_CenterMapperOnRawHex(ptr<MapperEngine> mapper, ipos32 rawHex)
{
    auto map = RequireCurMapperMap(mapper);

    CenterMapperViewOnRawHex(map, rawHex);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperOverlayVisible(ptr<MapperEngine> mapper, bool visible)
{
    auto map = RequireCurMapperMap(mapper);

    map->SetShowMapperOverlay(visible);
}

///@ ExportMethod
FO_SCRIPT_API bool Mapper_Game_IsMapperOverlayVisible(ptr<MapperEngine> mapper)
{
    auto map = mapper->GetCurMap();

    if (!map) {
        return false;
    }

    return map->IsShowMapperOverlay();
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperHexOverlayVisible(ptr<MapperEngine> mapper, bool visible)
{
    mapper->SetMapperHexOverlayVisible(visible);
}

///@ ExportMethod
FO_SCRIPT_API bool Mapper_Game_IsMapperHexOverlayVisible(ptr<MapperEngine> mapper)
{
    return mapper->IsMapperHexOverlayVisible();
}

///@ ExportMethod
FO_SCRIPT_API vector<mpos> Mapper_Game_GetMapperTrackOverlayHexes(ptr<MapperEngine> mapper)
{
    return mapper->GetMapperTrackOverlayHexes();
}

///@ ExportMethod
FO_SCRIPT_API vector<int32_t> Mapper_Game_GetMapperTrackOverlayKinds(ptr<MapperEngine> mapper)
{
    return mapper->GetMapperTrackOverlayKinds();
}

///@ ExportMethod
FO_SCRIPT_API vector<mpos> Mapper_Game_GetMapperScrollBorderHexes(ptr<MapperEngine> mapper)
{
    auto map = mapper->GetCurMap();

    if (!map) {
        return {};
    }

    irect32 scroll_area = map->GetScrollAxialArea();

    if (scroll_area.is_zero()) {
        return {};
    }

    msize map_size = map->GetSize();
    vector<mpos> hexes;

    for (int32_t hy = 0; hy < map_size.height; hy++) {
        for (int32_t hx = 0; hx < map_size.width; hx++) {
            mpos hex = map_size.from_raw_pos(hx, hy);

            if (!map->IsHexToDraw(hex)) {
                continue;
            }

            ipos32 axial_hex = GeometryHelper::GetHexAxialCoord(hex);

            if (axial_hex.x == scroll_area.x || axial_hex.y == scroll_area.y || axial_hex.x == scroll_area.x + scroll_area.width || axial_hex.y == scroll_area.y + scroll_area.height) {
                hexes.emplace_back(hex);
            }
        }
    }

    return hexes;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperHiddenSpritesVisible(ptr<MapperEngine> mapper, bool visible)
{
    auto map = RequireCurMapperMap(mapper);

    map->SetShowMapperHiddenSprites(visible);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_AddMapperIgnoredItemPids(ptr<MapperEngine> mapper, readonly_vector<hstring> itemPids)
{
    auto map = RequireCurMapperMap(mapper);

    for (hstring item_pid : itemPids) {
        map->AddIgnorePid(item_pid);
    }

    map->RebuildMap();
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperScrollCheckEnabled(ptr<MapperEngine> mapper, bool enabled)
{
    auto map = RequireCurMapperMap(mapper);

    map->SetScrollCheck(enabled);
    map->RebuildMap();
}

///@ ExportMethod
FO_SCRIPT_API float32_t Mapper_Game_CalcMapperFitZoom(ptr<MapperEngine> mapper, isize32 viewportSize)
{
    if (viewportSize.width <= 0 || viewportSize.height <= 0) {
        throw ScriptException("Viewport size must be positive", viewportSize.width, viewportSize.height);
    }

    auto map = RequireCurMapperMap(mapper);

    // Pixel extents of the playable area: ScrollAxialArea (in axial coordinates) maps to
    // (axial_w * MAP_HEX_WIDTH/2) x (axial_h * MAP_HEX_LINE_HEIGHT) — same basis as the
    // engine's RefreshMinZoom. For maps without an explicit axial area fall back to the
    // map's hex bounding box.
    int32_t pixel_w;
    int32_t pixel_h;

    if (irect32 area = map->GetScrollAxialArea(); !area.is_zero()) {
        pixel_w = area.width * (GameSettings::MAP_HEX_WIDTH / 2);
        pixel_h = area.height * GameSettings::MAP_HEX_LINE_HEIGHT;
    }
    else {
        auto hex_size = map->GetSize();
        pixel_w = numeric_cast<int32_t>(hex_size.width) * GameSettings::MAP_HEX_WIDTH;
        pixel_h = numeric_cast<int32_t>(hex_size.height) * GameSettings::MAP_HEX_LINE_HEIGHT + (GameSettings::MAP_HEX_HEIGHT - GameSettings::MAP_HEX_LINE_HEIGHT);
    }

    float32_t zoom_x = numeric_cast<float32_t>(viewportSize.width) / numeric_cast<float32_t>(pixel_w);
    float32_t zoom_y = numeric_cast<float32_t>(viewportSize.height) / numeric_cast<float32_t>(pixel_h);
    return std::min(zoom_x, zoom_y);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperZoom(ptr<MapperEngine> mapper, float32_t zoom)
{
    if (!(zoom > 0.0f)) {
        throw ScriptException("Zoom must be positive", zoom);
    }

    auto map = RequireCurMapperMap(mapper);

    map->SetSpritesZoomTarget(zoom);
    map->InstantZoom(zoom, fpos32(0.0f, 0.0f));
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SaveMapperScreenshot(ptr<MapperEngine> mapper, string_view filePath)
{
    if (filePath.empty()) {
        throw ScriptException("Screenshot file path is empty");
    }

    auto map = RequireCurMapperMap(mapper);

    // The mapper's main window paints into the swap-chain backbuffer (no virtual RT) but
    // SpriteManager keeps an intermediate _rtMain that holds the full frame just before it
    // is blit out. Re-run the mapper's draw routine to refresh _rtMain, then read pixels
    // from there. Two paints per save is acceptable for batch tooling.
    mapper->DrawMapperFrame();

    auto main_rt = mapper->SprMngr.GetMainRenderTarget();

    if (!main_rt) {
        throw ScriptException("SpriteManager has no main render target (FO_DIRECT_SPRITES_DRAW build?)");
    }

    auto texture = main_rt->GetTexture();
    isize32 size = texture->Size;
    auto pixels = texture->GetTextureRegion({0, 0}, size);
    bool flipped = texture->FlippedHeight;

    if (flipped) {
        auto width = numeric_cast<size_t>(size.width);

        if (width != 0) {
            vector<ucolor> row_buf(width);
            size_t row_bytes = width * sizeof(ucolor);

            for (int32_t y = 0; y < size.height / 2; y++) {
                auto top = numeric_cast<size_t>(y) * width;
                auto bottom = numeric_cast<size_t>(size.height - 1 - y) * width;
                auto row = make_ptr(row_buf.data());
                auto top_row = make_ptr(pixels.data() + top);
                auto bottom_row = make_ptr(pixels.data() + bottom);
                MemCopy(row, top_row, row_bytes);
                MemCopy(top_row, bottom_row, row_bytes);
                MemCopy(bottom_row, row, row_bytes);
            }
        }
    }

    WriteSimpleTga(filePath, size, std::move(pixels));
}

FO_END_NAMESPACE
