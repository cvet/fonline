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
    void SetName(string_view name);
    void SetAttachedCritters(vector<ident_t> attached_critters);

protected:
    void OnDestroySelf() override;

    vector<refcount_ptr<ItemView>> _invItems {};
    vector<ident_t> _attachedCritters {};
};

FO_END_NAMESPACE
