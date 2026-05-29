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

FO_BEGIN_NAMESPACE

static void CenterMapperViewOnHex(MapView* map, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    map->InstantScrollTo(hex);

    constexpr ipos32 hex_center {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    const ipos32 hex_screen_pos = map->MapToScreenPos(map->GetHexMapPos(hex) + hex_center);
    const isize32 screen_size = map->GetScreenSize();
    const ipos32 screen_center {screen_size.width / 2, screen_size.height / 2};
    const fpos32 correction = fpos32(hex_screen_pos - screen_center) / std::max(map->GetSpritesZoom(), 0.001f);

    map->InstantScroll(correction);
    map->RebuildMap();
}

static void CenterMapperViewOnRawHex(MapView* map, ipos32 rawHex)
{
    FO_STACK_TRACE_ENTRY();

    const ipos32 new_screen_hex = map->ConvertToScreenRawHex(rawHex);
    const ipos32 offset_to_new_pos = GeometryHelper::GetHexOffset(map->GetScreenRawHex(), new_screen_hex);
    map->InstantScroll(fpos32(offset_to_new_pos));

    constexpr ipos32 hex_center {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    const ipos32 raw_hex_pos = GeometryHelper::GetHexOffset(map->GetScreenRawHex(), rawHex);
    const ipos32 center_screen_pos = map->MapToScreenPos(raw_hex_pos + hex_center);
    const isize32 screen_size = map->GetScreenSize();
    const ipos32 screen_center {screen_size.width / 2, screen_size.height / 2};
    const fpos32 correction = fpos32(center_screen_pos - screen_center) / std::max(map->GetSpritesZoom(), 0.001f);

    map->InstantScroll(correction);
    map->RebuildMap();
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Mapper_Game_AddItem(MapperEngine* mapper, hstring pid, mpos hex)
{
    if (!mapper->GetCurMap()->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return mapper->CreateItem(pid, hex, nullptr);
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Mapper_Game_AddItem(MapperEngine* mapper, ProtoItem* proto, mpos hex)
{
    if (!mapper->GetCurMap()->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return mapper->CreateItem(proto->GetProtoId(), hex, nullptr);
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Mapper_Game_AddCritter(MapperEngine* mapper, hstring pid, mpos hex)
{
    if (!mapper->GetCurMap()->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return mapper->CreateCritter(pid, hex);
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Mapper_Game_AddCritter(MapperEngine* mapper, ProtoCritter* proto, mpos hex)
{
    if (!mapper->GetCurMap()->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return mapper->CreateCritter(proto->GetProtoId(), hex);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE ItemView* Mapper_Game_GetItemOnHex(MapperEngine* mapper, mpos hex)
{
    return mapper->GetCurMap()->GetItemOnHex(hex, hstring());
}

///@ ExportMethod
FO_SCRIPT_API vector<ItemView*> Mapper_Game_GetItemsOnHex(MapperEngine* mapper, mpos hex)
{
    const auto hex_items = mapper->GetCurMap()->GetItemsOnHex(hex);
    return vec_transform(hex_items, [](auto&& item) -> ItemView* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Mapper_Game_GetCritterOnHex(MapperEngine* mapper, mpos hex, CritterFindType findType)
{
    const auto critters = mapper->GetCurMap()->GetCrittersOnHex(hex, findType);
    return !critters.empty() ? critters.front() : nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Mapper_Game_GetCrittersOnHex(MapperEngine* mapper, mpos hex, CritterFindType findType)
{
    return vec_transform(mapper->GetCurMap()->GetCrittersOnHex(hex, findType), [](auto&& cr) -> CritterView* { return cr; });
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_MoveEntity(MapperEngine* mapper, FO_NULLABLE ClientEntity* entity, mpos hex)
{
    if (!mapper->GetCurMap()->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    mapper->MoveEntity(entity, hex);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_DeleteEntity(MapperEngine* mapper, ClientEntity* entity)
{
    mapper->DeleteEntity(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_DeleteEntities(MapperEngine* mapper, readonly_vector<ClientEntity*> entities)
{
    for (auto* entity : entities) {
        if (entity != nullptr && !entity->IsDestroyed()) {
            mapper->DeleteEntity(entity);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SelectEntity(MapperEngine* mapper, ClientEntity* entity, bool set)
{
    if (set) {
        mapper->SelectAdd(entity);
    }
    else {
        mapper->SelectRemove(entity);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SelectEntities(MapperEngine* mapper, readonly_vector<ClientEntity*> entities, bool set)
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
FO_SCRIPT_API ClientEntity* Mapper_Game_GetSelectedEntity(MapperEngine* mapper)
{
    return !mapper->SelectedEntities.empty() ? mapper->SelectedEntities[0].get() : nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<ClientEntity*> Mapper_Game_GetSelectedEntities(MapperEngine* mapper)
{
    vector<ClientEntity*> entities;
    entities.reserve(mapper->SelectedEntities.size());

    for (auto& entity : mapper->SelectedEntities) {
        entities.emplace_back(entity.get());
    }

    return entities;
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Mapper_Game_AddTile(MapperEngine* mapper, hstring pid, mpos hex, int32_t layer, bool roof)
{
    if (mapper->GetCurMap() == nullptr) {
        throw ScriptException("Map not loaded");
    }
    if (!mapper->GetCurMap()->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }
    if (mapper->GetProtoItem(pid) == nullptr) {
        throw ScriptException("Invalid item proto", pid);
    }

    const auto corrected_layer = numeric_cast<uint8_t>(std::clamp(layer, 0, 4));

    return mapper->GetCurMap()->AddMapperTile(pid, hex, corrected_layer, roof);
}

///@ ExportMethod
FO_SCRIPT_API MapView* Mapper_Game_LoadMap(MapperEngine* mapper, string_view fileName)
{
    return mapper->LoadMap(fileName);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_UnloadMap(MapperEngine* mapper, MapView* map)
{
    mapper->UnloadMap(map);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SaveMap(MapperEngine* mapper, MapView* map, string_view customName)
{
    mapper->SaveMap(map, customName);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_ShowMap(MapperEngine* mapper, MapView* map)
{
    mapper->ShowMap(map);
}

///@ ExportMethod
FO_SCRIPT_API vector<MapView*> Mapper_Game_GetLoadedMaps(MapperEngine* mapper, int32_t& index)
{
    index = -1;

    for (int32_t i = 0, j = numeric_cast<int32_t>(mapper->LoadedMaps.size()); i < j; i++) {
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
FO_SCRIPT_API vector<string> Mapper_Game_GetMapFileNames(MapperEngine* mapper, string_view dir, bool recursive)
{
    vector<string> names;

    auto map_files = mapper->MapsFileSys.FilterFiles("fomap", dir, recursive);

    for (const auto& map_file_header : map_files) {
        names.emplace_back(map_file_header.GetNameNoExt());
    }

    return names;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_ResizeMap(MapperEngine* mapper, int32_t width, int32_t height)
{
    if (mapper->GetCurMap() == nullptr) {
        throw ScriptException("Map not loaded");
    }

    mapper->ResizeMap(mapper->GetCurMap(), width, height);
}

///@ ExportMethod
FO_SCRIPT_API vector<hstring> Mapper_Game_TabGetItemPids(MapperEngine* mapper, int32_t tab, string_view subTab)
{
    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && mapper->Tabs[tab].count(string(subTab)) == 0) {
        return {};
    }

    vector<hstring> pids;
    const auto& stab = mapper->Tabs[tab][!subTab.empty() ? string(subTab) : MapperEngine::DEFAULT_SUB_TAB];

    for (const auto& proto : stab.ItemProtos) {
        pids.emplace_back(proto->GetProtoId());
    }

    return pids;
}

///@ ExportMethod
FO_SCRIPT_API vector<hstring> Mapper_Game_TabGetCritterPids(MapperEngine* mapper, int32_t tab, string_view subTab)
{
    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }
    if (!subTab.empty() && mapper->Tabs[tab].count(string(subTab)) == 0) {
        return {};
    }

    vector<hstring> pids;
    const auto& stab = mapper->Tabs[tab][!subTab.empty() ? string(subTab) : MapperEngine::DEFAULT_SUB_TAB];

    for (const auto& proto : stab.CritterProtos) {
        pids.emplace_back(proto->GetProtoId());
    }

    return pids;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSetItemPids(MapperEngine* mapper, int32_t tab, string_view subTab, readonly_vector<hstring> itemPids)
{
    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    if (subTab.empty() || subTab == MapperEngine::DEFAULT_SUB_TAB) {
        return;
    }

    // Add protos to sub tab
    if (!itemPids.empty()) {
        vector<raw_ptr<const ProtoItem>> protos;

        for (const auto item_pid : itemPids) {
            const auto* proto = mapper->GetProtoItem(item_pid);

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

        for (auto& proto : stab.ItemProtos) {
            stab_default.ItemProtos.emplace_back(proto);
        }
    }

    if (mapper->ActiveSubTabs[tab] == nullptr) {
        mapper->ActiveSubTabs[tab] = &stab_default;
    }

    mapper->RefreshActiveProtoLists();
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSetCritterPids(MapperEngine* mapper, int32_t tab, string_view subTab, readonly_vector<hstring> critterPids)
{
    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    if (subTab.empty() || subTab == MapperEngine::DEFAULT_SUB_TAB) {
        return;
    }

    // Add protos to sub tab
    if (!critterPids.empty()) {
        vector<raw_ptr<const ProtoCritter>> protos;

        for (const auto pid : critterPids) {
            const auto* proto = mapper->GetProtoCritter(pid);
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

    if (mapper->ActiveSubTabs[tab] == nullptr) {
        mapper->ActiveSubTabs[tab] = &stab_default;
    }

    // Refresh
    mapper->RefreshActiveProtoLists();
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabDelete(MapperEngine* mapper, int32_t tab)
{
    if (tab < 0 || tab >= MapperEngine::TAB_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    mapper->Tabs[tab].clear();
    auto& stab_default = mapper->Tabs[tab][MapperEngine::DEFAULT_SUB_TAB];
    mapper->ActiveSubTabs[tab] = &stab_default;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSelect(MapperEngine* mapper, int32_t tab, string_view subTab, bool show)
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

    const auto it = mapper->Tabs[tab].find(!subTab.empty() ? string(subTab) : MapperEngine::DEFAULT_SUB_TAB);
    if (it != mapper->Tabs[tab].end()) {
        mapper->ActiveSubTabs[tab] = &it->second;
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_TabSetName(MapperEngine* mapper, int32_t tab, string_view tabName)
{
    if (tab < 0 || tab >= MapperEngine::INT_MODE_COUNT) {
        throw ScriptException("Wrong tab arg");
    }

    mapper->PanelModeNames[tab] = tabName;
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_AddMessage(MapperEngine* mapper, string_view message)
{
    mapper->AddMess(message);
}

///@ ExportMethod
FO_SCRIPT_API msize Mapper_Game_GetCurMapHexSize(MapperEngine* mapper)
{
    FO_STACK_TRACE_ENTRY();

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    return map->GetSize();
}

///@ ExportMethod
FO_SCRIPT_API isize32 Mapper_Game_GetCurMapPixelSize(MapperEngine* mapper)
{
    FO_STACK_TRACE_ENTRY();

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    const auto hex_size = map->GetSize();
    const auto width = numeric_cast<int32_t>(hex_size.width) * GameSettings::MAP_HEX_WIDTH;
    const auto height = numeric_cast<int32_t>(hex_size.height) * GameSettings::MAP_HEX_LINE_HEIGHT + (GameSettings::MAP_HEX_HEIGHT - GameSettings::MAP_HEX_LINE_HEIGHT);
    return {width, height};
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperViewSize(MapperEngine* mapper, isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    if (size.width <= 0 || size.height <= 0) {
        throw ScriptException("View size must be positive", size.width, size.height);
    }

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    map->SetScreenSize(size);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_CenterMapperOnPlayableArea(MapperEngine* mapper)
{
    FO_STACK_TRACE_ENTRY();

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    if (const irect32 area = map->GetScrollAxialArea(); !area.is_zero()) {
        // Axial -> pixel center, then resolve back to a raw hex without clamping to the
        // authored map rectangle. ScrollAxialArea may intentionally extend into negative
        // axial space, and preview captures need to align to that editor boundary.
        const int32_t axial_cx = area.x + area.width / 2;
        const int32_t axial_cy = area.y + area.height / 2;
        const ipos32 pixel_center {axial_cx * (GameSettings::MAP_HEX_WIDTH / 2), axial_cy * GameSettings::MAP_HEX_LINE_HEIGHT};
        const ipos32 raw_hex = GeometryHelper::GetHexPosCoord(pixel_center);
        CenterMapperViewOnRawHex(map, raw_hex);
    }
    else {
        const auto map_size = map->GetSize();
        const mpos center_hex {numeric_cast<int16_t>(map_size.width / 2), numeric_cast<int16_t>(map_size.height / 2)};
        CenterMapperViewOnHex(map, center_hex);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_CenterMapperOnHex(MapperEngine* mapper, mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Hex is outside the map", hex.x, hex.y);
    }

    CenterMapperViewOnHex(map, hex);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_CenterMapperOnRawHex(MapperEngine* mapper, ipos32 rawHex)
{
    FO_STACK_TRACE_ENTRY();

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    CenterMapperViewOnRawHex(map, rawHex);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperOverlayVisible(MapperEngine* mapper, bool visible)
{
    FO_STACK_TRACE_ENTRY();

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    map->SetShowMapperOverlay(visible);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperHiddenSpritesVisible(MapperEngine* mapper, bool visible)
{
    FO_STACK_TRACE_ENTRY();

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    map->SetShowMapperHiddenSprites(visible);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_AddMapperIgnoredItemPids(MapperEngine* mapper, readonly_vector<hstring> itemPids)
{
    FO_STACK_TRACE_ENTRY();

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    for (const hstring item_pid : itemPids) {
        map->AddIgnorePid(item_pid);
    }

    map->RebuildMap();
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperScrollCheckEnabled(MapperEngine* mapper, bool enabled)
{
    FO_STACK_TRACE_ENTRY();

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    map->SetScrollCheck(enabled);
    map->RebuildMap();
}

///@ ExportMethod
FO_SCRIPT_API float32_t Mapper_Game_CalcMapperFitZoom(MapperEngine* mapper, isize32 viewportSize)
{
    FO_STACK_TRACE_ENTRY();

    if (viewportSize.width <= 0 || viewportSize.height <= 0) {
        throw ScriptException("Viewport size must be positive", viewportSize.width, viewportSize.height);
    }

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    // Pixel extents of the playable area: ScrollAxialArea (in axial coordinates) maps to
    // (axial_w * MAP_HEX_WIDTH/2) x (axial_h * MAP_HEX_LINE_HEIGHT) — same basis as the
    // engine's RefreshMinZoom. For maps without an explicit axial area fall back to the
    // map's hex bounding box.
    int32_t pixel_w;
    int32_t pixel_h;

    if (const irect32 area = map->GetScrollAxialArea(); !area.is_zero()) {
        pixel_w = area.width * (GameSettings::MAP_HEX_WIDTH / 2);
        pixel_h = area.height * GameSettings::MAP_HEX_LINE_HEIGHT;
    }
    else {
        const auto hex_size = map->GetSize();
        pixel_w = numeric_cast<int32_t>(hex_size.width) * GameSettings::MAP_HEX_WIDTH;
        pixel_h = numeric_cast<int32_t>(hex_size.height) * GameSettings::MAP_HEX_LINE_HEIGHT + (GameSettings::MAP_HEX_HEIGHT - GameSettings::MAP_HEX_LINE_HEIGHT);
    }

    const float32_t zoom_x = numeric_cast<float32_t>(viewportSize.width) / numeric_cast<float32_t>(pixel_w);
    const float32_t zoom_y = numeric_cast<float32_t>(viewportSize.height) / numeric_cast<float32_t>(pixel_h);
    return std::min(zoom_x, zoom_y);
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SetMapperZoom(MapperEngine* mapper, float32_t zoom)
{
    FO_STACK_TRACE_ENTRY();

    if (!(zoom > 0.0f)) {
        throw ScriptException("Zoom must be positive", zoom);
    }

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    map->SetSpritesZoomTarget(zoom);
    map->InstantZoom(zoom, fpos32(0.0f, 0.0f));
}

///@ ExportMethod
FO_SCRIPT_API void Mapper_Game_SaveMapperScreenshot(MapperEngine* mapper, string_view filePath)
{
    FO_STACK_TRACE_ENTRY();

    if (filePath.empty()) {
        throw ScriptException("Screenshot file path is empty");
    }

    auto* map = mapper->GetCurMap();

    if (map == nullptr) {
        throw ScriptException("No current map shown in the mapper");
    }

    // The mapper's main window paints into the swap-chain backbuffer (no virtual RT) but
    // SpriteManager keeps an intermediate _rtMain that holds the full frame just before it
    // is blit out. Re-run the mapper's draw routine to refresh _rtMain, then read pixels
    // from there. Two paints per save is acceptable for batch tooling.
    mapper->DrawMapperFrame();

    auto* main_rt = mapper->SprMngr.GetMainRenderTarget();

    if (main_rt == nullptr) {
        throw ScriptException("SpriteManager has no main render target (FO_DIRECT_SPRITES_DRAW build?)");
    }

    auto* texture = main_rt->GetTexture();
    const auto size = texture->Size;
    auto pixels = texture->GetTextureRegion({0, 0}, size);
    const bool flipped = texture->FlippedHeight;

    if (flipped) {
        const auto width = numeric_cast<size_t>(size.width);
        vector<ucolor> row_buf(width);

        for (int32_t y = 0; y < size.height / 2; y++) {
            const auto top = numeric_cast<size_t>(y) * width;
            const auto bottom = numeric_cast<size_t>(size.height - 1 - y) * width;
            MemCopy(row_buf.data(), pixels.data() + top, width * sizeof(ucolor));
            MemCopy(pixels.data() + top, pixels.data() + bottom, width * sizeof(ucolor));
            MemCopy(pixels.data() + bottom, row_buf.data(), width * sizeof(ucolor));
        }
    }

    WriteSimpleTga(filePath, size, std::move(pixels));
}

FO_END_NAMESPACE
