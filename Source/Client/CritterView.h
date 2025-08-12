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

#pragma once

#include "Common.h"

#include "ClientEntity.h"
#include "EntityProperties.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE();

class ItemView;

class CritterView : public ClientEntity, public EntityWithProto, public CritterProperties
{
public:
    CritterView() = delete;
    CritterView(FOClient* engine, ident_t id, const ProtoCritter* proto, const Properties* props = nullptr);
    CritterView(const CritterView&) = delete;
    CritterView(CritterView&&) noexcept = delete;
    auto operator=(const CritterView&) = delete;
    auto operator=(CritterView&&) noexcept = delete;
    ~CritterView() override = default;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _name; }
    [[nodiscard]] auto IsAlive() const noexcept -> bool { return GetCondition() == CritterCondition::Alive; }
    [[nodiscard]] auto IsKnockout() const noexcept -> bool { return GetCondition() == CritterCondition::Knockout; }
    [[nodiscard]] auto IsDead() const noexcept -> bool { return GetCondition() == CritterCondition::Dead; }
    [[nodiscard]] auto CheckFind(CritterFindType find_type) const noexcept -> bool;
    [[nodiscard]] auto GetInvItem(ident_t item_id) noexcept -> ItemView*;
    [[nodiscard]] auto GetInvItemByPid(hstring item_pid) noexcept -> ItemView*;
    [[nodiscard]] auto GetInvItems() const noexcept -> const vector<refcount_ptr<ItemView>>& { return _invItems; }
    [[nodiscard]] auto GetInvItems() noexcept -> vector<refcount_ptr<ItemView>>& { return _invItems; }

    auto AddMapperInvItem(ident_t id, const ProtoItem* proto, CritterItemSlot slot, const Properties* props) -> ItemView*;
    auto AddReceivedInvItem(ident_t id, const ProtoItem* proto, CritterItemSlot slot, const vector<vector<uint8>>& props_data) -> ItemView*;
    auto AddRawInvItem(ItemView* item) -> ItemView*;
    void DeleteInvItem(ItemView* item);
    void DeleteAllInvItems();
    void SetName(string_view name);

    vector<ident_t> AttachedCritters {}; // Todo: incapsulate AttachedCritters

protected:
    void OnDestroySelf() override;

    vector<refcount_ptr<ItemView>> _invItems {};
};

FO_END_NAMESPACE();
