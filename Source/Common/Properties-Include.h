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

///@ ExportProperty Global ReadOnly
GLOBAL_PROPERTY(PrivateCommon, ushort, Year)
///@ ExportProperty Global ReadOnly
GLOBAL_PROPERTY(PrivateCommon, ushort, Month)
///@ ExportProperty Global ReadOnly
GLOBAL_PROPERTY(PrivateCommon, ushort, Day)
///@ ExportProperty Global ReadOnly
GLOBAL_PROPERTY(PrivateCommon, ushort, Hour)
///@ ExportProperty Global ReadOnly NoHistory
GLOBAL_PROPERTY(PrivateCommon, ushort, Minute)
///@ ExportProperty Global ReadOnly NoHistory
GLOBAL_PROPERTY(PrivateCommon, ushort, Second)
///@ ExportProperty Global ReadOnly
GLOBAL_PROPERTY(PrivateCommon, ushort, TimeMultiplier)
///@ ExportProperty Global ReadOnly NoHistory
GLOBAL_PROPERTY(PrivateServer, uint, LastEntityId)
///@ ExportProperty Global ReadOnly NoHistory
GLOBAL_PROPERTY(PrivateCommon, uint, LastDeferredCallId)
///@ ExportProperty Global ReadOnly NoHistory
GLOBAL_PROPERTY(PrivateCommon, uint, HistoryRecordsId)

///@ ExportProperty Player
PLAYER_PROPERTY(PrivateServer, vector<uint>, ConnectionIp)
///@ ExportProperty Player
PLAYER_PROPERTY(PrivateServer, vector<ushort>, ConnectionPort)

///@ ExportProperty Item ReadOnly Type = ItemOwnership
ITEM_PROPERTY(Public, uchar, Accessory)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uint, MapId)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, ushort, HexX)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, ushort, HexY)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uint, CritId)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uchar, CritSlot)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uint, ContainerId)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uint, ContainerStack)
///@ ExportProperty Item
ITEM_PROPERTY(Public, float, FlyEffectSpeed)
///@ ExportProperty Item
ITEM_PROPERTY(Public, hash, PicMap)
///@ ExportProperty Item
ITEM_PROPERTY(Public, hash, PicInv)
///@ ExportProperty Item
ITEM_PROPERTY(Public, short, OffsetX)
///@ ExportProperty Item
ITEM_PROPERTY(Public, short, OffsetY)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, bool, Stackable)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, GroundLevel)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, Opened)
///@ ExportProperty Item ReadOnly Type = CornerType
ITEM_PROPERTY(Public, uchar, Corner)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uchar, Slot)
///@ ExportProperty Item
ITEM_PROPERTY(Public, uint, Weight)
///@ ExportProperty Item
ITEM_PROPERTY(Public, uint, Volume)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, bool, DisableEgg)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, ushort, AnimWaitBase)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, ushort, AnimWaitRndMin)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, ushort, AnimWaitRndMax)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uchar, AnimStay0)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uchar, AnimStay1)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uchar, AnimShow0)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uchar, AnimShow1)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uchar, AnimHide0)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, uchar, AnimHide1)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, char, DrawOrderOffsetHexY)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, vector<uchar>, BlockLines)
///@ ExportProperty Item ReadOnly
ITEM_PROPERTY(Public, bool, IsStatic)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsScenery)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsWall)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsCanOpen)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsScrollBlock)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsHidden)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsHiddenPicture)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsHiddenInStatic)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsFlat)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsNoBlock)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsShootThru)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsLightThru)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsAlwaysView)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsBadItem)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsNoHighlight)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsShowAnim)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsShowAnimExt)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsLight)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsGeck)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsTrap)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsTrigger)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsNoLightInfluence)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsGag)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsColorize)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsColorizeInv)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsCanTalk)
///@ ExportProperty Item
ITEM_PROPERTY(Public, bool, IsRadio)
///@ ExportProperty Item
ITEM_PROPERTY(Public, string, Lexems)
///@ ExportProperty Item
ITEM_PROPERTY(PublicModifiable, short, SortValue)
///@ ExportProperty Item
ITEM_PROPERTY(Public, uchar, Info)
///@ ExportProperty Item
ITEM_PROPERTY(PublicModifiable, uchar, Mode)
///@ ExportProperty Item
ITEM_PROPERTY(Public, char, LightIntensity)
///@ ExportProperty Item
ITEM_PROPERTY(Public, uchar, LightDistance)
///@ ExportProperty Item
ITEM_PROPERTY(Public, uchar, LightFlags)
///@ ExportProperty Item
ITEM_PROPERTY(Public, uint, LightColor)
///@ ExportProperty Item
ITEM_PROPERTY(PrivateServer, hash, ScriptId)
///@ ExportProperty Item
ITEM_PROPERTY(Public, uint, Count)
///@ ExportProperty Item
ITEM_PROPERTY(Protected, short, TrapValue)
///@ ExportProperty Item
ITEM_PROPERTY(Protected, ushort, RadioChannel)
///@ ExportProperty Item
ITEM_PROPERTY(Protected, ushort, RadioFlags)
///@ ExportProperty Item
ITEM_PROPERTY(Protected, uchar, RadioBroadcastSend)
///@ ExportProperty Item
ITEM_PROPERTY(Protected, uchar, RadioBroadcastRecv)

///@ ExportProperty Critter
CRITTER_PROPERTY(Public, hash, ModelName)
///@ ExportProperty Critter
CRITTER_PROPERTY(Protected, uint, WalkTime)
///@ ExportProperty Critter
CRITTER_PROPERTY(Protected, uint, RunTime)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(Protected, uint, Multihex)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, MapId)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, RefMapId)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(PrivateServer, hash, RefMapPid)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, RefLocationId)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(PrivateServer, hash, RefLocationPid)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, ushort, HexX)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, ushort, HexY)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uchar, Dir)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(PrivateServer, string, Password)
///@ ExportProperty Critter ReadOnly Type = CritterCondition
CRITTER_PROPERTY(PrivateCommon, uchar, Cond)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(PrivateServer, bool, ClientToDelete)
///@ ExportProperty Critter NoHistory
CRITTER_PROPERTY(Protected, ushort, WorldX)
///@ ExportProperty Critter NoHistory
CRITTER_PROPERTY(Protected, ushort, WorldY)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(Protected, uint, GlobalMapLeaderId)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, GlobalMapTripId)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, RefGlobalMapTripId)
///@ ExportProperty Critter ReadOnly
CRITTER_PROPERTY(PrivateServer, uint, RefGlobalMapLeaderId)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, ushort, LastMapHexX)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, ushort, LastMapHexY)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim1Life)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim1Knockout)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim1Dead)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim2Life)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim2Knockout)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateCommon, uint, Anim2Dead)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, vector<uchar>, GlobalMapFog)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, vector<hash>, TE_FuncNum)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, vector<uint>, TE_Rate)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, vector<uint>, TE_NextTime)
///@ ExportProperty Critter ReadOnly NoHistory
CRITTER_PROPERTY(PrivateServer, vector<int>, TE_Identifier)
///@ ExportProperty Critter
CRITTER_PROPERTY(VirtualPrivateServer, uint, SneakCoefficient)
///@ ExportProperty Critter
CRITTER_PROPERTY(VirtualProtected, uint, LookDistance)
///@ ExportProperty Critter
CRITTER_PROPERTY(Public, char, Gender)
///@ ExportProperty Critter
CRITTER_PROPERTY(Protected, hash, NpcRole)
///@ ExportProperty Critter
CRITTER_PROPERTY(Protected, int, ReplicationTime)
///@ ExportProperty Critter
CRITTER_PROPERTY(Public, uint, TalkDistance)
///@ ExportProperty Critter
CRITTER_PROPERTY(Public, int, ScaleFactor)
///@ ExportProperty Critter
CRITTER_PROPERTY(Public, int, CurrentHp)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateServer, uint, MaxTalkers)
///@ ExportProperty Critter
CRITTER_PROPERTY(Public, hash, DialogId)
///@ ExportProperty Critter
CRITTER_PROPERTY(Public, string, Lexems)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateServer, uint, HomeMapId)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateServer, ushort, HomeHexX)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateServer, ushort, HomeHexY)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateServer, uchar, HomeDir)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateServer, vector<uint>, KnownLocations)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateServer, uint, ShowCritterDist1)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateServer, uint, ShowCritterDist2)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateServer, uint, ShowCritterDist3)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateServer, hash, ScriptId)
///@ ExportProperty Critter
CRITTER_PROPERTY(Protected, vector<hash>, KnownLocProtoId)
///@ ExportProperty Critter
CRITTER_PROPERTY(PrivateClient, vector<int>, ModelLayers)
///@ ExportProperty Critter
CRITTER_PROPERTY(Protected, bool, IsHide)
///@ ExportProperty Critter
CRITTER_PROPERTY(Protected, bool, IsNoHome)
///@ ExportProperty Critter
CRITTER_PROPERTY(Protected, bool, IsGeck)
///@ ExportProperty Critter
CRITTER_PROPERTY(Protected, bool, IsNoUnarmed)
///@ ExportProperty Critter
CRITTER_PROPERTY(VirtualProtected, bool, IsNoWalk)
///@ ExportProperty Critter
CRITTER_PROPERTY(VirtualProtected, bool, IsNoRun)
///@ ExportProperty Critter
CRITTER_PROPERTY(Protected, bool, IsNoRotate)
///@ ExportProperty Critter
CRITTER_PROPERTY(Public, bool, IsNoTalk)
///@ ExportProperty Critter
CRITTER_PROPERTY(Public, bool, IsNoFlatten)
///@ ExportProperty Critter
CRITTER_PROPERTY(Public, uint, TimeoutBattle)
///@ ExportProperty Critter Temporary
CRITTER_PROPERTY(Protected, uint, TimeoutTransfer)
///@ ExportProperty Critter Temporary
CRITTER_PROPERTY(Protected, uint, TimeoutRemoveFromGame)

///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, uint, LoopTime1)
///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, uint, LoopTime2)
///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, uint, LoopTime3)
///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, uint, LoopTime4)
///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, uint, LoopTime5)
///@ ExportProperty Map ReadOnly Temporary
MAP_PROPERTY(PrivateServer, string, FileDir)
///@ ExportProperty Map ReadOnly
MAP_PROPERTY(PrivateServer, ushort, Width)
///@ ExportProperty Map ReadOnly
MAP_PROPERTY(PrivateServer, ushort, Height)
///@ ExportProperty Map ReadOnly
MAP_PROPERTY(PrivateServer, ushort, WorkHexX)
///@ ExportProperty Map ReadOnly
MAP_PROPERTY(PrivateServer, ushort, WorkHexY)
///@ ExportProperty Map ReadOnly
MAP_PROPERTY(PrivateServer, uint, LocId)
///@ ExportProperty Map ReadOnly
MAP_PROPERTY(PrivateServer, uint, LocMapIndex)
///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, uchar, RainCapacity)
///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, int, CurDayTime)
///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, hash, ScriptId)
///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, vector<int>, DayTime)
///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, vector<uchar>, DayColor)
///@ ExportProperty Map
MAP_PROPERTY(PrivateServer, bool, IsNoLogOut)

///@ ExportProperty Location ReadOnly
LOCATION_PROPERTY(PrivateServer, vector<hash>, MapProtos)
///@ ExportProperty Location ReadOnly
LOCATION_PROPERTY(PrivateServer, vector<hash>, MapEntrances)
///@ ExportProperty Location ReadOnly
LOCATION_PROPERTY(PrivateServer, vector<hash>, Automaps)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, uint, MaxPlayers)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, bool, AutoGarbage)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, bool, GeckVisible)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, hash, EntranceScript)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, ushort, WorldX)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, ushort, WorldY)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, ushort, Radius)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, bool, Hidden)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, bool, ToGarbage)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, uint, Color)
///@ ExportProperty Location
LOCATION_PROPERTY(PrivateServer, bool, IsEncounter)

#undef GLOBAL_PROPERTY
#undef PLAYER_PROPERTY
#undef ITEM_PROPERTY
#undef CRITTER_PROPERTY
#undef MAP_PROPERTY
#undef LOCATION_PROPERTY
