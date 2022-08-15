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

#pragma once

#include "Common.h"

#include "ClientConnection.h"
#include "EntityProperties.h"
#include "EntityProtos.h"
#include "ServerEntity.h"

class Item;
class Critter;
class Map;
class Location;
class MapManager;

class Player final : public ServerEntity, public PlayerProperties
{
public:
    Player() = delete;
    Player(FOServer* engine, uint id, ClientConnection* connection);
    Player(const Player&) = delete;
    Player(Player&&) noexcept = delete;
    auto operator=(const Player&) = delete;
    auto operator=(Player&&) noexcept = delete;
    ~Player() override;

    [[nodiscard]] auto GetName() const -> string_view override;

    [[nodiscard]] auto GetIp() const -> uint;
    [[nodiscard]] auto GetHost() const -> string_view;
    [[nodiscard]] auto GetPort() const -> ushort;

    [[nodiscard]] auto GetOwnedCritter() const -> const Critter* { return _ownedCr; }
    [[nodiscard]] auto GetOwnedCritter() -> Critter* { return _ownedCr; }

    void SetName(string_view name);
    void SetOwnedCritter(Critter* cr);

    void Send_Property(NetProperty type, const Property* prop, Entity* entity);

    void Send_Move(Critter* from_cr);
    void Send_Dir(Critter* from_cr);
    void Send_AddCritter(Critter* cr);
    void Send_RemoveCritter(Critter* cr);
    void Send_LoadMap(Map* map);
    void Send_Position(Critter* cr);
    void Send_AddItemOnMap(Item* item);
    void Send_EraseItemFromMap(Item* item);
    void Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm);
    void Send_AddItem(Item* item);
    void Send_EraseItem(Item* item);
    void Send_GlobalInfo(uchar flags);
    void Send_GlobalLocation(Location* loc, bool add);
    void Send_GlobalMapFog(ushort zx, ushort zy, uchar fog);
    void Send_Teleport(Critter* cr, ushort to_hx, ushort to_hy);
    void Send_AllProperties();
    void Send_Talk();
    void Send_TimeSync();
    void Send_Text(Critter* from_cr, string_view text, uchar how_say);
    void Send_TextEx(uint from_id, string_view text, uchar how_say, bool unsafe_text);
    void Send_TextMsg(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg);
    void Send_TextMsg(uint from_id, uint num_str, uchar how_say, ushort num_msg);
    void Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, string_view lexems);
    void Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, string_view lexems);
    void Send_Action(Critter* from_cr, int action, int action_ext, Item* item);
    void Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot);
    void Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void Send_SetAnims(Critter* from_cr, CritterCondition cond, uint anim1, uint anim2);
    void Send_AutomapsInfo(void* locs_vec, Location* loc);
    void Send_Effect(hstring eff_pid, ushort hx, ushort hy, ushort radius);
    void Send_FlyEffect(hstring eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy);
    void Send_PlaySound(uint crid_synchronize, string_view sound_name);
    void Send_MapText(ushort hx, ushort hy, uint color, string_view text, bool unsafe_text);
    void Send_MapTextMsg(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str);
    void Send_MapTextMsgLex(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, string_view lexems);
    void Send_ViewMap();
    void Send_SomeItem(Item* item); // Without checks
    void Send_PlaceToGameComplete();
    void Send_AddAllItems();
    void Send_AllAutomapsInfo();
    void Send_SomeItems(const vector<Item*>* items, int param);

    ///@ ExportEvent
    ENTITY_EVENT(OnGetAccess, int /*arg1*/, string& /*arg2*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnAllowCommand, string /*arg1*/, uchar /*arg2*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnLogout);

    ClientConnection* Connection {};
    uchar Access {ACCESS_CLIENT};
    string LastSay {};
    uint LastSayEqualCount {};
    const Entity* SendIgnoreEntity {};
    const Property* SendIgnoreProperty {};

private:
    string _name {"(Unlogined)"};
    Critter* _ownedCr {}; // Todo: allow attach many critters to sigle player
    uint _talkNextTick {};
};
