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
#include "Script.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"

CritterManager::CritterManager(ServerSettings& sett, ProtoManager& proto_mngr, EntityManager& entity_mngr,
    MapManager& map_mngr, ItemManager& item_mngr) :
    settings {sett},
    geomHelper(settings),
    protoMngr {proto_mngr},
    entityMngr {entity_mngr},
    mapMngr {map_mngr},
    itemMngr {item_mngr}
{
}

void CritterManager::AddItemToCritter(Critter* cr, Item*& item, bool send)
{
    RUNTIME_ASSERT(cr);
    RUNTIME_ASSERT(item);

    // Add
    if (item->GetStackable())
    {
        Item* item_already = cr->GetItemByPid(item->GetProtoId());
        if (item_already)
        {
            uint count = item->GetCount();
            itemMngr.DeleteItem(item);
            item = item_already;
            item->ChangeCount(count);
            return;
        }
    }

    item->SetSortValue(cr->invItems);
    cr->SetItem(item);

    // Send
    if (send)
    {
        cr->Send_AddItem(item);
        if (item->GetCritSlot())
            cr->SendAA_MoveItem(item, ACTION_REFRESH, 0);
    }

    // Change item
    Script::RaiseInternalEvent(ServerFunctions.CritterMoveItem, cr, item, -1);
}

void CritterManager::EraseItemFromCritter(Critter* cr, Item* item, bool send)
{
    RUNTIME_ASSERT(cr);
    RUNTIME_ASSERT(item);

    auto it = std::find(cr->invItems.begin(), cr->invItems.end(), item);
    RUNTIME_ASSERT(it != cr->invItems.end());
    cr->invItems.erase(it);

    item->SetAccessory(ITEM_ACCESSORY_NONE);

    if (send)
        cr->Send_EraseItem(item);
    if (item->GetCritSlot())
        cr->SendAA_MoveItem(item, ACTION_REFRESH, 0);

    Script::RaiseInternalEvent(ServerFunctions.CritterMoveItem, cr, item, item->GetCritSlot());
}

Npc* CritterManager::CreateNpc(
    hash proto_id, Properties* props, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy)
{
    RUNTIME_ASSERT(map);
    RUNTIME_ASSERT(proto_id);
    RUNTIME_ASSERT(hx < map->GetWidth());
    RUNTIME_ASSERT(hy < map->GetHeight());

    ProtoCritter* proto = protoMngr.GetProtoCritter(proto_id);
    RUNTIME_ASSERT(proto);

    uint multihex;
    if (!props)
        multihex = proto->GetMultihex();
    else
        multihex = props->GetPropValue<uint>(Critter::PropertyMultihex);

    if (!map->IsHexesPassed(hx, hy, multihex))
    {
        if (accuracy)
            return nullptr;

        short hx_ = hx;
        short hy_ = hy;
        short *sx, *sy;
        geomHelper.GetHexOffsets(hx & 1, sx, sy);

        // Find in 2 hex radius
        int pos = -1;
        while (true)
        {
            pos++;
            if (pos >= 18)
                return nullptr;

            if (hx_ + sx[pos] < 0 || hx_ + sx[pos] >= map->GetWidth())
                continue;
            if (hy_ + sy[pos] < 0 || hy_ + sy[pos] >= map->GetHeight())
                continue;
            if (!map->IsHexesPassed(hx_ + sx[pos], hy_ + sy[pos], multihex))
                continue;
            break;
        }

        hx_ += sx[pos];
        hy_ += sy[pos];
        hx = hx_;
        hy = hy_;
    }

    Npc* npc = new Npc(0, proto, settings);
    if (props)
        npc->Props = *props;

    entityMngr.RegisterEntity(npc);

    npc->SetCond(COND_LIFE);

    Location* loc = map->GetLocation();
    RUNTIME_ASSERT(loc);

    if (dir >= settings.MapDirCount)
        dir = GenericUtils::Random(0, settings.MapDirCount - 1);
    npc->SetWorldX(loc ? loc->GetWorldX() : 0);
    npc->SetWorldY(loc ? loc->GetWorldY() : 0);
    npc->SetHomeMapId(map->GetId());
    npc->SetHomeHexX(hx);
    npc->SetHomeHexY(hy);
    npc->SetHomeDir(dir);
    npc->SetHexX(hx);
    npc->SetHexY(hy);

    bool can = mapMngr.CanAddCrToMap(npc, map, hx, hy, 0);
    RUNTIME_ASSERT(can);
    mapMngr.AddCrToMap(npc, map, hx, hy, dir, 0);

    Script::RaiseInternalEvent(ServerFunctions.CritterInit, npc, true);
    npc->SetScript(nullptr, true);

    mapMngr.ProcessVisibleCritters(npc);
    mapMngr.ProcessVisibleItems(npc);
    return npc;
}

bool CritterManager::RestoreNpc(uint id, hash proto_id, const DataBase::Document& doc)
{
    ProtoCritter* proto = protoMngr.GetProtoCritter(proto_id);
    if (!proto)
    {
        WriteLog("Proto critter '{}' is not loaded.\n", _str().parseHash(proto_id));
        return false;
    }

    Npc* npc = new Npc(id, proto, settings);
    if (!PropertiesSerializator::LoadFromDbDocument(&npc->Props, doc))
    {
        WriteLog("Fail to restore properties for critter '{}' ({}).\n", _str().parseHash(proto_id), id);
        npc->Release();
        return false;
    }

    entityMngr.RegisterEntity(npc);
    return true;
}

void CritterManager::DeleteNpc(Critter* cr)
{
    RUNTIME_ASSERT(cr->IsNpc());

    // Redundant calls
    if (cr->IsDestroying || cr->IsDestroyed)
        return;
    cr->IsDestroying = true;

    // Finish event
    Script::RaiseInternalEvent(ServerFunctions.CritterFinish, cr);

    // Tear off from environment
    cr->LockMapTransfers++;
    while (cr->GetMapId() || cr->GlobalMapGroup || cr->RealCountItems())
    {
        // Delete inventory
        DeleteInventory(cr);

        // Delete from maps
        Map* map = mapMngr.GetMap(cr->GetMapId());
        if (map)
            mapMngr.EraseCrFromMap(cr, map);
        else if (cr->GlobalMapGroup)
            mapMngr.EraseCrFromMap(cr, nullptr);
        else if (cr->GetMapId())
            cr->SetMapId(0);
    }
    cr->LockMapTransfers--;

    // Erase from main collection
    entityMngr.UnregisterEntity(cr);

    // Invalidate for use
    cr->IsDestroyed = true;
    cr->Release();
}

void CritterManager::DeleteInventory(Critter* cr)
{
    while (!cr->invItems.empty())
        itemMngr.DeleteItem(*cr->invItems.begin());
}

void CritterManager::GetCritters(CritterVec& critters)
{
    CritterVec all_critters;
    entityMngr.GetCritters(all_critters);

    CritterVec find_critters;
    find_critters.reserve(all_critters.size());
    for (auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it)
        find_critters.push_back(*it);

    critters = find_critters;
}

void CritterManager::GetNpcs(NpcVec& npcs)
{
    CritterVec all_critters;
    entityMngr.GetCritters(all_critters);

    NpcVec find_npcs;
    find_npcs.reserve(all_critters.size());
    for (auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it)
    {
        Critter* cr = *it;
        if (cr->IsNpc())
            find_npcs.push_back((Npc*)cr);
    }

    npcs = find_npcs;
}

void CritterManager::GetClients(ClientVec& players, bool on_global_map /* = false */)
{
    CritterVec all_critters;
    entityMngr.GetCritters(all_critters);

    ClientVec find_players;
    find_players.reserve(all_critters.size());
    for (auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it)
    {
        Critter* cr = *it;
        if (cr->IsPlayer() && (!on_global_map || !cr->GetMapId()))
            find_players.push_back((Client*)cr);
    }

    players = find_players;
}

void CritterManager::GetGlobalMapCritters(ushort wx, ushort wy, uint radius, int find_type, CritterVec& critters)
{
    CritterVec all_critters;
    entityMngr.GetCritters(all_critters);

    CritterVec find_critters;
    find_critters.reserve(all_critters.size());
    for (auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it)
    {
        Critter* cr = *it;
        if (!cr->GetMapId() && GenericUtils::DistSqrt((int)cr->GetWorldX(), (int)cr->GetWorldY(), wx, wy) <= radius &&
            cr->CheckFind(find_type))
            find_critters.push_back(cr);
    }

    critters = find_critters;
}

Critter* CritterManager::GetCritter(uint crid)
{
    return entityMngr.GetCritter(crid);
}

Client* CritterManager::GetPlayer(uint crid)
{
    if (!IS_CLIENT_ID(crid))
        return nullptr;

    return (Client*)entityMngr.GetEntity(crid, EntityType::Client);
}

Client* CritterManager::GetPlayer(const char* name)
{
    EntityVec entities;
    entityMngr.GetEntities(EntityType::Client, entities);

    Client* cl = nullptr;
    for (auto it = entities.begin(); it != entities.end(); ++it)
    {
        Client* cl_ = (Client*)*it;
        if (_str(name).compareIgnoreCaseUtf8(cl_->GetName()))
        {
            cl = cl_;
            break;
        }
    }
    return cl;
}

Npc* CritterManager::GetNpc(uint crid)
{
    if (IS_CLIENT_ID(crid))
        return nullptr;

    return (Npc*)entityMngr.GetEntity(crid, EntityType::Npc);
}

Item* CritterManager::GetItemByPidInvPriority(Critter* cr, hash item_pid)
{
    ProtoItem* proto_item = protoMngr.GetProtoItem(item_pid);
    if (!proto_item)
        return nullptr;

    if (proto_item->GetStackable())
    {
        for (auto item : cr->invItems)
            if (item->GetProtoId() == item_pid)
                return item;
    }
    else
    {
        Item* another_slot = nullptr;
        for (auto item : cr->invItems)
        {
            if (item->GetProtoId() == item_pid)
            {
                if (!item->GetCritSlot())
                    return item;
                another_slot = item;
            }
        }
        return another_slot;
    }
    return nullptr;
}

void CritterManager::ProcessTalk(Client* cl, bool force)
{
    if (!force && Timer::GameTick() < cl->talkNextTick)
        return;
    cl->talkNextTick = Timer::GameTick() + PROCESS_TALK_TICK;
    if (cl->Talk.TalkType == TALK_NONE)
        return;

    // Check time of talk
    if (cl->Talk.TalkTime && Timer::GameTick() - cl->Talk.StartTick > cl->Talk.TalkTime)
    {
        CloseTalk(cl);
        return;
    }

    // Check npc
    Npc* npc = nullptr;
    if (cl->Talk.TalkType == TALK_WITH_NPC)
    {
        npc = GetNpc(cl->Talk.TalkNpc);
        if (!npc)
        {
            CloseTalk(cl);
            return;
        }

        if (!npc->IsLife())
        {
            cl->Send_TextMsg(cl, STR_DIALOG_NPC_NOT_LIFE, SAY_NETMSG, TEXTMSG_GAME);
            CloseTalk(cl);
            return;
        }

        // Todo: IsPlaneNoTalk
    }

    // Check distance
    if (!cl->Talk.IgnoreDistance)
    {
        uint map_id = 0;
        ushort hx = 0, hy = 0;
        uint talk_distance = 0;
        if (cl->Talk.TalkType == TALK_WITH_NPC)
        {
            map_id = npc->GetMapId();
            hx = npc->GetHexX();
            hy = npc->GetHexY();
            talk_distance = npc->GetTalkDistance();
            talk_distance = (talk_distance ? talk_distance : settings.TalkDistance) + cl->GetMultihex();
        }
        else if (cl->Talk.TalkType == TALK_WITH_HEX)
        {
            map_id = cl->Talk.TalkHexMap;
            hx = cl->Talk.TalkHexX;
            hy = cl->Talk.TalkHexY;
            talk_distance = settings.TalkDistance + cl->GetMultihex();
        }

        if (cl->GetMapId() != map_id || !geomHelper.CheckDist(cl->GetHexX(), cl->GetHexY(), hx, hy, talk_distance))
        {
            cl->Send_TextMsg(cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME);
            CloseTalk(cl);
        }
    }
}

void CritterManager::CloseTalk(Client* cl)
{
    if (cl->Talk.TalkType != TALK_NONE)
    {
        Npc* npc = nullptr;
        if (cl->Talk.TalkType == TALK_WITH_NPC)
        {
            cl->Talk.TalkType = TALK_NONE;
            npc = GetNpc(cl->Talk.TalkNpc);
            if (npc)
            {
                if (cl->Talk.Barter)
                    Script::RaiseInternalEvent(ServerFunctions.CritterBarter, cl, npc, false, npc->GetBarterPlayers());
                Script::RaiseInternalEvent(ServerFunctions.CritterTalk, cl, npc, false, npc->GetTalkedPlayers());
            }
        }

        if (cl->Talk.CurDialog.DlgScript)
        {
            Script::PrepareContext(cl->Talk.CurDialog.DlgScript, cl->GetName());
            Script::SetArgEntity(cl);
            Script::SetArgEntity(npc);
            Script::SetArgEntity(nullptr);
            cl->Talk.Locked = true;
            Script::RunPrepared();
            cl->Talk.Locked = false;
        }
    }

    cl->Talk = Talking {};
    cl->Send_Talk();
}

uint CritterManager::PlayersInGame()
{
    return entityMngr.GetEntitiesCount(EntityType::Client);
}

uint CritterManager::NpcInGame()
{
    return entityMngr.GetEntitiesCount(EntityType::Npc);
}

uint CritterManager::CrittersInGame()
{
    return PlayersInGame() + NpcInGame();
}
