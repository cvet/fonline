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
#include "ProtoManager.h"
#include "Server.h"
#include "StringUtils.h"

ItemManager::ItemManager(FOServer* engine) : _engine {engine}
{
}

void ItemManager::LinkItems()
{
    WriteLog("Link items...\n");

    for (auto* item : GetItems()) {
        if (item->IsStatic()) {
            throw EntitiesLoadException("Can't link static item", item->GetName(), item->GetId());
        }

        switch (item->GetOwnership()) {
        case ItemOwnership::CritterInventory: {
            Critter* cr = _engine->CrMngr.GetCritter(item->GetCritId());
            if (cr == nullptr) {
                throw EntitiesLoadException("Item critter not found", item->GetName(), item->GetId(), item->GetCritId());
            }

            cr->SetItem(item);
        } break;
        case ItemOwnership::MapHex: {
            auto* map = _engine->MapMngr.GetMap(item->GetMapId());
            if (map == nullptr) {
                throw EntitiesLoadException("Item map not found", item->GetName(), item->GetId(), item->GetMapId(), item->GetHexX(), item->GetHexY());
            }

            if (item->GetHexX() >= map->GetWidth() || item->GetHexY() >= map->GetHeight()) {
                throw EntitiesLoadException("Item invalid hex position", item->GetName(), item->GetId(), item->GetHexX(), item->GetHexY());
            }

            map->SetItem(item, item->GetHexX(), item->GetHexY());
        } break;
        case ItemOwnership::ItemContainer: {
            auto* cont = GetItem(item->GetContainerId());
            if (cont == nullptr) {
                throw EntitiesLoadException("Item container not found", item->GetName(), item->GetId(), item->GetContainerId());
            }

            SetItemToContainer(cont, item);
        } break;
        default:
            throw EntitiesLoadException("Unknown item accessory id", item->GetName(), item->GetId(), item->GetOwnership());
        }
    }

    WriteLog("Link items complete.\n");
}

void ItemManager::InitAfterLoad()
{
}

auto ItemManager::GetItemHolder(Item* item) -> Entity*
{
    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory:
        return _engine->CrMngr.GetCritter(item->GetCritId());
    case ItemOwnership::MapHex:
        return _engine->MapMngr.GetMap(item->GetMapId());
    case ItemOwnership::ItemContainer:
        return GetItem(item->GetContainerId());
    default:
        break;
    }
    return nullptr;
}

void ItemManager::EraseItemHolder(Item* item, Entity* holder)
{
    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        if (holder != nullptr) {
            _engine->CrMngr.EraseItemFromCritter(dynamic_cast<Critter*>(holder), item, true);
        }
        else if (item->GetIsRadio()) {
            RegisterRadio(item);
        }
        item->SetCritId(0);
        item->SetCritSlot(0);
    } break;
    case ItemOwnership::MapHex: {
        if (holder != nullptr) {
            dynamic_cast<Map*>(holder)->EraseItem(item->GetId());
        }
        item->SetMapId(0);
    } break;
    case ItemOwnership::ItemContainer: {
        if (holder != nullptr) {
            EraseItemFromContainer(dynamic_cast<Item*>(holder), item);
        }
        item->SetContainerId(0);
        item->SetContainerStack(0);
    } break;
    default:
        break;
    }
    item->SetOwnership(ItemOwnership::Nowhere);
}

void ItemManager::SetItemToContainer(Item* cont, Item* item)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(cont);
    RUNTIME_ASSERT(item);

    if (cont->_childItems == nullptr) {
        cont->_childItems = new vector<Item*>();
    }

    RUNTIME_ASSERT(std::find(cont->_childItems->begin(), cont->_childItems->end(), item) == cont->_childItems->end());

    cont->_childItems->push_back(item);
    item->SetOwnership(ItemOwnership::ItemContainer);
    item->SetContainerId(cont->GetId());
}

void ItemManager::AddItemToContainer(Item* cont, Item*& item, uint stack_id)
{
    RUNTIME_ASSERT(cont);
    RUNTIME_ASSERT(item);

    if (cont->_childItems == nullptr) {
        cont->_childItems = new vector<Item*>();
    }

    if (item->GetStackable()) {
        auto* item_ = cont->ContGetItemByPid(item->GetProtoId(), stack_id);
        if (item_ != nullptr) {
            item_->ChangeCount(item->GetCount());
            DeleteItem(item);
            item = item_;
            return;
        }
    }

    item->SetContainerStack(stack_id);
    item->EvaluateSortValue(*cont->_childItems);
    SetItemToContainer(cont, item);
}

void ItemManager::EraseItemFromContainer(Item* cont, Item* item)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(cont);
    RUNTIME_ASSERT(cont->_childItems);
    RUNTIME_ASSERT(item);

    const auto it = std::find(cont->_childItems->begin(), cont->_childItems->end(), item);
    RUNTIME_ASSERT(it != cont->_childItems->end());
    cont->_childItems->erase(it);

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetContainerId(0);
    item->SetContainerStack(0);

    if (cont->_childItems->empty()) {
        delete cont->_childItems;
        cont->_childItems = nullptr;
    }
}

auto ItemManager::GetItems() -> vector<Item*>
{
    NON_CONST_METHOD_HINT();

    return _engine->EntityMngr.GetItems();
}

auto ItemManager::GetItemsCount() const -> uint
{
    return static_cast<uint>(_engine->EntityMngr.GetItems().size());
}

auto ItemManager::CreateItem(hstring pid, uint count, const Properties* props) -> Item*
{
    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    if (proto == nullptr) {
        WriteLog("Proto item '{}' not found.\n", pid);
        return nullptr;
    }

    auto* item = new Item(_engine, 0, proto);
    if (props != nullptr) {
        item->SetProperties(*props);
    }

    // Main collection
    _engine->EntityMngr.RegisterEntity(item);

    // Count
    if (count != 0u) {
        item->SetCount(count);
    }

    // Radio collection
    if (item->GetIsRadio()) {
        RegisterRadio(item);
    }

    // Scripts
    _engine->OnItemInit.Fire(item, true);
    if (!item->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine->ScriptSys, item, item->GetInitScript(), true);
    }

    // Verify destroying
    if (item->IsDestroyed()) {
        WriteLog("Item destroyed after prototype '{}' initialization.\n", pid);
        return nullptr;
    }

    return item;
}

void ItemManager::DeleteItem(Item* item)
{
    // Redundant calls
    if (item->IsDestroying() || item->IsDestroyed()) {
        return;
    }

    item->MarkAsDestroying();

    // Finish events
    _engine->OnItemFinish.Fire(item);

    // Tear off from environment
    while (item->GetOwnership() != ItemOwnership::Nowhere || item->ContIsItems()) {
        // Delete from owner
        EraseItemHolder(item, GetItemHolder(item));

        // Delete child items
        while (item->_childItems != nullptr) {
            RUNTIME_ASSERT(!item->_childItems->empty());
            DeleteItem(*item->_childItems->begin());
        }
    }

    // Erase from statistics
    ChangeItemStatistics(item->GetProtoId(), -static_cast<int>(item->GetCount()));

    // Erase from radio collection
    if (item->GetIsRadio()) {
        UnregisterRadio(item);
    }

    // Erase from main collection
    _engine->EntityMngr.UnregisterEntity(item);

    // Invalidate for use
    item->MarkAsDestroyed();
    item->Release();
}

auto ItemManager::SplitItem(Item* item, uint count) -> Item*
{
    const auto item_count = item->GetCount();
    RUNTIME_ASSERT(item->GetStackable());
    RUNTIME_ASSERT(count > 0);
    RUNTIME_ASSERT(count < item_count);

    auto* new_item = CreateItem(item->GetProtoId(), count, &item->GetProperties()); // Ignore init script
    if (new_item == nullptr) {
        WriteLog("Create item '{}' fail, count {}.\n", item->GetName(), count);
        return nullptr;
    }

    new_item->SetOwnership(ItemOwnership::Nowhere);
    new_item->SetCritId(0);
    new_item->SetCritSlot(0);
    new_item->SetMapId(0);
    new_item->SetContainerId(0);
    new_item->SetContainerStack(0);

    item->ChangeCount(-static_cast<int>(count));

    // Radio collection
    if (new_item->GetIsRadio()) {
        RegisterRadio(new_item);
    }

    return new_item;
}

auto ItemManager::GetItem(uint item_id) -> Item*
{
    return _engine->EntityMngr.GetItem(item_id);
}

auto ItemManager::GetItem(uint item_id) const -> const Item*
{
    return const_cast<ItemManager*>(this)->GetItem(item_id);
}

void ItemManager::MoveItem(Item* item, uint count, Critter* to_cr, bool skip_checks)
{
    if (item->GetOwnership() == ItemOwnership::CritterInventory && item->GetCritId() == to_cr->GetId()) {
        return;
    }

    auto* holder = GetItemHolder(item);
    if (holder == nullptr) {
        return;
    }

    if (!skip_checks && !ItemCheckMove(item, count, holder, to_cr)) {
        return;
    }

    if (count >= item->GetCount() || !item->GetStackable()) {
        EraseItemHolder(item, holder);
        _engine->CrMngr.AddItemToCritter(to_cr, item, true);
    }
    else {
        auto* item_ = SplitItem(item, count);
        if (item_ != nullptr) {
            _engine->CrMngr.AddItemToCritter(to_cr, item_, true);
        }
    }
}

void ItemManager::MoveItem(Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy, bool skip_checks)
{
    if (item->GetOwnership() == ItemOwnership::MapHex && item->GetMapId() == to_map->GetId() && item->GetHexX() == to_hx && item->GetHexY() == to_hy) {
        return;
    }

    auto* holder = GetItemHolder(item);
    if (holder == nullptr) {
        return;
    }

    if (!skip_checks && !ItemCheckMove(item, count, holder, to_map)) {
        return;
    }

    if (count >= item->GetCount() || !item->GetStackable()) {
        EraseItemHolder(item, holder);
        to_map->AddItem(item, to_hx, to_hy);
    }
    else {
        auto* item_ = SplitItem(item, count);
        if (item_ != nullptr) {
            to_map->AddItem(item_, to_hx, to_hy);
        }
    }
}

void ItemManager::MoveItem(Item* item, uint count, Item* to_cont, uint stack_id, bool skip_checks)
{
    if (item->GetOwnership() == ItemOwnership::ItemContainer && item->GetContainerId() == to_cont->GetId() && item->GetContainerStack() == stack_id) {
        return;
    }

    auto* holder = GetItemHolder(item);
    if (holder == nullptr) {
        return;
    }

    if (!skip_checks && !ItemCheckMove(item, count, holder, to_cont)) {
        return;
    }

    if (count >= item->GetCount() || !item->GetStackable()) {
        EraseItemHolder(item, holder);
        AddItemToContainer(to_cont, item, stack_id);
    }
    else {
        auto* item_ = SplitItem(item, count);
        if (item_ != nullptr) {
            AddItemToContainer(to_cont, item_, stack_id);
        }
    }
}

auto ItemManager::AddItemContainer(Item* cont, hstring pid, uint count, uint stack_id) -> Item*
{
    RUNTIME_ASSERT(cont);

    auto* item = cont->ContGetItemByPid(pid, stack_id);
    Item* result = nullptr;

    if (item != nullptr) {
        if (item->GetStackable()) {
            item->ChangeCount(count);
            result = item;
        }
        else {
            if (count > MAX_ADDED_NOGROUP_ITEMS) {
                count = MAX_ADDED_NOGROUP_ITEMS;
            }
            for (uint i = 0; i < count; ++i) {
                item = CreateItem(pid, 0, nullptr);
                if (item == nullptr) {
                    continue;
                }
                AddItemToContainer(cont, item, stack_id);
                result = item;
            }
        }
    }
    else {
        const auto* proto_item = _engine->ProtoMngr.GetProtoItem(pid);
        if (proto_item == nullptr) {
            return result;
        }

        if (proto_item->GetStackable()) {
            item = CreateItem(pid, count, nullptr);
            if (item == nullptr) {
                return result;
            }
            AddItemToContainer(cont, item, stack_id);
            result = item;
        }
        else {
            if (count > MAX_ADDED_NOGROUP_ITEMS) {
                count = MAX_ADDED_NOGROUP_ITEMS;
            }
            for (uint i = 0; i < count; ++i) {
                item = CreateItem(pid, 0, nullptr);
                if (item == nullptr) {
                    continue;
                }

                AddItemToContainer(cont, item, stack_id);
                result = item;
            }
        }
    }

    return result;
}

auto ItemManager::AddItemCritter(Critter* cr, hstring pid, uint count) -> Item*
{
    if (count == 0u) {
        return nullptr;
    }

    auto* item = cr->GetItemByPid(pid);
    Item* result = nullptr;

    if (item != nullptr && item->GetStackable()) {
        item->ChangeCount(count);
        result = item;
    }
    else {
        const auto* proto_item = _engine->ProtoMngr.GetProtoItem(pid);
        if (proto_item == nullptr) {
            return result;
        }

        if (proto_item->GetStackable()) {
            item = CreateItem(pid, count, nullptr);
            if (item == nullptr) {
                return result;
            }
            _engine->CrMngr.AddItemToCritter(cr, item, true);
            result = item;
        }
        else {
            if (count > MAX_ADDED_NOGROUP_ITEMS) {
                count = MAX_ADDED_NOGROUP_ITEMS;
            }
            for (uint i = 0; i < count; ++i) {
                item = CreateItem(pid, 0, nullptr);
                if (item == nullptr) {
                    break;
                }
                _engine->CrMngr.AddItemToCritter(cr, item, true);
                result = item;
            }
        }
    }

    return result;
}

auto ItemManager::SubItemCritter(Critter* cr, hstring pid, uint count, vector<Item*>* erased_items) -> bool
{
    if (count == 0u) {
        return true;
    }

    auto* item = _engine->CrMngr.GetItemByPidInvPriority(cr, pid);
    if (item == nullptr) {
        return true;
    }

    if (item->GetStackable()) {
        if (count >= item->GetCount()) {
            _engine->CrMngr.EraseItemFromCritter(cr, item, true);
            if (erased_items == nullptr) {
                DeleteItem(item);
            }
            else {
                erased_items->push_back(item);
            }
        }
        else {
            if (erased_items != nullptr) {
                auto* item_ = SplitItem(item, count);
                if (item_ != nullptr) {
                    erased_items->push_back(item_);
                }
            }
            else {
                item->ChangeCount(-static_cast<int>(count));
            }
        }
    }
    else {
        for (uint i = 0; i < count; ++i) {
            _engine->CrMngr.EraseItemFromCritter(cr, item, true);
            if (erased_items == nullptr) {
                DeleteItem(item);
            }
            else {
                erased_items->push_back(item);
            }
            item = _engine->CrMngr.GetItemByPidInvPriority(cr, pid);
            if (item == nullptr) {
                return true;
            }
        }
    }

    return true;
}

auto ItemManager::SetItemCritter(Critter* cr, hstring pid, uint count) -> bool
{
    const auto cur_count = cr->CountItemPid(pid);
    if (cur_count > count) {
        return SubItemCritter(cr, pid, cur_count - count, nullptr);
    }
    if (cur_count < count) {
        return AddItemCritter(cr, pid, count - cur_count) != nullptr;
    }
    return true;
}

auto ItemManager::ItemCheckMove(Item* item, uint count, Entity* from, Entity* to) const -> bool
{
    return _engine->OnItemCheckMove.Fire(item, count, from, to);
}

void ItemManager::RegisterRadio(Item* radio)
{
    const auto it = std::find(_radioItems.begin(), _radioItems.end(), radio);
    if (it == _radioItems.end()) {
        radio->AddRef();
        _radioItems.push_back(radio);
    }
}

void ItemManager::UnregisterRadio(Item* radio)
{
    const auto it = std::find(_radioItems.begin(), _radioItems.end(), radio);
    if (it != _radioItems.end()) {
        radio->Release();
        _radioItems.erase(it);
    }
}

void ItemManager::RadioSendText(Critter* cr, string_view text, bool unsafe_text, ushort text_msg, uint num_str, vector<ushort>& channels)
{
    vector<Item*> radios;
    for (auto* item : cr->GetRawItems()) {
        if (item->GetIsRadio() && item->RadioIsSendActive() && std::find(channels.begin(), channels.end(), item->GetRadioChannel()) == channels.end()) {
            channels.push_back(item->GetRadioChannel());
            radios.push_back(item);
        }
    }

    for (uint i = 0, j = static_cast<uint>(radios.size()); i < j; i++) {
        RadioSendTextEx(channels[i], radios[i]->GetRadioBroadcastSend(), cr->GetMapId(), cr->GetWorldX(), cr->GetWorldY(), text, unsafe_text, text_msg, num_str, "");
    }
}

void ItemManager::RadioSendTextEx(ushort channel, uchar broadcast_type, uint from_map_id, ushort from_wx, ushort from_wy, string_view text, bool unsafe_text, ushort text_msg, uint num_str, string_view lexems)
{
    // Broadcast
    if (broadcast_type != RADIO_BROADCAST_FORCE_ALL && broadcast_type != RADIO_BROADCAST_WORLD && broadcast_type != RADIO_BROADCAST_MAP && broadcast_type != RADIO_BROADCAST_LOCATION && !(broadcast_type >= 101 && broadcast_type <= 200) /*RADIO_BROADCAST_ZONE*/) {
        return;
    }
    if ((broadcast_type == RADIO_BROADCAST_MAP || broadcast_type == RADIO_BROADCAST_LOCATION) && from_map_id == 0u) {
        return;
    }

    uchar broadcast;
    uint broadcast_map_id = 0;
    uint broadcast_loc_id = 0;

    // Multiple sending controlling
    // Not thread safe, but this not so important in this case
    static uint msg_count = 0;
    msg_count++;

    // Send
    for (auto* radio : _radioItems) {
        if (!radio->IsDestroyed() && radio->GetRadioChannel() == channel && radio->RadioIsRecvActive()) {
            if (broadcast_type != RADIO_BROADCAST_FORCE_ALL && radio->GetRadioBroadcastRecv() != RADIO_BROADCAST_FORCE_ALL) {
                if (broadcast_type == RADIO_BROADCAST_WORLD) {
                    broadcast = radio->GetRadioBroadcastRecv();
                }
                else if (radio->GetRadioBroadcastRecv() == RADIO_BROADCAST_WORLD) {
                    broadcast = broadcast_type;
                }
                else {
                    broadcast = std::min(broadcast_type, radio->GetRadioBroadcastRecv());
                }

                if (broadcast == RADIO_BROADCAST_WORLD) {
                    broadcast = RADIO_BROADCAST_FORCE_ALL;
                }
                else if (broadcast == RADIO_BROADCAST_MAP || broadcast == RADIO_BROADCAST_LOCATION) {
                    if (broadcast_map_id == 0u) {
                        auto* map = _engine->MapMngr.GetMap(from_map_id);
                        if (map == nullptr) {
                            continue;
                        }
                        broadcast_map_id = map->GetId();
                        broadcast_loc_id = map->GetLocation()->GetId();
                    }
                }
                else if (!(broadcast >= 101 && broadcast <= 200) /*RADIO_BROADCAST_ZONE*/) {
                    continue;
                }
            }
            else {
                broadcast = RADIO_BROADCAST_FORCE_ALL;
            }

            if (radio->GetOwnership() == ItemOwnership::CritterInventory) {
                auto* cr = _engine->CrMngr.GetCritter(radio->GetCritId());
                if (cr != nullptr && cr->RadioMessageSended != msg_count) {
                    if (broadcast != RADIO_BROADCAST_FORCE_ALL) {
                        if (broadcast == RADIO_BROADCAST_MAP) {
                            if (broadcast_map_id != cr->GetMapId()) {
                                continue;
                            }
                        }
                        else if (broadcast == RADIO_BROADCAST_LOCATION) {
                            auto* map = _engine->MapMngr.GetMap(cr->GetMapId());
                            if (map == nullptr || broadcast_loc_id != map->GetLocation()->GetId()) {
                                continue;
                            }
                        }
                        else if (broadcast >= 101 && broadcast <= 200) // RADIO_BROADCAST_ZONE
                        {
                            if (!_engine->MapMngr.IsIntersectZone(from_wx, from_wy, 0, cr->GetWorldX(), cr->GetWorldY(), 0, broadcast - 101)) {
                                continue;
                            }
                        }
                        else {
                            continue;
                        }
                    }

                    if (!text.empty()) {
                        cr->Send_TextEx(radio->GetId(), text, SAY_RADIO, unsafe_text);
                    }
                    else if (!lexems.empty()) {
                        cr->Send_TextMsgLex(radio->GetId(), num_str, SAY_RADIO, text_msg, lexems);
                    }
                    else {
                        cr->Send_TextMsg(radio->GetId(), num_str, SAY_RADIO, text_msg);
                    }

                    cr->RadioMessageSended = msg_count;
                }
            }
            else if (radio->GetOwnership() == ItemOwnership::MapHex) {
                if (broadcast == RADIO_BROADCAST_MAP && broadcast_map_id != radio->GetMapId()) {
                    continue;
                }

                auto* map = _engine->MapMngr.GetMap(radio->GetMapId());
                if (map != nullptr) {
                    if (broadcast != RADIO_BROADCAST_FORCE_ALL && broadcast != RADIO_BROADCAST_MAP) {
                        if (broadcast == RADIO_BROADCAST_LOCATION) {
                            const auto* loc = map->GetLocation();
                            if (broadcast_loc_id != loc->GetId()) {
                                continue;
                            }
                        }
                        else if (broadcast >= 101 && broadcast <= 200) // RADIO_BROADCAST_ZONE
                        {
                            auto* loc = map->GetLocation();
                            if (!_engine->MapMngr.IsIntersectZone(from_wx, from_wy, 0, loc->GetWorldX(), loc->GetWorldY(), loc->GetRadius(), broadcast - 101)) {
                                continue;
                            }
                        }
                        else {
                            continue;
                        }
                    }

                    if (!text.empty()) {
                        map->SetText(radio->GetHexX(), radio->GetHexY(), 0xFFFFFFFE, text, unsafe_text);
                    }
                    else if (!lexems.empty()) {
                        map->SetTextMsgLex(radio->GetHexX(), radio->GetHexY(), 0xFFFFFFFE, text_msg, num_str, lexems);
                    }
                    else {
                        map->SetTextMsg(radio->GetHexX(), radio->GetHexY(), 0xFFFFFFFE, text_msg, num_str);
                    }
                }
            }
        }
    }
}

void ItemManager::ChangeItemStatistics(hstring pid, int val) const
{
    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    if (proto != nullptr) {
        proto->InstanceCount += val;
    }
}

auto ItemManager::GetItemStatistics(hstring pid) const -> int64
{
    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    return proto != nullptr ? proto->InstanceCount : 0;
}

auto ItemManager::GetItemsStatistics() const -> string
{
    vector<const ProtoItem*> protos;
    const auto& proto_items = _engine->ProtoMngr.GetProtoItems();
    protos.reserve(proto_items.size());
    for (const auto& [pid, proto] : proto_items) {
        protos.push_back(proto);
    }

    std::sort(protos.begin(), protos.end(), [](const ProtoItem* p1, const ProtoItem* p2) { return p1->GetName().compare(p2->GetName()); });

    string result = "Name                                     Count\n";
    for (const auto* proto : protos) {
        result += _str("{:<40} {:<20}\n", proto->GetName(), _str("{}", proto->InstanceCount));
    }
    return result;
}
