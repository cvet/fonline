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
    WriteLog("Link critters...");

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
            throw EntitiesLoadException("Map not found for critter", cr->GetMapId(), cr->GetName(), cr->GetHexX(), cr->GetHexY());
        }

        if (!_engine->MapMngr.CanAddCrToMap(cr, map, cr->GetHexX(), cr->GetHexY(), 0)) {
            throw EntitiesLoadException("Error parsing npc to map", cr->GetName(), cr->GetId(), cr->GetMapId(), cr->GetHexX(), cr->GetHexY());
        }

        _engine->MapMngr.AddCrToMap(cr, map, cr->GetHexX(), cr->GetHexY(), cr->GetDir(), 0);

        if (map == nullptr) {
            cr->SetGlobalMapTripId(cr->GetGlobalMapTripId() - 1);
        }
    }

    // Move critters to global groups
    for (auto* cr : critter_groups) {
        if (!_engine->MapMngr.CanAddCrToMap(cr, nullptr, 0, 0, cr->GetGlobalMapLeaderId())) {
            throw EntitiesLoadException("Error parsing npc to global group", cr->GetName(), cr->GetGlobalMapLeaderId());
        }

        _engine->MapMngr.AddCrToMap(cr, nullptr, 0, 0, 0, cr->GetGlobalMapLeaderId());
    }

    WriteLog("Link critters complete");
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
            item->ChangeCount(count);
            return;
        }
    }

    item->EvaluateSortValue(cr->_invItems);
    cr->SetItem(item);

    // Send
    if (send) {
        cr->Send_AddItem(item);
        if (item->GetCritSlot() != 0u) {
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

    const auto it = std::find(cr->_invItems.begin(), cr->_invItems.end(), item);
    RUNTIME_ASSERT(it != cr->_invItems.end());
    cr->_invItems.erase(it);

    item->SetOwnership(ItemOwnership::Nowhere);

    if (send) {
        cr->Send_EraseItem(item);
    }
    if (item->GetCritSlot() != 0u) {
        cr->SendAndBroadcast_MoveItem(item, ACTION_REFRESH, 0);
    }

    _engine->OnCritterMoveItem.Fire(cr, item, item->GetCritSlot());
}

auto CritterManager::CreateNpc(hstring proto_id, const Properties* props, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy) -> Critter*
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

    auto* npc = new Critter(_engine, 0, nullptr, proto);
    if (props != nullptr) {
        npc->SetProperties(*props);
    }

    _engine->EntityMngr.RegisterEntity(npc);

    auto* loc = map->GetLocation();
    RUNTIME_ASSERT(loc);

    if (dir >= _engine->Settings.MapDirCount) {
        dir = static_cast<uchar>(GenericUtils::Random(0u, _engine->Settings.MapDirCount - 1u));
    }
    npc->SetWorldX(loc != nullptr ? loc->GetWorldX() : 0);
    npc->SetWorldY(loc != nullptr ? loc->GetWorldY() : 0);
    npc->SetHomeMapId(map->GetId());
    npc->SetHomeHexX(hx);
    npc->SetHomeHexY(hy);
    npc->SetHomeDir(dir);
    npc->SetHexX(hx);
    npc->SetHexY(hy);

    const auto can = _engine->MapMngr.CanAddCrToMap(npc, map, hx, hy, 0);
    RUNTIME_ASSERT(can);
    _engine->MapMngr.AddCrToMap(npc, map, hx, hy, dir, 0);

    _engine->OnCritterInit.Fire(npc, true);
    ScriptHelpers::CallInitScript(_engine->ScriptSys, npc, npc->GetInitScript(), true);

    _engine->MapMngr.ProcessVisibleItems(npc);
    return npc;
}

void CritterManager::DeleteNpc(Critter* cr)
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

    auto all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> critters;
    critters.reserve(all_critters.size());

    for (auto* cr : all_critters) {
        critters.push_back(cr);
    }

    return critters;
}

auto CritterManager::GetAllNpc() -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    auto all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> npcs;
    npcs.reserve(all_critters.size());

    for (auto* cr : all_critters) {
        if (cr->IsNpc()) {
            npcs.push_back(dynamic_cast<Critter*>(cr));
        }
    }

    return npcs;
}

auto CritterManager::GetPlayerCritters(bool on_global_map_only) -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    auto all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> player_critters;
    player_critters.reserve(all_critters.size());

    for (auto* cr : all_critters) {
        if (cr->IsPlayer() && (!on_global_map_only || cr->GetMapId() == 0u)) {
            player_critters.push_back(dynamic_cast<Critter*>(cr));
        }
    }

    return player_critters;
}

auto CritterManager::GetGlobalMapCritters(ushort wx, ushort wy, uint radius, CritterFindType find_type) -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    const auto all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> critters;
    critters.reserve(all_critters.size());

    for (auto* cr : all_critters) {
        if (cr->GetMapId() == 0u && GenericUtils::DistSqrt(static_cast<int>(cr->GetWorldX()), static_cast<int>(cr->GetWorldY()), wx, wy) <= radius && cr->CheckFind(find_type)) {
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

    return dynamic_cast<Player*>(_engine->EntityMngr.GetEntity(id));
}

auto CritterManager::GetPlayerByName(string_view name) -> Player*
{
    NON_CONST_METHOD_HINT();

    for (auto* entity : _engine->EntityMngr.GetEntities()) {
        if (auto* player = dynamic_cast<Player*>(entity); player != nullptr && _str(name).compareIgnoreCaseUtf8(player->GetName())) {
            return player;
        }
    }

    return nullptr;
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
                if (item->GetCritSlot() == 0u) {
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

    if (cr->_talk.Type == TalkType::None) {
        return;
    }

    // Check time of talk
    if (cr->_talk.TalkTime != 0u && _engine->GameTime.GameTick() - cr->_talk.StartTick > cr->_talk.TalkTime) {
        CloseTalk(cr);
        return;
    }

    // Check npc
    Critter* talker = nullptr;
    if (cr->_talk.Type == TalkType::Critter) {
        talker = GetCritter(cr->_talk.CritterId);
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
    if (!cr->_talk.IgnoreDistance) {
        uint map_id = 0;
        ushort hx = 0;
        ushort hy = 0;
        uint talk_distance = 0;
        if (cr->_talk.Type == TalkType::Critter) {
            map_id = talker->GetMapId();
            hx = talker->GetHexX();
            hy = talker->GetHexY();
            talk_distance = talker->GetTalkDistance();
            talk_distance = (talk_distance != 0u ? talk_distance : _engine->Settings.TalkDistance) + cr->GetMultihex();
        }
        else if (cr->_talk.Type == TalkType::Hex) {
            map_id = cr->_talk.TalkHexMap;
            hx = cr->_talk.TalkHexX;
            hy = cr->_talk.TalkHexY;
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

    if (cr->_talk.Type != TalkType::None) {
        Critter* talker = nullptr;

        if (cr->_talk.Type == TalkType::Critter) {
            cr->_talk.Type = TalkType::None;

            talker = GetCritter(cr->_talk.CritterId);
            if (talker != nullptr) {
                if (cr->_talk.Barter) {
                    talker->OnBarter.Fire(cr, false, talker->GetBarterPlayers());
                    _engine->OnCritterBarter.Fire(talker, cr, false, talker->GetBarterPlayers());
                }
                talker->OnTalk.Fire(cr, false, talker->GetTalkedPlayers());
                _engine->OnCritterTalk.Fire(talker, cr, false, talker->GetTalkedPlayers());
            }
        }

        if (!cr->_talk.CurDialog.DlgScriptFunc.empty()) {
            cr->_talk.Locked = true;
            if (!_engine->ScriptSys->CallFunc<void, Critter*, Critter*>(cr->_talk.CurDialog.DlgScriptFunc, cr, talker)) {
                // Nop
            }
            cr->_talk.Locked = false;
        }
    }

    cr->_talk = TalkData();
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
