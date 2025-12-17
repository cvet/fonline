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

#include "Client.h"
#include "Geometry.h"
#include "MapSprite.h"

FO_BEGIN_NAMESPACE();

///@ ExportMethod
FO_SCRIPT_API void Client_Map_DrawMap(MapView* self)
{
    if (!self->GetEngine()->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    self->DrawMap();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_DrawMapSprite(MapView* self, MapSpriteHolder* mapSpr)
{
    if (mapSpr == nullptr) {
        throw ScriptException("Map sprite arg is null");
    }

    if (!self->GetSize().is_valid_pos(mapSpr->Hex)) {
        return;
    }
    if (!self->IsHexToDraw(mapSpr->Hex)) {
        return;
    }

    const auto* anim = self->GetEngine()->AnimGetSpr(mapSpr->SprId);

    if (anim == nullptr) {
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

    if (mapSpr->ProtoId) {
        const auto* proto = self->GetEngine()->ProtoMngr.GetProtoItem(mapSpr->ProtoId);
        color = proto->GetColorize() ? proto->GetColorizeColor() : ucolor::clear;
        is_flat = proto->GetDrawFlatten();
        const auto is_item = proto->GetIsScenery() || proto->GetIsWall();
        no_light = is_flat && !is_item;
        draw_order = is_flat ? (is_item ? DrawOrderType::FlatItem : DrawOrderType::FlatScenery) : (is_item ? DrawOrderType::Item : DrawOrderType::Scenery);
        draw_order_hy_offset = numeric_cast<int32>(proto->GetDrawOrderOffsetHexY());
        corner = proto->GetCorner();
        disable_egg = proto->GetDisableEgg();
        contour_color = proto->GetBadItem() ? ucolor {255, 0, 0} : ucolor::clear;
    }

    auto* mspr = self->AddMapSprite(anim, mapSpr->Hex, draw_order, draw_order_hy_offset, //
        mapSpr->Offset, mapSpr->IsTweakOffs ? &mapSpr->TweakOffset : nullptr, //
        mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, &mapSpr->Valid);

    mapSpr->MSpr = mspr;

    if (!no_light) {
        mspr->SetLight(corner, self->GetLightData(), self->GetSize());
    }

    if (!is_flat && !disable_egg) {
        EggAppearenceType egg_appearence;

        switch (corner) {
        case CornerType::South:
            egg_appearence = EggAppearenceType::ByXOrY;
            break;
        case CornerType::North:
            egg_appearence = EggAppearenceType::ByXAndY;
            break;
        case CornerType::EastWest:
        case CornerType::West:
            egg_appearence = EggAppearenceType::ByY;
            break;
        default:
            egg_appearence = EggAppearenceType::ByX;
            break;
        }

        mspr->SetEggAppearence(egg_appearence);
    }

    if (color != ucolor::clear) {
        mspr->SetColor(ucolor {color, 0});
        mspr->SetFixedAlpha(color.comp.a);
    }

    if (contour_color != ucolor::clear) {
        mspr->SetContour(ContourType::Custom, contour_color);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_RebuildFog(MapView* self)
{
    self->RebuildFog();
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Client_Map_GetItem(MapView* self, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    return self->GetItem(itemId);
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Client_Map_GetItem(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Item id arg is zero");
    }

    return self->GetItemOnHex(hex);
}

///@ ExportMethod
FO_SCRIPT_API vector<ItemView*> Client_Map_GetItems(MapView* self)
{
    return vec_transform(self->GetItems(), [](auto&& item) -> ItemView* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<ItemView*> Client_Map_GetItems(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Item id arg is zero");
    }

    return vec_transform(self->GetItemsOnHex(hex), [](auto&& item) -> ItemView* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Client_Map_GetCritter(MapView* self, ident_t critterId)
{
    if (!critterId) {
        return nullptr;
    }

    return self->GetCritter(critterId);
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCritters(MapView* self)
{
    return vec_transform(self->GetCritters(), [](auto&& cr) -> CritterView* { return cr.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCritters(MapView* self, CritterFindType findType)
{
    const auto map_critters = self->GetCritters();

    vector<CritterView*> critters;
    critters.reserve(map_critters.size());

    for (auto& cr : self->GetCritters()) {
        if (cr->CheckFind(findType)) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCritters(MapView* self, hstring pid, CritterFindType findType)
{
    vector<CritterView*> critters;

    for (auto& cr : self->GetCritters()) {
        if (cr->GetProtoId() == pid && cr->CheckFind(findType)) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCritters(MapView* self, mpos hex, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<CritterView*> critters;

    for (auto& cr : self->GetCritters()) {
        if (cr->CheckFind(findType) && GeometryHelper::CheckDist(hex, cr->GetHex(), cr->GetMultihex())) {
            critters.emplace_back(cr.get());
        }
    }

    std::ranges::stable_sort(critters, [&hex](const CritterView* cr1, const CritterView* cr2) {
        const auto dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        const auto dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCritters(MapView* self, mpos hex, int32 radius, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<CritterView*> critters;

    for (auto& cr : self->GetCritters()) {
        if (cr->CheckFind(findType) && GeometryHelper::CheckDist(hex, cr->GetHex(), radius + cr->GetMultihex())) {
            critters.emplace_back(cr.get());
        }
    }

    std::ranges::stable_sort(critters, [&hex](const CritterView* cr1, const CritterView* cr2) {
        const auto dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        const auto dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCrittersInPath(MapView* self, mpos fromHex, mpos toHex, float32 angle, int32 dist, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    vector<CritterHexView*> critters;
    self->TraceBullet(fromHex, toHex, dist, angle, &critters, findType, nullptr, nullptr, nullptr, true);
    return vec_transform(critters, [](auto&& cr) -> CritterView* { return cr; });
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCrittersWithBlockInPath(MapView* self, mpos fromHex, mpos toHex, float32 angle, int32 dist, CritterFindType findType, mpos& preBlockHex, mpos& blockHex)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    vector<CritterHexView*> critters;
    self->TraceBullet(fromHex, toHex, dist, angle, &critters, findType, &preBlockHex, &blockHex, nullptr, true);
    return vec_transform(critters, [](auto&& cr) -> CritterView* { return cr; });
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_GetHexInPath(MapView* self, mpos fromHex, mpos& toHex, float32 angle, int32 dist)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    mpos pre_block;
    self->TraceBullet(fromHex, toHex, dist, angle, nullptr, CritterFindType::Any, &pre_block, nullptr, nullptr, true);
    toHex = pre_block;
}

///@ ExportMethod
FO_SCRIPT_API vector<uint8> Client_Map_GetPath(MapView* self, mpos fromHex, mpos toHex, int32 cut)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    if (GeometryHelper::GetDistance(fromHex, toHex) <= 1) {
        if (GeometryHelper::GetDistance(fromHex, toHex) > 0 && cut == 0) {
            return {GeometryHelper::GetDir(fromHex, toHex)};
        }
        return {};
    }

    auto to_hex = toHex;
    const auto init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(nullptr, fromHex, to_hex, numeric_cast<int32>(cut))) {
        return {};
    }

    if (cut > 0 && GeometryHelper::GetDistance(fromHex, init_to_hex) <= cut && GeometryHelper::GetDistance(fromHex, to_hex) <= 1) {
        return {};
    }

    auto result = self->FindPath(nullptr, fromHex, to_hex, -1);
    if (!result) {
        return {};
    }

    return result->DirSteps;
}

///@ ExportMethod
FO_SCRIPT_API vector<uint8> Client_Map_GetPath(MapView* self, CritterView* cr, mpos toHex, int32 cut)
{
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    if (GeometryHelper::GetDistance(cr->GetHex(), toHex) <= 1 + cr->GetMultihex()) {
        if (GeometryHelper::GetDistance(cr->GetHex(), toHex) > cr->GetMultihex() && cut == 0) {
            return {GeometryHelper::GetDir(cr->GetHex(), toHex)};
        }
        return {};
    }

    auto to_hex = toHex;
    const auto init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(hex_cr, cr->GetHex(), to_hex, numeric_cast<int32>(cut))) {
        return {};
    }

    if (cut > 0 && GeometryHelper::GetDistance(cr->GetHex(), init_to_hex) <= cut + cr->GetMultihex() && GeometryHelper::GetDistance(cr->GetHex(), to_hex) <= 1 + cr->GetMultihex()) {
        return {};
    }

    auto result = self->FindPath(hex_cr, cr->GetHex(), to_hex, -1);
    if (!result) {
        return {};
    }

    return result->DirSteps;
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Map_GetPathLength(MapView* self, mpos fromHex, mpos toHex, int32 cut)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    if (GeometryHelper::GetDistance(fromHex, toHex) <= 1) {
        return cut > 0 ? 0 : 1;
    }

    auto to_hex = toHex;
    const auto init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(nullptr, fromHex, to_hex, numeric_cast<int32>(cut))) {
        return 0;
    }

    if (cut > 0 && GeometryHelper::GetDistance(fromHex, init_to_hex) <= cut && GeometryHelper::GetDistance(fromHex, to_hex) <= 1) {
        return 0;
    }

    const auto result = self->FindPath(nullptr, fromHex, to_hex, -1);

    if (!result) {
        return 0;
    }

    return numeric_cast<int32>(result->DirSteps.size());
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Map_GetPathLength(MapView* self, CritterView* cr, mpos toHex, int32 cut)
{
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    auto* hex_cr = dynamic_cast<CritterHexView*>(cr);

    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    if (GeometryHelper::GetDistance(cr->GetHex(), toHex) <= 1 + cr->GetMultihex()) {
        return cut > 0 ? 0 : 1;
    }

    auto to_hex = toHex;
    const auto init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(hex_cr, cr->GetHex(), to_hex, numeric_cast<int32>(cut))) {
        return 0;
    }

    if (cut > 0 && GeometryHelper::GetDistance(cr->GetHex(), init_to_hex) <= cut + cr->GetMultihex() && GeometryHelper::GetDistance(cr->GetHex(), to_hex) <= 1 + cr->GetMultihex()) {
        return 0;
    }

    const auto result = self->FindPath(hex_cr, cr->GetHex(), to_hex, -1);

    if (!result) {
        return 0;
    }

    return numeric_cast<int32>(result->DirSteps.size());
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_MoveScreenToHex(MapView* self, mpos hex, ipos16 hex_offset, int32 speed, bool canStop)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }
    if (speed < 0) {
        throw ScriptException("Negative speed");
    }

    if (speed == 0) {
        self->InstantScrollTo(hex);
    }
    else {
        self->ScrollToHex(hex, hex_offset, speed, canStop);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_ApplyScreenScroll(MapView* self, ipos32 offset, int32 speed, bool canStop)
{
    self->ApplyScrollOffset(offset, speed, canStop);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_LockScreenScroll(MapView* self, CritterView* cr, int32 speed, bool softLock, bool unlockIfSame)
{
    self->LockScreenScroll(cr, speed, softLock, unlockIfSame);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_MoveHexByDir(MapView* self, mpos& hex, uint8 dir)
{
    if (dir >= GameSettings::MAP_DIR_COUNT) {
        throw ScriptException("Invalid dir arg");
    }

    if (GeometryHelper::MoveHexByDir(hex, dir, self->GetSize())) {
        return true;
    }
    else {
        return false;
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Client_Map_MoveHexByDir(MapView* self, mpos& hex, uint8 dir, int32 steps)
{
    if (dir >= GameSettings::MAP_DIR_COUNT) {
        throw ScriptException("Invalid dir arg");
    }

    int32 result = 0;

    for (int32 i = 0; i < steps; i++) {
        if (GeometryHelper::MoveHexByDir(hex, dir, self->GetSize())) {
            result++;
        }
        else {
            break;
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_RedrawMap(MapView* self)
{
    self->RebuildMap();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_ChangeZoom(MapView* self, float32 targetZoom)
{
    if (is_float_equal(targetZoom, self->GetSpritesZoomTarget())) {
        return;
    }

    const fpos32 mouse_pos = fpos32(App->Input.GetMousePosition());
    const fsize32 screen_size = fsize32(self->GetScreenSize());
    const float32 mouse_x_factor = std::clamp(mouse_pos.x / screen_size.width, 0.0f, 1.0f);
    const float32 mouse_y_factor = std::clamp(mouse_pos.y / screen_size.height, 0.0f, 1.0f);

    self->ChangeZoom(targetZoom, {mouse_x_factor, mouse_y_factor});
}

///@ ExportMethod
FO_SCRIPT_API ipos32 Client_Map_GetHexScreenPos(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex provided");
    }

    const auto hex_pos = self->GetHexMapPos(hex);
    const auto& settings = self->GetEngine()->Settings;
    const ipos32 hex_center = {settings.MapHexWidth / 2, settings.MapHexHeight / 2};
    const auto screen_pos = self->MapToScreenPos(hex_pos + hex_center);
    return screen_pos;
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_GetHexAtScreenPos(MapView* self, ipos32 pos, mpos& hex)
{
    return self->GetHexAtScreen(pos, hex, nullptr);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_GetHexAtScreenPos(MapView* self, ipos32 pos, mpos& hex, ipos32& hexOffset)
{
    return self->GetHexAtScreen(pos, hex, &hexOffset);
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Client_Map_GetItemAtScreenPos(MapView* self, ipos32 pos)
{
    bool item_egg;
    return self->GetItemAtScreen(pos, item_egg, 0, true).first;
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Client_Map_GetCritterAtScreenPos(MapView* self, ipos32 pos)
{
    return self->GetCritterAtScreen(pos, false, 0, true).first;
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Client_Map_GetCritterAtScreenPos(MapView* self, ipos32 pos, int32 extraRange)
{
    auto* cr = self->GetCritterAtScreen(pos, false, 0, true).first;

    if (cr == nullptr && extraRange != 0) {
        cr = self->GetCritterAtScreen(pos, true, extraRange, false).first;
    }

    return cr;
}

///@ ExportMethod
FO_SCRIPT_API ClientEntity* Client_Map_GetEntityAtScreenPos(MapView* self, ipos32 pos)
{
    return self->GetEntityAtScreen(pos, 0, true).first;
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsHexVisible(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->GetField(hex).IsView;
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsHexMovable(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return !self->GetField(hex).MoveBlocked;
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsHexShootable(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return !self->GetField(hex).ShootBlocked;
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsOutsideArea(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->IsOutsideArea(hex);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetShootBorders(MapView* self, bool enabled, int32 dist)
{
    self->SetShootBorders(enabled, dist);
}

///@ ExportMethod
FO_SCRIPT_API SpritePattern* Client_Map_RunSpritePattern(MapView* self, string_view spriteName, int32 spriteCount)
{
    if (spriteCount < 1) {
        throw ScriptException("Invalid sprite count");
    }

    return self->RunSpritePattern(spriteName, spriteCount);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetCrittersContour(MapView* self, ContourType contour)
{
    self->SetCrittersContour(contour);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_ResetCritterContour(MapView* self)
{
    self->SetCritterContour(ident_t {}, ContourType::None);
}

///@ ExportMethod
FO_SCRIPT_API isize32 Client_Map_GetHexContentSize(MapView* self, mpos hex)
{
    return self->GetHexContentSize(hex);
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Client_Map_CreateLocalItem(MapView* self, hstring pid, mpos hex)
{
    if (self->GetEngine()->ProtoMngr.GetProtoItemSafe(pid) == nullptr) {
        throw ScriptException("Invalid item pid arg");
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->AddLocalItem(pid, hex);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetHiddenRoof(MapView* self, mpos hex)
{
    self->SetHiddenRoof(hex);
}

FO_END_NAMESPACE();
