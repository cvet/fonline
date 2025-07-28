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
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoItem(protoId);

    if (!self->IsPlaceForProtoItem(hex, proto)) {
        throw ScriptException("No place for item");
    }

    if (count <= 0) {
        return nullptr;
    }

    return self->GetEngine()->CreateItemOnHex(self, hex, protoId, count, nullptr);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_AddItem(Map* self, mpos hex, hstring protoId, int32 count, const map<ItemProperty, int32>& props)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoItem(protoId);

    if (!self->IsPlaceForProtoItem(hex, proto)) {
        throw ScriptException("No place for item");
    }

    if (count <= 0) {
        return nullptr;
    }

    if (!props.empty()) {
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
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItemHex(hex, pid, nullptr);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, mpos hex, ItemComponent component)
{
    const auto& map_items = self->GetItems(hex);

    for (auto* item : map_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return item;
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, mpos hex, ItemProperty property, int32 propertyValue)
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

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, mpos hex, int32 radius, ItemComponent component)
{
    const auto map_items = self->GetItemsInRadius(hex, radius, hstring());

    for (auto* item : map_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return item;
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Map_GetItem(Map* self, mpos hex, int32 radius, ItemProperty property, int32 propertyValue)
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

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self)
{
    return self->GetItemsByProto(hstring());
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItems(hex);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, int32 radius)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItemsInRadius(hex, radius, hstring());
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, int32 radius, hstring pid)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItemsInRadius(hex, radius, pid);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, hstring pid)
{
    return self->GetItemsByProto(pid);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, ItemComponent component)
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

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, ItemProperty property, int32 propertyValue)
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

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, ItemComponent component)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    const auto& map_items = self->GetItems(hex);

    vector<Item*> items;
    items.reserve(map_items.size());

    for (auto* item : map_items) {
        if (item->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            items.push_back(item);
        }
    }

    return items;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, ItemProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    const auto& map_items = self->GetItems(hex);

    vector<Item*> items;
    items.reserve(map_items.size());

    for (auto* item : map_items) {
        if (item->GetValueAsInt(prop) == propertyValue) {
            items.push_back(item);
        }
    }

    return items;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, int32 radius, ItemComponent component)
{
    if (!self->GetSize().isValidPos(hex)) {
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

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Map_GetItems(Map* self, mpos hex, int32 radius, ItemProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Item>(self->GetEngine(), property);

    if (!self->GetSize().isValidPos(hex)) {
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

///@ ExportMethod
FO_SCRIPT_API StaticItem* Server_Map_GetStaticItem(Map* self, ident_t id)
{
    return self->GetStaticItem(id);
}

///@ ExportMethod
FO_SCRIPT_API StaticItem* Server_Map_GetStaticItem(Map* self, mpos hex, hstring pid)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetStaticItem(hex, pid);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, mpos hex)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetStaticItemsHex(hex);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, mpos hex, int32 radius, hstring pid)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetStaticItemsHexEx(hex, radius, pid);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, hstring pid)
{
    return self->GetStaticItemsByPid(pid);
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, ItemComponent component)
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

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self, ItemProperty property, int32 propertyValue)
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

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Map_GetStaticItems(Map* self)
{
    return self->GetStaticMap()->StaticItems;
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_GetCritter(Map* self, ident_t crid)
{
    return self->GetCritter(crid);
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_GetCritter(Map* self, mpos hex)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    auto* cr = self->GetCritter(hex, CritterFindType::NonDead);

    if (cr == nullptr) {
        cr = self->GetCritter(hex, CritterFindType::Dead);
    }

    return cr;
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_GetCritter(Map* self, CritterComponent component, CritterFindType findType)
{
    const auto& map_critters = self->GetCritters();

    for (auto* cr : map_critters) {
        if (cr->CheckFind(findType) && cr->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return cr;
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_GetCritter(Map* self, CritterProperty property, int32 propertyValue, CritterFindType findType)
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

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(Map* self, mpos hex, CritterFindType findType)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    vector<Critter*> critters = self->GetCritters(hex, findType);

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
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    vector<Critter*> critters;

    if (radius == 0) {
        critters = self->GetCritters(hex, findType);
    }
    else {
        critters = self->GetCritters(hex, radius, findType);
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
    const auto& map_critters = self->GetCritters();

    critters.reserve(map_critters.size());

    for (auto* cr : map_critters) {
        if (cr->CheckFind(findType)) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(Map* self, hstring pid, CritterFindType findType)
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

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(Map* self, CritterComponent component, CritterFindType findType)
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

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCritters(Map* self, CritterProperty property, int32 propertyValue, CritterFindType findType)
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

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersInPath(Map* self, mpos fromHex, mpos toHex, float32 angle, int32 dist, CritterFindType findType)
{
    vector<Critter*> critters;

    TraceData trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.MaxDist = dist;
    trace.Angle = angle;
    trace.Critters = &critters;
    trace.FindType = findType;

    self->GetEngine()->MapMngr.TraceBullet(trace);

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersInPath(Map* self, mpos fromHex, mpos toHex, float32 angle, int32 dist, CritterFindType findType, mpos& preBlockHex, mpos& blockHex)
{
    vector<Critter*> critters;
    mpos block;
    mpos pre_block;

    TraceData trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.MaxDist = dist;
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

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersWhoSeeHex(Map* self, mpos hex, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto& map_critters = self->GetCritters();

    for (auto* cr : map_critters) {
        if (cr->CheckFind(findType) && GeometryHelper::CheckDist(cr->GetHex(), hex, cr->GetLookDistance())) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersWhoSeeHex(Map* self, mpos hex, int32 radius, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto& map_critters = self->GetCritters();

    for (auto* cr : map_critters) {
        if (cr->CheckFind(findType) && GeometryHelper::CheckDist(cr->GetHex(), hex, cr->GetLookDistance() + radius)) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Map_GetCrittersWhoSeePath(Map* self, mpos fromHex, mpos toHex, CritterFindType findType)
{
    vector<Critter*> critters;
    const auto& map_critters = self->GetCritters();

    for (auto* cr : map_critters) {
        const auto hex = cr->GetHex();

        if (cr->CheckFind(findType) && GenericUtils::IntersectCircleLine(hex.x, hex.y, cr->GetLookDistance(), fromHex.x, fromHex.y, toHex.x, toHex.y)) {
            critters.push_back(cr);
        }
    }

    return critters;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_GetHexInPath(Map* self, mpos fromHex, mpos& toHex, float32 angle, int32 dist)
{
    mpos pre_block;
    mpos block;

    TraceData trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.MaxDist = dist;
    trace.Angle = angle;
    trace.PreBlock = &pre_block;
    trace.Block = &block;

    self->GetEngine()->MapMngr.TraceBullet(trace);

    toHex = pre_block;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_GetWallHexInPath(Map* self, mpos fromHex, mpos& toHex, float32 angle, int32 dist)
{
    mpos last_movable;

    TraceData trace;
    trace.TraceMap = self;
    trace.StartHex = fromHex;
    trace.TargetHex = toHex;
    trace.MaxDist = dist;
    trace.Angle = angle;
    trace.LastMovable = &last_movable;

    self->GetEngine()->MapMngr.TraceBullet(trace);

    if (trace.HasLastMovable) {
        toHex = last_movable;
    }
    else {
        toHex = fromHex;
    }
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Map_GetPathLength(Map* self, mpos fromHex, mpos toHex, int32 cut)
{
    if (!self->GetSize().isValidPos(fromHex)) {
        throw ScriptException("Invalid from hexes args");
    }
    if (!self->GetSize().isValidPos(toHex)) {
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

    return numeric_cast<int32>(output.Steps.size());
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Map_GetPathLength(Map* self, Critter* cr, mpos toHex, int32 cut)
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (!self->GetSize().isValidPos(toHex)) {
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

    return numeric_cast<int32>(output.Steps.size());
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_AddCritter(Map* self, hstring protoId, mpos hex, uint8 dir)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetEngine()->CrMngr.CreateCritterOnMap(protoId, nullptr, self, hex, dir);
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Map_AddCritter(Map* self, hstring protoId, mpos hex, uint8 dir, const map<CritterProperty, int32>& props)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
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
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
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
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->IsHexMovable(hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexesMovable(Map* self, mpos hex, int32 radius)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->IsHexesMovable(hex, radius);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsHexShootable(Map* self, mpos hex)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->IsHexShootable(hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_IsScrollBlock(Map* self, mpos hex)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    return self->IsScrollBlock(hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Map_CheckPlaceForItem(Map* self, mpos hex, hstring pid)
{
    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoItem(pid);

    return self->IsPlaceForProtoItem(hex, proto);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_BlockHex(Map* self, mpos hex, bool full)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
    }

    self->SetHexManualBlock(hex, true, full);
    // Todo: notify clients about manual hex block
}

///@ ExportMethod
FO_SCRIPT_API void Server_Map_UnblockHex(Map* self, mpos hex)
{
    if (!self->GetSize().isValidPos(hex)) {
        throw ScriptException("Invalid hexes args");
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
    if (!self->GetSize().isValidPos(hex)) {
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

FO_END_NAMESPACE();
