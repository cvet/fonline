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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GenericUtils.h"

// ReSharper disable CppInconsistentNaming

///@ ExportMethod
[[maybe_unused]] void Client_Map_RebuildFog(MapView* self)
{
    self->RebuildFog();
}

/// # ...
/// # param fromHx ...
/// # param fromHy ...
/// # param toHx ...
/// # param toHy ...
/// # param cut ...
/// # return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<uchar> Client_Map_GetPath(MapView* self, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, uint cut)
{
    if (fromHx >= self->GetWidth() || fromHy >= self->GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= self->GetWidth() || toHy >= self->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (self->GetEngine()->Geometry.DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        if (self->GetEngine()->Geometry.DistGame(fromHx, fromHy, to_hx, to_hy) > 0 && cut == 0) {
            return {self->GetEngine()->Geometry.GetFarDir(fromHx, fromHy, to_hx, to_hy)};
        }
        return {};
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !self->CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, cut)) {
        return {};
    }

    if (cut > 0 && self->GetEngine()->Geometry.DistGame(fromHx, fromHy, init_to_hx, init_to_hy) <= cut && self->GetEngine()->Geometry.DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        return {};
    }

    auto result = self->FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, -1);
    if (!result) {
        return {};
    }

    return result->Steps;
}

/// # ...
/// # param cr ...
/// # param toHx ...
/// # param toHy ...
/// # param cut ...
/// # return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<uchar> Client_Map_GetPath(MapView* self, CritterView* cr, ushort toHx, ushort toHy, uint cut)
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

    if (self->GetEngine()->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        if (self->GetEngine()->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) > cr->GetMultihex() && cut == 0) {
            return {self->GetEngine()->Geometry.GetFarDir(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy)};
        }
        return {};
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !self->CutPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, cut)) {
        return {};
    }

    if (cut > 0 && self->GetEngine()->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), init_to_hx, init_to_hy) <= cut + cr->GetMultihex() && self->GetEngine()->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        return {};
    }

    auto result = self->FindPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, -1);
    if (!result) {
        return {};
    }

    return result->Steps;
}

/// # ...
/// # param fromHx ...
/// # param fromHy ...
/// # param toHx ...
/// # param toHy ...
/// # param cut ...
/// # return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Map_GetPathLength(MapView* self, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, uint cut)
{
    if (fromHx >= self->GetWidth() || fromHy >= self->GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= self->GetWidth() || toHy >= self->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    auto to_hx = toHx;
    auto to_hy = toHy;

    if (self->GetEngine()->Geometry.DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        return cut > 0 ? 0 : 1;
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !self->CutPath(nullptr, fromHx, fromHy, to_hx, to_hy, cut)) {
        return 0;
    }

    if (cut > 0 && self->GetEngine()->Geometry.DistGame(fromHx, fromHy, init_to_hx, init_to_hy) <= cut && self->GetEngine()->Geometry.DistGame(fromHx, fromHy, to_hx, to_hy) <= 1) {
        return 0;
    }

    const auto result = self->FindPath(nullptr, fromHx, fromHy, to_hx, to_hy, -1);
    if (!result) {
        return 0;
    }

    return static_cast<uint>(result->Steps.size());
}

/// # ...
/// # param cr ...
/// # param toHx ...
/// # param toHy ...
/// # param cut ...
/// # return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] uint Client_Map_GetPathLength(MapView* self, CritterView* cr, ushort toHx, ushort toHy, uint cut)
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

    if (self->GetEngine()->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        return cut > 0 ? 0 : 1;
    }

    const auto init_to_hx = to_hx;
    const auto init_to_hy = to_hy;

    if (cut > 0 && !self->CutPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, cut)) {
        return 0;
    }

    if (cut > 0 && self->GetEngine()->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), init_to_hx, init_to_hy) <= cut + cr->GetMultihex() && self->GetEngine()->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), to_hx, to_hy) <= 1 + cr->GetMultihex()) {
        return 0;
    }

    const auto result = self->FindPath(hex_cr, hex_cr->GetHexX(), hex_cr->GetHexY(), to_hx, to_hy, -1);
    if (!result) {
        return 0;
    }

    return static_cast<uint>(result->Steps.size());
}

/// # ...
/// # param hx ...
/// # param hy ...
/// # param speed ...
/// # param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_MoveScreenToHex(MapView* self, ushort hx, ushort hy, uint speed, bool canStop)
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

/// # ...
/// # param ox ...
/// # param oy ...
/// # param speed ...
/// # param canStop ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_MoveScreenOffset(MapView* self, int ox, int oy, uint speed, bool canStop)
{
    self->ScrollOffset(ox, oy, static_cast<float>(speed) / 1000.0f, canStop);
}

/// # ...
/// # param cr ...
/// # param softLock ...
/// # param unlockIfSame ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_LockScreenScroll(MapView* self, CritterView* cr, bool softLock, bool unlockIfSame)
{
    const auto id = (cr != nullptr ? cr->GetId() : 0);
    if (softLock) {
        if (unlockIfSame && id == self->AutoScroll.SoftLockedCritter) {
            self->AutoScroll.SoftLockedCritter = 0;
        }
        else {
            self->AutoScroll.SoftLockedCritter = id;
        }

        self->AutoScroll.CritterLastHexX = (cr ? cr->GetHexX() : 0);
        self->AutoScroll.CritterLastHexY = (cr ? cr->GetHexY() : 0);
    }
    else {
        if (unlockIfSame && id == self->AutoScroll.HardLockedCritter) {
            self->AutoScroll.HardLockedCritter = 0;
        }
        else {
            self->AutoScroll.HardLockedCritter = id;
        }
    }
}

/// # ...
/// # param hx ...
/// # param hy ...
/// # param dir ...
/// # param steps ...
/// # return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Map_MoveHexByDir(MapView* self, ushort& hx, ushort& hy, uchar dir, uint steps)
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
            result |= self->GetEngine()->Geometry.MoveHexByDir(hx_, hy_, dir, self->GetWidth(), self->GetHeight());
        }
    }
    else {
        result = self->GetEngine()->Geometry.MoveHexByDir(hx_, hy_, dir, self->GetWidth(), self->GetHeight());
    }

    hx = hx_;
    hy = hy_;
    return result;
}

/// # ...
/// # param hx ...
/// # param hy ...
/// # param roof ...
/// # param layer ...
/// # return ...
///@ ExportMethod
[[maybe_unused]] hstring Client_Map_GetTileName(MapView* self, ushort hx, ushort hy, bool roof, int layer)
{
    if (hx >= self->GetWidth()) {
        throw ScriptException("Invalid hex x arg");
    }
    if (hy >= self->GetHeight()) {
        throw ScriptException("Invalid hex y arg");
    }

    const auto* simply_tile = self->GetField(hx, hy).SimplyTile[roof ? 1 : 0];
    if (simply_tile != nullptr && layer == 0) {
        return simply_tile->Name;
    }

    const auto* tiles = self->GetField(hx, hy).Tiles[roof ? 1 : 0];
    if (tiles == nullptr || tiles->empty()) {
        return {};
    }

    for (const auto& tile : *tiles) {
        if (tile.Layer == layer) {
            return tile.Anim->Name;
        }
    }
    return {};
}

/// # ...
/// # param onlyTiles ...
/// # param onlyRoof ...
/// # param onlyLight ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_RedrawMap(MapView* self, bool onlyTiles, bool onlyRoof, bool onlyLight)
{
    if (onlyTiles) {
        self->RebuildTiles();
    }
    else if (onlyRoof) {
        self->RebuildRoof();
    }
    else if (onlyLight) {
        self->RebuildLight();
    }
    else {
        self->RefreshMap();
    }
}

/// # ...
/// # param targetZoom ...
///@ ExportMethod
[[maybe_unused]] void Client_Map_ChangeZoom(MapView* self, float targetZoom)
{
    if (Math::FloatCompare(targetZoom, self->GetSpritesZoom())) {
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

            if (Math::FloatCompare(self->GetSpritesZoom(), old_zoom)) {
                break;
            }
        }
    }
    else if (targetZoom < self->GetSpritesZoom()) {
        while (targetZoom < self->GetSpritesZoom()) {
            const auto old_zoom = self->GetSpritesZoom();

            self->ChangeZoom(-1);

            if (Math::FloatCompare(self->GetSpritesZoom(), old_zoom)) {
                break;
            }
        }
    }

    if (!Math::FloatCompare(init_zoom, self->GetSpritesZoom())) {
        self->RebuildFog();
    }
}

/// # ...
/// # param hx ...
/// # param hy ...
/// # param x ...
/// # param y ...
/// # return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_GetHexScreenPos(MapView* self, ushort hx, ushort hy, int& x, int& y)
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

/// # ...
/// # param x ...
/// # param y ...
/// # param hx ...
/// # param hy ...
/// # return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_GetHexAtScreenPos(MapView* self, int x, int y, ushort& hx, ushort& hy)
{
    ushort hx_ = 0;
    ushort hy_ = 0;
    const auto result = self->GetHexAtScreenPos(x, y, hx_, hy_, nullptr, nullptr);
    if (result) {
        hx = hx_;
        hy = hy_;
        return true;
    }
    return false;
}

/// # ...
/// # param x ...
/// # param y ...
/// # param hx ...
/// # param hy ...
/// # param ox ...
/// # param oy ...
/// # return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_GetHexAtScreenPos(MapView* self, int x, int y, ushort& hx, ushort& hy, int& ox, int& oy)
{
    ushort hx_ = 0;
    ushort hy_ = 0;
    const auto result = self->GetHexAtScreenPos(x, y, hx_, hy_, &ox, &oy);
    if (result) {
        hx = hx_;
        hy = hy_;
        return true;
    }
    return false;
}

/// # ...
/// # param x ...
/// # param y ...
/// # return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Client_Map_GetItemAtScreenPos(MapView* self, int x, int y)
{
    bool item_egg;
    return self->GetItemAtScreenPos(x, y, item_egg, 0, true);
}

/// # ...
/// # param x ...
/// # param y ...
/// # return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Map_GetCritterAtScreenPos(MapView* self, int x, int y)
{
    return self->GetCritterAtScreenPos(x, y, false, 0, true);
}

/// # ...
/// # param x ...
/// # param y ...
/// # param extraRange
/// # return ...
///@ ExportMethod
[[maybe_unused]] CritterView* Client_Map_GetCritterAtScreenPos(MapView* self, int x, int y, int extraRange)
{
    auto* cr = self->GetCritterAtScreenPos(x, y, false, 0, true);
    if (cr == nullptr && extraRange != 0) {
        cr = self->GetCritterAtScreenPos(x, y, true, extraRange, false);
    }
    return cr;
}

/// # ...
/// # param x ...
/// # param y ...
/// # return ...
///@ ExportMethod
[[maybe_unused]] ClientEntity* Client_Map_GetEntityAtScreenPos(MapView* self, int x, int y)
{
    return self->GetEntityAtScreenPos(x, y, 0, true);
}

/// # ...
/// # param hx ...
/// # param hy ...
/// # return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_IsMapHexPassed(MapView* self, ushort hx, ushort hy)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    return !self->GetField(hx, hy).Flags.IsMoveBlocked;
}

/// # ...
/// # param hx ...
/// # param hy ...
/// # return ...
///@ ExportMethod
[[maybe_unused]] bool Client_Map_IsMapHexShooted(MapView* self, ushort hx, ushort hy)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hex args");
    }

    return !self->GetField(hx, hy).Flags.IsShootBlocked;
}

///@ ExportMethod
[[maybe_unused]] void Client_Map_SetShootBorders(MapView* self, bool enabled)
{
    self->SetShootBorders(enabled);
}
