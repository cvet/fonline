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
#include "Networking.h"
#include "ServerScripting.h"
#include "Settings.h"
#include "Timer.h"

#define FO_API_CRITTER_HEADER
#include "ScriptApi.h"

#define CLIENT_OUTPUT_BEGIN(cl) \
    { \
        SCOPE_LOCK((cl)->Connection->BoutLocker)
#define CLIENT_OUTPUT_END(cl) \
    } \
    (cl)->Connection->Dispatch()

enum class ClientState
{
    None,
    Connected,
    Playing,
    Transferring,
};

class MapManager;
class Item;
using ItemVec = vector<Item*>;
using ItemMap = map<uint, Item*>;
class Critter;
using CritterMap = map<uint, Critter*>;
using CritterVec = vector<Critter*>;
class Npc;
using NpcMap = map<uint, Npc*>;
using NpcVec = vector<Npc*>;
class Client;
using ClientMap = map<uint, Client*>;
using ClientVec = vector<Client*>;
class Map;
using MapVec = vector<Map*>;
using MapMap = map<uint, Map*>;
class Location;
using LocationVec = vector<Location*>;
using LocationMap = map<uint, Location*>;

class Critter : public Entity
{
    friend class CritterManager;

public:
    Critter() = delete;
    Critter(const Critter&) = delete;
    Critter(Critter&&) noexcept = delete;
    auto operator=(const Critter&) = delete;
    auto operator=(Critter&&) noexcept = delete;
    ~Critter() override = default;

    void ClearVisible();

    [[nodiscard]] auto GetCritSelf(uint crid) -> Critter*;
    void GetCrFromVisCr(CritterVec& critters, uchar find_type, bool vis_cr_self);
    [[nodiscard]] auto GetGlobalMapCritter(uint cr_id) const -> Critter*;

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

    void SetItem(Item* item);
    [[nodiscard]] auto GetItem(uint item_id, bool skip_hide) -> Item*;
    [[nodiscard]] auto GetItemsNoLock() -> ItemVec& { return _invItems; }
    [[nodiscard]] auto GetItemByPid(hash item_pid) -> Item*;
    [[nodiscard]] auto GetItemByPidSlot(hash item_pid, int slot) -> Item*;
    [[nodiscard]] auto GetItemSlot(int slot) -> Item*;
    void GetItemsSlot(int slot, ItemVec& items);
    [[nodiscard]] auto CountItemPid(hash item_pid) -> uint;
    [[nodiscard]] auto RealCountItems() const -> uint { return static_cast<uint>(_invItems.size()); }
    [[nodiscard]] auto CountItems() -> uint;
    [[nodiscard]] auto GetInventory() -> ItemVec&;
    [[nodiscard]] auto IsHaveGeckItem() -> bool;

    auto SetScript(const string& func, bool first_time) -> bool;

    [[nodiscard]] auto IsFree() const -> bool;
    [[nodiscard]] auto IsBusy() const -> bool;
    void SetBreakTime(uint ms);
    void SetBreakTimeDelta(uint ms);
    void SetWait(uint ms);
    [[nodiscard]] auto IsWait() const -> bool;

    auto IsSendDisabled() const -> bool { return DisableSend > 0; }
    void Send_Property(NetProperty::Type type, Property* prop, Entity* entity);
    void Send_Move(Critter* from_cr, uint move_params);
    void Send_Dir(Critter* from_cr);
    void Send_AddCritter(Critter* cr);
    void Send_RemoveCritter(Critter* cr);
    void Send_LoadMap(Map* map, MapManager& map_mngr);
    void Send_XY(Critter* cr);
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
    void Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems);
    void Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems);
    void Send_Action(Critter* from_cr, int action, int action_ext, Item* item);
    void Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot);
    void Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void Send_SetAnims(Critter* from_cr, int cond, uint anim1, uint anim2);
    void Send_CombatResult(uint* combat_res, uint len);
    void Send_AutomapsInfo(void* locs_vec, Location* loc);
    void Send_Effect(hash eff_pid, ushort hx, ushort hy, ushort radius);
    void Send_FlyEffect(hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy);
    void Send_PlaySound(uint crid_synchronize, const string& sound_name);

    // Send all
    void SendA_Property(NetProperty::Type type, Property* prop, Entity* entity);
    void SendA_Move(uint move_params);
    void SendA_XY();
    void SendA_Action(int action, int action_ext, Item* item);
    void SendAA_Action(int action, int action_ext, Item* item);
    void SendAA_MoveItem(Item* item, uchar action, uchar prev_slot);
    void SendAA_Animate(uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void SendAA_SetAnims(int cond, uint anim1, uint anim2);
    void SendAA_Text(const CritterVec& to_cr, const string& text, uchar how_say, bool unsafe_text);
    void SendAA_Msg(const CritterVec& to_cr, uint num_str, uchar how_say, ushort num_msg);
    void SendAA_MsgLex(const CritterVec& to_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems);
    void SendA_Dir();
    void SendA_CustomCommand(ushort num_param, int val);

    // Chosen data
    void Send_AddAllItems();
    void Send_AllAutomapsInfo(MapManager& map_mngr);

    [[nodiscard]] auto IsPlayer() const -> bool { return !CritterIsNpc; }
    [[nodiscard]] auto IsNpc() const -> bool { return CritterIsNpc; }
    void SendMessage(int num, int val, int to, MapManager& map_mngr);
    [[nodiscard]] auto GetAttackDist(Item* weap, uchar use) -> uint;
    [[nodiscard]] auto IsAlive() const -> bool;
    [[nodiscard]] auto IsDead() const -> bool;
    [[nodiscard]] auto IsKnockout() const -> bool;
    [[nodiscard]] auto CheckFind(uchar find_type) const -> bool;

    // Timeouts
    [[nodiscard]] auto IsTransferTimeouts(bool send) -> bool;

    // Time events
    void AddCrTimeEvent(hash func_num, uint rate, uint duration, int identifier) const;
    void EraseCrTimeEvent(int index);
    void ContinueTimeEvents(int offs_time);

    bool CritterIsNpc {};
    uint Flags {};
    string Name {};
    bool IsRunning {};
    int LockMapTransfers {};
    uint AllowedToDownloadMap {};

    CritterVec VisCr {};
    CritterVec VisCrSelf {};
    CritterMap VisCrMap {};
    CritterMap VisCrSelfMap {};
    UIntSet VisCr1 {};
    UIntSet VisCr2 {};
    UIntSet VisCr3 {};
    UIntSet VisItem {};
    uint ViewMapId {};
    hash ViewMapPid {};
    ushort ViewMapLook {};
    ushort ViewMapHx {};
    ushort ViewMapHy {};
    uchar ViewMapDir {};
    uint ViewMapLocId {};
    uint ViewMapLocEnt {};

    struct
    {
        int State {1};
        uint TargId {};
        ushort HexX {};
        ushort HexY {};
        uint Cut {};
        bool IsRun {};
        uint PathNum {};
        uint Iter {};
        uint Trace {};
        uint GagEntityId {};
    } Moving {};

    uint CacheValuesNextTick {};
    uint LookCacheValue {};

    int DisableSend {};
    CritterVec* GlobalMapGroup {};
    bool CanBeRemoved {};

#define FO_API_CRITTER_CLASS
#include "ScriptApi.h"

    PROPERTIES_HEADER();
#define FO_API_CRITTER_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"

protected:
    Critter(uint id, EntityType type, const ProtoCritter* proto, CritterSettings& settings, ServerScriptSystem& script_sys, GameTimer& game_time);

    CritterSettings& _settings;
    GeometryHelper _geomHelper;
    ServerScriptSystem& _scriptSys;
    GameTimer& _gameTime;
    ItemVec _invItems {};

private:
    uint _startBreakTime {};
    uint _breakTime {};
    uint _waitEndTick {};
};

// Todo: rework Client class to ClientConnection
class Client final : public Critter
{
    friend class CritterManager;

public:
    Client() = delete;
    Client(NetConnection* conn, const ProtoCritter* proto, CritterSettings& settings, ServerScriptSystem& script_sys, GameTimer& game_time);
    Client(const Client&) = delete;
    Client(Client&&) noexcept = delete;
    auto operator=(const Client&) = delete;
    auto operator=(Client&&) noexcept = delete;
    ~Client() override;

    [[nodiscard]] auto GetIp() const -> uint;
    [[nodiscard]] auto GetIpStr() const -> const char*;
    [[nodiscard]] auto GetPort() const -> ushort;
    [[nodiscard]] auto IsOnline() const -> bool;
    [[nodiscard]] auto IsOffline() const -> bool;
    void Disconnect() const;
    void RemoveFromGame();
    [[nodiscard]] auto GetOfflineTime() const -> uint;

    [[nodiscard]] auto IsToPing() const -> bool;
    void PingClient();
    void PingOk(uint next_ping);

    void Send_Property(NetProperty::Type type, Property* prop, Entity* entity);
    void Send_Move(Critter* from_cr, uint move_params) const;
    void Send_Dir(Critter* from_cr) const;
    void Send_AddCritter(Critter* cr);
    void Send_RemoveCritter(Critter* cr) const;
    void Send_LoadMap(Map* map, MapManager& map_mngr);
    void Send_XY(Critter* cr) const;
    void Send_AddItemOnMap(Item* item) const;
    void Send_EraseItemFromMap(Item* item) const;
    void Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm) const;
    void Send_AddItem(Item* item) const;
    void Send_EraseItem(Item* item) const;
    void Send_SomeItems(const ItemVec* items, int param) const;
    void Send_GlobalInfo(uchar flags, MapManager& map_mngr);
    void Send_GlobalLocation(Location* loc, bool add) const;
    void Send_GlobalMapFog(ushort zx, ushort zy, uchar fog) const;
    void Send_CustomCommand(Critter* cr, ushort cmd, int val) const;
    void Send_AllProperties();
    void Send_Talk();
    void Send_GameInfo(Map* map) const;
    void Send_Text(Critter* from_cr, const string& text, uchar how_say) const;
    void Send_TextEx(uint from_id, const string& text, uchar how_say, bool unsafe_text) const;
    void Send_TextMsg(Critter* from_cr, uint str_num, uchar how_say, ushort num_msg) const;
    void Send_TextMsg(uint from_id, uint str_num, uchar how_say, ushort num_msg) const;
    void Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems) const;
    void Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems) const;
    void Send_Action(Critter* from_cr, int action, int action_ext, Item* item) const;
    void Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot) const;
    void Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play) const;
    void Send_SetAnims(Critter* from_cr, int cond, uint anim1, uint anim2) const;
    void Send_CombatResult(uint* combat_res, uint len) const;
    void Send_AutomapsInfo(void* locs_vec, Location* loc) const;
    void Send_Effect(hash eff_pid, ushort hx, ushort hy, ushort radius) const;
    void Send_FlyEffect(hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy) const;
    void Send_PlaySound(uint crid_synchronize, const string& sound_name) const;
    void Send_MapText(ushort hx, ushort hy, uint color, const string& text, bool unsafe_text) const;
    void Send_MapTextMsg(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str) const;
    void Send_MapTextMsgLex(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, const char* lexems, ushort lexems_len) const;
    void Send_ViewMap() const;
    void Send_SomeItem(Item* item) const; // Without checks
    void Send_CustomMessage(uint msg) const;
    void Send_CustomMessage(uint msg, uchar* data, uint data_size) const;

    [[nodiscard]] auto IsTalking() const -> bool;

    NetConnection* Connection {};
    uchar Access {ACCESS_DEFAULT};
    uint LanguageMsg {};
    ClientState State {};
    uint LastActivityTime {};
    uint LastSendedMapTick {};
    string LastSay {};
    uint LastSayEqualCount {};
    uint RadioMessageSended {};
    int UpdateFileIndex {-1};
    uint UpdateFilePortion {};
    TalkData Talk {};

private:
    uint _pingNextTick {};
    bool _pingOk {true};
    uint _talkNextTick {};
};

class Npc final : public Critter
{
    friend class CritterManager;

public:
    Npc() = delete;
    Npc(uint id, const ProtoCritter* proto, CritterSettings& settings, ServerScriptSystem& script_sys, GameTimer& game_time);
    Npc(const Npc&) = delete;
    Npc(Npc&&) noexcept = delete;
    auto operator=(const Npc&) = delete;
    auto operator=(Npc&&) noexcept = delete;
    ~Npc() override = default;

    [[nodiscard]] auto GetTalkedPlayers() const -> uint;
    [[nodiscard]] auto IsTalkedPlayers() const -> bool;
    [[nodiscard]] auto GetBarterPlayers() const -> uint;
    [[nodiscard]] auto IsFreeToTalk() const -> bool;
};
