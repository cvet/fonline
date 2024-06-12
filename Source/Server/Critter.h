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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

///@ ExportEnum
enum class MovingState : uint8
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
    TraceFailed = 11,
    NotAlive = 12,
    Attached = 13,
};

class Critter final : public ServerEntity, public EntityWithProto, public CritterProperties
{
    friend class Player;
    friend class CritterManager;

public:
    Critter() = delete;
    Critter(FOServer* engine, ident_t id, const ProtoCritter* proto, const Properties* props = nullptr);
    Critter(const Critter&) = delete;
    Critter(Critter&&) noexcept = delete;
    auto operator=(const Critter&) = delete;
    auto operator=(Critter&&) noexcept = delete;
    ~Critter() override;

    [[nodiscard]] auto GetPlayer() const -> const Player* { return _player; }
    [[nodiscard]] auto GetPlayer() -> Player* { return _player; }
    [[nodiscard]] auto GetOfflineTime() const -> time_duration;
    [[nodiscard]] auto IsAlive() const -> bool;
    [[nodiscard]] auto IsDead() const -> bool;
    [[nodiscard]] auto IsKnockout() const -> bool;
    [[nodiscard]] auto CheckFind(CritterFindType find_type) const -> bool;
    [[nodiscard]] auto GetInvItem(ident_t item_id, bool skip_hidden) -> Item*;
    [[nodiscard]] auto GetRawInvItems() -> vector<Item*>& { return _invItems; }
    [[nodiscard]] auto GetConstRawInvItems() const -> const vector<Item*>& { return _invItems; }
    [[nodiscard]] auto GetInvItemByPid(hstring item_pid) -> Item*;
    [[nodiscard]] auto GetInvItemByPidSlot(hstring item_pid, CritterItemSlot slot) -> Item*;
    [[nodiscard]] auto GetInvItemSlot(CritterItemSlot slot) -> Item*;
    [[nodiscard]] auto GetInvItemsSlot(CritterItemSlot slot) -> vector<Item*>;
    [[nodiscard]] auto CountInvItemPid(hstring item_pid) const -> uint;
    [[nodiscard]] auto RealCountInvItems() const -> uint { return static_cast<uint>(_invItems.size()); }
    [[nodiscard]] auto CountInvItems() const -> uint;
    [[nodiscard]] auto IsHaveGeckItem() const -> bool;
    [[nodiscard]] auto GetCrSelf(ident_t cr_id) -> Critter*;
    [[nodiscard]] auto GetCrFromVisCr(CritterFindType find_type, bool vis_cr_self) -> vector<Critter*>;
    [[nodiscard]] auto GetGlobalMapCritter(ident_t cr_id) const -> Critter*;
    [[nodiscard]] auto IsTalking() const -> bool;
    [[nodiscard]] auto GetTalkingCritters() const -> uint;
    [[nodiscard]] auto GetBarterCritters() const -> uint;
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

    void MarkIsForPlayer();
    void AttachPlayer(Player* player);
    void DetachPlayer();
    void ClearMove();
    void AttachToCritter(Critter* cr);
    void DetachFromCritter();
    void MoveAttachedCritters();
    void ClearVisible();
    void SetItem(Item* item);
    void ChangeDir(uint8 dir);
    void ChangeDirAngle(int dir_angle);

    void Broadcast_Property(NetProperty type, const Property* prop, const ServerEntity* entity);
    void Broadcast_Action(CritterAction action, int action_data, const Item* item);
    void Broadcast_Dir();
    void Broadcast_Teleport(uint16 to_hx, uint16 to_hy);

    void SendAndBroadcast(const Player* ignore_player, const std::function<void(Critter*)>& callback);
    void SendAndBroadcast_Moving();
    void SendAndBroadcast_Action(CritterAction action, int action_data, const Item* context_item);
    void SendAndBroadcast_MoveItem(const Item* item, CritterAction action, CritterItemSlot prev_slot);
    void SendAndBroadcast_Animate(CritterStateAnim state_anim, CritterActionAnim action_anim, const Item* context_item, bool clear_sequence, bool delay_play);
    void SendAndBroadcast_SetAnims(CritterCondition cond, CritterStateAnim state_anim, CritterActionAnim action_anim);
    void SendAndBroadcast_Text(const vector<Critter*>& to_cr, string_view text, uint8 how_say, bool unsafe_text);
    void SendAndBroadcast_Msg(const vector<Critter*>& to_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num);
    void SendAndBroadcast_MsgLex(const vector<Critter*>& to_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems);
    void SendAndBroadcast_Attachments();

    void Send_Property(NetProperty type, const Property* prop, const ServerEntity* entity);
    void Send_Moving(const Critter* from_cr);
    void Send_Dir(const Critter* from_cr);
    void Send_AddCritter(const Critter* cr);
    void Send_RemoveCritter(const Critter* cr);
    void Send_LoadMap(const Map* map);
    void Send_AddItemOnMap(const Item* item);
    void Send_EraseItemFromMap(const Item* item);
    void Send_AnimateItem(const Item* item, hstring anim_name, bool looped, bool reversed);
    void Send_AddItem(const Item* item);
    void Send_EraseItem(const Item* item);
    void Send_GlobalInfo();
    void Send_GlobalLocation(const Location* loc, bool add);
    void Send_GlobalMapFog(uint16 zx, uint16 zy, uint8 fog);
    void Send_Teleport(const Critter* cr, uint16 to_hx, uint16 to_hy);
    void Send_AllProperties();
    void Send_Talk();
    void Send_TimeSync();
    void Send_Text(const Critter* from_cr, string_view text, uint8 how_say);
    void Send_TextEx(ident_t from_id, string_view text, uint8 how_say, bool unsafe_text);
    void Send_TextMsg(const Critter* from_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num);
    void Send_TextMsg(ident_t from_id, uint8 how_say, TextPackName text_pack, TextPackKey str_num);
    void Send_TextMsgLex(const Critter* from_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems);
    void Send_TextMsgLex(ident_t from_id, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems);
    void Send_Action(const Critter* from_cr, CritterAction action, int action_data, const Item* context_item);
    void Send_MoveItem(const Critter* from_cr, const Item* item, CritterAction action, CritterItemSlot prev_slot);
    void Send_Animate(const Critter* from_cr, CritterStateAnim state_anim, CritterActionAnim action_anim, const Item* context_item, bool clear_sequence, bool delay_play);
    void Send_SetAnims(const Critter* from_cr, CritterCondition cond, CritterStateAnim state_anim, CritterActionAnim action_anim);
    void Send_AutomapsInfo(const void* locs_vec, const Location* loc);
    void Send_Effect(hstring eff_pid, uint16 hx, uint16 hy, uint16 radius);
    void Send_FlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy);
    void Send_PlaySound(ident_t cr_id_synchronize, string_view sound_name);
    void Send_MapText(uint16 hx, uint16 hy, ucolor color, string_view text, bool unsafe_text);
    void Send_MapTextMsg(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num);
    void Send_MapTextMsgLex(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num, string_view lexems);
    void Send_ViewMap();
    void Send_PlaceToGameComplete();
    void Send_AddAllItems();
    void Send_AllAutomapsInfo();
    void Send_SomeItems(const vector<Item*>* items, int param);
    void Send_Attachments(const Critter* from_cr);

    void AddTimeEvent(hstring func_name, uint rate, tick_t duration, const any_t& identifier);
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
    ENTITY_EVENT(OnItemOnMapAppeared, Item* /*item*/, Critter* /*dropper*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemOnMapDisappeared, Item* /*item*/, Critter* /*picker*/);
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

    time_point LastIdleCall {};
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

    struct TargetMovingData
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

    struct MovingData
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

    vector<Critter*> AttachedCritters {};

private:
    Player* _player {};
    time_point _playerDetachTime {};
    vector<Item*> _invItems {};
    time_point _talkNextTime {};
};
