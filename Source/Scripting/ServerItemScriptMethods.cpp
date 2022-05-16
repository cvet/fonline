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

#include "Common.h"

#include "Server.h"
#include "StringUtils.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Item_SetupScript(Item* self, InitFunc<Item*> initFunc)
{
    ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true);
    self->SetInitScript(initFunc);
}

///# ...
///# param initFunc ...
///@ ExportMethod
[[maybe_unused]] void Server_Item_SetupScriptEx(Item* self, hstring initFunc)
{
    ScriptHelpers::CallInitScript(self->GetEngine()->ScriptSys, self, initFunc, true);
    self->SetInitScript(initFunc);
}

///# ...
///# param pid ...
///# param count ...
///# param stackId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Item_AddItem(Item* self, hstring pid, uint count, uint stackId)
{
    if (!self->GetEngine()->ProtoMngr.GetProtoItem(pid)) {
        throw ScriptException("Invalid proto '{}' arg.", pid);
    }

    return self->GetEngine()->ItemMngr.AddItemContainer(self, pid, count > 0 ? count : 1, stackId);
}

///# ...
///# param stackId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Item_GetItems(Item* self, uint stackId)
{
    return self->ContGetItems(stackId);
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Map* Server_Item_GetMapPosition(Item* self, ushort& hx, ushort& hy)
{
    Map* map;

    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto* cr = self->GetEngine()->CrMngr.GetCritter(self->GetCritId());
        if (!cr) {
            throw ScriptException("Critter accessory, critter not found");
        }

        if (!cr->GetMapId()) {
            hx = cr->GetWorldX();
            hy = cr->GetWorldY();
            return static_cast<Map*>(nullptr);
        }

        map = self->GetEngine()->MapMngr.GetMap(cr->GetMapId());
        if (!map) {
            throw ScriptException("Critter accessory, map not found");
        }

        hx = cr->GetHexX();
        hy = cr->GetHexY();
    } break;
    case ItemOwnership::MapHex: {
        map = self->GetEngine()->MapMngr.GetMap(self->GetMapId());
        if (!map) {
            throw ScriptException("Hex accessory, map not found");
        }

        hx = self->GetHexX();
        hy = self->GetHexY();
    } break;
    case ItemOwnership::ItemContainer: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Container accessory, crosslink");
        }

        auto* cont = self->GetEngine()->ItemMngr.GetItem(self->GetContainerId());
        if (!cont) {
            throw ScriptException("Container accessory, container not found");
        }

        map = Server_Item_GetMapPosition(cont, hx, hy);
    } break;
    default:
        throw ScriptException("Unknown accessory");
    }

    return map;
}

///# ...
///# param fromFrm ...
///# param toFrm ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] void Server_Item_Animate(Item* self, uchar fromFrm, uchar toFrm)
{
    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        // Critter* cr=CrMngr.GetCrit(self->ACC_CRITTER.Id);
        // if(cr) cr->Send_AnimateItem(self,from_frm,to_frm);
    } break;
    case ItemOwnership::MapHex: {
        if (auto* map = self->GetEngine()->MapMngr.GetMap(self->GetMapId()); map != nullptr) {
            map->AnimateItem(self, fromFrm, toFrm);
        }
    } break;
    case ItemOwnership::ItemContainer:
        break;
    default:
        throw ScriptException("Unknown accessory");
    }
}
