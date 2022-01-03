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
#include "EntityProperties.h"
#include "EntityProtos.h"
#include "ServerEntity.h"

///@ ExportEnum
enum class MovingState : uchar
{
    InProgress = 0,
    Success = 1,
    TargetNotFound = 2,
    CantMove = 3,
    GagCritter = 4,
    GagItem = 5,
    InternalError = 6,
    HexTooFar = 7,
    HexBusy = 8,
    HexBusyRing = 9,
    Deadlock = 10,
    TraceFail = 11,
};

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

class Critter final : public ServerEntity, public CritterProperties
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
    Critter(FOServer* engine, uint id, Player* owner, const ProtoCritter* proto);
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
    [[nodiscard]] auto GetRawItems() -> vector<Item*>& { return _invItems; }
    [[nodiscard]] auto GetItemByPid(hstring item_pid) -> Item*;
    [[nodiscard]] auto GetItemByPidSlot(hstring item_pid, int slot) -> Item*;
    [[nodiscard]] auto GetItemSlot(int slot) -> Item*;
    [[nodiscard]] auto GetItemsSlot(int slot) -> vector<Item*>;
    [[nodiscard]] auto CountItemPid(hstring item_pid) const -> uint;
    [[nodiscard]] auto RealCountItems() const -> uint { return static_cast<uint>(_invItems.size()); }
    [[nodiscard]] auto CountItems() const -> uint;
    [[nodiscard]] auto GetInventory() -> vector<Item*>&;
    [[nodiscard]] auto IsHaveGeckItem() const -> bool;
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
    auto SetScript(string_view func, bool first_time) -> bool;
    void SendMessage(int num, int val, int to, MapManager& map_mngr);
    void AddCrTimeEvent(hstring func_num, uint rate, uint duration, int identifier) const;
    void EraseCrTimeEvent(int index);
    void ContinueTimeEvents(int offs_time);

    void Broadcast_Property(NetProperty type, const Property* prop, ServerEntity* entity);
    void Broadcast_Move(uint move_params);
    void Broadcast_Position();
    void Broadcast_Action(int action, int action_ext, Item* item);
    void Broadcast_Dir();
    void Broadcast_CustomCommand(ushort num_param, int val);

    void SendAndBroadcast_Action(int action, int action_ext, Item* item);
    void SendAndBroadcast_MoveItem(Item* item, uchar action, uchar prev_slot);
    void SendAndBroadcast_Animate(uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void SendAndBroadcast_SetAnims(CritterCondition cond, uint anim1, uint anim2);
    void SendAndBroadcast_Text(const vector<Critter*>& to_cr, string_view text, uchar how_say, bool unsafe_text);
    void SendAndBroadcast_Msg(const vector<Critter*>& to_cr, uint num_str, uchar how_say, ushort num_msg);
    void SendAndBroadcast_MsgLex(const vector<Critter*>& to_cr, uint num_str, uchar how_say, ushort num_msg, string_view lexems);

    void Send_Property(NetProperty type, const Property* prop, ServerEntity* entity);
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
    void Send_Text(Critter* from_cr, string_view text, uchar how_say);
    void Send_TextEx(uint from_id, string_view text, uchar how_say, bool unsafe_text);
    void Send_TextMsg(Critter* from_cr, uint str_num, uchar how_say, ushort num_msg);
    void Send_TextMsg(uint from_id, uint str_num, uchar how_say, ushort num_msg);
    void Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, string_view lexems);
    void Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, string_view lexems);
    void Send_Action(Critter* from_cr, int action, int action_ext, Item* item);
    void Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot);
    void Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void Send_SetAnims(Critter* from_cr, CritterCondition cond, uint anim1, uint anim2);
    void Send_CombatResult(uint* combat_res, uint len);
    void Send_AutomapsInfo(void* locs_vec, Location* loc);
    void Send_Effect(hstring eff_pid, ushort hx, ushort hy, ushort radius);
    void Send_FlyEffect(hstring eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy);
    void Send_PlaySound(uint crid_synchronize, string_view sound_name);
    void Send_MapText(ushort hx, ushort hy, uint color, string_view text, bool unsafe_text);
    void Send_MapTextMsg(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str);
    void Send_MapTextMsgLex(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, string_view lexems);
    void Send_ViewMap();
    void Send_SomeItem(Item* item);
    void Send_CustomMessage(uint msg);
    void Send_AddAllItems();
    void Send_AllAutomapsInfo(MapManager& map_mngr);
    void Send_SomeItems(const vector<Item*>* items, int param);

    ///@ ExportEvent
    ScriptEvent<> FinishEvent {};
    ///@ ExportEvent
    ScriptEvent<> IdleEvent {};
    ///@ ExportEvent
    ScriptEvent<Item* /*item*/, uchar /*toSlot*/> CheckMoveItemEvent {};
    ///@ ExportEvent
    ScriptEvent<Item* /*item*/, uchar /*fromSlot*/> ItemMovedEvent {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*showCritter*/> CritterShownEvent {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*showCritter*/> CritterShownDist1Event {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*showCritter*/> CritterShownDist2Event {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*showCritter*/> CritterShownDist3Event {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*hideCritter*/> CritterHiddenEvent {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*hideCritter*/> CritterHiddenDist1Event {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*hideCritter*/> CritterHiddenDist2Event {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*hideCritter*/> CritterHiddenDist3Event {};
    ///@ ExportEvent
    ScriptEvent<Item* /*item*/, bool /*added*/, Critter* /*fromCritter*/> ItemOnMapShownEvent {};
    ///@ ExportEvent
    ScriptEvent<Item* /*item*/, bool /*removed*/, Critter* /*toCritter*/> ItemOnMapHiddenEvent {};
    ///@ ExportEvent
    ScriptEvent<Item* /*item*/> ItemOnMapChangedEvent {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*receiver*/, int /*num*/, int /*value*/> MessageEvent {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*who*/, bool /*begin*/, uint /*talkers*/> TalkEvent {};
    ///@ ExportEvent
    ScriptEvent<Critter* /*trader*/, bool /*begin*/, uint /*barterCount*/> BarterEvent {};
    ///@ ExportEvent
    ScriptEvent<> GlobalMapIdleEvent {};
    ///@ ExportEvent
    ScriptEvent<> GlobalMapInEvent {};
    ///@ ExportEvent
    ScriptEvent<> GlobalMapOutEvent {};

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
    hstring ViewMapPid {};
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
    TalkData Talk {}; // Todo: incapsulate Critter::Talk

private:
    Player* _player {};
    bool _playerDetached {};
    uint _playerDetachTick {};
    vector<Item*> _invItems {};
    TalkData _talk {};
    uint _talkNextTick {};
};
