//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ServerScripting.h"
#include "StringUtils.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param pid ...
///# param count ...
///# param stackId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Item_AddItem([[maybe_unused]] FOServer* server, Item* self, hash pid, uint count, uint stackId)
{
    if (!server->ProtoMngr.GetProtoItem(pid)) {
        throw ScriptException("Invalid proto '{}' arg.", _str().parseHash(pid));
    }

    return server->ItemMngr.AddItemContainer(self, pid, count > 0 ? count : 1, stackId);
}

///# ...
///# param stackId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Item_GetItems([[maybe_unused]] FOServer* server, Item* self, uint stackId)
{
    return self->ContGetItems(stackId);
}

///# ...
///# param func ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Item_SetScript([[maybe_unused]] FOServer* server, Item* self, const std::function<void(Item*)>& func)
{
    if (func) {
        // if (!self->SetScript(func, true))
        //    throw ScriptException("Script function not found");
    }
    else {
        self->SetScriptId(0);
    }
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Map* Server_Item_GetMapPos([[maybe_unused]] FOServer* server, Item* self, ushort& hx, ushort& hy)
{
    Map* map = nullptr;

    switch (self->GetAccessory()) {
    case ITEM_ACCESSORY_CRITTER: {
        auto* cr = server->CrMngr.GetCritter(self->GetCritId());
        if (!cr) {
            throw ScriptException("Critter accessory, critter not found");
        }

        if (!cr->GetMapId()) {
            hx = cr->GetWorldX();
            hy = cr->GetWorldY();
            return static_cast<Map*>(nullptr);
        }

        map = server->MapMngr.GetMap(cr->GetMapId());
        if (!map) {
            throw ScriptException("Critter accessory, map not found");
        }

        hx = cr->GetHexX();
        hy = cr->GetHexY();
    } break;
    case ITEM_ACCESSORY_HEX: {
        map = server->MapMngr.GetMap(self->GetMapId());
        if (!map) {
            throw ScriptException("Hex accessory, map not found");
        }

        hx = self->GetHexX();
        hy = self->GetHexY();
    } break;
    case ITEM_ACCESSORY_CONTAINER: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Container accessory, crosslink");
        }

        auto* cont = server->ItemMngr.GetItem(self->GetContainerId());
        if (!cont) {
            throw ScriptException("Container accessory, container not found");
        }

        // return Item_GetMapPosition(cont, hx, hy); // Todo: fix ITEM_ACCESSORY_CONTAINER recursion
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
[[maybe_unused]] void Server_Item_Animate([[maybe_unused]] FOServer* server, Item* self, uchar fromFrm, uchar toFrm)
{
    switch (self->GetAccessory()) {
    case ITEM_ACCESSORY_CRITTER: {
        // Critter* cr=CrMngr.GetCrit(self->ACC_CRITTER.Id);
        // if(cr) cr->Send_AnimateItem(self,from_frm,to_frm);
    } break;
    case ITEM_ACCESSORY_HEX: {
        auto* map = server->MapMngr.GetMap(self->GetMapId());
        if (map) {
            map->AnimateItem(self, fromFrm, toFrm);
        }
    } break;
    case ITEM_ACCESSORY_CONTAINER:
        break;
    default:
        throw ScriptException("Unknown accessory");
    }
}

///# ...
///# param cr ...
///# param staticItem ...
///# param param ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Item_CallStaticItemFunction([[maybe_unused]] FOServer* server, Item* self, Critter* cr, Item* staticItem, int param)
{
    if (!self->SceneryScriptFunc) {
        return false;
    }

    return self->SceneryScriptFunc(cr, self, staticItem, param) && self->SceneryScriptFunc.GetResult();
}
