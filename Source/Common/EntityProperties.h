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

    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(synctime, SynchronizedTime);
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(ident_t, LastEntityId);
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(ident_t, HistoryRecordsId);
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(nanotime, FrameTime);
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(timespan, FrameDeltaTime);
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(int32, FramesPerSecond);

    // Todo: exclude player properties from engine:
    ///@ ExportProperty Server
    FO_ENTITY_PROPERTY(uint32, LastGlobalMapTripId);
    ///@ ExportProperty Client Mutable Persistent
    FO_ENTITY_PROPERTY(int32, GlobalDayTime);
};

class PlayerProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Player";

    explicit PlayerProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    ///@ ExportProperty Server
    FO_ENTITY_PROPERTY(ident_t, ControlledCritterId);

    // Todo: exclude player properties from engine:
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(ident_t, LastControlledCritterId);
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<string>, ConnectionHost);
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<uint16>, ConnectionPort);
    ///@ ExportProperty Server Mutable Persistent
    FO_ENTITY_PROPERTY(string, Password);
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
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = ItemInit
    FO_ENTITY_PROPERTY(hstring, InitScript);
    ///@ MigrationRule Property Item IsStatic Static
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, Static);
    ///@ MigrationRule Property Item Accessory Ownership
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ItemOwnership, Ownership);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ident_t, MapId);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(mpos, Hex);
    ///@ MigrationRule Property Item CritId CritterId
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ident_t, CritterId);
    ///@ MigrationRule Property Item CritSlot CritterSlot
    ///@ MigrationRule Property Item Slot CritterSlot
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(CritterItemSlot, CritterSlot);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ident_t, ContainerId);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(any_t, ContainerStack);
    ///@ MigrationRule Property Item SubItemIds InnerItemIds
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, InnerItemIds);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, Stackable);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int32, Count);
    ///@ ExportProperty Common Mutable PublicSync Persistent Resource
    FO_ENTITY_PROPERTY(hstring, PicMap);
    ///@ ExportProperty Client Persistent
    FO_ENTITY_PROPERTY(ipos16, Offset);
    ///@ ExportProperty Client Persistent
    FO_ENTITY_PROPERTY(CornerType, Corner);
    ///@ ExportProperty Client Persistent
    FO_ENTITY_PROPERTY(bool, DisableEgg);
    ///@ MigrationRule Property Item BlockLines MultihexLines
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(vector<uint8>, MultihexLines);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(vector<mpos>, MultihexMesh);
    ///@ ExportProperty Client Persistent
    FO_ENTITY_PROPERTY(MultihexGenerationType, MultihexGeneration);
    ///@ ExportProperty Client Persistent
    FO_ENTITY_PROPERTY(bool, DrawMultihexLines);
    ///@ ExportProperty Client Persistent
    FO_ENTITY_PROPERTY(bool, DrawMultihexMesh);
    ///@ MigrationRule Property Item IsHidden Hidden
    ///@ ExportProperty Server Mutable Persistent
    FO_ENTITY_PROPERTY(bool, Hidden);
    ///@ ExportProperty Client Mutable Persistent
    FO_ENTITY_PROPERTY(bool, HideSprite);
    ///@ MigrationRule Property Item IsHiddenPicture AlwaysHideSprite
    ///@ ExportProperty Client Persistent
    FO_ENTITY_PROPERTY(bool, AlwaysHideSprite);
    ///@ MigrationRule Property Item IsNoBlock NoBlock
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, NoBlock);
    ///@ MigrationRule Property Item IsShootThru ShootThru
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, ShootThru);
    ///@ MigrationRule Property Item IsLightThru LightThru
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, LightThru);
    ///@ MigrationRule Property Item IsAlwaysView AlwaysView
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, AlwaysView);
    ///@ MigrationRule Property Item IsLight LightSource
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, LightSource);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int8, LightIntensity);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(uint8, LightDistance);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(uint8, LightFlags);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(ucolor, LightColor);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(ucolor, ColorizeColor);

    // Todo: exclude item properties from engine:
    ///@ MigrationRule Property Item SceneryScript StaticScript
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = ItemStatic
    FO_ENTITY_PROPERTY(hstring, StaticScript);
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = ItemTrigger
    FO_ENTITY_PROPERTY(hstring, TriggerScript);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, IsTrigger);
    ///@ ExportProperty Common Mutable PublicSync Persistent Resource
    FO_ENTITY_PROPERTY(hstring, PicInv);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, IsScenery);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, IsWall);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, IsTile);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, IsRoofTile);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(uint8, TileLayer);
    ///@ MigrationRule Property Item IsFlat DrawFlatten
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, DrawFlatten);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(int8, DrawOrderOffsetHexY);
    ///@ MigrationRule Property Item IsBadItem BadItem
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, BadItem);
    ///@ MigrationRule Property Item IsNoHighlight NoHighlight
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, NoHighlight);
    ///@ MigrationRule Property Item IsNoLightInfluence NoLightInfluence
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, NoLightInfluence);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, IsGag);
    ///@ MigrationRule Property Item IsColorize Colorize
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, Colorize);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(string, Lexems);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, IsTrap);
    ///@ ExportProperty Common Mutable OwnerSync Persistent
    FO_ENTITY_PROPERTY(int16, TrapValue);
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
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = CritterInit
    FO_ENTITY_PROPERTY(hstring, InitScript);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ident_t, MapId);
    ///@ ExportProperty Server
    FO_ENTITY_PROPERTY(uint32, GlobalMapTripId);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(mpos, Hex);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ipos16, HexOffset);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(uint8, Dir);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(int16, DirAngle);
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, ItemIds);
    ///@ ExportProperty Common Mutable PublicSync Persistent Resource
    FO_ENTITY_PROPERTY(hstring, ModelName);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(int32, Multihex);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int32, ScaleFactor);
    ///@ ExportProperty Server Mutable Persistent
    FO_ENTITY_PROPERTY(int32, ShowCritterDist1);
    ///@ ExportProperty Server Mutable Persistent
    FO_ENTITY_PROPERTY(int32, ShowCritterDist2);
    ///@ ExportProperty Server Mutable Persistent
    FO_ENTITY_PROPERTY(int32, ShowCritterDist3);
    ///@ ExportProperty Client Mutable
    FO_ENTITY_PROPERTY(vector<int32>, ModelLayers);
    ///@ MigrationRule Property Critter IsControlledByPlayer ControlledByPlayer
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(bool, ControlledByPlayer);
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(bool, IsChosen);
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(bool, IsPlayerOffline);
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(bool, IsAttached);
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(ident_t, AttachMaster);
    ///@ ExportProperty Client Mutable Persistent
    FO_ENTITY_PROPERTY(bool, HideSprite);
    ///@ ExportProperty Server
    FO_ENTITY_PROPERTY(int32, MovingSpeed);

    // Todo: exclude critter properties from engine:
    ///@ ExportProperty Client Mutable
    FO_ENTITY_PROPERTY(bool, SexTagFemale);
    ///@ MigrationRule Property Critter Cond Condition
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(CritterCondition, Condition);
    ///@ ExportProperty Client Mutable Persistent
    FO_ENTITY_PROPERTY(int16, NameOffset);
    ///@ ExportProperty Server Mutable Persistent
    FO_ENTITY_PROPERTY(int32, SneakCoefficient);
    ///@ ExportProperty Common Mutable OwnerSync Persistent
    FO_ENTITY_PROPERTY(int32, LookDistance);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(string, Lexems);
    ///@ ExportProperty Common Mutable OwnerSync Persistent
    FO_ENTITY_PROPERTY(bool, InSneakMode);
    ///@ MigrationRule Property Critter IsNoFlatten DeadDrawNoFlatten
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, DeadDrawNoFlatten);
    ///@ ExportProperty Client Mutable Persistent
    FO_ENTITY_PROPERTY(ucolor, ContourColor);
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
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = MapInit
    FO_ENTITY_PROPERTY(hstring, InitScript);
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(ident_t, LocId);
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(int32, LocMapIndex);
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, CritterIds);
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, ItemIds);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(msize, Size);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(mpos, WorkHex);
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(ipos32, ScrollOffset);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(irect32, ScrollAxialArea);

    // Todo: exclude map properties from engine:
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(float32, SpritesZoom);
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(float32, SpritesZoomTarget);
    ///@ MigrationRule Property Map CurDayTime FixedDayTime
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int32, FixedDayTime);
    ///@ MigrationRule Property Map DayTime DayColorTime
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(vector<int32>, DayColorTime);
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(vector<uint8>, DayColor);
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
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = LocationInit
    FO_ENTITY_PROPERTY(hstring, InitScript);
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, MapIds);
};

FO_END_NAMESPACE();
