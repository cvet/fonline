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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "Common.h"

#include "Geometry.h"
#include "ScriptSystem.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

// Run the init callback and store its name as the init script
///@ ExportMethod
FO_SCRIPT_API void Server_Map_SetupScript(ptr<Map> self, ScriptFunc<void, ptr<Map>, bool> initFunc)
{
    if (initFunc.IsDelegate()) {
        throw ScriptException("Init function must not be a delegate");
    }
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc.GetName().first, true)) {
        throw ScriptException("Call init failed", initFunc.GetName().first);
    }

    self->SetInitScript(initFunc.GetName().first);
}

// Run the named init script and store its name
///@ ExportMethod
FO_SCRIPT_API void Server_Map_SetupScriptEx(ptr<Map> self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

// Return the parent location of this map
///@ ExportMethod
FO_SCRIPT_API FO_RETURNS_PARENT ptr<Location> Server_Map_GetLocation(ptr<Map> self)
{
    auto loc {self->GetLocation()};
    return loc;
}

// Create a map item from a prototype id on a valid hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Item> Server_Map_AddItem(ptr<Map> self, mpos hex, hstring protoId)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item {self->GetEngine()->ItemMngr.CreateItemOnHex(self, hex, protoId, nullptr)};
    return item;
}

// Create a map item from a prototype descriptor on a valid hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Item> Server_Map_AddItem(ptr<Map> self, mpos hex, ptr<ProtoItem> proto)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item {self->GetEngine()->ItemMngr.CreateItemOnHex(self, hex, proto->GetProtoId(), nullptr)};
    return item;
}

// Create a map item with integer property overrides on a valid hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Item> Server_Map_AddItem(ptr<Map> self, mpos hex, hstring protoId, readonly_map<ItemProperty, int32_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto proto {self->GetEngine()->GetProtoItem(protoId)};

    if (!proto) {
        throw ScriptException("Invalid item proto id arg", protoId);
    }

    Properties props_ {proto->GetProperties()->Copy()};

    for (const auto& [key, value] : props) {
        props_.SetValueAsIntProps(static_cast<int32_t>(key), value);
    }

    auto item {self->GetEngine()->ItemMngr.CreateItemOnHex(self, hex, protoId, &props_)};
    return item;
}

// Create a map item from a prototype descriptor with integer property overrides on a valid hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Item> Server_Map_AddItem(ptr<Map> self, mpos hex, ptr<ProtoItem> proto, readonly_map<ItemProperty, int32_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    Properties props_ {proto->GetProperties()->Copy()};

    for (const auto& [key, value] : props) {
        props_.SetValueAsIntProps(static_cast<int32_t>(key), value);
    }

    auto item {self->GetEngine()->ItemMngr.CreateItemOnHex(self, hex, proto->GetProtoId(), &props_)};
    return item;
}

// Return a map item by id, or null when it is absent or the id is zero
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Item> Server_Map_GetItem(ptr<Map> self, ident_t itemId)
{
    if (!itemId) {
        return nullptr;
    }

    auto item {self->GetItem(itemId)};
    return item;
}

// Return the first map item with the given prototype id on a valid hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Item> Server_Map_GetItemOnHex(ptr<Map> self, mpos hex, hstring pid)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item {self->GetItemOnHex(hex, pid, nullptr)};
    return item;
}

// Return the first map item with the given prototype descriptor on a valid hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Item> Server_Map_GetItemOnHex(ptr<Map> self, mpos hex, ptr<ProtoItem> proto)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item {self->GetItemOnHex(hex, proto->GetProtoId(), nullptr)};
    return item;
}

// Return the first item on a valid hex whose integer property equals the value
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Item> Server_Map_GetItemOnHex(ptr<Map> self, mpos hex, ItemProperty property, int32_t propertyValue)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto prop {ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property)};
    vector<ptr<Item>> hex_items {self->GetItemsOnHex(hex)};

    for (ptr<Item> item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item;
        }
    }

    return nullptr;
}

// Return the first item with the given prototype id within a non-negative radius
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Item> Server_Map_GetItemInRadius(ptr<Map> self, mpos hex, int32_t radius, hstring pid)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items {self->GetItemsInRadius(hex, radius)};

    for (ptr<Item> item : hex_items) {
        if (item->GetProtoId() == pid) {
            return item;
        }
    }

    return nullptr;
}

// Return the first item with the given prototype descriptor within a non-negative radius
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Item> Server_Map_GetItemInRadius(ptr<Map> self, mpos hex, int32_t radius, ptr<ProtoItem> proto)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items {self->GetItemsInRadius(hex, radius)};

    for (ptr<Item> item : hex_items) {
        if (item->GetProtoId() == proto->GetProtoId()) {
            return item;
        }
    }

    return nullptr;
}

// Return the first item within a non-negative radius whose integer property equals the value
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Item> Server_Map_GetItemInRadius(ptr<Map> self, mpos hex, int32_t radius, ItemProperty property, int32_t propertyValue)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto prop {ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property)};
    vector<ptr<Item>> hex_items {self->GetItemsInRadius(hex, radius)};

    for (ptr<Item> item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item;
        }
    }

    return nullptr;
}

// Return all dynamic map items
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Map_GetItems(ptr<Map> self)
{
    span<ptr<Item>> items {self->GetItems()};
    return vector<ptr<Item>>(items.begin(), items.end());
}

// Return dynamic map items with the given prototype id
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Map_GetItems(ptr<Map> self, hstring pid)
{
    span<ptr<Item>> map_items {self->GetItems()};

    vector<ptr<Item>> result {};
    result.reserve(map_items.size());

    for (ptr<Item> item : map_items) {
        if (item->GetProtoId() == pid) {
            result.emplace_back(item);
        }
    }

    return result;
}

// Return dynamic map items with the given prototype descriptor
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Map_GetItems(ptr<Map> self, ptr<ProtoItem> proto)
{
    span<ptr<Item>> map_items {self->GetItems()};

    vector<ptr<Item>> result {};
    result.reserve(map_items.size());

    for (ptr<Item> item : map_items) {
        if (item->GetProtoId() == proto->GetProtoId()) {
            result.emplace_back(item);
        }
    }

    return result;
}

// Return dynamic map items on a valid hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Map_GetItemsOnHex(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items {self->GetItemsOnHex(hex)};
    return hex_items;
}

// Return dynamic map items within a non-negative radius of a valid hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Map_GetItemsInRadius(ptr<Map> self, mpos hex, int32_t radius)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->GetItemsInRadius(hex, radius);
}

// Return dynamic map items with the given prototype id within a non-negative radius
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Map_GetItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, hstring pid)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items {self->GetItemsInRadius(hex, radius)};

    vector<ptr<Item>> result {};
    result.reserve(hex_items.size());

    for (ptr<Item> item : hex_items) {
        if (item->GetProtoId() == pid) {
            result.emplace_back(item);
        }
    }

    return result;
}

// Return dynamic map items with the given prototype descriptor within a non-negative radius
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Map_GetItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, ptr<ProtoItem> proto)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items {self->GetItemsInRadius(hex, radius)};

    vector<ptr<Item>> result {};
    result.reserve(hex_items.size());

    for (ptr<Item> item : hex_items) {
        if (item->GetProtoId() == proto->GetProtoId()) {
            result.emplace_back(item);
        }
    }

    return result;
}

// Return dynamic map items whose integer property equals the value
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Map_GetItems(ptr<Map> self, ItemProperty property, int32_t propertyValue)
{
    auto prop {ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property)};
    span<ptr<Item>> map_items {self->GetItems()};

    vector<ptr<Item>> result {};
    result.reserve(map_items.size());

    for (ptr<Item> item : map_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return result;
}

// Return dynamic map items on a valid hex whose integer property equals the value
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Map_GetItemsOnHex(ptr<Map> self, mpos hex, ItemProperty property, int32_t propertyValue)
{
    auto prop {ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property)};

    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items {self->GetItemsOnHex(hex)};

    vector<ptr<Item>> result {};
    result.reserve(hex_items.size());

    for (ptr<Item> item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return result;
}

// Return dynamic map items within a non-negative radius whose integer property equals the value
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Map_GetItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, ItemProperty property, int32_t propertyValue)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }

    auto prop {ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property)};

    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items {self->GetItemsInRadius(hex, radius)};

    vector<ptr<Item>> items {};
    items.reserve(hex_items.size());

    for (ptr<Item> item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            items.emplace_back(item);
        }
    }

    return items;
}

// Removal is one-way for the life of the map instance: clients already on the map are only ever told to drop
// a static item, so taking an id back out of the list is rejected rather than silently ignored
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_RemoveStaticItem(ptr<Map> self, ident_t staticItemId)
{
    if (!self->HasStaticItem(staticItemId)) {
        throw ScriptException("Static item not found on map", staticItemId);
    }

    return self->RemoveStaticItem(staticItemId);
}

// Remove a static item belonging to this map, rejecting items from another map
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_RemoveStaticItem(ptr<Map> self, ptr<StaticItem> staticItem)
{
    if (!self->HasStaticItem(staticItem->GetId())) {
        throw ScriptException("Static item belongs to another map", staticItem->GetId(), staticItem->GetProtoId());
    }

    return self->RemoveStaticItem(staticItem->GetId());
}

// Return a static map item by id
///@ ExportMethod
FO_SCRIPT_API nptr<StaticItem> Server_Map_GetStaticItem(ptr<Map> self, ident_t id)
{
    auto item {self->GetStaticItem(id)};
    return item;
}

// Return a static item with the given prototype id on a valid hex
///@ ExportMethod
FO_SCRIPT_API nptr<StaticItem> Server_Map_GetStaticItemOnHex(ptr<Map> self, mpos hex, hstring pid)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item {self->GetStaticItemOnHex(hex, pid)};
    return item;
}

// Return a static item with the given prototype descriptor on a valid hex
///@ ExportMethod
FO_SCRIPT_API nptr<StaticItem> Server_Map_GetStaticItemOnHex(ptr<Map> self, mpos hex, ptr<ProtoItem> proto)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item {self->GetStaticItemOnHex(hex, proto->GetProtoId())};
    return item;
}

// Return static items on a valid hex
///@ ExportMethod
FO_SCRIPT_API vector<ptr<StaticItem>> Server_Map_GetStaticItemsOnHex(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    const_span<ptr<StaticItem>> hex_static_items {self->GetStaticItemsOnHex(hex)};
    return vector<ptr<StaticItem>>(hex_static_items.begin(), hex_static_items.end());
}

// Return static items with the given prototype id within a non-negative radius
///@ ExportMethod
FO_SCRIPT_API vector<ptr<StaticItem>> Server_Map_GetStaticItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, hstring pid)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<StaticItem>> static_items {self->GetStaticItemsInRadius(hex, radius, pid)};
    return static_items;
}

// Return static items with the given prototype descriptor within a non-negative radius
///@ ExportMethod
FO_SCRIPT_API vector<ptr<StaticItem>> Server_Map_GetStaticItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, ptr<ProtoItem> proto)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<StaticItem>> static_items {self->GetStaticItemsInRadius(hex, radius, proto->GetProtoId())};
    return static_items;
}

// Return static items on a valid hex whose integer property equals the value
///@ ExportMethod
FO_SCRIPT_API vector<ptr<StaticItem>> Server_Map_GetStaticItemsOnHex(ptr<Map> self, mpos hex, ItemProperty property, int32_t propertyValue)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto prop {ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property)};
    const_span<ptr<StaticItem>> map_static_items {self->GetStaticItemsOnHex(hex)};

    vector<ptr<StaticItem>> result {};
    result.reserve(map_static_items.size());

    for (ptr<StaticItem> item : map_static_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return result;
}

// Return static items within a non-negative radius whose integer property equals the value
///@ ExportMethod
FO_SCRIPT_API vector<ptr<StaticItem>> Server_Map_GetStaticItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, ItemProperty property, int32_t propertyValue)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto prop {ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property)};
    vector<ptr<StaticItem>> map_static_items {self->GetStaticItemsInRadius(hex, radius, {})};

    vector<ptr<StaticItem>> result {};
    result.reserve(map_static_items.size());

    for (ptr<StaticItem> item : map_static_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return result;
}

// Return static items with the given prototype id
///@ ExportMethod
FO_SCRIPT_API vector<ptr<StaticItem>> Server_Map_GetStaticItems(ptr<Map> self, hstring pid)
{
    vector<ptr<StaticItem>> static_items {self->GetStaticItems(pid)};
    return static_items;
}

// Return static items with the given prototype descriptor
///@ ExportMethod
FO_SCRIPT_API vector<ptr<StaticItem>> Server_Map_GetStaticItems(ptr<Map> self, ptr<ProtoItem> proto)
{
    vector<ptr<StaticItem>> static_items {self->GetStaticItems(proto->GetProtoId())};
    return static_items;
}

// Return static items whose integer property equals the value
///@ ExportMethod
FO_SCRIPT_API vector<ptr<StaticItem>> Server_Map_GetStaticItems(ptr<Map> self, ItemProperty property, int32_t propertyValue)
{
    auto prop {ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property)};
    const_span<ptr<StaticItem>> map_static_items {self->GetStaticItems()};

    vector<ptr<StaticItem>> result {};
    result.reserve(map_static_items.size());

    for (ptr<StaticItem> item : map_static_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return result;
}

// Return all static items of this map
///@ ExportMethod
FO_SCRIPT_API vector<ptr<StaticItem>> Server_Map_GetStaticItems(ptr<Map> self)
{
    const_span<ptr<StaticItem>> map_static_items {self->GetStaticItems()};
    return vector<ptr<StaticItem>>(map_static_items.begin(), map_static_items.end());
}

// Return a map critter by id
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Critter> Server_Map_GetCritter(ptr<Map> self, ident_t crid)
{
    auto cr {self->GetCritter(crid)};
    return cr;
}

// Return a living critter on a valid hex, or a dead one when no living critter is present
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Critter> Server_Map_GetCritterOnHex(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto cr {self->GetCritterOnHex(hex, CritterFindType::NonDead)};

    if (!cr) {
        cr = self->GetCritterOnHex(hex, CritterFindType::Dead);
    }

    return cr;
}

// Return the first critter matching an integer property and find type
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER nptr<Critter> Server_Map_GetCritter(ptr<Map> self, CritterProperty property, int32_t propertyValue, CritterFindType findType)
{
    auto prop {ScriptHelpers::GetIntConvertibleEntityProperty<Critter>(self->GetEngine(), property)};

    for (ptr<Critter> cr : self->GetCritters(findType)) {
        if (cr->GetValueAsInt(prop) == propertyValue) {
            return cr;
        }
    }

    return nullptr;
}

// Return critters on a valid hex sorted by distance including multihex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCrittersOnHex(ptr<Map> self, mpos hex, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Critter>> critters {self->GetCrittersOnHex(hex, findType)};

    std::ranges::stable_sort(critters, [hex](ptr<const Critter> cr1, ptr<const Critter> cr2) {
        int32_t dist1 {GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex()};
        int32_t dist2 {GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex()};
        return dist1 < dist2;
    });

    return critters;
}

// Return critters within a non-negative radius of a valid hex sorted by distance
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCrittersInRadius(ptr<Map> self, mpos hex, int32_t radius, CritterFindType findType)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Critter>> critters {};

    if (radius == 0) {
        critters = self->GetCrittersOnHex(hex, findType);
    }
    else {
        critters = self->GetCrittersInRadius(hex, radius, findType);
    }

    std::ranges::stable_sort(critters, [hex](ptr<const Critter> cr1, ptr<const Critter> cr2) {
        int32_t dist1 {GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex()};
        int32_t dist2 {GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex()};
        return dist1 < dist2;
    });

    return critters;
}

// Return the number of player critters currently on the map
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Map_GetPlayerCritterCount(ptr<Map> self)
{
    // The map already maintains this membership, so a count needs no filtered copy of the whole population
    return numeric_cast<int32_t>(self->GetPlayerCritters().size());
}

// Return map critters matching the find type
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCritters(ptr<Map> self, CritterFindType findType)
{
    return self->GetCritters(findType);
}

// Return a snapshot of the players spectating this map
///@ ExportMethod
FO_SCRIPT_API vector<ptr<Player>> Server_Map_GetSpectatorPlayers(ptr<Map> self)
{
    // Spectator membership changes under the Player's cover, not the map's, so read the lock-guarded snapshot
    vector<refcount_ptr<Player>> spectators {self->GetSpectatorPlayersForSend()};

    return MakeScriptHandleVector<Player>(spectators);
}

// Return map critters matching the optional prototype id and find type
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCritters(ptr<Map> self, hstring pid, CritterFindType findType)
{
    vector<ptr<Critter>> map_critters {self->GetCritters(findType)};
    vector<ptr<Critter>> critters {};

    critters.reserve(map_critters.size());

    for (ptr<Critter> cr : map_critters) {
        if (!pid || cr->GetProtoId() == pid) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

// Return map critters matching the given prototype descriptor and find type
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCritters(ptr<Map> self, ptr<ProtoCritter> proto, CritterFindType findType)
{
    vector<ptr<Critter>> map_critters {self->GetCritters(findType)};
    vector<ptr<Critter>> critters {};

    critters.reserve(map_critters.size());

    for (ptr<Critter> cr : map_critters) {
        if (cr->GetProtoId() == proto->GetProtoId()) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

// Return map critters matching an integer property and find type
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCritters(ptr<Map> self, CritterProperty property, int32_t propertyValue, CritterFindType findType)
{
    auto prop {ScriptHelpers::GetIntConvertibleEntityProperty<Critter>(self->GetEngine(), property)};
    vector<ptr<Critter>> map_critters {self->GetCritters(findType)};
    vector<ptr<Critter>> critters {};

    critters.reserve(map_critters.size());

    for (ptr<Critter> cr : map_critters) {
        if (cr->GetValueAsInt(prop) == propertyValue) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

// Trace a path and return its critters matching the find type
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCrittersInPath(ptr<Map> self, mpos fromHex, mpos toHex, float32_t angle, int32_t dist, CritterFindType findType)
{
    auto trace_output {self->GetEngine()->MapMngr.TracePath(self, fromHex, toHex, dist, angle, nullptr, findType, false, true)};
    vector<ptr<const Critter>> trace_critters {trace_output.Critters};
    return MakeMutableScriptHandleVector<Critter>(trace_critters);
}

// Trace a path, write pre-block and block hexes, and return its critters
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCrittersInPath(ptr<Map> self, mpos fromHex, mpos toHex, float32_t angle, int32_t dist, CritterFindType findType, mpos& preBlockHex, mpos& blockHex)
{
    auto trace_output {self->GetEngine()->MapMngr.TracePath(self, fromHex, toHex, dist, angle, nullptr, findType, false, true)};
    preBlockHex = trace_output.PreBlock;
    blockHex = trace_output.Block;
    vector<ptr<const Critter>> trace_critters {trace_output.Critters};
    return MakeMutableScriptHandleVector<Critter>(trace_critters);
}

// Return critters whose look distance covers the hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCrittersWhoSeeHex(ptr<Map> self, mpos hex, CritterFindType findType)
{
    vector<ptr<Critter>> critters {};

    for (ptr<Critter> cr : self->GetCritters(findType)) {
        if (GeometryHelper::CheckDist(cr->GetHex(), hex, cr->GetLookDistance())) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

// Return critters whose look distance plus radius covers the hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCrittersWhoSeeHex(ptr<Map> self, mpos hex, int32_t radius, CritterFindType findType)
{
    vector<ptr<Critter>> critters {};

    for (ptr<Critter> cr : self->GetCritters(findType)) {
        if (GeometryHelper::CheckDist(cr->GetHex(), hex, cr->GetLookDistance() + radius)) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

// Return critters whose look circle intersects the path
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Critter>> Server_Map_GetCrittersWhoSeePath(ptr<Map> self, mpos fromHex, mpos toHex, CritterFindType findType)
{
    vector<ptr<Critter>> critters {};

    for (ptr<Critter> cr : self->GetCritters(findType)) {
        mpos hex {cr->GetHex()};

        if (GeometryHelper::IntersectCircleLine(hex.x, hex.y, cr->GetLookDistance(), fromHex.x, fromHex.y, toHex.x, toHex.y)) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

// Trace a path and write the last hex before the first blocker
///@ ExportMethod
FO_SCRIPT_API void Server_Map_GetHexInPath(ptr<Map> self, mpos fromHex, mpos& toHex, float32_t angle, int32_t dist)
{
    auto trace_output {self->GetEngine()->MapMngr.TracePath(self, fromHex, toHex, dist, angle)};
    toHex = trace_output.PreBlock;
}

// Trace a wall-only path and write the last movable hex, or the start hex when none
///@ ExportMethod
FO_SCRIPT_API void Server_Map_GetWallHexInPath(ptr<Map> self, mpos fromHex, mpos& toHex, float32_t angle, int32_t dist)
{
    auto trace_output {self->GetEngine()->MapMngr.TracePath(self, fromHex, toHex, dist, angle, nullptr, CritterFindType::Any, true)};

    if (trace_output.HasLastMovable) {
        toHex = trace_output.LastMovable;
    }
    else {
        toHex = fromHex;
    }
}

// Return the path length in steps from a valid hex to another, or zero when no path exists
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Map_GetPathLength(ptr<Map> self, mpos fromHex, mpos toHex, int32_t cut, ScriptFunc<bool, ptr<Item>> gagCallabck)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid from hex args");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid to hex args");
    }

    function<bool(ptr<const Item>)> gag_callback {};

    if (gagCallabck) {
        gag_callback = [gag_cb = safe_alloc::make_shared<ScriptFunc<bool, ptr<Item>>>(std::move(gagCallabck))](ptr<const Item> gag) mutable { return gag_cb->Call(make_ptr(const_cast<Item*>(std::addressof(*gag)))) && gag_cb->GetResult(); };
    }

    auto output {self->GetEngine()->MapMngr.FindPath(self, nullptr, fromHex, toHex, 0, cut, ipos16 {}, std::move(gag_callback))};

    if (output.Result != FindPathOutput::ResultType::Ok) {
        return 0;
    }

    return numeric_cast<int32_t>(output.Steps.size());
}

// Return the path length in steps for a critter to a valid hex, or zero when no path exists
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Map_GetPathLength(ptr<Map> self, ptr<Critter> cr, mpos toHex, int32_t cut, ScriptFunc<bool, ptr<Critter>, ptr<Item>> gagCallabck)
{
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid to hex args");
    }

    ValidateEntityAccess(cr);

    function<bool(ptr<const Item>)> gag_callback {};

    if (gagCallabck) {
        gag_callback = [gag_cb = safe_alloc::make_shared<ScriptFunc<bool, ptr<Critter>, ptr<Item>>>(std::move(gagCallabck)), cr](ptr<const Item> gag) mutable { return gag_cb->Call(cr, make_ptr(const_cast<Item*>(std::addressof(*gag)))) && gag_cb->GetResult(); };
    }

    auto output {self->GetEngine()->MapMngr.FindPath(self, cr, cr->GetHex(), toHex, cr->GetMultihex(), cut, ipos16 {}, std::move(gag_callback))};

    if (output.Result != FindPathOutput::ResultType::Ok) {
        return 0;
    }

    return numeric_cast<int32_t>(output.Steps.size());
}

// Find a path from a valid hex to any target and write its length and chosen target
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_FindPathToAny(ptr<Map> self, mpos fromHex, readonly_vector<mpos> targetHexes, int32_t& pathLength, mpos& targetHex, ScriptFunc<bool, ptr<Item>> gagCallback)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid from hex arg");
    }
    if (targetHexes.empty()) {
        throw ScriptException("Empty target hexes arg");
    }

    for (mpos candidateHex : targetHexes) {
        if (!self->GetSize().is_valid_pos(candidateHex)) {
            throw ScriptException("Invalid target hex arg");
        }
    }

    function<bool(ptr<const Item>)> gag_callback {};

    if (gagCallback) {
        gag_callback = [gag_cb = safe_alloc::make_shared<ScriptFunc<bool, ptr<Item>>>(std::move(gagCallback))](ptr<const Item> gag) mutable { return gag_cb->Call(make_ptr(const_cast<Item*>(std::addressof(*gag)))) && gag_cb->GetResult(); };
    }

    auto output {self->GetEngine()->MapMngr.FindPathToAny(self, nullptr, fromHex, targetHexes, 0, std::move(gag_callback))};
    pathLength = output.Result == FindPathOutput::ResultType::Ok ? numeric_cast<int32_t>(output.Steps.size()) : 0;

    if (output.Result != FindPathOutput::ResultType::Ok && output.Result != FindPathOutput::ResultType::AlreadyHere) {
        return false;
    }

    targetHex = output.NewToHex;
    return true;
}

// Find a path for a critter to any target and write its length and chosen target
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_FindPathToAny(ptr<Map> self, ptr<Critter> cr, readonly_vector<mpos> targetHexes, int32_t& pathLength, mpos& targetHex, ScriptFunc<bool, ptr<Critter>, ptr<Item>> gagCallback)
{
    if (targetHexes.empty()) {
        throw ScriptException("Empty target hexes arg");
    }

    for (mpos candidateHex : targetHexes) {
        if (!self->GetSize().is_valid_pos(candidateHex)) {
            throw ScriptException("Invalid target hex arg");
        }
    }

    ValidateEntityAccess(cr);

    function<bool(ptr<const Item>)> gag_callback {};

    if (gagCallback) {
        gag_callback = [gag_cb = safe_alloc::make_shared<ScriptFunc<bool, ptr<Critter>, ptr<Item>>>(std::move(gagCallback)), cr](ptr<const Item> gag) mutable { return gag_cb->Call(cr, make_ptr(const_cast<Item*>(std::addressof(*gag)))) && gag_cb->GetResult(); };
    }

    auto output {self->GetEngine()->MapMngr.FindPathToAny(self, cr, cr->GetHex(), targetHexes, cr->GetMultihex(), std::move(gag_callback))};
    pathLength = output.Result == FindPathOutput::ResultType::Ok ? numeric_cast<int32_t>(output.Steps.size()) : 0;

    if (output.Result != FindPathOutput::ResultType::Ok && output.Result != FindPathOutput::ResultType::AlreadyHere) {
        return false;
    }

    targetHex = output.NewToHex;
    return true;
}

// Answers which target hexes are reachable from the start within the configured path-length limit
///@ ExportMethod
FO_SCRIPT_API vector<mpos> Server_Map_FindReachableHexes(ptr<Map> self, mpos fromHex, readonly_vector<mpos> targetHexes, ScriptFunc<bool, ptr<Item>> gagCallback)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid from hex arg");
    }

    for (mpos candidateHex : targetHexes) {
        if (!self->GetSize().is_valid_pos(candidateHex)) {
            throw ScriptException("Invalid target hex arg");
        }
    }

    function<bool(ptr<const Item>)> gag_callback {};

    if (gagCallback) {
        gag_callback = [gag_cb = safe_alloc::make_shared<ScriptFunc<bool, ptr<Item>>>(std::move(gagCallback))](ptr<const Item> gag) mutable { return gag_cb->Call(make_ptr(const_cast<Item*>(std::addressof(*gag)))) && gag_cb->GetResult(); };
    }

    return self->GetEngine()->MapMngr.FindReachableHexes(self, fromHex, targetHexes, std::move(gag_callback));
}

// Create a critter from a prototype id on a valid map hex and direction
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Critter> Server_Map_AddCritter(ptr<Map> self, hstring protoId, mpos hex, mdir dir)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto cr {self->GetEngine()->CrMngr.CreateCritterOnMap(protoId, nullptr, self, hex, dir)};
    return cr;
}

// Create a critter from a prototype descriptor on a valid map hex and direction
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Critter> Server_Map_AddCritter(ptr<Map> self, ptr<ProtoCritter> proto, mpos hex, mdir dir)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto cr {self->GetEngine()->CrMngr.CreateCritterOnMap(proto->GetProtoId(), nullptr, self, hex, dir)};
    return cr;
}

// Create a critter with integer property overrides on a valid map hex and direction
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Critter> Server_Map_AddCritter(ptr<Map> self, hstring protoId, mpos hex, mdir dir, readonly_map<CritterProperty, int32_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto proto {self->GetEngine()->GetProtoCritter(protoId)};

    if (!proto) {
        throw ScriptException("Invalid critter proto id arg", protoId);
    }

    Properties props_ {proto->GetProperties()->Copy()};

    for (const auto& [key, value] : props) {
        props_.SetValueAsIntProps(static_cast<int32_t>(key), value);
    }

    auto cr {self->GetEngine()->CrMngr.CreateCritterOnMap(protoId, &props_, self, hex, dir)};
    return cr;
}

// Create a critter from a prototype descriptor with integer property overrides on a valid map hex
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Critter> Server_Map_AddCritter(ptr<Map> self, ptr<ProtoCritter> proto, mpos hex, mdir dir, readonly_map<CritterProperty, int32_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    Properties props_ {proto->GetProperties()->Copy()};

    for (const auto& [key, value] : props) {
        props_.SetValueAsIntProps(static_cast<int32_t>(key), value);
    }

    auto cr {self->GetEngine()->CrMngr.CreateCritterOnMap(proto->GetProtoId(), &props_, self, hex, dir)};
    return cr;
}

// Create a critter with property overrides on a valid map hex and direction
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Critter> Server_Map_AddCritter(ptr<Map> self, hstring protoId, mpos hex, mdir dir, readonly_map<CritterProperty, any_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto proto {self->GetEngine()->GetProtoCritter(protoId)};

    if (!proto) {
        throw ScriptException("Invalid critter proto id arg", protoId);
    }

    Properties props_ {proto->GetProperties()->Copy()};

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    auto cr {self->GetEngine()->CrMngr.CreateCritterOnMap(protoId, &props_, self, hex, dir)};
    return cr;
}

// Create a critter from a prototype descriptor with property overrides on a valid map hex and direction
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Critter> Server_Map_AddCritter(ptr<Map> self, ptr<ProtoCritter> proto, mpos hex, mdir dir, readonly_map<CritterProperty, any_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    Properties props_ {proto->GetProperties()->Copy()};

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    auto cr {self->GetEngine()->CrMngr.CreateCritterOnMap(proto->GetProtoId(), &props_, self, hex, dir)};
    return cr;
}

// Return whether the hex lies inside the map bounds
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexValid(ptr<Map> self, mpos hex)
{
    return self->GetSize().is_valid_pos(hex);
}

// Return whether a valid map hex is movable
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexMovable(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->IsHexMovable(hex);
}

// Return whether the hex and its neighborhood within the given radius are free for movement
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexesMovable(ptr<Map> self, mpos hex, int32_t radius)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->IsHexesMovable(hex, radius);
}

// Return whether a valid map hex is shootable
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexShootable(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->IsHexShootable(hex);
}

// Return whether a valid map hex lies outside an indoor area
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsOutsideArea(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->IsOutsideArea(hex);
}

// Check whether an item of this prototype can be placed on the hex
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_CheckPlaceForItem(ptr<Map> self, mpos hex, hstring pid)
{
    auto proto_ptr {self->GetEngine()->GetProtoItem(pid)};

    if (!proto_ptr) {
        throw ScriptException("Invalid item proto id arg", pid);
    }

    return self->IsValidPlaceForItem(hex, proto_ptr);
}

// Check whether an item of this prototype can be placed on the hex
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_CheckPlaceForItem(ptr<Map> self, mpos hex, ptr<ProtoItem> proto)
{
    return self->IsValidPlaceForItem(hex, proto);
}

// Set manual blocking for a valid map hex with optional full blocking
///@ ExportMethod
FO_SCRIPT_API void Server_Map_BlockHex(ptr<Map> self, mpos hex, bool full)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot modify a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    self->SetHexManualBlock(hex, true, full);
}

// Clear manual blocking for a valid map hex
///@ ExportMethod
FO_SCRIPT_API void Server_Map_UnblockHex(ptr<Map> self, mpos hex)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot modify a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    self->SetHexManualBlock(hex, false, false);
}

// Regenerate map content and placement caches
///@ ExportMethod
FO_SCRIPT_API void Server_Map_Regenerate(ptr<Map> self)
{
    self->GetEngine()->MapMngr.RegenerateMap(self);
}

// Move a hex one step in a direction using map bounds and return whether it moved
///@ ExportMethod
FO_SCRIPT_API bool Server_Map_MoveHexByDir(ptr<Map> self, mpos& hex, mdir dir)
{
    if (GeometryHelper::MoveHexByDir(hex, dir, self->GetSize())) {
        return true;
    }
    else {
        return false;
    }
}

// Move a hex up to the given steps in a direction and return the completed steps
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Map_MoveHexByDir(ptr<Map> self, mpos& hex, mdir dir, int32_t steps)
{
    int32_t result {0};

    for (int32_t i {0}; i < steps; i++) {
        if (GeometryHelper::MoveHexByDir(hex, dir, self->GetSize())) {
            result++;
        }
        else {
            break;
        }
    }

    return result;
}

// Verify map triggers for a critter crossing into a valid hex from the previous direction
///@ ExportMethod
FO_SCRIPT_API void Server_Map_VerifyTrigger(ptr<Map> self, FO_REQUIRES_COVER ptr<Critter> cr, mpos hex, mdir dir)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot modify a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    ValidateEntityAccess(cr);

    mpos from_hex {hex};

    if (GeometryHelper::MoveHexByDir(from_hex, dir.reverse(), self->GetSize())) {
        self->VerifyTrigger(cr, from_hex, hex, dir);
    }
}

FO_END_NAMESPACE
