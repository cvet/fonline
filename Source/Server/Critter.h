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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "EntityProperties.h"
#include "EntityProtos.h"
#include "Geometry.h"
#include "ServerEntity.h"

FO_BEGIN_NAMESPACE();

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
public:
    Critter() = delete;
    Critter(FOServer* engine, ident_t id, const ProtoCritter* proto, const Properties* props = nullptr) noexcept;
    Critter(const Critter&) = delete;
    Critter(Critter&&) noexcept = delete;
    auto operator=(const Critter&) = delete;
    auto operator=(Critter&&) noexcept = delete;
    ~Critter() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _proto->GetName(); }
    [[nodiscard]] auto GetPlayer() const noexcept -> const Player* { return _player.get(); }
    [[nodiscard]] auto GetPlayer() noexcept -> Player* { return _player.get(); }
    [[nodiscard]] auto GetOfflineTime() const -> timespan;
    [[nodiscard]] auto IsAlive() const noexcept -> bool;
    [[nodiscard]] auto IsDead() const noexcept -> bool;
    [[nodiscard]] auto IsKnockout() const noexcept -> bool;
    [[nodiscard]] auto CheckFind(CritterFindType find_type) const noexcept -> bool;
    [[nodiscard]] auto GetInvItem(ident_t item_id) noexcept -> Item*;
    [[nodiscard]] auto GetInvItems() noexcept -> span<raw_ptr<Item>> { return _invItems; }
    [[nodiscard]] auto GetInvItems() const noexcept -> span<const raw_ptr<Item>> { return _invItems; }
    [[nodiscard]] auto GetInvItemByPid(hstring item_pid) noexcept -> Item*;
    [[nodiscard]] auto GetInvItemBySlot(CritterItemSlot slot) noexcept -> Item*;
    [[nodiscard]] auto CountInvItemByPid(hstring item_pid) const noexcept -> int32;
    [[nodiscard]] auto HasItems() const noexcept -> bool { return !_invItems.empty(); }
    [[nodiscard]] auto GetVisibleItems() const noexcept -> const unordered_set<ident_t>& { return _visibleItems; }
    [[nodiscard]] auto IsSeeItem(ident_t item_id) const noexcept -> bool { return _visibleItems.contains(item_id); }
    [[nodiscard]] auto IsSeeCritter(ident_t cr_id) const -> bool;
    [[nodiscard]] auto GetCritter(ident_t cr_id, CritterSeeType see_type) -> Critter*;
    [[nodiscard]] auto GetCritters(CritterSeeType see_type, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetGlobalMapGroup() -> span<raw_ptr<Critter>>;
    [[nodiscard]] auto GetRawGlobalMapGroup() -> auto& { return _globalMapGroup; }
    [[nodiscard]] auto IsMoving() const noexcept -> bool { return !Moving.Steps.empty(); }

    auto AddVisibleCritter(Critter* cr) -> bool;
    auto RemoveVisibleCritter(Critter* cr) -> bool;
    auto AddCrIntoVisGroup1(ident_t cr_id) noexcept -> bool;
    auto AddCrIntoVisGroup2(ident_t cr_id) noexcept -> bool;
    auto AddCrIntoVisGroup3(ident_t cr_id) noexcept -> bool;
    auto RemoveCrFromVisGroup1(ident_t cr_id) noexcept -> bool;
    auto RemoveCrFromVisGroup2(ident_t cr_id) noexcept -> bool;
    auto RemoveCrFromVisGroup3(ident_t cr_id) noexcept -> bool;
    auto AddVisibleItem(ident_t item_id) noexcept -> bool;
    auto RemoveVisibleItem(ident_t item_id) noexcept -> bool;
    auto CheckVisibleItem(ident_t item_id) const noexcept -> bool;

    void MarkIsForPlayer();
    void AttachPlayer(Player* player);
    void DetachPlayer();
    void ClearMove();
    void AttachToCritter(Critter* cr);
    void DetachFromCritter();
    void MoveAttachedCritters();
    void ClearVisibleEnitites();
    void SetItem(Item* item);
    void RemoveItem(Item* item);
    void ChangeDir(uint8 dir);
    void ChangeDirAngle(int32 dir_angle);

    void Broadcast_Property(NetProperty type, const Property* prop, const ServerEntity* entity);
    void Broadcast_Action(CritterAction action, int32 action_data, const Item* item);
    void Broadcast_Dir();
    void Broadcast_Teleport(mpos to_hex);

    void SendAndBroadcast(const Player* ignore_player, const function<void(Critter*)>& callback);
    void SendAndBroadcast_Moving();
    void SendAndBroadcast_Action(CritterAction action, int32 action_data, const Item* context_item);
    void SendAndBroadcast_MoveItem(const Item* item, CritterAction action, CritterItemSlot prev_slot);
    void SendAndBroadcast_Attachments();

    void Send_Property(NetProperty type, const Property* prop, const ServerEntity* entity);
    void Send_Moving(const Critter* from_cr);
    void Send_MovingSpeed(const Critter* from_cr);
    void Send_Dir(const Critter* from_cr);
    void Send_AddCritter(const Critter* cr);
    void Send_RemoveCritter(const Critter* cr);
    void Send_LoadMap(const Map* map);
    void Send_AddItemOnMap(const Item* item);
    void Send_RemoveItemFromMap(const Item* item);
    void Send_ChosenAddItem(const Item* item);
    void Send_ChosenRemoveItem(const Item* item);
    void Send_Teleport(const Critter* cr, mpos to_hex);
    void Send_TimeSync();
    void Send_InfoMessage(EngineInfoMessage info_message, string_view extra_text = "");
    void Send_Action(const Critter* from_cr, CritterAction action, int32 action_data, const Item* context_item);
    void Send_MoveItem(const Critter* from_cr, const Item* item, CritterAction action, CritterItemSlot prev_slot);
    void Send_ViewMap();
    void Send_PlaceToGameComplete();
    void Send_SomeItems(const vector<Item*>& items, bool owned, bool with_inner_entities, const any_t& context_param);
    void Send_Attachments(const Critter* from_cr);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAppeared, Critter* /*appearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAppearedDist1, Critter* /*appearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAppearedDist2, Critter* /*appearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterAppearedDist3, Critter* /*appearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterDisappeared, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterDisappearedDist1, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterDisappearedDist2, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterDisappearedDist3, Critter* /*disappearedCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemOnMapAppeared, Item* /*item*/, Critter* /*dropper*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemOnMapDisappeared, Item* /*item*/, Critter* /*picker*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemOnMapChanged, Item* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnTalk, Critter* /*playerCr*/, bool /*begin*/, int32 /*talkers*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnBarter, Critter* /*playerCr*/, bool /*begin*/, int32 /*barterCount*/);

    // Todo: incapsulate Critter data
    int32 LockMapTransfers {};

    ident_t ViewMapId {};
    hstring ViewMapPid {};
    uint16 ViewMapLook {};
    mpos ViewMapHex {};
    uint8 ViewMapDir {};
    ident_t ViewMapLocId {};
    int32 ViewMapLocEnt {};

    struct TargetMovingData
    {
        MovingState State {MovingState::Success};
        ident_t TargId {};
        mpos TargHex {};
        int32 Cut {};
        uint16 Speed {};
        int32 TraceDist {};
        ident_t GagEntityId {};
    } TargetMoving {};

    struct MovingData
    {
        uint32 Uid {};
        uint16 Speed {};
        vector<uint8> Steps {};
        vector<uint16> ControlSteps {};
        nanotime StartTime {};
        timespan OffsetTime {};
        mpos StartHex {};
        mpos EndHex {};
        float32 WholeTime {};
        float32 WholeDist {};
        ipos16 StartHexOffset {};
        ipos16 EndHexOffset {};
    } Moving {};

    vector<raw_ptr<Critter>> AttachedCritters {};

private:
    refcount_ptr<Player> _player {};
    nanotime _playerDetachTime {};
    vector<raw_ptr<Item>> _invItems {};
    vector<raw_ptr<Critter>> _visibleCrWhoSeeMe {};
    vector<raw_ptr<Critter>> _visibleCr {};
    unordered_map<ident_t, raw_ptr<Critter>> _visibleCrWhoSeeMeMap {};
    unordered_map<ident_t, raw_ptr<Critter>> _visibleCrMap {};
    unordered_set<ident_t> _visibleCrGroup1 {};
    unordered_set<ident_t> _visibleCrGroup2 {};
    unordered_set<ident_t> _visibleCrGroup3 {};
    unordered_set<ident_t> _visibleItems {};
    shared_ptr<vector<raw_ptr<Critter>>> _globalMapGroup {};
};

FO_END_NAMESPACE();
