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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ItemManager.h"
#include "MapManager.h"
#include "ProtoManager.h"
#include "Server.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

CritterManager::CritterManager(ptr<ServerEngine> engine) noexcept :
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

auto CritterManager::AddItemToCritter(ptr<Critter> cr, ptr<Item> item, bool send) -> ptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Cannot add an item to an already destroyed critter", cr->GetId());
    FO_VERIFY_AND_THROW(!cr->IsDestroying(), "Cannot add an item to a critter that is being destroyed", cr->GetId());
    auto cr_holder = cr.hold_ref();
    auto item_holder = item.hold_ref();
    ignore_unused(cr_holder);
    ignore_unused(item_holder);
    ValidateEntityAccess(cr);
    EnsureEntitySynced(item);

    if (item->GetStackable()) {
        auto item_already = cr->GetInvItemByPid(item->GetProtoId());

        if (item_already) {
            if (item_already == item) {
                return item;
            }

            const auto count = item->GetCount();
            _engine->ItemMngr.DestroyItem(item);
            item_already->SetCount(item_already->GetCount() + count);
            return item_already;
        }
    }

    if (item->GetOwnership() != ItemOwnership::CritterInventory) {
        item->SetCritterSlot(CritterItemSlot::Inventory);
    }

    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(cr->GetId());

    cr->SetItem(item);

    auto item_ids = cr->GetItemIds();
    vec_add_unique_value(item_ids, item->GetId());
    cr->SetItemIds(item_ids);

    if (cr->IsPersistent()) {
        _engine->EntityMngr.MakePersistent(item, true);
    }

    if (send && !item->GetHidden()) {
        cr->Send_ChosenAddItem(item);

        if (item->GetCritterSlot() != CritterItemSlot::Inventory) {
            cr->SendAndBroadcast_MoveItem(item, CritterAction::Refresh, CritterItemSlot::Inventory);
        }
    }

    _engine->OnCritterItemMoved.Fire(cr, item, CritterItemSlot::Outside);

    if (cr->IsDestroyed() || item->IsDestroyed()) {
        throw CritterManagerException("Critter item add event destroyed the committed entity", cr->GetId(), item->GetId());
    }
    if (item->GetOwnership() != ItemOwnership::CritterInventory || item->GetCritterId() != cr->GetId() || cr->GetInvItem(item->GetId()) != item) {
        throw CritterManagerException("Critter item add event moved the committed item", cr->GetId(), item->GetId());
    }

    return item;
}

void CritterManager::RemoveItemFromCritter(ptr<Critter> cr, ptr<Item> item, bool send)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);
    EnsureEntitySynced(item);

    cr->RemoveItem(item);

    auto item_ids = cr->GetItemIds();
    vec_remove_unique_value(item_ids, item->GetId());
    cr->SetItemIds(item_ids);

    item->SetOwnership(ItemOwnership::Nowhere);

    if (send) {
        cr->Send_ChosenRemoveItem(item);
    }
    if (item->GetCritterSlot() != CritterItemSlot::Inventory) {
        cr->SendAndBroadcast_MoveItem(item, CritterAction::Refresh, CritterItemSlot::Inventory);
    }

    const auto prev_slot = item->GetCritterSlot();

    item->SetCritterId(ident_t {});
    item->SetCritterSlot(CritterItemSlot::Inventory);

    if (item->IsPersistent() && !item->IsExplicitlyPersistent()) {
        _engine->EntityMngr.MakePersistent(item, false);
    }

    if (!cr->IsDestroying()) {
        _engine->OnCritterItemMoved.Fire(cr, item, prev_slot);
    }
}

auto CritterManager::CreateCritterOnMap(hstring proto_id, nptr<const Properties> props, ptr<Map> map, mpos hex, mdir dir) -> ptr<Critter>
{
    FO_STACK_TRACE_ENTRY();

    auto map_holder = map.hold_ref();
    ignore_unused(map_holder);
    ValidateEntityAccess(map);
    FO_VERIFY_AND_THROW(map->GetSize().is_valid_pos(hex), "Critter creation hex is outside target map bounds", proto_id, map->GetId(), hex, map->GetSize());

    auto proto = _engine->GetProtoCritter(proto_id);

    if (!proto) {
        throw CritterManagerException("Critter proto not found", proto_id);
    }

    int32_t multihex;

    if (props) {
        auto props_copy = props->Copy();
        const auto cr_props = CritterProperties(props_copy);
        multihex = cr_props.GetMultihex();
    }
    else {
        multihex = proto->GetMultihex();
    }

    // Find better place if target hex busy
    auto final_hex = hex;

    if (!map->IsHexesMovable(hex, multihex)) {
        const auto hexes_around = GeometryHelper::HexesInRadius(2);
        const auto map_size = map->GetSize();

        for (int32_t i = 1; i <= hexes_around; i++) {
            auto check_hex = hex;

            if (!GeometryHelper::MoveHexAroundAway(check_hex, i, map_size)) {
                continue;
            }
            if (!map->IsHexesMovable(check_hex, multihex)) {
                continue;
            }

            final_hex = check_hex;
            break;
        }
    }

    // Create critter
    auto cr = SafeAlloc::MakeRefCounted<Critter>(_engine, ident_t {}, proto.as_ptr(), props);

    _engine->EntityMngr.RegisterCritter(cr);

    auto loc = map->GetLocation();
    FO_VERIFY_AND_THROW(loc, "Missing location instance");

    const ident_t target_map_id = map->GetId();

    _engine->MapMngr.AddCritterToMap(cr, map, final_hex, dir, ident_t {});

    if (!cr->IsDestroyed()) {
        _engine->OnMapCritterIn.Fire(map, cr);

        if (!cr->IsDestroyed()) {
            _engine->EntityMngr.CallInit(cr, true);
            _engine->MapMngr.ProcessVisibleCritters(cr);
            _engine->MapMngr.ProcessVisibleItems(cr);
        }
    }

    if (cr->IsDestroyed()) {
        throw CritterManagerException("Critter destroyed during init");
    }
    if (map->IsDestroyed()) {
        throw CritterManagerException("Critter map add event destroyed the target map", proto_id);
    }
    auto map_cr = map->GetCritter(cr->GetId());

    if (cr->GetMapId() != target_map_id || cr->GetHex() != final_hex || !(map_cr == cr)) {
        throw CritterManagerException("Critter map add event moved the committed critter", proto_id);
    }

    return cr;
}

void CritterManager::DestroyCritter(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    auto cr_holder = cr.hold_ref();
    ignore_unused(cr_holder);

    EnsureEntitySynced(cr);
    FO_VERIFY_AND_THROW(!cr->GetControlledByPlayer(), "Critter is controlled by a player in an NPC-only operation");

    // Skip redundant calls
    if (cr->IsDestroying() || cr->IsDestroyed()) {
        return;
    }

    cr->MarkAsDestroying();

    // Finish event
    _engine->OnCritterFinish.Fire(cr);
    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");

    // Tear off from environment
    {
        cr->LockMapTransfers();
        auto restore_transfers = scope_exit([cr]() mutable noexcept { cr->UnlockMapTransfers(); });

        if (cr->GetMapId()) {
            auto map = cr->GetParent<Map>();
            FO_VERIFY_AND_THROW(map, "Missing map instance");
            ValidateEntityAccess(map);
        }

        for (size_t prev_deps = std::numeric_limits<size_t>::max(); cr->GetMapId() || cr->GetRawGlobalMapGroup() || cr->HasItems() || cr->HasInnerEntities() || cr->GetIsAttached() || cr->HasAttachedCritters();) {
            try {
                DestroyInventory(cr);

                if (cr->HasInnerEntities()) {
                    _engine->EntityMngr.DestroyInnerEntities(cr);
                }

                if (cr->GetIsAttached()) {
                    cr->DetachFromCritter();
                }

                if (cr->HasAttachedCritters()) {
                    for (ptr<Critter> attached_cr : copy_hold_ref(cr->GetAttachedCritters())) {
                        attached_cr->DetachFromCritter();
                    }
                }

                if (cr->GetMapId()) {
                    auto map = cr->GetParent<Map>();
                    FO_VERIFY_AND_THROW(map, "Missing map instance");
                    ValidateEntityAccess(map);
                    _engine->MapMngr.RemoveCritterFromMap(cr, map);
                }
                else if (cr->GetRawGlobalMapGroup()) {
                    _engine->MapMngr.RemoveCritterFromMap(cr, nullptr);
                }
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }

            // Each teardown pass must strictly reduce the critter's remaining dependencies; a stalled
            // (or growing) count means the destruction can never converge, so terminate deterministically
            // rather than leave a half-destroyed "undead" critter in the registry until restart.
            const size_t remaining_deps = (cr->GetMapId() ? 1 : 0) + (cr->GetRawGlobalMapGroup() ? 1 : 0) + cr->GetInvItems().size() + cr->GetInnerEntitiesCount() + (cr->GetIsAttached() ? 1 : 0) + cr->GetAttachedCritters().size();
            FO_STRONG_ASSERT(remaining_deps < prev_deps, "Critter destruction made no progress", cr->GetId(), remaining_deps, prev_deps);
            prev_deps = remaining_deps;
        }
    }

    _engine->TimeEventMngr.CancelAllForEntity(cr);
    cr->SetParent(nullptr);
    cr->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterCritter(cr);
}

void CritterManager::DestroyInventory(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);

    for (size_t prev_deps = std::numeric_limits<size_t>::max(); cr->HasItems();) {
        ptr<Item> item = cr->GetInvItems().front();
        ValidateEntityAccess(item);
        _engine->ItemMngr.DestroyItem(item);

        const size_t remaining_deps = cr->GetInvItems().size();
        FO_STRONG_ASSERT(remaining_deps < prev_deps, "Critter inventory destruction made no progress", cr->GetId(), remaining_deps, prev_deps);
        prev_deps = remaining_deps;
    }
}

auto CritterManager::GetNonPlayerCritters() -> vector<refcount_ptr<Critter>>
{
    FO_STACK_TRACE_ENTRY();

    vector<refcount_ptr<Critter>> all_critters = _engine->EntityMngr.GetCritters();
    vector<refcount_ptr<Critter>> result;
    result.reserve(all_critters.size());

    for (auto& cr : all_critters) {
        if (!cr->GetControlledByPlayer()) {
            result.emplace_back(cr);
        }
    }

    return result;
}

auto CritterManager::GetPlayerCritters() -> vector<refcount_ptr<Critter>>
{
    FO_STACK_TRACE_ENTRY();

    vector<refcount_ptr<Critter>> all_critters = _engine->EntityMngr.GetCritters();

    vector<refcount_ptr<Critter>> result;
    result.reserve(all_critters.size());

    for (auto& cr : all_critters) {
        if (cr->GetControlledByPlayer()) {
            result.emplace_back(cr);
        }
    }

    return result;
}

FO_END_NAMESPACE
