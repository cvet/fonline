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
#include "Server.h"

FO_BEGIN_NAMESPACE

///@ ExportMethod
FO_SCRIPT_API void Server_Item_SetupScript(Item* self, ScriptFunc<void, Item*, bool> initFunc)
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
FO_SCRIPT_API void Server_Item_SetupScriptEx(Item* self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine(), self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Item_AddItem(Item* self, hstring pid, int32_t count, any_t stackId = any_t {})
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a container that is being destroyed", self->GetId());
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    return self->GetEngine()->ItemMngr.AddItemContainer(self, pid, count, stackId);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Item_AddItem(Item* self, ProtoItem* proto, int32_t count, any_t stackId = any_t {})
{
    if (self->IsDestroying()) {
        throw ScriptException("Cannot add an item to a container that is being destroyed", self->GetId());
    }
    if (count <= 0) {
        throw ScriptException("Count arg must be positive", count);
    }

    return self->GetEngine()->ItemMngr.AddItemContainer(self, proto->GetProtoId(), count, stackId);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Item_GetItems(Item* self, any_t stackId = any_t {})
{
    return self->GetInnerItems(stackId);
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE Map* Server_Item_GetMap(Item* self)
{
    refcount_ptr<Map> map;

    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto cr = self->GetParent<Critter>();

        if (cr == nullptr) {
            throw ScriptException("Critter ownership, critter not found");
        }

        if (!cr->GetMapId()) {
            return nullptr;
        }

        map = cr->GetParent<Map>();

        if (!map) {
            throw ScriptException("Critter ownership, map not found");
        }
    } break;
    case ItemOwnership::MapHex: {
        map = self->GetParent<Map>();

        if (!map) {
            throw ScriptException("Hex ownership, map not found");
        }
    } break;
    case ItemOwnership::ItemContainer: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto cont = self->GetParent<Item>();

        if (!cont) {
            throw ScriptException("Container ownership, container not found");
        }

        map = Server_Item_GetMap(cont.get());
    } break;
    default:
        throw ScriptException("Invalid ownership");
    }

    return map.release_ownership();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE Map* Server_Item_GetMapPosition(Item* self, mpos& hex)
{
    refcount_ptr<Map> map;

    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto cr = self->GetParent<Critter>();

        if (cr == nullptr) {
            throw ScriptException("Critter ownership, critter not found");
        }

        if (!cr->GetMapId()) {
            hex = {};
            return nullptr;
        }

        map = cr->GetParent<Map>();

        if (map == nullptr) {
            throw ScriptException("Critter ownership, map not found");
        }

        hex = cr->GetHex();
    } break;
    case ItemOwnership::MapHex: {
        map = self->GetParent<Map>();

        if (!map) {
            throw ScriptException("Hex ownership, map not found");
        }

        hex = self->GetHex();
    } break;
    case ItemOwnership::ItemContainer: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto cont = self->GetParent<Item>();

        if (!cont) {
            throw ScriptException("Container ownership, container not found");
        }

        map = Server_Item_GetMapPosition(cont.get(), hex);
    } break;
    default:
        throw ScriptException("Invalid ownership");
    }

    return map.release_ownership();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE Critter* Server_Item_GetCritter(Item* self)
{
    refcount_ptr<Critter> cr;

    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        cr = self->GetParent<Critter>();

        if (!cr) {
            throw ScriptException("Critter ownership, critter not found");
        }
    } break;
    case ItemOwnership::MapHex: {
        cr = nullptr;
    } break;
    case ItemOwnership::ItemContainer: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto cont = self->GetParent<Item>();

        if (!cont) {
            throw ScriptException("Container ownership, container not found");
        }

        cr = Server_Item_GetCritter(cont.get());
    } break;
    default:
        throw ScriptException("Invalid ownership");
    }

    return cr.release_ownership();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Item_RefreshVisibility(Item* self)
{
    if (self->GetOwnership() == ItemOwnership::MapHex) {
        auto map = self->GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        map->ChangeViewItem(self);
        map->RecacheHexFlags(self->GetHex());
    }
}

FO_END_NAMESPACE
