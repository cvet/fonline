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

#include "CritterView.h"
#include "Client.h"
#include "ItemView.h"

CritterView::CritterView(FOClient* engine, ident_t id, const ProtoCritter* proto, const Properties* props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    CritterProperties(GetInitRef())
{
    STACK_TRACE_ENTRY();

    _name = strex("{}_{}", proto->GetName(), id);

#if FO_ENABLE_3D
    if (auto layers = GetModelLayers(); layers.size() != MODEL_LAYERS_COUNT) {
        layers.resize(MODEL_LAYERS_COUNT);
        SetModelLayers(layers);
    }
#endif
}

void CritterView::OnDestroySelf()
{
    STACK_TRACE_ENTRY();

    for (auto* item : _invItems) {
        item->DestroySelf();
    }

    _invItems.clear();
}

void CritterView::SetName(string_view name)
{
    STACK_TRACE_ENTRY();

    _name = name;
}

auto CritterView::AddMapperInvItem(ident_t id, const ProtoItem* proto, CritterItemSlot slot, const Properties* props) -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* item = new ItemView(_engine, id, proto, props);

    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(GetId());
    item->SetCritterSlot(slot);

    return AddRawInvItem(item);
}

auto CritterView::AddReceivedInvItem(ident_t id, const ProtoItem* proto, CritterItemSlot slot, const vector<vector<uint8>>& props_data) -> ItemView*
{
    STACK_TRACE_ENTRY();

    auto* item = new ItemView(_engine, id, proto, nullptr);

    item->RestoreData(props_data);
    item->SetIsStatic(false);
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(GetId());
    item->SetCritterSlot(slot);

    return AddRawInvItem(item);
}

auto CritterView::AddRawInvItem(ItemView* item) -> ItemView*
{
    RUNTIME_ASSERT(!item->GetStatic());
    RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::CritterInventory);
    RUNTIME_ASSERT(item->GetCritterId() == GetId());

    vec_add_unique_value(_invItems, item);
    std::stable_sort(_invItems.begin(), _invItems.end(), [](const ItemView* l, const ItemView* r) { return l->GetSortValue() < r->GetSortValue(); });

    return item;
}

void CritterView::DeleteInvItem(ItemView* item, bool animate)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(animate);

    vec_remove_unique_value(_invItems, item);

    item->DestroySelf();
}

void CritterView::DeleteAllInvItems()
{
    STACK_TRACE_ENTRY();

    while (!_invItems.empty()) {
        DeleteInvItem(*_invItems.begin(), false);
    }
}

auto CritterView::GetInvItem(ident_t item_id) noexcept -> ItemView*
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

auto CritterView::GetInvItemByPid(hstring item_pid) noexcept -> ItemView*
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

auto CritterView::GetInvItems() noexcept -> const vector<ItemView*>&
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _invItems;
}

auto CritterView::GetConstInvItems() const -> vector<const ItemView*>
{
    STACK_TRACE_ENTRY();

    return vec_static_cast<const ItemView*>(_invItems);
}

auto CritterView::CheckFind(CritterFindType find_type) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    if (find_type == CritterFindType::Any) {
        return true;
    }
    if (IsEnumSet(find_type, CritterFindType::Players) && !GetControlledByPlayer()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Npc) && GetControlledByPlayer()) {
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

auto CritterView::GetStateAnim() const noexcept -> CritterStateAnim
{
    NO_STACK_TRACE_ENTRY();

    switch (GetCondition()) {
    case CritterCondition::Alive:
        return GetAliveStateAnim() != CritterStateAnim::None ? GetAliveStateAnim() : CritterStateAnim::Unarmed;
    case CritterCondition::Knockout:
        return GetKnockoutStateAnim() != CritterStateAnim::None ? GetKnockoutStateAnim() : CritterStateAnim::Unarmed;
    case CritterCondition::Dead:
        return GetDeadStateAnim() != CritterStateAnim::None ? GetDeadStateAnim() : CritterStateAnim::Unarmed;
    }

    return CritterStateAnim::Unarmed;
}
