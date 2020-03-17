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

#ifdef FO_API_ENUM_GROUP_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ENUM_GROUP(MessageBoxTextType)

#ifdef FO_API_ENUM_GROUP_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ENUM_GROUP(MouseButton)

#ifdef FO_API_ENUM_GROUP_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ENUM_GROUP(KeyCode)

#ifdef FO_API_ENUM_GROUP_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ENUM_GROUP(CornerType)
FO_API_ENUM_ENTRY(CornerType, NorthSouth, 0)
FO_API_ENUM_ENTRY(CornerType, West, 1)
FO_API_ENUM_ENTRY(CornerType, East, 2)
FO_API_ENUM_ENTRY(CornerType, South, 3)
FO_API_ENUM_ENTRY(CornerType, North, 4)
FO_API_ENUM_ENTRY(CornerType, EastWest, 5)

#ifdef FO_API_ENUM_GROUP_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
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

#ifdef FO_API_ENUM_GROUP_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ENUM_GROUP(CritterCondition)
FO_API_ENUM_ENTRY(CritterCondition, Unknown, 0)
FO_API_ENUM_ENTRY(CritterCondition, Alive, 1)
FO_API_ENUM_ENTRY(CritterCondition, Knockout, 2)
FO_API_ENUM_ENTRY(CritterCondition, Dead, 3)

#ifdef FO_API_ENUM_GROUP_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ENUM_GROUP(ItemOwnership)
FO_API_ENUM_ENTRY(ItemOwnership, Nowhere, 0)
FO_API_ENUM_ENTRY(ItemOwnership, CritterInventory, 1)
FO_API_ENUM_ENTRY(ItemOwnership, MapHex, 2)
FO_API_ENUM_ENTRY(ItemOwnership, ItemContainer, 3)

#ifdef FO_API_ENUM_GROUP_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ENUM_GROUP(Anim1)
FO_API_ENUM_ENTRY(Anim1, None, 0)

#ifdef FO_API_ENUM_GROUP_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ENUM_GROUP(CursorType)
FO_API_ENUM_ENTRY(CursorType, Default, 0)
