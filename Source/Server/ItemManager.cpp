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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

ItemManager::ItemManager(FOServer* engine) :
    _engine {engine}
{
    STACK_TRACE_ENTRY();
}

auto ItemManager::GetItemHolder(Item* item) -> Entity*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory:
        return _engine->EntityMngr.GetCritter(item->GetCritterId());
    case ItemOwnership::MapHex:
        return _engine->EntityMngr.GetMap(item->GetMapId());
    case ItemOwnership::ItemContainer:
        return _engine->EntityMngr.GetItem(item->GetContainerId());
    default:
        break;
    }

    return nullptr;
}

void ItemManager::RemoveItemHolder(Item* item, Entity* holder)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(item);
    RUNTIME_ASSERT(holder);

    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        if (auto* cr = dynamic_cast<Critter*>(holder); cr != nullptr) {
            _engine->CrMngr.RemoveItemFromCritter(cr, item, true);
        }
        else {
            throw GenericException("Item owner (critter inventory) not found");
        }
    } break;
    case ItemOwnership::MapHex: {
        if (auto* map = dynamic_cast<Map*>(holder); map != nullptr) {
            map->RemoveItem(item->GetId());
        }
        else {
            throw GenericException("Item owner (map) not found");
        }
    } break;
    case ItemOwnership::ItemContainer: {
        if (auto* cont = dynamic_cast<Item*>(holder); cont != nullptr) {
            cont->RemoveItemFromContainer(item);
        }
        else {
            throw GenericException("Item owner (container) not found");
        }
    } break;
    default:
        break;
    }

    item->SetOwnership(ItemOwnership::Nowhere);
}

auto ItemManager::CreateItem(hstring pid, uint count, const Properties* props) -> Item*
{
    STACK_TRACE_ENTRY();

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto* item = new Item(_engine, ident_t {}, proto, props);
    auto self_destroy_fuse = RefCountHolder(item);

    item->SetIsStatic(false);
    item->SetOwnership(ItemOwnership::Nowhere);

    // Reset ownership properties
    if (props != nullptr) {
        item->SetMapId(ident_t {});
        item->SetHexX(0);
        item->SetHexY(0);
        item->SetCritterId(ident_t {});
        item->SetCritterSlot(CritterItemSlot::Inventory);
        item->SetContainerId(ident_t {});
        item->SetContainerStack(ContainerItemStack::Root);
        item->SetInnerItemIds({});
    }

    _engine->EntityMngr.RegisterEntity(item);

    if (count != 0 && item->GetStackable()) {
        item->SetCount(count);
    }

    if (item->GetIsRadio()) {
        RegisterRadio(item);
    }

    _engine->EntityMngr.CallInit(item, true);

    if (item->IsDestroyed()) {
        throw GenericException("Item destroyed during init", pid);
    }

    RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::Nowhere);

    return item;
}

void ItemManager::DestroyItem(Item* item)
{
    STACK_TRACE_ENTRY();

    // Redundant calls
    if (item->IsDestroying() || item->IsDestroyed()) {
        return;
    }

    item->MarkAsDestroying();

    // Finish events
    _engine->OnItemFinish.Fire(item);

    // Tear off from environment
    for (InfinityLoopDetector detector; item->GetOwnership() != ItemOwnership::Nowhere || item->HasInnerItems() || item->HasInnerEntities(); detector.AddLoop()) {
        if (item->GetOwnership() != ItemOwnership::Nowhere) {
            RemoveItemHolder(item, GetItemHolder(item));
        }

        while (item->HasInnerItems()) {
            DestroyItem(item->GetAllInnerItems().front());
        }

        if (item->HasInnerEntities()) {
            _engine->EntityMngr.DestroyInnerEntities(item);
        }
    }

    // Erase from statistics
    ChangeItemStatistics(item->GetProtoId(), -static_cast<int>(item->GetCount()));

    // Erase from radio collection
    if (item->GetIsRadio()) {
        UnregisterRadio(item);
    }

    // Erase from main collection
    _engine->EntityMngr.UnregisterEntity(item, true);

    // Invalidate for use
    item->MarkAsDestroyed();
    item->Release();
}

auto ItemManager::SplitItem(Item* item, uint count) -> Item*
{
    STACK_TRACE_ENTRY();

    const auto item_count = item->GetCount();
    RUNTIME_ASSERT(item->GetStackable());
    RUNTIME_ASSERT(count > 0);
    RUNTIME_ASSERT(count < item_count);

    auto* new_item = CreateItem(item->GetProtoId(), count, &item->GetProperties());

    item->SetCount(item_count - count);
    RUNTIME_ASSERT(!new_item->IsDestroyed());

    return new_item;
}

void ItemManager::MoveItem(Item* item, uint count, Critter* to_cr)
{
    STACK_TRACE_ENTRY();

    if (item->GetOwnership() == ItemOwnership::CritterInventory && item->GetCritterId() == to_cr->GetId()) {
        return;
    }

    auto* holder = GetItemHolder(item);

    if (holder == nullptr) {
        return;
    }

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);
        _engine->CrMngr.AddItemToCritter(to_cr, item, true);
    }
    else {
        auto* item_ = SplitItem(item, count);
        _engine->CrMngr.AddItemToCritter(to_cr, item_, true);
    }
}

void ItemManager::MoveItem(Item* item, uint count, Map* to_map, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    if (item->GetOwnership() == ItemOwnership::MapHex && item->GetMapId() == to_map->GetId() && item->GetHexX() == to_hx && item->GetHexY() == to_hy) {
        return;
    }

    auto* holder = GetItemHolder(item);

    if (holder == nullptr) {
        return;
    }

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);
        to_map->AddItem(item, to_hx, to_hy, dynamic_cast<Critter*>(holder));
    }
    else {
        auto* item_ = SplitItem(item, count);
        to_map->AddItem(item_, to_hx, to_hy, dynamic_cast<Critter*>(holder));
    }
}

void ItemManager::MoveItem(Item* item, uint count, Item* to_cont, ContainerItemStack stack_id)
{
    STACK_TRACE_ENTRY();

    if (item->GetOwnership() == ItemOwnership::ItemContainer && item->GetContainerId() == to_cont->GetId() && item->GetContainerStack() == stack_id) {
        return;
    }

    auto* holder = GetItemHolder(item);

    if (holder == nullptr) {
        return;
    }

    if (count >= item->GetCount() || !item->GetStackable()) {
        RemoveItemHolder(item, holder);
        to_cont->AddItemToContainer(item, stack_id);
    }
    else {
        auto* item_ = SplitItem(item, count);
        to_cont->AddItemToContainer(item_, stack_id);
    }
}

auto ItemManager::AddItemContainer(Item* cont, hstring pid, uint count, ContainerItemStack stack_id) -> Item*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(cont);

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto* item = cont->GetInnerItemByPid(pid, stack_id);
    Item* result = nullptr;

    if (item != nullptr) {
        if (item->GetStackable()) {
            item->SetCount(item->GetCount() + count);
            result = item;
        }
        else {
            if (count > MAX_ADDED_NOGROUP_ITEMS) {
                count = MAX_ADDED_NOGROUP_ITEMS;
            }
            for (uint i = 0; i < count; ++i) {
                item = CreateItem(pid, 0, nullptr);
                item = cont->AddItemToContainer(item, stack_id);
                result = item;
            }
        }
    }
    else {
        if (proto->GetStackable()) {
            item = CreateItem(pid, count, nullptr);
            item = cont->AddItemToContainer(item, stack_id);
            result = item;
        }
        else {
            if (count > MAX_ADDED_NOGROUP_ITEMS) {
                count = MAX_ADDED_NOGROUP_ITEMS;
            }
            for (uint i = 0; i < count; ++i) {
                item = CreateItem(pid, 0, nullptr);
                item = cont->AddItemToContainer(item, stack_id);
                result = item;
            }
        }
    }

    return result;
}

auto ItemManager::AddItemCritter(Critter* cr, hstring pid, uint count) -> Item*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(count > 0);

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);
    auto* item = cr->GetInvItemByPid(pid);
    Item* result = nullptr;

    if (item != nullptr && item->GetStackable()) {
        item->SetCount(item->GetCount() + count);
        result = item;
    }
    else {
        if (proto->GetStackable()) {
            item = CreateItem(pid, count, nullptr);
            item = _engine->CrMngr.AddItemToCritter(cr, item, true);
            result = item;
        }
        else {
            if (count > MAX_ADDED_NOGROUP_ITEMS) {
                count = MAX_ADDED_NOGROUP_ITEMS;
            }

            for (uint i = 0; i < count; ++i) {
                item = CreateItem(pid, 0, nullptr);
                item = _engine->CrMngr.AddItemToCritter(cr, item, true);
                result = item;
            }
        }
    }

    return result;
}

void ItemManager::SubItemCritter(Critter* cr, hstring pid, uint count)
{
    STACK_TRACE_ENTRY();

    if (count == 0) {
        return;
    }

    auto* item = _engine->CrMngr.GetItemByPidInvPriority(cr, pid);

    if (item == nullptr) {
        return;
    }

    if (item->GetStackable()) {
        if (count >= item->GetCount()) {
            DestroyItem(item);
        }
        else {
            item->SetCount(item->GetCount() - count);
        }
    }
    else {
        for (uint i = 0; i < count; ++i) {
            DestroyItem(item);

            item = _engine->CrMngr.GetItemByPidInvPriority(cr, pid);

            if (item == nullptr) {
                break;
            }
        }
    }
}

void ItemManager::SetItemCritter(Critter* cr, hstring pid, uint count)
{
    STACK_TRACE_ENTRY();

    const auto cur_count = cr->CountInvItemPid(pid);

    if (cur_count > count) {
        SubItemCritter(cr, pid, cur_count - count);
    }
    if (cur_count < count) {
        AddItemCritter(cr, pid, count - cur_count);
    }
}

void ItemManager::RegisterRadio(Item* radio)
{
    STACK_TRACE_ENTRY();

    const auto it = _radioItems.find(radio);

    if (it == _radioItems.end()) {
        _radioItems.emplace(radio);
        radio->AddRef();
    }
}

void ItemManager::UnregisterRadio(Item* radio)
{
    STACK_TRACE_ENTRY();

    const auto it = _radioItems.find(radio);

    if (it != _radioItems.end()) {
        _radioItems.erase(it);
        radio->Release();
    }
}

void ItemManager::RadioSendText(Critter* cr, string_view text, bool unsafe_text, TextPackName text_pack, TextPackKey str_num, vector<uint16>& channels)
{
    STACK_TRACE_ENTRY();

    vector<Item*> radios;

    for (auto* item : cr->GetInvItems()) {
        if (item->GetIsRadio() && item->RadioIsSendActive() && std::find(channels.begin(), channels.end(), item->GetRadioChannel()) == channels.end()) {
            channels.push_back(item->GetRadioChannel());
            radios.push_back(item);
        }
    }

    for (uint i = 0, j = static_cast<uint>(radios.size()); i < j; i++) {
        RadioSendTextEx(channels[i], radios[i]->GetRadioBroadcastSend(), cr->GetMapId(), cr->GetWorldX(), cr->GetWorldY(), text, unsafe_text, text_pack, str_num, "");
    }
}

void ItemManager::RadioSendTextEx(uint16 channel, uint8 broadcast_type, ident_t from_map_id, uint16 from_wx, uint16 from_wy, string_view text, bool unsafe_text, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (broadcast_type != RADIO_BROADCAST_FORCE_ALL && broadcast_type != RADIO_BROADCAST_WORLD && broadcast_type != RADIO_BROADCAST_MAP && broadcast_type != RADIO_BROADCAST_LOCATION && (broadcast_type < 101 || broadcast_type > 200) /*RADIO_BROADCAST_ZONE*/) {
        return;
    }
    if ((broadcast_type == RADIO_BROADCAST_MAP || broadcast_type == RADIO_BROADCAST_LOCATION) && !from_map_id) {
        return;
    }

    uint8 broadcast;
    auto broadcast_map_id = ident_t {};
    auto broadcast_loc_id = ident_t {};

    const auto cur_send = ++_radioSendCounter;

    for (const auto* radio : _radioItems) {
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
                    if (!broadcast_map_id) {
                        auto* map = _engine->EntityMngr.GetMap(from_map_id);

                        if (map == nullptr) {
                            continue;
                        }

                        broadcast_map_id = map->GetId();
                        broadcast_loc_id = map->GetLocation()->GetId();
                    }
                }
                else if (broadcast < 101 || broadcast > 200 /*RADIO_BROADCAST_ZONE*/) {
                    continue;
                }
            }
            else {
                broadcast = RADIO_BROADCAST_FORCE_ALL;
            }

            if (radio->GetOwnership() == ItemOwnership::CritterInventory) {
                auto* cr = _engine->EntityMngr.GetCritter(radio->GetCritterId());

                if (cr != nullptr && cr->RadioMessageSended != cur_send) {
                    if (broadcast != RADIO_BROADCAST_FORCE_ALL) {
                        if (broadcast == RADIO_BROADCAST_MAP) {
                            if (broadcast_map_id != cr->GetMapId()) {
                                continue;
                            }
                        }
                        else if (broadcast == RADIO_BROADCAST_LOCATION) {
                            auto* map = _engine->EntityMngr.GetMap(cr->GetMapId());

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
                        cr->Send_TextMsgLex(radio->GetId(), SAY_RADIO, text_pack, str_num, lexems);
                    }
                    else {
                        cr->Send_TextMsg(radio->GetId(), SAY_RADIO, text_pack, str_num);
                    }

                    cr->RadioMessageSended = cur_send;
                }
            }
            else if (radio->GetOwnership() == ItemOwnership::MapHex) {
                if (broadcast == RADIO_BROADCAST_MAP && broadcast_map_id != radio->GetMapId()) {
                    continue;
                }

                auto* map = _engine->EntityMngr.GetMap(radio->GetMapId());

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
                            const auto* loc = map->GetLocation();

                            if (!_engine->MapMngr.IsIntersectZone(from_wx, from_wy, 0, loc->GetWorldX(), loc->GetWorldY(), loc->GetRadius(), broadcast - 101)) {
                                continue;
                            }
                        }
                        else {
                            continue;
                        }
                    }

                    if (!text.empty()) {
                        map->SetText(radio->GetHexX(), radio->GetHexY(), ucolor {255, 255, 254, 255}, text, unsafe_text);
                    }
                    else if (!lexems.empty()) {
                        map->SetTextMsgLex(radio->GetHexX(), radio->GetHexY(), ucolor {255, 255, 254, 255}, text_pack, str_num, lexems);
                    }
                    else {
                        map->SetTextMsg(radio->GetHexX(), radio->GetHexY(), ucolor {255, 255, 254, 255}, text_pack, str_num);
                    }
                }
            }
        }
    }
}

void ItemManager::ChangeItemStatistics(hstring pid, int val) const
{
    STACK_TRACE_ENTRY();

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);

    proto->InstanceCount += val;
}

auto ItemManager::GetItemStatistics(hstring pid) const -> int64
{
    STACK_TRACE_ENTRY();

    const auto* proto = _engine->ProtoMngr.GetProtoItem(pid);

    return proto->InstanceCount;
}

auto ItemManager::GetItemsStatistics() const -> string
{
    STACK_TRACE_ENTRY();

    string result = "Name                                     Count\n";

    for (auto&& [pid, proto] : _engine->ProtoMngr.GetProtoItems()) {
        result += format("{:<40} {:<20}\n", proto->GetName(), proto->InstanceCount);
    }

    return result;
}
