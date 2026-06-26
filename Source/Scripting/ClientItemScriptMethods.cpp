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

#include "Client.h"
#include "Geometry.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

[[maybe_unused]] static auto AsHexItem(nptr<ItemView> item) noexcept -> nptr<ItemHexView>
{
    return item.dyn_cast<ItemHexView>();
}

[[maybe_unused]] static auto AsHexItem(ptr<ItemView> item) noexcept -> nptr<ItemHexView>
{
    return item.dyn_cast<ItemHexView>();
}

[[maybe_unused]] static auto AsHexItem(nptr<const ItemView> item) noexcept -> nptr<const ItemHexView>
{
    return item.dyn_cast<const ItemHexView>();
}

[[maybe_unused]] static auto AsHexItem(ptr<const ItemView> item) noexcept -> nptr<const ItemHexView>
{
    return item.dyn_cast<const ItemHexView>();
}

static auto ReturnScriptItemView(ptr<ItemView> item) noexcept -> ItemView*
{
    FO_NO_STACK_TRACE_ENTRY();

    return item.get_no_const();
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Item_IsVisible(ptr<ItemView> self)
{
    auto nullable_hex_item = AsHexItem(self);

    if (!nullable_hex_item) {
        throw ScriptException("Item is not on map");
    }
    auto hex_item = nullable_hex_item.as_ptr();

    return hex_item->IsMapSpriteVisible();
}

///@ ExportMethod
FO_SCRIPT_API ipos32 Client_Item_GetSpriteOffset(ptr<ItemView> self)
{
    auto nullable_hex_item = AsHexItem(self);

    if (!nullable_hex_item) {
        throw ScriptException("Item is not on map");
    }
    auto hex_item = nullable_hex_item.as_ptr();

    return hex_item->GetSpriteOffset();
}

///@ ExportMethod
FO_SCRIPT_API ptr<ItemView> Client_Item_Clone(ptr<ItemView> self)
{
    auto cloned_item = self->CreateRefClone();
    cloned_item->AddRef();
    return ReturnScriptItemView(cloned_item.as_ptr());
}

///@ ExportMethod
FO_SCRIPT_API ptr<ItemView> Client_Item_Clone(ptr<ItemView> self, int32_t count)
{
    auto cloned_item = self->CreateRefClone();
    cloned_item->SetCount(count);
    cloned_item->AddRef();
    return ReturnScriptItemView(cloned_item.as_ptr());
}

static void ItemGetMapPos(ptr<ItemView> item, mpos& hex)
{
    FO_STACK_TRACE_ENTRY();

    auto nullable_map = item->GetEngine()->GetCurMap();

    if (!nullable_map) {
        throw ScriptException("Map is not loaded");
    }
    auto map = nullable_map.as_ptr();

    switch (item->GetOwnership()) {
    case ItemOwnership::CritterInventory: {
        auto nullable_cr = map->GetCritter(item->GetCritterId());
        if (!nullable_cr) {
            throw ScriptException("Invalid critter ownership, critter not found");
        }
        auto cr = nullable_cr.as_ptr();

        hex = cr->GetHex();
    } break;
    case ItemOwnership::MapHex: {
        hex = item->GetHex();
    } break;
    case ItemOwnership::ItemContainer: {
        if (item->GetId() == item->GetContainerId()) {
            throw ScriptException("Invalid container ownership, crosslinks");
        }

        auto nullable_cont = map->GetItem(item->GetContainerId());
        if (!nullable_cont) {
            throw ScriptException("Invalid container ownership, container not found");
        }

        // Look recursively
        auto cont = nullable_cont.as_ptr();
        ItemGetMapPos(cont, hex);
    } break;
    default:
        throw ScriptException("Invalid item ownership");
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_GetMapPos(ptr<ItemView> self, mpos& hex)
{
    if (!self->GetEngine()->GetCurMap()) {
        throw ScriptException("Map is not loaded");
    }

    ItemGetMapPos(self, hex);
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Item_IsAnimPlaying(ptr<ItemView> self)
{
    if (nptr<const ItemHexView> nullable_hex_item = AsHexItem(self); nullable_hex_item) {
        auto hex_item = nullable_hex_item.as_ptr();
        return hex_item->GetAnim()->IsPlaying();
    }

    return false;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_PlayAnim(ptr<ItemView> self, hstring animName, bool looped, bool reversed)
{
    if (nptr<ItemHexView> nullable_hex_item = AsHexItem(self); nullable_hex_item) {
        auto hex_item = nullable_hex_item.as_ptr();
        hex_item->PlayAnim(animName, looped, reversed);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_StopAnim(ptr<ItemView> self)
{
    if (nptr<ItemHexView> nullable_hex_item = AsHexItem(self); nullable_hex_item) {
        auto hex_item = nullable_hex_item.as_ptr();
        hex_item->StopAnim();
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_SetAnimTime(ptr<ItemView> self, float32_t normalizedTime)
{
    if (nptr<ItemHexView> nullable_hex_item = AsHexItem(self); nullable_hex_item) {
        auto hex_item = nullable_hex_item.as_ptr();
        hex_item->SetAnimTime(normalizedTime);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_SetAnimDir(ptr<ItemView> self, mdir dir)
{
    if (nptr<ItemHexView> nullable_hex_item = AsHexItem(self); nullable_hex_item) {
        auto hex_item = nullable_hex_item.as_ptr();
        hex_item->SetAnimDir(dir);
    }
}

///@ ExportMethod
FO_SCRIPT_API bool Client_Item_IsMoving(ptr<ItemView> self)
{
    if (nptr<const ItemHexView> nullable_hex_item = AsHexItem(self); nullable_hex_item) {
        auto hex_item = nullable_hex_item.as_ptr();
        return hex_item->IsMoving();
    }

    return false;
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_MoveToHex(ptr<ItemView> self, mpos hex, float32_t speed)
{
    if (nptr<ItemHexView> nullable_hex_item = AsHexItem(self); nullable_hex_item) {
        auto hex_item = nullable_hex_item.as_ptr();
        hex_item->MoveToHex(hex, speed);
    }
}

///@ ExportMethod
FO_SCRIPT_API vector<ItemView*> Client_Item_GetInnerItems(ptr<ItemView> self)
{
    span<refcount_ptr<ItemView>> inner_items = self->GetInnerItems();

    vector<ptr<ItemView>> items;
    items.reserve(inner_items.size());

    for (size_t i = 0; i < inner_items.size(); i++) {
        auto item = inner_items[i].as_ptr();
        items.emplace_back(item);
    }

    return MakeScriptHandleVector<ItemView>(items);
}

///@ ExportMethod
FO_SCRIPT_API uint8_t Client_Item_GetAlpha(ptr<ItemView> self)
{
    auto nullable_hex_item = AsHexItem(self);
    if (!nullable_hex_item) {
        return 0xFF;
    }

    auto hex_item = nullable_hex_item.as_ptr();
    return hex_item->GetCurAlpha();
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_SetAlpha(ptr<ItemView> self, uint8_t alpha)
{
    if (nptr<ItemHexView> nullable_hex_item = AsHexItem(self); nullable_hex_item) {
        auto hex_item = nullable_hex_item.as_ptr();
        hex_item->SetTargetAlpha(alpha);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Client_Item_Finish(ptr<ItemView> self)
{
    if (nptr<ItemHexView> nullable_hex_item = AsHexItem(self); nullable_hex_item) {
        auto hex_item = nullable_hex_item.as_ptr();
        if (!hex_item->IsFinishing()) {
            hex_item->Finish();
        }
    }
}

FO_END_NAMESPACE
