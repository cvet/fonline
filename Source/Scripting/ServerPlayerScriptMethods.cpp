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

#include "Server.h"

FO_BEGIN_NAMESPACE

// SyncScope: requires self; reads connection endpoint only.
///@ ExportMethod
FO_SCRIPT_API string Server_Player_GetHost(ptr<Player> self)
{
    return string(self->GetConnection()->GetHost());
}

// SyncScope: requires self; reads connection endpoint only.
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Player_GetPort(ptr<Player> self)
{
    return numeric_cast<int32_t>(self->GetConnection()->GetPort());
}

// SyncScope: requires self; closes this player's connection.
///@ ExportMethod
FO_SCRIPT_API void Server_Player_Disconnect(ptr<Player> self)
{
    self->GetConnection()->GracefulDisconnect();
}

// SyncScope: requires self; forcibly closes this player's connection.
///@ ExportMethod
FO_SCRIPT_API void Server_Player_HardDisconnect(ptr<Player> self)
{
    self->GetConnection()->HardDisconnect();
}

// SyncScope: requires self; mutates player name only.
///@ ExportMethod
FO_SCRIPT_API void Server_Player_SetName(ptr<Player> self, string_view name)
{
    if (name.empty()) {
        throw ScriptException("Player name arg is empty");
    }
    if (strvex(name).trim() != name) {
        throw ScriptException("Wrong player name (trimmed space)");
    }
    if (!strvex(name).is_valid_utf8()) {
        throw ScriptException("Wrong player name encoding");
    }

    self->SetName(name);
}

// SyncScope: requires self + cr when cr is non-null; before linkage callers must explicitly cover both.
///@ ExportMethod
FO_SCRIPT_API void Server_Player_SwitchCritter(ptr<Player> self, nptr<Critter> cr)
{
    ValidateEntityAccess(cr);

    self->GetEngine()->SwitchPlayerCritter(self, cr);
}

// SyncScope: requires self; returns the controlled critter handle, auto-widened when self is Sync'd.
///@ ExportMethod
FO_SCRIPT_API nptr<Critter> Server_Player_GetControlledCritter(ptr<Player> self)
{
    auto controlled_cr = self->GetControlledCritter();
    return controlled_cr;
}

// SyncScope: requires self + cr; sends movement state for a critter visible on player's current map.
///@ ExportMethod
FO_SCRIPT_API void Server_Player_RefreshCritterMoving(ptr<Player> self, ptr<Critter> cr)
{
    ValidateEntityAccess(cr);

    if (cr->GetMapId() == ident_t {}) {
        throw ScriptException("Critter is not on map");
    }

    ident_t player_map_id {};

    if (auto controlled_cr = self->GetControlledCritter()) {
        player_map_id = controlled_cr->GetMapId();
    }
    else if (auto view_map = self->GetViewMap(); view_map) {
        player_map_id = view_map->MapId;
    }

    if (player_map_id != cr->GetMapId()) {
        throw ScriptException("Critter is not on player's current map");
    }

    self->Send_Moving(cr);
}

// SyncScope: requires self; returns the map self currently spectates, but does not cover it for later reads.
// The view target is an independent Map root that self's cover does not include, so a caller that reads or
// mutates it covers the returned map first and re-reads this handle to prove the view did not change.
///@ ExportMethod
FO_SCRIPT_API nptr<Map> Server_Player_GetViewMapTarget(ptr<Player> self)
{
    return self->GetViewMapTarget();
}

// SyncScope: requires self + map + map location; sends load-map data and opens a map view for self.
///@ ExportMethod
FO_SCRIPT_API void Server_Player_ViewMap(ptr<Player> self, ptr<Map> map, mpos hex)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot view a map for a player that is being destroyed", self->GetId());
    }
    if (self->GetControlledCritter()) {
        throw ScriptException("Player controls critter");
    }

    ValidateEntityAccess(map);

    if (!map->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    auto loc = map->GetLocation();
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    ValidateEntityAccess(loc);

    self->SetViewMap(map, hex);
    self->Send_LoadMap(map);
    self->GetEngine()->MapMngr.ViewMap(self, map);
    self->Send_ViewMap();
    self->Send_PlaceToGameComplete();
}

// SyncScope: requires self; clears view-map state, no entity reparent.
///@ ExportMethod
FO_SCRIPT_API void Server_Player_ResetViewMap(ptr<Player> self)
{
    self->ResetViewMap();
}

// SyncScope: requires self; clears view-map state and sends unload, no entity reparent.
///@ ExportMethod
FO_SCRIPT_API void Server_Player_UnloadMap(ptr<Player> self)
{
    if (auto controlled_cr = self->GetControlledCritter()) {
        throw ScriptException("Player controls critter");
    }

    self->ResetViewMap();
    self->Send_LoadMap(nullptr);
}

FO_END_NAMESPACE
