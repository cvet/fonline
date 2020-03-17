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

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(Init)

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(GenerateWorld)

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(Start)

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(Finish)

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(Loop)

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(GlobalMapCritterIn, FO_API_ARG_OBJ(Critter, critter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(GlobalMapCritterOut, FO_API_ARG_OBJ(Critter, critter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param location ...
 * @param firstTime ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(LocationInit, FO_API_ARG_OBJ(Location, location), FO_API_ARG(bool, firstTime))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param location ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(LocationFinish, FO_API_ARG_OBJ(Location, location))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param firstTime ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(MapInit, FO_API_ARG_OBJ(Map, map), FO_API_ARG(bool, firstTime))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(MapFinish, FO_API_ARG_OBJ(Map, map))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param loopIndex ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(MapLoop, FO_API_ARG_OBJ(Map, map), FO_API_ARG(uint, loopIndex))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param critter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(MapCritterIn, FO_API_ARG_OBJ(Map, map), FO_API_ARG_OBJ(Critter, critter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param critter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(MapCritterOut, FO_API_ARG_OBJ(Map, map), FO_API_ARG_OBJ(Critter, critter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param critter ...
 * @param target ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(
    MapCheckLook, FO_API_ARG_OBJ(Map, map), FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, target))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 * @param critter ...
 * @param item ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(
    MapCheckTrapLook, FO_API_ARG_OBJ(Map, map), FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Item, item))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param firstTime ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterInit, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG(bool, firstTime))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterFinish, FO_API_ARG_OBJ(Critter, critter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterIdle, FO_API_ARG_OBJ(Critter, critter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterGlobalMapIdle, FO_API_ARG_OBJ(Critter, critter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param item ...
 * @param toSlot ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(
    CritterCheckMoveItem, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Item, item), FO_API_ARG(uchar, toSlot))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param item ...
 * @param fromSlot ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(
    CritterMoveItem, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Item, item), FO_API_ARG(uchar, fromSlot))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param showCritter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterShow, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, showCritter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param showCritter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterShowDist1, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, showCritter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param showCritter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterShowDist2, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, showCritter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param showCritter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterShowDist3, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, showCritter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param hideCritter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterHide, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, hideCritter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param hideCritter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterHideDist1, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, hideCritter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param hideCritter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterHideDist2, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, hideCritter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param hideCritter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterHideDist3, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, hideCritter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param item ...
 * @param added ...
 * @param fromCritter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterShowItemOnMap, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Item, item),
    FO_API_ARG(bool, added), FO_API_ARG_OBJ(Critter, fromCritter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param item ...
 * @param removed ...
 * @param toCritter ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterHideItemOnMap, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Item, item),
    FO_API_ARG(bool, removed), FO_API_ARG_OBJ(Critter, toCritter))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param item ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterChangeItemOnMap, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Item, item))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param receiver ...
 * @param num ...
 * @param value ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterMessage, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, receiver),
    FO_API_ARG(int, num), FO_API_ARG(int, value))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param who ...
 * @param begin ...
 * @param talkers ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterTalk, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Critter, who),
    FO_API_ARG(bool, begin), FO_API_ARG(uint, talkers))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param cr ...
 * @param trader ...
 * @param begin ...
 * @param barterCount ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterBarter, FO_API_ARG_OBJ(Critter, cr), FO_API_ARG_OBJ(Critter, trader),
    FO_API_ARG(bool, begin), FO_API_ARG(uint, barterCount))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param item ...
 * @param itemMode ...
 * @param dist ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(CritterGetAttackDistantion, FO_API_ARG_OBJ(Critter, critter), FO_API_ARG_OBJ(Item, item),
    FO_API_ARG(uchar, itemMode), FO_API_ARG_REF(uint, dist))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param ip ...
 * @param name ...
 * @param disallowMsgNum ...
 * @param disallowStrNum ...
 * @param disallowLex ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(PlayerRegistration, FO_API_ARG(uint, ip), FO_API_ARG(string, name),
    FO_API_ARG_REF(uint, disallowMsgNum), FO_API_ARG_REF(uint, disallowStrNum), FO_API_ARG_REF(string, disallowLex))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param ip ...
 * @param name ...
 * @param id ...
 * @param disallowMsgNum ...
 * @param disallowStrNum ...
 * @param disallowLex ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(PlayerLogin, FO_API_ARG(uint, ip), FO_API_ARG(string, name), FO_API_ARG(uint, id),
    FO_API_ARG_REF(uint, disallowMsgNum), FO_API_ARG_REF(uint, disallowStrNum), FO_API_ARG_REF(string, disallowLex))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param player ...
 * @param arg1 ...
 * @param arg2 ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(
    PlayerGetAccess, FO_API_ARG_OBJ(Critter, player), FO_API_ARG(int, arg1), FO_API_ARG_REF(string, arg2))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param player ...
 * @param arg1 ...
 * @param arg2 ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(
    PlayerAllowCommand, FO_API_ARG_OBJ(Critter, player), FO_API_ARG(string, arg1), FO_API_ARG(uchar, arg2))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param player ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(PlayerLogout, FO_API_ARG_OBJ(Critter, player))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param firstTime ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(ItemInit, FO_API_ARG_OBJ(Item, item), FO_API_ARG(bool, firstTime))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(ItemFinish, FO_API_ARG_OBJ(Item, item))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param critter ...
 * @param isIn ...
 * @param dir ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(ItemWalk, FO_API_ARG_OBJ(Item, item), FO_API_ARG_OBJ(Critter, critter), FO_API_ARG(bool, isIn),
    FO_API_ARG(uchar, dir))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param count ...
 * @param from ...
 * @param to ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(ItemCheckMove, FO_API_ARG_OBJ(Item, item), FO_API_ARG(uint, count), FO_API_ARG_OBJ(Entity, from),
    FO_API_ARG_OBJ(Entity, to))

#ifdef FO_API_SERVER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param critter ...
 * @param isIn ...
 * @param dir ...
 ******************************************************************************/
#endif
FO_API_SERVER_EVENT(StaticItemWalk, FO_API_ARG_OBJ(Item, item), FO_API_ARG_OBJ(Critter, critter),
    FO_API_ARG(bool, isIn), FO_API_ARG(uchar, dir))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(Start)

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(Finish)

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param login ...
 * @param password ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(AutoLogin, FO_API_ARG(string, login), FO_API_ARG(string, password))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(Loop)

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param screens ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(GetActiveScreens, FO_API_ARG_ARR_REF(int, screens))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param show ...
 * @param screen ...
 * @param params ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ScreenChange, FO_API_ARG(bool, show), FO_API_ARG(int, screen), FO_API_ARG_DICT(string, int, params))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param offsetX ...
 * @param offsetY ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ScreenScroll, FO_API_ARG(int, offsetX), FO_API_ARG(int, offsetY))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(RenderIface)

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(RenderMap)

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param button ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(MouseDown, FO_API_ARG_ENUM(MouseButton, button))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param button ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(MouseUp, FO_API_ARG_ENUM(MouseButton, button))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param offsetX ...
 * @param offsetY ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(MouseMove, FO_API_ARG(int, offsetX), FO_API_ARG(int, offsetY))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param key ...
 * @param text ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(KeyDown, FO_API_ARG_ENUM(KeyCode, key), FO_API_ARG(string, text))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param key ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(KeyUp, FO_API_ARG_ENUM(KeyCode, key))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(InputLost)

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(CritterIn, FO_API_ARG_OBJ(CritterView, critter))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(CritterOut, FO_API_ARG_OBJ(CritterView, critter))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ItemMapIn, FO_API_ARG_OBJ(ItemView, item))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param oldItem ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ItemMapChanged, FO_API_ARG_OBJ(ItemView, item), FO_API_ARG_OBJ(ItemView, oldItem))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ItemMapOut, FO_API_ARG_OBJ(ItemView, item))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ItemInvAllIn)

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ItemInvIn, FO_API_ARG_OBJ(ItemView, item))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param oldItem ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ItemInvChanged, FO_API_ARG_OBJ(ItemView, item), FO_API_ARG_OBJ(ItemView, oldItem))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ItemInvOut, FO_API_ARG_OBJ(ItemView, item))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(MapLoad)

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(MapUnload)

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param items ...
 * @param param ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ReceiveItems, FO_API_ARG_OBJ_ARR(ItemView, items), FO_API_ARG(int, param))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param hexX ...
 * @param hexY ...
 * @param color ...
 * @param delay ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(MapMessage, FO_API_ARG_REF(string, text), FO_API_ARG_REF(ushort, hexX),
    FO_API_ARG_REF(ushort, hexY), FO_API_ARG_REF(uint, color), FO_API_ARG_REF(uint, delay))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param sayType ...
 * @param critterId ...
 * @param delay ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(InMessage, FO_API_ARG(string, text), FO_API_ARG_REF(int, sayType), FO_API_ARG_REF(uint, critterId),
    FO_API_ARG_REF(uint, delay))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param sayType ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(OutMessage, FO_API_ARG_REF(string, text), FO_API_ARG_REF(int, sayType))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param type ...
 * @param scriptCall ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(MessageBox, FO_API_ARG(string, text), FO_API_ARG_ENUM(MessageBoxTextType, type), FO_API_ARG(bool, scriptCall))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param result ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(CombatResult, FO_API_ARG_ARR(uint, result))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param item ...
 * @param count ...
 * @param from ...
 * @param to ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(ItemCheckMove, FO_API_ARG_OBJ(ItemView, item), FO_API_ARG(uint, count),
    FO_API_ARG_OBJ(Entity, from), FO_API_ARG_OBJ(Entity, to))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param localCall ...
 * @param critter ...
 * @param action ...
 * @param actionExt ...
 * @param actionItem ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(CritterAction, FO_API_ARG(bool, localCall), FO_API_ARG_OBJ(CritterView, critter),
    FO_API_ARG(int, action), FO_API_ARG(int, actionExt), FO_API_ARG_OBJ(ItemView, actionItem))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param arg1 ...
 * @param arg2 ...
 * @param arg3 ...
 * @param arg4 ...
 * @param arg5 ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(Animation2dProcess, FO_API_ARG(bool, arg1), FO_API_ARG_OBJ(CritterView, arg2),
    FO_API_ARG(uint, arg3), FO_API_ARG(uint, arg4), FO_API_ARG_OBJ(ItemView, arg5))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param arg1 ...
 * @param arg2 ...
 * @param arg3 ...
 * @param arg4 ...
 * @param arg5 ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(Animation3dProcess, FO_API_ARG(bool, arg1), FO_API_ARG_OBJ(CritterView, arg2),
    FO_API_ARG(uint, arg3), FO_API_ARG(uint, arg4), FO_API_ARG_OBJ(ItemView, arg5))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param arg1 ...
 * @param arg2 ...
 * @param arg3 ...
 * @param arg4 ...
 * @param arg5 ...
 * @param arg6 ...
 * @param arg7 ...
 * @param arg8 ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(CritterAnimation, FO_API_ARG(hash, arg1), FO_API_ARG(uint, arg2), FO_API_ARG(uint, arg3),
    FO_API_ARG_REF(uint, arg4), FO_API_ARG_REF(uint, arg5), FO_API_ARG_REF(int, arg6), FO_API_ARG_REF(int, arg7),
    FO_API_ARG_REF(string, arg8))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param arg1 ...
 * @param arg2 ...
 * @param arg3 ...
 * @param arg4 ...
 * @param arg5 ...
 * @param arg6 ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(CritterAnimationSubstitute, FO_API_ARG(hash, arg1), FO_API_ARG(uint, arg2), FO_API_ARG(uint, arg3),
    FO_API_ARG_REF(hash, arg4), FO_API_ARG_REF(uint, arg5), FO_API_ARG_REF(uint, arg6))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param arg1 ...
 * @param arg2 ...
 * @param arg3 ...
 * @param arg4 ...
 * @param arg5 ...
 * @param arg6 ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(CritterAnimationFallout, FO_API_ARG(hash, arg1), FO_API_ARG_REF(uint, arg2),
    FO_API_ARG_REF(uint, arg3), FO_API_ARG_REF(uint, arg4), FO_API_ARG_REF(uint, arg5), FO_API_ARG_REF(uint, arg6))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param item ...
 * @param toSlot ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(CritterCheckMoveItem, FO_API_ARG_OBJ(CritterView, critter), FO_API_ARG_OBJ(ItemView, item),
    FO_API_ARG(uchar, toSlot))

#ifdef FO_API_CLIENT_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param critter ...
 * @param item ...
 * @param itemMode ...
 * @param dist ...
 ******************************************************************************/
#endif
FO_API_CLIENT_EVENT(CritterGetAttackDistantion, FO_API_ARG_OBJ(CritterView, critter), FO_API_ARG_OBJ(ItemView, item),
    FO_API_ARG(uchar, itemMode), FO_API_ARG_REF(uint, dist))

#ifdef FO_API_MAPPER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param text ...
 ******************************************************************************/
#endif
FO_API_MAPPER_EVENT(ConsoleMessage, FO_API_ARG_REF(string, text))

#ifdef FO_API_MAPPER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 ******************************************************************************/
#endif
FO_API_MAPPER_EVENT(MapLoad, FO_API_ARG_OBJ(MapView, map))

#ifdef FO_API_MAPPER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param map ...
 ******************************************************************************/
#endif
FO_API_MAPPER_EVENT(MapSave, FO_API_ARG_OBJ(MapView, map))

#ifdef FO_API_MAPPER_EVENT_DOC
/*******************************************************************************
 * ...
 *
 * @param entity ...
 * @param properties ...
 ******************************************************************************/
#endif
FO_API_MAPPER_EVENT(InspectorProperties, FO_API_ARG_OBJ(Entity, entity), FO_API_ARG_ARR_REF(int, properties))
