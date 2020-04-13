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
#include "Testing.h"
#include "Timer.h"

#define FO_API_ITEM_VIEW_IMPL
#include "ScriptApi.h"

PROPERTIES_IMPL(ItemView, "Item", false);
#define FO_API_ITEM_PROPERTY(access, type, name, ...) CLASS_PROPERTY_IMPL(ItemView, access, type, name, __VA_ARGS__);
#include "ScriptApi.h"

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
