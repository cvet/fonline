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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

///@ ExportMethod
FO_SCRIPT_API void Server_Item_SetupScript(Item* self, InitFunc<Item*> initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Item_SetupScriptEx(Item* self, hstring initFunc)
{
    if (!ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true)) {
        throw ScriptException("Call init failed", initFunc);
    }

    self->SetInitScript(initFunc);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Item_AddItem(Item* self, hstring pid, uint count, ContainerItemStack stackId)
{
    if (count == 0) {
        return nullptr;
    }

    return self->GetEngine()->ItemMngr.AddItemContainer(self, pid, count, stackId);
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Item_GetItems(Item* self, ContainerItemStack stackId)
{
    return self->GetInnerItems(stackId);
}

///@ ExportMethod
FO_SCRIPT_API Map* Server_Item_GetMap(Item* self)
{
    Map* map;

    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        const auto* cr = self->GetEngine()->EntityMngr.GetCritter(self->GetCritterId());
        if (cr == nullptr) {
            throw ScriptException("Critter ownership, critter not found");
        }

        if (cr->GetMapId() == ident_t {}) {
            return nullptr;
        }

        map = self->GetEngine()->EntityMngr.GetMap(cr->GetMapId());
        if (map == nullptr) {
            throw ScriptException("Critter ownership, map not found");
        }
    } break;
    case ItemOwnership::MapHex: {
        map = self->GetEngine()->EntityMngr.GetMap(self->GetMapId());
        if (map == nullptr) {
            throw ScriptException("Hex ownership, map not found");
        }
    } break;
    case ItemOwnership::ItemContainer: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto* cont = self->GetEngine()->EntityMngr.GetItem(self->GetContainerId());
        if (cont == nullptr) {
            throw ScriptException("Container ownership, container not found");
        }

        map = Server_Item_GetMap(cont);
    } break;
    default:
        throw ScriptException("Unknown ownership");
    }

    return map;
}

///@ ExportMethod
FO_SCRIPT_API Map* Server_Item_GetMapPosition(Item* self, mpos& hex)
{
    Map* map;

    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        const auto* cr = self->GetEngine()->EntityMngr.GetCritter(self->GetCritterId());
        if (cr == nullptr) {
            throw ScriptException("Critter ownership, critter not found");
        }

        if (!cr->GetMapId()) {
            hex = {};
            return nullptr;
        }

        map = self->GetEngine()->EntityMngr.GetMap(cr->GetMapId());
        if (map == nullptr) {
            throw ScriptException("Critter ownership, map not found");
        }

        hex = cr->GetHex();
    } break;
    case ItemOwnership::MapHex: {
        map = self->GetEngine()->EntityMngr.GetMap(self->GetMapId());
        if (map == nullptr) {
            throw ScriptException("Hex ownership, map not found");
        }

        hex = self->GetHex();
    } break;
    case ItemOwnership::ItemContainer: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Container ownership, crosslink");
        }

        auto* cont = self->GetEngine()->EntityMngr.GetItem(self->GetContainerId());
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

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API void Server_Item_Animate(Item* self, hstring animName, bool looped, bool reversed)
{
    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        if (auto* cr = self->GetEngine()->EntityMngr.GetCritter(self->GetCritterId()); cr != nullptr) {
            cr->Send_AnimateItem(self, animName, looped, reversed);
        }
    } break;
    case ItemOwnership::MapHex: {
        if (auto* map = self->GetEngine()->EntityMngr.GetMap(self->GetMapId()); map != nullptr) {
            map->AnimateItem(self, animName, looped, reversed);
        }
    } break;
    case ItemOwnership::ItemContainer:
        break;
    default:
        break;
    }
}
