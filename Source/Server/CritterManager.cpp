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

#include "CritterManager.h"
#include "EntityManager.h"
#include "GenericUtils.h"
#include "ItemManager.h"
#include "Log.h"
#include "MapManager.h"
#include "PropertiesSerializator.h"
#include "ProtoManager.h"
#include "Settings.h"
#include "StringUtils.h"

CritterManager::CritterManager(ServerSettings& settings, ProtoManager& proto_mngr, EntityManager& entity_mngr, MapManager& map_mngr, ItemManager& item_mngr, ServerScriptSystem& script_sys, GameTimer& game_time) : _settings {settings}, _geomHelper(_settings), _protoMngr {proto_mngr}, _entityMngr {entity_mngr}, _mapMngr {map_mngr}, _itemMngr {item_mngr}, _scriptSys {script_sys}, _gameTime {game_time}
{
}

void CritterManager::AddItemToCritter(Critter* cr, Item*& item, bool send)
{
    NON_CONST_METHOD_HINT(_dummy);

    RUNTIME_ASSERT(cr);
    RUNTIME_ASSERT(item);

    // Add
    if (item->GetStackable()) {
        auto* item_already = cr->GetItemByPid(item->GetProtoId());
        if (item_already != nullptr) {
            const auto count = item->GetCount();
            _itemMngr.DeleteItem(item);
            item = item_already;
            item->ChangeCount(count);
            return;
        }
    }

    item->SetSortValue(cr->_invItems);
    cr->SetItem(item);

    // Send
    if (send) {
        cr->Send_AddItem(item);
        if (item->GetCritSlot() != 0u) {
            cr->SendAA_MoveItem(item, ACTION_REFRESH, 0);
        }
    }

    // Change item
    _scriptSys.CritterMoveItemEvent(cr, item, -1);
}

void CritterManager::EraseItemFromCritter(Critter* cr, Item* item, bool send)
{
    NON_CONST_METHOD_HINT(_dummy);

    RUNTIME_ASSERT(cr);
    RUNTIME_ASSERT(item);

    const auto it = std::find(cr->_invItems.begin(), cr->_invItems.end(), item);
    RUNTIME_ASSERT(it != cr->_invItems.end());
    cr->_invItems.erase(it);

    item->SetAccessory(ITEM_ACCESSORY_NONE);

    if (send) {
        cr->Send_EraseItem(item);
    }
    if (item->GetCritSlot() != 0u) {
        cr->SendAA_MoveItem(item, ACTION_REFRESH, 0);
    }

    _scriptSys.CritterMoveItemEvent(cr, item, item->GetCritSlot());
}

auto CritterManager::CreateNpc(hash proto_id, Properties* props, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy) -> Npc*
{
    NON_CONST_METHOD_HINT(_dummy);

    RUNTIME_ASSERT(map);
    RUNTIME_ASSERT(proto_id);
    RUNTIME_ASSERT(hx < map->GetWidth());
    RUNTIME_ASSERT(hy < map->GetHeight());

    const auto* proto = _protoMngr.GetProtoCritter(proto_id);
    RUNTIME_ASSERT(proto);

    auto multihex = 0u;
    if (props == nullptr) {
        multihex = proto->GetMultihex();
    }
    else {
        multihex = props->GetValue<uint>(Critter::PropertyMultihex);
    }

    if (!map->IsHexesPassed(hx, hy, multihex)) {
        if (accuracy) {
            return nullptr;
        }

        short hx_ = hx;
        short hy_ = hy;
        short* sx = nullptr;
        short* sy = nullptr;
        _geomHelper.GetHexOffsets((hx & 1) != 0, sx, sy);

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

    auto* npc = new Npc(0, proto, _settings, _scriptSys, _gameTime);
    if (props != nullptr) {
        npc->Props = *props;
    }

    _entityMngr.RegisterEntity(npc);

    npc->SetCond(COND_ALIVE);

    auto* loc = map->GetLocation();
    RUNTIME_ASSERT(loc);

    if (dir >= _settings.MapDirCount) {
        dir = static_cast<uchar>(GenericUtils::Random(0u, _settings.MapDirCount - 1u));
    }
    npc->SetWorldX(loc != nullptr ? loc->GetWorldX() : 0);
    npc->SetWorldY(loc != nullptr ? loc->GetWorldY() : 0);
    npc->SetHomeMapId(map->GetId());
    npc->SetHomeHexX(hx);
    npc->SetHomeHexY(hy);
    npc->SetHomeDir(dir);
    npc->SetHexX(hx);
    npc->SetHexY(hy);

    const auto can = _mapMngr.CanAddCrToMap(npc, map, hx, hy, 0);
    RUNTIME_ASSERT(can);
    _mapMngr.AddCrToMap(npc, map, hx, hy, dir, 0);

    _scriptSys.CritterInitEvent(npc, true);
    npc->SetScript(nullptr, true);

    _mapMngr.ProcessVisibleItems(npc);
    return npc;
}

auto CritterManager::RestoreNpc(uint id, hash proto_id, const DataBase::Document& doc) -> bool
{
    NON_CONST_METHOD_HINT(_dummy);

    const auto* proto = _protoMngr.GetProtoCritter(proto_id);
    if (proto == nullptr) {
        WriteLog("Proto critter '{}' is not loaded.\n", _str().parseHash(proto_id));
        return false;
    }

    auto* npc = new Npc(id, proto, _settings, _scriptSys, _gameTime);
    if (!PropertiesSerializator::LoadFromDbDocument(&npc->Props, doc, _scriptSys)) {
        WriteLog("Fail to restore properties for critter '{}' ({}).\n", _str().parseHash(proto_id), id);
        npc->Release();
        return false;
    }

    _entityMngr.RegisterEntity(npc);
    return true;
}

void CritterManager::DeleteNpc(Critter* cr)
{
    NON_CONST_METHOD_HINT(_dummy);

    RUNTIME_ASSERT(cr->IsNpc());

    // Redundant calls
    if (cr->IsDestroying || cr->IsDestroyed) {
        return;
    }
    cr->IsDestroying = true;

    // Finish event
    _scriptSys.CritterFinishEvent(cr);

    // Tear off from environment
    cr->LockMapTransfers++;
    while (cr->GetMapId() != 0u || cr->GlobalMapGroup != nullptr || cr->RealCountItems() != 0u) {
        // Delete inventory
        DeleteInventory(cr);

        // Delete from maps
        auto* map = _mapMngr.GetMap(cr->GetMapId());
        if (map != nullptr) {
            _mapMngr.EraseCrFromMap(cr, map);
        }
        else if (cr->GlobalMapGroup != nullptr) {
            _mapMngr.EraseCrFromMap(cr, nullptr);
        }
        else if (cr->GetMapId() != 0u) {
            cr->SetMapId(0);
        }
    }
    cr->LockMapTransfers--;

    // Erase from main collection
    _entityMngr.UnregisterEntity(cr);

    // Invalidate for use
    cr->IsDestroyed = true;
    cr->Release();
}

void CritterManager::DeleteInventory(Critter* cr)
{
    NON_CONST_METHOD_HINT(_dummy);

    while (!cr->_invItems.empty()) {
        _itemMngr.DeleteItem(*cr->_invItems.begin());
    }
}

void CritterManager::GetCritters(CritterVec& critters)
{
    NON_CONST_METHOD_HINT(_dummy);

    CritterVec all_critters;
    _entityMngr.GetCritters(all_critters);

    CritterVec find_critters;
    find_critters.reserve(all_critters.size());
    for (auto* cr : all_critters) {
        find_critters.push_back(cr);
    }

    critters = find_critters;
}

void CritterManager::GetNpcs(NpcVec& npcs)
{
    NON_CONST_METHOD_HINT(_dummy);

    CritterVec all_critters;
    _entityMngr.GetCritters(all_critters);

    NpcVec find_npcs;
    find_npcs.reserve(all_critters.size());
    for (auto* cr : all_critters) {
        if (cr->IsNpc()) {
            find_npcs.push_back(dynamic_cast<Npc*>(cr));
        }
    }

    npcs = find_npcs;
}

void CritterManager::GetClients(ClientVec& players, bool on_global_map)
{
    NON_CONST_METHOD_HINT(_dummy);

    CritterVec all_critters;
    _entityMngr.GetCritters(all_critters);

    ClientVec find_players;
    find_players.reserve(all_critters.size());
    for (auto* cr : all_critters) {
        if (cr->IsPlayer() && (!on_global_map || cr->GetMapId() == 0u)) {
            find_players.push_back(dynamic_cast<Client*>(cr));
        }
    }

    players = find_players;
}

void CritterManager::GetGlobalMapCritters(ushort wx, ushort wy, uint radius, int find_type, CritterVec& critters)
{
    NON_CONST_METHOD_HINT(_dummy);

    CritterVec all_critters;
    _entityMngr.GetCritters(all_critters);

    CritterVec find_critters;
    find_critters.reserve(all_critters.size());
    for (auto* cr : all_critters) {
        if (cr->GetMapId() == 0u && GenericUtils::DistSqrt(static_cast<int>(cr->GetWorldX()), static_cast<int>(cr->GetWorldY()), wx, wy) <= radius && cr->CheckFind(find_type)) {
            find_critters.push_back(cr);
        }
    }

    critters = find_critters;
}

auto CritterManager::GetCritter(uint crid) -> Critter*
{
    NON_CONST_METHOD_HINT(_dummy);

    return _entityMngr.GetCritter(crid);
}

auto CritterManager::GetPlayer(uint crid) -> Client*
{
    NON_CONST_METHOD_HINT(_dummy);

    if (!IS_CLIENT_ID(crid)) {
        return nullptr;
    }

    return dynamic_cast<Client*>(_entityMngr.GetEntity(crid, EntityType::Client));
}

auto CritterManager::GetPlayer(const char* name) -> Client*
{
    NON_CONST_METHOD_HINT(_dummy);

    EntityVec entities;
    _entityMngr.GetEntities(EntityType::Client, entities);

    Client* cl = nullptr;
    for (auto* entity : entities) {
        auto* cl_ = dynamic_cast<Client*>(entity);
        if (_str(name).compareIgnoreCaseUtf8(cl_->GetName())) {
            cl = cl_;
            break;
        }
    }
    return cl;
}

auto CritterManager::GetNpc(uint crid) -> Npc*
{
    NON_CONST_METHOD_HINT(_dummy);

    if (IS_CLIENT_ID(crid)) {
        return nullptr;
    }

    return dynamic_cast<Npc*>(_entityMngr.GetEntity(crid, EntityType::Npc));
}

auto CritterManager::GetItemByPidInvPriority(Critter* cr, hash item_pid) -> Item*
{
    NON_CONST_METHOD_HINT(_dummy);

    const auto* proto_item = _protoMngr.GetProtoItem(item_pid);
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

void CritterManager::ProcessTalk(Client* cl, bool force)
{
    NON_CONST_METHOD_HINT(_dummy);

    if (!force && _gameTime.GameTick() < cl->_talkNextTick) {
        return;
    }

    cl->_talkNextTick = _gameTime.GameTick() + PROCESS_TALK_TICK;

    if (cl->Talk.Type == TalkType::None) {
        return;
    }

    // Check time of talk
    if (cl->Talk.TalkTime != 0u && _gameTime.GameTick() - cl->Talk.StartTick > cl->Talk.TalkTime) {
        CloseTalk(cl);
        return;
    }

    // Check npc
    Npc* npc = nullptr;
    if (cl->Talk.Type == TalkType::Npc) {
        npc = GetNpc(cl->Talk.TalkNpc);
        if (npc == nullptr) {
            CloseTalk(cl);
            return;
        }

        if (!npc->IsAlive()) {
            cl->Send_TextMsg(cl, STR_DIALOG_NPC_NOT_LIFE, SAY_NETMSG, TEXTMSG_GAME);
            CloseTalk(cl);
            return;
        }

        // Todo: don't remeber but need check (IsPlaneNoTalk)
    }

    // Check distance
    if (!cl->Talk.IgnoreDistance) {
        uint map_id = 0;
        ushort hx = 0;
        ushort hy = 0;
        uint talk_distance = 0;
        if (cl->Talk.Type == TalkType::Npc) {
            map_id = npc->GetMapId();
            hx = npc->GetHexX();
            hy = npc->GetHexY();
            talk_distance = npc->GetTalkDistance();
            talk_distance = (talk_distance != 0u ? talk_distance : _settings.TalkDistance) + cl->GetMultihex();
        }
        else if (cl->Talk.Type == TalkType::Hex) {
            map_id = cl->Talk.TalkHexMap;
            hx = cl->Talk.TalkHexX;
            hy = cl->Talk.TalkHexY;
            talk_distance = _settings.TalkDistance + cl->GetMultihex();
        }

        if (cl->GetMapId() != map_id || !_geomHelper.CheckDist(cl->GetHexX(), cl->GetHexY(), hx, hy, talk_distance)) {
            cl->Send_TextMsg(cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME);
            CloseTalk(cl);
        }
    }
}

void CritterManager::CloseTalk(Client* cl)
{
    NON_CONST_METHOD_HINT(_dummy);

    if (cl->Talk.Type != TalkType::None) {
        Npc* npc = nullptr;
        if (cl->Talk.Type == TalkType::Npc) {
            cl->Talk.Type = TalkType::None;

            npc = GetNpc(cl->Talk.TalkNpc);
            if (npc != nullptr) {
                if (cl->Talk.Barter) {
                    _scriptSys.CritterBarterEvent(cl, npc, false, npc->GetBarterPlayers());
                }
                _scriptSys.CritterTalkEvent(cl, npc, false, npc->GetTalkedPlayers());
            }
        }

        if (cl->Talk.CurDialog.DlgScriptFunc) {
            cl->Talk.Locked = true;
            cl->Talk.CurDialog.DlgScriptFunc(cl, npc);
            cl->Talk.Locked = false;
        }
    }

    cl->Talk = TalkData();
    cl->Send_Talk();
}

auto CritterManager::PlayersInGame() const -> uint
{
    return _entityMngr.GetEntitiesCount(EntityType::Client);
}

auto CritterManager::NpcInGame() const -> uint
{
    return _entityMngr.GetEntitiesCount(EntityType::Npc);
}

auto CritterManager::CrittersInGame() const -> uint
{
    return PlayersInGame() + NpcInGame();
}
