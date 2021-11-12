//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Dialogs.h"
#include "Entity.h"
#include "GeometryHelper.h"
#include "ServerScripting.h"
#include "Settings.h"
#include "Timer.h"

#define FO_API_PLAYER_HEADER 1
#include "ScriptApi.h"

class Critter;
class MapManager;

class Player final : public Entity
{
public:
    Player() = delete;
    Player(uint id, ClientConnection* connection, const ProtoCritter* proto, CritterSettings& settings, ServerScriptSystem& script_sys, GameTimer& game_time);
    Player(const Player&) = delete;
    Player(Player&&) noexcept = delete;
    auto operator=(const Player&) = delete;
    auto operator=(Player&&) noexcept = delete;
    ~Player() override;

    [[nodiscard]] auto IsSendDisabled() const -> bool { return DisableSend > 0; }
    [[nodiscard]] auto GetIp() const -> uint;
    [[nodiscard]] auto GetHost() const -> const string&;
    [[nodiscard]] auto GetPort() const -> ushort;

    [[nodiscard]] auto GetOwnedCritter() const -> const Critter* { return _ownedCr; }
    [[nodiscard]] auto GetOwnedCritter() -> Critter* { return _ownedCr; }

    void Send_Property(NetProperty::Type type, Property* prop, Entity* entity);

    void Send_Move(Critter* from_cr, uint move_params);
    void Send_Dir(Critter* from_cr);
    void Send_AddCritter(Critter* cr);
    void Send_RemoveCritter(Critter* cr);
    void Send_LoadMap(Map* map, MapManager& map_mngr);
    void Send_Position(Critter* cr);
    void Send_AddItemOnMap(Item* item);
    void Send_EraseItemFromMap(Item* item);
    void Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm);
    void Send_AddItem(Item* item);
    void Send_EraseItem(Item* item);
    void Send_GlobalInfo(uchar flags, MapManager& map_mngr);
    void Send_GlobalLocation(Location* loc, bool add);
    void Send_GlobalMapFog(ushort zx, ushort zy, uchar fog);
    void Send_CustomCommand(Critter* cr, ushort cmd, int val);
    void Send_AllProperties();
    void Send_Talk();
    void Send_GameInfo(Map* map);
    void Send_Text(Critter* from_cr, const string& text, uchar how_say);
    void Send_TextEx(uint from_id, const string& text, uchar how_say, bool unsafe_text);
    void Send_TextMsg(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg);
    void Send_TextMsg(uint from_id, uint num_str, uchar how_say, ushort num_msg);
    void Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const string& lexems);
    void Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, const string& lexems);
    void Send_Action(Critter* from_cr, int action, int action_ext, Item* item);
    void Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot);
    void Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void Send_SetAnims(Critter* from_cr, int cond, uint anim1, uint anim2);
    void Send_CombatResult(uint* combat_res, uint len);
    void Send_AutomapsInfo(void* locs_vec, Location* loc);
    void Send_Effect(hash eff_pid, ushort hx, ushort hy, ushort radius);
    void Send_FlyEffect(hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy);
    void Send_PlaySound(uint crid_synchronize, const string& sound_name);
    void Send_MapText(ushort hx, ushort hy, uint color, const string& text, bool unsafe_text);
    void Send_MapTextMsg(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str);
    void Send_MapTextMsgLex(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, const string& lexems);
    void Send_ViewMap();
    void Send_SomeItem(Item* item); // Without checks
    void Send_CustomMessage(uint msg);
    void Send_AddAllItems();
    void Send_AllAutomapsInfo(MapManager& map_mngr);
    void Send_SomeItems(const vector<Item*>* items, int param);

    string Name {};
    ClientConnection* Connection {};
    int DisableSend {};
    uchar Access {ACCESS_CLIENT};
    uint LanguageMsg {};
    bool IsTransferring {true};
    string LastSay {};
    uint LastSayEqualCount {};

#define FO_API_PLAYER_CLASS 1
#include "ScriptApi.h"

    PROPERTIES_HEADER();
#define FO_API_PLAYER_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"

private:
    CritterSettings& _settings;
    ServerScriptSystem& _scriptSys;
    GameTimer& _gameTime;
    Critter* _ownedCr {}; // Todo: allow attach many critters to sigle player
    uint _talkNextTick {};
};
