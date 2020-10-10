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

/*******************************************************************************
 * ...
 ******************************************************************************/
FO_API_ENUM_GROUP(MessageBoxTextType)

/*******************************************************************************
 * Mouse button types
 ******************************************************************************/
FO_API_ENUM_GROUP(MouseButton)
FO_API_ENUM_ENTRY(MouseButton, Left, 0)
FO_API_ENUM_ENTRY(MouseButton, Right, 1)
FO_API_ENUM_ENTRY(MouseButton, Middle, 2)
FO_API_ENUM_ENTRY(MouseButton, Ext0, 5)
FO_API_ENUM_ENTRY(MouseButton, Ext1, 6)
FO_API_ENUM_ENTRY(MouseButton, Ext2, 7)
FO_API_ENUM_ENTRY(MouseButton, Ext3, 8)
FO_API_ENUM_ENTRY(MouseButton, Ext4, 9)

/*******************************************************************************
 * Keyboard key codes
 ******************************************************************************/
FO_API_ENUM_GROUP(KeyCode)
#define KEY_CODE(name, index, code) FO_API_ENUM_ENTRY(KeyCode, name, index)
#include "KeyCodes-Include.h"

/*******************************************************************************
 * ...
 ******************************************************************************/
FO_API_ENUM_GROUP(CornerType)
FO_API_ENUM_ENTRY(CornerType, NorthSouth, 0)
FO_API_ENUM_ENTRY(CornerType, West, 1)
FO_API_ENUM_ENTRY(CornerType, East, 2)
FO_API_ENUM_ENTRY(CornerType, South, 3)
FO_API_ENUM_ENTRY(CornerType, North, 4)
FO_API_ENUM_ENTRY(CornerType, EastWest, 5)

/*******************************************************************************
 * ...
 ******************************************************************************/
FO_API_ENUM_GROUP(MovingState)
FO_API_ENUM_ENTRY(MovingState, InProgress, 0)
FO_API_ENUM_ENTRY(MovingState, Success, 1)
FO_API_ENUM_ENTRY(MovingState, TargetNotFound, 2)
FO_API_ENUM_ENTRY(MovingState, CantMove, 3)
FO_API_ENUM_ENTRY(MovingState, GagCritter, 4)
FO_API_ENUM_ENTRY(MovingState, GagItem, 5)
FO_API_ENUM_ENTRY(MovingState, InternalError, 6)
FO_API_ENUM_ENTRY(MovingState, HexTooFar, 7)
FO_API_ENUM_ENTRY(MovingState, HexBusy, 8)
FO_API_ENUM_ENTRY(MovingState, HexBusyRing, 9)
FO_API_ENUM_ENTRY(MovingState, Deadlock, 10)
FO_API_ENUM_ENTRY(MovingState, TraceFail, 11)

/*******************************************************************************
 * ...
 ******************************************************************************/
FO_API_ENUM_GROUP(CritterCondition)
FO_API_ENUM_ENTRY(CritterCondition, Unknown, 0)
FO_API_ENUM_ENTRY(CritterCondition, Alive, 1)
FO_API_ENUM_ENTRY(CritterCondition, Knockout, 2)
FO_API_ENUM_ENTRY(CritterCondition, Dead, 3)

/*******************************************************************************
 * ...
 ******************************************************************************/
FO_API_ENUM_GROUP(ItemOwnership)
FO_API_ENUM_ENTRY(ItemOwnership, Nowhere, 0)
FO_API_ENUM_ENTRY(ItemOwnership, CritterInventory, 1)
FO_API_ENUM_ENTRY(ItemOwnership, MapHex, 2)
FO_API_ENUM_ENTRY(ItemOwnership, ItemContainer, 3)

/*******************************************************************************
 * ...
 ******************************************************************************/
FO_API_ENUM_GROUP(Anim1)
FO_API_ENUM_ENTRY(Anim1, None, 0)

/*******************************************************************************
 * ...
 ******************************************************************************/
FO_API_ENUM_GROUP(CursorType)
FO_API_ENUM_ENTRY(CursorType, Default, 0)

/*******************************************************************************
 * ...
 ******************************************************************************/
FO_API_ENUM_GROUP(EffectType)
FO_API_ENUM_ENTRY(EffectType, GenericSprite, 0x00000001) // Subtype can be item id, zero for all items
FO_API_ENUM_ENTRY(EffectType, CritterSprite, 0x00000002) // Subtype can be critter id, zero for all critters
FO_API_ENUM_ENTRY(EffectType, TileSprite, 0x00000004)
FO_API_ENUM_ENTRY(EffectType, RoofSprite, 0x00000008)
FO_API_ENUM_ENTRY(EffectType, RainSprite, 0x00000010)
FO_API_ENUM_ENTRY(EffectType, SkinnedMesh, 0x00000400)
FO_API_ENUM_ENTRY(EffectType, Interface, 0x00001000)
FO_API_ENUM_ENTRY(EffectType, Contour, 0x00002000)
FO_API_ENUM_ENTRY(EffectType, Font, 0x00010000) // Subtype is FONT_*, -1 default for all fonts
FO_API_ENUM_ENTRY(EffectType, Primitive, 0x00100000)
FO_API_ENUM_ENTRY(EffectType, Light, 0x00200000)
FO_API_ENUM_ENTRY(EffectType, Fog, 0x00400000)
FO_API_ENUM_ENTRY(EffectType, FlushRenderTarget, 0x01000000)
FO_API_ENUM_ENTRY(EffectType, FlushRenderTargetMultisampled, 0x02000000) // Multisample
FO_API_ENUM_ENTRY(EffectType, FlushPrimitive, 0x04000000)
FO_API_ENUM_ENTRY(EffectType, FlushMap, 0x08000000)
FO_API_ENUM_ENTRY(EffectType, FlushLight, 0x10000000)
FO_API_ENUM_ENTRY(EffectType, FlushFog, 0x20000000)
FO_API_ENUM_ENTRY(EffectType, Offscreen, 0x40000000)
