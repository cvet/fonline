//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GeometryHelper.h"
#include "Server.h"
#include "StringUtils.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Location* Server_Map_GetLocation(Map* self)
{
    return self->GetLocation();
}

///# ...
///# param funcName ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_SetScript(Map* self, string_view funcName)
{
    if (!funcName.empty()) {
        if (!self->SetScript(funcName, true)) {
            throw ScriptException("Script function not found");
        }
    }
    else {
        self->SetScriptId(0);
    }
}

///# ...
///# param hx ...
///# param hy ...
///# param protoId ...
///# param count ...
///# param props ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_AddItem(Map* self, ushort hx, ushort hy, hash protoId, uint count, const map<int, int>& props)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoItem(protoId);
    if (!proto) {
        throw ScriptException("Invalid proto '{}' arg.", _str().parseHash(protoId));
    }
    if (!self->IsPlaceForProtoItem(hx, hy, proto)) {
        throw ScriptException("No place for item");
    }

    if (!props.empty()) {
        Properties props_(self->GetEngine()->GetPropertyRegistrator("Item"));
        props_ = proto->Props;

        for (const auto& [key, value] : props) {
            props_.SetValueAsIntProps(key, value);
        }

        return self->GetEngine()->CreateItemOnHex(self, hx, hy, protoId, count > 0 ? count : 1, &props_, true);
    }

    return self->GetEngine()->CreateItemOnHex(self, hx, hy, protoId, count > 0 ? count : 1, nullptr, true);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItems(Map* self)
{
    return self->GetItemsPid(0);
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItemsOnHex(Map* self, ushort hx, ushort hy)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItemsHex(hx, hy);
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItemsAroundHex(Map* self, ushort hx, ushort hy, uint radius, hash pid)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItemsHexEx(hx, hy, radius, pid);
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItemsByPid(Map* self, hash pid)
{
    return self->GetItemsPid(pid);
}

///# ...
///# param predicate ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItemsByPredicate(Map* self, const std::function<bool(Item*)>& predicate)
{
    auto map_items = self->GetItems();
    vector<Item*> items;
    items.reserve(map_items.size());
    for (auto* item : map_items) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
            items.push_back(item);
        }
    }
    return items;
}

///# ...
///# param hx ...
///# param hy ...
///# param predicate ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItemsOnHexByPredicate(Map* self, ushort hx, ushort hy, const std::function<bool(Item*)>& predicate)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    const auto map_items = self->GetItemsHex(hx, hy);
    vector<Item*> items;
    items.reserve(map_items.size());

    for (auto* item : map_items) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
            items.push_back(item);
        }
    }
    return items;
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param predicate ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetItemsAroundHexByPredicate(Map* self, ushort hx, ushort hy, uint radius, const std::function<bool(Item*)>& predicate)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    const auto map_items = self->GetItemsHexEx(hx, hy, radius, 0);
    vector<Item*> items;
    items.reserve(map_items.size());

    for (auto* item : map_items) {
        if (!item->IsDestroyed && predicate(item) && !item->IsDestroyed) {
            items.push_back(item);
        }
    }
    return items;
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_GetItem(Map* self, uint itemId)
{
    if (itemId == 0u) {
        throw ScriptException("Item id arg is zero");
    }

    return self->GetItem(itemId);
}

///# ...
///# param hx ...
///# param hy ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_GetItemOnHex(Map* self, ushort hx, ushort hy, hash pid)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetItemHex(hx, hy, pid, nullptr);
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_GetCritterOnHex(Map* self, ushort hx, ushort hy)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    auto* cr = self->GetHexCritter(hx, hy, false);
    if (!cr) {
        cr = self->GetHexCritter(hx, hy, true);
    }
    return cr;
}

///# ...
///# param hx ...
///# param hy ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Map_GetStaticItemOnHex(Map* self, ushort hx, ushort hy, hash pid)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetStaticItem(hx, hy, pid);
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetStaticItemsInHex(Map* self, ushort hx, ushort hy)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetStaticItemsHex(hx, hy);
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetStaticItemsAroundHex(Map* self, ushort hx, ushort hy, uint radius, hash pid)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    return self->GetStaticItemsHexEx(hx, hy, radius, pid);
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetStaticItemsByPid(Map* self, hash pid)
{
    return self->GetStaticItemsByPid(pid);
}

///# ...
///# param predicate ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetStaticItemsByPredicate(Map* self, const std::function<bool(Item*)>& predicate)
{
    const auto& map_static_items = self->GetStaticMap()->StaticItemsVec;
    vector<Item*> items;
    items.reserve(map_static_items.size());
    for (auto* item : map_static_items) {
        if (predicate(item)) {
            items.push_back(item);
        }
    }
    return items;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Map_GetStaticItems(Map* self)
{
    return self->GetStaticMap()->StaticItemsVec;
}

///# ...
///# param crid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_GetCritter(Map* self, uint crid)
{
    return self->GetCritter(crid);
}

///# ...
///# param hx ...
///# param hy ...
///# param radius ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCrittersAroundHex(Map* self, ushort hx, ushort hy, uint radius, uchar findType)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    auto critters = self->GetCrittersHex(hx, hy, radius, findType);
    std::sort(critters.begin(), critters.end(), [self, hx, hy](const Critter* cr1, const Critter* cr2) { return self->GetEngine()->GeomHelper.DistGame(hx, hy, cr1->GetHexX(), cr1->GetHexY()) < self->GetEngine()->GeomHelper.DistGame(hx, hy, cr2->GetHexX(), cr2->GetHexY()); });
    return critters;
}

///# ...
///# param pid ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCrittersByPids(Map* self, hash pid, uchar findType)
{
    vector<Critter*> critters;
    if (pid == 0u) {
        auto map_critters = self->GetCritters();
        critters.reserve(map_critters.size());
        for (auto* cr : map_critters) {
            if (cr->CheckFind(findType)) {
                critters.push_back(cr);
            }
        }
    }
    else {
        auto map_npcs = self->GetNpcs();
        critters.reserve(map_npcs.size());
        for (auto* npc : map_npcs) {
            if (npc->GetProtoId() == pid && npc->CheckFind(findType)) {
                critters.push_back(npc);
            }
        }
    }

    return critters;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCrittersInPath(Map* self, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, float angle, uint dist, int findType)
{
    vector<Critter*> critters;
    TraceData trace;
    trace.TraceMap = self;
    trace.BeginHx = fromHx;
    trace.BeginHy = fromHy;
    trace.EndHx = toHx;
    trace.EndHy = toHy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.Critters = &critters;
    trace.FindType = findType;
    self->GetEngine()->MapMngr.TraceBullet(trace);

    return critters;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///# param findType ...
///# param preBlockHx ...
///# param preBlockHy ...
///# param blockHx ...
///# param blockHy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCrittersWithBlockInPath(Map* self, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, float angle, uint dist, int findType, ushort& preBlockHx, ushort& preBlockHy, ushort& blockHx, ushort& blockHy)
{
    vector<Critter*> critters;
    pair<ushort, ushort> block;
    pair<ushort, ushort> pre_block;
    TraceData trace;
    trace.TraceMap = self;
    trace.BeginHx = fromHx;
    trace.BeginHy = fromHy;
    trace.EndHx = toHx;
    trace.EndHy = toHy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.Critters = &critters;
    trace.FindType = findType;
    trace.PreBlock = &pre_block;
    trace.Block = &block;

    self->GetEngine()->MapMngr.TraceBullet(trace);

    preBlockHx = pre_block.first;
    preBlockHy = pre_block.second;
    blockHx = block.first;
    blockHy = block.second;
    return critters;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Map_GetCrittersWhoViewPath(Map* self, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, uchar findType)
{
    vector<Critter*> critters;
    for (auto* cr : self->GetCritters()) {
        if (cr->CheckFind(findType) && std::find(critters.begin(), critters.end(), cr) == critters.end() && GenericUtils::IntersectCircleLine(cr->GetHexX(), cr->GetHexY(), cr->GetLookDistance(), fromHx, fromHy, toHx, toHy)) {
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
[[maybe_unused]] vector<Critter*> Server_Map_GetCrittersSeeing(Map* self, const vector<Critter*>& critters, bool lookOnThem, uchar findType)
{
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
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_GetHexInPath(Map* self, ushort fromHx, ushort fromHy, ushort& toHx, ushort& toHy, float angle, uint dist)
{
    pair<ushort, ushort> pre_block;
    pair<ushort, ushort> block;
    TraceData trace;
    trace.TraceMap = self;
    trace.BeginHx = fromHx;
    trace.BeginHy = fromHy;
    trace.EndHx = toHx;
    trace.EndHy = toHy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.PreBlock = &pre_block;
    trace.Block = &block;

    self->GetEngine()->MapMngr.TraceBullet(trace);

    toHx = pre_block.first;
    toHy = pre_block.second;
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param angle ...
///# param dist ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_GetWallHexInPath(Map* self, ushort fromHx, ushort fromHy, ushort& toHx, ushort& toHy, float angle, uint dist)
{
    pair<ushort, ushort> last_passed;
    TraceData trace;
    trace.TraceMap = self;
    trace.BeginHx = fromHx;
    trace.BeginHy = fromHy;
    trace.EndHx = toHx;
    trace.EndHy = toHy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.LastPassed = &last_passed;

    self->GetEngine()->MapMngr.TraceBullet(trace);

    if (trace.IsHaveLastPassed) {
        toHx = last_passed.first;
        toHy = last_passed.second;
    }
    else {
        toHx = fromHx;
        toHy = fromHy;
    }
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param cut ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Map_GetPathLengthToHex(Map* self, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, uint cut)
{
    if (fromHx >= self->GetWidth() || fromHy >= self->GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= self->GetWidth() || toHy >= self->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    FindPathInput input;
    input.MapId = self->GetId();
    input.FromX = fromHx;
    input.FromY = fromHy;
    input.ToX = toHx;
    input.ToY = toHy;
    input.Cut = cut;

    const auto output = self->GetEngine()->MapMngr.FindPath(input);

    if (output.Result != FindPathResult::Ok) {
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
[[maybe_unused]] uint Server_Map_GetPathLengthToCritter(Map* self, Critter* cr, ushort toHx, ushort toHy, uint cut)
{
    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (toHx >= self->GetWidth() || toHy >= self->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    FindPathInput input;
    input.MapId = self->GetId();
    input.FromCritter = cr;
    input.FromX = cr->GetHexX();
    input.FromY = cr->GetHexY();
    input.ToX = toHx;
    input.ToY = toHy;
    input.Multihex = cr->GetMultihex();
    input.Cut = cut;

    const auto output = self->GetEngine()->MapMngr.FindPath(input);

    if (output.Result != FindPathResult::Ok) {
        return 0;
    }

    return static_cast<uint>(output.Steps.size());
}

///# ...
///# param protoId ...
///# param hx ...
///# param hy ...
///# param dir ...
///# param props ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_AddNpc(Map* self, hash protoId, ushort hx, ushort hy, uchar dir, const map<int, int>& props)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    const auto* proto = self->GetEngine()->ProtoMngr.GetProtoCritter(protoId);
    if (!proto) {
        throw ScriptException("Proto '{}' not found.", _str().parseHash(protoId));
    }

    Critter* npc;
    if (!props.empty()) {
        Properties props_(self->GetEngine()->GetPropertyRegistrator("Critter"));
        props_ = proto->Props;
        for (const auto& [key, value] : props) {
            props_.SetValueAsIntProps(key, value);
        }

        npc = self->GetEngine()->CrMngr.CreateNpc(protoId, &props_, self, hx, hy, dir, false);
    }
    else {
        npc = self->GetEngine()->CrMngr.CreateNpc(protoId, nullptr, self, hx, hy, dir, false);
    }

    if (npc == nullptr) {
        throw ScriptException("Create npc fail");
    }

    return npc;
}

///# ...
///# param npcRole ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Map_GetNpcCount(Map* self, int npcRole, uchar findType)
{
    return self->GetNpcCount(npcRole, findType);
}

///# ...
///# param npcRole ...
///# param findType ...
///# param skipCount ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Map_GetNpc(Map* self, int npcRole, uchar findType, uint skipCount)
{
    return self->GetNpc(npcRole, findType, skipCount);
}

///# ...
///# param hexX ...
///# param hexY ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Map_IsHexPassed(Map* self, ushort hexX, ushort hexY)
{
    if (hexX >= self->GetWidth() || hexY >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    return self->IsHexPassed(hexX, hexY);
}

///# ...
///# param hexX ...
///# param hexY ...
///# param radius ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Map_IsHexesPassed(Map* self, ushort hexX, ushort hexY, uint radius)
{
    if (hexX >= self->GetWidth() || hexY >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    return self->IsHexesPassed(hexX, hexY, radius);
}

///# ...
///# param hexX ...
///# param hexY ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Map_IsHexRaked(Map* self, ushort hexX, ushort hexY)
{
    if (hexX >= self->GetWidth() || hexY >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    return self->IsHexRaked(hexX, hexY);
}

///# ...
///# param hexX ...
///# param hexY ...
///# param color ...
///# param text ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_SetText(Map* self, ushort hexX, ushort hexY, uint color, string_view text)
{
    if (hexX >= self->GetWidth() || hexY >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }
    self->SetText(hexX, hexY, color, text, false);
}

///# ...
///# param hexX ...
///# param hexY ...
///# param color ...
///# param textMsg ...
///# param strNum ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_SetTextMsg(Map* self, ushort hexX, ushort hexY, uint color, ushort textMsg, uint strNum)
{
    if (hexX >= self->GetWidth() || hexY >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    self->SetTextMsg(hexX, hexY, color, textMsg, strNum);
}

///# ...
///# param hexX ...
///# param hexY ...
///# param color ...
///# param textMsg ...
///# param strNum ...
///# param lexems ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_SetTextMsgLex(Map* self, ushort hexX, ushort hexY, uint color, ushort textMsg, uint strNum, string_view lexems)
{
    if (hexX >= self->GetWidth() || hexY >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    self->SetTextMsgLex(hexX, hexY, color, textMsg, strNum, lexems);
}

///# ...
///# param effPid ...
///# param hx ...
///# param hy ...
///# param radius ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_RunEffect(Map* self, hash effPid, ushort hx, ushort hy, uint radius)
{
    if (effPid == 0u) {
        throw ScriptException("Effect pid invalid arg");
    }
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    self->SendEffect(effPid, hx, hy, static_cast<ushort>(radius));
}

///# ...
///# param effPid ...
///# param fromCr ...
///# param toCr ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_RunFlyEffect(Map* self, hash effPid, Critter* fromCr, Critter* toCr, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy)
{
    if (effPid == 0u) {
        throw ScriptException("Effect pid invalid arg");
    }
    if (fromHx >= self->GetWidth() || fromHy >= self->GetHeight()) {
        throw ScriptException("Invalid from hexes args");
    }
    if (toHx >= self->GetWidth() || toHy >= self->GetHeight()) {
        throw ScriptException("Invalid to hexes args");
    }

    const auto from_crid = (fromCr != nullptr ? fromCr->GetId() : 0u);
    const auto to_crid = (toCr != nullptr ? toCr->GetId() : 0u);
    self->SendFlyEffect(effPid, from_crid, to_crid, fromHx, fromHy, toHx, toHy);
}

///# ...
///# param hx ...
///# param hy ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Map_CheckPlaceForItem(Map* self, ushort hx, ushort hy, hash pid)
{
    const auto* proto_item = self->GetEngine()->ProtoMngr.GetProtoItem(pid);
    if (!proto_item) {
        throw ScriptException("Proto item not found");
    }

    return self->IsPlaceForProtoItem(hx, hy, proto_item);
}

///# ...
///# param hx ...
///# param hy ...
///# param full ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_BlockHex(Map* self, ushort hx, ushort hy, bool full)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    self->SetHexFlag(hx, hy, FH_BLOCK_ITEM);
    if (full) {
        self->SetHexFlag(hx, hy, FH_NRAKE_ITEM);
    }
}

///# ...
///# param hx ...
///# param hy ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_UnblockHex(Map* self, ushort hx, ushort hy)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    self->UnsetHexFlag(hx, hy, FH_BLOCK_ITEM);
    self->UnsetHexFlag(hx, hy, FH_NRAKE_ITEM);
}

///# ...
///# param soundName ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_PlaySound(Map* self, string_view soundName)
{
    for (Critter* cr : self->GetPlayers()) {
        cr->Send_PlaySound(0, soundName);
    }
}

///# ...
///# param soundName ...
///# param hx ...
///# param hy ...
///# param radius ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_PlaySoundInRadius(Map* self, string_view soundName, ushort hx, ushort hy, uint radius)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }

    for (Critter* cr : self->GetPlayers()) {
        if (self->GetEngine()->GeomHelper.CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius == 0 ? cr->LookCacheValue : radius)) {
            cr->Send_PlaySound(0, soundName);
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
///# param hx ...
///# param hy ...
///# param dir ...
///# param steps ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Map_MoveHexByDir(Map* self, ushort& hx, ushort& hy, uchar dir, uint steps)
{
    if (dir >= self->GetEngine()->Settings.MapDirCount) {
        throw ScriptException("Invalid dir arg");
    }
    if (steps == 0u) {
        throw ScriptException("Steps arg is zero");
    }

    auto result = false;
    auto hx_ = hx;
    auto hy_ = hy;
    const auto maxhx = self->GetWidth();
    const auto maxhy = self->GetHeight();

    if (steps > 1) {
        for (uint i = 0; i < steps; i++) {
            result |= self->GetEngine()->GeomHelper.MoveHexByDir(hx_, hy_, dir, maxhx, maxhy);
        }
    }
    else {
        result = self->GetEngine()->GeomHelper.MoveHexByDir(hx_, hy_, dir, maxhx, maxhy);
    }

    hx = hx_;
    hy = hy_;
    return result;
}

///# ...
///# param cr ...
///# param hx ...
///# param hy ...
///# param dir ...
///@ ExportMethod
[[maybe_unused]] void Server_Map_VerifyTrigger(Map* self, Critter* cr, ushort hx, ushort hy, uchar dir)
{
    if (hx >= self->GetWidth() || hy >= self->GetHeight()) {
        throw ScriptException("Invalid hexes args");
    }
    if (dir >= self->GetEngine()->Settings.MapDirCount) {
        throw ScriptException("Invalid dir arg");
    }

    auto from_hx = hx;
    auto from_hy = hy;
    if (self->GetEngine()->GeomHelper.MoveHexByDir(from_hx, from_hy, self->GetEngine()->GeomHelper.ReverseDir(dir), self->GetWidth(), self->GetHeight())) {
        self->GetEngine()->VerifyTrigger(self, cr, from_hx, from_hy, hx, hy, dir);
    }
}