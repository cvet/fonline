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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
            _engine->ItemMngr.DeleteItem(item);
            item_already->SetCount(item_already->GetCount() + count);
            _engine->OnItemStackChanged.Fire(item_already, +static_cast<int>(count));
            return item_already;
        }
    }

    item->EvaluateSortValue(cr->_invItems);
    cr->SetItem(item);

    auto item_ids = cr->GetItemIds();
    RUNTIME_ASSERT(std::find(item_ids.begin(), item_ids.end(), item->GetId()) == item_ids.end());
    item_ids.emplace_back(item->GetId());
    cr->SetItemIds(std::move(item_ids));

    if (send) {
        cr->Send_AddItem(item);
        if (item->GetCritterSlot() != CritterItemSlot::Inventory) {
            cr->SendAndBroadcast_MoveItem(item, CritterAction::Refresh, CritterItemSlot::Inventory);
        }
    }

    _engine->OnCritterMoveItem.Fire(cr, item, CritterItemSlot::Outside);

    return item;
}

void CritterManager::EraseItemFromCritter(Critter* cr, Item* item, bool send)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(cr);
    RUNTIME_ASSERT(item);

    if (item->GetIsRadio()) {
        _engine->ItemMngr.UnregisterRadio(item);
    }

    const auto it = std::find(cr->_invItems.begin(), cr->_invItems.end(), item);
    RUNTIME_ASSERT(it != cr->_invItems.end());
    cr->_invItems.erase(it);

    item->SetOwnership(ItemOwnership::Nowhere);

    if (send) {
        cr->Send_EraseItem(item);
    }
    if (item->GetCritterSlot() != CritterItemSlot::Inventory) {
        cr->SendAndBroadcast_MoveItem(item, CritterAction::Refresh, CritterItemSlot::Inventory);
    }

    const auto prev_slot = item->GetCritterSlot();

    item->SetCritterId(ident_t {});
    item->SetCritterSlot(CritterItemSlot::Inventory);

    auto item_ids = cr->GetItemIds();
    const auto item_id_it = std::find(item_ids.begin(), item_ids.end(), item->GetId());
    RUNTIME_ASSERT(item_id_it != item_ids.end());
    item_ids.erase(item_id_it);
    cr->SetItemIds(std::move(item_ids));

    _engine->OnCritterMoveItem.Fire(cr, item, prev_slot);
}

auto CritterManager::CreateCritter(hstring proto_id, const Properties* props, Map* map, uint16 hx, uint16 hy, uint8 dir, bool accuracy) -> Critter*
{
    STACK_TRACE_ENTRY();

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

    if (!map->IsHexesMovable(hx, hy, multihex)) {
        if (accuracy) {
            return nullptr;
        }

        int hx_ = hx;
        int hy_ = hy;
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
            if (!map->IsHexesMovable(static_cast<uint16>(hx_ + sx[pos]), static_cast<uint16>(hy_ + sy[pos]), multihex)) {
                continue;
            }
            break;
        }

        hx_ += sx[pos];
        hy_ += sy[pos];
        hx = static_cast<uint16>(hx_);
        hy = static_cast<uint16>(hy_);
        RUNTIME_ASSERT(hx < map->GetWidth());
        RUNTIME_ASSERT(hy < map->GetHeight());
    }

    auto* cr = new Critter(_engine, ident_t {}, proto, props);

    _engine->EntityMngr.RegisterEntity(cr);

    const auto* loc = map->GetLocation();
    RUNTIME_ASSERT(loc);

    if (dir >= GameSettings::MAP_DIR_COUNT) {
        dir = static_cast<uint8>(GenericUtils::Random(0u, GameSettings::MAP_DIR_COUNT - 1u));
    }
    cr->SetWorldX(loc != nullptr ? loc->GetWorldX() : 0);
    cr->SetWorldY(loc != nullptr ? loc->GetWorldY() : 0);
    cr->SetHomeMapId(map->GetId());
    cr->SetHomeMapPid(map->GetProtoId());
    cr->SetHomeHexX(hx);
    cr->SetHomeHexY(hy);
    cr->SetHomeDir(dir);

    _engine->MapMngr.AddCrToMap(cr, map, hx, hy, dir, ident_t {});

    _engine->EntityMngr.CallInit(cr, true);

    _engine->MapMngr.ProcessVisibleItems(cr);

    return cr;
}

void CritterManager::DeleteCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(!cr->GetIsControlledByPlayer());

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

        while (cr->GetMapId() || cr->GlobalMapGroup != nullptr || cr->RealCountInvItems() != 0 || cr->GetIsAttached() || !cr->AttachedCritters.empty()) {
            DeleteInventory(cr);

            if (cr->GetMapId()) {
                auto* map = _engine->MapMngr.GetMap(cr->GetMapId());
                RUNTIME_ASSERT(map);
                _engine->MapMngr.EraseCrFromMap(cr, map);
            }
            else {
                RUNTIME_ASSERT(cr->GlobalMapGroup);
                _engine->MapMngr.EraseCrFromMap(cr, nullptr);
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
    _engine->EntityMngr.UnregisterEntity(cr);

    // Invalidate for use
    cr->MarkAsDestroyed();
    cr->Release();
}

void CritterManager::DeleteInventory(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    while (!cr->_invItems.empty()) {
        _engine->ItemMngr.DeleteItem(*cr->_invItems.begin());
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
        if (!cr->GetIsControlledByPlayer()) {
            non_player_critters.push_back(cr);
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
        if (cr->GetIsControlledByPlayer() && (!on_global_map_only || !cr->GetMapId())) {
            player_critters.push_back(cr);
        }
    }

    return player_critters;
}

auto CritterManager::GetGlobalMapCritters(uint16 wx, uint16 wy, uint radius, CritterFindType find_type) -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& all_critters = _engine->EntityMngr.GetCritters();

    vector<Critter*> critters;
    critters.reserve(all_critters.size());

    for (auto&& [id, cr] : all_critters) {
        if (!cr->GetMapId() && GenericUtils::DistSqrt(cr->GetWorldX(), cr->GetWorldY(), wx, wy) <= radius && cr->CheckFind(find_type)) {
            critters.push_back(cr);
        }
    }

    return critters;
}

auto CritterManager::GetCritter(ident_t cr_id) -> Critter*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _engine->EntityMngr.GetCritter(cr_id);
}

auto CritterManager::GetCritter(ident_t cr_id) const -> const Critter*
{
    STACK_TRACE_ENTRY();

    return const_cast<CritterManager*>(this)->GetCritter(cr_id);
}

auto CritterManager::GetItemByPidInvPriority(Critter* cr, hstring item_pid) -> Item*
{
    STACK_TRACE_ENTRY();

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
        talker = GetCritter(cr->Talk.CritterId);
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
        auto map_id = ident_t {};
        uint16 hx = 0;
        uint16 hy = 0;
        uint talk_distance = 0;
        if (cr->Talk.Type == TalkType::Critter) {
            map_id = talker->GetMapId();
            hx = talker->GetHexX();
            hy = talker->GetHexY();
            talk_distance = talker->GetTalkDistance();
            talk_distance = (talk_distance != 0 ? talk_distance : _engine->Settings.TalkDistance) + cr->GetMultihex();
        }
        else if (cr->Talk.Type == TalkType::Hex) {
            map_id = cr->Talk.TalkHexMap;
            hx = cr->Talk.TalkHexX;
            hy = cr->Talk.TalkHexY;
            talk_distance = _engine->Settings.TalkDistance + cr->GetMultihex();
        }

        if (cr->GetMapId() != map_id || !GeometryHelper::CheckDist(cr->GetHexX(), cr->GetHexY(), hx, hy, talk_distance)) {
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

            talker = GetCritter(cr->Talk.CritterId);
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

auto CritterManager::PlayersInGame() const -> size_t
{
    STACK_TRACE_ENTRY();

    return _engine->EntityMngr.GetPlayers().size();
}

auto CritterManager::CrittersInGame() const -> size_t
{
    STACK_TRACE_ENTRY();

    return _engine->EntityMngr.GetCritters().size();
}
