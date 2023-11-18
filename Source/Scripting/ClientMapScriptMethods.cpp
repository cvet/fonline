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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "MapSprite.h"

// ReSharper disable CppInconsistentNaming

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_DrawMap(MapView* self)
{
    if (!self->GetEngine()->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    self->DrawMap();
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_DrawMapTexts(MapView* self)
{
    if (!self->GetEngine()->CanDrawInScripts) {
        throw ScriptException("You can use this function only in RenderIface event");
    }

    self->DrawMapTexts();
}

///# ...
///# param text ...
///# param hx ...
///# param hy ...
///# param showTime ...
///# param color ...
///# param fade ...
///# param endOx ...
///# param endOy ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_Message(MapView* self, string_view text, uint16 hx, uint16 hy, tick_t showTime, uint color, bool fade, int endOx, int endOy)
{
    self->AddMapText(text, hx, hy, ucolor {color, true}, std::chrono::milliseconds {showTime.underlying_value()}, fade, endOx, endOy);
}

///# ...
///# param mapSpr ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_DrawMapSprite(MapView* self, MapSpriteData* mapSpr)
{
    if (mapSpr == nullptr) {
        throw ScriptException("Map sprite arg is null");
    }

    if (mapSpr->HexX >= self->GetWidth() || mapSpr->HexY >= self->GetHeight()) {
        return;
    }
    if (!self->IsHexToDraw(mapSpr->HexX, mapSpr->HexY)) {
        return;
    }

    const auto* anim = self->GetEngine()->AnimGetSpr(mapSpr->SprId);
    if (anim == nullptr) {
        return;
    }

    auto color = ucolor {mapSpr->Color, true};
    auto is_flat = mapSpr->IsFlat;
    auto no_light = mapSpr->NoLight;
    auto draw_order = mapSpr->DrawOrder;
    auto draw_order_hy_offset = mapSpr->DrawOrderHyOffset;
    auto corner = mapSpr->Corner;
    auto disable_egg = mapSpr->DisableEgg;
    auto contour_color = ucolor {mapSpr->ContourColor, true};

    if (mapSpr->ProtoId) {
        const auto* proto_item = self->GetEngine()->ProtoMngr.GetProtoItem(mapSpr->ProtoId);
        if (proto_item == nullptr) {
            return;
        }

        color = ucolor {proto_item->GetIsColorize() ? proto_item->GetLightColor() : 0, true};
        is_flat = proto_item->GetIsFlat();
        const auto is_item = proto_item->GetIsScenery() || proto_item->GetIsWall();
        no_light = (is_flat && !is_item);
        draw_order = (is_flat ? (is_item ? DrawOrderType::FlatItem : DrawOrderType::FlatScenery) : (is_item ? DrawOrderType::Item : DrawOrderType::Scenery));
        draw_order_hy_offset = static_cast<int>(static_cast<int8>(proto_item->GetDrawOrderOffsetHexY()));
        corner = proto_item->GetCorner();
        disable_egg = proto_item->GetDisableEgg();
        contour_color = (proto_item->GetIsBadItem() ? ucolor {255, 0, 0} : ucolor::clear);
    }

    const auto& field = self->GetField(mapSpr->HexX, mapSpr->HexY);
    auto& spr = self->GetDrawList().InsertSprite(draw_order, mapSpr->HexX, mapSpr->HexY + static_cast<uint16>(draw_order_hy_offset), //
        (self->GetEngine()->Settings.MapHexWidth / 2) + mapSpr->OffsX, (self->GetEngine()->Settings.MapHexHeight / 2) + mapSpr->OffsY, &field.ScrX, &field.ScrY, anim, nullptr, //
        mapSpr->IsTweakOffs ? &mapSpr->TweakOffsX : nullptr, mapSpr->IsTweakOffs ? &mapSpr->TweakOffsY : nullptr, mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, nullptr, &mapSpr->Valid);

    spr.MapSpr = mapSpr;
    mapSpr->AddRef();

    if (!no_light) {
        spr.SetLight(corner, self->GetLightHex(0, 0), self->GetWidth(), self->GetHeight());
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
        spr.SetEggAppearence(egg_appearence);
    }

    if (color != ucolor::clear) {
        spr.SetColor(ucolor {color, 0});
        spr.SetFixedAlpha(color.comp.a);
    }

    if (contour_color != ucolor::clear) {
        spr.SetContour(ContourType::Custom, contour_color);
    }
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_RebuildFog(MapView* self)
{
    self->RebuildFog();
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] ItemView* Client_Map_GetItem(MapView* self, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    // On map
    ItemView* item = self->GetItem(itemId);

    // On Chosen
    if (item == nullptr && self->GetEngine()->GetChosen() != nullptr) {
        item = self->GetEngine()->GetChosen()->GetInvItem(itemId);
    }

    // On other critters
    if (item == nullptr) {
        for (auto* cr : self->GetCritters()) {
            if (!cr->IsChosen()) {
                item = cr->GetInvItem(itemId);
            }
        }
    }

    if (item == nullptr || item->IsDestroyed()) {
        return nullptr;
    }
    return item;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Client_Map_GetVisibleItems(MapView* self)
{
    auto&& all_items = self->GetItems();

    vector<ItemView*> items;
    items.reserve(all_items.size());

    for (auto* item : all_items) {
        if (!item->IsFinishing() && !item->GetIsTile()) {
            items.emplace_back(item);
        }
    }

    return items;
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Client_Map_GetVisibleItemsOnHex(MapView* self, uint16 hx, uint16 hy)
{
    auto&& hex_items = self->GetItems(hx, hy);

    vector<ItemView*> items;
    items.reserve(hex_items.size());

    for (auto* item : hex_items) {
        if (!item->IsFinishing()) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param critterId ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] CritterView* Client_Map_GetCritter(MapView* self, ident_t critterId)
{
    if (!critterId) {
        return nullptr;
    }

    auto* cr = self->GetCritter(critterId);
    if (cr == nullptr || cr->IsDestroyed() || cr->IsDestroying()) {
        return nullptr;
    }

    return cr;
}

///# ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Map_GetCritters(MapView* self, CritterFindType findType)
{
    vector<CritterView*> critters;

    for (auto* cr : self->GetCritters()) {
        if (cr->CheckFind(findType)) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///# ...
///# param pid ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Map_GetCritters(MapView* self, hstring pid, CritterFindType findType)
{
    vector<CritterView*> critters;

    if (!pid) {
        for (auto* cr : self->GetCritters()) {
            if (cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }
    else {
        for (auto* cr : self->GetCritters()) {
            if (cr->IsNpc() && cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }

    return critters;
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Map_GetCritters(MapView* self, uint16 hx, uint16 hy, uint radius, CritterFindType findType)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    vector<CritterView*> critters;

    for (auto* cr : self->GetCritters()) {
        if (cr->CheckFind(findType) && GeometryHelper::CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius)) {
            critters.push_back(cr);
        }
    }

    std::sort(critters.begin(), critters.end(), [&hx, &hy](const CritterView* cr1, const CritterView* cr2) {
        //
        return GeometryHelper::DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY()) < GeometryHelper::DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY());
    });

    return critters;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Map_GetCrittersInPath(MapView* self, uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, CritterFindType findType)
{
    vector<CritterHexView*> critters;
    self->TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, &critters, findType, nullptr, nullptr, nullptr, true);
    return vec_cast<CritterView*>(critters);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///# param findType ...
///# param preBlockHx ...
///# param preBlockHy ...
///# param blockHx ...
///# param blockHy ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Map_GetCrittersWithBlockInPath(MapView* self, uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, CritterFindType findType, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy)
{
    vector<CritterHexView*> critters;
    pair<uint16, uint16> block = {};
    pair<uint16, uint16> pre_block = {};
    self->TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, &critters, findType, &block, &pre_block, nullptr, true);
    preBlockHx = pre_block.first;
    preBlockHy = pre_block.second;
    blockHx = block.first;
    blockHy = block.second;
    return vec_cast<CritterView*>(critters);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Client_Map_GetHexInPath(MapView* self, uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist)
{
    pair<uint16, uint16> pre_block = {};
    pair<uint16, uint16> block = {};
    self->TraceBullet(fromHx, fromHy, toHx, toHy, dist, angle, nullptr, CritterFindType::Any, &block, &pre_block, nullptr, true);
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
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<uint8> Client_Map_GetPath(MapView* self, uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)
{
    if (fromHx >= self->GetWidth() || fromHy >= self->GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= self->GetWidth() || toHy >= self->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (GeometryHelper::DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        if (GeometryHelper::DistGame(fromHx, fromHy, to_hx, to_hy) > 0 && cut == 0) {
            return {GeometryHelper::GetFarDir(fromHx, fromHy, to_hx, to_hy)};
        }
        return {};
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !self->CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, static_cast<int>(cut))) {
        return {};
    }

    if (cut > 0 && GeometryHelper::DistGame(fromHx, fromHy, init_to_hx, init_to_hy) <= cut && GeometryHelper::DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        return {};
    }

    auto result = self->FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, -1);
    if (!result) {
        return {};
    }

    return result->Steps;
}

///# ...
///# param cr ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<uint8> Client_Map_GetPath(MapView* self, CritterView* cr, uint16 toHx, uint16 toHy, uint cut)
{
    if (toHx >= self->GetWidth() || toHy >= self->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        if (GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) > cr->GetMultihex() && cut == 0) {
            return {GeometryHelper::GetFarDir(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy)};
        }
        return {};
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !self->CutPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, static_cast<int>(cut))) {
        return {};
    }

    if (cut > 0 && GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), init_to_hx, init_to_hy) <= cut + cr->GetMultihex() && GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        return {};
    }

    auto result = self->FindPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, -1);
    if (!result) {
        return {};
    }

    return result->Steps;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Map_GetPathLength(MapView* self, uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)
{
    if (fromHx >= self->GetWidth() || fromHy >= self->GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= self->GetWidth() || toHy >= self->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (GeometryHelper::DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        return cut > 0 ? 0 : 1;
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !self->CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, static_cast<int>(cut))) {
        return 0;
    }

    if (cut > 0 && GeometryHelper::DistGame(fromHx, fromHy, init_to_hx, init_to_hy) <= cut && GeometryHelper::DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        return 0;
    }

    const auto result = self->FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, -1);
    if (!result) {
        return 0;
    }

    return static_cast<uint>(result->Steps.size());
}

///# ...
///# param cr ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Map_GetPathLength(MapView* self, CritterView* cr, uint16 toHx, uint16 toHy, uint cut)
{
    if (toHx >= self->GetWidth() || toHy >= self->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        return cut > 0 ? 0 : 1;
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !self->CutPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, static_cast<int>(cut))) {
        return 0;
    }

    if (cut > 0 && GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), init_to_hx, init_to_hy) <= cut + cr->GetMultihex() && GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        return 0;
    }

    const auto result = self->FindPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, -1);
    if (!result) {
        return 0;
    }

    return static_cast<uint>(result->Steps.size());
}

///# ...
///# param hx ...
///# param hy ...
///# param speed ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_MoveScreenToHex(MapView* self, uint16 hx, uint16 hy, uint speed, bool canStop)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    if (speed == 0u) {
        self->FindSetCenter(hx, hy);
    }
    else {
        self->ScrollToHex(hx, hy, static_cast<float>(speed) / 1000.0f, canStop);
    }
}

///# ...
///# param ox ...
///# param oy ...
///# param speed ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_MoveScreenOffset(MapView* self, int ox, int oy, uint speed, bool canStop)
{
    self->ScrollOffset(ox, oy, static_cast<float>(speed) / 1000.0f, canStop);
}

///# ...
///# param cr ...
///# param softLock ...
///# param unlockIfSame ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_LockScreenScroll(MapView* self, CritterView* cr, bool softLock, bool unlockIfSame)
{
    const auto id = cr != nullptr ? cr->GetId() : ident_t {};
    if (softLock) {
        if (unlockIfSame && id == self->AutoScroll.SoftLockedCritter) {
            self->AutoScroll.SoftLockedCritter = ident_t {};
        }
        else {
            self->AutoScroll.SoftLockedCritter = id;
        }

        self->AutoScroll.CritterLastHexX = (cr != nullptr ? cr->GetHexX() : 0);
        self->AutoScroll.CritterLastHexY = (cr != nullptr ? cr->GetHexY() : 0);
    }
    else {
        if (unlockIfSame && id == self->AutoScroll.HardLockedCritter) {
            self->AutoScroll.HardLockedCritter = ident_t {};
        }
        else {
            self->AutoScroll.HardLockedCritter = id;
        }
    }
}

///# ...
///# param hx ...
///# param hy ...
///# param dir ...
///# param steps ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Map_MoveHexByDir(MapView* self, uint16& hx, uint16& hy, uint8 dir, uint steps)
{
    if (dir >= GameSettings::MAP_DIR_COUNT) {
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
            result |= GeometryHelper::MoveHexByDir(hx_, hy_, dir, self->GetWidth(), self->GetHeight());
        }
    }
    else {
        result = GeometryHelper::MoveHexByDir(hx_, hy_, dir, self->GetWidth(), self->GetHeight());
    }

    hx = hx_;
    hy = hy_;
    return result;
}

///# ...
///# param hx ...
///# param hy ...
///# param roof ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Client_Map_GetTile(MapView* self, uint16 hx, uint16 hy, bool roof)
{
    if (hx >= self->GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= self->GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    return self->GetTile(hx, hy, roof, -1);
}

///# ...
///# param hx ...
///# param hy ...
///# param roof ...
///# param layer ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Client_Map_GetTile(MapView* self, uint16 hx, uint16 hy, bool roof, uint8 layer)
{
    if (hx >= self->GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= self->GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    return self->GetTile(hx, hy, roof, layer);
}

///# ...
///# param hx ...
///# param hy ...
///# param roof ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Client_Map_GetTiles(MapView* self, uint16 hx, uint16 hy, bool roof)
{
    if (hx >= self->GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= self->GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    return vec_cast<ItemView*>(self->GetTiles(hx, hy, roof));
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_RedrawMap(MapView* self)
{
    self->RefreshMap();
}

///# ...
///# param targetZoom ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_ChangeZoom(MapView* self, float targetZoom)
{
    if (is_float_equal(targetZoom, self->GetSpritesZoom())) {
        return;
    }

    const auto init_zoom = self->GetSpritesZoom();

    if (targetZoom == 1.0f) {
        self->ChangeZoom(0);
    }
    else if (targetZoom > self->GetSpritesZoom()) {
        while (targetZoom > self->GetSpritesZoom()) {
            const auto old_zoom = self->GetSpritesZoom();

            self->ChangeZoom(1);

            if (is_float_equal(self->GetSpritesZoom(), old_zoom)) {
                break;
            }
        }
    }
    else if (targetZoom < self->GetSpritesZoom()) {
        while (targetZoom < self->GetSpritesZoom()) {
            const auto old_zoom = self->GetSpritesZoom();

            self->ChangeZoom(-1);

            if (is_float_equal(self->GetSpritesZoom(), old_zoom)) {
                break;
            }
        }
    }

    if (!is_float_equal(init_zoom, self->GetSpritesZoom())) {
        self->RebuildFog();
    }
}

///# ...
///# param hx ...
///# param hy ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_GetHexScreenPos(MapView* self, uint16 hx, uint16 hy, int& x, int& y)
{
    x = 0;
    y = 0;

    if (hx < self->GetWidth() && hy < self->GetHeight()) {
        self->GetHexCurrentPosition(hx, hy, x, y);
        x += self->GetEngine()->Settings.ScrOx + (self->GetEngine()->Settings.MapHexWidth / 2);
        y += self->GetEngine()->Settings.ScrOy + (self->GetEngine()->Settings.MapHexHeight / 2);
        x = static_cast<int>(static_cast<float>(x) / self->GetSpritesZoom());
        y = static_cast<int>(static_cast<float>(y) / self->GetSpritesZoom());
        return true;
    }

    return false;
}

///# ...
///# param x ...
///# param y ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_GetHexAtScreenPos(MapView* self, int x, int y, uint16& hx, uint16& hy)
{
    uint16 hx_ = 0;
    uint16 hy_ = 0;
    const auto result = self->GetHexAtScreenPos(x, y, hx_, hy_, nullptr, nullptr);
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
///# param hx ...
///# param hy ...
///# param ox ...
///# param oy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_GetHexAtScreenPos(MapView* self, int x, int y, uint16& hx, uint16& hy, int& ox, int& oy)
{
    uint16 hx_ = 0;
    uint16 hy_ = 0;
    const auto result = self->GetHexAtScreenPos(x, y, hx_, hy_, &ox, &oy);
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
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Client_Map_GetItemAtScreenPos(MapView* self, int x, int y)
{
    bool item_egg;
    return self->GetItemAtScreenPos(x, y, item_egg, 0, true);
}

///# ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Map_GetCritterAtScreenPos(MapView* self, int x, int y)
{
    return self->GetCritterAtScreenPos(x, y, false, 0, true);
}

///# ...
///# param x ...
///# param y ...
///# param extraRange
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Map_GetCritterAtScreenPos(MapView* self, int x, int y, int extraRange)
{
    auto* cr = self->GetCritterAtScreenPos(x, y, false, 0, true);
    if (cr == nullptr && extraRange != 0) {
        cr = self->GetCritterAtScreenPos(x, y, true, extraRange, false);
    }
    return cr;
}

///# ...
///# param x ...
///# param y ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ClientEntity* Client_Map_GetEntityAtScreenPos(MapView* self, int x, int y)
{
    return self->GetEntityAtScreenPos(x, y, 0, true);
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_IsMapHexPassed(MapView* self, uint16 hx, uint16 hy)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    return !self->GetField(hx, hy).Flags.IsMoveBlocked;
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_IsMapHexShooted(MapView* self, uint16 hx, uint16 hy)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    return !self->GetField(hx, hy).Flags.IsShootBlocked;
}

///# ...
///# param enabled ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_SetShootBorders(MapView* self, bool enabled)
{
    self->SetShootBorders(enabled);
}

///# ...
///@ ExportMethod
[[maybe_unused]] SpritePattern* Client_Map_RunSpritePattern(MapView* self, string_view spriteName, uint spriteCount)
{
    if (spriteCount < 1) {
        throw ScriptException("Invalid sprite count");
    }

    return self->RunSpritePattern(spriteName, spriteCount);
}

///@ ExportMethod
[[maybe_unused]] void Client_Map_SetCursorPos(MapView* self, CritterView* cr, int mouseX, int mouseY, bool showSteps, bool forceRefresh)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    self->SetCursorPos(hex_cr, mouseX, mouseY, showSteps, forceRefresh);
}
