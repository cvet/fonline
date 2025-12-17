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

#include "Common.h"

#include "Geometry.h"
#include "Server.h"

FO_BEGIN_NAMESPACE();

///@ ExportMethod
FO_SCRIPT_API void Server_Map_SetupScript(Map* self, InitFunc<Map*> initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_SetupScriptEx(Map* self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Map_GetLocation(Map* self)
{
    return self->GetLocation();
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_AddItem(Map* self, mpos hex, hstring protoId, int32 count)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    if (count <= 0) {
        return nullptr;
    }

    return self->GetEngine()->CreateItemOnHex(self, hex, protoId, count, nullptr);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_AddItem(Map* self, mpos hex, hstring protoId, int32 count, const map<ItemProperty, int32>& props)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    if (count <= 0) {
        return nullptr;
    }

    if (!props.empty()) {
        const auto* proto = self->GetEngine()->ProtoMngr.GetProtoItem(protoId);
        Properties props_ = proto->GetProperties().Copy();

        for (const auto& [key, value] : props) {
            props_.SetValueAsIntProps(static_cast<int32>(key), value);
        }

        return self->GetEngine()->CreateItemOnHex(self, hex, protoId, count, &props_);
    }

    return self->GetEngine()->CreateItemOnHex(self, hex, protoId, count, nullptr);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, ident_t itemId)
{
    if (!itemId) {
        return nullptr;
    }

    return self->GetItem(itemId);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, mpos hex, hstring pid)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return self->GetItemOnHex(hex, pid, nullptr);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, mpos hex, ItemComponent component)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto hex_items = self->GetItemsOnHex(hex);

    for (auto& item : hex_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return item.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, mpos hex, ItemProperty property, int32 propertyValue)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    const auto hex_items = self->GetItemsOnHex(hex);

    for (auto& item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, mpos hex, int32 radius, hstring pid)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    auto hex_items = self->GetItemsInRadius(hex, radius);

    for (auto& item : hex_items) {
        if (item->GetProtoId() == pid) {
            return item.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, mpos hex, int32 radius, ItemComponent component)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    auto hex_items = self->GetItemsInRadius(hex, radius);

    for (auto& item : hex_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return item.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, mpos hex, int32 radius, ItemProperty property, int32 propertyValue)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    auto hex_items = self->GetItemsInRadius(hex, radius);

    for (auto& item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self)
{
    return vec_transform(self->GetItems(), [](auto&& item) -> Item* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, hstring pid)
{
    const auto map_items = self->GetItems();

    vector<Item*> result;
    result.reserve(map_items.size());

    for (auto& item : map_items) {
        if (item->GetProtoId() == pid) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto hex_items = self->GetItemsOnHex(hex);
    return vec_transform(hex_items, [](auto&& item) -> Item* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, int32 radius)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return vec_transform(self->GetItemsInRadius(hex, radius), [](auto&& item) -> Item* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, int32 radius, hstring pid)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    auto hex_items = self->GetItemsInRadius(hex, radius);

    vector<Item*> result;
    result.reserve(hex_items.size());

    for (auto& item : hex_items) {
        if (item->GetProtoId() == pid) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, ItemComponent component)
{
    const auto map_items = self->GetItems();

    vector<Item*> result;
    result.reserve(map_items.size());

    for (auto& item : map_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, ItemProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    const auto map_items = self->GetItems();

    vector<Item*> result;
    result.reserve(map_items.size());

    for (auto& item : map_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, ItemComponent component)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto hex_items = self->GetItemsOnHex(hex);

    vector<Item*> result;
    result.reserve(hex_items.size());

    for (auto& item : hex_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, ItemProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto hex_items = self->GetItemsOnHex(hex);

    vector<Item*> result;
    result.reserve(hex_items.size());

    for (auto& item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, int32 radius, ItemComponent component)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    auto hex_items = self->GetItemsInRadius(hex, radius);

    vector<Item*> result;
    result.reserve(hex_items.size());

    for (auto& item : hex_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, int32 radius, ItemProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    auto hex_items = self->GetItemsInRadius(hex, radius);

    vector<Item*> items;
    items.reserve(hex_items.size());

    for (auto& item : hex_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            items.emplace_back(item.get());
        }
    }

    return items;
}

///@ ExportMethod
FO_SCRIPT_API StaticItem* Server_Map_GetStaticItem(Map* self, ident_t id)
{
    return self->GetStaticItem(id);
}

///@ ExportMethod
FO_SCRIPT_API StaticItem* Server_Map_GetStaticItem(Map* self, mpos hex, hstring pid)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return self->GetStaticItemOnHex(hex, pid);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto hex_static_items = self->GetStaticItemsOnHex(hex);
    return vec_transform(hex_static_items, [](auto&& item) -> StaticItem* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, mpos hex, int32 radius, hstring pid)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return self->GetStaticItemsInRadius(hex, radius, pid);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, mpos hex, ItemComponent component)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto map_static_items = self->GetStaticItemsOnHex(hex);

    vector<StaticItem*> result;
    result.reserve(map_static_items.size());

    for (auto& item : map_static_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, mpos hex, int32 radius, ItemComponent component)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto map_static_items = self->GetStaticItemsInRadius(hex, radius, {});

    vector<StaticItem*> result;
    result.reserve(map_static_items.size());

    for (auto* item : map_static_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(item);
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, mpos hex, ItemProperty property, int32 propertyValue)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    const auto map_static_items = self->GetStaticItemsOnHex(hex);

    vector<StaticItem*> result;
    result.reserve(map_static_items.size());

    for (auto& item : map_static_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, mpos hex, int32 radius, ItemProperty property, int32 propertyValue)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    const auto map_static_items = self->GetStaticItemsInRadius(hex, radius, {});

    vector<StaticItem*> result;
    result.reserve(map_static_items.size());

    for (auto* item : map_static_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item);
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, hstring pid)
{
    return self->GetStaticItems(pid);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, ItemComponent component)
{
    const auto map_static_items = self->GetStaticItems();

    vector<StaticItem*> result;
    result.reserve(map_static_items.size());

    for (auto& item : map_static_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, ItemProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    const auto map_static_items = self->GetStaticItems();

    vector<StaticItem*> result;
    result.reserve(map_static_items.size());

    for (auto& item : map_static_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self)
{
    const auto map_static_items = self->GetStaticItems();
    return vec_transform(map_static_items, [](auto&& item) -> StaticItem* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_GetCritter(Map* self, ident_t crid)
{
    return self->GetCritter(crid);
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_GetCritter(Map* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    auto* cr = self->GetCritterOnHex(hex, CritterFindType::NonDead);

    if (cr == nullptr) {
        cr = self->GetCritterOnHex(hex, CritterFindType::Dead);
    }

    return cr;
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_GetCritter(Map* self, CritterComponent component, CritterFindType findType)
{
    const auto map_critters = self->GetCritters();

    for (auto& cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return cr.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_GetCritter(Map* self, CritterProperty property, int32 propertyValue, CritterFindType findType)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Critter>(self->GetEngine(), property);
    const auto map_critters = self->GetCritters();

    for (auto& cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetValueAsInt(prop) == propertyValue) {
            return cr.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(Map* self, mpos hex, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    vector<Critter*> critters = self->GetCrittersOnHex(hex, findType);

    std::ranges::stable_sort(critters, [hex](const Critter* cr1, const Critter* cr2) {
        const auto dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        const auto dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(Map* self, mpos hex, int32 radius, CritterFindType findType)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    vector<Critter*> critters;

    if (radius == 0) {
        critters = self->GetCrittersOnHex(hex, findType);
    }
    else {
        critters = self->GetCrittersInRadius(hex, radius, findType);
    }

    std::ranges::stable_sort(critters, [hex](const Critter* cr1, const Critter* cr2) {
        const auto dist1 = GeometryHelper::GetDistance(hex, cr1->GetHex()) - cr1->GetMultihex();
        const auto dist2 = GeometryHelper::GetDistance(hex, cr2->GetHex()) - cr2->GetMultihex();
        return dist1 < dist2;
    });

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(Map* self, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto map_critters = self->GetCritters();

    critters.reserve(map_critters.size());

    for (auto& cr : map_critters) {
        if (cr->CheckFind(findType)) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(Map* self, hstring pid, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto map_critters = self->GetCritters();

    critters.reserve(map_critters.size());

    for (auto& cr : map_critters) {
        if ((!pid || cr->GetProtoId() == pid) && cr->CheckFind(findType)) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(Map* self, CritterComponent component, CritterFindType findType)
{
    const auto map_critters = self->GetCritters();
    vector<Critter*> critters;

    critters.reserve(map_critters.size());

    for (auto& cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(Map* self, CritterProperty property, int32 propertyValue, CritterFindType findType)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Critter>(self->GetEngine(), property);
    const auto map_critters = self->GetCritters();
    vector<Critter*> critters;

    critters.reserve(map_critters.size());

    for (auto& cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetValueAsInt(prop) == propertyValue) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersInPath(Map* self, mpos fromHex, mpos toHex, float32 angle, int32 dist, CritterFindType findType)
{
    TracePathInput trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.MaxDist = dist;
    trace.Angle = angle;
    trace.CollectCritters = true;
    trace.FindType = findType;

    auto trace_output = self->GetEngine()->MapMngr.TracePath(trace);
    return vec_transform(trace_output.Critters, [](auto&& cr) -> Critter* { return cr.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersInPath(Map* self, mpos fromHex, mpos toHex, float32 angle, int32 dist, CritterFindType findType, mpos& preBlockHex, mpos& blockHex)
{
    TracePathInput trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.MaxDist = dist;
    trace.Angle = angle;
    trace.CollectCritters = true;
    trace.FindType = findType;

    auto trace_output = self->GetEngine()->MapMngr.TracePath(trace);
    preBlockHex = trace_output.PreBlock;
    blockHex = trace_output.Block;
    return vec_transform(trace_output.Critters, [](auto&& cr) -> Critter* { return cr.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersWhoSeeHex(Map* self, mpos hex, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto map_critters = self->GetCritters();

    for (auto cr : map_critters) {
        if (cr->CheckFind(findType) && GeometryHelper::CheckDist(cr->GetHex(), hex, cr->GetLookDistance())) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersWhoSeeHex(Map* self, mpos hex, int32 radius, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto map_critters = self->GetCritters();

    for (auto& cr : map_critters) {
        if (cr->CheckFind(findType) && GeometryHelper::CheckDist(cr->GetHex(), hex, cr->GetLookDistance() + radius)) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersWhoSeePath(Map* self, mpos fromHex, mpos toHex, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto map_critters = self->GetCritters();

    for (auto& cr : map_critters) {
        const auto hex = cr->GetHex();

        if (cr->CheckFind(findType) && GenericUtils::IntersectCircleLine(hex.x, hex.y, cr->GetLookDistance(), fromHex.x, fromHex.y, toHex.x, toHex.y)) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_GetHexInPath(Map* self, mpos fromHex, mpos& toHex, float32 angle, int32 dist)
{
    TracePathInput trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.MaxDist = dist;
    trace.Angle = angle;

    const auto trace_output = self->GetEngine()->MapMngr.TracePath(trace);
    toHex = trace_output.PreBlock;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_GetWallHexInPath(Map* self, mpos fromHex, mpos& toHex, float32 angle, int32 dist)
{
    TracePathInput trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.MaxDist = dist;
    trace.Angle = angle;
    trace.CheckLastMovable = true;

    const auto trace_output = self->GetEngine()->MapMngr.TracePath(trace);

    if (trace_output.HasLastMovable) {
        toHex = trace_output.LastMovable;
    }
    else {
        toHex = fromHex;
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Map_GetPathLength(Map* self, mpos fromHex, mpos toHex, int32 cut)
{
    if (!self->GetSize().is_valid_pos(fromHex)) {
        throw ScriptException("Invalid from hex args");
    }
    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid to hex args");
    }

    FindPathInput input;
    input.TargetMap = self;
    input.FromHex = fromHex;
    input.ToHex = toHex;
    input.Cut = cut;

    const auto output = self->GetEngine()->MapMngr.FindPath(input);

    if (output.Result != FindPathOutput::ResultType::Ok) {
        return 0;
    }

    return numeric_cast<int32>(output.Steps.size());
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Map_GetPathLength(Map* self, Critter* cr, mpos toHex, int32 cut)
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (!self->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid to hex args");
    }

    FindPathInput input;
    input.TargetMap = self;
    input.FromCritter = cr;
    input.FromHex = cr->GetHex();
    input.ToHex = toHex;
    input.Multihex = cr->GetMultihex();
    input.Cut = cut;

    const auto output = self->GetEngine()->MapMngr.FindPath(input);

    if (output.Result != FindPathOutput::ResultType::Ok) {
        return 0;
    }

    return numeric_cast<int32>(output.Steps.size());
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_AddCritter(Map* self, hstring protoId, mpos hex, uint8 dir)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return self->GetEngine()->CrMngr.CreateCritterOnMap(protoId, nullptr, self, hex, dir);
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_AddCritter(Map* self, hstring protoId, mpos hex, uint8 dir, const map<CritterProperty, int32>& props)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoCritter(protoId);
    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsIntProps(static_cast<int32>(key), value);
    }

    return self->GetEngine()->CrMngr.CreateCritterOnMap(protoId, &props_, self, hex, dir);
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_AddCritter(Map* self, hstring protoId, mpos hex, uint8 dir, const map<CritterProperty, any_t>& props)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoCritter(protoId);
    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32>(key), value);
    }

    return self->GetEngine()->CrMngr.CreateCritterOnMap(protoId, &props_, self, hex, dir);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexMovable(Map* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return self->IsHexMovable(hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexesMovable(Map* self, mpos hex, int32 radius)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return self->IsHexesMovable(hex, radius);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexShootable(Map* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return self->IsHexShootable(hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsOutsideArea(Map* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    return self->IsOutsideArea(hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_CheckPlaceForItem(Map* self, mpos hex, hstring pid)
{
    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoItem(pid);

    return self->IsValidPlaceForItem(hex, proto);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_BlockHex(Map* self, mpos hex, bool full)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    self->SetHexManualBlock(hex, true, full);
    // Todo: notify clients about manual hex block
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_UnblockHex(Map* self, mpos hex)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }

    self->SetHexManualBlock(hex, false, false);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_Regenerate(Map* self)
{
    self->GetEngine()->MapMngr.RegenerateMap(self);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_MoveHexByDir(Map* self, mpos& hex, uint8 dir)
{
    if (dir >= GameSettings::MAP_DIR_COUNT) {
        throw ScriptException("Invalid dir arg");
    }

    if (GeometryHelper::MoveHexByDir(hex, dir, self->GetSize())) {
        return true;
    }
    else {
        return false;
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Map_MoveHexByDir(Map* self, mpos& hex, uint8 dir, int32 steps)
{
    if (dir >= GameSettings::MAP_DIR_COUNT) {
        throw ScriptException("Invalid dir arg");
    }

    int32 result = 0;

    for (int32 i = 0; i < steps; i++) {
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
FO_SCRIPT_API void Server_Map_VerifyTrigger(Map* self, Critter* cr, mpos hex, uint8 dir)
{
    if (!self->GetSize().is_valid_pos(hex)) {
        throw ScriptException("Invalid hex args");
    }
    if (dir >= GameSettings::MAP_DIR_COUNT) {
        throw ScriptException("Invalid dir arg");
    }

    auto from_hex = hex;

    if (GeometryHelper::MoveHexByDir(from_hex, GeometryHelper::ReverseDir(dir), self->GetSize())) {
        self->GetEngine()->VerifyTrigger(self, cr, from_hex, hex, dir);
    }
}

FO_END_NAMESPACE();
