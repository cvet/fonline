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

#include "Item.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "Log.h"
#include "MapManager.h"
#include "ProtoManager.h"
#include "StringUtils.h"
#include "Testing.h"

PROPERTIES_IMPL(Item);
CLASS_PROPERTY_IMPL(Item, PicMap);
CLASS_PROPERTY_IMPL(Item, PicInv);
CLASS_PROPERTY_IMPL(Item, OffsetX);
CLASS_PROPERTY_IMPL(Item, OffsetY);
CLASS_PROPERTY_IMPL(Item, LightIntensity);
CLASS_PROPERTY_IMPL(Item, LightDistance);
CLASS_PROPERTY_IMPL(Item, LightFlags);
CLASS_PROPERTY_IMPL(Item, LightColor);
CLASS_PROPERTY_IMPL(Item, Stackable);
CLASS_PROPERTY_IMPL(Item, Opened);
CLASS_PROPERTY_IMPL(Item, Corner);
CLASS_PROPERTY_IMPL(Item, Slot);
CLASS_PROPERTY_IMPL(Item, DisableEgg);
CLASS_PROPERTY_IMPL(Item, AnimWaitBase);
CLASS_PROPERTY_IMPL(Item, AnimWaitRndMin);
CLASS_PROPERTY_IMPL(Item, AnimWaitRndMax);
CLASS_PROPERTY_IMPL(Item, AnimStay0);
CLASS_PROPERTY_IMPL(Item, AnimStay1);
CLASS_PROPERTY_IMPL(Item, AnimShow0);
CLASS_PROPERTY_IMPL(Item, AnimShow1);
CLASS_PROPERTY_IMPL(Item, AnimHide0);
CLASS_PROPERTY_IMPL(Item, AnimHide1);
CLASS_PROPERTY_IMPL(Item, SpriteCut);
CLASS_PROPERTY_IMPL(Item, DrawOrderOffsetHexY);
CLASS_PROPERTY_IMPL(Item, BlockLines);
CLASS_PROPERTY_IMPL(Item, ScriptId);
CLASS_PROPERTY_IMPL(Item, Accessory);
CLASS_PROPERTY_IMPL(Item, MapId);
CLASS_PROPERTY_IMPL(Item, HexX);
CLASS_PROPERTY_IMPL(Item, HexY);
CLASS_PROPERTY_IMPL(Item, CritId);
CLASS_PROPERTY_IMPL(Item, CritSlot);
CLASS_PROPERTY_IMPL(Item, ContainerId);
CLASS_PROPERTY_IMPL(Item, ContainerStack);
CLASS_PROPERTY_IMPL(Item, IsStatic);
CLASS_PROPERTY_IMPL(Item, IsScenery);
CLASS_PROPERTY_IMPL(Item, IsWall);
CLASS_PROPERTY_IMPL(Item, IsCanOpen);
CLASS_PROPERTY_IMPL(Item, IsScrollBlock);
CLASS_PROPERTY_IMPL(Item, IsHidden);
CLASS_PROPERTY_IMPL(Item, IsHiddenPicture);
CLASS_PROPERTY_IMPL(Item, IsHiddenInStatic);
CLASS_PROPERTY_IMPL(Item, IsFlat);
CLASS_PROPERTY_IMPL(Item, IsNoBlock);
CLASS_PROPERTY_IMPL(Item, IsShootThru);
CLASS_PROPERTY_IMPL(Item, IsLightThru);
CLASS_PROPERTY_IMPL(Item, IsAlwaysView);
CLASS_PROPERTY_IMPL(Item, IsBadItem);
CLASS_PROPERTY_IMPL(Item, IsNoHighlight);
CLASS_PROPERTY_IMPL(Item, IsShowAnim);
CLASS_PROPERTY_IMPL(Item, IsShowAnimExt);
CLASS_PROPERTY_IMPL(Item, IsLight);
CLASS_PROPERTY_IMPL(Item, IsGeck);
CLASS_PROPERTY_IMPL(Item, IsTrap);
CLASS_PROPERTY_IMPL(Item, IsTrigger);
CLASS_PROPERTY_IMPL(Item, IsNoLightInfluence);
CLASS_PROPERTY_IMPL(Item, IsGag);
CLASS_PROPERTY_IMPL(Item, IsColorize);
CLASS_PROPERTY_IMPL(Item, IsColorizeInv);
CLASS_PROPERTY_IMPL(Item, IsCanTalk);
CLASS_PROPERTY_IMPL(Item, IsRadio);
CLASS_PROPERTY_IMPL(Item, SortValue);
CLASS_PROPERTY_IMPL(Item, Mode);
CLASS_PROPERTY_IMPL(Item, Count);
CLASS_PROPERTY_IMPL(Item, TrapValue);
CLASS_PROPERTY_IMPL(Item, RadioChannel);
CLASS_PROPERTY_IMPL(Item, RadioFlags);
CLASS_PROPERTY_IMPL(Item, RadioBroadcastSend);
CLASS_PROPERTY_IMPL(Item, RadioBroadcastRecv);
CLASS_PROPERTY_IMPL(Item, FlyEffectSpeed);

Item::Item(uint id, ProtoItem* proto, ScriptSystem& script_sys) :
    Entity(id, EntityType::Item, PropertiesRegistrator, proto), scriptSys {script_sys}
{
    RUNTIME_ASSERT(proto);
    RUNTIME_ASSERT(GetCount() > 0);
}

void Item::SetProto(ProtoItem* proto)
{
    RUNTIME_ASSERT(proto);
    proto->AddRef();
    Proto->Release();
    Proto = proto;
    Props = proto->Props;
}

bool Item::SetScript(asIScriptFunction* func, bool first_time)
{
    if (func)
    {
        hash func_num = scriptSys.BindScriptFuncNumByFunc(func);
        if (!func_num)
        {
            WriteLog("Script bind fail, item '{}'.\n", GetName());
            return false;
        }
        SetScriptId(func_num);
    }

    if (GetScriptId())
    {
        scriptSys.PrepareScriptFuncContext(GetScriptId(), _str("Item '{}' ({})", GetName(), GetId()));
        scriptSys.SetArgEntity(this);
        scriptSys.SetArgBool(first_time);
        scriptSys.RunPrepared();
    }
    return true;
}

void Item::SetSortValue(ItemVec& items)
{
    short sort_value = 0;
    for (auto it = items.begin(), end = items.end(); it != end; ++it)
    {
        Item* item = *it;
        if (item == this)
            continue;
        if (sort_value >= item->GetSortValue())
            sort_value = item->GetSortValue() - 1;
    }
    SetSortValue(sort_value);
}

void Item::ChangeCount(int val)
{
    SetCount(GetCount() + val);
}

Item* Item::ContGetItem(uint item_id, bool skip_hide)
{
    RUNTIME_ASSERT(item_id);

    if (!childItems)
        return nullptr;

    for (auto it = childItems->begin(), end = childItems->end(); it != end; ++it)
    {
        Item* item = *it;
        if (item->GetId() == item_id)
        {
            if (skip_hide && item->GetIsHidden())
                return nullptr;
            return item;
        }
    }
    return nullptr;
}

void Item::ContGetAllItems(ItemVec& items, bool skip_hide)
{
    if (!childItems)
        return;

    for (auto it = childItems->begin(), end = childItems->end(); it != end; ++it)
    {
        Item* item = *it;
        if (!skip_hide || !item->GetIsHidden())
            items.push_back(item);
    }
}

Item* Item::ContGetItemByPid(hash pid, uint stack_id)
{
    if (!childItems)
        return nullptr;

    for (auto it = childItems->begin(), end = childItems->end(); it != end; ++it)
    {
        Item* item = *it;
        if (item->GetProtoId() == pid && (stack_id == uint(-1) || item->GetContainerStack() == stack_id))
            return item;
    }
    return nullptr;
}

void Item::ContGetItems(ItemVec& items, uint stack_id)
{
    if (!childItems)
        return;

    for (auto it = childItems->begin(), end = childItems->end(); it != end; ++it)
    {
        Item* item = *it;
        if (stack_id == uint(-1) || item->GetContainerStack() == stack_id)
            items.push_back(item);
    }
}

bool Item::ContIsItems()
{
    return childItems && childItems->size();
}
