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

#include "Common.h"

#include "Geometry.h"
#include "ScriptSystem.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

static auto ResolveItemMap(ptr<Item> item) -> refcount_nptr<Map>;
static auto ResolveItemMapPosition(ptr<Item> item, mpos& hex) -> refcount_nptr<Map>;
static auto ResolveItemCritter(ptr<Item> item) -> refcount_nptr<Critter>;

template<typename TParent, typename TEntity>
static auto RequireParent(ptr<TEntity> entity, string_view error_message) -> refcount_ptr<TParent>
{
    FO_STACK_TRACE_ENTRY();

    auto parent = entity->template GetParent<TParent>();

    if (!parent) {
        throw ScriptException(error_message);
    }

    return std::move(parent).take_not_null();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Item_SetupScript(ptr<Item> self, ScriptFunc<void, Item*, bool> initFunc)
{
    if (initFunc.IsDelegate()) {
        throw ScriptException("Init function must not be a delegate");
    }

    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc.GetName().first, true)) {
        throw ScriptException("Call init failed", initFunc.GetName().first);
    }

    self->SetInitScript(initFunc.GetName().first);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Item_SetupScriptEx(ptr<Item> self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API ptr<Item> Server_Item_AddItem(ptr<Item> self, hstring pid, int32_t count, any_t stackId = any_t {})
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a container that is being destroyed", self->GetId());
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    auto item = self->GetEngine()->ItemMngr.AddItemContainer(self, pid, count, stackId);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Item> Server_Item_AddItem(ptr<Item> self, ptr<ProtoItem> proto, int32_t count, any_t stackId = any_t {})
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a container that is being destroyed", self->GetId());
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    auto item = self->GetEngine()->ItemMngr.AddItemContainer(self, proto->GetProtoId(), count, stackId);
    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Item_GetItems(ptr<Item> self, any_t stackId = any_t {})
{
    vector<ptr<Item>> items = self->GetInnerItems(stackId);

    return MakeScriptHandleVector<Item>(items);
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Map> Server_Item_GetMap(ptr<Item> self)
{
    auto map = ResolveItemMap(self);

    return ReleaseNullableScriptOwnership(std::move(map));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Map> Server_Item_GetMapPosition(ptr<Item> self, mpos& hex)
{
    auto map = ResolveItemMapPosition(self, hex);

    return ReleaseNullableScriptOwnership(std::move(map));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Critter> Server_Item_GetCritter(ptr<Item> self)
{
    auto cr = ResolveItemCritter(self);

    return ReleaseNullableScriptOwnership(std::move(cr));
}

///@ ExportMethod
FO_SCRIPT_API void Server_Item_RefreshVisibility(ptr<Item> self)
{
    if (self->GetOwnership() == ItemOwnership::MapHex) {
        auto map_holder = RequireParent<Map>(self, "Missing map instance");
        auto map = map_holder.as_ptr();
        map->ChangeViewItem(self);
        map->RecacheHexFlags(self->GetHex());
    }
}

static auto ResolveItemMap(ptr<Item> item) -> refcount_nptr<Map>
{
    FO_STACK_TRACE_ENTRY();

    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto cr = RequireParent<Critter>(item, "Critter ownership, critter not found");
        auto cr_ptr = cr.as_ptr();

        if (!cr_ptr->GetMapId()) {
            return nullptr;
        }

        auto map = RequireParent<Map>(cr_ptr, "Critter ownership, map not found");

        return std::move(map);
    } break;
    case ItemOwnership::MapHex: {
        auto map = RequireParent<Map>(item, "Hex ownership, map not found");

        return std::move(map);
    } break;
    case ItemOwnership::ItemContainer: {
        if (item->GetId() == item->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto cont = RequireParent<Item>(item, "Container ownership, container not found");

        return ResolveItemMap(cont.as_ptr());
    } break;
    default:
        throw ScriptException("Invalid ownership");
    }

    FO_UNREACHABLE_PLACE();
}

static auto ResolveItemMapPosition(ptr<Item> item, mpos& hex) -> refcount_nptr<Map>
{
    FO_STACK_TRACE_ENTRY();

    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto cr = RequireParent<Critter>(item, "Critter ownership, critter not found");
        auto cr_ptr = cr.as_ptr();

        if (!cr_ptr->GetMapId()) {
            hex = {};
            return nullptr;
        }

        auto map = RequireParent<Map>(cr_ptr, "Critter ownership, map not found");

        hex = cr_ptr->GetHex();
        return std::move(map);
    } break;
    case ItemOwnership::MapHex: {
        auto map = RequireParent<Map>(item, "Hex ownership, map not found");

        hex = item->GetHex();
        return std::move(map);
    } break;
    case ItemOwnership::ItemContainer: {
        if (item->GetId() == item->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto cont = RequireParent<Item>(item, "Container ownership, container not found");

        return ResolveItemMapPosition(cont.as_ptr(), hex);
    } break;
    default:
        throw ScriptException("Invalid ownership");
    }

    FO_UNREACHABLE_PLACE();
}

static auto ResolveItemCritter(ptr<Item> item) -> refcount_nptr<Critter>
{
    FO_STACK_TRACE_ENTRY();

    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto cr = RequireParent<Critter>(item, "Critter ownership, critter not found");

        return std::move(cr);
    } break;
    case ItemOwnership::MapHex:
        return nullptr;
    case ItemOwnership::ItemContainer: {
        if (item->GetId() == item->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto cont = RequireParent<Item>(item, "Container ownership, container not found");

        return ResolveItemCritter(cont.as_ptr());
    } break;
    default:
        throw ScriptException("Invalid ownership");
    }

    FO_UNREACHABLE_PLACE();
}

FO_END_NAMESPACE
