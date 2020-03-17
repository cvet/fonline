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

#include "ItemManager.h"
#include "CritterManager.h"
#include "EntityManager.h"
#include "Log.h"
#include "MapManager.h"
#include "PropertiesSerializator.h"
#include "ProtoManager.h"
#include "StringUtils.h"
#include "Testing.h"

ItemManager::ItemManager(ProtoManager& proto_mngr, EntityManager& entity_mngr, MapManager& map_mngr,
    CritterManager& cr_mngr, ServerScriptSystem& script_sys) :
    protoMngr {proto_mngr}, entityMngr {entity_mngr}, mapMngr {map_mngr}, crMngr {cr_mngr}, scriptSys {script_sys}
{
}

Entity* ItemManager::GetItemHolder(Item* item)
{
    switch (item->GetAccessory())
    {
    case ITEM_ACCESSORY_CRITTER:
        return crMngr.GetCritter(item->GetCritId());
    case ITEM_ACCESSORY_HEX:
        return mapMngr.GetMap(item->GetMapId());
    case ITEM_ACCESSORY_CONTAINER:
        return GetItem(item->GetContainerId());
    default:
        break;
    }
    return nullptr;
}

void ItemManager::EraseItemHolder(Item* item, Entity* holder)
{
    switch (item->GetAccessory())
    {
    case ITEM_ACCESSORY_CRITTER: {
        if (holder)
            crMngr.EraseItemFromCritter((Critter*)holder, item, true);
        else if (item->GetIsRadio())
            RadioRegister(item, true);
        item->SetCritId(0);
        item->SetCritSlot(0);
    }
    break;
    case ITEM_ACCESSORY_HEX: {
        if (holder)
            ((Map*)holder)->EraseItem(item->GetId());
        item->SetMapId(0);
    }
    break;
    case ITEM_ACCESSORY_CONTAINER: {
        if (holder)
            EraseItemFromContainer((Item*)holder, item);
        item->SetContainerId(0);
        item->SetContainerStack(0);
    }
    break;
    default:
        break;
    }
    item->SetAccessory(ITEM_ACCESSORY_NONE);
}

void ItemManager::SetItemToContainer(Item* cont, Item* item)
{
    RUNTIME_ASSERT(cont);
    RUNTIME_ASSERT(item);

    if (!cont->childItems)
        cont->childItems = new ItemVec();

    RUNTIME_ASSERT(std::find(cont->childItems->begin(), cont->childItems->end(), item) == cont->childItems->end());

    cont->childItems->push_back(item);
    item->SetAccessory(ITEM_ACCESSORY_CONTAINER);
    item->SetContainerId(cont->Id);
}

void ItemManager::AddItemToContainer(Item* cont, Item*& item, uint stack_id)
{
    RUNTIME_ASSERT(cont);
    RUNTIME_ASSERT(item);

    if (!cont->childItems)
        cont->childItems = new ItemVec();

    if (item->GetStackable())
    {
        Item* item_ = cont->ContGetItemByPid(item->GetProtoId(), stack_id);
        if (item_)
        {
            item_->ChangeCount(item->GetCount());
            DeleteItem(item);
            item = item_;
            return;
        }
    }

    item->SetContainerStack(stack_id);
    item->SetSortValue(*cont->childItems);
    SetItemToContainer(cont, item);
}

void ItemManager::EraseItemFromContainer(Item* cont, Item* item)
{
    RUNTIME_ASSERT(cont);
    RUNTIME_ASSERT(cont->childItems);
    RUNTIME_ASSERT(item);

    auto it = std::find(cont->childItems->begin(), cont->childItems->end(), item);
    RUNTIME_ASSERT(it != cont->childItems->end());
    cont->childItems->erase(it);

    item->SetAccessory(ITEM_ACCESSORY_NONE);
    item->SetContainerId(0);
    item->SetContainerStack(0);

    if (cont->childItems->empty())
        SAFEDEL(cont->childItems);
}

void ItemManager::GetGameItems(ItemVec& items)
{
    entityMngr.GetItems(items);
}

uint ItemManager::GetItemsCount()
{
    return entityMngr.GetEntitiesCount(EntityType::Item);
}

void ItemManager::SetCritterItems(Critter* cr)
{
    ItemVec items;
    entityMngr.GetCritterItems(cr->GetId(), items);

    for (auto it = items.begin(); it != items.end(); ++it)
    {
        Item* item = *it;
        cr->SetItem(item);
        if (item->GetIsRadio())
            RadioRegister(item, true);
    }
}

Item* ItemManager::CreateItem(hash pid, uint count /* = 0 */, Properties* props /* = nullptr */)
{
    ProtoItem* proto = protoMngr.GetProtoItem(pid);
    if (!proto)
    {
        WriteLog("Proto item '{}' not found.\n", _str().parseHash(pid));
        return nullptr;
    }

    Item* item = new Item(0, proto, scriptSys);
    if (props)
        item->Props = *props;

    // Main collection
    entityMngr.RegisterEntity(item);

    // Count
    if (count)
        item->SetCount(count);

    // Radio collection
    if (item->GetIsRadio())
        RadioRegister(item, true);

    // Scripts
    scriptSys.ItemInitEvent(item, true);
    if (!item->IsDestroyed)
        item->SetScript(nullptr, true);

    // Verify destroying
    if (item->IsDestroyed)
    {
        WriteLog("Item destroyed after prototype '{}' initialization.\n", _str().parseHash(pid));
        return nullptr;
    }

    return item;
}

bool ItemManager::RestoreItem(uint id, hash proto_id, const DataBase::Document& doc)
{
    ProtoItem* proto = protoMngr.GetProtoItem(proto_id);
    if (!proto)
    {
        WriteLog("Proto item '{}' is not loaded.\n", _str().parseHash(proto_id));
        return false;
    }

    Item* item = new Item(id, proto, scriptSys);
    if (!PropertiesSerializator::LoadFromDbDocument(&item->Props, doc, scriptSys))
    {
        WriteLog("Fail to restore properties for item '{}' ({}).\n", _str().parseHash(proto_id), id);
        item->Release();
        return false;
    }

    entityMngr.RegisterEntity(item);
    return true;
}

void ItemManager::DeleteItem(Item* item)
{
    // Redundant calls
    if (item->IsDestroying || item->IsDestroyed)
        return;
    item->IsDestroying = true;

    // Finish events
    scriptSys.ItemFinishEvent(item);

    // Tear off from environment
    while (item->GetAccessory() != ITEM_ACCESSORY_NONE || item->ContIsItems())
    {
        // Delete from owner
        EraseItemHolder(item, GetItemHolder(item));

        // Delete child items
        while (item->childItems)
        {
            RUNTIME_ASSERT(!item->childItems->empty());
            DeleteItem(*item->childItems->begin());
        }
    }

    // Erase from statistics
    ChangeItemStatistics(item->GetProtoId(), -(int)item->GetCount());

    // Erase from radio collection
    if (item->GetIsRadio())
        RadioRegister(item, false);

    // Erase from main collection
    entityMngr.UnregisterEntity(item);

    // Invalidate for use
    item->IsDestroyed = true;
    item->Release();
}

Item* ItemManager::SplitItem(Item* item, uint count)
{
    uint item_count = item->GetCount();
    RUNTIME_ASSERT(item->GetStackable());
    RUNTIME_ASSERT(count > 0);
    RUNTIME_ASSERT(count < item_count);

    Item* new_item = CreateItem(item->GetProtoId(), count, &item->Props); // Ignore init script
    if (!new_item)
    {
        WriteLog("Create item '{}' fail, count {}.\n", item->GetName(), count);
        return nullptr;
    }

    new_item->SetAccessory(ITEM_ACCESSORY_NONE);
    new_item->SetCritId(0);
    new_item->SetCritSlot(0);
    new_item->SetMapId(0);
    new_item->SetContainerId(0);
    new_item->SetContainerStack(0);

    item->ChangeCount(-(int)count);

    // Radio collection
    if (new_item->GetIsRadio())
        RadioRegister(new_item, true);

    return new_item;
}

Item* ItemManager::GetItem(uint item_id)
{
    return (Item*)entityMngr.GetEntity(item_id, EntityType::Item);
}

void ItemManager::MoveItem(Item* item, uint count, Critter* to_cr, bool skip_checks)
{
    if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER && item->GetCritId() == to_cr->GetId())
        return;

    Entity* holder = GetItemHolder(item);
    if (!holder)
        return;

    if (!skip_checks && !ItemCheckMove(item, count, holder, to_cr))
        return;

    if (count >= item->GetCount() || !item->GetStackable())
    {
        EraseItemHolder(item, holder);
        crMngr.AddItemToCritter(to_cr, item, true);
    }
    else
    {
        Item* item_ = SplitItem(item, count);
        if (item_)
            crMngr.AddItemToCritter(to_cr, item_, true);
    }
}

void ItemManager::MoveItem(Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy, bool skip_checks)
{
    if (item->GetAccessory() == ITEM_ACCESSORY_HEX && item->GetMapId() == to_map->GetId() && item->GetHexX() == to_hx &&
        item->GetHexY() == to_hy)
        return;

    Entity* holder = GetItemHolder(item);
    if (!holder)
        return;

    if (!skip_checks && !ItemCheckMove(item, count, holder, to_map))
        return;

    if (count >= item->GetCount() || !item->GetStackable())
    {
        EraseItemHolder(item, holder);
        to_map->AddItem(item, to_hx, to_hy);
    }
    else
    {
        Item* item_ = SplitItem(item, count);
        if (item_)
            to_map->AddItem(item_, to_hx, to_hy);
    }
}

void ItemManager::MoveItem(Item* item, uint count, Item* to_cont, uint stack_id, bool skip_checks)
{
    if (item->GetAccessory() == ITEM_ACCESSORY_CONTAINER && item->GetContainerId() == to_cont->GetId() &&
        item->GetContainerStack() == stack_id)
        return;

    Entity* holder = GetItemHolder(item);
    if (!holder)
        return;

    if (!skip_checks && !ItemCheckMove(item, count, holder, to_cont))
        return;

    if (count >= item->GetCount() || !item->GetStackable())
    {
        EraseItemHolder(item, holder);
        AddItemToContainer(to_cont, item, stack_id);
    }
    else
    {
        Item* item_ = SplitItem(item, count);
        if (item_)
            AddItemToContainer(to_cont, item_, stack_id);
    }
}

Item* ItemManager::AddItemContainer(Item* cont, hash pid, uint count, uint stack_id)
{
    RUNTIME_ASSERT(cont);

    Item* item = cont->ContGetItemByPid(pid, stack_id);
    Item* result = nullptr;

    if (item)
    {
        if (item->GetStackable())
        {
            item->ChangeCount(count);
            result = item;
        }
        else
        {
            if (count > MAX_ADDED_NOGROUP_ITEMS)
                count = MAX_ADDED_NOGROUP_ITEMS;
            for (uint i = 0; i < count; ++i)
            {
                item = CreateItem(pid);
                if (!item)
                    continue;
                AddItemToContainer(cont, item, stack_id);
                result = item;
            }
        }
    }
    else
    {
        ProtoItem* proto_item = protoMngr.GetProtoItem(pid);
        if (!proto_item)
            return result;

        if (proto_item->GetStackable())
        {
            item = CreateItem(pid, count);
            if (!item)
                return result;
            AddItemToContainer(cont, item, stack_id);
            result = item;
        }
        else
        {
            if (count > MAX_ADDED_NOGROUP_ITEMS)
                count = MAX_ADDED_NOGROUP_ITEMS;
            for (uint i = 0; i < count; ++i)
            {
                item = CreateItem(pid);
                if (!item)
                    continue;

                AddItemToContainer(cont, item, stack_id);
                result = item;
            }
        }
    }

    return result;
}

Item* ItemManager::AddItemCritter(Critter* cr, hash pid, uint count)
{
    if (!count)
        return nullptr;

    Item* item = cr->GetItemByPid(pid);
    Item* result = nullptr;

    if (item && item->GetStackable())
    {
        item->ChangeCount(count);
        result = item;
    }
    else
    {
        ProtoItem* proto_item = protoMngr.GetProtoItem(pid);
        if (!proto_item)
            return result;

        if (proto_item->GetStackable())
        {
            item = CreateItem(pid, count);
            if (!item)
                return result;
            crMngr.AddItemToCritter(cr, item, true);
            result = item;
        }
        else
        {
            if (count > MAX_ADDED_NOGROUP_ITEMS)
                count = MAX_ADDED_NOGROUP_ITEMS;
            for (uint i = 0; i < count; ++i)
            {
                item = CreateItem(pid);
                if (!item)
                    break;
                crMngr.AddItemToCritter(cr, item, true);
                result = item;
            }
        }
    }

    return result;
}

bool ItemManager::SubItemCritter(Critter* cr, hash pid, uint count, ItemVec* erased_items)
{
    if (!count)
        return true;

    Item* item = crMngr.GetItemByPidInvPriority(cr, pid);
    if (!item)
        return true;

    if (item->GetStackable())
    {
        if (count >= item->GetCount())
        {
            crMngr.EraseItemFromCritter(cr, item, true);
            if (!erased_items)
                DeleteItem(item);
            else
                erased_items->push_back(item);
        }
        else
        {
            if (erased_items)
            {
                Item* item_ = SplitItem(item, count);
                if (item_)
                    erased_items->push_back(item_);
            }
            else
            {
                item->ChangeCount(-(int)count);
            }
        }
    }
    else
    {
        for (uint i = 0; i < count; ++i)
        {
            crMngr.EraseItemFromCritter(cr, item, true);
            if (!erased_items)
                DeleteItem(item);
            else
                erased_items->push_back(item);
            item = crMngr.GetItemByPidInvPriority(cr, pid);
            if (!item)
                return true;
        }
    }

    return true;
}

bool ItemManager::SetItemCritter(Critter* cr, hash pid, uint count)
{
    uint cur_count = cr->CountItemPid(pid);
    if (cur_count > count)
        return SubItemCritter(cr, pid, cur_count - count);
    else if (cur_count < count)
        return AddItemCritter(cr, pid, count - cur_count) != nullptr;
    return true;
}

bool ItemManager::ItemCheckMove(Item* item, uint count, Entity* from, Entity* to)
{
    return scriptSys.ItemCheckMoveEvent(item, count, from, to);
}

bool ItemManager::MoveItemCritters(Critter* from_cr, Critter* to_cr, Item* item, uint count)
{
    RUNTIME_ASSERT(item);
    RUNTIME_ASSERT(count > 0);
    RUNTIME_ASSERT(count <= item->GetCount());

    if (!ItemCheckMove(item, count, from_cr, to_cr))
        return false;

    if (item->GetStackable() && item->GetCount() > count)
    {
        Item* item_ = to_cr->GetItemByPid(item->GetProtoId());
        if (!item_)
        {
            item_ = CreateItem(item->GetProtoId(), count);
            if (!item_)
            {
                WriteLog("Create item '{}' fail.\n", item->GetName());
                return false;
            }

            crMngr.AddItemToCritter(to_cr, item_, true);
        }
        else
        {
            item_->ChangeCount(count);
        }

        item->ChangeCount(-(int)count);
    }
    else
    {
        crMngr.EraseItemFromCritter(from_cr, item, true);
        crMngr.AddItemToCritter(to_cr, item, true);
    }

    return true;
}

bool ItemManager::MoveItemCritterToCont(Critter* from_cr, Item* to_cont, Item* item, uint count, uint stack_id)
{
    RUNTIME_ASSERT(item);
    RUNTIME_ASSERT(count > 0);
    RUNTIME_ASSERT(count <= item->GetCount());

    if (!ItemCheckMove(item, count, from_cr, to_cont))
        return false;

    if (item->GetStackable() && item->GetCount() > count)
    {
        Item* item_ = to_cont->ContGetItemByPid(item->GetProtoId(), stack_id);
        if (!item_)
        {
            item_ = CreateItem(item->GetProtoId(), count);
            if (!item_)
            {
                WriteLog("Create item '{}' fail.\n", item->GetName());
                return false;
            }

            item_->SetContainerStack(stack_id);
            SetItemToContainer(to_cont, item_);
        }
        else
        {
            item_->ChangeCount(count);
        }

        item->ChangeCount(-(int)count);
    }
    else
    {
        crMngr.EraseItemFromCritter(from_cr, item, true);
        AddItemToContainer(to_cont, item, stack_id);
    }

    return true;
}

bool ItemManager::MoveItemCritterFromCont(Item* from_cont, Critter* to_cr, Item* item, uint count)
{
    RUNTIME_ASSERT(item);
    RUNTIME_ASSERT(count > 0);
    RUNTIME_ASSERT(count <= item->GetCount());

    if (!ItemCheckMove(item, count, from_cont, to_cr))
        return false;

    if (item->GetStackable() && item->GetCount() > count)
    {
        Item* item_ = to_cr->GetItemByPid(item->GetProtoId());
        if (!item_)
        {
            item_ = CreateItem(item->GetProtoId(), count);
            if (!item_)
            {
                WriteLog("Create item '{}' fail.\n", item->GetName());
                return false;
            }

            crMngr.AddItemToCritter(to_cr, item_, true);
        }
        else
        {
            item_->ChangeCount(count);
        }

        item->ChangeCount(-(int)count);
    }
    else
    {
        EraseItemFromContainer(from_cont, item);
        crMngr.AddItemToCritter(to_cr, item, true);
    }

    return true;
}

void ItemManager::RadioClear()
{
    radioItems.clear();
}

void ItemManager::RadioRegister(Item* radio, bool add)
{
    auto it = std::find(radioItems.begin(), radioItems.end(), radio);

    if (add)
    {
        if (it == radioItems.end())
            radioItems.push_back(radio);
    }
    else
    {
        if (it != radioItems.end())
            radioItems.erase(it);
    }
}

void ItemManager::RadioSendText(
    Critter* cr, const string& text, bool unsafe_text, ushort text_msg, uint num_str, UShortVec& channels)
{
    ItemVec radios;
    ItemVec items = cr->GetItemsNoLock();
    for (auto it = items.begin(), end = items.end(); it != end; ++it)
    {
        Item* item = *it;
        if (item->GetIsRadio() && item->RadioIsSendActive() &&
            std::find(channels.begin(), channels.end(), item->GetRadioChannel()) == channels.end())
        {
            channels.push_back(item->GetRadioChannel());
            radios.push_back(item);
        }
    }

    for (uint i = 0, j = (uint)radios.size(); i < j; i++)
    {
        RadioSendTextEx(channels[i], radios[i]->GetRadioBroadcastSend(), cr->GetMapId(), cr->GetWorldX(),
            cr->GetWorldY(), text, unsafe_text, text_msg, num_str, nullptr);
    }
}

void ItemManager::RadioSendTextEx(ushort channel, int broadcast_type, uint from_map_id, ushort from_wx, ushort from_wy,
    const string& text, bool unsafe_text, ushort text_msg, uint num_str, const char* lexems)
{
    // Broadcast
    if (broadcast_type != RADIO_BROADCAST_FORCE_ALL && broadcast_type != RADIO_BROADCAST_WORLD &&
        broadcast_type != RADIO_BROADCAST_MAP && broadcast_type != RADIO_BROADCAST_LOCATION &&
        !(broadcast_type >= 101 && broadcast_type <= 200) /*RADIO_BROADCAST_ZONE*/)
        return;
    if ((broadcast_type == RADIO_BROADCAST_MAP || broadcast_type == RADIO_BROADCAST_LOCATION) && !from_map_id)
        return;

    int broadcast = 0;
    uint broadcast_map_id = 0;
    uint broadcast_loc_id = 0;

    // Multiple sending controlling
    // Not thread safe, but this not so important in this case
    static uint msg_count = 0;
    msg_count++;

    // Send
    for (auto it = radioItems.begin(), end = radioItems.end(); it != end; ++it)
    {
        Item* radio = *it;

        if (radio->GetRadioChannel() == channel && radio->RadioIsRecvActive())
        {
            if (broadcast_type != RADIO_BROADCAST_FORCE_ALL &&
                radio->GetRadioBroadcastRecv() != RADIO_BROADCAST_FORCE_ALL)
            {
                if (broadcast_type == RADIO_BROADCAST_WORLD)
                    broadcast = radio->GetRadioBroadcastRecv();
                else if (radio->GetRadioBroadcastRecv() == RADIO_BROADCAST_WORLD)
                    broadcast = broadcast_type;
                else
                    broadcast = MIN(broadcast_type, radio->GetRadioBroadcastRecv());

                if (broadcast == RADIO_BROADCAST_WORLD)
                {
                    broadcast = RADIO_BROADCAST_FORCE_ALL;
                }
                else if (broadcast == RADIO_BROADCAST_MAP || broadcast == RADIO_BROADCAST_LOCATION)
                {
                    if (!broadcast_map_id)
                    {
                        Map* map = mapMngr.GetMap(from_map_id);
                        if (!map)
                            continue;
                        broadcast_map_id = map->GetId();
                        broadcast_loc_id = map->GetLocation()->GetId();
                    }
                }
                else if (!(broadcast >= 101 && broadcast <= 200) /*RADIO_BROADCAST_ZONE*/)
                {
                    continue;
                }
            }
            else
            {
                broadcast = RADIO_BROADCAST_FORCE_ALL;
            }

            if (radio->GetAccessory() == ITEM_ACCESSORY_CRITTER)
            {
                Client* cl = crMngr.GetPlayer(radio->GetCritId());
                if (cl && cl->RadioMessageSended != msg_count)
                {
                    if (broadcast != RADIO_BROADCAST_FORCE_ALL)
                    {
                        if (broadcast == RADIO_BROADCAST_MAP)
                        {
                            if (broadcast_map_id != cl->GetMapId())
                                continue;
                        }
                        else if (broadcast == RADIO_BROADCAST_LOCATION)
                        {
                            Map* map = mapMngr.GetMap(cl->GetMapId());
                            if (!map || broadcast_loc_id != map->GetLocation()->GetId())
                                continue;
                        }
                        else if (broadcast >= 101 && broadcast <= 200) // RADIO_BROADCAST_ZONE
                        {
                            if (!mapMngr.IsIntersectZone(
                                    from_wx, from_wy, 0, cl->GetWorldX(), cl->GetWorldY(), 0, broadcast - 101))
                                continue;
                        }
                        else
                        {
                            continue;
                        }
                    }

                    if (!text.empty())
                        cl->Send_TextEx(radio->GetId(), text, SAY_RADIO, unsafe_text);
                    else if (lexems)
                        cl->Send_TextMsgLex(radio->GetId(), num_str, SAY_RADIO, text_msg, lexems);
                    else
                        cl->Send_TextMsg(radio->GetId(), num_str, SAY_RADIO, text_msg);

                    cl->RadioMessageSended = msg_count;
                }
            }
            else if (radio->GetAccessory() == ITEM_ACCESSORY_HEX)
            {
                if (broadcast == RADIO_BROADCAST_MAP && broadcast_map_id != radio->GetMapId())
                    continue;

                Map* map = mapMngr.GetMap(radio->GetMapId());
                if (map)
                {
                    if (broadcast != RADIO_BROADCAST_FORCE_ALL && broadcast != RADIO_BROADCAST_MAP)
                    {
                        if (broadcast == RADIO_BROADCAST_LOCATION)
                        {
                            Location* loc = map->GetLocation();
                            if (broadcast_loc_id != loc->GetId())
                                continue;
                        }
                        else if (broadcast >= 101 && broadcast <= 200) // RADIO_BROADCAST_ZONE
                        {
                            Location* loc = map->GetLocation();
                            if (!mapMngr.IsIntersectZone(from_wx, from_wy, 0, loc->GetWorldX(), loc->GetWorldY(),
                                    loc->GetRadius(), broadcast - 101))
                                continue;
                        }
                        else
                            continue;
                    }

                    if (!text.empty())
                        map->SetText(radio->GetHexX(), radio->GetHexY(), 0xFFFFFFFE, text, unsafe_text);
                    else if (lexems)
                        map->SetTextMsgLex(radio->GetHexX(), radio->GetHexY(), 0xFFFFFFFE, text_msg, num_str, lexems,
                            (ushort)strlen(lexems));
                    else
                        map->SetTextMsg(radio->GetHexX(), radio->GetHexY(), 0xFFFFFFFE, text_msg, num_str);
                }
            }
        }
    }
}

void ItemManager::ChangeItemStatistics(hash pid, int val)
{
    ProtoItem* proto = protoMngr.GetProtoItem(pid);
    if (proto)
        proto->InstanceCount += (int64)val;
}

int64 ItemManager::GetItemStatistics(hash pid)
{
    ProtoItem* proto = protoMngr.GetProtoItem(pid);
    return proto ? proto->InstanceCount : 0;
}

string ItemManager::GetItemsStatistics()
{
    vector<ProtoItem*> protos;
    auto& proto_items = protoMngr.GetProtoItems();
    protos.reserve(proto_items.size());
    for (auto& kv : proto_items)
        protos.push_back(kv.second);

    std::sort(protos.begin(), protos.end(),
        [](ProtoItem* p1, ProtoItem* p2) { return p1->GetName().compare(p2->GetName()); });

    string result = "Name                                     Count\n";
    for (ProtoItem* proto_item : protos)
        result += _str("{:<40} {:<20}\n", proto_item->GetName(), _str("{}", proto_item->InstanceCount).c_str());
    return result;
}
