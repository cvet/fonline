//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#pragma once

#include "Common.h"

#include "Entity.h"
#include "Geometry.h"

FO_BEGIN_NAMESPACE();

class GameProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Game";

    explicit GameProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, synctime, SynchronizedTime);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, ident_t, LastEntityId);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, ident_t, HistoryRecordsId);
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateServer, uint32, LastGlobalMapTripId);
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateCommon, nanotime, FrameTime);
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateCommon, timespan, FrameDeltaTime);
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateCommon, int32, FramesPerSecond);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateClient, int32, GlobalDayTime);
};

class PlayerProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Player";

    explicit PlayerProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateServer, ident_t, ControlledCritterId);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, ident_t, LastControlledCritterId);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateServer, vector<uint32>, ConnectionIp);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateServer, vector<uint16>, ConnectionPort);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateServer, string, Password);
};

class ItemProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Item";

    explicit ItemProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    ///@ MigrationRule Property Item ScriptId InitScript
    ///@ ExportProperty ScriptFuncType = ItemInit
    FO_ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ MigrationRule Property Item IsStatic Static
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, Static);
    ///@ MigrationRule Property Item Accessory Ownership
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, ItemOwnership, Ownership);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, ident_t, MapId);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, mpos, Hex);
    ///@ MigrationRule Property Item CritId CritterId
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, ident_t, CritterId);
    ///@ MigrationRule Property Item CritSlot CritterSlot
    ///@ MigrationRule Property Item Slot CritterSlot
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, CritterItemSlot, CritterSlot);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, ident_t, ContainerId);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, any_t, ContainerStack);
    ///@ MigrationRule Property Item SubItemIds InnerItemIds
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, vector<ident_t>, InnerItemIds);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, Stackable);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, int32, Count);
    ///@ ExportProperty Resource
    FO_ENTITY_PROPERTY(Public, hstring, PicMap);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, ipos16, Offset);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, CornerType, Corner);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, DisableEgg);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, vector<uint8>, BlockLines);
    ///@ MigrationRule Property Item IsScrollBlock ScrollBlock
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, ScrollBlock);
    ///@ MigrationRule Property Item IsHidden Hidden
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateServer, bool, Hidden);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateClient, bool, HideSprite);
    ///@ MigrationRule Property Item IsHiddenPicture AlwaysHideSprite
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, AlwaysHideSprite);
    ///@ MigrationRule Property Item IsHiddenInStatic HiddenInStatic
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, HiddenInStatic);
    ///@ MigrationRule Property Item IsNoBlock NoBlock
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, NoBlock);
    ///@ MigrationRule Property Item IsShootThru ShootThru
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, ShootThru);
    ///@ MigrationRule Property Item IsLightThru LightThru
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, LightThru);
    ///@ MigrationRule Property Item IsAlwaysView AlwaysView
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, AlwaysView);
    ///@ MigrationRule Property Item IsLight LightSource
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, LightSource);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, int8, LightIntensity);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, uint8, LightDistance);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, uint8, LightFlags);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, ucolor, LightColor);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, ucolor, ColorizeColor);

    // Todo: exclude item properties from engine:
    ///@ MigrationRule Property Item SceneryScript StaticScript
    ///@ ExportProperty ScriptFuncType = ItemStatic
    FO_ENTITY_PROPERTY(PrivateCommon, hstring, StaticScript);
    ///@ ExportProperty ScriptFuncType = ItemTrigger
    FO_ENTITY_PROPERTY(PrivateServer, hstring, TriggerScript);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, IsTrigger);
    ///@ ExportProperty Resource
    FO_ENTITY_PROPERTY(Public, hstring, PicInv);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, float32, FlyEffectSpeed);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, IsScenery);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, IsWall);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, IsTile);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, IsRoofTile);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, uint8, TileLayer);
    ///@ MigrationRule Property Item IsFlat DrawFlatten
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, DrawFlatten);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, int8, DrawOrderOffsetHexY);
    ///@ MigrationRule Property Item IsBadItem BadItem
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, BadItem);
    ///@ MigrationRule Property Item IsNoHighlight NoHighlight
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, NoHighlight);
    ///@ MigrationRule Property Item IsNoLightInfluence NoLightInfluence
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, NoLightInfluence);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, IsGag);
    ///@ MigrationRule Property Item IsColorize Colorize
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, Colorize);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, string, Lexems);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PublicModifiable, int16, SortValue);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, IsTrap);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Protected, int16, TrapValue);
    ///@ MigrationRule Property Item IsCanOpen CanOpen
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, bool, CanOpen);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, Opened);
};

class CritterProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Critter";

    explicit CritterProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    ///@ MigrationRule Property Critter ScriptId InitScript
    ///@ ExportProperty ScriptFuncType = CritterInit
    FO_ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, ident_t, MapId);
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateServer, uint32, GlobalMapTripId);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, mpos, Hex);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, ipos16, HexOffset);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, uint8, Dir);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, int16, DirAngle);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, vector<ident_t>, ItemIds);
    ///@ ExportProperty Resource
    FO_ENTITY_PROPERTY(Public, hstring, ModelName);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(Protected, int32, Multihex);
    ///@ MigrationRule Property Critter Anim1Life AliveStateAnim
    ///@ MigrationRule Property Critter Anim1Alive AliveStateAnim
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, CritterStateAnim, AliveStateAnim);
    ///@ MigrationRule Property Critter Anim1Knockout KnockoutStateAnim
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, CritterStateAnim, KnockoutStateAnim);
    ///@ MigrationRule Property Critter Anim1Dead DeadStateAnim
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, CritterStateAnim, DeadStateAnim);
    ///@ MigrationRule Property Critter Anim2Life AliveActionAnim
    ///@ MigrationRule Property Critter Anim2Alive AliveActionAnim
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, CritterActionAnim, AliveActionAnim);
    ///@ MigrationRule Property Critter Anim2Knockout KnockoutActionAnim
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, CritterActionAnim, KnockoutActionAnim);
    ///@ MigrationRule Property Critter Anim2Dead DeadActionAnim
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, CritterActionAnim, DeadActionAnim);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, int32, ScaleFactor);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateServer, int32, ShowCritterDist1);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateServer, int32, ShowCritterDist2);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateServer, int32, ShowCritterDist3);
    ///@ ExportProperty Temporary
    FO_ENTITY_PROPERTY(PrivateClient, vector<int32>, ModelLayers);
    ///@ MigrationRule Property Critter IsControlledByPlayer ControlledByPlayer
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateCommon, bool, ControlledByPlayer);
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateClient, bool, IsChosen);
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateClient, bool, IsPlayerOffline);
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateCommon, bool, IsAttached);
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateCommon, ident_t, AttachMaster);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateClient, bool, HideSprite);
    ///@ ExportProperty ReadOnly Temporary
    FO_ENTITY_PROPERTY(PrivateServer, int32, MovingSpeed);

    // Todo: exclude critter properties from engine:
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateClient, bool, SexTagFemale);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateClient, bool, ModelInCombatMode);
    ///@ MigrationRule Property Critter Cond Condition
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, CritterCondition, Condition);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateClient, int16, NameOffset);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateServer, int32, SneakCoefficient);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Protected, int32, LookDistance);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, string, Lexems);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Protected, bool, InSneakMode);
    ///@ MigrationRule Property Critter IsNoFlatten DeadDrawNoFlatten
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, bool, DeadDrawNoFlatten);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateClient, ucolor, ContourColor);
};

class MapProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Map";

    explicit MapProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    ///@ MigrationRule Property Map ScriptId InitScript
    ///@ ExportProperty ScriptFuncType = MapInit
    FO_ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, ident_t, LocId);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, int32, LocMapIndex);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, vector<ident_t>, CritterIds);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, vector<ident_t>, ItemIds);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, msize, Size);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, mpos, WorkHex);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateCommon, ident_t, WorkEntityId);

    // Todo: exclude map properties from engine:
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateClient, float32, SpritesZoom);
    ///@ MigrationRule Property Map CurDayTime FixedDayTime
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(Public, int32, FixedDayTime);
    ///@ MigrationRule Property Map DayTime DayColorTime
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateCommon, vector<int32>, DayColorTime);
    ///@ ExportProperty
    FO_ENTITY_PROPERTY(PrivateCommon, vector<uint8>, DayColor);
};

class LocationProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Location";

    explicit LocationProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    ///@ MigrationRule Property Location ScriptId InitScript
    // Todo: implement Location InitScript
    ///@ ExportProperty ScriptFuncType = LocationInit
    FO_ENTITY_PROPERTY(PrivateServer, hstring, InitScript);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, vector<ident_t>, MapIds);
    ///@ ExportProperty ReadOnly
    FO_ENTITY_PROPERTY(PrivateServer, vector<hstring>, MapProtos);
};

FO_END_NAMESPACE();
