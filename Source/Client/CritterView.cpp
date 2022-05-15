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

CritterView::CritterView(FOClient* engine, uint id, const ProtoCritter* proto) : ClientEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME), proto), CritterProperties(GetInitRef())
{
}

void CritterView::AddItem(ItemView* item)
{
    item->SetOwnership(ItemOwnership::CritterInventory);
    item->SetCritId(GetId());

    InvItems.push_back(item);

    std::sort(InvItems.begin(), InvItems.end(), [](ItemView* l, ItemView* r) { return l->GetSortValue() < r->GetSortValue(); });
}

void CritterView::DeleteItem(ItemView* item, bool animate)
{
    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetCritId(0);
    item->SetCritSlot(0);

    const auto it = std::find(InvItems.begin(), InvItems.end(), item);
    RUNTIME_ASSERT(it != InvItems.end());
    InvItems.erase(it);

    item->MarkAsDestroyed();
    item->Release();
}

void CritterView::DeleteAllItems()
{
    while (!InvItems.empty()) {
        DeleteItem(*InvItems.begin(), false);
    }
}

auto CritterView::GetItem(uint item_id) -> ItemView*
{
    NON_CONST_METHOD_HINT();

    for (auto* item : InvItems) {
        if (item->GetId() == item_id) {
            return item;
        }
    }
    return nullptr;
}

auto CritterView::GetItemByPid(hstring item_pid) -> ItemView*
{
    NON_CONST_METHOD_HINT();

    for (auto* item : InvItems) {
        if (item->GetProtoId() == item_pid) {
            return item;
        }
    }
    return nullptr;
}

auto CritterView::CountItemPid(hstring item_pid) const -> uint
{
    uint result = 0;
    for (const auto* item : InvItems) {
        if (item->GetProtoId() == item_pid) {
            result += item->GetCount();
        }
    }
    return result;
}

auto CritterView::CheckFind(CritterFindType find_type) const -> bool
{
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

auto CritterView::IsFree() const -> bool
{
    return _engine->GameTime.GameTick() - _startTick >= _tickCount;
}

void CritterView::TickStart(uint ms)
{
    _tickCount = ms;
    _startTick = _engine->GameTime.GameTick();
}

void CritterView::TickNull()
{
    _tickCount = 0;
}
