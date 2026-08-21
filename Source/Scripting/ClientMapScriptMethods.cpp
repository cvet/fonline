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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

// Draws the loaded map during the RenderIface event; throws when called outside that render callback.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_DrawMap(ptr<MapView> self)
{
    if (!self->GetEngine()->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    self->DrawMap();
}

// Adds a holder-defined sprite to this map draw, silently skipping invalid, offscreen, or unresolved sprites and applying referenced item-prototype rendering rules when present.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_DrawMapSprite(ptr<MapView> self, ptr<MapSpriteHolder> mapSpr)
{
    auto engine = self->GetEngine();

    if (!self->GetSize().is_valid_pos(mapSpr->Hex)) {
        return;
    }
    if (!self->IsHexToDraw(mapSpr->Hex)) {
        return;
    }

    auto anim = engine->AnimGetSpr(mapSpr->SprId);

    if (!anim) {
        return;
    }

    ucolor color = mapSpr->Color;
    bool is_flat = mapSpr->IsFlat;
    bool no_light = mapSpr->NoLight;
    DrawOrderType draw_order = mapSpr->DrawOrder;
    int32_t draw_order_hy_offset = mapSpr->DrawOrderHyOffset;
    CornerType corner = mapSpr->Corner;
    bool disable_egg = mapSpr->DisableEgg;

    if (mapSpr->ProtoId) {
        auto proto = engine->GetProtoItem(mapSpr->ProtoId);
        FO_VERIFY_AND_THROW(proto, "Map sprite references unknown item proto");
        color = proto->GetColorize() ? proto->GetColorizeColor() : ucolor::clear;
        is_flat = proto->GetDrawFlatten();
        no_light = is_flat && !(proto->GetIsScenery() || proto->GetIsWall());
        draw_order = is_flat ? (proto->GetStatic() ? DrawOrderType::FlatItemPreLight : DrawOrderType::FlatItemAfterLight) : DrawOrderType::Item;
        draw_order_hy_offset = numeric_cast<int32_t>(proto->GetDrawOrderOffsetHexY());
        corner = proto->GetCorner();
        disable_egg = proto->GetDisableEgg();
    }

    auto mspr = self->AddMapSprite(anim, mapSpr->Hex, draw_order, draw_order_hy_offset, //
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

// Draws a client entity through the selected offscreen effect during a render event and returns whether drawing succeeded; throws outside rendering or for negative padding.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_DrawEntitySprite(ptr<MapView> self, ptr<ClientEntity> entity, int32_t effectSubtype, ucolor color, int32_t padding)
{
    auto engine = self->GetEngine();

    if (!engine->CanDrawInScripts) {
        throw ScriptException("You can use this function only in render events");
    }
    if (padding < 0) {
        throw ScriptException("Negative padding");
    }

    auto effect = engine->GetOffscreenEffect(effectSubtype);
    return self->DrawEntitySprite(entity, effect, color, padding);
}

// Rebuilds this map view's fog geometry and rendering state from its current fog layers.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_RebuildFog(ptr<MapView> self)
{
    self->RebuildFog();
}

// Replaces the map-local and global day colors together with their light-capacity values for this client map view.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetDayColors(ptr<MapView> self, ucolor mapDayColor, int32_t mapLightCapacity, ucolor globalDayColor, int32_t globalLightCapacity)
{
    self->SetDayColors(mapDayColor, mapLightCapacity, globalDayColor, globalLightCapacity);
}

// Returns the current pixel size of the map view's rendering viewport.
///@ ExportMethod
FO_SCRIPT_API isize32 Client_Map_GetScreenSize(ptr<MapView> self)
{
    return self->GetScreenSize();
}

// Sets the pixel size used by this map view for projection, visibility, and rendering.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetScreenSize(ptr<MapView> self, isize32 size)
{
    self->SetScreenSize(size);
}

// Returns the map view's current scroll-check flag.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsScrollCheck(ptr<MapView> self)
{
    return self->IsScrollCheck();
}

// Enables or disables the map view's scroll-check behavior.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetScrollCheck(ptr<MapView> self, bool enabled)
{
    self->SetScrollCheck(enabled);
}

// Sets an additional floating-point scroll offset applied by this map view.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetExtraScrollOffset(ptr<MapView> self, fpos32 offset)
{
    self->SetExtraScrollOffset(offset);
}

// Returns the map item with the nonzero runtime id, or null when this map does not contain it; a zero id throws.
///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Map_GetItem(ptr<MapView> self, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    auto item = self->GetItem(itemId);
    return item;
}

// Returns the map's single-item lookup result for a valid hex, or null when no item is selected there; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Map_GetItemOnHex(ptr<MapView> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item = self->GetItemOnHex(hex);
    return item;
}

// Returns all client item views currently attached to this map.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ItemView>> Client_Map_GetItems(ptr<MapView> self)
{
    span<refcount_ptr<ItemHexView>> map_items = self->GetItems();
    return MakeScriptRefHandleVectorAs<ItemView, ItemHexView>(map_items);
}

// Returns all client item views occupying a valid hex; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<ItemView>> Client_Map_GetItemsOnHex(ptr<MapView> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    span<ptr<ItemHexView>> hex_items = self->GetItemsOnHex(hex);
    return vector<ptr<ItemView>>(hex_items.begin(), hex_items.end());
}

// Returns the critter with the supplied runtime id, or null for a zero or unknown id.
///@ ExportMethod
FO_SCRIPT_API nptr<CritterView> Client_Map_GetCritter(ptr<MapView> self, ident_t critterId)
{
    if (!critterId) {
        return nullptr;
    }

    auto cr = self->GetCritter(critterId);
    return cr;
}

// Returns the matching critter nearest to a valid hex after accounting for multihex size, or null when none match; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API nptr<CritterView> Client_Map_GetCritterOnHex(ptr<MapView> self, mpos hex, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<CritterHexView>> critters = self->GetCrittersOnHex(hex, findType);

    std::ranges::stable_sort(critters, [&hex](ptr<const CritterHexView> cr1, ptr<const CritterHexView> cr2) {
        int32_t dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        int32_t dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    if (critters.empty()) {
        return nullptr;
    }

    auto cr = critters.front();
    return cr;
}

// Returns the matching critter nearest to a valid center hex within the requested radius after accounting for multihex size, or null when none match.
///@ ExportMethod
FO_SCRIPT_API nptr<CritterView> Client_Map_GetCritterInRadius(ptr<MapView> self, mpos hex, int32_t radius, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<CritterHexView>> critters = self->GetCrittersInRadius(hex, radius, findType);

    std::ranges::stable_sort(critters, [&hex](ptr<const CritterHexView> cr1, ptr<const CritterHexView> cr2) {
        int32_t dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        int32_t dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    if (critters.empty()) {
        return nullptr;
    }

    auto cr = critters.front();
    return cr;
}

// Returns every critter on this map that passes the requested find-type filter.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Map_GetCritters(ptr<MapView> self, CritterFindType findType = CritterFindType::Any)
{
    span<refcount_ptr<CritterHexView>> map_critters = self->GetCritters();

    vector<ptr<CritterView>> critters;
    critters.reserve(map_critters.size());

    for (size_t i = 0; i < map_critters.size(); i++) {
        auto cr = map_critters[i].as_ptr();

        if (cr->CheckFind(findType)) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

// Returns every critter on this map whose prototype id and find-type state match the supplied filters.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Map_GetCritters(ptr<MapView> self, hstring pid, CritterFindType findType)
{
    span<refcount_ptr<CritterHexView>> map_critters = self->GetCritters();
    vector<ptr<CritterView>> critters;

    for (size_t i = 0; i < map_critters.size(); i++) {
        auto cr = map_critters[i].as_ptr();

        if (cr->GetProtoId() == pid && cr->CheckFind(findType)) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

// Returns every critter on this map whose prototype and find-type state match the supplied filters.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Map_GetCritters(ptr<MapView> self, ptr<ProtoCritter> proto, CritterFindType findType)
{
    span<refcount_ptr<CritterHexView>> map_critters = self->GetCritters();

    vector<ptr<CritterView>> critters;

    for (size_t i = 0; i < map_critters.size(); i++) {
        auto cr = map_critters[i].as_ptr();

        if (cr->GetProtoId() == proto->GetProtoId() && cr->CheckFind(findType)) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

// Returns matching critters overlapping a valid hex, ordered by multihex-adjusted distance from it; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Map_GetCrittersOnHex(ptr<MapView> self, mpos hex, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<CritterHexView>> critters = self->GetCrittersOnHex(hex, findType);

    std::ranges::stable_sort(critters, [&hex](ptr<const CritterHexView> cr1, ptr<const CritterHexView> cr2) {
        int32_t dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        int32_t dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return vector<ptr<CritterView>>(critters.begin(), critters.end());
}

// Returns matching critters within the requested radius of a valid hex, ordered by multihex-adjusted distance from it; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Map_GetCrittersInRadius(ptr<MapView> self, mpos hex, int32_t radius, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<CritterHexView>> critters = self->GetCrittersInRadius(hex, radius, findType);

    std::ranges::stable_sort(critters, [&hex](ptr<const CritterHexView> cr1, ptr<const CritterHexView> cr2) {
        int32_t dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        int32_t dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return vector<ptr<CritterView>>(critters.begin(), critters.end());
}

// Traces the requested path corridor and returns matching critters intersected along it; invalid endpoint hexes throw.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Map_GetCrittersInPath(ptr<MapView> self, mpos fromHex, mpos toHex, float32_t angle, int32_t dist, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    vector<ptr<CritterHexView>> critters;
    self->TraceBullet(fromHex, toHex, dist, angle, &critters, findType, nullptr, nullptr, nullptr, true);
    return vector<ptr<CritterView>>(critters.begin(), critters.end());
}

// Traces the requested path corridor, returns matching critters, and writes the last pre-block and first blocking hexes; invalid endpoints throw.
///@ ExportMethod
FO_SCRIPT_API vector<ptr<CritterView>> Client_Map_GetCrittersWithBlockInPath(ptr<MapView> self, mpos fromHex, mpos toHex, float32_t angle, int32_t dist, CritterFindType findType, mpos& preBlockHex, mpos& blockHex)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    vector<ptr<CritterHexView>> critters;
    self->TraceBullet(fromHex, toHex, dist, angle, &critters, findType, &preBlockHex, &blockHex, nullptr, true);
    return vector<ptr<CritterView>>(critters.begin(), critters.end());
}

// Traces toward a valid destination and replaces it with the last hex before the path is blocked or reaches its distance limit; invalid endpoints throw.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_GetHexInPath(ptr<MapView> self, mpos fromHex, mpos& toHex, float32_t angle, int32_t dist)
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

// Finds movement directions between valid hexes, optionally stopping the endpoint cut hexes short; returns an empty array when no movement or path is available.
///@ ExportMethod
FO_SCRIPT_API vector<mdir> Client_Map_GetPath(ptr<MapView> self, mpos fromHex, mpos toHex, int32_t cut)
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

    mpos to_hex = toHex;
    mpos init_to_hex = toHex;

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

// Finds movement directions from an on-map critter to a valid hex, accounting for its multihex and optionally stopping cut hexes short; returns an empty array when no movement or path is available.
///@ ExportMethod
FO_SCRIPT_API vector<mdir> Client_Map_GetPath(ptr<MapView> self, ptr<CritterView> cr, mpos toHex, int32_t cut)
{
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    auto hex_cr = cr.dyn_cast<CritterHexView>();

    if (!hex_cr) {
        throw ScriptException("Critter is not on map");
    }

    if (GeometryHelper::GetDistance(hex_cr->GetHex(), toHex) <= 1 + hex_cr->GetMultihex()) {
        if (GeometryHelper::GetDistance(hex_cr->GetHex(), toHex) > hex_cr->GetMultihex() && cut == 0) {
            return {GeometryHelper::GetHexDir(hex_cr->GetHex(), toHex)};
        }

        return {};
    }

    mpos to_hex = toHex;
    mpos init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(hex_cr, hex_cr->GetHex(), to_hex, numeric_cast<int32_t>(cut))) {
        return {};
    }
    if (cut > 0 && GeometryHelper::GetDistance(hex_cr->GetHex(), init_to_hex) <= cut + hex_cr->GetMultihex() && GeometryHelper::GetDistance(hex_cr->GetHex(), to_hex) <= 1 + hex_cr->GetMultihex()) {
        return {};
    }

    auto result = self->FindPath(hex_cr, hex_cr->GetHex(), to_hex, -1);

    if (!result) {
        return {};
    }

    return result->DirSteps;
}

// Returns the number of movement steps between valid hexes, optionally stopping cut hexes short; returns zero when cut removes the move or no path is available.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Map_GetPathLength(ptr<MapView> self, mpos fromHex, mpos toHex, int32_t cut)
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

    mpos to_hex = toHex;
    mpos init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(nullptr, fromHex, to_hex, numeric_cast<int32_t>(cut))) {
        return 0;
    }

    if (cut > 0 && GeometryHelper::GetDistance(fromHex, init_to_hex) <= cut && GeometryHelper::GetDistance(fromHex, to_hex) <= 1) {
        return 0;
    }

    auto result = self->FindPath(nullptr, fromHex, to_hex, -1);

    if (!result) {
        return 0;
    }

    return numeric_cast<int32_t>(result->DirSteps.size());
}

// Returns the number of movement steps from an on-map critter to a valid hex, accounting for its multihex and optional cut; returns zero when no movement or path is available.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Map_GetPathLength(ptr<MapView> self, ptr<CritterView> cr, mpos toHex, int32_t cut)
{
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    auto hex_cr = cr.dyn_cast<CritterHexView>();

    if (!hex_cr) {
        throw ScriptException("Critter is not on map");
    }

    if (GeometryHelper::GetDistance(hex_cr->GetHex(), toHex) <= 1 + hex_cr->GetMultihex()) {
        return cut > 0 ? 0 : 1;
    }

    mpos to_hex = toHex;
    mpos init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(hex_cr, hex_cr->GetHex(), to_hex, numeric_cast<int32_t>(cut))) {
        return 0;
    }

    if (cut > 0 && GeometryHelper::GetDistance(hex_cr->GetHex(), init_to_hex) <= cut + hex_cr->GetMultihex() && GeometryHelper::GetDistance(hex_cr->GetHex(), to_hex) <= 1 + hex_cr->GetMultihex()) {
        return 0;
    }

    auto result = self->FindPath(hex_cr, hex_cr->GetHex(), to_hex, -1);

    if (!result) {
        return 0;
    }

    return numeric_cast<int32_t>(result->DirSteps.size());
}

// Scrolls the view toward a valid hex using the supplied offset and nonnegative speed; speed zero jumps immediately and ignores the offset.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_MoveScreenToHex(ptr<MapView> self, mpos hex, ipos16 hex_offset, int32_t speed, bool canStop)
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

// Applies a pixel scroll delta using the requested speed and interruption policy.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_ApplyScreenScroll(ptr<MapView> self, ipos32 offset, int32_t speed, bool canStop)
{
    self->ApplyScrollOffset(offset, speed, canStop);
}

// Returns whether this map view is currently performing an automatic scroll.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsAutoScrolling(ptr<MapView> self)
{
    return self->IsAutoScrolling();
}

// Moves a hex one cell in the requested direction when the destination remains inside the map, updates the argument, and reports success.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_MoveHexByDir(ptr<MapView> self, mpos& hex, mdir dir)
{
    if (GeometryHelper::MoveHexByDir(hex, dir, self->GetSize())) {
        return true;
    }
    else {
        return false;
    }
}

// Moves a hex up to the requested number of in-bounds cells in one direction, updates the argument, and returns the completed step count.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Map_MoveHexByDir(ptr<MapView> self, mpos& hex, mdir dir, int32_t steps)
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

// Rebuilds the map view's renderable map state from its current client-side contents.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_RedrawMap(ptr<MapView> self)
{
    self->RebuildMap();
}

// Starts a zoom change anchored under the mouse cursor, or the screen center when mouse input is unavailable; an unchanged target is ignored.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_ChangeZoom(ptr<MapView> self, float32_t targetZoom)
{
    if (is_float_equal(targetZoom, self->GetSpritesZoomTarget())) {
        return;
    }

    fsize32 screen_size = fsize32(self->GetScreenSize());
    auto input = self->GetEngine()->SprMngr.GetInput();
    fpos32 mouse_pos = input->IsMouseAvailable() ? fpos32(input->GetMousePosition()) : fpos32 {screen_size.width / 2.0f, screen_size.height / 2.0f};
    float32_t mouse_x_factor = std::clamp(mouse_pos.x / screen_size.width, 0.0f, 1.0f);
    float32_t mouse_y_factor = std::clamp(mouse_pos.y / screen_size.height, 0.0f, 1.0f);

    self->ChangeZoom(targetZoom, {mouse_x_factor, mouse_y_factor});
}

// Returns the integer screen position of the visual center of a valid hex after current scrolling and projection; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API ipos32 Client_Map_GetHexScreenPos(ptr<MapView> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex provided");
    }

    ipos32 hex_pos = self->GetHexMapPos(hex);
    ipos32 hex_center = {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    return self->MapToScreenPos(hex_pos + hex_center);
}

// Returns the integer map-space position of the top-left of a valid hex cell; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API ipos32 Client_Map_GetHexMapPos(ptr<MapView> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex provided");
    }

    return self->GetHexMapPos(hex);
}

// Returns the floating-point screen position of a valid hex center after current scrolling and sprite zoom; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API fpos32 Client_Map_GetHexScreenPosF(ptr<MapView> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex provided");
    }

    ipos32 hex_pos = self->GetHexMapPos(hex);
    ipos32 hex_center = {GameSettings::MAP_HEX_WIDTH / 2, GameSettings::MAP_HEX_HEIGHT / 2};
    ipos32 map_pos = hex_pos + hex_center;
    return (fpos32(map_pos) - self->GetScrollOffset()) * self->GetSpritesZoom();
}

// Resolves a screen position to a map hex, writes the hex on success, and returns whether the position maps to the view.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_GetHexAtScreenPos(ptr<MapView> self, ipos32 pos, mpos& hex)
{
    return self->GetHexAtScreen(pos, hex, nullptr);
}

// Resolves a screen position to a map hex and its pixel offset within that hex, writes both outputs on success, and reports whether resolution succeeded.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_GetHexAtScreenPos(ptr<MapView> self, ipos32 pos, mpos& hex, ipos32& hexOffset)
{
    return self->GetHexAtScreen(pos, hex, &hexOffset);
}

// Configures a transparent-egg slot around an explicit hex-relative rectangle without attaching it to a critter.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetTransparentEgg(ptr<MapView> self, TransparentEggSlot slot, mpos hex, ipos32 hexOffset, isize32 eggSize)
{
    self->SetTransparentEgg(slot, hex, hexOffset, eggSize, false);
}

// Configures a transparent-egg slot around an on-map critter's current sprite bounds, or clears the slot when the critter is absent, foreign, or not drawable.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetTransparentEgg(ptr<MapView> self, TransparentEggSlot slot, nptr<CritterView> cr)
{
    auto cr_hex = cr.dyn_cast<CritterHexView>();

    if (!cr_hex) {
        self->ClearTransparentEgg(slot);
        return;
    }

    if (cr_hex->GetMap() != self || !cr_hex->IsMapSpriteValid()) {
        self->ClearTransparentEgg(slot);
        return;
    }

    // SetTransparentEgg expects a hex-center-relative offset; GetHexMapPos is the cell top-left,
    // so reference the hex visual center (top-left + half a hex) when measuring the sprite center
    irect32 rect = cr_hex->GetViewRect();
    ipos32 hex_pos = self->GetHexMapPos(cr_hex->GetHex());
    ipos32 hex_center = {hex_pos.x + GameSettings::MAP_HEX_WIDTH / 2, hex_pos.y + GameSettings::MAP_HEX_HEIGHT / 2};
    ipos32 center_offset = {rect.x + rect.width / 2 - hex_center.x, rect.y + rect.height / 2 - hex_center.y};
    self->SetTransparentEgg(slot, cr_hex->GetHex(), center_offset, rect.size(), true);
}

// Clears the selected transparent-egg slot from this map view.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_ClearTransparentEgg(ptr<MapView> self, TransparentEggSlot slot)
{
    self->ClearTransparentEgg(slot);
}

// Returns the top selectable map item at a screen position using pixel-precise hit testing, or null when none is hit.
///@ ExportMethod
FO_SCRIPT_API nptr<ItemView> Client_Map_GetItemAtScreenPos(ptr<MapView> self, ipos32 pos)
{
    bool item_egg;
    auto item = self->GetItemAtScreen(pos, item_egg, 0, true).first;
    return item;
}

// Returns the pixel-precise critter hit at a screen position, optionally retrying with an expanded range when the exact lookup finds none.
///@ ExportMethod
FO_SCRIPT_API nptr<CritterView> Client_Map_GetCritterAtScreenPos(ptr<MapView> self, ipos32 pos, int32_t extraRange = 0)
{
    auto cr = self->GetCritterAtScreen(pos, false, 0, true).first;

    if (!cr && extraRange != 0) {
        cr = self->GetCritterAtScreen(pos, true, extraRange, false).first;
    }

    return cr;
}

// Returns the top selectable client entity at a screen position using pixel-precise hit testing, or null when none is hit.
///@ ExportMethod
FO_SCRIPT_API nptr<ClientEntity> Client_Map_GetEntityAtScreenPos(ptr<MapView> self, ipos32 pos)
{
    auto entity = self->GetEntityAtScreen(pos, 0, true).first;
    return entity;
}

// Returns whether the supplied hex lies inside this map's dimensions without throwing for an invalid coordinate.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsHexValid(ptr<MapView> self, mpos hex)
{
    return self->GetSize().is_valid_pos(hex);
}

// Returns every map hex currently selected for drawing by this view.
///@ ExportMethod
FO_SCRIPT_API vector<mpos> Client_Map_GetVisibleHexes(ptr<MapView> self)
{
    msize map_size = self->GetSize();
    vector<mpos> hexes;
    hexes.reserve(numeric_cast<size_t>(map_size.width) * numeric_cast<size_t>(map_size.height));

    for (int32_t hy = 0; hy < map_size.height; hy++) {
        for (int32_t hx = 0; hx < map_size.width; hx++) {
            mpos hex = map_size.from_raw_pos(hx, hy);

            if (self->IsHexToDraw(hex)) {
                hexes.emplace_back(hex);
            }
        }
    }

    return hexes;
}

// Returns the client visibility flag stored for a valid hex; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsHexVisible(ptr<MapView> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->GetField(hex).IsView;
}

// Returns whether movement is currently unblocked on a valid hex; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsHexMovable(ptr<MapView> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return !self->GetField(hex).MoveBlocked;
}

// Returns whether shooting is currently unblocked through a valid hex; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsHexShootable(ptr<MapView> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return !self->GetField(hex).ShootBlocked;
}

// Returns whether a valid hex lies outside the map's configured playable area; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API bool Client_Map_IsOutsideArea(ptr<MapView> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->IsOutsideArea(hex);
}

// Adds and returns a fog layer attached to an optional critter at the requested draw order, using an offscreen flush effect when its subtype is nonnegative.
///@ ExportMethod
FO_SCRIPT_API ptr<FogLayer> Client_Map_AddFog(ptr<MapView> self, nptr<CritterView> cr, DrawOrderType drawOrder, int32_t flushEffectSubtype = -1)
{
    nptr<RenderEffect> customFlushEffect = nullptr;
    if (flushEffectSubtype >= 0) {
        customFlushEffect = self->GetEngine()->GetOffscreenEffect(flushEffectSubtype);
    }

    auto fog = self->AddFog(cr, drawOrder, customFlushEffect);
    return fog;
}

// Adds and returns a fog layer at a valid hex and draw order, using an offscreen flush effect when its subtype is nonnegative; an invalid hex throws.
///@ ExportMethod
FO_SCRIPT_API ptr<FogLayer> Client_Map_AddFog(ptr<MapView> self, mpos hex, DrawOrderType drawOrder, int32_t flushEffectSubtype = -1)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    nptr<RenderEffect> customFlushEffect = nullptr;
    if (flushEffectSubtype >= 0) {
        customFlushEffect = self->GetEngine()->GetOffscreenEffect(flushEffectSubtype);
    }

    auto fog = self->AddFog(hex, drawOrder, customFlushEffect);
    return fog;
}

// Starts a named sprite pattern with a positive frame count and returns its controller, or null when the pattern cannot be created.
///@ ExportMethod
FO_SCRIPT_API nptr<SpritePattern> Client_Map_RunSpritePattern(ptr<MapView> self, string_view spriteName, int32_t spriteCount)
{
    if (spriteCount < 1) {
        throw ScriptException("Invalid sprite count");
    }

    auto sprite_pattern = self->RunSpritePattern(spriteName, spriteCount);
    return sprite_pattern;
}

// Returns the aggregate pixel size of content associated with the supplied hex in this map view.
///@ ExportMethod
FO_SCRIPT_API isize32 Client_Map_GetHexContentSize(ptr<MapView> self, mpos hex)
{
    return self->GetHexContentSize(hex);
}

// Creates and attaches a client-local item of a known prototype at a valid map hex; invalid prototypes or coordinates throw.
///@ ExportMethod
FO_SCRIPT_API ptr<ItemView> Client_Map_CreateLocalItem(ptr<MapView> self, hstring pid, mpos hex)
{
    auto proto = self->GetEngine()->GetProtoItem(pid);

    if (!proto) {
        throw ScriptException("Invalid item pid arg");
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item = self->AddLocalItem(pid, hex);
    return item;
}

// Sets the hex used by this map view's roof-hiding logic.
///@ ExportMethod
FO_SCRIPT_API void Client_Map_SetHiddenRoof(ptr<MapView> self, mpos hex)
{
    self->SetHiddenRoof(hex);
}

// Returns the roof group number at a valid map hex; an out-of-range coordinate throws.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Map_GetRoofNum(ptr<MapView> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->GetField(hex).RoofNum;
}

// Returns the roof group currently hidden by this map view's roof-hiding logic.
///@ ExportMethod
FO_SCRIPT_API int32_t Client_Map_GetHiddenRoofNum(ptr<MapView> self)
{
    return self->GetHiddenRoofNum();
}

FO_END_NAMESPACE
