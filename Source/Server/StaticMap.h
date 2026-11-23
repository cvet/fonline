// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Geometry.h"
#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE

class StaticItem;
class Critter;

// The baked static layer of one ProtoMap shared by every live instance of that map, or the static items of one instance
// It owns the hex field outright: callers describe content through the loading API and read it back
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
    StaticMap(msize map_size, string_view grid_type);
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

    void ForEachItemHex(ident_t static_item_id, const function<void(mpos)>& callback) const;
    auto BuildFieldWithout(mpos hex, const unordered_set<ident_t>& removed_ids) -> Field;
    void MergeFieldInto(mpos hex, ptr<Field> field);
    void ReserveCritters(size_t critter_count);
    void ReserveItems(size_t item_count);
    void AddCritterBillet(ident_t critter_id, refcount_ptr<Critter> cr);
    void AddOwnedItemBillet(ident_t item_id, refcount_ptr<StaticItem> item);
    void AddStaticItem(ident_t item_id, ptr<StaticItem> item);
    void AddHexItemBillet(ident_t item_id, ptr<StaticItem> item);
    void AddChildItemBillet(ident_t item_id, ptr<StaticItem> item);
    void MarkScrollBlocked(mpos hex);
    void ShrinkToFit();
    void RemoveStaticItems(const unordered_set<ident_t>& static_item_ids);

private:
    void ApplyItemToField(ptr<StaticItem> item, ptr<Field> field);
    auto CreateHexField(string_view grid_type) -> unique_ptr<TwoDimensionalGrid<Field, mpos, msize>>;
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
