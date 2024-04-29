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

#include "Common.h"

#include "GenericUtils.h"
#include "Server.h"
#include "StringUtils.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_SetupScript(Map* self, InitFunc<Map*> initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_SetupScriptEx(Map* self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Location* Server_Map_GetLocation(Map* self)
{
    return self->GetLocation();
}

///# ...
///# param hex ...
///# param protoId ...
///# param count ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_AddItem(Map* self, mpos hex, hstring protoId, uint count)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoItem(protoId);
    if (proto == nullptr) {
        throw ScriptException("Invalid proto '{}' arg", protoId);
    }
    if (!self->IsPlaceForProtoItem(hex, proto)) {
        throw ScriptException("No place for item");
    }

    if (count == 0) {
        return nullptr;
    }

    return self->GetEngine()->CreateItemOnHex(self, hex, protoId, count, nullptr, false);
}

///# ...
///# param hex ...
///# param protoId ...
///# param count ...
///# param props ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_AddItem(Map* self, mpos hex, hstring protoId, uint count, const map<ItemProperty, int>& props)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoItem(protoId);
    if (proto == nullptr) {
        throw ScriptException("Invalid proto '{}' arg", protoId);
    }
    if (!self->IsPlaceForProtoItem(hex, proto)) {
        throw ScriptException("No place for item");
    }

    if (count == 0) {
        return nullptr;
    }

    if (!props.empty()) {
        Properties props_(self->GetEngine()->GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME));
        props_ = proto->GetProperties();

        for (const auto& [key, value] : props) {
            props_.SetValueAsIntProps(static_cast<int>(key), value);
        }

        return self->GetEngine()->CreateItemOnHex(self, hex, protoId, count, &props_, false);
    }

    return self->GetEngine()->CreateItemOnHex(self, hex, protoId, count, nullptr, false);
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_GetItem(Map* self, ident_t itemId)
{
    if (!itemId) {
        return nullptr;
    }

    return self->GetItem(itemId);
}

///# ...
///# param hex ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_GetItem(Map* self, mpos hex, hstring pid)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItemHex(hex, pid, nullptr);
}

///# ...
///# param hex ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_GetItem(Map* self, mpos hex, ItemComponent component)
{
    const auto& map_items = self->GetItems(hex);

    for (auto* item : map_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return item;
        }
    }

    return nullptr;
}

///# ...
///# param hex ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_GetItem(Map* self, mpos hex, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    const auto& map_items = self->GetItems(hex);

    for (auto* item : map_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item;
        }
    }

    return nullptr;
}

///# ...
///# param hex ...
///# param radius ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_GetItem(Map* self, mpos hex, uint radius, ItemComponent component)
{
    const auto map_items = self->GetItemsInRadius(hex, radius, hstring());

    for (auto* item : map_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return item;
        }
    }

    return nullptr;
}

///# ...
///# param hex ...
///# param radius ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_GetItem(Map* self, mpos hex, uint radius, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    const auto map_items = self->GetItemsInRadius(hex, radius, hstring());

    for (auto* item : map_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            return item;
        }
    }

    return nullptr;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self)
{
    return self->GetItemsByProto(hstring());
}

///# ...
///# param hex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self, mpos hex)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItems(hex);
}

///# ...
///# param hex ...
///# param radius ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self, mpos hex, uint radius)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItemsInRadius(hex, radius, hstring());
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self, mpos hex, uint radius, hstring pid)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItemsInRadius(hex, radius, pid);
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self, hstring pid)
{
    return self->GetItemsByProto(pid);
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self, ItemComponent component)
{
    const auto& map_items = self->GetItems();

    vector<Item*> items;
    items.reserve(map_items.size());

    for (auto* item : map_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    const auto& map_items = self->GetItems();

    vector<Item*> items;
    items.reserve(map_items.size());

    for (auto* item : map_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param hx ...
///# param hy ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self, mpos hex, ItemComponent component)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    vector<Item*> items;
    const auto& map_items = self->GetItems(hex);

    items.reserve(map_items.size());

    for (auto* item : map_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param hx ...
///# param hy ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self, mpos hex, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    vector<Item*> items;
    const auto& map_items = self->GetItems(hex);

    items.reserve(map_items.size());

    for (auto* item : map_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self, mpos hex, uint radius, ItemComponent component)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    vector<Item*> items;
    const auto map_items = self->GetItemsInRadius(hex, radius, hstring());

    items.reserve(map_items.size());

    for (auto* item : map_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self, mpos hex, uint radius, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    vector<Item*> items;
    const auto map_items = self->GetItemsInRadius(hex, radius, hstring());

    items.reserve(map_items.size());

    for (auto* item : map_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# param id ...
///# return ...
///@ ExportMethod
[[maybe_unused]] StaticItem* Server_Map_GetStaticItem(Map* self, ident_t id)
{
    return self->GetStaticItem(id);
}

///# ...
///# param hx ...
///# param hy ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] StaticItem* Server_Map_GetStaticItem(Map* self, mpos hex, hstring pid)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetStaticItem(hex, pid);
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<StaticItem*> Server_Map_GetStaticItems(Map* self, mpos hex)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetStaticItemsHex(hex);
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<StaticItem*> Server_Map_GetStaticItems(Map* self, mpos hex, uint radius, hstring pid)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetStaticItemsHexEx(hex, radius, pid);
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<StaticItem*> Server_Map_GetStaticItems(Map* self, hstring pid)
{
    return self->GetStaticItemsByPid(pid);
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<StaticItem*> Server_Map_GetStaticItems(Map* self, ItemComponent component)
{
    const auto& map_static_items = self->GetStaticMap()->StaticItems;

    vector<StaticItem*> result;
    result.reserve(map_static_items.size());

    for (auto* item : map_static_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.push_back(item);
        }
    }

    return result;
}

///# ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<StaticItem*> Server_Map_GetStaticItems(Map* self, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);
    const auto& map_static_items = self->GetStaticMap()->StaticItems;

    vector<StaticItem*> result;
    result.reserve(map_static_items.size());

    for (auto* item : map_static_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            result.push_back(item);
        }
    }

    return result;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<StaticItem*> Server_Map_GetStaticItems(Map* self)
{
    return self->GetStaticMap()->StaticItems;
}

///# ...
///# param crid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_GetCritter(Map* self, ident_t crid)
{
    return self->GetCritter(crid);
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_GetCritter(Map* self, mpos hex)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    auto* cr = self->GetNonDeadCritter(hex);
    if (cr == nullptr) {
        cr = self->GetDeadCritter(hex);
    }
    return cr;
}

///# ...
///# param component ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_GetCritter(Map* self, CritterComponent component, CritterFindType findType)
{
    const auto& map_critters = self->GetCritters();

    for (auto* cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return cr;
        }
    }

    return nullptr;
}

///# ...
///# param property ...
///# param propertyValue ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_GetCritter(Map* self, CritterProperty property, int propertyValue, CritterFindType findType)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Critter>(self->GetEngine(), property);
    const auto& map_critters = self->GetCritters();

    for (auto* cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetValueAsInt(prop) == propertyValue) {
            return cr;
        }
    }

    return nullptr;
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCritters(Map* self, mpos hex, uint radius, CritterFindType findType)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    auto critters = self->GetCritters(hex, radius, findType);

    std::sort(critters.begin(), critters.end(), [hex](const Critter* cr1, const Critter* cr2) {
        const uint dist1 = GeometryHelper::DistGame(hex, cr1->GetHex());
        const uint dist2 = GeometryHelper::DistGame(hex, cr2->GetHex());

        return dist1 - std::min(dist1, cr1->GetMultihex()) < dist2 - std::min(dist2, cr2->GetMultihex());
    });

    return critters;
}

///# ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCritters(Map* self, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto& map_critters = self->GetCritters();

    critters.reserve(map_critters.size());

    for (auto* cr : map_critters) {
        if (cr->CheckFind(findType)) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///# ...
///# param pid ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCritters(Map* self, hstring pid, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto& map_critters = self->GetCritters();

    critters.reserve(map_critters.size());

    for (auto* cr : map_critters) {
        if ((!pid || cr->GetProtoId() == pid) && cr->CheckFind(findType)) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///# ...
///# param component ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCritters(Map* self, CritterComponent component, CritterFindType findType)
{
    const auto& map_critters = self->GetCritters();
    vector<Critter*> critters;

    critters.reserve(map_critters.size());

    for (auto* cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///# ...
///# param property ...
///# param propertyValue ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCritters(Map* self, CritterProperty property, int propertyValue, CritterFindType findType)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Critter>(self->GetEngine(), property);
    const auto& map_critters = self->GetCritters();
    vector<Critter*> critters;

    critters.reserve(map_critters.size());

    for (auto* cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetValueAsInt(prop) == propertyValue) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param angle ...
///# param dist ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCrittersInPath(Map* self, mpos fromHex, mpos toHex, float angle, uint dist, CritterFindType findType)
{
    vector<Critter*> critters;

    TraceData trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.Critters = &critters;
    trace.FindType = findType;

    self->GetEngine()->MapMngr.TraceBullet(trace);

    return critters;
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param angle ...
///# param dist ...
///# param findType ...
///# param preBlockHex ...
///# param blockHex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCrittersInPath(Map* self, mpos fromHex, mpos toHex, float angle, uint dist, CritterFindType findType, mpos& preBlockHex, mpos& blockHex)
{
    vector<Critter*> critters;
    mpos block;
    mpos pre_block;

    TraceData trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.Critters = &critters;
    trace.FindType = findType;
    trace.PreBlock = &pre_block;
    trace.Block = &block;

    self->GetEngine()->MapMngr.TraceBullet(trace);

    preBlockHex = pre_block;
    blockHex = block;

    return critters;
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCrittersWhoViewPath(Map* self, mpos fromHex, mpos toHex, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto& map_critters = self->GetCritters();

    for (auto* cr : map_critters) {
        const auto hex = cr->GetHex();

        if (cr->CheckFind(findType) && GenericUtils::IntersectCircleLine(hex.x, hex.y, static_cast<int>(cr->GetLookDistance()), fromHex.x, fromHex.y, toHex.x, toHex.y)) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///# ...
///# param critters ...
///# param lookOnThem ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCrittersSeeing(Map* self, const vector<Critter*>& critters, bool lookOnThem, CritterFindType findType)
{
    UNUSED_VARIABLE(self);

    vector<Critter*> result_critters;

    for (auto* cr : critters) {
        for (auto* cr_ : cr->GetCrFromVisCr(findType, !lookOnThem)) {
            if (std::find(result_critters.begin(), result_critters.end(), cr_) == result_critters.end()) {
                result_critters.push_back(cr_);
            }
        }
    }

    return result_critters;
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param angle ...
///# param dist ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_GetHexInPath(Map* self, mpos fromHex, mpos& toHex, float angle, uint dist)
{
    mpos pre_block;
    mpos block;

    TraceData trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.PreBlock = &pre_block;
    trace.Block = &block;

    self->GetEngine()->MapMngr.TraceBullet(trace);

    toHex = pre_block;
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param angle ...
///# param dist ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_GetWallHexInPath(Map* self, mpos fromHex, mpos& toHex, float angle, uint dist)
{
    mpos last_movable;

    TraceData trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.LastMovable = &last_movable;

    self->GetEngine()->MapMngr.TraceBullet(trace);

    if (trace.IsHaveLastMovable) {
        toHex = last_movable;
    }
    else {
        toHex = fromHex;
    }
}

///# ...
///# param fromHex ...
///# param toHex ...
///# param cut ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Map_GetPathLength(Map* self, mpos fromHex, mpos toHex, uint cut)
{
    if (!self->GetSize().IsValidPos(fromHex)) {
        throw ScriptException("Invalid from hexes args");
    }
    if (!self->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid to hexes args");
    }

    FindPathInput input;
    input.MapId = self->GetId();
    input.FromHex = fromHex;
    input.ToHex = toHex;
    input.Cut = cut;

    const auto output = self->GetEngine()->MapMngr.FindPath(input);

    if (output.Result != FindPathOutput::ResultType::Ok) {
        return 0;
    }

    return static_cast<uint>(output.Steps.size());
}

///# ...
///# param cr ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Map_GetPathLength(Map* self, Critter* cr, mpos toHex, uint cut)
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (!self->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid to hexes args");
    }

    FindPathInput input;
    input.MapId = self->GetId();
    input.FromCritter = cr;
    input.FromHex = cr->GetHex();
    input.ToHex = toHex;
    input.Multihex = cr->GetMultihex();
    input.Cut = cut;

    const auto output = self->GetEngine()->MapMngr.FindPath(input);

    if (output.Result != FindPathOutput::ResultType::Ok) {
        return 0;
    }

    return static_cast<uint>(output.Steps.size());
}

///# ...
///# param protoId ...
///# param hx ...
///# param hy ...
///# param dir ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_AddNpc(Map* self, hstring protoId, mpos hex, uint8 dir)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoCritter(protoId);
    if (proto == nullptr) {
        throw ScriptException("Proto '{}' not found.", protoId);
    }

    Critter* npc = self->GetEngine()->CrMngr.CreateCritter(protoId, nullptr, self, hex, dir, false);
    if (npc == nullptr) {
        throw ScriptException("Create npc failed");
    }

    return npc;
}

///# ...
///# param protoId ...
///# param hx ...
///# param hy ...
///# param dir ...
///# param props ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_AddNpc(Map* self, hstring protoId, mpos hex, uint8 dir, const map<CritterProperty, int>& props)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoCritter(protoId);
    if (proto == nullptr) {
        throw ScriptException("Proto '{}' not found.", protoId);
    }

    Properties props_(proto->GetProperties());
    for (const auto& [key, value] : props) {
        props_.SetValueAsIntProps(static_cast<int>(key), value);
    }

    Critter* npc = self->GetEngine()->CrMngr.CreateCritter(protoId, &props_, self, hex, dir, false);
    if (npc == nullptr) {
        throw ScriptException("Create npc failed");
    }

    return npc;
}

///# ...
///# param protoId ...
///# param hx ...
///# param hy ...
///# param dir ...
///# param props ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_AddNpc(Map* self, hstring protoId, mpos hex, uint8 dir, const map<CritterProperty, any_t>& props)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoCritter(protoId);
    if (proto == nullptr) {
        throw ScriptException("Proto '{}' not found.", protoId);
    }

    Properties props_(proto->GetProperties());
    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int>(key), value);
    }

    Critter* npc = self->GetEngine()->CrMngr.CreateCritter(protoId, &props_, self, hex, dir, false);
    if (npc == nullptr) {
        throw ScriptException("Create npc failed");
    }

    return npc;
}

///# ...
///# param hex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Map_IsHexMovable(Map* self, mpos hex)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->IsHexMovable(hex);
}

///# ...
///# param hex ...
///# param radius ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Map_IsHexesMovable(Map* self, mpos hex, uint radius)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->IsHexesMovable(hex, radius);
}

///# ...
///# param hex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Map_IsHexShootable(Map* self, mpos hex)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->IsHexShootable(hex);
}

///# ...
///# param hex ...
///# param color ...
///# param text ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_SetText(Map* self, mpos hex, ucolor color, string_view text)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    self->SetText(hex, color, text, false);
}

///# ...
///# param hex ...
///# param color ...
///# param textMsg ...
///# param strNum ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_SetTextMsg(Map* self, mpos hex, ucolor color, TextPackName textPack, uint strNum)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    self->SetTextMsg(hex, color, textPack, strNum);
}

///# ...
///# param hex ...
///# param color ...
///# param textMsg ...
///# param strNum ...
///# param lexems ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_SetTextMsg(Map* self, mpos hex, ucolor color, TextPackName textPack, uint strNum, string_view lexems)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    self->SetTextMsgLex(hex, color, textPack, strNum, lexems);
}

///# ...
///# param effPid ...
///# param hex ...
///# param radius ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_RunEffect(Map* self, hstring effPid, mpos hex, uint radius)
{
    if (!effPid) {
        throw ScriptException("Effect pid invalid arg");
    }
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    self->SendEffect(effPid, hex, static_cast<uint16>(radius));
}

///# ...
///# param effPid ...
///# param fromCr ...
///# param toCr ...
///# param fromHex ...
///# param toHex ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_RunFlyEffect(Map* self, hstring effPid, Critter* fromCr, Critter* toCr, mpos fromHex, mpos toHex)
{
    if (!effPid) {
        throw ScriptException("Effect pid invalid arg");
    }
    if (!self->GetSize().IsValidPos(fromHex)) {
        throw ScriptException("Invalid from hexes args");
    }
    if (!self->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid to hexes args");
    }

    const auto from_crid = fromCr != nullptr ? fromCr->GetId() : ident_t {};
    const auto to_crid = toCr != nullptr ? toCr->GetId() : ident_t {};

    self->SendFlyEffect(effPid, from_crid, to_crid, fromHex, toHex);
}

///# ...
///# param hx ...
///# param hy ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Map_CheckPlaceForItem(Map* self, mpos hex, hstring pid)
{
    const auto* proto_item = self->GetEngine()->ProtoMngr.GetProtoItem(pid);
    if (proto_item == nullptr) {
        throw ScriptException("Proto item not found");
    }

    return self->IsPlaceForProtoItem(hex, proto_item);
}

///# ...
///# param hx ...
///# param hy ...
///# param full ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_BlockHex(Map* self, mpos hex, bool full)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    self->SetHexManualBlock(hex, true, full);
    // Todo: notify clients about manual hex block
}

///# ...
///# param hex ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_UnblockHex(Map* self, mpos hex)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    self->SetHexManualBlock(hex, false, false);
}

///# ...
///# param soundName ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_PlaySound(Map* self, string_view soundName)
{
    for (Critter* cr : self->GetPlayerCritters()) {
        cr->Send_PlaySound(ident_t {}, soundName);
    }
}

///# ...
///# param soundName ...
///# param hex ...
///# param radius ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_PlaySound(Map* self, string_view soundName, mpos hex, uint radius)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    for (Critter* cr : self->GetPlayerCritters()) {
        if (GeometryHelper::CheckDist(hex, cr->GetHex(), (radius == 0 ? cr->GetLookDistance() : radius) + cr->GetMultihex())) {
            cr->Send_PlaySound(ident_t {}, soundName);
        }
    }
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_Regenerate(Map* self)
{
    self->GetEngine()->MapMngr.RegenerateMap(self);
}

///# ...
///# param hex ...
///# param dir ...
///@ ExportMethod
[[maybe_unused]] uint Server_Map_MoveHexByDir(Map* self, mpos& hex, uint8 dir)
{
    if (dir >= GameSettings::MAP_DIR_COUNT) {
        throw ScriptException("Invalid dir arg");
    }

    if (GeometryHelper::MoveHexByDir(hex, dir, self->GetSize())) {
        return 1;
    }
    else {
        return 0;
    }
}

///# ...
///# param hex ...
///# param dir ...
///# param steps ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Map_MoveHexByDir(Map* self, mpos& hex, uint8 dir, uint steps)
{
    if (dir >= GameSettings::MAP_DIR_COUNT) {
        throw ScriptException("Invalid dir arg");
    }

    uint result = 0;

    for (uint i = 0; i < steps; i++) {
        if (GeometryHelper::MoveHexByDir(hex, dir, self->GetSize())) {
            result++;
        }
        else {
            break;
        }
    }

    return result;
}

///# ...
///# param cr ...
///# param hex ...
///# param dir ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_VerifyTrigger(Map* self, Critter* cr, mpos hex, uint8 dir)
{
    if (!self->GetSize().IsValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }
    if (dir >= GameSettings::MAP_DIR_COUNT) {
        throw ScriptException("Invalid dir arg");
    }

    auto from_hex = hex;

    if (GeometryHelper::MoveHexByDir(from_hex, GeometryHelper::ReverseDir(dir), self->GetSize())) {
        self->GetEngine()->VerifyTrigger(self, cr, from_hex, hex, dir);
    }
}
