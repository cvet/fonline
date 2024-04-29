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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Server.h"
#include "StringUtils.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Item_SetupScript(Item* self, InitFunc<Item*> initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Item_SetupScriptEx(Item* self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///# ...
///# param pid ...
///# param count ...
///# param stackId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Item_AddItem(Item* self, hstring pid, uint count, ContainerItemStack stackId)
{
    if (!pid || self->GetEngine()->ProtoMngr.GetProtoItem(pid) == nullptr) {
        throw ScriptException("Invalid proto '{}' arg.", pid);
    }

    if (count == 0) {
        return nullptr;
    }

    return self->GetEngine()->ItemMngr.AddItemContainer(self, pid, count, stackId);
}

///# ...
///# param stackId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Item_GetItems(Item* self, ContainerItemStack stackId)
{
    return self->GetInnerItems(stackId);
}

///# ...
///# param hex ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Map* Server_Item_GetMapPosition(Item* self, mpos& hex)
{
    Map* map;

    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        const auto* cr = self->GetEngine()->CrMngr.GetCritter(self->GetCritterId());
        if (cr == nullptr) {
            throw ScriptException("Critter ownership, critter not found");
        }

        if (!cr->GetMapId()) {
            hex = {};
            return nullptr;
        }

        map = self->GetEngine()->MapMngr.GetMap(cr->GetMapId());
        if (map == nullptr) {
            throw ScriptException("Critter ownership, map not found");
        }

        hex = cr->GetHex();
    } break;
    case ItemOwnership::MapHex: {
        map = self->GetEngine()->MapMngr.GetMap(self->GetMapId());
        if (map == nullptr) {
            throw ScriptException("Hex ownership, map not found");
        }

        hex = self->GetHex();
    } break;
    case ItemOwnership::ItemContainer: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto* cont = self->GetEngine()->ItemMngr.GetItem(self->GetContainerId());
        if (cont == nullptr) {
            throw ScriptException("Container ownership, container not found");
        }

        map = Server_Item_GetMapPosition(cont, hex);
    } break;
    default:
        throw ScriptException("Unknown ownership");
    }

    return map;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] upos16 Server_Item_GetWorldPosition(Item* self)
{
    upos16 wpos;

    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        const auto* cr = self->GetEngine()->CrMngr.GetCritter(self->GetCritterId());
        if (cr == nullptr) {
            throw ScriptException("Critter ownership, critter not found");
        }

        wpos = cr->GetWorldPos();
    } break;
    case ItemOwnership::MapHex: {
        Map* map = self->GetEngine()->MapMngr.GetMap(self->GetMapId());
        if (map == nullptr) {
            throw ScriptException("Hex ownership, map not found");
        }

        wpos = map->GetLocation()->GetWorldPos();
    } break;
    case ItemOwnership::ItemContainer: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto* cont = self->GetEngine()->ItemMngr.GetItem(self->GetContainerId());
        if (cont == nullptr) {
            throw ScriptException("Container ownership, container not found");
        }

        wpos = Server_Item_GetWorldPosition(cont);
    } break;
    default:
        throw ScriptException("Unknown ownership");
    }

    return wpos;
}

///# ...
///# param animName ...
///# param looped ...
///# param reversed ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Server_Item_Animate(Item* self, hstring animName, bool looped, bool reversed)
{
    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        if (auto* cr = self->GetEngine()->CrMngr.GetCritter(self->GetCritterId()); cr != nullptr) {
            cr->Send_AnimateItem(self, animName, looped, reversed);
        }
    } break;
    case ItemOwnership::MapHex: {
        if (auto* map = self->GetEngine()->MapMngr.GetMap(self->GetMapId()); map != nullptr) {
            map->AnimateItem(self, animName, looped, reversed);
        }
    } break;
    case ItemOwnership::ItemContainer:
        break;
    default:
        break;
    }
}
