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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Geometry.h"

FO_BEGIN_NAMESPACE();

///@ ExportMethod
FO_SCRIPT_API bool Client_Item_IsVisible(ItemView* self)
{
    const auto* hex_item = dynamic_cast<ItemHexView*>(self);

    if (hex_item == nullptr) {
        throw ScriptException("Item is not on map");
    }

    return hex_item->IsMapSpriteVisible();
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Client_Item_Clone(ItemView* self)
{
    auto cloned_item = self->CreateRefClone();
    cloned_item->AddRef();
    return cloned_item.get();
}

///@ ExportMethod
FO_SCRIPT_API ItemView* Client_Item_Clone(ItemView* self, int32 count)
{
    auto cloned_item = self->CreateRefClone();
    cloned_item->SetCount(count);
    cloned_item->AddRef();
    return cloned_item.get();
}

static void ItemGetMapPos(ItemView* self, mpos& hex)
{
    if (self->GetEngine()->GetCurMap() == nullptr) {
        throw ScriptException("Map is not loaded");
    }

    switch (self->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        const auto* cr = self->GetEngine()->GetCurMap()->GetCritter(self->GetCritterId());
        if (cr == nullptr) {
            throw ScriptException("Invalid critter ownership, critter not found");
        }

        hex = cr->GetHex();
    } break;
    case ItemOwnership::MapHex: {
        hex = self->GetHex();
    } break;
    case ItemOwnership::ItemContainer: {
        if (self->GetId() == self->GetContainerId()) {
            throw ScriptException("Invalid container ownership, crosslinks");
        }

        auto* cont = self->GetEngine()->GetCurMap()->GetItem(self->GetContainerId());
        if (cont == nullptr) {
            throw ScriptException("Invalid container ownership, container not found");
        }

        // Look recursively
        ItemGetMapPos(cont, hex);
    } break;
    default:
        throw ScriptException("Invalid item ownership");
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_GetMapPos(ItemView* self, mpos& hex)
{
    if (self->GetEngine()->GetCurMap() == nullptr) {
        throw ScriptException("Map is not loaded");
    }

    ItemGetMapPos(self, hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Item_IsAnimPlaying(ItemView* self)
{
    if (const auto* hex_item = dynamic_cast<ItemHexView*>(self); hex_item != nullptr) {
        return hex_item->GetAnim()->IsPlaying();
    }

    return false;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_PlayAnim(ItemView* self, hstring animName, bool looped, bool reversed)
{
    if (auto* hex_item = dynamic_cast<ItemHexView*>(self); hex_item != nullptr) {
        hex_item->PlayAnim(animName, looped, reversed);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_StopAnim(ItemView* self)
{
    if (auto* hex_item = dynamic_cast<ItemHexView*>(self); hex_item != nullptr) {
        hex_item->StopAnim();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_SetAnimTime(ItemView* self, float32 normalizedTime)
{
    if (auto* hex_item = dynamic_cast<ItemHexView*>(self); hex_item != nullptr) {
        hex_item->SetAnimTime(normalizedTime);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_SetAnimDir(ItemView* self, uint8 dir)
{
    if (auto* hex_item = dynamic_cast<ItemHexView*>(self); hex_item != nullptr) {
        hex_item->SetAnimDir(dir);
    }
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Item_IsMoving(ItemView* self)
{
    if (const auto* hex_item = dynamic_cast<ItemHexView*>(self); hex_item != nullptr) {
        return hex_item->IsMoving();
    }

    return false;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_MoveToHex(ItemView* self, mpos hex, float32 speed)
{
    if (auto* hex_item = dynamic_cast<ItemHexView*>(self); hex_item != nullptr) {
        hex_item->MoveToHex(hex, speed);
    }
}

///@ ExportMethod
FO_SCRIPT_API vector<ItemView*> Client_Item_GetInnerItems(ItemView* self)
{
    auto& inner_items = self->GetInnerItems();

    vector<ItemView*> items;
    items.reserve(inner_items.size());

    for (auto& item : inner_items) {
        items.emplace_back(item.get());
    }

    return items;
}

///@ ExportMethod
FO_SCRIPT_API uint8 Client_Item_GetAlpha(ItemView* self)
{
    const auto* hex_item = dynamic_cast<ItemHexView*>(self);
    return hex_item != nullptr ? hex_item->GetCurAlpha() : 0xFF;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_SetAlpha(ItemView* self, uint8 alpha)
{
    if (auto* hex_item = dynamic_cast<ItemHexView*>(self); hex_item != nullptr) {
        hex_item->SetTargetAlpha(alpha);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_Finish(ItemView* self)
{
    if (auto* hex_item = dynamic_cast<ItemHexView*>(self); hex_item != nullptr) {
        if (!hex_item->IsFinishing()) {
            hex_item->Finish();
        }
    }
}

FO_END_NAMESPACE();
