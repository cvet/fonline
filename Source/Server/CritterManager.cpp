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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

CritterManager::CritterManager(FOServer* engine) :
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

auto CritterManager::AddItemToCritter(Critter* cr, Item* item, bool send) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr);
    FO_RUNTIME_ASSERT(item);

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

    cr->SetItem(item);

    auto item_ids = cr->GetItemIds();
    vec_add_unique_value(item_ids, item->GetId());
    cr->SetItemIds(item_ids);

    if (send && !item->GetHidden()) {
        cr->Send_ChosenAddItem(item);

        if (item->GetCritterSlot() != CritterItemSlot::Inventory) {
            cr->SendAndBroadcast_MoveItem(item, CritterAction::Refresh, CritterItemSlot::Inventory);
        }
    }

    _engine->OnCritterItemMoved.Fire(cr, item, CritterItemSlot::Outside);

    return item;
}

void CritterManager::RemoveItemFromCritter(Critter* cr, Item* item, bool send)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr);
    FO_RUNTIME_ASSERT(item);

    cr->RemoveItem(item);

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

    _engine->OnCritterItemMoved.Fire(cr, item, prev_slot);
}

auto CritterManager::CreateCritterOnMap(hstring proto_id, const Properties* props, Map* map, mpos hex, uint8 dir) -> Critter*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(map);
    FO_RUNTIME_ASSERT(map->GetSize().is_valid_pos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoCritter(proto_id);

    int32 multihex;

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

        for (int32 i = 1; i <= hexes_around; i++) {
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

    // Resolve direction
    auto final_dir = dir;

    if (dir >= GameSettings::MAP_DIR_COUNT) {
        final_dir = numeric_cast<uint8>(GenericUtils::Random(0u, GameSettings::MAP_DIR_COUNT - 1u));
    }

    // Create critter
    auto cr = SafeAlloc::MakeRefCounted<Critter>(_engine.get(), ident_t {}, proto, props);

    _engine->EntityMngr.RegisterCritter(cr.get());

    const auto* loc = map->GetLocation();
    FO_RUNTIME_ASSERT(loc);

    _engine->MapMngr.AddCritterToMap(cr.get(), map, final_hex, final_dir, ident_t {});

    _engine->EntityMngr.CallInit(cr.get(), true);
    _engine->MapMngr.ProcessVisibleItems(cr.get());

    if (cr->IsDestroyed()) {
        throw GenericException("Critter destroyed during init");
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
    _engine->OnCritterFinish.Fire(cr);

    // Tear off from environment
    {
        cr->LockMapTransfers++;
        auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

        for (InfinityLoopDetector detector; cr->GetMapId() || cr->GetRawGlobalMapGroup() || cr->HasItems() || cr->HasInnerEntities() || cr->GetIsAttached() || !cr->AttachedCritters.empty(); detector.AddLoop()) {
            try {
                if (cr->GetMapId()) {
                    auto* map = _engine->EntityMngr.GetMap(cr->GetMapId());
                    FO_RUNTIME_ASSERT(map);
                    _engine->MapMngr.RemoveCritterFromMap(cr, map);
                }
                else {
                    FO_RUNTIME_ASSERT(cr->GetRawGlobalMapGroup());
                    _engine->MapMngr.RemoveCritterFromMap(cr, nullptr);
                }

                DestroyInventory(cr);

                if (cr->HasInnerEntities()) {
                    _engine->EntityMngr.DestroyInnerEntities(cr);
                }

                if (cr->GetIsAttached()) {
                    cr->DetachFromCritter();
                }

                if (!cr->AttachedCritters.empty()) {
                    for (auto& attached_cr : copy_hold_ref(cr->AttachedCritters)) {
                        attached_cr->DetachFromCritter();
                    }
                }
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        }
    }

    // Invalidate for use
    cr->MarkAsDestroyed();

    // Erase from main collection
    _engine->EntityMngr.UnregisterCritter(cr);
}

void CritterManager::DestroyInventory(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    for (InfinityLoopDetector detector {cr->GetInvItems().size()}; cr->HasItems(); detector.AddLoop()) {
        _engine->ItemMngr.DestroyItem(cr->GetInvItems().front().get());
    }
}

auto CritterManager::GetNonPlayerCritters() -> vector<Critter*>
{
    FO_STACK_TRACE_ENTRY();

    auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> non_player_critters;
    non_player_critters.reserve(all_critters.size());

    for (auto& cr : all_critters | std::views::values) {
        if (!cr->GetControlledByPlayer()) {
            non_player_critters.emplace_back(cr.get());
        }
    }

    return non_player_critters;
}

auto CritterManager::GetPlayerCritters(bool on_global_map_only) -> vector<Critter*>
{
    FO_STACK_TRACE_ENTRY();

    auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> player_critters;
    player_critters.reserve(all_critters.size());

    for (auto& cr : all_critters | std::views::values) {
        if (cr->GetControlledByPlayer() && (!on_global_map_only || !cr->GetMapId())) {
            player_critters.emplace_back(cr.get());
        }
    }

    return player_critters;
}

auto CritterManager::GetGlobalMapCritters(CritterFindType find_type) -> vector<Critter*>
{
    FO_STACK_TRACE_ENTRY();

    auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> critters;
    critters.reserve(all_critters.size());

    for (auto& cr : all_critters | std::views::values) {
        if (!cr->GetMapId() && cr->CheckFind(find_type)) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

auto CritterManager::GetItemByPidInvPriority(Critter* cr, hstring item_pid) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    const auto* proto = _engine->ProtoMngr.GetProtoItem(item_pid);

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

FO_END_NAMESPACE();
