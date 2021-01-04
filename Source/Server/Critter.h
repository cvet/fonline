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

#define FO_API_CRITTER_HEADER 1
#include "ScriptApi.h"

#define CLIENT_OUTPUT_BEGIN(cl) \
    { \
        std::lock_guard locker((cl)->Connection->BoutLocker)
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

struct PathStep
{
    ushort HexX {};
    ushort HexY {};
    uint MoveParams {};
    uchar Dir {};
};

class MapManager;
class Item;
class Npc;
class Client;
class Map;
class Location;

class Critter : public Entity
{
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
    Critter(const Critter&) = delete;
    Critter(Critter&&) noexcept = delete;
    auto operator=(const Critter&) = delete;
    auto operator=(Critter&&) noexcept = delete;
    ~Critter() override = default;

    [[nodiscard]] auto IsPlayer() const -> bool { return !CritterIsNpc; }
    [[nodiscard]] auto IsNpc() const -> bool { return CritterIsNpc; }
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
    [[nodiscard]] auto IsFree() const -> bool;
    [[nodiscard]] auto IsBusy() const -> bool;
    [[nodiscard]] auto IsWait() const -> bool;
    [[nodiscard]] auto IsSendDisabled() const -> bool { return DisableSend > 0; }
    [[nodiscard]] auto GetCrSelf(uint crid) -> Critter*;
    [[nodiscard]] auto GetCrFromVisCr(uchar find_type, bool vis_cr_self) -> vector<Critter*>;
    [[nodiscard]] auto GetGlobalMapCritter(uint cr_id) const -> Critter*;
    [[nodiscard]] auto IsTransferTimeouts(bool send) -> bool;

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

    void ClearVisible();
    void SetItem(Item* item);
    auto SetScript(const string& func, bool first_time) -> bool;
    void SetBreakTime(uint ms);
    void SetBreakTimeDelta(uint ms);
    void SetWait(uint ms);
    void SendMessage(int num, int val, int to, MapManager& map_mngr);
    void AddCrTimeEvent(hash func_num, uint rate, uint duration, int identifier) const;
    void EraseCrTimeEvent(int index);
    void ContinueTimeEvents(int offs_time);

    void Broadcast_Property(NetProperty::Type type, Property* prop, Entity* entity);
    void Broadcast_Move(uint move_params);
    void Broadcast_XY();
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

    virtual void Send_Property(NetProperty::Type type, Property* prop, Entity* entity) { }
    virtual void Send_Move(Critter* from_cr, uint move_params) { }
    virtual void Send_Dir(Critter* from_cr) { }
    virtual void Send_AddCritter(Critter* cr) { }
    virtual void Send_RemoveCritter(Critter* cr) { }
    virtual void Send_LoadMap(Map* map, MapManager& map_mngr) { }
    virtual void Send_XY(Critter* cr) { }
    virtual void Send_AddItemOnMap(Item* item) { }
    virtual void Send_EraseItemFromMap(Item* item) { }
    virtual void Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm) { }
    virtual void Send_AddItem(Item* item) { }
    virtual void Send_EraseItem(Item* item) { }
    virtual void Send_GlobalInfo(uchar flags, MapManager& map_mngr) { }
    virtual void Send_GlobalLocation(Location* loc, bool add) { }
    virtual void Send_GlobalMapFog(ushort zx, ushort zy, uchar fog) { }
    virtual void Send_CustomCommand(Critter* cr, ushort cmd, int val) { }
    virtual void Send_AllProperties() { }
    virtual void Send_Talk() { }
    virtual void Send_GameInfo(Map* map) { }
    virtual void Send_Text(Critter* from_cr, const string& text, uchar how_say) { }
    virtual void Send_TextEx(uint from_id, const string& text, uchar how_say, bool unsafe_text) { }
    virtual void Send_TextMsg(Critter* from_cr, uint str_num, uchar how_say, ushort num_msg) { }
    virtual void Send_TextMsg(uint from_id, uint str_num, uchar how_say, ushort num_msg) { }
    virtual void Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems) { }
    virtual void Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems) { }
    virtual void Send_Action(Critter* from_cr, int action, int action_ext, Item* item) { }
    virtual void Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot) { }
    virtual void Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play) { }
    virtual void Send_SetAnims(Critter* from_cr, int cond, uint anim1, uint anim2) { }
    virtual void Send_CombatResult(uint* combat_res, uint len) { }
    virtual void Send_AutomapsInfo(void* locs_vec, Location* loc) { }
    virtual void Send_Effect(hash eff_pid, ushort hx, ushort hy, ushort radius) { }
    virtual void Send_FlyEffect(hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy) { }
    virtual void Send_PlaySound(uint crid_synchronize, const string& sound_name) { }

    bool CritterIsNpc {};
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
    int DisableSend {};
    vector<Critter*>* GlobalMapGroup {};
    bool CanBeRemoved {};

#define FO_API_CRITTER_CLASS 1
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
    vector<Item*> _invItems {};

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
    [[nodiscard]] auto GetOfflineTime() const -> uint;
    [[nodiscard]] auto IsToPing() const -> bool;
    [[nodiscard]] auto IsTalking() const -> bool;

    void Disconnect();
    void RemoveFromGame();
    void PingClient();
    void PingOk(uint next_ping);

    void Send_Property(NetProperty::Type type, Property* prop, Entity* entity) override;
    void Send_Move(Critter* from_cr, uint move_params) override;
    void Send_Dir(Critter* from_cr) override;
    void Send_AddCritter(Critter* cr) override;
    void Send_RemoveCritter(Critter* cr) override;
    void Send_LoadMap(Map* map, MapManager& map_mngr) override;
    void Send_XY(Critter* cr) override;
    void Send_AddItemOnMap(Item* item) override;
    void Send_EraseItemFromMap(Item* item) override;
    void Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm) override;
    void Send_AddItem(Item* item) override;
    void Send_EraseItem(Item* item) override;
    void Send_GlobalInfo(uchar flags, MapManager& map_mngr) override;
    void Send_GlobalLocation(Location* loc, bool add) override;
    void Send_GlobalMapFog(ushort zx, ushort zy, uchar fog) override;
    void Send_CustomCommand(Critter* cr, ushort cmd, int val) override;
    void Send_AllProperties() override;
    void Send_Talk() override;
    void Send_GameInfo(Map* map) override;
    void Send_Text(Critter* from_cr, const string& text, uchar how_say) override;
    void Send_TextEx(uint from_id, const string& text, uchar how_say, bool unsafe_text) override;
    void Send_TextMsg(Critter* from_cr, uint str_num, uchar how_say, ushort num_msg) override;
    void Send_TextMsg(uint from_id, uint str_num, uchar how_say, ushort num_msg) override;
    void Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems) override;
    void Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems) override;
    void Send_Action(Critter* from_cr, int action, int action_ext, Item* item) override;
    void Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot) override;
    void Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play) override;
    void Send_SetAnims(Critter* from_cr, int cond, uint anim1, uint anim2) override;
    void Send_CombatResult(uint* combat_res, uint len) override;
    void Send_AutomapsInfo(void* locs_vec, Location* loc) override;
    void Send_Effect(hash eff_pid, ushort hx, ushort hy, ushort radius) override;
    void Send_FlyEffect(hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy) override;
    void Send_PlaySound(uint crid_synchronize, const string& sound_name) override;

    void Send_MapText(ushort hx, ushort hy, uint color, const string& text, bool unsafe_text);
    void Send_MapTextMsg(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str);
    void Send_MapTextMsgLex(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, const char* lexems, ushort lexems_len);
    void Send_ViewMap();
    void Send_SomeItem(Item* item); // Without checks
    void Send_CustomMessage(uint msg);
    void Send_CustomMessage(uint msg, uchar* data, uint data_size);
    void Send_AddAllItems();
    void Send_AllAutomapsInfo(MapManager& map_mngr);
    void Send_SomeItems(const vector<Item*>* items, int param);

    NetConnection* Connection {};
    uchar Access {ACCESS_CLIENT};
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
