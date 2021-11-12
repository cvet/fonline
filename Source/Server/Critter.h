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

#include "Dialogs.h"
#include "Entity.h"
#include "GeometryHelper.h"
#include "ServerScripting.h"
#include "Settings.h"
#include "Timer.h"

#define FO_API_CRITTER_HEADER 1
#include "ScriptApi.h"

struct PathStep
{
    ushort HexX {};
    ushort HexY {};
    uint MoveParams {};
    uchar Dir {};
};

class Player;
class MapManager;
class Item;
class Map;
class Location;

class Critter final : public Entity
{
    friend class Player;
    friend class CritterManager;

public:
    struct MovingData
    {
        int State {1};
        uint TargId {};
        ushort HexX {};
        ushort HexY {};
        uint Cut {};
        bool IsRun {};
        vector<PathStep> Steps {};
        uint Iter {};
        uint Trace {};
        uint GagEntityId {};
    };

    Critter() = delete;
    Critter(uint id, Player* owner, const ProtoCritter* proto, CritterSettings& settings, ServerScriptSystem& script_sys, GameTimer& game_time);
    Critter(const Critter&) = delete;
    Critter(Critter&&) noexcept = delete;
    auto operator=(const Critter&) = delete;
    auto operator=(Critter&&) noexcept = delete;
    ~Critter() override;

    [[nodiscard]] auto IsPlayer() const -> bool { return _player != nullptr || _playerDetached; } // Todo: rename to IsOwnedByPlayer
    [[nodiscard]] auto IsNpc() const -> bool { return _player == nullptr && !_playerDetached; } // Todo: replace to !IsOwnedByPlayer
    [[nodiscard]] auto GetOwner() const -> const Player* { return _player; }
    [[nodiscard]] auto GetOwner() -> Player* { return _player; }
    [[nodiscard]] auto GetOfflineTime() const -> uint;
    [[nodiscard]] auto GetAttackDist(Item* weap, uchar use) -> uint;
    [[nodiscard]] auto IsAlive() const -> bool;
    [[nodiscard]] auto IsDead() const -> bool;
    [[nodiscard]] auto IsKnockout() const -> bool;
    [[nodiscard]] auto CheckFind(uchar find_type) const -> bool;
    [[nodiscard]] auto GetItem(uint item_id, bool skip_hide) -> Item*;
    [[nodiscard]] auto GetItemsNoLock() -> vector<Item*>& { return _invItems; }
    [[nodiscard]] auto GetItemByPid(hash item_pid) -> Item*;
    [[nodiscard]] auto GetItemByPidSlot(hash item_pid, int slot) -> Item*;
    [[nodiscard]] auto GetItemSlot(int slot) -> Item*;
    [[nodiscard]] auto GetItemsSlot(int slot) -> vector<Item*>;
    [[nodiscard]] auto CountItemPid(hash item_pid) -> uint;
    [[nodiscard]] auto RealCountItems() const -> uint { return static_cast<uint>(_invItems.size()); }
    [[nodiscard]] auto CountItems() -> uint;
    [[nodiscard]] auto GetInventory() -> vector<Item*>&;
    [[nodiscard]] auto IsHaveGeckItem() -> bool;
    [[nodiscard]] auto GetCrSelf(uint crid) -> Critter*;
    [[nodiscard]] auto GetCrFromVisCr(uchar find_type, bool vis_cr_self) -> vector<Critter*>;
    [[nodiscard]] auto GetGlobalMapCritter(uint cr_id) const -> Critter*;
    [[nodiscard]] auto IsTransferTimeouts(bool send) -> bool;
    [[nodiscard]] auto IsTalking() const -> bool;
    [[nodiscard]] auto GetTalkedPlayers() const -> uint;
    [[nodiscard]] auto IsTalkedPlayers() const -> bool;
    [[nodiscard]] auto GetBarterPlayers() const -> uint;
    [[nodiscard]] auto IsFreeToTalk() const -> bool;

    auto AddCrIntoVisVec(Critter* add_cr) -> bool;
    auto DelCrFromVisVec(Critter* del_cr) -> bool;
    auto AddCrIntoVisSet1(uint crid) -> bool;
    auto AddCrIntoVisSet2(uint crid) -> bool;
    auto AddCrIntoVisSet3(uint crid) -> bool;
    auto DelCrFromVisSet1(uint crid) -> bool;
    auto DelCrFromVisSet2(uint crid) -> bool;
    auto DelCrFromVisSet3(uint crid) -> bool;
    auto AddIdVisItem(uint item_id) -> bool;
    auto DelIdVisItem(uint item_id) -> bool;
    auto CountIdVisItem(uint item_id) const -> bool;

    void DetachPlayer();
    void AttachPlayer(Player* owner);
    void ClearVisible();
    void SetItem(Item* item);
    auto SetScript(const string& func, bool first_time) -> bool;
    void SendMessage(int num, int val, int to, MapManager& map_mngr);
    void AddCrTimeEvent(hash func_num, uint rate, uint duration, int identifier) const;
    void EraseCrTimeEvent(int index);
    void ContinueTimeEvents(int offs_time);

    void Broadcast_Property(NetProperty::Type type, Property* prop, Entity* entity);
    void Broadcast_Move(uint move_params);
    void Broadcast_Position();
    void Broadcast_Action(int action, int action_ext, Item* item);
    void Broadcast_Dir();
    void Broadcast_CustomCommand(ushort num_param, int val);

    void SendAndBroadcast_Action(int action, int action_ext, Item* item);
    void SendAndBroadcast_MoveItem(Item* item, uchar action, uchar prev_slot);
    void SendAndBroadcast_Animate(uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void SendAndBroadcast_SetAnims(int cond, uint anim1, uint anim2);
    void SendAndBroadcast_Text(const vector<Critter*>& to_cr, const string& text, uchar how_say, bool unsafe_text);
    void SendAndBroadcast_Msg(const vector<Critter*>& to_cr, uint num_str, uchar how_say, ushort num_msg);
    void SendAndBroadcast_MsgLex(const vector<Critter*>& to_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems);

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
    void Send_TextMsg(Critter* from_cr, uint str_num, uchar how_say, ushort num_msg);
    void Send_TextMsg(uint from_id, uint str_num, uchar how_say, ushort num_msg);
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
    void Send_SomeItem(Item* item);
    void Send_CustomMessage(uint msg);
    void Send_AddAllItems();
    void Send_AllAutomapsInfo(MapManager& map_mngr);
    void Send_SomeItems(const vector<Item*>* items, int param);

    uint Flags {};
    string Name {};
    bool IsRunning {};
    int LockMapTransfers {};
    uint AllowedToDownloadMap {};
    vector<Critter*> VisCr {};
    vector<Critter*> VisCrSelf {};
    map<uint, Critter*> VisCrMap {};
    map<uint, Critter*> VisCrSelfMap {};
    set<uint> VisCr1 {};
    set<uint> VisCr2 {};
    set<uint> VisCr3 {};
    set<uint> VisItem {};
    uint ViewMapId {};
    hash ViewMapPid {};
    ushort ViewMapLook {};
    ushort ViewMapHx {};
    ushort ViewMapHy {};
    uchar ViewMapDir {};
    uint ViewMapLocId {};
    uint ViewMapLocEnt {};
    MovingData Moving {};
    uint CacheValuesNextTick {};
    uint LookCacheValue {};
    vector<Critter*>* GlobalMapGroup {};
    uint RadioMessageSended {};

#define FO_API_CRITTER_CLASS 1
#include "ScriptApi.h"

    PROPERTIES_HEADER();
#define FO_API_CRITTER_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"

    TalkData Talk {}; // Todo: !!!

private:
    CritterSettings& _settings;
    GeometryHelper _geomHelper;
    ServerScriptSystem& _scriptSys;
    GameTimer& _gameTime;
    Player* _player {};
    bool _playerDetached {};
    uint _playerDetachTick {};
    vector<Item*> _invItems {};
    TalkData _talk {};
    uint _talkNextTick {};
};
