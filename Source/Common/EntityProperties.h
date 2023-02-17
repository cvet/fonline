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

    explicit GameProperties(Properties& props) :
        EntityProperties(props)
    {
    }

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
    ENTITY_PROPERTY(PrivateServer, ident_t, LastEntityId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ident_t, LastDeferredCallId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ident_t, HistoryRecordsId);
};

class PlayerProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_CLASS_NAME = "Player";

    explicit PlayerProperties(Properties& props) :
        EntityProperties(props)
    {
    }

    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<ident_t>, OwnedCritterIds);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, string, Password);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<uint>, ConnectionIp);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<ushort>, ConnectionPort);
};

class ItemProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_CLASS_NAME = "Item";

    explicit ItemProperties(Properties& props) :
        EntityProperties(props)
    {
    }

    ///@ ExportProperty ScriptFuncType = ItemInit Alias = ScriptId
    ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ ExportProperty ScriptFuncType = ItemScenery
    ENTITY_PROPERTY(PrivateServer, hstring, SceneryScript);
    ///@ ExportProperty ScriptFuncType = ItemTrigger
    ENTITY_PROPERTY(PrivateServer, hstring, TriggerScript);
    ///@ ExportProperty ReadOnly Alias = Accessory
    ENTITY_PROPERTY(PrivateCommon, ItemOwnership, Ownership);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ident_t, MapId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, HexX);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, HexY);
    ///@ ExportProperty ReadOnly Alias = CritId
    ENTITY_PROPERTY(PrivateCommon, ident_t, CritterId);
    ///@ ExportProperty ReadOnly Alias = CritSlot Alias = Slot
    ENTITY_PROPERTY(PrivateCommon, uchar, CritterSlot);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ident_t, ContainerId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, ContainerStack);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<ident_t>, SubItemIds);
    ///@ ExportProperty Resource
    ENTITY_PROPERTY(Public, hstring, PicMap);
    ///@ ExportProperty Resource
    ENTITY_PROPERTY(Public, hstring, PicInv);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, Opened);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, short, OffsetX);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, short, OffsetY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, float, FlyEffectSpeed);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, Stackable);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, GroundLevel);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, CornerType, Corner);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Weight);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Volume);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, DisableEgg);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, AnimWaitBase);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, AnimWaitRndMin);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, AnimWaitRndMax);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uchar, AnimStay0);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uchar, AnimStay1);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uchar, AnimShow0);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uchar, AnimShow1);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uchar, AnimHide0);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uchar, AnimHide1);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, char, DrawOrderOffsetHexY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, vector<uchar>, BlockLines);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, IsStatic);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, IsScenery);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, IsWall);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, IsCanOpen);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, IsScrollBlock);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsHidden);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, IsHiddenPicture);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, IsHiddenInStatic);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, bool, IsFlat);
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

    explicit CritterProperties(Properties& props) :
        EntityProperties(props)
    {
    }

    ///@ ExportProperty ScriptFuncType = CritterInit Alias = ScriptId
    ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateClient, string, CustomName);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, hstring, ModelName);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Protected, uint, Multihex);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ident_t, MapId);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, ushort, WorldX);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, ushort, WorldY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Protected, ident_t, GlobalMapLeaderId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, GlobalMapTripId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ident_t, LastMapId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, hstring, LastMapPid);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ident_t, LastLocationId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, hstring, LastLocationPid);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ident_t, LastGlobalMapLeaderId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, MapLeaveHexX);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, MapLeaveHexY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, HexX);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, HexY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, short, HexOffsX);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, short, HexOffsY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uchar, Dir);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, short, DirAngle);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<ident_t>, ItemIds);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, CritterCondition, Cond);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, bool, ClientToDelete);
    ///@ ExportProperty ReadOnly Alias = Anim1Life
    ENTITY_PROPERTY(PrivateCommon, uint, Anim1Alive);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim1Knockout);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim1Dead);
    ///@ ExportProperty ReadOnly Alias = Anim2Life
    ENTITY_PROPERTY(PrivateCommon, uint, Anim2Alive);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim2Knockout);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim2Dead);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateClient, short, NameOffset);
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
    ENTITY_PROPERTY(PrivateServer, ident_t, HomeMapId);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, hstring, HomeMapPid);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, HomeHexX);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, HomeHexY);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uchar, HomeDir);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<ident_t>, KnownLocations);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, ShowCritterDist1);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, ShowCritterDist2);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, ShowCritterDist3);
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, vector<hstring>, KnownLocProtoId);
    ///@ ExportProperty Temporary
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
    ENTITY_PROPERTY(Public, bool, IsNoTalk);
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoFlatten);
    ///@ ExportProperty Temporary
    ENTITY_PROPERTY(Protected, uint, TimeoutRemoveFromGame);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateClient, uint, NameColor);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateClient, uint, ContourColor);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<int>, TE_Identifier);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<uint>, TE_FireTime);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<hstring>, TE_FuncName);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<uint>, TE_Rate);
    ///@ ExportProperty
    ENTITY_PROPERTY(VirtualPrivateClient, bool, IsSexTagFemale);
    ///@ ExportProperty
    ENTITY_PROPERTY(VirtualPrivateClient, bool, IsModelInCombatMode);
};

class MapProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_CLASS_NAME = "Map";

    explicit MapProperties(Properties& props) :
        EntityProperties(props)
    {
    }

    ///@ ExportProperty ScriptFuncType = MapInit Alias = ScriptId
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
    ENTITY_PROPERTY(PrivateClient, string, FileDir);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Width);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Height);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateClient, ushort, WorkHexX);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateClient, ushort, WorkHexY);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ident_t, LocId);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, LocMapIndex);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<ident_t>, CritterIds);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<ident_t>, ItemIds);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateCommon, uchar, RainCapacity);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateCommon, int, CurDayTime);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateCommon, vector<int>, DayTime);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateCommon, vector<uchar>, DayColor);
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, IsNoLogOut);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateClient, float, SpritesZoom);
};

class LocationProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_CLASS_NAME = "Location";

    explicit LocationProperties(Properties& props) :
        EntityProperties(props)
    {
    }

    // Todo: implement Location InitScript
    ///@ ExportProperty ScriptFuncType = LocationInit Alias = ScriptId
    ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<ident_t>, MapIds);
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
