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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "MapLoader.h"
#include "ScriptSystem.h"
#include "ServerEntity.h"
#include "TwoDimensionalGrid.h"

class Item;
using StaticItem = Item;
class Critter;
class Map;
class Location;

struct StaticMap
{
    struct Field
    {
        bool MoveBlocked {};
        bool ShootBlocked {};
        vector<StaticItem*> StaticItems {};
        vector<StaticItem*> TriggerItems {};
    };

    unique_ptr<TwoDimensionalGrid<Field, mpos, msize>> HexField {};
    vector<pair<ident_t, const Critter*>> CritterBillets {};
    vector<pair<ident_t, const Item*>> ItemBillets {};
    vector<pair<ident_t, const Item*>> HexItemBillets {};
    vector<pair<ident_t, const Item*>> ChildItemBillets {};
    vector<StaticItem*> StaticItems {};
    unordered_map<ident_t, StaticItem*> StaticItemsById {};
};

class Map final : public ServerEntity, public EntityWithProto, public MapProperties
{
    friend class MapManager;

public:
    Map() = delete;
    Map(FOServer* engine, ident_t id, const ProtoMap* proto, Location* location, const StaticMap* static_map, const Properties* props = nullptr);
    Map(const Map&) = delete;
    Map(Map&&) noexcept = delete;
    auto operator=(const Map&) = delete;
    auto operator=(Map&&) noexcept = delete;
    ~Map() override;

    [[nodiscard]] auto GetStaticMap() const noexcept -> const StaticMap* { return _staticMap; }
    [[nodiscard]] auto GetProtoMap() const noexcept -> const ProtoMap* { return static_cast<const ProtoMap*>(_proto); }
    [[nodiscard]] auto GetLocation() noexcept -> Location* { return _mapLocation; }
    [[nodiscard]] auto GetLocation() const noexcept -> const Location* { return _mapLocation; }
    [[nodiscard]] auto IsHexMovable(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsHexShootable(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsHexesMovable(mpos hex, uint radius) const -> bool;
    [[nodiscard]] auto IsHexesMovable(mpos hex, uint radius, Critter* skip_cr) -> bool;
    [[nodiscard]] auto IsBlockItem(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsItemTrigger(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto IsItemGag(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto GetItem(ident_t item_id) noexcept -> Item*;
    [[nodiscard]] auto GetItemHex(mpos hex, hstring item_pid, Critter* picker) -> Item*;
    [[nodiscard]] auto GetItemGag(mpos hex) noexcept -> Item*;
    [[nodiscard]] auto GetItems() noexcept -> const vector<Item*>&;
    [[nodiscard]] auto GetItems(mpos hex) noexcept -> const vector<Item*>&;
    [[nodiscard]] auto GetItemsInRadius(mpos hex, uint radius, hstring pid) -> vector<Item*>;
    [[nodiscard]] auto GetItemsByProto(hstring pid) -> vector<Item*>;
    [[nodiscard]] auto GetItemsTrigger(mpos hex) -> vector<Item*>;
    [[nodiscard]] auto IsPlaceForProtoItem(mpos hex, const ProtoItem* proto_item) const -> bool;
    [[nodiscard]] auto FindStartHex(mpos hex, uint multihex, uint seek_radius, bool skip_unsafe) const -> optional<mpos>;
    [[nodiscard]] auto IsCritter(mpos hex, CritterFindType find_type) const -> bool;
    [[nodiscard]] auto IsCritter(mpos hex, const Critter* cr) const -> bool;
    [[nodiscard]] auto GetCritter(ident_t cr_id) noexcept -> Critter*;
    [[nodiscard]] auto GetCritter(mpos hex, CritterFindType find_type) noexcept -> Critter*;
    [[nodiscard]] auto GetCritters(mpos hex, uint radius, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetCritters(mpos hex, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetCritters() noexcept -> const vector<Critter*>&;
    [[nodiscard]] auto GetPlayerCritters() noexcept -> const vector<Critter*>&;
    [[nodiscard]] auto GetNonPlayerCritters() noexcept -> const vector<Critter*>&;
    [[nodiscard]] auto GetCrittersCount() const noexcept -> uint;
    [[nodiscard]] auto GetPlayerCrittersCount() const noexcept -> uint;
    [[nodiscard]] auto GetNonPlayerCrittersCount() const noexcept -> uint;
    [[nodiscard]] auto IsStaticItemTrigger(mpos hex) const noexcept -> bool;
    [[nodiscard]] auto GetStaticItem(ident_t id) noexcept -> StaticItem*;
    [[nodiscard]] auto GetStaticItem(mpos hex, hstring pid) noexcept -> StaticItem*;
    [[nodiscard]] auto GetStaticItemsHex(mpos hex) noexcept -> const vector<StaticItem*>&;
    [[nodiscard]] auto GetStaticItemsHexEx(mpos hex, uint radius, hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsByPid(hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsTrigger(mpos hex) noexcept -> const vector<StaticItem*>&;

    void SetLocation(Location* loc);
    void Process();
    void ProcessLoop(int index, uint time, uint tick);
    void SetText(mpos hex, ucolor color, string_view text, bool unsafe_text);
    void SetTextMsg(mpos hex, ucolor color, TextPackName text_pack, TextPackKey str_num);
    void SetTextMsgLex(mpos hex, ucolor color, TextPackName text_pack, TextPackKey str_num, string_view lexems);
    void AddCritter(Critter* cr);
    void RemoveCritter(Critter* cr);
    void AddItem(Item* item, mpos hex, Critter* dropper);
    void SetItem(Item* item);
    void RemoveItem(ident_t item_id);
    void SendProperty(NetProperty type, const Property* prop, ServerEntity* entity);
    void ChangeViewItem(Item* item);
    void AnimateItem(Item* item, hstring anim_name, bool looped, bool reversed);
    void SendEffect(hstring eff_pid, mpos hex, uint16 radius);
    void SendFlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, mpos from_hex, mpos to_hex);
    void SetHexManualBlock(mpos hex, bool enable, bool full);
    void AddCritterToField(Critter* cr);
    void RemoveCritterFromField(Critter* cr);
    void RecacheHexFlags(mpos hex);

    ///@ ExportEvent
    ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    ENTITY_EVENT(OnLoop);
    ///@ ExportEvent
    ENTITY_EVENT(OnLoopEx, int /*loopIndex*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCheckLook, Critter* /*cr*/, Critter* /*target*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCheckTrapLook, Critter* /*cr*/, Item* /*item*/);

private:
    struct Field
    {
        bool HasCritter {};
        bool HasBlockCritter {};
        bool HasGagItem {};
        bool HasTriggerItem {};
        bool HasNoMoveItem {};
        bool HasNoShootItem {};
        bool MoveBlocked {};
        bool ShootBlocked {};
        vector<Critter*> Critters {};
        vector<Critter*> MultihexCritters {};
        vector<Item*> Items {};
        vector<Item*> BlockLines {};
        bool ManualBlock {};
        bool ManualBlockFull {};
    };

    void SetMultihexCritter(Critter* cr, bool set);
    void RecacheHexFlags(Field& field);

    const StaticMap* _staticMap {};
    msize _mapSize {};
    unique_ptr<TwoDimensionalGrid<Field, mpos, msize>> _hexField {};
    vector<Critter*> _critters {};
    unordered_map<ident_t, Critter*> _crittersMap {};
    vector<Critter*> _playerCritters {};
    vector<Critter*> _nonPlayerCritters {};
    vector<Item*> _items {};
    unordered_map<ident_t, Item*> _itemsMap {};
    Location* _mapLocation {};
    uint _loopLastTick[5] {};
};
