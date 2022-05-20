//      __________        ___               ______            _
//     / ____/ __ \____  / (_);___  ___     / ____/___  ____ _(_);___  ___
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
// Copyright (c); 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software");, to deal
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

#pragma once

#include "Common.h"

#include "Entity.h"

class GameProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_CLASS_NAME = "Game";

    explicit GameProperties(Properties& props) : EntityProperties(props) { }

    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Year);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Month);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Day);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Hour);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Minute);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Second);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, TimeMultiplier);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, LastEntityId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, LastDeferredCallId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, HistoryRecordsId);
};

class PlayerProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_CLASS_NAME = "Player";

    explicit PlayerProperties(Properties& props) : EntityProperties(props) { }

    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<uint>, ConnectionIp);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<ushort>, ConnectionPort);
};

class ItemProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_CLASS_NAME = "Item";

    explicit ItemProperties(Properties& props) : EntityProperties(props) { }

    ///@ ExportProperty ScriptFuncType = ItemInit
    ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ItemOwnership, Ownership);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uint, MapId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ushort, HexX);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ushort, HexY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uint, CritId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, CritSlot);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uint, ContainerId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uint, ContainerStack);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, float, FlyEffectSpeed);
    ///@ ExportProperty Resource
    ENTITY_PROPERTY(Public, hstring, PicMap);
    ///@ ExportProperty Resource
    ENTITY_PROPERTY(Public, hstring, PicInv);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, short, OffsetX);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, short, OffsetY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, bool, Stackable);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, GroundLevel);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, Opened);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, CornerType, Corner);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, Slot);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, Weight);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, Volume);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, bool, DisableEgg);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ushort, AnimWaitBase);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ushort, AnimWaitRndMin);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ushort, AnimWaitRndMax);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimStay0);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimStay1);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimShow0);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimShow1);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimHide0);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimHide1);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, char, DrawOrderOffsetHexY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, vector<uchar>, BlockLines);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, bool, IsStatic);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsScenery);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsWall);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsCanOpen);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsScrollBlock);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsHidden);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsHiddenPicture);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsHiddenInStatic);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsFlat);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoBlock);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsShootThru);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsLightThru);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsAlwaysView);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsBadItem);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoHighlight);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsShowAnim);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsShowAnimExt);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsLight);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsGeck);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsTrap);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsTrigger);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoLightInfluence);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsGag);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsColorize);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsColorizeInv);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsCanTalk);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsRadio);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, string, Lexems);
    ///@ ExportProperty
    ENTITY_PROPERTY(PublicModifiable, short, SortValue);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uchar, Info);
    ///@ ExportProperty
    ENTITY_PROPERTY(PublicModifiable, uchar, Mode);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, char, LightIntensity);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uchar, LightDistance);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uchar, LightFlags);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, LightColor);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, Count);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, short, TrapValue);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, ushort, RadioChannel);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, ushort, RadioFlags);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, uchar, RadioBroadcastSend);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, uchar, RadioBroadcastRecv);
};

class CritterProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_CLASS_NAME = "Critter";

    explicit CritterProperties(Properties& props) : EntityProperties(props) { }

    ///@ ExportProperty ScriptFuncType = CritterInit
    ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, hstring, ModelName);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, uint, WalkTime);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, uint, RunTime);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Protected, uint, Multihex);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, MapId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, RefMapId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, hstring, RefMapPid);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, RefLocationId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, hstring, RefLocationPid);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, HexX);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, HexY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uchar, Dir);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, string, Password);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, CritterCondition, Cond);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, bool, ClientToDelete);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, ushort, WorldX);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, ushort, WorldY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Protected, uint, GlobalMapLeaderId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, GlobalMapTripId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, RefGlobalMapTripId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, RefGlobalMapLeaderId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, LastMapHexX);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, LastMapHexY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim1Alive);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim1Knockout);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim1Dead);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim2Alive);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim2Knockout);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim2Dead);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<uchar>, GlobalMapFog);
    ///@ ExportProperty
    ENTITY_PROPERTY(VirtualPrivateServer, uint, SneakCoefficient);
    ///@ ExportProperty
    ENTITY_PROPERTY(VirtualProtected, uint, LookDistance);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, int, ReplicationTime);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, TalkDistance);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, int, ScaleFactor);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, int, CurrentHp);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, MaxTalkers);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, hstring, DialogId);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, string, Lexems);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, HomeMapId);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, HomeHexX);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, HomeHexY);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uchar, HomeDir);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<uint>, KnownLocations);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, ShowCritterDist1);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, ShowCritterDist2);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, ShowCritterDist3);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, vector<hstring>, KnownLocProtoId);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateClient, vector<int>, ModelLayers);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, bool, IsHide);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, bool, IsNoHome);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, bool, IsGeck);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, bool, IsNoUnarmed);
    ///@ ExportProperty
    ENTITY_PROPERTY(VirtualProtected, bool, IsNoWalk);
    ///@ ExportProperty
    ENTITY_PROPERTY(VirtualProtected, bool, IsNoRun);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, bool, IsNoRotate);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoTalk);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoFlatten);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, TimeoutBattle);
    ///@ ExportProperty Temporary
    ENTITY_PROPERTY(Protected, uint, TimeoutTransfer);
    ///@ ExportProperty Temporary
    ENTITY_PROPERTY(Protected, uint, TimeoutRemoveFromGame);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateClient, uint, NameColor);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateClient, uint, ContourColor);
};

class MapProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_CLASS_NAME = "Map";

    explicit MapProperties(Properties& props) : EntityProperties(props) { }

    ///@ ExportProperty ScriptFuncType = MapInit
    ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, LoopTime1);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, LoopTime2);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, LoopTime3);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, LoopTime4);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, LoopTime5);
    ///@ ExportProperty ReadOnly Temporary
    ENTITY_PROPERTY(PrivateServer, string, FileDir);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, Width);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, Height);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, WorkHexX);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, WorkHexY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, LocId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, LocMapIndex);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uchar, RainCapacity);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, int, CurDayTime);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<int>, DayTime);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<uchar>, DayColor);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, IsNoLogOut);
};

class LocationProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_CLASS_NAME = "Location";

    explicit LocationProperties(Properties& props) : EntityProperties(props) { }

    // Todo: implement Location InitScript
    ///@ ExportProperty ScriptFuncType = LocationInit
    ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<hstring>, MapProtos);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<hstring>, MapEntrances);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<hstring>, Automaps);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, MaxPlayers);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, AutoGarbage);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, GeckVisible);
    ///@ ExportProperty ScriptFuncType = LocationEntrance
    ENTITY_PROPERTY(PrivateServer, hstring, EntranceScript);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, WorldX);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, WorldY);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, Radius);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, Hidden);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, ToGarbage);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, Color);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, IsEncounter);
};
