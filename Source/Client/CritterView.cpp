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

#include "CritterView.h"
#include "Client.h"
#include "ItemView.h"

FO_BEGIN_NAMESPACE

CritterView::CritterView(ptr<ClientEngine> engine, ident_t id, ptr<const ProtoCritter> proto, nptr<const Properties> props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME).as_ptr(), props ? props : nptr<const Properties> {&proto->GetProperties()}, &proto->GetProperties()),
    EntityWithProto(proto),
    CritterProperties(*GetInitRef())
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

CritterView::~CritterView()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_CONTINUE(_invItems.empty(), "Client critter view has inventory items during destruction", GetId(), _invItems.size());
    FO_VERIFY_AND_CONTINUE(_attachedCritters.empty(), "Client critter view has attached critters during destruction", GetId(), _attachedCritters.size());
}

void CritterView::OnDestroySelf()
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < _invItems.size(); i++) {
        safe_call([&] { _invItems[i]->DestroySelf(); });
    }

    _invItems.clear();
}

void CritterView::SetName(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    _name = name;
}

auto CritterView::IsAttachedCritter(ident_t cr_id) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::ranges::find(_attachedCritters, cr_id) != _attachedCritters.end();
}

void CritterView::SetAttachedCritters(vector<ident_t> attached_critters)
{
    FO_STACK_TRACE_ENTRY();

    _attachedCritters = std::move(attached_critters);
}

auto CritterView::AddMapperInvItem(ident_t id, ptr<const ProtoItem> proto, CritterItemSlot slot, nptr<const Properties> props) -> ptr<ItemView>
{
    FO_STACK_TRACE_ENTRY();

    refcount_ptr<ItemView> item = SafeAlloc::MakeRefCounted<ItemView>(_engine, id, proto, props);
    auto destroy_on_fail = scope_fail([&]() noexcept { safe_call([&] { item->DestroySelf(); }); });

    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(GetId());
    item->SetCritterSlot(slot);

    return AddRawInvItem(item);
}

auto CritterView::AddReceivedInvItem(ident_t id, ptr<const ProtoItem> proto, CritterItemSlot slot, const vector<vector<uint8_t>>& props_data) -> ptr<ItemView>
{
    FO_STACK_TRACE_ENTRY();

    refcount_ptr<ItemView> item = SafeAlloc::MakeRefCounted<ItemView>(_engine, id, proto, nullptr);
    auto destroy_on_fail = scope_fail([&]() noexcept { safe_call([&] { item->DestroySelf(); }); });

    item->RestoreData(props_data);
    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(GetId());
    item->SetCritterSlot(slot);

    return AddRawInvItem(item);
}

auto CritterView::AddRawInvItem(ptr<ItemView> item) -> ptr<ItemView>
{
    FO_VERIFY_AND_THROW(!item->GetStatic(), "Item is static and cannot be attached here");
    FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::CritterInventory, "Item is not owned by critter inventory");
    FO_VERIFY_AND_THROW(item->GetCritterId() == GetId(), "Item belongs to a different critter");

    vec_add_unique_value(_invItems, item.hold_ref());

    return item;
}

void CritterView::DeleteInvItem(ptr<ItemView> item)
{
    FO_STACK_TRACE_ENTRY();

    auto item_ref_holder = item.hold_ref();
    vec_remove_unique_value(_invItems, item_ref_holder);

    item->DestroySelf();
}

void CritterView::DeleteAllInvItems()
{
    FO_STACK_TRACE_ENTRY();

    while (!_invItems.empty()) {
        DeleteInvItem(_invItems.front());
    }
}

auto CritterView::GetInvItem(ident_t item_id) noexcept -> nptr<ItemView>
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < _invItems.size(); i++) {
        auto item = _invItems[i].as_ptr();

        if (item->GetId() == item_id) {
            return item;
        }
    }

    return nullptr;
}

auto CritterView::GetInvItemByPid(hstring item_pid) noexcept -> nptr<ItemView>
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < _invItems.size(); i++) {
        auto item = _invItems[i].as_ptr();

        if (item->GetProtoId() == item_pid) {
            return item;
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

FO_END_NAMESPACE
