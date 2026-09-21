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

#pragma once

#include "Common.h"

#include "Geometry.h"
#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE

class StaticItem;
class Critter;

// The baked static layer of one ProtoMap, shared by every live instance of that map. It owns the hex field
// outright: callers describe content through the loading API and read it back, but never touch a Field
class StaticMap final
{
public:
    // Passive per-hex aggregate. StaticMap performs every mutation of it, so readers only ever read
    struct Field
    {
        bool MoveBlocked {};
        bool ShootBlocked {};
        bool ScrollBlocked {};
        vector<ptr<StaticItem>> StaticItems {};
        vector<ptr<StaticItem>> TriggerItems {};
    };

    using CritterBillet = pair<ident_t, refcount_ptr<Critter>>;
    using OwnedItemBillet = pair<ident_t, refcount_ptr<StaticItem>>;
    using ItemBillet = pair<ident_t, ptr<StaticItem>>;

    StaticMap() = delete;
    StaticMap(msize map_size, bool static_grid);
    StaticMap(const StaticMap&) = delete;
    StaticMap(StaticMap&&) noexcept = delete;
    auto operator=(const StaticMap&) = delete;
    auto operator=(StaticMap&&) noexcept = delete;
    ~StaticMap() = default;

    [[nodiscard]] auto GetSize() const noexcept -> msize;
    [[nodiscard]] auto GetField(mpos hex) const noexcept -> const Field&;
    [[nodiscard]] auto GetStaticItem(ident_t static_item_id) const noexcept -> nptr<StaticItem>;
    [[nodiscard]] auto HasStaticItem(ident_t static_item_id) const noexcept -> bool;
    [[nodiscard]] auto GetStaticItems() const noexcept -> const_span<ptr<StaticItem>>;
    [[nodiscard]] auto GetCritterBillets() const noexcept -> const_span<CritterBillet>;
    [[nodiscard]] auto GetOwnedItemBillets() const noexcept -> const_span<OwnedItemBillet>;
    [[nodiscard]] auto GetHexItemBillets() const noexcept -> const_span<ItemBillet>;
    [[nodiscard]] auto GetChildItemBillets() const noexcept -> const_span<ItemBillet>;

    // The hexes one static item occupies, so a map instance hiding it knows which fields to rebuild
    void ForEachItemHex(ident_t static_item_id, const function<void(mpos)>& callback) const;
    // A detached copy of one hex field with the listed items left out, keeping the baked scroll block
    auto BuildFieldWithout(mpos hex, const unordered_set<ident_t>& removed_ids) const -> Field;

    void ReserveCritters(size_t critter_count);
    void ReserveItems(size_t item_count);
    void AddCritterBillet(ident_t critter_id, refcount_ptr<Critter> cr);
    void AddOwnedItemBillet(ident_t item_id, refcount_ptr<StaticItem> item);
    void AddStaticItem(ident_t item_id, ptr<StaticItem> item);
    void AddHexItemBillet(ident_t item_id, ptr<StaticItem> item);
    void AddChildItemBillet(ident_t item_id, ptr<StaticItem> item);
    void MarkScrollBlocked(mpos hex);
    void ShrinkToFit();

private:
    static auto CreateHexField(msize map_size, bool static_grid) -> unique_ptr<TwoDimensionalGrid<Field, mpos, msize>>;
    static void ApplyItemToField(ptr<StaticItem> item, ptr<Field> field);

    void ForEachItemHex(ptr<const StaticItem> item, const function<void(mpos)>& callback) const;

    msize _mapSize;
    unique_ptr<TwoDimensionalGrid<Field, mpos, msize>> _hexField;
    vector<CritterBillet> _critterBillets {};
    vector<OwnedItemBillet> _ownedItemBillets {};
    vector<ItemBillet> _hexItemBillets {};
    vector<ItemBillet> _childItemBillets {};
    vector<ptr<StaticItem>> _staticItems {};
    unordered_map<ident_t, ptr<StaticItem>> _staticItemsById {};
};

FO_END_NAMESPACE
