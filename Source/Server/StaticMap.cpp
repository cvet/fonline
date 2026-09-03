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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "StaticMap.h"
#include "Critter.h"
#include "Item.h"

FO_BEGIN_NAMESPACE

StaticMap::StaticMap(msize map_size, bool static_grid) :
    _mapSize {map_size},
    _hexField {CreateHexField(map_size, static_grid)}
{
    FO_STACK_TRACE_ENTRY();
}

auto StaticMap::GetSize() const noexcept -> msize
{
    FO_NO_STACK_TRACE_ENTRY();

    return _mapSize;
}

auto StaticMap::GetField(mpos hex) const noexcept -> const Field&
{
    FO_NO_STACK_TRACE_ENTRY();

    // Reuse the guaranteed shared cell without invoking mutating GetCellForWriting. Runtime-map locks do
    // not serialize this cross-map static grid
    return _hexField->GetCellForReading(hex);
}

auto StaticMap::GetStaticItem(ident_t static_item_id) const noexcept -> nptr<StaticItem>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (auto it = _staticItemsById.find(static_item_id); it != _staticItemsById.end()) {
        return it->second;
    }

    return nullptr;
}

auto StaticMap::HasStaticItem(ident_t static_item_id) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _staticItemsById.count(static_item_id) != 0;
}

auto StaticMap::GetStaticItems() const noexcept -> const_span<ptr<StaticItem>>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _staticItems;
}

auto StaticMap::GetCritterBillets() const noexcept -> const_span<CritterBillet>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _critterBillets;
}

auto StaticMap::GetOwnedItemBillets() const noexcept -> const_span<OwnedItemBillet>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _ownedItemBillets;
}

auto StaticMap::GetHexItemBillets() const noexcept -> const_span<ItemBillet>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _hexItemBillets;
}

auto StaticMap::GetChildItemBillets() const noexcept -> const_span<ItemBillet>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _childItemBillets;
}

void StaticMap::ForEachItemHex(ident_t static_item_id, const function<void(mpos)>& callback) const
{
    FO_STACK_TRACE_ENTRY();

    auto it = _staticItemsById.find(static_item_id);
    FO_STRONG_ASSERT(it != _staticItemsById.end(), "Static item id is missing from the baked map", static_item_id);

    ForEachItemHex(it->second, callback);
}

auto StaticMap::BuildFieldWithout(mpos hex, const unordered_set<ident_t>& removed_ids) const -> Field
{
    FO_STACK_TRACE_ENTRY();

    const auto& shared_field = _hexField->GetCellForReading(hex);
    Field field;

    // Seeded from the baked scroll block, which owns no item and would otherwise be lost here
    field.ScrollBlocked = shared_field.ScrollBlocked;
    field.MoveBlocked = shared_field.ScrollBlocked;

    for (ptr<StaticItem> static_item : shared_field.StaticItems) {
        if (removed_ids.count(static_item->GetId()) == 0) {
            ApplyItemToField(static_item, make_ptr(&field));
        }
    }

    return field;
}

void StaticMap::ReserveCritters(size_t critter_count)
{
    FO_STACK_TRACE_ENTRY();

    _critterBillets.reserve(critter_count);
}

void StaticMap::ReserveItems(size_t item_count)
{
    FO_STACK_TRACE_ENTRY();

    // The record count is the upper bound for each list, because one item lands in exactly one of them
    _ownedItemBillets.reserve(item_count);
    _hexItemBillets.reserve(item_count);
    _childItemBillets.reserve(item_count);
    _staticItems.reserve(item_count);
    _staticItemsById.reserve(item_count);
}

void StaticMap::AddCritterBillet(ident_t critter_id, refcount_ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    _critterBillets.emplace_back(critter_id, std::move(cr));
}

void StaticMap::AddOwnedItemBillet(ident_t item_id, refcount_ptr<StaticItem> item)
{
    FO_STACK_TRACE_ENTRY();

    _ownedItemBillets.emplace_back(item_id, std::move(item));
}

void StaticMap::AddStaticItem(ident_t item_id, ptr<StaticItem> item)
{
    FO_STACK_TRACE_ENTRY();

    _staticItems.emplace_back(item);
    _staticItemsById.emplace(item_id, item);

    ForEachItemHex(item, [this, item](mpos item_hex) {
        auto field = _hexField->GetCellForWriting(item_hex);
        ApplyItemToField(item, field);
    });
}

void StaticMap::AddHexItemBillet(ident_t item_id, ptr<StaticItem> item)
{
    FO_STACK_TRACE_ENTRY();

    _hexItemBillets.emplace_back(item_id, item);
}

void StaticMap::AddChildItemBillet(ident_t item_id, ptr<StaticItem> item)
{
    FO_STACK_TRACE_ENTRY();

    _childItemBillets.emplace_back(item_id, item);
}

void StaticMap::MarkScrollBlocked(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    auto field = _hexField->GetCellForWriting(hex);

    field->ScrollBlocked = true;
    field->MoveBlocked = true;
}

void StaticMap::ShrinkToFit()
{
    FO_STACK_TRACE_ENTRY();

    _critterBillets.shrink_to_fit();
    _ownedItemBillets.shrink_to_fit();
    _hexItemBillets.shrink_to_fit();
    _childItemBillets.shrink_to_fit();
    _staticItems.shrink_to_fit();
}

auto StaticMap::CreateHexField(msize map_size, bool static_grid) -> unique_ptr<TwoDimensionalGrid<Field, mpos, msize>>
{
    FO_STACK_TRACE_ENTRY();

    if (static_grid) {
        return safe_alloc::make_unique<StaticTwoDimensionalGrid<Field, mpos, msize>>(map_size);
    }

    return safe_alloc::make_unique<DynamicTwoDimensionalGrid<Field, mpos, msize>>(map_size);
}

void StaticMap::ApplyItemToField(ptr<StaticItem> item, ptr<Field> field)
{
    FO_STACK_TRACE_ENTRY();

    if (vec_exists(field->StaticItems, item)) {
        return;
    }

    field->StaticItems.reserve(field->StaticItems.size() + 1);
    field->StaticItems.emplace_back(item);

    if (item->GetIsTrigger()) {
        field->TriggerItems.reserve(field->TriggerItems.size() + 1);
        field->TriggerItems.emplace_back(item);
    }

    if (!item->GetNoBlock()) {
        field->MoveBlocked = true;
    }
    if (!item->GetShootThru()) {
        field->ShootBlocked = true;
        field->MoveBlocked = true;
    }
}

void StaticMap::ForEachItemHex(ptr<const StaticItem> item, const function<void(mpos)>& callback) const
{
    FO_STACK_TRACE_ENTRY();

    mpos hex = item->GetHex();
    callback(hex);

    if (item->IsNonEmptyMultihexLines()) {
        GeometryHelper::ForEachMultihexLines(item->GetMultihexLines(), hex, _mapSize, [&](mpos multihex) {
            if (multihex != hex) {
                callback(multihex);
            }
        });
    }

    if (item->IsNonEmptyMultihexMesh()) {
        for (mpos multihex : item->GetMultihexMesh()) {
            if (multihex != hex && _mapSize.is_valid_pos(multihex)) {
                callback(multihex);

                // A mesh hex carries the lines too, so an item drawn as a mesh still blocks along them
                if (item->IsNonEmptyMultihexLines()) {
                    GeometryHelper::ForEachMultihexLines(item->GetMultihexLines(), multihex, _mapSize, [&](mpos line_hex) { callback(line_hex); });
                }
            }
        }
    }
}

FO_END_NAMESPACE
