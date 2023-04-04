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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "CritterView.h"
#include "Client.h"
#include "ItemView.h"
#include "Timer.h"

CritterView::CritterView(FOClient* engine, ident_t id, const ProtoCritter* proto, const Properties* props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    CritterProperties(GetInitRef())
{
    STACK_TRACE_ENTRY();

    _name = _str("{}_{}", proto->GetName(), id);

#if FO_ENABLE_3D
    if (auto layers = GetModelLayers(); layers.size() != LAYERS3D_COUNT) {
        layers.resize(LAYERS3D_COUNT);
        SetModelLayers(layers);
    }
#endif
}

void CritterView::Init()
{
    STACK_TRACE_ENTRY();
}

void CritterView::Finish()
{
    STACK_TRACE_ENTRY();
}

void CritterView::MarkAsDestroyed()
{
    STACK_TRACE_ENTRY();

    for (auto* item : _invItems) {
        item->MarkAsDestroyed();
        item->Release();
    }
    _invItems.clear();

    Entity::MarkAsDestroying();
    Entity::MarkAsDestroyed();
}

void CritterView::SetName(string_view name)
{
    STACK_TRACE_ENTRY();

    _name = name;
}

void CritterView::SetPlayer(bool is_player, bool is_chosen)
{
    STACK_TRACE_ENTRY();

    _ownedByPlayer = is_player;
    _isChosen = is_chosen;
}

void CritterView::SetPlayerOffline(bool is_offline)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_ownedByPlayer);

    _isPlayerOffline = is_offline;
}

auto CritterView::AddInvItem(ident_t id, const ProtoItem* proto, uint8 slot, const Properties* props) -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* item = new ItemView(_engine, id, proto);

    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(GetId());
    item->SetCritterSlot(slot);

    _invItems.push_back(item);

    std::sort(_invItems.begin(), _invItems.end(), [](const ItemView* l, const ItemView* r) { return l->GetSortValue() < r->GetSortValue(); });

    return item;
}

auto CritterView::AddInvItem(ident_t id, const ProtoItem* proto, uint8 slot, const vector<vector<uint8>>& props_data) -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* item = AddInvItem(id, proto, slot, nullptr);

    item->RestoreData(props_data);

    std::sort(_invItems.begin(), _invItems.end(), [](const ItemView* l, const ItemView* r) { return l->GetSortValue() < r->GetSortValue(); });

    return item;
}

void CritterView::DeleteInvItem(ItemView* item, bool animate)
{
    STACK_TRACE_ENTRY();

    const auto it = std::find(_invItems.begin(), _invItems.end(), item);
    RUNTIME_ASSERT(it != _invItems.end());
    _invItems.erase(it);

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetCritterId(ident_t {});
    item->SetCritterSlot(0);

    item->MarkAsDestroyed();
    item->Release();
}

void CritterView::DeleteAllInvItems()
{
    STACK_TRACE_ENTRY();

    while (!_invItems.empty()) {
        DeleteInvItem(*_invItems.begin(), false);
    }
}

auto CritterView::GetInvItem(ident_t item_id) -> ItemView*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetId() == item_id) {
            return item;
        }
    }
    return nullptr;
}

auto CritterView::GetInvItemByPid(hstring item_pid) -> ItemView*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* item : _invItems) {
        if (item->GetProtoId() == item_pid) {
            return item;
        }
    }
    return nullptr;
}

auto CritterView::GetInvItems() -> const vector<ItemView*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _invItems;
}

auto CritterView::GetConstInvItems() const -> vector<const ItemView*>
{
    STACK_TRACE_ENTRY();

    return vec_cast<const ItemView*>(_invItems);
}

auto CritterView::CheckFind(CritterFindType find_type) const -> bool
{
    STACK_TRACE_ENTRY();

    if (find_type == CritterFindType::Any) {
        return true;
    }
    if (IsEnumSet(find_type, CritterFindType::Players) && IsNpc()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Npc) && !IsNpc()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Alive) && IsDead()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Dead) && !IsDead()) {
        return false;
    }
    return true;
}

auto CritterView::GetAnim1() const -> uint
{
    STACK_TRACE_ENTRY();

    switch (GetCond()) {
    case CritterCondition::Alive:
        return GetAnim1Alive() != 0u ? GetAnim1Alive() : ANIM1_UNARMED;
    case CritterCondition::Knockout:
        return GetAnim1Knockout() != 0u ? GetAnim1Knockout() : ANIM1_UNARMED;
    case CritterCondition::Dead:
        return GetAnim1Dead() != 0u ? GetAnim1Dead() : ANIM1_UNARMED;
    }
    return ANIM1_UNARMED;
}
