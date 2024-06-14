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
    // Todo: memory optimization - improve dynamic grid for map hex fields
    struct Field
    {
        bool IsMoveBlocked {};
        bool IsShootBlocked {};
        vector<StaticItem*> StaticItems {};
        vector<StaticItem*> TriggerItems {};
    };

    unique_ptr<BaseTwoDimensionalGrid<Field, uint16>> HexField {};
    vector<pair<uint, const Critter*>> CritterBillets {};
    vector<pair<uint, const Item*>> ItemBillets {};
    vector<pair<uint, const Item*>> HexItemBillets {};
    vector<pair<uint, const Item*>> ChildItemBillets {};
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

    [[nodiscard]] auto GetStaticMap() const -> const StaticMap* { return _staticMap; }
    [[nodiscard]] auto GetProtoMap() const -> const ProtoMap*;
    [[nodiscard]] auto GetLocation() -> Location*;
    [[nodiscard]] auto GetLocation() const -> const Location*;
    [[nodiscard]] auto IsHexMovable(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsHexShootable(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsHexesMovable(uint16 hx, uint16 hy, uint radius) const -> bool;
    [[nodiscard]] auto IsHexesMovable(uint16 to_hx, uint16 to_hy, uint radius, Critter* skip_cr) -> bool;
    [[nodiscard]] auto IsBlockItem(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsItemTrigger(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsItemGag(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto GetItem(ident_t item_id) -> Item*;
    [[nodiscard]] auto GetItemHex(uint16 hx, uint16 hy, hstring item_pid, Critter* picker) -> Item*;
    [[nodiscard]] auto GetItemGag(uint16 hx, uint16 hy) -> Item*;
    [[nodiscard]] auto GetItems() -> const vector<Item*>&;
    [[nodiscard]] auto GetItems(uint16 hx, uint16 hy) -> const vector<Item*>&;
    [[nodiscard]] auto GetItemsInRadius(uint16 hx, uint16 hy, uint radius, hstring pid) -> vector<Item*>;
    [[nodiscard]] auto GetItemsByProto(hstring pid) -> vector<Item*>;
    [[nodiscard]] auto GetItemsTrigger(uint16 hx, uint16 hy) -> vector<Item*>;
    [[nodiscard]] auto IsPlaceForProtoItem(uint16 hx, uint16 hy, const ProtoItem* proto_item) const -> bool;
    [[nodiscard]] auto FindStartHex(uint16 hx, uint16 hy, uint multihex, uint seek_radius, bool skip_unsafe) const -> optional<tuple<uint16, uint16>>;
    [[nodiscard]] auto IsAnyCritter(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsNonDeadCritter(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsDeadCritter(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto IsCritter(uint16 hx, uint16 hy, const Critter* cr) const -> bool;
    [[nodiscard]] auto GetCritter(ident_t cr_id) -> Critter*;
    [[nodiscard]] auto GetNonDeadCritter(uint16 hx, uint16 hy) -> Critter*;
    [[nodiscard]] auto GetDeadCritter(uint16 hx, uint16 hy) -> Critter*;
    [[nodiscard]] auto GetCritters(uint16 hx, uint16 hy, uint radius, CritterFindType find_type) -> vector<Critter*>;
    [[nodiscard]] auto GetCritters() -> const vector<Critter*>&;
    [[nodiscard]] auto GetPlayerCritters() -> const vector<Critter*>&;
    [[nodiscard]] auto GetNonPlayerCritters() -> const vector<Critter*>&;
    [[nodiscard]] auto GetCrittersCount() const -> uint;
    [[nodiscard]] auto GetPlayerCrittersCount() const -> uint;
    [[nodiscard]] auto GetNonPlayerCrittersCount() const -> uint;
    [[nodiscard]] auto IsStaticItemTrigger(uint16 hx, uint16 hy) const -> bool;
    [[nodiscard]] auto GetStaticItem(ident_t id) -> StaticItem*;
    [[nodiscard]] auto GetStaticItem(uint16 hx, uint16 hy, hstring pid) -> StaticItem*;
    [[nodiscard]] auto GetStaticItemsHex(uint16 hx, uint16 hy) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsHexEx(uint16 hx, uint16 hy, uint radius, hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsByPid(hstring pid) -> vector<StaticItem*>;
    [[nodiscard]] auto GetStaticItemsTrigger(uint16 hx, uint16 hy) -> vector<StaticItem*>;

    void SetLocation(Location* loc);
    void Process();
    void ProcessLoop(int index, uint time, uint tick);
    void SetText(uint16 hx, uint16 hy, ucolor color, string_view text, bool unsafe_text);
    void SetTextMsg(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num);
    void SetTextMsgLex(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num, string_view lexems);
    void AddCritter(Critter* cr);
    void EraseCritter(Critter* cr);
    auto AddItem(Item* item, uint16 hx, uint16 hy, Critter* dropper) -> bool;
    void SetItem(Item* item, uint16 hx, uint16 hy);
    void EraseItem(ident_t item_id);
    void SendProperty(NetProperty type, const Property* prop, ServerEntity* entity);
    void ChangeViewItem(Item* item);
    void AnimateItem(Item* item, hstring anim_name, bool looped, bool reversed);
    void SendEffect(hstring eff_pid, uint16 hx, uint16 hy, uint16 radius);
    void SendFlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy);
    void SetHexManualBlock(uint16 hx, uint16 hy, bool enable, bool full);
    void AddCritterToField(Critter* cr);
    void RemoveCritterFromField(Critter* cr);
    void RecacheHexFlags(uint16 hx, uint16 hy);

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
        bool IsNonDeadCritter {};
        bool IsDeadCritter {};
        bool IsGagItem {};
        bool IsTriggerItem {};
        bool IsNoMoveItem {};
        bool IsNoShootItem {};
        bool IsMoveBlocked {};
        bool IsShootBlocked {};
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
    uint16 _width {};
    uint16 _height {};
    unique_ptr<BaseTwoDimensionalGrid<Field, uint16>> _hexField {};
    vector<Critter*> _critters {};
    unordered_map<ident_t, Critter*> _crittersMap {};
    vector<Critter*> _playerCritters {};
    vector<Critter*> _nonPlayerCritters {};
    vector<Item*> _items {};
    unordered_map<ident_t, Item*> _itemsMap {};
    Location* _mapLocation {};
    uint _loopLastTick[5] {};
};
