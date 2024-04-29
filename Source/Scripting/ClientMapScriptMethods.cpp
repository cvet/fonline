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
///# param hex ...
///# param showTime ...
///# param color ...
///# param fade ...
///# param endOffset ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_Message(MapView* self, string_view text, mpos hex, tick_t showTime, ucolor color, bool fade, ipos endOffset)
{
    self->AddMapText(text, hex, color, std::chrono::milliseconds {showTime.underlying_value()}, fade, endOffset);
}

///# ...
///# param mapSpr ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_DrawMapSprite(MapView* self, MapSpriteData* mapSpr)
{
    if (mapSpr == nullptr) {
        throw ScriptException("Map sprite arg is null");
    }

    if (!self->GetSize().IsValidPos(mapSpr->Hex)) {
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
        const auto* proto_item = self->GetEngine()->ProtoMngr.GetProtoItem(mapSpr->ProtoId);
        if (proto_item == nullptr) {
            return;
        }

        color = proto_item->GetIsColorize() ? proto_item->GetLightColor() : ucolor::clear;
        is_flat = proto_item->GetIsFlat();
        const auto is_item = proto_item->GetIsScenery() || proto_item->GetIsWall();
        no_light = is_flat && !is_item;
        draw_order = is_flat ? (is_item ? DrawOrderType::FlatItem : DrawOrderType::FlatScenery) : (is_item ? DrawOrderType::Item : DrawOrderType::Scenery);
        draw_order_hy_offset = static_cast<int>(static_cast<int8>(proto_item->GetDrawOrderOffsetHexY()));
        corner = proto_item->GetCorner();
        disable_egg = proto_item->GetDisableEgg();
        contour_color = proto_item->GetIsBadItem() ? ucolor {255, 0, 0} : ucolor::clear;
    }

    const auto& field = self->GetField(mapSpr->Hex);
    auto& mspr = self->GetDrawList().InsertSprite(draw_order, {mapSpr->Hex.x, static_cast<uint16>(mapSpr->Hex.y + draw_order_hy_offset)}, //
        {(self->GetEngine()->Settings.MapHexWidth / 2) + mapSpr->Offset.x, (self->GetEngine()->Settings.MapHexHeight / 2) + mapSpr->Offset.y}, &field.Offset, anim, nullptr, //
        mapSpr->IsTweakOffs ? &mapSpr->TweakOffset : nullptr, mapSpr->IsTweakAlpha ? &mapSpr->TweakAlpha : nullptr, nullptr, &mapSpr->Valid);

    mspr.MapSpr = mapSpr;
    mapSpr->AddRef();

    if (!no_light) {
        mspr.SetLight(corner, self->GetLightData(), self->GetSize());
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

        mspr.SetEggAppearence(egg_appearence);
    }

    if (color != ucolor::clear) {
        mspr.SetColor(ucolor {color, 0});
        mspr.SetFixedAlpha(color.comp.a);
    }

    if (contour_color != ucolor::clear) {
        mspr.SetContour(ContourType::Custom, contour_color);
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
            if (!cr->GetIsChosen()) {
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
///# param hex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Client_Map_GetVisibleItemsOnHex(MapView* self, mpos hex)
{
    auto&& hex_items = self->GetItems(hex);

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
            if (cr->GetProtoId() == pid && cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }

    return critters;
}

///# ...
///# param hex ...
///# param radius ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Map_GetCritters(MapView* self, mpos hex, uint radius, CritterFindType findType)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<CritterView*> critters;

    for (auto* cr : self->GetCritters()) {
        if (cr->CheckFind(findType) && GeometryHelper::CheckDist(hex, cr->GetHex(), radius + cr->GetMultihex())) {
            critters.push_back(cr);
        }
    }

    std::sort(critters.begin(), critters.end(), [&hex](const CritterView* cr1, const CritterView* cr2) {
        const uint dist1 = GeometryHelper::DistGame(hex, cr1->GetHex());
        const uint dist2 = GeometryHelper::DistGame(hex, cr2->GetHex());
        return dist1 - std::min(dist1, cr1->GetMultihex()) < dist2 - std::min(dist2, cr2->GetMultihex());
    });

    return critters;
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param angle ...
///# param dist ...
///# param findType ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Map_GetCrittersInPath(MapView* self, mpos fromHex, mpos toHex, float angle, uint dist, CritterFindType findType)
{
    if (!self->GetSize().IsValidPos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    vector<CritterHexView*> critters;

    self->TraceBullet(fromHex, toHex, dist, angle, &critters, findType, nullptr, nullptr, nullptr, true);

    return vec_cast<CritterView*>(critters);
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param angle ...
///# param dist ...
///# param findType ...
///# param preBlockHex ...
///# param blockHex ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<CritterView*> Client_Map_GetCrittersWithBlockInPath(MapView* self, mpos fromHex, mpos toHex, float angle, uint dist, CritterFindType findType, mpos& preBlockHex, mpos& blockHex)
{
    if (!self->GetSize().IsValidPos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    vector<CritterHexView*> critters;

    self->TraceBullet(fromHex, toHex, dist, angle, &critters, findType, &preBlockHex, &blockHex, nullptr, true);

    return vec_cast<CritterView*>(critters);
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param angle ...
///# param dist ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Client_Map_GetHexInPath(MapView* self, mpos fromHex, mpos& toHex, float angle, uint dist)
{
    if (!self->GetSize().IsValidPos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    mpos pre_block;

    self->TraceBullet(fromHex, toHex, dist, angle, nullptr, CritterFindType::Any, &pre_block, nullptr, nullptr, true);

    toHex = pre_block;
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<uint8> Client_Map_GetPath(MapView* self, mpos fromHex, mpos toHex, uint cut)
{
    if (!self->GetSize().IsValidPos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    if (GeometryHelper::DistGame(fromHex, toHex) <= 1) {
        if (GeometryHelper::DistGame(fromHex, toHex) > 0 && cut == 0) {
            return {GeometryHelper::GetFarDir(fromHex, toHex)};
        }
        return {};
    }

    auto to_hex = toHex;
    const auto init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(nullptr, fromHex, to_hex, static_cast<int>(cut))) {
        return {};
    }

    if (cut > 0 && GeometryHelper::DistGame(fromHex, init_to_hex) <= cut && GeometryHelper::DistGame(fromHex, to_hex) <= 1) {
        return {};
    }

    auto result = self->FindPath(nullptr, fromHex, to_hex, -1);
    if (!result) {
        return {};
    }

    return result->DirSteps;
}

///# ...
///# param cr ...
///# param toHex ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<uint8> Client_Map_GetPath(MapView* self, CritterView* cr, mpos toHex, uint cut)
{
    if (!self->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    if (GeometryHelper::DistGame(cr->GetHex(), toHex) <= 1 + cr->GetMultihex()) {
        if (GeometryHelper::DistGame(cr->GetHex(), toHex) > cr->GetMultihex() && cut == 0) {
            return {GeometryHelper::GetFarDir(cr->GetHex(), toHex)};
        }
        return {};
    }

    auto to_hex = toHex;
    const auto init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(hex_cr, cr->GetHex(), to_hex, static_cast<int>(cut))) {
        return {};
    }

    if (cut > 0 && GeometryHelper::DistGame(cr->GetHex(), init_to_hex) <= cut + cr->GetMultihex() && GeometryHelper::DistGame(cr->GetHex(), to_hex) <= 1 + cr->GetMultihex()) {
        return {};
    }

    auto result = self->FindPath(hex_cr, cr->GetHex(), to_hex, -1);
    if (!result) {
        return {};
    }

    return result->DirSteps;
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Map_GetPathLength(MapView* self, mpos fromHex, mpos toHex, uint cut)
{
    if (!self->GetSize().IsValidPos(fromHex)) {
        throw ScriptException("Invalid fromHex arg");
    }
    if (!self->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    if (GeometryHelper::DistGame(fromHex, toHex) <= 1) {
        return cut > 0 ? 0 : 1;
    }

    auto to_hex = toHex;
    const auto init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(nullptr, fromHex, to_hex, static_cast<int>(cut))) {
        return 0;
    }

    if (cut > 0 && GeometryHelper::DistGame(fromHex, init_to_hex) <= cut && GeometryHelper::DistGame(fromHex, to_hex) <= 1) {
        return 0;
    }

    const auto result = self->FindPath(nullptr, fromHex, to_hex, -1);
    if (!result) {
        return 0;
    }

    return static_cast<uint>(result->DirSteps.size());
}

///# ...
///# param cr ...
///# param toHex ...
///# param cut ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Map_GetPathLength(MapView* self, CritterView* cr, mpos toHex, uint cut)
{
    if (!self->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid toHex arg");
    }

    auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    if (GeometryHelper::DistGame(cr->GetHex(), toHex) <= 1 + cr->GetMultihex()) {
        return cut > 0 ? 0 : 1;
    }

    auto to_hex = toHex;
    const auto init_to_hex = toHex;

    if (cut > 0 && !self->CutPath(hex_cr, cr->GetHex(), to_hex, static_cast<int>(cut))) {
        return 0;
    }

    if (cut > 0 && GeometryHelper::DistGame(cr->GetHex(), init_to_hex) <= cut + cr->GetMultihex() && GeometryHelper::DistGame(cr->GetHex(), to_hex) <= 1 + cr->GetMultihex()) {
        return 0;
    }

    const auto result = self->FindPath(hex_cr, cr->GetHex(), to_hex, -1);
    if (!result) {
        return 0;
    }

    return static_cast<uint>(result->DirSteps.size());
}

///# ...
///# param hex ...
///# param speed ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_MoveScreenToHex(MapView* self, mpos hex, uint speed, bool canStop)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    if (speed == 0) {
        self->FindSetCenter(hex);
    }
    else {
        self->ScrollToHex(hex, static_cast<float>(speed) / 1000.0f, canStop);
    }
}

///# ...
///# param offset ...
///# param speed ...
///# param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_MoveScreenOffset(MapView* self, ipos offset, uint speed, bool canStop)
{
    self->ScrollOffset(offset, static_cast<float>(speed) / 1000.0f, canStop);
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

        self->AutoScroll.CritterLastHex = cr != nullptr ? cr->GetHex() : mpos {};
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
///# param hex ...
///# param dir ...
///# param steps ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Map_MoveHexByDir(MapView* self, mpos& hex, uint8 dir, uint steps)
{
    if (dir >= GameSettings::MAP_DIR_COUNT) {
        throw ScriptException("Invalid dir arg");
    }
    if (steps == 0) {
        throw ScriptException("DirSteps arg is zero");
    }

    bool result = false;

    if (steps > 1) {
        for (uint i = 0; i < steps; i++) {
            result |= GeometryHelper::MoveHexByDir(hex, dir, self->GetSize());
        }
    }
    else {
        result = GeometryHelper::MoveHexByDir(hex, dir, self->GetSize());
    }

    return result;
}

///# ...
///# param hex ...
///# param roof ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Client_Map_GetTile(MapView* self, mpos hex, bool roof)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->GetTile(hex, roof, -1);
}

///# ...
///# param hex ...
///# param roof ...
///# param layer ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Client_Map_GetTile(MapView* self, mpos hex, bool roof, uint8 layer)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->GetTile(hex, roof, layer);
}

///# ...
///# param hex ...
///# param roof ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ItemView*> Client_Map_GetTiles(MapView* self, mpos hex, bool roof)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return vec_cast<ItemView*>(self->GetTiles(hex, roof));
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
///# param hex ...
///# param hexOffset ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_GetHexScreenPos(MapView* self, mpos hex, ipos& hexOffset)
{
    hexOffset.x = 0;
    hexOffset.y = 0;

    if (self->GetSize().IsValidPos(hex)) {
        hexOffset = self->GetHexCurrentPosition(hex);
        hexOffset.x += self->GetEngine()->Settings.ScreenOffset.x + (self->GetEngine()->Settings.MapHexWidth / 2);
        hexOffset.y += self->GetEngine()->Settings.ScreenOffset.y + (self->GetEngine()->Settings.MapHexHeight / 2);
        hexOffset.x = static_cast<int>(static_cast<float>(hexOffset.x) / self->GetSpritesZoom());
        hexOffset.y = static_cast<int>(static_cast<float>(hexOffset.y) / self->GetSpritesZoom());

        return true;
    }

    return false;
}

///# ...
///# param pos ...
///# param hex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_GetHexAtScreenPos(MapView* self, ipos pos, mpos& hex)
{
    return self->GetHexAtScreenPos(pos, hex, nullptr);
}

///# ...
///# param pos ...
///# param hex ...
///# param hexOffset ...
///# param oy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_GetHexAtScreenPos(MapView* self, ipos pos, mpos& hex, ipos& hexOffset)
{
    return self->GetHexAtScreenPos(pos, hex, &hexOffset);
}

///# ...
///# param pos ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Client_Map_GetItemAtScreenPos(MapView* self, ipos pos)
{
    bool item_egg;
    return self->GetItemAtScreenPos(pos, item_egg, 0, true);
}

///# ...
///# param pos ...
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Map_GetCritterAtScreenPos(MapView* self, ipos pos)
{
    return self->GetCritterAtScreenPos(pos, false, 0, true);
}

///# ...
///# param pos ...
///# param extraRange
///# return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Map_GetCritterAtScreenPos(MapView* self, ipos pos, int extraRange)
{
    auto* cr = self->GetCritterAtScreenPos(pos, false, 0, true);

    if (cr == nullptr && extraRange != 0) {
        cr = self->GetCritterAtScreenPos(pos, true, extraRange, false);
    }

    return cr;
}

///# ...
///# param pos ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ClientEntity* Client_Map_GetEntityAtScreenPos(MapView* self, ipos pos)
{
    return self->GetEntityAtScreenPos(pos, 0, true);
}

///# ...
///# param hex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_IsHexMovable(MapView* self, mpos hex)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return !self->GetField(hex).Flags.IsMoveBlocked;
}

///# ...
///# param hex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_IsHexShootable(MapView* self, mpos hex)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return !self->GetField(hex).Flags.IsShootBlocked;
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
[[maybe_unused]] void Client_Map_SetCursorPos(MapView* self, CritterView* cr, ipos mousePos, bool showSteps, bool forceRefresh)
{
    auto* hex_cr = dynamic_cast<CritterHexView*>(cr);
    if (hex_cr == nullptr) {
        throw ScriptException("Critter is not on map");
    }

    self->SetCursorPos(hex_cr, mousePos, showSteps, forceRefresh);
}
