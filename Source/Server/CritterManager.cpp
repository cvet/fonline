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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "CritterManager.h"
#include "EntityManager.h"
#include "GenericUtils.h"
#include "ItemManager.h"
#include "Log.h"
#include "MapManager.h"
#include "ProtoManager.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"

CritterManager::CritterManager(FOServer* engine) : _engine {engine}
{
}

void CritterManager::LinkCritters()
{
    WriteLog("Link critters");

    int errors = 0;

    const auto critters = GetAllCritters();
    vector<Critter*> critter_groups;
    critter_groups.reserve(critters.size());

    // Move all critters to local maps and global map leaders
    for (auto* cr : critters) {
        if (cr->GetMapId() == 0u && cr->GetGlobalMapLeaderId() != 0u && cr->GetGlobalMapLeaderId() != cr->GetId()) {
            critter_groups.push_back(cr);
            continue;
        }

        auto* map = _engine->MapMngr.GetMap(cr->GetMapId());
        if (cr->GetMapId() != 0u && map == nullptr) {
            WriteLog("Map {} not found for critter {} at hex {} {}", cr->GetMapId(), cr->GetName(), cr->GetHexX(), cr->GetHexY());
            errors++;
            continue;
        }

        if (!_engine->MapMngr.CanAddCrToMap(cr, map, cr->GetHexX(), cr->GetHexY(), 0)) {
            WriteLog("Error parsing npc {} to map {} at hex {} {}", cr->GetName(), cr->GetMapId(), cr->GetHexX(), cr->GetHexY());
            errors++;
            continue;
        }

        if (map != nullptr) {
            map->AddCritter(cr);
        }
        else {
            cr->GlobalMapGroup = new vector<Critter*>();
            cr->GlobalMapGroup->push_back(cr);
        }
    }

    // Move critters to global groups
    for (auto* cr : critter_groups) {
        if (!_engine->MapMngr.CanAddCrToMap(cr, nullptr, 0, 0, cr->GetGlobalMapLeaderId())) {
            WriteLog("Error parsing npc {} to global group {}", cr->GetName(), cr->GetGlobalMapLeaderId());
            errors++;
            continue;
        }

        _engine->MapMngr.AddCrToMap(cr, nullptr, 0, 0, 0, cr->GetGlobalMapLeaderId());
    }

    if (errors != 0) {
        throw ServerInitException("Link critters failed");
    }
}

void CritterManager::InitAfterLoad()
{
}

void CritterManager::AddItemToCritter(Critter* cr, Item*& item, bool send)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(cr);
    RUNTIME_ASSERT(item);

    // Add
    if (item->GetStackable()) {
        auto* item_already = cr->GetItemByPid(item->GetProtoId());
        if (item_already != nullptr) {
            const auto count = item->GetCount();
            _engine->ItemMngr.DeleteItem(item);
            item = item_already;
            item->SetCount(item->GetCount() + count);
            return;
        }
    }

    item->EvaluateSortValue(cr->_invItems);
    cr->SetItem(item);

    auto item_ids = cr->GetItemIds();
    RUNTIME_ASSERT(std::find(item_ids.begin(), item_ids.end(), item->GetId()) == item_ids.end());
    item_ids.emplace_back(item->GetId());
    cr->SetItemIds(std::move(item_ids));

    // Send
    if (send) {
        cr->Send_AddItem(item);
        if (item->GetCritterSlot() != 0u) {
            cr->SendAndBroadcast_MoveItem(item, ACTION_REFRESH, 0);
        }
    }

    // Change item
    _engine->OnCritterMoveItem.Fire(cr, item, -1);
}

void CritterManager::EraseItemFromCritter(Critter* cr, Item* item, bool send)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(cr);
    RUNTIME_ASSERT(item);

    if (item->GetIsRadio()) {
        _engine->ItemMngr.UnregisterRadio(item);
    }

    const auto it = std::find(cr->_invItems.begin(), cr->_invItems.end(), item);
    RUNTIME_ASSERT(it != cr->_invItems.end());
    cr->_invItems.erase(it);

    item->SetOwnership(ItemOwnership::Nowhere);

    if (send) {
        cr->Send_EraseItem(item);
    }
    if (item->GetCritterSlot() != 0) {
        cr->SendAndBroadcast_MoveItem(item, ACTION_REFRESH, 0);
    }

    _engine->OnCritterMoveItem.Fire(cr, item, item->GetCritterSlot());

    item->SetCritterId(0);
    item->SetCritterSlot(0);

    auto item_ids = cr->GetItemIds();
    const auto item_id_it = std::find(item_ids.begin(), item_ids.end(), item->GetId());
    RUNTIME_ASSERT(item_id_it != item_ids.end());
    item_ids.erase(item_id_it);
    cr->SetItemIds(std::move(item_ids));
}

auto CritterManager::CreateCritter(hstring proto_id, const Properties* props, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy) -> Critter*
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(map);
    RUNTIME_ASSERT(proto_id);
    RUNTIME_ASSERT(hx < map->GetWidth());
    RUNTIME_ASSERT(hy < map->GetHeight());

    const auto* proto = _engine->ProtoMngr.GetProtoCritter(proto_id);
    RUNTIME_ASSERT(proto);

    uint multihex;
    if (props == nullptr) {
        multihex = proto->GetMultihex();
    }
    else {
        multihex = props->GetValue<uint>(props->GetRegistrator()->Find("Multihex"));
    }

    if (!map->IsHexesPassed(hx, hy, multihex)) {
        if (accuracy) {
            return nullptr;
        }

        short hx_ = hx;
        short hy_ = hy;
        const auto [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);

        // Find in 2 hex radius
        auto pos = -1;
        while (true) {
            pos++;
            if (pos >= 18) {
                return nullptr;
            }

            if (hx_ + sx[pos] < 0 || hx_ + sx[pos] >= map->GetWidth()) {
                continue;
            }
            if (hy_ + sy[pos] < 0 || hy_ + sy[pos] >= map->GetHeight()) {
                continue;
            }
            if (!map->IsHexesPassed(hx_ + sx[pos], hy_ + sy[pos], multihex)) {
                continue;
            }
            break;
        }

        hx_ += sx[pos];
        hy_ += sy[pos];
        hx = hx_;
        hy = hy_;
    }

    auto* cr = new Critter(_engine, 0u, nullptr, proto);
    if (props != nullptr) {
        cr->SetProperties(*props);
    }

    _engine->EntityMngr.RegisterEntity(cr);

    const auto* loc = map->GetLocation();
    RUNTIME_ASSERT(loc);

    if (dir >= GameSettings::MAP_DIR_COUNT) {
        dir = static_cast<uchar>(GenericUtils::Random(0u, GameSettings::MAP_DIR_COUNT - 1u));
    }
    cr->SetWorldX(loc != nullptr ? loc->GetWorldX() : 0);
    cr->SetWorldY(loc != nullptr ? loc->GetWorldY() : 0);
    cr->SetHomeMapId(map->GetId());
    cr->SetHomeHexX(hx);
    cr->SetHomeHexY(hy);
    cr->SetHomeDir(dir);
    cr->SetHexX(hx);
    cr->SetHexY(hy);

    const auto can = _engine->MapMngr.CanAddCrToMap(cr, map, hx, hy, 0);
    RUNTIME_ASSERT(can);
    _engine->MapMngr.AddCrToMap(cr, map, hx, hy, dir, 0);

    _engine->OnCritterInit.Fire(cr, true);
    ScriptHelpers::CallInitScript(_engine->ScriptSys, cr, cr->GetInitScript(), true);

    _engine->MapMngr.ProcessVisibleItems(cr);
    return cr;
}

void CritterManager::DeleteCritter(Critter* cr)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(cr->IsNpc());

    // Redundant calls
    if (cr->IsDestroying() || cr->IsDestroyed()) {
        return;
    }

    cr->MarkAsDestroying();

    // Finish event
    _engine->OnCritterFinish.Fire(cr);

    // Tear off from environment
    cr->LockMapTransfers++;
    while (cr->GetMapId() != 0u || cr->GlobalMapGroup != nullptr || cr->RealCountItems() != 0u) {
        // Delete inventory
        DeleteInventory(cr);

        // Delete from maps
        auto* map = _engine->MapMngr.GetMap(cr->GetMapId());
        if (map != nullptr) {
            _engine->MapMngr.EraseCrFromMap(cr, map);
        }
        else if (cr->GlobalMapGroup != nullptr) {
            _engine->MapMngr.EraseCrFromMap(cr, nullptr);
        }
        else if (cr->GetMapId() != 0u) {
            cr->SetMapId(0);
        }
    }
    cr->LockMapTransfers--;

    // Erase from main collection
    _engine->EntityMngr.UnregisterEntity(cr);

    // Invalidate for use
    cr->MarkAsDestroyed();
    cr->Release();
}

void CritterManager::DeleteInventory(Critter* cr)
{
    NON_CONST_METHOD_HINT();

    while (!cr->_invItems.empty()) {
        _engine->ItemMngr.DeleteItem(*cr->_invItems.begin());
    }
}

auto CritterManager::GetAllCritters() -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    const auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> critters;
    critters.reserve(all_critters.size());

    for (auto&& [id, cr] : all_critters) {
        critters.push_back(cr);
    }

    return critters;
}

auto CritterManager::GetAllNpc() -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    const auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> npcs;
    npcs.reserve(all_critters.size());

    for (auto&& [id, cr] : all_critters) {
        if (cr->IsNpc()) {
            npcs.push_back(cr);
        }
    }

    return npcs;
}

auto CritterManager::GetPlayerCritters(bool on_global_map_only) -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    const auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> player_critters;
    player_critters.reserve(all_critters.size());

    for (auto&& [id, cr] : all_critters) {
        if (cr->IsOwnedByPlayer() && (!on_global_map_only || cr->GetMapId() == 0u)) {
            player_critters.push_back(cr);
        }
    }

    return player_critters;
}

auto CritterManager::GetGlobalMapCritters(ushort wx, ushort wy, uint radius, CritterFindType find_type) -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    const auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> critters;
    critters.reserve(all_critters.size());

    for (auto&& [id, cr] : all_critters) {
        if (cr->GetMapId() == 0u && GenericUtils::DistSqrt(cr->GetWorldX(), cr->GetWorldY(), wx, wy) <= radius && cr->CheckFind(find_type)) {
            critters.push_back(cr);
        }
    }

    return critters;
}

auto CritterManager::GetCritter(uint cr_id) -> Critter*
{
    NON_CONST_METHOD_HINT();

    return _engine->EntityMngr.GetCritter(cr_id);
}

auto CritterManager::GetCritter(uint cr_id) const -> const Critter*
{
    return const_cast<CritterManager*>(this)->GetCritter(cr_id);
}

auto CritterManager::GetPlayerById(uint id) -> Player*
{
    NON_CONST_METHOD_HINT();

    return _engine->EntityMngr.GetPlayer(id);
}

auto CritterManager::GetPlayerByName(string_view name) -> Player*
{
    NON_CONST_METHOD_HINT();

    const auto player_id = _engine->MakePlayerId(name);
    return _engine->EntityMngr.GetPlayer(player_id);
}

auto CritterManager::GetItemByPidInvPriority(Critter* cr, hstring item_pid) -> Item*
{
    NON_CONST_METHOD_HINT();

    const auto* proto_item = _engine->ProtoMngr.GetProtoItem(item_pid);
    if (proto_item == nullptr) {
        return nullptr;
    }

    if (proto_item->GetStackable()) {
        for (auto* item : cr->_invItems) {
            if (item->GetProtoId() == item_pid) {
                return item;
            }
        }
    }
    else {
        Item* another_slot = nullptr;
        for (auto* item : cr->_invItems) {
            if (item->GetProtoId() == item_pid) {
                if (item->GetCritterSlot() == 0u) {
                    return item;
                }
                another_slot = item;
            }
        }
        return another_slot;
    }
    return nullptr;
}

void CritterManager::ProcessTalk(Critter* cr, bool force)
{
    NON_CONST_METHOD_HINT();

    if (!force && _engine->GameTime.GameTick() < cr->_talkNextTick) {
        return;
    }

    cr->_talkNextTick = _engine->GameTime.GameTick() + PROCESS_TALK_TICK;

    if (cr->Talk.Type == TalkType::None) {
        return;
    }

    // Check time of talk
    if (cr->Talk.TalkTime != 0u && _engine->GameTime.GameTick() - cr->Talk.StartTick > cr->Talk.TalkTime) {
        CloseTalk(cr);
        return;
    }

    // Check npc
    const Critter* talker = nullptr;
    if (cr->Talk.Type == TalkType::Critter) {
        talker = GetCritter(cr->Talk.CritterId);
        if (talker == nullptr) {
            CloseTalk(cr);
            return;
        }

        if (!talker->IsAlive()) {
            cr->Send_TextMsg(cr, STR_DIALOG_NPC_NOT_LIFE, SAY_NETMSG, TEXTMSG_GAME);
            CloseTalk(cr);
            return;
        }

        // Todo: don't remeber but need check (IsPlaneNoTalk)
    }

    // Check distance
    if (!cr->Talk.IgnoreDistance) {
        uint map_id = 0;
        ushort hx = 0;
        ushort hy = 0;
        uint talk_distance = 0;
        if (cr->Talk.Type == TalkType::Critter) {
            map_id = talker->GetMapId();
            hx = talker->GetHexX();
            hy = talker->GetHexY();
            talk_distance = talker->GetTalkDistance();
            talk_distance = (talk_distance != 0u ? talk_distance : _engine->Settings.TalkDistance) + cr->GetMultihex();
        }
        else if (cr->Talk.Type == TalkType::Hex) {
            map_id = cr->Talk.TalkHexMap;
            hx = cr->Talk.TalkHexX;
            hy = cr->Talk.TalkHexY;
            talk_distance = _engine->Settings.TalkDistance + cr->GetMultihex();
        }

        if (cr->GetMapId() != map_id || !_engine->Geometry.CheckDist(cr->GetHexX(), cr->GetHexY(), hx, hy, talk_distance)) {
            cr->Send_TextMsg(cr, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME);
            CloseTalk(cr);
        }
    }
}

void CritterManager::CloseTalk(Critter* cr)
{
    NON_CONST_METHOD_HINT();

    if (cr->Talk.Type != TalkType::None) {
        Critter* talker = nullptr;

        if (cr->Talk.Type == TalkType::Critter) {
            cr->Talk.Type = TalkType::None;

            talker = GetCritter(cr->Talk.CritterId);
            if (talker != nullptr) {
                if (cr->Talk.Barter) {
                    talker->OnBarter.Fire(cr, false, talker->GetBarterPlayers());
                    _engine->OnCritterBarter.Fire(talker, cr, false, talker->GetBarterPlayers());
                }
                talker->OnTalk.Fire(cr, false, talker->GetTalkedPlayers());
                _engine->OnCritterTalk.Fire(talker, cr, false, talker->GetTalkedPlayers());
            }
        }

        if (cr->Talk.CurDialog.DlgScriptFuncName) {
            cr->Talk.Locked = true;
            string close = "*";
            if (auto func = _engine->ScriptSys->FindFunc<void, Critter*, Critter*, string*>(cr->Talk.CurDialog.DlgScriptFuncName)) {
                func(cr, talker, &close);
            }
            if (auto func = _engine->ScriptSys->FindFunc<uint, Critter*, Critter*, string*>(cr->Talk.CurDialog.DlgScriptFuncName)) {
                func(cr, talker, &close);
            }
            cr->Talk.Locked = false;
        }
    }

    cr->Talk = TalkData();
    cr->Send_Talk();
}

auto CritterManager::PlayersInGame() const -> uint
{
    return static_cast<uint>(_engine->EntityMngr.GetPlayers().size());
}

auto CritterManager::NpcInGame() const -> uint
{
    return static_cast<uint>(_engine->EntityMngr.GetCritters().size());
}

auto CritterManager::CrittersInGame() const -> uint
{
    return PlayersInGame() + NpcInGame();
}
