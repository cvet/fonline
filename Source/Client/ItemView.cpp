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

#include "ItemView.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "SpriteManager.h"
#include "Testing.h"
#include "Timer.h"

PROPERTIES_IMPL(ItemView);
CLASS_PROPERTY_IMPL(ItemView, PicMap);
CLASS_PROPERTY_IMPL(ItemView, PicInv);
CLASS_PROPERTY_IMPL(ItemView, OffsetX);
CLASS_PROPERTY_IMPL(ItemView, OffsetY);
CLASS_PROPERTY_IMPL(ItemView, LightIntensity);
CLASS_PROPERTY_IMPL(ItemView, LightDistance);
CLASS_PROPERTY_IMPL(ItemView, LightFlags);
CLASS_PROPERTY_IMPL(ItemView, LightColor);
CLASS_PROPERTY_IMPL(ItemView, Stackable);
CLASS_PROPERTY_IMPL(ItemView, Opened);
CLASS_PROPERTY_IMPL(ItemView, Corner);
CLASS_PROPERTY_IMPL(ItemView, Slot);
CLASS_PROPERTY_IMPL(ItemView, DisableEgg);
CLASS_PROPERTY_IMPL(ItemView, AnimWaitBase);
CLASS_PROPERTY_IMPL(ItemView, AnimWaitRndMin);
CLASS_PROPERTY_IMPL(ItemView, AnimWaitRndMax);
CLASS_PROPERTY_IMPL(ItemView, AnimStay0);
CLASS_PROPERTY_IMPL(ItemView, AnimStay1);
CLASS_PROPERTY_IMPL(ItemView, AnimShow0);
CLASS_PROPERTY_IMPL(ItemView, AnimShow1);
CLASS_PROPERTY_IMPL(ItemView, AnimHide0);
CLASS_PROPERTY_IMPL(ItemView, AnimHide1);
CLASS_PROPERTY_IMPL(ItemView, SpriteCut);
CLASS_PROPERTY_IMPL(ItemView, DrawOrderOffsetHexY);
CLASS_PROPERTY_IMPL(ItemView, BlockLines);
CLASS_PROPERTY_IMPL(ItemView, ScriptId);
CLASS_PROPERTY_IMPL(ItemView, Accessory);
CLASS_PROPERTY_IMPL(ItemView, MapId);
CLASS_PROPERTY_IMPL(ItemView, HexX);
CLASS_PROPERTY_IMPL(ItemView, HexY);
CLASS_PROPERTY_IMPL(ItemView, CritId);
CLASS_PROPERTY_IMPL(ItemView, CritSlot);
CLASS_PROPERTY_IMPL(ItemView, ContainerId);
CLASS_PROPERTY_IMPL(ItemView, ContainerStack);
CLASS_PROPERTY_IMPL(ItemView, IsStatic);
CLASS_PROPERTY_IMPL(ItemView, IsScenery);
CLASS_PROPERTY_IMPL(ItemView, IsWall);
CLASS_PROPERTY_IMPL(ItemView, IsCanOpen);
CLASS_PROPERTY_IMPL(ItemView, IsScrollBlock);
CLASS_PROPERTY_IMPL(ItemView, IsHidden);
CLASS_PROPERTY_IMPL(ItemView, IsHiddenPicture);
CLASS_PROPERTY_IMPL(ItemView, IsHiddenInStatic);
CLASS_PROPERTY_IMPL(ItemView, IsFlat);
CLASS_PROPERTY_IMPL(ItemView, IsNoBlock);
CLASS_PROPERTY_IMPL(ItemView, IsShootThru);
CLASS_PROPERTY_IMPL(ItemView, IsLightThru);
CLASS_PROPERTY_IMPL(ItemView, IsAlwaysView);
CLASS_PROPERTY_IMPL(ItemView, IsBadItem);
CLASS_PROPERTY_IMPL(ItemView, IsNoHighlight);
CLASS_PROPERTY_IMPL(ItemView, IsShowAnim);
CLASS_PROPERTY_IMPL(ItemView, IsShowAnimExt);
CLASS_PROPERTY_IMPL(ItemView, IsLight);
CLASS_PROPERTY_IMPL(ItemView, IsGeck);
CLASS_PROPERTY_IMPL(ItemView, IsTrap);
CLASS_PROPERTY_IMPL(ItemView, IsTrigger);
CLASS_PROPERTY_IMPL(ItemView, IsNoLightInfluence);
CLASS_PROPERTY_IMPL(ItemView, IsGag);
CLASS_PROPERTY_IMPL(ItemView, IsColorize);
CLASS_PROPERTY_IMPL(ItemView, IsColorizeInv);
CLASS_PROPERTY_IMPL(ItemView, IsCanTalk);
CLASS_PROPERTY_IMPL(ItemView, IsRadio);
CLASS_PROPERTY_IMPL(ItemView, SortValue);
CLASS_PROPERTY_IMPL(ItemView, Mode);
CLASS_PROPERTY_IMPL(ItemView, Count);
CLASS_PROPERTY_IMPL(ItemView, TrapValue);
CLASS_PROPERTY_IMPL(ItemView, RadioChannel);
CLASS_PROPERTY_IMPL(ItemView, RadioFlags);
CLASS_PROPERTY_IMPL(ItemView, RadioBroadcastSend);
CLASS_PROPERTY_IMPL(ItemView, RadioBroadcastRecv);
CLASS_PROPERTY_IMPL(ItemView, FlyEffectSpeed);

ItemView::ItemView(uint id, ProtoItem* proto) : Entity(id, EntityType::ItemView, PropertiesRegistrator, proto)
{
    RUNTIME_ASSERT(Proto);
    RUNTIME_ASSERT(GetCount() > 0);
}

ItemView* ItemView::Clone()
{
    ItemView* clone = new ItemView(Id, (ProtoItem*)Proto);
    clone->Props = Props;
    return clone;
}

bool ItemView::IsStatic()
{
    return GetIsStatic();
}

bool ItemView::IsAnyScenery()
{
    return IsScenery() || IsWall();
}

bool ItemView::IsScenery()
{
    return GetIsScenery();
}

bool ItemView::IsWall()
{
    return GetIsWall();
}

bool ItemView::IsColorize()
{
    return GetIsColorize();
}

uint ItemView::GetColor()
{
    return GetLightColor() & 0xFFFFFF;
}

uchar ItemView::GetAlpha()
{
    return GetLightColor() >> 24;
}

uint ItemView::GetInvColor()
{
    return GetIsColorizeInv() ? GetLightColor() : 0;
}

uint ItemView::LightGetHash()
{
    return GetIsLight() ? GetLightIntensity() + GetLightDistance() + GetLightFlags() + GetLightColor() : 0;
}
