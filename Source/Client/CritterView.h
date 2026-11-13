// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ClientEntity.h"
#include "EntityProperties.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

class ItemView;

class CritterView : public ClientEntity, public EntityWithProto, public CritterProperties
{
public:
    CritterView() = delete;
    CritterView(ptr<ClientEngine> engine, ident_t id, ptr<const ProtoCritter> proto, nptr<const Properties> props = nullptr);
    CritterView(const CritterView&) = delete;
    CritterView(CritterView&&) noexcept = delete;
    auto operator=(const CritterView&) = delete;
    auto operator=(CritterView&&) noexcept = delete;
    ~CritterView() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _name; }
    [[nodiscard]] auto IsAlive() const noexcept -> bool { return GetCondition() == CritterCondition::Alive; }
    [[nodiscard]] auto IsKnockout() const noexcept -> bool { return GetCondition() == CritterCondition::Knockout; }
    [[nodiscard]] auto IsDead() const noexcept -> bool { return GetCondition() == CritterCondition::Dead; }
    [[nodiscard]] auto CheckFind(CritterFindType find_type) const noexcept -> bool;
    [[nodiscard]] auto GetInvItem(ident_t item_id) noexcept -> nptr<ItemView>;
    [[nodiscard]] auto GetInvItemByPid(hstring item_pid) noexcept -> nptr<ItemView>;
    [[nodiscard]] auto GetInvItems() const noexcept -> const_span<refcount_ptr<ItemView>> { return _invItems; }
    [[nodiscard]] auto GetInvItems() noexcept -> span<refcount_ptr<ItemView>> { return _invItems; }
    [[nodiscard]] auto HasAttachedCritters() const noexcept -> bool { return !_attachedCritters.empty(); }
    [[nodiscard]] auto GetAttachedCritters() const noexcept -> const_span<ident_t> { return _attachedCritters; }
    [[nodiscard]] auto IsAttachedCritter(ident_t cr_id) const noexcept -> bool;

    auto AddMapperInvItem(ident_t id, ptr<const ProtoItem> proto, CritterItemSlot slot, nptr<const Properties> props) -> ptr<ItemView>;
    auto AddReceivedInvItem(ident_t id, ptr<const ProtoItem> proto, CritterItemSlot slot, const vector<vector<uint8_t>>& props_data) -> ptr<ItemView>;
    auto AddRawInvItem(ptr<ItemView> item) -> ptr<ItemView>;
    void DeleteInvItem(ptr<ItemView> item);
    void DeleteAllInvItems();
    void ApplyAuthoritativeItemSlotMove(ident_t item_id, CritterItemSlot previous_slot, CritterItemSlot current_slot);
    void SetName(string_view name);
    void SetAttachedCritters(vector<ident_t> attached_critters);

protected:
    void OnDestroySelf() override;

    vector<refcount_ptr<ItemView>> _invItems {};
    vector<ident_t> _attachedCritters {};
};

FO_END_NAMESPACE
