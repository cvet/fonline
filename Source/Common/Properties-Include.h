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

#ifndef GLOBAL_PROPERTY
#define GLOBAL_PROPERTY(...)
#endif
#ifndef PLAYER_PROPERTY
#define PLAYER_PROPERTY(...)
#endif
#ifndef ITEM_PROPERTY
#define ITEM_PROPERTY(...)
#endif
#ifndef CRITTER_PROPERTY
#define CRITTER_PROPERTY(...)
#endif
#ifndef MAP_PROPERTY
#define MAP_PROPERTY(...)
#endif
#ifndef LOCATION_PROPERTY
#define LOCATION_PROPERTY(...)
#endif

///@ ExportProperty ReadOnly
GLOBAL_PROPERTY(PrivateCommon, ushort, Year)
///@ ExportProperty ReadOnly
GLOBAL_PROPERTY(PrivateCommon, ushort, Month)
///@ ExportProperty ReadOnly
GLOBAL_PROPERTY(PrivateCommon, ushort, Day)
///@ ExportProperty ReadOnly
GLOBAL_PROPERTY(PrivateCommon, ushort, Hour)
///@ ExportProperty ReadOnly NoHistory
GLOBAL_PROPERTY(PrivateCommon, ushort, Minute)
///@ ExportProperty ReadOnly NoHistory
GLOBAL_PROPERTY(PrivateCommon, ushort, Second)
///@ ExportProperty ReadOnly
GLOBAL_PROPERTY(PrivateCommon, ushort, TimeMultiplier)
///@ ExportProperty ReadOnly NoHistory
GLOBAL_PROPERTY(PrivateServer, uint, LastEntityId)
///@ ExportProperty ReadOnly NoHistory
GLOBAL_PROPERTY(PrivateCommon, uint, LastDeferredCallId)
///@ ExportProperty ReadOnly NoHistory
GLOBAL_PROPERTY(PrivateCommon, uint, HistoryRecordsId)

///@ ExportProperty
PLAYER_PROPERTY(PrivateServer, vector<uint>, ConnectionIp)
///@ ExportProperty
PLAYER_PROPERTY(PrivateServer, vector<ushort>, ConnectionPort)

///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uchar, Accessory)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uint, MapId)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, ushort, HexX)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, ushort, HexY)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uint, CritId)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uchar, CritSlot)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uint, ContainerId)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uint, ContainerStack)
///@ ExportProperty
ITEM_PROPERTY(Public, float, FlyEffectSpeed)
///@ ExportProperty
ITEM_PROPERTY(Public, hash, PicMap)
///@ ExportProperty
ITEM_PROPERTY(Public, hash, PicInv)
///@ ExportProperty
ITEM_PROPERTY(Public, short, OffsetX)
///@ ExportProperty
ITEM_PROPERTY(Public, short, OffsetY)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, bool, Stackable)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, GroundLevel)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, Opened)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uchar, Corner)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uchar, Slot)
///@ ExportProperty
ITEM_PROPERTY(Public, uint, Weight)
///@ ExportProperty
ITEM_PROPERTY(Public, uint, Volume)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, bool, DisableEgg)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, ushort, AnimWaitBase)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, ushort, AnimWaitRndMin)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, ushort, AnimWaitRndMax)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uchar, AnimStay0)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uchar, AnimStay1)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uchar, AnimShow0)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uchar, AnimShow1)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uchar, AnimHide0)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, uchar, AnimHide1)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, char, DrawOrderOffsetHexY)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, vector<uchar>, BlockLines)
///@ ExportProperty ReadOnly
ITEM_PROPERTY(Public, bool, IsStatic)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsScenery)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsWall)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsCanOpen)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsScrollBlock)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsHidden)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsHiddenPicture)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsHiddenInStatic)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsFlat)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsNoBlock)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsShootThru)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsLightThru)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsAlwaysView)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsBadItem)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsNoHighlight)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsShowAnim)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsShowAnimExt)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsLight)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsGeck)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsTrap)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsTrigger)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsNoLightInfluence)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsGag)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsColorize)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsColorizeInv)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsCanTalk)
///@ ExportProperty
ITEM_PROPERTY(Public, bool, IsRadio)
///@ ExportProperty
ITEM_PROPERTY(Public, string, Lexems)
///@ ExportProperty
ITEM_PROPERTY(PublicModifiable, short, SortValue)
///@ ExportProperty
ITEM_PROPERTY(Public, uchar, Info)
///@ ExportProperty
ITEM_PROPERTY(PublicModifiable, uchar, Mode)
///@ ExportProperty
ITEM_PROPERTY(Public, char, LightIntensity)
///@ ExportProperty
ITEM_PROPERTY(Public, uchar, LightDistance)
///@ ExportProperty
ITEM_PROPERTY(Public, uchar, LightFlags)
///@ ExportProperty
ITEM_PROPERTY(Public, uint, LightColor)
///@ ExportProperty
ITEM_PROPERTY(PrivateServer, hash, ScriptId)
///@ ExportProperty
ITEM_PROPERTY(Public, uint, Count)
///@ ExportProperty
ITEM_PROPERTY(Protected, short, TrapValue)
///@ ExportProperty
ITEM_PROPERTY(Protected, ushort, RadioChannel)
///@ ExportProperty
ITEM_PROPERTY(Protected, ushort, RadioFlags)
///@ ExportProperty
ITEM_PROPERTY(Protected, uchar, RadioBroadcastSend)
///@ ExportProperty
ITEM_PROPERTY(Protected, uchar, RadioBroadcastRecv)

///@ ExportProperty
CRITTER_PROPERTY(Public, hash, ModelName)
///@ ExportProperty
CRITTER_PROPERTY(Protected, uint, WalkTime)
///@ ExportProperty
CRITTER_PROPERTY(Protected, uint, RunTime)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(Protected, uint, Multihex)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, MapId)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, RefMapId)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateServer, hash, RefMapPid)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, RefLocationId)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateServer, hash, RefLocationPid)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, ushort, HexX)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, ushort, HexY)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uchar, Dir)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateServer, string, Password)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateCommon, CritterCondition, Cond)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateServer, bool, ClientToDelete)
///@ ExportProperty NoHistory
CRITTER_PROPERTY(Protected, ushort, WorldX)
///@ ExportProperty NoHistory
CRITTER_PROPERTY(Protected, ushort, WorldY)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(Protected, uint, GlobalMapLeaderId)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, GlobalMapTripId)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, RefGlobalMapTripId)
///@ ExportProperty ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, RefGlobalMapLeaderId)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, ushort, LastMapHexX)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, ushort, LastMapHexY)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim1Life)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim1Knockout)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim1Dead)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim2Life)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim2Knockout)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim2Dead)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, vector<uchar>, GlobalMapFog)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, vector<hash>, TE_FuncNum)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, vector<uint>, TE_Rate)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, vector<uint>, TE_NextTime)
///@ ExportProperty ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, vector<int>, TE_Identifier)
///@ ExportProperty
CRITTER_PROPERTY(VirtualPrivateServer, uint, SneakCoefficient)
///@ ExportProperty
CRITTER_PROPERTY(VirtualProtected, uint, LookDistance)
///@ ExportProperty
CRITTER_PROPERTY(Public, char, Gender)
///@ ExportProperty
CRITTER_PROPERTY(Protected, hash, NpcRole)
///@ ExportProperty
CRITTER_PROPERTY(Protected, int, ReplicationTime)
///@ ExportProperty
CRITTER_PROPERTY(Public, uint, TalkDistance)
///@ ExportProperty
CRITTER_PROPERTY(Public, int, ScaleFactor)
///@ ExportProperty
CRITTER_PROPERTY(Public, int, CurrentHp)
///@ ExportProperty
CRITTER_PROPERTY(PrivateServer, uint, MaxTalkers)
///@ ExportProperty
CRITTER_PROPERTY(Public, hash, DialogId)
///@ ExportProperty
CRITTER_PROPERTY(Public, string, Lexems)
///@ ExportProperty
CRITTER_PROPERTY(PrivateServer, uint, HomeMapId)
///@ ExportProperty
CRITTER_PROPERTY(PrivateServer, ushort, HomeHexX)
///@ ExportProperty
CRITTER_PROPERTY(PrivateServer, ushort, HomeHexY)
///@ ExportProperty
CRITTER_PROPERTY(PrivateServer, uchar, HomeDir)
///@ ExportProperty
CRITTER_PROPERTY(PrivateServer, vector<uint>, KnownLocations)
///@ ExportProperty
CRITTER_PROPERTY(PrivateServer, uint, ShowCritterDist1)
///@ ExportProperty
CRITTER_PROPERTY(PrivateServer, uint, ShowCritterDist2)
///@ ExportProperty
CRITTER_PROPERTY(PrivateServer, uint, ShowCritterDist3)
///@ ExportProperty
CRITTER_PROPERTY(PrivateServer, hash, ScriptId)
///@ ExportProperty
CRITTER_PROPERTY(Protected, vector<hash>, KnownLocProtoId)
///@ ExportProperty
CRITTER_PROPERTY(PrivateClient, vector<int>, ModelLayers)
///@ ExportProperty
CRITTER_PROPERTY(Protected, bool, IsHide)
///@ ExportProperty
CRITTER_PROPERTY(Protected, bool, IsNoHome)
///@ ExportProperty
CRITTER_PROPERTY(Protected, bool, IsGeck)
///@ ExportProperty
CRITTER_PROPERTY(Protected, bool, IsNoUnarmed)
///@ ExportProperty
CRITTER_PROPERTY(VirtualProtected, bool, IsNoWalk)
///@ ExportProperty
CRITTER_PROPERTY(VirtualProtected, bool, IsNoRun)
///@ ExportProperty
CRITTER_PROPERTY(Protected, bool, IsNoRotate)
///@ ExportProperty
CRITTER_PROPERTY(Public, bool, IsNoTalk)
///@ ExportProperty
CRITTER_PROPERTY(Public, bool, IsNoFlatten)
///@ ExportProperty
CRITTER_PROPERTY(Public, uint, TimeoutBattle)
///@ ExportProperty Temporary
CRITTER_PROPERTY(Protected, uint, TimeoutTransfer)
///@ ExportProperty Temporary
CRITTER_PROPERTY(Protected, uint, TimeoutRemoveFromGame)

///@ ExportProperty
MAP_PROPERTY(PrivateServer, uint, LoopTime1)
///@ ExportProperty
MAP_PROPERTY(PrivateServer, uint, LoopTime2)
///@ ExportProperty
MAP_PROPERTY(PrivateServer, uint, LoopTime3)
///@ ExportProperty
MAP_PROPERTY(PrivateServer, uint, LoopTime4)
///@ ExportProperty
MAP_PROPERTY(PrivateServer, uint, LoopTime5)
///@ ExportProperty ReadOnly Temporary
MAP_PROPERTY(PrivateServer, string, FileDir)
///@ ExportProperty ReadOnly
MAP_PROPERTY(PrivateServer, ushort, Width)
///@ ExportProperty ReadOnly
MAP_PROPERTY(PrivateServer, ushort, Height)
///@ ExportProperty ReadOnly
MAP_PROPERTY(PrivateServer, ushort, WorkHexX)
///@ ExportProperty ReadOnly
MAP_PROPERTY(PrivateServer, ushort, WorkHexY)
///@ ExportProperty ReadOnly
MAP_PROPERTY(PrivateServer, uint, LocId)
///@ ExportProperty ReadOnly
MAP_PROPERTY(PrivateServer, uint, LocMapIndex)
///@ ExportProperty
MAP_PROPERTY(PrivateServer, uchar, RainCapacity)
///@ ExportProperty
MAP_PROPERTY(PrivateServer, int, CurDayTime)
///@ ExportProperty
MAP_PROPERTY(PrivateServer, hash, ScriptId)
///@ ExportProperty
MAP_PROPERTY(PrivateServer, vector<int>, DayTime)
///@ ExportProperty
MAP_PROPERTY(PrivateServer, vector<uchar>, DayColor)
///@ ExportProperty
MAP_PROPERTY(PrivateServer, bool, IsNoLogOut)

///@ ExportProperty ReadOnly
LOCATION_PROPERTY(PrivateServer, vector<hash>, MapProtos)
///@ ExportProperty ReadOnly
LOCATION_PROPERTY(PrivateServer, vector<hash>, MapEntrances)
///@ ExportProperty ReadOnly
LOCATION_PROPERTY(PrivateServer, vector<hash>, Automaps)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, uint, MaxPlayers)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, bool, AutoGarbage)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, bool, GeckVisible)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, hash, EntranceScript)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, ushort, WorldX)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, ushort, WorldY)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, ushort, Radius)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, bool, Hidden)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, bool, ToGarbage)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, uint, Color)
///@ ExportProperty
LOCATION_PROPERTY(PrivateServer, bool, IsEncounter)

#undef GLOBAL_PROPERTY
#undef PLAYER_PROPERTY
#undef ITEM_PROPERTY
#undef CRITTER_PROPERTY
#undef MAP_PROPERTY
#undef LOCATION_PROPERTY
