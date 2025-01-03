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
#include "GenericUtils.h"
#include "ItemManager.h"
#include "Log.h"
#include "MapManager.h"
#include "ProtoManager.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"

CritterManager::CritterManager(FOServer* engine) :
    _engine {engine}
{
    STACK_TRACE_ENTRY();
}

auto CritterManager::AddItemToCritter(Critter* cr, Item* item, bool send) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(cr);
    RUNTIME_ASSERT(item);

    if (item->GetStackable()) {
        auto* item_already = cr->GetInvItemByPid(item->GetProtoId());

        if (item_already != nullptr) {
            const auto count = item->GetCount();

            _engine->ItemMngr.DestroyItem(item);
            item_already->SetCount(item_already->GetCount() + count);

            return item_already;
        }
    }

    item->EvaluateSortValue(cr->_invItems);

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
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(cr);
    RUNTIME_ASSERT(item);

    if (item->GetIsRadio()) {
        _engine->ItemMngr.UnregisterRadio(item);
    }

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
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(map);
    RUNTIME_ASSERT(map->GetSize().IsValidPos(hex));

    const auto* proto = _engine->ProtoMngr.GetProtoCritter(proto_id);

    uint multihex;

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
        const auto map_size = map->GetSize();
        const auto [sx, sy] = _engine->Geometry.GetHexOffsets(hex);

        // Find in 2 hex radius
        int pos = -1;

        while (true) {
            // Todo: find better place for critter in square geometry
            if (GameSettings::SQUARE_GEOMETRY) {
                break;
            }

            pos++;

            if (pos >= 18) {
                break;
            }

            const auto raw_check_hex = ipos {hex.x + sx[pos], hex.y + sy[pos]};

            if (!map_size.IsValidPos(raw_check_hex)) {
                continue;
            }
            if (!map->IsHexesMovable(map_size.FromRawPos(raw_check_hex), multihex)) {
                continue;
            }

            final_hex = map_size.FromRawPos(raw_check_hex);
            break;
        }
    }

    // Resolve direction
    auto final_dir = dir;

    if (dir >= GameSettings::MAP_DIR_COUNT) {
        final_dir = static_cast<uint8>(GenericUtils::Random(0u, GameSettings::MAP_DIR_COUNT - 1u));
    }

    // Create critter
    auto* cr = SafeAlloc::MakeRaw<Critter>(_engine, ident_t {}, proto, props);

    _engine->EntityMngr.RegisterCritter(cr);

    const auto* loc = map->GetLocation();
    RUNTIME_ASSERT(loc);

    _engine->MapMngr.AddCritterToMap(cr, map, final_hex, final_dir, ident_t {});

    _engine->EntityMngr.CallInit(cr, true);
    _engine->MapMngr.ProcessVisibleItems(cr);

    return cr;
}

void CritterManager::DestroyCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(!cr->GetControlledByPlayer());

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

        for (InfinityLoopDetector detector; cr->GetMapId() || cr->GlobalMapGroup != nullptr || cr->RealCountInvItems() != 0 || cr->HasInnerEntities() || cr->GetIsAttached() || !cr->AttachedCritters.empty(); detector.AddLoop()) {
            if (cr->GetMapId()) {
                auto* map = _engine->EntityMngr.GetMap(cr->GetMapId());
                RUNTIME_ASSERT(map);
                _engine->MapMngr.RemoveCritterFromMap(cr, map);
            }
            else {
                RUNTIME_ASSERT(cr->GlobalMapGroup);
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
                for (auto* attached_cr : copy(cr->AttachedCritters)) {
                    attached_cr->DetachFromCritter();
                }
            }
        }
    }

    // Erase from main collection
    _engine->EntityMngr.UnregisterCritter(cr);

    // Invalidate for use
    cr->MarkAsDestroyed();
    cr->Release();
}

void CritterManager::DestroyInventory(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (InfinityLoopDetector detector {cr->_invItems.size()}; !cr->_invItems.empty(); detector.AddLoop()) {
        _engine->ItemMngr.DestroyItem(cr->_invItems.front());
    }
}

auto CritterManager::GetNonPlayerCritters() -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> non_player_critters;
    non_player_critters.reserve(all_critters.size());

    for (auto&& [id, cr] : all_critters) {
        if (!cr->GetControlledByPlayer()) {
            non_player_critters.emplace_back(cr);
        }
    }

    return non_player_critters;
}

auto CritterManager::GetPlayerCritters(bool on_global_map_only) -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> player_critters;
    player_critters.reserve(all_critters.size());

    for (auto&& [id, cr] : all_critters) {
        if (cr->GetControlledByPlayer() && (!on_global_map_only || !cr->GetMapId())) {
            player_critters.emplace_back(cr);
        }
    }

    return player_critters;
}

auto CritterManager::GetGlobalMapCritters(CritterFindType find_type) -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> critters;
    critters.reserve(all_critters.size());

    for (auto&& [id, cr] : all_critters) {
        if (!cr->GetMapId() && cr->CheckFind(find_type)) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

auto CritterManager::GetItemByPidInvPriority(Critter* cr, hstring item_pid) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto* proto = _engine->ProtoMngr.GetProtoItem(item_pid);

    if (proto->GetStackable()) {
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
                if (item->GetCritterSlot() == CritterItemSlot::Inventory) {
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
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (cr->Talk.Type == TalkType::None) {
        return;
    }

    if (!force && _engine->GameTime.GameplayTime() < cr->_talkNextTime) {
        return;
    }

    cr->_talkNextTime = _engine->GameTime.GameplayTime() + std::chrono::milliseconds {PROCESS_TALK_TIME};

    // Check time of talk
    if (cr->Talk.TalkTime != time_duration {} && _engine->GameTime.GameplayTime() - cr->Talk.StartTime >= cr->Talk.TalkTime) {
        CloseTalk(cr);
        return;
    }

    // Check npc
    const Critter* talker = nullptr;

    if (cr->Talk.Type == TalkType::Critter) {
        talker = _engine->EntityMngr.GetCritter(cr->Talk.CritterId);

        if (talker == nullptr) {
            CloseTalk(cr);
            return;
        }

        if (!talker->IsAlive()) {
            cr->Send_TextMsg(cr, SAY_NETMSG, TextPackName::Game, STR_DIALOG_NPC_NOT_LIFE);
            CloseTalk(cr);
            return;
        }

        // Todo: don't remeber but need check (IsPlaneNoTalk)
    }

    // Check distance
    if (!cr->Talk.IgnoreDistance) {
        ident_t map_id;
        mpos hex;
        uint talk_distance = 0;

        if (cr->Talk.Type == TalkType::Critter) {
            map_id = talker->GetMapId();
            hex = talker->GetHex();
            talk_distance = talker->GetTalkDistance();
            talk_distance = (talk_distance != 0 ? talk_distance : _engine->Settings.TalkDistance) + cr->GetMultihex();
        }
        else if (cr->Talk.Type == TalkType::Hex) {
            map_id = cr->Talk.TalkHexMap;
            hex = cr->Talk.TalkHex;
            talk_distance = _engine->Settings.TalkDistance + cr->GetMultihex();
        }

        if (cr->GetMapId() != map_id || !GeometryHelper::CheckDist(cr->GetHex(), hex, talk_distance)) {
            cr->Send_TextMsg(cr, SAY_NETMSG, TextPackName::Game, STR_DIALOG_DIST_TOO_LONG);
            CloseTalk(cr);
        }
    }
}

void CritterManager::CloseTalk(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (cr->Talk.Type != TalkType::None) {
        Critter* talker = nullptr;

        if (cr->Talk.Type == TalkType::Critter) {
            cr->Talk.Type = TalkType::None;

            talker = _engine->EntityMngr.GetCritter(cr->Talk.CritterId);

            if (talker != nullptr) {
                if (cr->Talk.Barter) {
                    talker->OnBarter.Fire(cr, false, talker->GetBarterCritters());
                    _engine->OnCritterBarter.Fire(cr, talker, false, talker->GetBarterCritters());
                }

                talker->OnTalk.Fire(cr, false, talker->GetTalkingCritters());
                _engine->OnCritterTalk.Fire(cr, talker, false, talker->GetTalkingCritters());
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
