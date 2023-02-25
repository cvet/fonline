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

#include "Dialogs.h"
#include "EntityProperties.h"
#include "EntityProtos.h"
#include "ServerEntity.h"

class Player;
class MapManager;
class Item;
class Map;
class Location;

class Critter final : public ServerEntity, public EntityWithProto, public CritterProperties
{
    friend class Player;
    friend class CritterManager;

public:
    Critter() = delete;
    Critter(FOServer* engine, ident_t id, Player* owner, const ProtoCritter* proto);
    Critter(const Critter&) = delete;
    Critter(Critter&&) noexcept = delete;
    auto operator=(const Critter&) = delete;
    auto operator=(Critter&&) noexcept = delete;
    ~Critter() override;

    [[nodiscard]] auto IsOwnedByPlayer() const -> bool { return _player != nullptr || _playerDetached; }
    [[nodiscard]] auto IsNpc() const -> bool { return !IsOwnedByPlayer(); }
    [[nodiscard]] auto GetOwner() const -> const Player* { return _player; }
    [[nodiscard]] auto GetOwner() -> Player* { return _player; }
    [[nodiscard]] auto GetOfflineTime() const -> time_duration;
    [[nodiscard]] auto IsAlive() const -> bool;
    [[nodiscard]] auto IsDead() const -> bool;
    [[nodiscard]] auto IsKnockout() const -> bool;
    [[nodiscard]] auto CheckFind(CritterFindType find_type) const -> bool;
    [[nodiscard]] auto GetItem(ident_t item_id, bool skip_hide) -> Item*;
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
    [[nodiscard]] auto GetCrSelf(ident_t cr_id) -> Critter*;
    [[nodiscard]] auto GetCrFromVisCr(CritterFindType find_type, bool vis_cr_self) -> vector<Critter*>;
    [[nodiscard]] auto GetGlobalMapCritter(ident_t cr_id) const -> Critter*;
    [[nodiscard]] auto IsTalking() const -> bool;
    [[nodiscard]] auto GetTalkedPlayers() const -> uint;
    [[nodiscard]] auto IsTalkedPlayers() const -> bool;
    [[nodiscard]] auto GetBarterPlayers() const -> uint;
    [[nodiscard]] auto IsFreeToTalk() const -> bool;
    [[nodiscard]] auto IsMoving() const -> bool { return !Moving.Steps.empty(); }

    auto AddCrIntoVisVec(Critter* add_cr) -> bool;
    auto DelCrFromVisVec(Critter* del_cr) -> bool;
    auto AddCrIntoVisSet1(ident_t cr_id) -> bool;
    auto AddCrIntoVisSet2(ident_t cr_id) -> bool;
    auto AddCrIntoVisSet3(ident_t cr_id) -> bool;
    auto DelCrFromVisSet1(ident_t cr_id) -> bool;
    auto DelCrFromVisSet2(ident_t cr_id) -> bool;
    auto DelCrFromVisSet3(ident_t cr_id) -> bool;
    auto AddIdVisItem(ident_t item_id) -> bool;
    auto DelIdVisItem(ident_t item_id) -> bool;
    auto CountIdVisItem(ident_t item_id) const -> bool;

    void DetachPlayer();
    void AttachPlayer(Player* owner);
    void ClearMove();
    void ClearVisible();
    void SetItem(Item* item);
    void ChangeDir(uint8 dir);
    void ChangeDirAngle(int dir_angle);

    void Broadcast_Property(NetProperty type, const Property* prop, ServerEntity* entity);
    void Broadcast_Move();
    void Broadcast_Position();
    void Broadcast_Action(int action, int action_ext, Item* item);
    void Broadcast_Dir();
    void Broadcast_Teleport(uint16 to_hx, uint16 to_hy);

    void SendAndBroadcast_Action(int action, int action_ext, Item* item);
    void SendAndBroadcast_MoveItem(Item* item, uint8 action, uint8 prev_slot);
    void SendAndBroadcast_Animate(uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void SendAndBroadcast_SetAnims(CritterCondition cond, uint anim1, uint anim2);
    void SendAndBroadcast_Text(const vector<Critter*>& to_cr, string_view text, uint8 how_say, bool unsafe_text);
    void SendAndBroadcast_Msg(const vector<Critter*>& to_cr, uint num_str, uint8 how_say, uint16 num_msg);
    void SendAndBroadcast_MsgLex(const vector<Critter*>& to_cr, uint num_str, uint8 how_say, uint16 num_msg, string_view lexems);

    void Send_Property(NetProperty type, const Property* prop, ServerEntity* entity);
    void Send_Move(Critter* from_cr);
    void Send_Dir(Critter* from_cr);
    void Send_AddCritter(Critter* cr);
    void Send_RemoveCritter(Critter* cr);
    void Send_LoadMap(Map* map);
    void Send_Position(Critter* cr);
    void Send_AddItemOnMap(Item* item);
    void Send_EraseItemFromMap(Item* item);
    void Send_AnimateItem(Item* item, uint8 from_frm, uint8 to_frm);
    void Send_AddItem(Item* item);
    void Send_EraseItem(Item* item);
    void Send_GlobalInfo(uint8 flags);
    void Send_GlobalLocation(Location* loc, bool add);
    void Send_GlobalMapFog(uint16 zx, uint16 zy, uint8 fog);
    void Send_Teleport(Critter* cr, uint16 to_hx, uint16 to_hy);
    void Send_AllProperties();
    void Send_Talk();
    void Send_TimeSync();
    void Send_Text(Critter* from_cr, string_view text, uint8 how_say);
    void Send_TextEx(ident_t from_id, string_view text, uint8 how_say, bool unsafe_text);
    void Send_TextMsg(Critter* from_cr, uint str_num, uint8 how_say, uint16 num_msg);
    void Send_TextMsg(ident_t from_id, uint str_num, uint8 how_say, uint16 num_msg);
    void Send_TextMsgLex(Critter* from_cr, uint num_str, uint8 how_say, uint16 num_msg, string_view lexems);
    void Send_TextMsgLex(ident_t from_id, uint num_str, uint8 how_say, uint16 num_msg, string_view lexems);
    void Send_Action(Critter* from_cr, int action, int action_ext, Item* item);
    void Send_MoveItem(Critter* from_cr, Item* item, uint8 action, uint8 prev_slot);
    void Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void Send_SetAnims(Critter* from_cr, CritterCondition cond, uint anim1, uint anim2);
    void Send_AutomapsInfo(void* locs_vec, Location* loc);
    void Send_Effect(hstring eff_pid, uint16 hx, uint16 hy, uint16 radius);
    void Send_FlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy);
    void Send_PlaySound(ident_t cr_id_synchronize, string_view sound_name);
    void Send_MapText(uint16 hx, uint16 hy, uint color, string_view text, bool unsafe_text);
    void Send_MapTextMsg(uint16 hx, uint16 hy, uint color, uint16 num_msg, uint num_str);
    void Send_MapTextMsgLex(uint16 hx, uint16 hy, uint color, uint16 num_msg, uint num_str, string_view lexems);
    void Send_ViewMap();
    void Send_PlaceToGameComplete();
    void Send_AddAllItems();
    void Send_AllAutomapsInfo();
    void Send_SomeItems(const vector<Item*>* items, int param);

    void AddTimeEvent(hstring func_name, uint rate, tick_t duration, int identifier);
    void EraseTimeEvent(size_t index);
    void ProcessTimeEvents();

    ///@ ExportEvent
    ENTITY_EVENT(OnIdle);
    ///@ ExportEvent
    ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterAppeared, Critter* /*appearedCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterAppearedDist1, Critter* /*appearedCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterAppearedDist2, Critter* /*appearedCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterAppearedDist3, Critter* /*appearedCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterDisappeared, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterDisappearedDist1, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterDisappearedDist2, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterDisappearedDist3, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemOnMapAppeared, Item* /*item*/, bool /*added*/, Critter* /*fromCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemOnMapDisappeared, Item* /*item*/, bool /*removed*/, Critter* /*toCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemOnMapChanged, Item* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnTalk, Critter* /*playerCr*/, bool /*begin*/, uint /*talkers*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnBarter, Critter* /*playerCr*/, bool /*begin*/, uint /*barterCount*/);

    int LockMapTransfers {};
    uint AllowedToDownloadMap {};
    vector<Critter*> VisCr {};
    vector<Critter*> VisCrSelf {};
    unordered_map<ident_t, Critter*> VisCrMap {};
    unordered_map<ident_t, Critter*> VisCrSelfMap {};
    unordered_set<ident_t> VisCr1 {};
    unordered_set<ident_t> VisCr2 {};
    unordered_set<ident_t> VisCr3 {};
    unordered_set<ident_t> VisItem {};

    time_point CacheValuesNextTime {};
    uint LookCacheValue {};
    vector<Critter*>* GlobalMapGroup {};
    uint RadioMessageSended {};
    TalkData Talk {}; // Todo: incapsulate Critter::Talk

    ident_t ViewMapId {};
    hstring ViewMapPid {};
    uint16 ViewMapLook {};
    uint16 ViewMapHx {};
    uint16 ViewMapHy {};
    uint8 ViewMapDir {};
    ident_t ViewMapLocId {};
    uint ViewMapLocEnt {};

    struct
    {
        MovingState State {MovingState::Success};
        ident_t TargId {};
        uint16 HexX {};
        uint16 HexY {};
        uint Cut {};
        uint16 Speed {};
        uint TraceDist {};
        ident_t GagEntityId {};
    } TargetMoving {};

    struct
    {
        uint16 Speed {};
        uint Uid {};
        vector<uint8> Steps {};
        vector<uint16> ControlSteps {};
        time_point StartTime {};
        uint16 StartHexX {};
        uint16 StartHexY {};
        uint16 EndHexX {};
        uint16 EndHexY {};
        float WholeTime {};
        float WholeDist {};
        int16 StartOx {};
        int16 StartOy {};
        int16 EndOx {};
        int16 EndOy {};
    } Moving {};

private:
    Player* _player {};
    bool _playerDetached {};
    time_point _playerDetachTime {};
    vector<Item*> _invItems {};
    time_point _talkNextTime {};
};
