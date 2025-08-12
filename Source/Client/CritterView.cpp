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

#include "CritterView.h"
#include "Client.h"
#include "ItemView.h"

FO_BEGIN_NAMESPACE();

CritterView::CritterView(FOClient* engine, ident_t id, const ProtoCritter* proto, const Properties* props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    CritterProperties(GetInitRef())
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    for (auto& item : _invItems) {
        item->DestroySelf();
    }

    _invItems.clear();
}

void CritterView::SetName(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    _name = name;
}

auto CritterView::AddMapperInvItem(ident_t id, const ProtoItem* proto, CritterItemSlot slot, const Properties* props) -> ItemView*
{
    FO_STACK_TRACE_ENTRY();

    auto item = SafeAlloc::MakeRefCounted<ItemView>(_engine.get(), id, proto, props);

    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(GetId());
    item->SetCritterSlot(slot);

    return AddRawInvItem(item.get());
}

auto CritterView::AddReceivedInvItem(ident_t id, const ProtoItem* proto, CritterItemSlot slot, const vector<vector<uint8>>& props_data) -> ItemView*
{
    FO_STACK_TRACE_ENTRY();

    auto item = SafeAlloc::MakeRefCounted<ItemView>(_engine.get(), id, proto, nullptr);

    item->RestoreData(props_data);
    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(GetId());
    item->SetCritterSlot(slot);

    return AddRawInvItem(item.get());
}

auto CritterView::AddRawInvItem(ItemView* item) -> ItemView*
{
    FO_RUNTIME_ASSERT(!item->GetStatic());
    FO_RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::CritterInventory);
    FO_RUNTIME_ASSERT(item->GetCritterId() == GetId());

    vec_add_unique_value(_invItems, item);

    return item;
}

void CritterView::DeleteInvItem(ItemView* item)
{
    FO_STACK_TRACE_ENTRY();

    refcount_ptr item_ref_holder = item;
    vec_remove_unique_value(_invItems, item);

    item->DestroySelf();
}

void CritterView::DeleteAllInvItems()
{
    FO_STACK_TRACE_ENTRY();

    while (!_invItems.empty()) {
        DeleteInvItem(_invItems.front().get());
    }
}

auto CritterView::GetInvItem(ident_t item_id) noexcept -> ItemView*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto& item : _invItems) {
        if (item->GetId() == item_id) {
            return item.get();
        }
    }

    return nullptr;
}

auto CritterView::GetInvItemByPid(hstring item_pid) noexcept -> ItemView*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    for (auto& item : _invItems) {
        if (item->GetProtoId() == item_pid) {
            return item.get();
        }
    }

    return nullptr;
}

auto CritterView::CheckFind(CritterFindType find_type) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (find_type == CritterFindType::Any) {
        return true;
    }
    if (IsEnumSet(find_type, CritterFindType::Players) && !GetControlledByPlayer()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Npc) && GetControlledByPlayer()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::NonDead) && IsDead()) {
        return false;
    }
    if (IsEnumSet(find_type, CritterFindType::Dead) && !IsDead()) {
        return false;
    }

    return true;
}

FO_END_NAMESPACE();
