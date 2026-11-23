// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Geometry.h"
#include "ScriptSystem.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

static auto ResolveItemMap(ptr<Item> item) -> refcount_nptr<Map>;
static auto ResolveItemMapPosition(ptr<Item> item, mpos& hex) -> refcount_nptr<Map>;
static auto ResolveItemCritter(ptr<Item> item) -> refcount_nptr<Critter>;

// Run the init callback and store its name as the init script
///@ ExportMethod
FO_SCRIPT_API void Server_Item_SetupScript(ptr<Item> self, ScriptFunc<void, ptr<Item>, bool> initFunc)
{
    if (initFunc.IsDelegate()) {
        throw ScriptException("Init function must not be a delegate");
    }

    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc.GetName().first, true)) {
        throw ScriptException("Call init failed", initFunc.GetName().first);
    }

    self->SetInitScript(initFunc.GetName().first);
}

// Run the named init script and store its name
///@ ExportMethod
FO_SCRIPT_API void Server_Item_SetupScriptEx(ptr<Item> self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

// Create an item from a prototype id and add it to a container
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Item> Server_Item_AddItem(ptr<Item> self, hstring pid, any_t stackId = any_t {})
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a container that is being destroyed", self->GetId());
    }
    if (!self->GetEngine()->GetProtoItem(pid)) {
        throw ScriptException("Invalid proto", pid);
    }

    auto item {self->GetEngine()->ItemMngr.CreateItem(pid, nullptr)};
    return self->AddItemToContainer(item, stackId);
}

// Create an item from a prototype descriptor and add it to a container
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Item> Server_Item_AddItem(ptr<Item> self, ptr<ProtoItem> proto, any_t stackId = any_t {})
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a container that is being destroyed", self->GetId());
    }

    auto item {self->GetEngine()->ItemMngr.CreateItem(proto->GetProtoId(), nullptr)};
    return self->AddItemToContainer(item, stackId);
}

// Return inner items of this container, optionally limited to a stack id
///@ ExportMethod
FO_SCRIPT_API FO_PROVIDES_COVER vector<ptr<Item>> Server_Item_GetItems(ptr<Item> self, any_t stackId = any_t {})
{
    vector<ptr<Item>> items {self->GetInnerItems(stackId)};

    return items;
}

// Resolve the map containing this item through its critter or container ownership chain
///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_RETURNS_ANCESTOR nptr<Map> Server_Item_GetMap(ptr<Item> self)
{
    auto map {ResolveItemMap(self)};

    return map ? map.take_not_null().release_ownership() : nullptr;
}

// Resolve the containing map and write the item's map hex through the output argument
///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_RETURNS_ANCESTOR nptr<Map> Server_Item_GetMapPosition(ptr<Item> self, mpos& hex)
{
    auto map {ResolveItemMapPosition(self, hex)};

    return map ? map.take_not_null().release_ownership() : nullptr;
}

// Resolve the critter holding this item through container ownership, or null when map-owned
///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_RETURNS_ANCESTOR nptr<Critter> Server_Item_GetCritter(ptr<Item> self)
{
    auto cr {ResolveItemCritter(self)};

    return cr ? cr.take_not_null().release_ownership() : nullptr;
}

// Refresh map visibility and blocking caches for an item owned by a map hex
///@ ExportMethod
FO_SCRIPT_API void Server_Item_RefreshVisibility(ptr<Item> self)
{
    if (self->GetOwnership() == ItemOwnership::MapHex) {
        auto map {self->GetParent<Map>()};
        FO_VERIFY_AND_THROW(map, "Map ownership without a map instance");
        ValidateEntityAccess(map);
        map->ChangeViewItem(self);
        map->RecacheHexFlags(self->GetHex());
    }
}

template<typename TParent, typename TEntity>
static auto RequireParent(ptr<TEntity> entity, string_view error_message) -> refcount_ptr<TParent>
{
    auto parent {entity->template GetParent<TParent>()};

    if (!parent) {
        throw ScriptException(error_message);
    }

    ValidateEntityAccess(parent);
    return std::move(parent).take_not_null();
}

static auto ResolveItemMap(ptr<Item> item) -> refcount_nptr<Map>
{
    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto cr {RequireParent<Critter>(item, "Critter ownership, critter not found")};

        if (!cr->GetMapId()) {
            return nullptr;
        }

        auto map {RequireParent<Map, Critter>(cr, "Critter ownership, map not found")};
        return map;
    } break;
    case ItemOwnership::MapHex: {
        auto map {RequireParent<Map>(item, "Hex ownership, map not found")};
        return map;
    } break;
    case ItemOwnership::ItemContainer: {
        if (item->GetId() == item->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto cont {RequireParent<Item>(item, "Container ownership, container not found")};
        return ResolveItemMap(cont);
    } break;
    default:
        throw ScriptException("Invalid ownership");
    }
}

static auto ResolveItemMapPosition(ptr<Item> item, mpos& hex) -> refcount_nptr<Map>
{
    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto cr {RequireParent<Critter>(item, "Critter ownership, critter not found")};

        if (!cr->GetMapId()) {
            hex = {};
            return nullptr;
        }

        auto map {RequireParent<Map, Critter>(cr, "Critter ownership, map not found")};
        hex = cr->GetHex();
        return map;
    } break;
    case ItemOwnership::MapHex: {
        auto map {RequireParent<Map>(item, "Hex ownership, map not found")};
        hex = item->GetHex();
        return map;
    } break;
    case ItemOwnership::ItemContainer: {
        if (item->GetId() == item->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto cont {RequireParent<Item>(item, "Container ownership, container not found")};
        return ResolveItemMapPosition(cont, hex);
    } break;
    default:
        throw ScriptException("Invalid ownership");
    }
}

static auto ResolveItemCritter(ptr<Item> item) -> refcount_nptr<Critter>
{
    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto cr {RequireParent<Critter>(item, "Critter ownership, critter not found")};
        return std::move(cr);
    } break;
    case ItemOwnership::MapHex:
        return nullptr;
    case ItemOwnership::ItemContainer: {
        if (item->GetId() == item->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto cont {RequireParent<Item>(item, "Container ownership, container not found")};
        return ResolveItemCritter(cont);
    } break;
    default:
        throw ScriptException("Invalid ownership");
    }
}

FO_END_NAMESPACE
