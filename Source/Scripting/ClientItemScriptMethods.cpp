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

#include "Client.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Client_Item_Clone(ItemView* self)
{
    return self->CreateRefClone();
}

///# ...
///# param count ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemView* Client_Item_Clone(ItemView* self, uint count)
{
    auto* cloned_item = self->CreateRefClone();
    cloned_item->SetCount(count);
    return cloned_item;
}

///# ...
///# param hx ...
///# param hy ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] bool Client_Item_GetMapPos(ItemView* self, ushort& hx, ushort& hy)
{
    if (self->GetEngine()->CurMap == nullptr) {
        throw ScriptException("Map is not loaded");
    }

    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        const auto* cr = self->GetEngine()->CurMap->GetCritter(self->GetCritterId());
        if (!cr) {
            throw ScriptException("CritterCl accessory, CritterCl not found");
        }
        hx = cr->GetHexX();
        hy = cr->GetHexY();
    } break;
    case ItemOwnership::MapHex: {
        hx = self->GetHexX();
        hy = self->GetHexY();
    } break;
    case ItemOwnership::ItemContainer: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Container accessory, crosslinks");
        }

        ItemView* cont = self->GetEngine()->CurMap->GetItem(self->GetContainerId());
        if (!cont) {
            throw ScriptException("Container accessory, container not found");
        }

        // return Item_GetMapPosition(cont, hx, hy); // Todo: solve recursion in GetMapPos
        throw NotImplementedException(LINE_STR);
    } break;
    default:
        throw ScriptException("Unknown accessory");
    }

    return true;
}

///# ...
///# param fromFrame ...
///# param toFrame ...
///@ ExportMethod
[[maybe_unused]] void Client_Item_Animate(ItemView* self, uint fromFrame, uint toFrame)
{
    if (auto* hex_item = dynamic_cast<ItemHexView*>(self); hex_item != nullptr) {
        hex_item->SetAnim(fromFrame, toFrame);
    }
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<ItemView*> Client_Item_GetInnerItems(ItemView* self)
{
    return self->GetInnerItems();
}
