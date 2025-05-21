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

class ItemView : public ClientEntity, public EntityWithProto, public ItemProperties
{
public:
    ItemView() = delete;
    ItemView(FOClient* engine, ident_t id, const ProtoItem* proto, const Properties* props = nullptr);
    ItemView(const ItemView&) = delete;
    ItemView(ItemView&&) noexcept = delete;
    auto operator=(const ItemView&) = delete;
    auto operator=(ItemView&&) noexcept = delete;
    ~ItemView() override = default;

    [[nodiscard]] auto GetInnerItems() const noexcept -> const vector<refcount_ptr<ItemView>>& { return _innerItems; }
    [[nodiscard]] auto GetInnerItems() noexcept -> vector<refcount_ptr<ItemView>>& { return _innerItems; }
    [[nodiscard]] auto CreateRefClone() -> refcount_ptr<ItemView>;

    auto AddMapperInnerItem(ident_t id, const ProtoItem* proto, const any_t& stack_id, const Properties* props) -> ItemView*;
    auto AddReceivedInnerItem(ident_t id, const ProtoItem* proto, const any_t& stack_id, const vector<vector<uint8>>& props_data) -> ItemView*;
    auto AddRawInnerItem(ItemView* item) -> ItemView*;
    void DestroyInnerItem(ItemView* item);

protected:
    void OnDestroySelf() override;

    vector<refcount_ptr<ItemView>> _innerItems {};
};

FO_END_NAMESPACE();
