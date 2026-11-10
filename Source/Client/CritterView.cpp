// FOnline Engine
// SPDX-License-Identifier: MIT

#include "CritterView.h"

#include "Client.h"
#include "ItemView.h"
#include "MapLoader.h"

FO_BEGIN_NAMESPACE

CritterView::CritterView(ptr<ClientEngine> engine, ident_t id, ptr<const ProtoCritter> proto, nptr<const Properties> props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrar(ENTITY_TYPE_NAME), props ? props : nptr<const Properties> {proto->GetProperties()}, proto->GetProperties()),
    EntityWithProto(proto),
    CritterProperties(*GetInitRef())
{
    auto unregister_on_fail {scope_fail([this]() noexcept {
        safe_call([this]() {
            if (GetId()) {
                _engine->UnregisterEntity(this);
            }
        });
    })};

    _name = strex("{}_{}", proto->GetName(), id);

#if FO_ENABLE_3D
    if (auto layers {GetModelLayers()}; layers.size() != MODEL_LAYERS_COUNT) {
        layers.resize(MODEL_LAYERS_COUNT);
        SetModelLayers(layers);
    }
#endif
}

CritterView::~CritterView()
{
    FO_VERIFY_AND_CONTINUE(_invItems.empty(), "Client critter view has inventory items during destruction", GetId(), _invItems.size());
    FO_VERIFY_AND_CONTINUE(_attachedCritters.empty(), "Client critter view has attached critters during destruction", GetId(), _attachedCritters.size());
}

void CritterView::OnDestroySelf()
{
    for (size_t i {0}; i < _invItems.size(); i++) {
        safe_call([&] { _invItems[i]->DestroySelf(); });
    }

    _invItems.clear();
    _attachedCritters.clear();
}

void CritterView::SetName(string_view name)
{
    _name = name;
}

auto CritterView::IsAttachedCritter(ident_t cr_id) const noexcept -> bool
{
    return std::ranges::find(_attachedCritters, cr_id) != _attachedCritters.end();
}

void CritterView::SetAttachedCritters(vector<ident_t> attached_critters)
{
    _attachedCritters = std::move(attached_critters);
}

auto CritterView::AddMapperInvItem(ident_t id, ptr<const ProtoItem> proto, CritterItemSlot slot, nptr<const Properties> props) -> ptr<ItemView>
{
    FO_TRACE_ZONE(Entity);

    auto item {safe_alloc::make_refcounted<ItemView>(_engine, id, proto, props)};
    auto destroy_on_fail {scope_fail([&]() noexcept { safe_call([&] { item->DestroySelf(); }); })};

    item->SetStatic(false);
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritterId(GetId());
    item->SetCritterSlot(slot);

    MapLoader::ValidateItemProperties(*item->GetProperties(), *proto->GetProperties(), proto->GetName(), id);

    return AddRawInvItem(item);
}

auto CritterView::AddReceivedInvItem(ident_t id, ptr<const ProtoItem> proto, CritterItemSlot slot, const vector<vector<uint8_t>>& props_data) -> ptr<ItemView>
{
    FO_TRACE_ZONE(Entity);

    auto item {safe_alloc::make_refcounted<ItemView>(_engine, id, proto, nullptr)};
    auto destroy_on_fail {scope_fail([&]() noexcept { safe_call([&] { item->DestroySelf(); }); })};

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
    auto item_ref_holder {item.hold_ref()};
    vec_remove_unique_value(_invItems, item_ref_holder);

    item->DestroySelf();
}

void CritterView::DeleteAllInvItems()
{
    while (!_invItems.empty()) {
        DeleteInvItem(_invItems.front());
    }
}

auto CritterView::GetInvItem(ident_t item_id) noexcept -> nptr<ItemView>
{
    for (size_t i {0}; i < _invItems.size(); i++) {
        auto item {_invItems[i].as_ptr()};

        if (item->GetId() == item_id) {
            return item;
        }
    }

    return nullptr;
}

void CritterView::ApplyAuthoritativeItemSlotMove(ident_t item_id, CritterItemSlot previous_slot, CritterItemSlot current_slot)
{
    FO_TRACE_ZONE(Entity);

    auto item = GetInvItem(item_id);

    if (!item || previous_slot == current_slot) {
        return;
    }

    auto self_ref_holder = ptr<CritterView> {this}.hold_ref();
    auto item_ref_holder = item.hold_ref();
    size_t slot_index = static_cast<size_t>(current_slot);
    bool multi_item_slot = slot_index < _engine->Settings->Critter.CritterSlotMultiItem.size() && _engine->Settings->Critter.CritterSlotMultiItem[slot_index];

    if (!multi_item_slot) {
        for (ptr<ItemView> peer : copy_hold_ref(_invItems)) {
            if (peer->IsDestroyed() || peer->GetOwnership() != ItemOwnership::CritterInventory || peer->GetCritterId() != GetId() || GetInvItem(peer->GetId()) != peer) {
                continue;
            }

            if (peer->GetId() != item_id && peer->GetCritterSlot() == current_slot) {
                peer->SetCritterSlot(previous_slot);

                if (IsDestroyed() || item->IsDestroyed() || item->GetOwnership() != ItemOwnership::CritterInventory || item->GetCritterId() != GetId() || GetInvItem(item_id) != item) {
                    return;
                }
            }
        }
    }

    item->SetCritterSlot(current_slot);
}

auto CritterView::GetInvItemByPid(hstring item_pid) noexcept -> nptr<ItemView>
{
    for (size_t i {0}; i < _invItems.size(); i++) {
        auto item {_invItems[i].as_ptr()};

        if (item->GetProtoId() == item_pid) {
            return item;
        }
    }

    return nullptr;
}

auto CritterView::CheckFind(CritterFindType find_type) const noexcept -> bool
{
    if (find_type == CritterFindType::Any) {
        return true;
    }
    if (is_enum_set(find_type, CritterFindType::Players) && !GetControlledByPlayer()) {
        return false;
    }
    if (is_enum_set(find_type, CritterFindType::Npc) && GetControlledByPlayer()) {
        return false;
    }
    if (is_enum_set(find_type, CritterFindType::NonDead) && IsDead()) {
        return false;
    }
    if (is_enum_set(find_type, CritterFindType::Dead) && !IsDead()) {
        return false;
    }

    return true;
}

FO_END_NAMESPACE
