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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

///@ ExportMethod
FO_SCRIPT_API void Server_Map_SetupScript(ptr<Map> self, ScriptFunc<void, Map*, bool> initFunc)
{
    if (initFunc.IsDelegate()) {
        throw ScriptException("Init function must not be a delegate");
    }
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc.GetName().first, true)) {
        throw ScriptException("Call init failed", initFunc.GetName().first);
    }

    self->SetInitScript(initFunc.GetName().first);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_SetupScriptEx(ptr<Map> self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API ptr<Location> Server_Map_GetLocation(ptr<Map> self)
{
    auto loc = self->GetLocation();
    return loc.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Item> Server_Map_AddItem(ptr<Map> self, mpos hex, hstring protoId, int32_t count)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    auto item = self->GetEngine()->ItemMngr.CreateItemOnHex(self, hex, protoId, count, nullptr);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Item> Server_Map_AddItem(ptr<Map> self, mpos hex, ptr<ProtoItem> proto, int32_t count)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    auto item = self->GetEngine()->ItemMngr.CreateItemOnHex(self, hex, proto->GetProtoId(), count, nullptr);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Item> Server_Map_AddItem(ptr<Map> self, mpos hex, hstring protoId, int32_t count, readonly_map<ItemProperty, int32_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    if (!props.empty()) {
        auto nullable_proto = self->GetEngine()->GetProtoItem(protoId);

        if (!nullable_proto) {
            throw ScriptException("Invalid item proto id arg", protoId);
        }

        auto proto = nullable_proto.as_ptr();
        Properties props_ = proto->GetProperties().Copy();

        for (const auto& [key, value] : props) {
            props_.SetValueAsIntProps(static_cast<int32_t>(key), value);
        }

        auto item = self->GetEngine()->ItemMngr.CreateItemOnHex(self, hex, protoId, count, &props_);
        return item.get_no_const();
    }

    auto item = self->GetEngine()->ItemMngr.CreateItemOnHex(self, hex, protoId, count, nullptr);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Item> Server_Map_AddItem(ptr<Map> self, mpos hex, ptr<ProtoItem> proto, int32_t count, readonly_map<ItemProperty, int32_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    if (!props.empty()) {
        Properties props_ = proto->GetProperties().Copy();

        for (const auto& [key, value] : props) {
            props_.SetValueAsIntProps(static_cast<int32_t>(key), value);
        }

        auto item = self->GetEngine()->ItemMngr.CreateItemOnHex(self, hex, proto->GetProtoId(), count, &props_);
        return item.get_no_const();
    }

    auto item = self->GetEngine()->ItemMngr.CreateItemOnHex(self, hex, proto->GetProtoId(), count, nullptr);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Map_GetItem(ptr<Map> self, ident_t itemId)
{
    if (!itemId) {
        return nullptr;
    }

    auto item = self->GetItem(itemId);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Map_GetItemOnHex(ptr<Map> self, mpos hex, hstring pid)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item = self->GetItemOnHex(hex, pid, nullptr);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Map_GetItemOnHex(ptr<Map> self, mpos hex, ptr<ProtoItem> proto)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item = self->GetItemOnHex(hex, proto->GetProtoId(), nullptr);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Map_GetItemOnHex(ptr<Map> self, mpos hex, ItemProperty property, int32_t propertyValue)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    vector<ptr<Item>> hex_items = self->GetItemsOnHex(hex);

    for (ptr<Item> item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item.get_no_const();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Map_GetItemInRadius(ptr<Map> self, mpos hex, int32_t radius, hstring pid)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items = self->GetItemsInRadius(hex, radius);

    for (ptr<Item> item : hex_items) {
        if (item->GetProtoId() == pid) {
            return item.get_no_const();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Map_GetItemInRadius(ptr<Map> self, mpos hex, int32_t radius, ptr<ProtoItem> proto)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items = self->GetItemsInRadius(hex, radius);

    for (ptr<Item> item : hex_items) {
        if (item->GetProtoId() == proto->GetProtoId()) {
            return item.get_no_const();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Map_GetItemInRadius(ptr<Map> self, mpos hex, int32_t radius, ItemProperty property, int32_t propertyValue)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    vector<ptr<Item>> hex_items = self->GetItemsInRadius(hex, radius);

    for (ptr<Item> item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item.get_no_const();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(ptr<Map> self)
{
    return MakeScriptHandleVector<Item>(self->GetItems());
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(ptr<Map> self, hstring pid)
{
    span<ptr<Item>> map_items = self->GetItems();

    vector<ptr<Item>> result;
    result.reserve(map_items.size());

    for (ptr<Item> item : map_items) {
        if (item->GetProtoId() == pid) {
            result.emplace_back(item);
        }
    }

    return MakeScriptHandleVector<Item>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(ptr<Map> self, ptr<ProtoItem> proto)
{
    span<ptr<Item>> map_items = self->GetItems();

    vector<ptr<Item>> result;
    result.reserve(map_items.size());

    for (ptr<Item> item : map_items) {
        if (item->GetProtoId() == proto->GetProtoId()) {
            result.emplace_back(item);
        }
    }

    return MakeScriptHandleVector<Item>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItemsOnHex(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items = self->GetItemsOnHex(hex);
    return MakeScriptHandleVector<Item>(hex_items);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItemsInRadius(ptr<Map> self, mpos hex, int32_t radius)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return MakeScriptHandleVector<Item>(self->GetItemsInRadius(hex, radius));
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, hstring pid)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items = self->GetItemsInRadius(hex, radius);

    vector<ptr<Item>> result;
    result.reserve(hex_items.size());

    for (ptr<Item> item : hex_items) {
        if (item->GetProtoId() == pid) {
            result.emplace_back(item);
        }
    }

    return MakeScriptHandleVector<Item>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, ptr<ProtoItem> proto)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items = self->GetItemsInRadius(hex, radius);

    vector<ptr<Item>> result;
    result.reserve(hex_items.size());

    for (ptr<Item> item : hex_items) {
        if (item->GetProtoId() == proto->GetProtoId()) {
            result.emplace_back(item);
        }
    }

    return MakeScriptHandleVector<Item>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(ptr<Map> self, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    span<ptr<Item>> map_items = self->GetItems();

    vector<ptr<Item>> result;
    result.reserve(map_items.size());

    for (ptr<Item> item : map_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return MakeScriptHandleVector<Item>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItemsOnHex(ptr<Map> self, mpos hex, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items = self->GetItemsOnHex(hex);

    vector<ptr<Item>> result;
    result.reserve(hex_items.size());

    for (ptr<Item> item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return MakeScriptHandleVector<Item>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Item>> hex_items = self->GetItemsInRadius(hex, radius);

    vector<ptr<Item>> items;
    items.reserve(hex_items.size());

    for (ptr<Item> item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            items.emplace_back(item);
        }
    }

    return MakeScriptHandleVector<Item>(items);
}

///@ ExportMethod
FO_SCRIPT_API nptr<StaticItem> Server_Map_GetStaticItem(ptr<Map> self, ident_t id)
{
    auto item = self->GetStaticItem(id);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<StaticItem> Server_Map_GetStaticItemOnHex(ptr<Map> self, mpos hex, hstring pid)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item = self->GetStaticItemOnHex(hex, pid);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<StaticItem> Server_Map_GetStaticItemOnHex(ptr<Map> self, mpos hex, ptr<ProtoItem> proto)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto item = self->GetStaticItemOnHex(hex, proto->GetProtoId());
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItemsOnHex(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    span<ptr<StaticItem>> hex_static_items = self->GetStaticItemsOnHex(hex);
    return MakeScriptHandleVector<StaticItem>(hex_static_items);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, hstring pid)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<StaticItem>> static_items = self->GetStaticItemsInRadius(hex, radius, pid);
    return MakeScriptHandleVector<StaticItem>(static_items);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, ptr<ProtoItem> proto)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<StaticItem>> static_items = self->GetStaticItemsInRadius(hex, radius, proto->GetProtoId());
    return MakeScriptHandleVector<StaticItem>(static_items);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItemsOnHex(ptr<Map> self, mpos hex, ItemProperty property, int32_t propertyValue)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    span<ptr<StaticItem>> map_static_items = self->GetStaticItemsOnHex(hex);

    vector<ptr<StaticItem>> result;
    result.reserve(map_static_items.size());

    for (ptr<StaticItem> item : map_static_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return MakeScriptHandleVector<StaticItem>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItemsInRadius(ptr<Map> self, mpos hex, int32_t radius, ItemProperty property, int32_t propertyValue)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    vector<ptr<StaticItem>> map_static_items = self->GetStaticItemsInRadius(hex, radius, {});

    vector<ptr<StaticItem>> result;
    result.reserve(map_static_items.size());

    for (ptr<StaticItem> item : map_static_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return MakeScriptHandleVector<StaticItem>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(ptr<Map> self, hstring pid)
{
    vector<ptr<StaticItem>> static_items = self->GetStaticItems(pid);
    return MakeScriptHandleVector<StaticItem>(static_items);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(ptr<Map> self, ptr<ProtoItem> proto)
{
    vector<ptr<StaticItem>> static_items = self->GetStaticItems(proto->GetProtoId());
    return MakeScriptHandleVector<StaticItem>(static_items);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(ptr<Map> self, ItemProperty property, int32_t propertyValue)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    span<ptr<StaticItem>> map_static_items = self->GetStaticItems();

    vector<ptr<StaticItem>> result;
    result.reserve(map_static_items.size());

    for (ptr<StaticItem> item : map_static_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return MakeScriptHandleVector<StaticItem>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(ptr<Map> self)
{
    span<ptr<StaticItem>> map_static_items = self->GetStaticItems();
    return MakeScriptHandleVector<StaticItem>(map_static_items);
}

///@ ExportMethod
FO_SCRIPT_API nptr<Critter> Server_Map_GetCritter(ptr<Map> self, ident_t crid)
{
    auto cr = self->GetCritter(crid);
    return cr.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Critter> Server_Map_GetCritterOnHex(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto cr = self->GetCritterOnHex(hex, CritterFindType::NonDead);

    if (!cr) {
        cr = self->GetCritterOnHex(hex, CritterFindType::Dead);
    }

    return cr.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API nptr<Critter> Server_Map_GetCritter(ptr<Map> self, CritterProperty property, int32_t propertyValue, CritterFindType findType)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Critter>(self->GetEngine(), property);
    span<ptr<Critter>> map_critters = self->GetCritters();

    for (ptr<Critter> cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetValueAsInt(prop) == propertyValue) {
            return cr.get_no_const();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersOnHex(ptr<Map> self, mpos hex, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Critter>> critters = self->GetCrittersOnHex(hex, findType);

    std::ranges::stable_sort(critters, [hex](ptr<const Critter> cr1, ptr<const Critter> cr2) {
        const int32_t dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        const int32_t dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return MakeScriptHandleVector<Critter>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersInRadius(ptr<Map> self, mpos hex, int32_t radius, CritterFindType findType)
{
    if (radius < 0) {
        throw ScriptException("Radius arg must not be negative", radius);
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    vector<ptr<Critter>> critters;

    if (radius == 0) {
        critters = self->GetCrittersOnHex(hex, findType);
    }
    else {
        critters = self->GetCrittersInRadius(hex, radius, findType);
    }

    std::ranges::stable_sort(critters, [hex](ptr<const Critter> cr1, ptr<const Critter> cr2) {
        const int32_t dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        const int32_t dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return MakeScriptHandleVector<Critter>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(ptr<Map> self, CritterFindType findType)
{
    vector<ptr<Critter>> critters;
    span<ptr<Critter>> map_critters = self->GetCritters();

    critters.reserve(map_critters.size());

    for (ptr<Critter> cr : map_critters) {
        if (cr->CheckFind(findType)) {
            critters.emplace_back(cr);
        }
    }

    return MakeScriptHandleVector<Critter>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(ptr<Map> self, hstring pid, CritterFindType findType)
{
    vector<ptr<Critter>> critters;
    span<ptr<Critter>> map_critters = self->GetCritters();

    critters.reserve(map_critters.size());

    for (ptr<Critter> cr : map_critters) {
        if ((!pid || cr->GetProtoId() == pid) && cr->CheckFind(findType)) {
            critters.emplace_back(cr);
        }
    }

    return MakeScriptHandleVector<Critter>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(ptr<Map> self, ptr<ProtoCritter> proto, CritterFindType findType)
{
    vector<ptr<Critter>> critters;
    span<ptr<Critter>> map_critters = self->GetCritters();

    critters.reserve(map_critters.size());

    for (ptr<Critter> cr : map_critters) {
        if (cr->GetProtoId() == proto->GetProtoId() && cr->CheckFind(findType)) {
            critters.emplace_back(cr);
        }
    }

    return MakeScriptHandleVector<Critter>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(ptr<Map> self, CritterProperty property, int32_t propertyValue, CritterFindType findType)
{
    auto prop = ScriptHelpers::GetIntConvertibleEntityProperty<Critter>(self->GetEngine(), property);
    span<ptr<Critter>> map_critters = self->GetCritters();
    vector<ptr<Critter>> critters;

    critters.reserve(map_critters.size());

    for (ptr<Critter> cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetValueAsInt(prop) == propertyValue) {
            critters.emplace_back(cr);
        }
    }

    return MakeScriptHandleVector<Critter>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersInPath(ptr<Map> self, mpos fromHex, mpos toHex, float32_t angle, int32_t dist, CritterFindType findType)
{
    auto trace_output = self->GetEngine()->MapMngr.TracePath(self, fromHex, toHex, dist, angle, nullptr, findType, false, true);
    vector<ptr<const Critter>> trace_critters = trace_output.Critters;
    return MakeMutableScriptHandleVector<Critter>(trace_critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersInPath(ptr<Map> self, mpos fromHex, mpos toHex, float32_t angle, int32_t dist, CritterFindType findType, mpos& preBlockHex, mpos& blockHex)
{
    auto trace_output = self->GetEngine()->MapMngr.TracePath(self, fromHex, toHex, dist, angle, nullptr, findType, false, true);
    preBlockHex = trace_output.PreBlock;
    blockHex = trace_output.Block;
    vector<ptr<const Critter>> trace_critters = trace_output.Critters;
    return MakeMutableScriptHandleVector<Critter>(trace_critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersWhoSeeHex(ptr<Map> self, mpos hex, CritterFindType findType)
{
    vector<ptr<Critter>> critters;
    span<ptr<Critter>> map_critters = self->GetCritters();

    for (ptr<Critter> cr : map_critters) {
        if (cr->CheckFind(findType) && GeometryHelper::CheckDist(cr->GetHex(), hex, cr->GetLookDistance())) {
            critters.emplace_back(cr);
        }
    }

    return MakeScriptHandleVector<Critter>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersWhoSeeHex(ptr<Map> self, mpos hex, int32_t radius, CritterFindType findType)
{
    vector<ptr<Critter>> critters;
    span<ptr<Critter>> map_critters = self->GetCritters();

    for (ptr<Critter> cr : map_critters) {
        if (cr->CheckFind(findType) && GeometryHelper::CheckDist(cr->GetHex(), hex, cr->GetLookDistance() + radius)) {
            critters.emplace_back(cr);
        }
    }

    return MakeScriptHandleVector<Critter>(critters);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersWhoSeePath(ptr<Map> self, mpos fromHex, mpos toHex, CritterFindType findType)
{
    vector<ptr<Critter>> critters;
    span<ptr<Critter>> map_critters = self->GetCritters();

    for (ptr<Critter> cr : map_critters) {
        const mpos hex = cr->GetHex();

        if (cr->CheckFind(findType) && GeometryHelper::IntersectCircleLine(hex.x, hex.y, cr->GetLookDistance(), fromHex.x, fromHex.y, toHex.x, toHex.y)) {
            critters.emplace_back(cr);
        }
    }

    return MakeScriptHandleVector<Critter>(critters);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_GetHexInPath(ptr<Map> self, mpos fromHex, mpos& toHex, float32_t angle, int32_t dist)
{
    const auto trace_output = self->GetEngine()->MapMngr.TracePath(self, fromHex, toHex, dist, angle);
    toHex = trace_output.PreBlock;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_GetWallHexInPath(ptr<Map> self, mpos fromHex, mpos& toHex, float32_t angle, int32_t dist)
{
    const auto trace_output = self->GetEngine()->MapMngr.TracePath(self, fromHex, toHex, dist, angle, nullptr, CritterFindType::Any, true);

    if (trace_output.HasLastMovable) {
        toHex = trace_output.LastMovable;
    }
    else {
        toHex = fromHex;
    }
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Map_GetPathLength(ptr<Map> self, mpos fromHex, mpos toHex, int32_t cut, ScriptFunc<bool, Item*> gagCallabck)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid from hex args");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid to hex args");
    }

    function<bool(ptr<const Item>)> gag_callback;

    if (gagCallabck) {
        gag_callback = [gag_cb = SafeAlloc::MakeShared<ScriptFunc<bool, Item*>>(std::move(gagCallabck))](ptr<const Item> gag) mutable { return gag_cb->Call(const_cast<Item*>(std::addressof(*gag))) && gag_cb->GetResult(); };
    }

    const auto output = self->GetEngine()->MapMngr.FindPath(self, nullptr, fromHex, toHex, 0, cut, ipos16 {}, std::move(gag_callback));

    if (output.Result != FindPathOutput::ResultType::Ok) {
        return 0;
    }

    return numeric_cast<int32_t>(output.Steps.size());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Map_GetPathLength(ptr<Map> self, ptr<Critter> cr, mpos toHex, int32_t cut, ScriptFunc<bool, Critter*, Item*> gagCallabck)
{
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid to hex args");
    }

    function<bool(ptr<const Item>)> gag_callback;

    if (gagCallabck) {
        gag_callback = [gag_cb = SafeAlloc::MakeShared<ScriptFunc<bool, Critter*, Item*>>(std::move(gagCallabck)), cr](ptr<const Item> gag) mutable { return gag_cb->Call(cr.get(), const_cast<Item*>(std::addressof(*gag))) && gag_cb->GetResult(); };
    }

    const auto output = self->GetEngine()->MapMngr.FindPath(self, cr, cr->GetHex(), toHex, cr->GetMultihex(), cut, ipos16 {}, std::move(gag_callback));

    if (output.Result != FindPathOutput::ResultType::Ok) {
        return 0;
    }

    return numeric_cast<int32_t>(output.Steps.size());
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Map_AddCritter(ptr<Map> self, hstring protoId, mpos hex, mdir dir)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto cr = self->GetEngine()->CrMngr.CreateCritterOnMap(protoId, nullptr, self, hex, dir);
    return cr.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Map_AddCritter(ptr<Map> self, ptr<ProtoCritter> proto, mpos hex, mdir dir)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto cr = self->GetEngine()->CrMngr.CreateCritterOnMap(proto->GetProtoId(), nullptr, self, hex, dir);
    return cr.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Map_AddCritter(ptr<Map> self, hstring protoId, mpos hex, mdir dir, readonly_map<CritterProperty, int32_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto nullable_proto = self->GetEngine()->GetProtoCritter(protoId);

    if (!nullable_proto) {
        throw ScriptException("Invalid critter proto id arg", protoId);
    }

    auto proto = nullable_proto.as_ptr();
    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsIntProps(static_cast<int32_t>(key), value);
    }

    auto cr = self->GetEngine()->CrMngr.CreateCritterOnMap(protoId, &props_, self, hex, dir);
    return cr.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Map_AddCritter(ptr<Map> self, ptr<ProtoCritter> proto, mpos hex, mdir dir, readonly_map<CritterProperty, int32_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsIntProps(static_cast<int32_t>(key), value);
    }

    auto cr = self->GetEngine()->CrMngr.CreateCritterOnMap(proto->GetProtoId(), &props_, self, hex, dir);
    return cr.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Map_AddCritter(ptr<Map> self, hstring protoId, mpos hex, mdir dir, readonly_map<CritterProperty, any_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto nullable_proto = self->GetEngine()->GetProtoCritter(protoId);

    if (!nullable_proto) {
        throw ScriptException("Invalid critter proto id arg", protoId);
    }

    auto proto = nullable_proto.as_ptr();
    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    auto cr = self->GetEngine()->CrMngr.CreateCritterOnMap(protoId, &props_, self, hex, dir);
    return cr.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Map_AddCritter(ptr<Map> self, ptr<ProtoCritter> proto, mpos hex, mdir dir, readonly_map<CritterProperty, any_t> props)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add a critter to a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    auto cr = self->GetEngine()->CrMngr.CreateCritterOnMap(proto->GetProtoId(), &props_, self, hex, dir);
    return cr.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexValid(ptr<Map> self, mpos hex)
{
    return self->GetSize().is_valid_pos(hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexMovable(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->IsHexMovable(hex);
}

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

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexShootable(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->IsHexShootable(hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsOutsideArea(ptr<Map> self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    return self->IsOutsideArea(hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_CheckPlaceForItem(ptr<Map> self, mpos hex, hstring pid)
{
    auto proto_ptr = self->GetEngine()->GetProtoItem(pid);

    if (!proto_ptr) {
        throw ScriptException("Invalid item proto id arg", pid);
    }

    return self->IsValidPlaceForItem(hex, proto_ptr.as_ptr());
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_CheckPlaceForItem(ptr<Map> self, mpos hex, ptr<ProtoItem> proto)
{
    return self->IsValidPlaceForItem(hex, proto);
}

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

///@ ExportMethod
FO_SCRIPT_API void Server_Map_Regenerate(ptr<Map> self)
{
    self->GetEngine()->MapMngr.RegenerateMap(self);
}

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

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Map_MoveHexByDir(ptr<Map> self, mpos& hex, mdir dir, int32_t steps)
{
    int32_t result = 0;

    for (int32_t i = 0; i < steps; i++) {
        if (GeometryHelper::MoveHexByDir(hex, dir, self->GetSize())) {
            result++;
        }
        else {
            break;
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_VerifyTrigger(ptr<Map> self, ptr<Critter> cr, mpos hex, mdir dir)
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot modify a map that is being destroyed", self->GetId());
    }
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex arg");
    }

    auto from_hex = hex;

    if (GeometryHelper::MoveHexByDir(from_hex, dir.reverse(), self->GetSize())) {
        self->VerifyTrigger(cr, from_hex, hex, dir);
    }
}

FO_END_NAMESPACE
