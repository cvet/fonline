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

#pragma once

#include "Common.h"

#include "ClientEntity.h"
#include "EntityProperties.h"
#include "EntityProtos.h"

class ItemView;

class CritterView : public ClientEntity, public EntityWithProto, public CritterProperties
{
public:
    CritterView() = delete;
    CritterView(FOClient* engine, uint id, const ProtoCritter* proto);
    CritterView(const CritterView&) = delete;
    CritterView(CritterView&&) noexcept = delete;
    auto operator=(const CritterView&) = delete;
    auto operator=(CritterView&&) noexcept = delete;
    ~CritterView() override = default;

    [[nodiscard]] auto GetName() const -> string_view override { return _name; }
    [[nodiscard]] auto IsNpc() const -> bool { return !_ownedByPlayer; }
    [[nodiscard]] auto IsOwnedByPlayer() const -> bool { return _ownedByPlayer; }
    [[nodiscard]] auto IsChosen() const -> bool { return _isChosen; }
    [[nodiscard]] auto IsPlayerOffline() const -> bool { return _isPlayerOffline; }
    [[nodiscard]] auto IsAlive() const -> bool { return GetCond() == CritterCondition::Alive; }
    [[nodiscard]] auto IsKnockout() const -> bool { return GetCond() == CritterCondition::Knockout; }
    [[nodiscard]] auto IsDead() const -> bool { return GetCond() == CritterCondition::Dead; }
    [[nodiscard]] auto CheckFind(CritterFindType find_type) const -> bool;
    [[nodiscard]] auto GetItem(uint item_id) -> ItemView*;
    [[nodiscard]] auto GetItemByPid(hstring item_pid) -> ItemView*;
    [[nodiscard]] auto GetItems() -> const vector<ItemView*>&;
    [[nodiscard]] auto GetAnim1() const -> uint;

    virtual void Init();
    virtual void Finish();
    void MarkAsDestroyed() override;
    virtual auto AddItem(uint id, const ProtoItem* proto, uchar slot, const vector<vector<uchar>>& properties_data) -> ItemView*;
    virtual void DeleteItem(ItemView* item, bool animate);
    void DeleteAllItems();
    void SetName(string_view name);
    void SetPlayer(bool is_player, bool is_chosen);
    void SetPlayerOffline(bool is_offline);

protected:
    bool _ownedByPlayer {};
    bool _isPlayerOffline {};
    bool _isChosen {};
    vector<ItemView*> _items {};
};
