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

#include "Server.h"

FO_BEGIN_NAMESPACE

// Return this player's connection host
///@ ExportMethod
FO_SCRIPT_API string Server_Player_GetHost(ptr<Player> self)
{
    return string(self->GetConnection()->GetHost());
}

// Return this player's connection port
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Player_GetPort(ptr<Player> self)
{
    return numeric_cast<int32_t>(self->GetConnection()->GetPort());
}

// Request graceful disconnection of this player's connection
///@ ExportMethod
FO_SCRIPT_API void Server_Player_Disconnect(ptr<Player> self)
{
    self->GetConnection()->GracefulDisconnect();
}

// Force-close this player's connection with the script request reason
///@ ExportMethod
FO_SCRIPT_API void Server_Player_HardDisconnect(ptr<Player> self)
{
    self->GetConnection()->HardDisconnect(DisconnectReason::ScriptRequest);
}

// Return the reason this player's connection was closed
///@ ExportMethod
FO_SCRIPT_API DisconnectReason Server_Player_GetDisconnectReason(ptr<Player> self)
{
    return self->GetConnection()->GetDisconnectReason();
}

// Set the player name after rejecting empty, untrimmed or invalid UTF-8 input
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

// Switch the player to the specified critter, or detach control when the argument is null
///@ ExportMethod
FO_SCRIPT_API void Server_Player_SwitchCritter(ptr<Player> self, FO_REQUIRES_COVER nptr<Critter> cr)
{
    ValidateEntityAccess(cr);

    self->GetEngine()->SwitchPlayerCritter(self, cr);
}

// Return the critter controlled by this player, or null when none is controlled
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Critter> Server_Player_GetControlledCritter(ptr<Player> self)
{
    auto controlled_cr {self->GetControlledCritter()};
    return controlled_cr;
}

// Send movement state for a critter on the player's current map
///@ ExportMethod
FO_SCRIPT_API void Server_Player_RefreshCritterMoving(ptr<Player> self, ptr<Critter> cr)
{
    ValidateEntityAccess(cr);

    if (cr->GetMapId() == ident_t {}) {
        throw ScriptException("Critter is not on map");
    }

    ident_t player_map_id {};

    if (auto controlled_cr {self->GetControlledCritter()}) {
        player_map_id = controlled_cr->GetMapId();
    }
    else if (auto view_map {self->GetViewMap()}; view_map) {
        player_map_id = view_map->MapId;
    }

    if (player_map_id != cr->GetMapId()) {
        throw ScriptException("Critter is not on player's current map");
    }

    self->Send_Moving(cr);
}

// Return the map this player currently spectates
///@ ExportMethod
FO_SCRIPT_API nptr<Map> Server_Player_GetViewMapTarget(ptr<Player> self)
{
    return self->GetViewMapTarget();
}

// Load the map for a player without a controlled critter and open a full map view
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

    auto loc {map->GetLocation()};
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    ValidateEntityAccess(loc);

    self->SetViewMap(map, hex);
    self->Send_LoadMap(map);
    self->GetEngine()->MapMngr.ViewMap(self, map);
    self->Send_ViewMap();
    self->Send_PlaceToGameComplete();
}

// Open a bounded view of the map after validating the player, hex and positive int16_t radius
///@ ExportMethod
FO_SCRIPT_API void Server_Player_ViewMapArea(ptr<Player> self, ptr<Map> map, mpos hex, int32_t radius)
{
    ValidateEntityAccess(map);

    if (self->IsDestroying() || !map->GetSize().is_valid_pos(hex) || radius <= 0 || radius > std::numeric_limits<int16_t>::max()) {
        throw ScriptException("Invalid bounded map view", self->GetId(), hex, radius);
    }

    auto loc {map->GetLocation()};
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    ValidateEntityAccess(loc);
    self->SetViewMap(map, hex, radius);
    self->Send_LoadMap(map);
    self->GetEngine()->MapMngr.ViewMap(self, map);
    self->Send_ViewMap();
    self->Send_PlaceToGameComplete();
}

// Return a viewing player with a controlled critter to ordinary display
///@ ExportMethod
FO_SCRIPT_API void Server_Player_ReturnFromViewMap(ptr<Player> self)
{
    auto cr {self->GetControlledCritter()};

    if (!cr) {
        throw ScriptException("Map view return requires a controlled critter", self->GetId());
    }

    ValidateEntityAccess(cr);
    self->GetEngine()->ReturnPlayerFromViewMap(self);
}

// Clear view-map state without reparenting any entity
///@ ExportMethod
FO_SCRIPT_API void Server_Player_ResetViewMap(ptr<Player> self)
{
    self->ResetViewMap();
}

// Clear view-map state and send a map unload when the player controls no critter
///@ ExportMethod
FO_SCRIPT_API void Server_Player_UnloadMap(ptr<Player> self)
{
    if (auto controlled_cr {self->GetControlledCritter()}) {
        throw ScriptException("Player controls critter");
    }

    self->ResetViewMap();
    self->Send_LoadMap(nullptr);
}

FO_END_NAMESPACE
