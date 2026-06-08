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

CritterManager::CritterManager(ServerEngine* engine) :
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

auto CritterManager::AddItemToCritter(Critter* cr, Item* item, bool send) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr);
    FO_RUNTIME_ASSERT(item);
    refcount_ptr cr_holder = cr;
    refcount_ptr item_holder = item;
    ignore_unused(cr_holder);
    ignore_unused(item_holder);

    if (item->GetStackable()) {
        auto* item_already = cr->GetInvItemByPid(item->GetProtoId());

        if (item_already != nullptr) {
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

    PropagateEntityLock(item, cr->GetEntityLock());

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

    ValidateEntityAccess(cr);
    ValidateEntityAccess(item);
    _engine->OnCritterItemMoved.Fire(cr, item, CritterItemSlot::Outside);

    if (cr->IsDestroyed() || item->IsDestroyed()) {
        throw CritterManagerException("Critter item add event destroyed the committed entity", cr->GetId(), item->GetId());
    }
    if (item->GetOwnership() != ItemOwnership::CritterInventory || item->GetCritterId() != cr->GetId() || cr->GetInvItem(item->GetId()) != item) {
        throw CritterManagerException("Critter item add event moved the committed item", cr->GetId(), item->GetId());
    }

    return item;
}

void CritterManager::RemoveItemFromCritter(Critter* cr, Item* item, bool send)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr);
    FO_RUNTIME_ASSERT(item);

    cr->RemoveItem(item);

    RevertEntityLock(item);
    auto* ctx = _engine->GetCurrentSyncContext();
    FO_RUNTIME_ASSERT(ctx);
    ctx->EnsureEntitySynced(item);

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

    auto item_ids = cr->GetItemIds();
    vec_remove_unique_value(item_ids, item->GetId());
    cr->SetItemIds(item_ids);

    if (item->IsPersistent() && !item->IsExplicitlyPersistent()) {
        _engine->EntityMngr.MakePersistent(item, false);
    }

    ValidateEntityAccess(cr);
    ValidateEntityAccess(item);
    _engine->OnCritterItemMoved.Fire(cr, item, prev_slot);
}

auto CritterManager::CreateCritterOnMap(hstring proto_id, const Properties* props, Map* map, mpos hex, mdir dir) -> Critter*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(map);
    refcount_ptr map_holder = map;
    ignore_unused(map_holder);

    auto* ctx = _engine->GetCurrentSyncContext();
    FO_RUNTIME_ASSERT(ctx);

    bool release_empty_sync_context = false;

    if (ctx->IsEmpty()) {
        ctx->SyncEntity(map);
        release_empty_sync_context = true;
    }

    auto release_empty_sync = scope_exit([ctx, release_empty_sync_context]() noexcept {
        if (release_empty_sync_context) {
            ctx->Release();
        }
    });

    ValidateEntityAccess(map);
    FO_RUNTIME_ASSERT(map->GetSize().is_valid_pos(hex));

    const auto* proto = _engine->GetProtoCritter(proto_id);

    if (proto == nullptr) {
        throw CritterManagerException("Critter proto not found", proto_id);
    }

    int32_t multihex;

    if (props != nullptr) {
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
    auto cr = SafeAlloc::MakeRefCounted<Critter>(_engine.get(), ident_t {}, proto, props);

    _engine->EntityMngr.RegisterCritter(cr.get());

    const auto* loc = map->GetLocation();
    FO_RUNTIME_ASSERT(loc);

    const ident_t target_map_id = map->GetId();

    _engine->MapMngr.AddCritterToMap(cr.get(), map, final_hex, dir, ident_t {});

    if (!cr->IsDestroyed()) {
        if (map != nullptr) {
            ValidateEntityAccess(map);
            ValidateEntityAccess(cr.get());
            _engine->OnMapCritterIn.Fire(map, cr.get());
        }
        else {
            ValidateEntityAccess(cr.get());
            _engine->OnGlobalMapCritterIn.Fire(cr.get());
        }

        if (!cr->IsDestroyed()) {
            _engine->EntityMngr.CallInit(cr.get(), true);
            _engine->MapMngr.ProcessVisibleCritters(cr.get());
            _engine->MapMngr.ProcessVisibleItems(cr.get());
        }
    }

    if (cr->IsDestroyed()) {
        throw CritterManagerException("Critter destroyed during init");
    }
    if (map->IsDestroyed()) {
        throw CritterManagerException("Critter map add event destroyed the target map", proto_id);
    }
    if (cr->GetMapId() != target_map_id || cr->GetHex() != final_hex || map->GetCritter(cr->GetId()) != cr.get()) {
        throw CritterManagerException("Critter map add event moved the committed critter", proto_id);
    }

    return cr.get();
}

void CritterManager::DestroyCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!cr->GetControlledByPlayer());

    // Skip redundant calls
    if (cr->IsDestroying() || cr->IsDestroyed()) {
        return;
    }

    cr->MarkAsDestroying();

    // Finish event
    ValidateEntityAccess(cr);
    _engine->OnCritterFinish.Fire(cr);
    FO_RUNTIME_ASSERT(!cr->IsDestroyed());

    // Tear off from environment
    {
        cr->LockMapTransfers();
        auto restore_transfers = scope_exit([cr]() noexcept { cr->UnlockMapTransfers(); });

        for (InfinityLoopDetector detector; cr->GetMapId() || cr->GetRawGlobalMapGroup() || cr->HasItems() || cr->HasInnerEntities() || cr->GetIsAttached() || cr->HasAttachedCritters(); detector.AddLoop()) {
            try {
                DestroyInventory(cr);

                if (cr->HasInnerEntities()) {
                    _engine->EntityMngr.DestroyInnerEntities(cr);
                }

                if (cr->GetIsAttached()) {
                    cr->DetachFromCritter();
                }

                if (cr->HasAttachedCritters()) {
                    for (auto* attached_cr : copy_hold_ref(cr->GetAttachedCritters())) {
                        attached_cr->DetachFromCritter();
                    }
                }

                if (cr->GetMapId()) {
                    auto map = cr->GetParent<Map>();
                    FO_RUNTIME_ASSERT(map);
                    auto* ctx = _engine->GetCurrentSyncContext();
                    FO_RUNTIME_ASSERT(ctx);
                    ctx->EnsureEntitySynced(map.get());
                    _engine->MapMngr.RemoveCritterFromMap(cr, map.get());
                }
                else {
                    FO_RUNTIME_ASSERT(cr->GetRawGlobalMapGroup());
                    _engine->MapMngr.RemoveCritterFromMap(cr, nullptr);
                }
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        }
    }

    _engine->TimeEventMngr.CancelAllForEntity(cr);
    cr->SetParent(nullptr);
    cr->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterCritter(cr);
}

void CritterManager::DestroyInventory(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    auto* ctx = _engine->GetCurrentSyncContext();
    FO_RUNTIME_ASSERT(ctx);

    for (InfinityLoopDetector detector {cr->GetInvItems().size()}; cr->HasItems(); detector.AddLoop()) {
        auto* item = cr->GetInvItems().front().get();
        ctx->EnsureEntitySynced(item);
        _engine->ItemMngr.DestroyItem(item);
    }
}

auto CritterManager::GetNonPlayerCritters() -> vector<refcount_ptr<Critter>>
{
    FO_STACK_TRACE_ENTRY();

    auto all_critters = _engine->EntityMngr.GetCritters();
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

    auto all_critters = _engine->EntityMngr.GetCritters();

    vector<refcount_ptr<Critter>> result;
    result.reserve(all_critters.size());

    for (auto& cr : all_critters) {
        if (cr->GetControlledByPlayer()) {
            result.emplace_back(cr);
        }
    }

    return result;
}

auto CritterManager::GetItemByPidInvPriority(Critter* cr, hstring item_pid) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);

    const auto* proto = _engine->GetProtoItem(item_pid);

    if (proto == nullptr) {
        throw CritterManagerException("Item proto not found", item_pid);
    }

    if (proto->GetStackable()) {
        for (auto& item : cr->GetInvItems()) {
            if (item->GetProtoId() == item_pid) {
                return item.get();
            }
        }
    }
    else {
        Item* another_slot = nullptr;

        for (auto& item : cr->GetInvItems()) {
            if (item->GetProtoId() == item_pid) {
                if (item->GetCritterSlot() == CritterItemSlot::Inventory) {
                    return item.get();
                }

                another_slot = item.get();
            }
        }

        return another_slot;
    }

    return nullptr;
}

FO_END_NAMESPACE
