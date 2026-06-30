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

#include "Client.h"
#include "Geometry.h"
#include "MapSprite.h"

FO_BEGIN_NAMESPACE

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

    if (mapSpr->ProtoId) {
        const auto* proto = self->GetEngine()->GetProtoItem(mapSpr->ProtoId);
        color = proto->GetColorize() ? proto->GetColorizeColor() : ucolor::clear;
        is_flat = proto->GetDrawFlatten();
        no_light = is_flat && !(proto->GetIsScenery() || proto->GetIsWall());
        draw_order = is_flat ? (proto->GetStatic() ? DrawOrderType::FlatItemPreLight : DrawOrderType::FlatItemAfterLight) : DrawOrderType::Item;
        draw_order_hy_offset = numeric_cast<int32_t>(proto->GetDrawOrderOffsetHexY());
        corner = proto->GetCorner();
        disable_egg = proto->GetDisableEgg();
    }

    auto* mspr = self->AddMapSprite(anim, mapSpr->Hex, draw_order, draw_order_hy_offset, //
        mapSpr->Offset, mapSpr->IsTweakOffs ? &mapSpr->TweakOffset : nullptr, //
        mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, &mapSpr->Valid);

    mapSpr->MSpr = mspr;

    if (mapSpr->Angle != 0) {
        mspr->SetAngle(mapSpr->Angle);
    }
    if (mapSpr->MapProjected) {
        mspr->SetMapProjected(true);
    }
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
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_DrawEntitySprite(MapView* self, ClientEntity* entity, int32_t effectSubtype, ucolor color, int32_t padding)
{
    FO_STACK_TRACE_ENTRY();

    auto* engine = self->GetEngine();

    if (!engine->CanDrawInScripts) {
        throw ScriptException("You can use this function only in render events");
    }
    if (padding < 0) {
        throw ScriptException("Negative padding");
    }
    if (effectSubtype < 0 || effectSubtype >= numeric_cast<int32_t>(engine->OffscreenEffects.size())) {
        throw ScriptException("Invalid effect subtype");
    }

    RenderEffect* effect = engine->OffscreenEffects[numeric_cast<size_t>(effectSubtype)].get();

    if (effect == nullptr) {
        throw ScriptException("Effect is null");
    }

    return self->DrawEntitySprite(entity, effect, color, padding);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_RebuildFog(MapView* self)
{
    self->RebuildFog();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetDayColors(MapView* self, ucolor mapDayColor, int32_t mapLightCapacity, ucolor globalDayColor, int32_t globalLightCapacity)
{
    self->SetDayColors(mapDayColor, mapLightCapacity, globalDayColor, globalLightCapacity);
}

///@ ExportMethod
FO_SCRIPT_API isize32 Client_Map_GetScreenSize(MapView* self)
{
    return self->GetScreenSize();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetScreenSize(MapView* self, isize32 size)
{
    self->SetScreenSize(size);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsScrollCheck(MapView* self)
{
    return self->IsScrollCheck();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetScrollCheck(MapView* self, bool enabled)
{
    self->SetScrollCheck(enabled);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetExtraScrollOffset(MapView* self, fpos32 offset)
{
    self->SetExtraScrollOffset(offset);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE ItemView* Client_Map_GetItem(MapView* self, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    return self->GetItem(itemId);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE ItemView* Client_Map_GetItemOnHex(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->GetItemOnHex(hex);
}

///@ ExportMethod
FO_SCRIPT_API vector<ItemView*> Client_Map_GetItems(MapView* self)
{
    return vec_transform(self->GetItems(), [](auto&& item) -> ItemView* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<ItemView*> Client_Map_GetItemsOnHex(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return vec_transform(self->GetItemsOnHex(hex), [](auto&& item) -> ItemView* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE CritterView* Client_Map_GetCritter(MapView* self, ident_t critterId)
{
    if (!critterId) {
        return nullptr;
    }

    return self->GetCritter(critterId);
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Client_Map_GetCritterOnHex(MapView* self, mpos hex, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<CritterHexView*> critters = self->GetCrittersOnHex(hex, findType);

    std::ranges::stable_sort(critters, [&hex](const CritterView* cr1, const CritterView* cr2) {
        const auto dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        const auto dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return !critters.empty() ? critters.front() : nullptr;
}

///@ ExportMethod
FO_SCRIPT_API CritterView* Client_Map_GetCritterInRadius(MapView* self, mpos hex, int32_t radius, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<CritterHexView*> critters = self->GetCrittersInRadius(hex, radius, findType);

    std::ranges::stable_sort(critters, [&hex](const CritterView* cr1, const CritterView* cr2) {
        const auto dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        const auto dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return !critters.empty() ? critters.front() : nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCritters(MapView* self, CritterFindType findType = CritterFindType::Any)
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
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCritters(MapView* self, ProtoCritter* proto, CritterFindType findType)
{
    if (proto == nullptr) {
        throw ScriptException("Critter proto arg is null");
    }

    vector<CritterView*> critters;

    for (auto& cr : self->GetCritters()) {
        if (cr->GetProtoId() == proto->GetProtoId() && cr->CheckFind(findType)) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCrittersOnHex(MapView* self, mpos hex, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<CritterHexView*> critters = self->GetCrittersOnHex(hex, findType);

    std::ranges::stable_sort(critters, [&hex](const CritterView* cr1, const CritterView* cr2) {
        const auto dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        const auto dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return vec_transform(critters, [](auto* cr) -> CritterView* { return cr; });
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCrittersInRadius(MapView* self, mpos hex, int32_t radius, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<CritterHexView*> critters = self->GetCrittersInRadius(hex, radius, findType);

    std::ranges::stable_sort(critters, [&hex](const CritterView* cr1, const CritterView* cr2) {
        const auto dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        const auto dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return vec_transform(critters, [](auto* cr) -> CritterView* { return cr; });
}

///@ ExportMethod
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCrittersInPath(MapView* self, mpos fromHex, mpos toHex, float32_t angle, int32_t dist, CritterFindType findType)
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
FO_SCRIPT_API vector<CritterView*> Client_Map_GetCrittersWithBlockInPath(MapView* self, mpos fromHex, mpos toHex, float32_t angle, int32_t dist, CritterFindType findType, mpos& preBlockHex, mpos& blockHex)
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
FO_SCRIPT_API void Client_Map_GetHexInPath(MapView* self, mpos fromHex, mpos& toHex, float32_t angle, int32_t dist)
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
FO_SCRIPT_API vector<mdir> Client_Map_GetPath(MapView* self, mpos fromHex, mpos toHex, int32_t cut)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    if (GeometryHelper::GetDistance(fromHex, toHex) <= 1) {
        if (GeometryHelper::GetDistance(fromHex, toHex) > 0 && cut == 0) {
            return {GeometryHelper::GetHexDir(fromHex, toHex)};
        }
        return {};
    }

    auto to_hex = toHex;
    const auto init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(nullptr, fromHex, to_hex, numeric_cast<int32_t>(cut))) {
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
FO_SCRIPT_API vector<mdir> Client_Map_GetPath(MapView* self, CritterView* cr, mpos toHex, int32_t cut)
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
            return {GeometryHelper::GetHexDir(cr->GetHex(), toHex)};
        }
        return {};
    }

    auto to_hex = toHex;
    const auto init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(hex_cr, cr->GetHex(), to_hex, numeric_cast<int32_t>(cut))) {
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
FO_SCRIPT_API int32_t Client_Map_GetPathLength(MapView* self, mpos fromHex, mpos toHex, int32_t cut)
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

    if (cut > 0 && !self->CutPath(nullptr, fromHex, to_hex, numeric_cast<int32_t>(cut))) {
        return 0;
    }

    if (cut > 0 && GeometryHelper::GetDistance(fromHex, init_to_hex) <= cut && GeometryHelper::GetDistance(fromHex, to_hex) <= 1) {
        return 0;
    }

    const auto result = self->FindPath(nullptr, fromHex, to_hex, -1);

    if (!result) {
        return 0;
    }

    return numeric_cast<int32_t>(result->DirSteps.size());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Map_GetPathLength(MapView* self, CritterView* cr, mpos toHex, int32_t cut)
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

    if (cut > 0 && !self->CutPath(hex_cr, cr->GetHex(), to_hex, numeric_cast<int32_t>(cut))) {
        return 0;
    }

    if (cut > 0 && GeometryHelper::GetDistance(cr->GetHex(), init_to_hex) <= cut + cr->GetMultihex() && GeometryHelper::GetDistance(cr->GetHex(), to_hex) <= 1 + cr->GetMultihex()) {
        return 0;
    }

    const auto result = self->FindPath(hex_cr, cr->GetHex(), to_hex, -1);

    if (!result) {
        return 0;
    }

    return numeric_cast<int32_t>(result->DirSteps.size());
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_MoveScreenToHex(MapView* self, mpos hex, ipos16 hex_offset, int32_t speed, bool canStop)
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
FO_SCRIPT_API void Client_Map_ApplyScreenScroll(MapView* self, ipos32 offset, int32_t speed, bool canStop)
{
    self->ApplyScrollOffset(offset, speed, canStop);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsAutoScrolling(MapView* self)
{
    return self->IsAutoScrolling();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_MoveHexByDir(MapView* self, mpos& hex, mdir dir)
{
    if (GeometryHelper::MoveHexByDir(hex, dir, self->GetSize())) {
        return true;
    }
    else {
        return false;
    }
}

///@ ExportMethod
FO_SCRIPT_API int32_t Client_Map_MoveHexByDir(MapView* self, mpos& hex, mdir dir, int32_t steps)
{
    int32_t result = 0;

    for (int32_t i = 0; i < steps; i++) {
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
FO_SCRIPT_API void Client_Map_ChangeZoom(MapView* self, float32_t targetZoom)
{
    if (is_float_equal(targetZoom, self->GetSpritesZoomTarget())) {
        return;
    }

    const fsize32 screen_size = fsize32(self->GetScreenSize());
    const auto& input = self->GetEngine()->SprMngr.GetInput();
    const fpos32 mouse_pos = input.IsMouseAvailable() ? fpos32(input.GetMousePosition()) : fpos32 {screen_size.width / 2.0f, screen_size.height / 2.0f};
    const float32_t mouse_x_factor = std::clamp(mouse_pos.x / screen_size.width, 0.0f, 1.0f);
    const float32_t mouse_y_factor = std::clamp(mouse_pos.y / screen_size.height, 0.0f, 1.0f);

    self->ChangeZoom(targetZoom, {mouse_x_factor, mouse_y_factor});
}

///@ ExportMethod
FO_SCRIPT_API ipos32 Client_Map_GetHexScreenPos(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex provided");
    }

    const auto hex_pos = self->GetHexMapPos(hex);
    const ipos32 hex_center = {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    return self->MapToScreenPos(hex_pos + hex_center);
}

///@ ExportMethod
FO_SCRIPT_API ipos32 Client_Map_GetHexMapPos(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex provided");
    }

    return self->GetHexMapPos(hex);
}

///@ ExportMethod
FO_SCRIPT_API fpos32 Client_Map_GetHexScreenPosF(MapView* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex provided");
    }

    const auto hex_pos = self->GetHexMapPos(hex);
    const ipos32 hex_center = {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    const ipos32 map_pos = hex_pos + hex_center;
    return (fpos32(map_pos) - self->GetScrollOffset()) * self->GetSpritesZoom();
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
FO_SCRIPT_API void Client_Map_SetTransparentEgg(MapView* self, TransparentEggSlot slot, mpos hex, ipos32 hexOffset, isize32 eggSize)
{
    self->SetTransparentEgg(slot, hex, hexOffset, eggSize, false);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetTransparentEgg(MapView* self, TransparentEggSlot slot, FO_NULLABLE CritterView* cr)
{
    const auto* cr_hex = dynamic_cast<CritterHexView*>(cr);

    if (cr_hex == nullptr || cr_hex->GetMap() != self || !cr_hex->IsMapSpriteValid()) {
        self->ClearTransparentEgg(slot);
        return;
    }

    // SetTransparentEgg expects a hex-center-relative offset; GetHexMapPos is the cell top-left,
    // so reference the hex visual center (top-left + half a hex) when measuring the sprite center.
    const auto rect = cr_hex->GetViewRect();
    const auto hex_pos = self->GetHexMapPos(cr_hex->GetHex());
    const auto hex_center = ipos32 {hex_pos.x + GameSettings::MAP_HEX_WIDTH / 2, hex_pos.y + GameSettings::MAP_HEX_HEIGHT / 2};
    const auto center_offset = ipos32 {rect.x + rect.width / 2 - hex_center.x, rect.y + rect.height / 2 - hex_center.y};
    self->SetTransparentEgg(slot, cr_hex->GetHex(), center_offset, rect.size(), true);
}

///@ ExportMethod
FO_SCRIPT_API void Client_Map_ClearTransparentEgg(MapView* self, TransparentEggSlot slot)
{
    self->ClearTransparentEgg(slot);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE ItemView* Client_Map_GetItemAtScreenPos(MapView* self, ipos32 pos)
{
    bool item_egg;
    return self->GetItemAtScreen(pos, item_egg, 0, true).first;
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE CritterView* Client_Map_GetCritterAtScreenPos(MapView* self, ipos32 pos, int32_t extraRange = 0)
{
    auto* cr = self->GetCritterAtScreen(pos, false, 0, true).first;

    if (cr == nullptr && extraRange != 0) {
        cr = self->GetCritterAtScreen(pos, true, extraRange, false).first;
    }

    return cr;
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE ClientEntity* Client_Map_GetEntityAtScreenPos(MapView* self, ipos32 pos)
{
    return self->GetEntityAtScreen(pos, 0, true).first;
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsHexValid(MapView* self, mpos hex)
{
    return self->GetSize().is_valid_pos(hex);
}

///@ ExportMethod
FO_SCRIPT_API vector<mpos> Client_Map_GetVisibleHexes(MapView* self)
{
    const msize map_size = self->GetSize();
    vector<mpos> hexes;
    hexes.reserve(numeric_cast<size_t>(map_size.width) * numeric_cast<size_t>(map_size.height));

    for (int32_t hy = 0; hy < map_size.height; hy++) {
        for (int32_t hx = 0; hx < map_size.width; hx++) {
            const mpos hex = map_size.from_raw_pos(hx, hy);

            if (self->IsHexToDraw(hex)) {
                hexes.emplace_back(hex);
            }
        }
    }

    return hexes;
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

///@ ExportMethod PassOwnership
FO_SCRIPT_API FogLayer* Client_Map_AddFog(MapView* self, FO_NULLABLE CritterView* cr, DrawOrderType drawOrder, int32_t flushEffectSubtype = -1)
{
    RenderEffect* customFlushEffect = flushEffectSubtype >= 0 ? self->GetEngine()->GetOffscreenEffect(flushEffectSubtype) : nullptr;

    return self->AddFog(cr, drawOrder, customFlushEffect);
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FogLayer* Client_Map_AddFog(MapView* self, mpos hex, DrawOrderType drawOrder, int32_t flushEffectSubtype = -1)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    RenderEffect* customFlushEffect = flushEffectSubtype >= 0 ? self->GetEngine()->GetOffscreenEffect(flushEffectSubtype) : nullptr;

    return self->AddFog(hex, drawOrder, customFlushEffect);
}

///@ ExportMethod
FO_SCRIPT_API SpritePattern* Client_Map_RunSpritePattern(MapView* self, string_view spriteName, int32_t spriteCount)
{
    if (spriteCount < 1) {
        throw ScriptException("Invalid sprite count");
    }

    return self->RunSpritePattern(spriteName, spriteCount);
}

///@ ExportMethod
FO_SCRIPT_API isize32 Client_Map_GetHexContentSize(MapView* self, mpos hex)
{
    return self->GetHexContentSize(hex);
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Client_Map_CreateLocalItem(MapView* self, hstring pid, mpos hex)
{
    if (self->GetEngine()->GetProtoItem(pid) == nullptr) {
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

FO_END_NAMESPACE
