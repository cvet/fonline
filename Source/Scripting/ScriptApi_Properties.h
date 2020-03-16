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

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(ushort), YearStart)

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(ushort), Year)

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(ushort), Month)

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(ushort), Day)

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(ushort), Hour)

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(ushort), Minute, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(ushort), Second, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(ushort), TimeMultiplier)

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), LastEntityId, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(
    PrivateCommon, FO_API_PROPERTY_TYPE(uint), LastDeferredCallId, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_GLOBAL_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_GLOBAL_READONLY_PROPERTY(
    PrivateCommon, FO_API_PROPERTY_TYPE(uint), HistoryRecordsId, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Public, FO_API_PROPERTY_TYPE(hash), ModelName)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(uint), WalkTime)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(uint), RunTime)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(Protected, FO_API_PROPERTY_TYPE(uint), Multihex)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), MapId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), RefMapId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(hash), RefMapPid)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), RefLocationId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(hash), RefLocationPid)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(ushort), HexX, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(ushort), HexY, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(uchar), Dir, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(string), Password)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE_ENUM(CritterCondition), Cond)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(bool), ClientToDelete)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(ushort), WorldX, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(ushort), WorldY, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(Protected, FO_API_PROPERTY_TYPE(uint), GlobalMapLeaderId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), GlobalMapTripId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), RefGlobalMapTripId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), RefGlobalMapLeaderId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(
    PrivateServer, FO_API_PROPERTY_TYPE(ushort), LastMapHexX, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(
    PrivateServer, FO_API_PROPERTY_TYPE(ushort), LastMapHexY, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(uint), Anim1Life, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(
    PrivateCommon, FO_API_PROPERTY_TYPE(uint), Anim1Knockout, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(uint), Anim1Dead, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(uint), Anim2Life, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(
    PrivateCommon, FO_API_PROPERTY_TYPE(uint), Anim2Knockout, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateCommon, FO_API_PROPERTY_TYPE(uint), Anim2Dead, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(
    PrivateServer, FO_API_PROPERTY_TYPE_ARR(uchar), GlobalMapFog, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(
    PrivateServer, FO_API_PROPERTY_TYPE_ARR(hash), TE_FuncNum, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE_ARR(uint), TE_Rate, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(
    PrivateServer, FO_API_PROPERTY_TYPE_ARR(uint), TE_NextTime, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_READONLY_PROPERTY(
    PrivateServer, FO_API_PROPERTY_TYPE_ARR(int), TE_Identifier, FO_API_PROPERTY_MOD(NoHistory))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(VirtualPrivateServer, FO_API_PROPERTY_TYPE(int), SneakCoefficient)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(VirtualProtected, FO_API_PROPERTY_TYPE(uint), LookDistance)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Public, FO_API_PROPERTY_TYPE(char), Gender)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(hash), NpcRole)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(int), ReplicationTime)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Public, FO_API_PROPERTY_TYPE(uint), TalkDistance)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Public, FO_API_PROPERTY_TYPE(int), ScaleFactor)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Public, FO_API_PROPERTY_TYPE(int), CurrentHp)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), MaxTalkers)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Public, FO_API_PROPERTY_TYPE(hash), DialogId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Public, FO_API_PROPERTY_TYPE(string), Lexems)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), HomeMapId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(ushort), HomeHexX)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(ushort), HomeHexY)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uchar), HomeDir)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE_ARR(uint), KnownLocations)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE_ARR(uint), ConnectionIp)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE_ARR(ushort), ConnectionPort)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), ShowCritterDist1)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), ShowCritterDist2)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), ShowCritterDist3)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(hash), ScriptId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE_ARR(hash), KnownLocProtoId)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(PrivateClient, FO_API_PROPERTY_TYPE_ARR(int), Anim3dLayer)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(bool), IsHide)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(bool), IsNoHome)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(bool), IsGeck)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(bool), IsNoUnarmed)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(VirtualProtected, FO_API_PROPERTY_TYPE(bool), IsNoWalk)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(VirtualProtected, FO_API_PROPERTY_TYPE(bool), IsNoRun)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(bool), IsNoRotate)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsNoTalk)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsNoFlatten)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Public, FO_API_PROPERTY_TYPE(uint), TimeoutBattle)

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(uint), TimeoutTransfer, FO_API_PROPERTY_MOD(Temporary))

#ifdef FO_API_CRITTER_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_CRITTER_PROPERTY(Protected, FO_API_PROPERTY_TYPE(uint), TimeoutRemoveFromGame, FO_API_PROPERTY_MOD(Temporary))

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE_ENUM(ItemOwnership), Accessory)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uint), MapId)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(ushort), HexX)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(ushort), HexY)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uint), CritId)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), CritSlot)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uint), ContainerId)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uint), ContainerStack)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(float), FlyEffectSpeed)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(hash), PicMap)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(hash), PicInv)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(short), OffsetX)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(short), OffsetY)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), Stackable)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), GroundLevel)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), Opened)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE_ENUM(CornerType), Corner)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), Slot)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(uint), Weight)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(uint), Volume)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), DisableEgg)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(ushort), AnimWaitBase)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(ushort), AnimWaitRndMin)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(ushort), AnimWaitRndMax)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), AnimStay0)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), AnimStay1)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), AnimShow0)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), AnimShow1)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), AnimHide0)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), AnimHide1)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), SpriteCut)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(char), DrawOrderOffsetHexY)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE_ARR(uchar), BlockLines)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_READONLY_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsStatic)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsScenery)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsWall)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsCanOpen)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsScrollBlock)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsHidden)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsHiddenPicture)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsHiddenInStatic)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsFlat)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsNoBlock)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsShootThru)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsLightThru)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsAlwaysView)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsBadItem)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsNoHighlight)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsShowAnim)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsShowAnimExt)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsLight)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsGeck)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsTrap)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsTrigger)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsNoLightInfluence)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsGag)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsColorize)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsColorizeInv)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsCanTalk)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(bool), IsRadio)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(string), Lexems)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(PublicModifiable, FO_API_PROPERTY_TYPE(short), SortValue)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), Info)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(PublicModifiable, FO_API_PROPERTY_TYPE(uchar), Mode)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(char), LightIntensity)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), LightDistance)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(uchar), LightFlags)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(uint), LightColor)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(hash), ScriptId)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Public, FO_API_PROPERTY_TYPE(uint), Count)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Protected, FO_API_PROPERTY_TYPE(short), TrapValue)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Protected, FO_API_PROPERTY_TYPE(ushort), RadioChannel)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Protected, FO_API_PROPERTY_TYPE(ushort), RadioFlags)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Protected, FO_API_PROPERTY_TYPE(uchar), RadioBroadcastSend)

#ifdef FO_API_ITEM_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_ITEM_PROPERTY(Protected, FO_API_PROPERTY_TYPE(uchar), RadioBroadcastRecv)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), LoopTime1)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), LoopTime2)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), LoopTime3)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), LoopTime4)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), LoopTime5)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(string), FileDir, FO_API_PROPERTY_MOD(Temporary))

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(ushort), Width)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(ushort), Height)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(ushort), WorkHexX)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(ushort), WorkHexY)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), LocId)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), LocMapIndex)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uchar), RainCapacity)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(int), CurDayTime)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(hash), ScriptId)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE_ARR(int), DayTime)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE_ARR(uchar), DayColor)

#ifdef FO_API_MAP_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_MAP_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(bool), IsNoLogOut)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE_ARR(hash), MapProtos)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE_ARR(hash), MapEntrances)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_READONLY_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE_ARR(hash), Automaps)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), MaxPlayers)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(bool), AutoGarbage)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(bool), GeckVisible)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(hash), EntranceScript)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(ushort), WorldX)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(ushort), WorldY)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(ushort), Radius)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(bool), Hidden)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(bool), ToGarbage)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(uint), Color)

#ifdef FO_API_LOCATION_PROPERTY_DOC
/*******************************************************************************
 * ...
 ******************************************************************************/
#endif
FO_API_LOCATION_PROPERTY(PrivateServer, FO_API_PROPERTY_TYPE(bool), IsEncounter)
