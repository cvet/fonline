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

#pragma once

#include "Common.h"

#include "Entity.h"

class PropertyRegistratorsHolder
{
public:
    explicit PropertyRegistratorsHolder(bool is_server);
    virtual ~PropertyRegistratorsHolder() = default;

    [[nodiscard]] auto GetPropertyRegistrator(string_view class_name) const -> const PropertyRegistrator*;
    [[nodiscard]] auto GetPropertyRegistratorForEdit(string_view class_name) -> PropertyRegistrator*;

    void ResetPropertyRegistrators();

    template<typename T, std::enable_if_t<std::is_convertible_v<T, EntityProperties>, int> = 0>
    void CreatePropertyRegistrator()
    {
        auto* registrator = new PropertyRegistrator(T::ENTITY_CLASS_NAME, _isServer);
        T::FillProperties(registrator);
    }

    void FinalizePropertyRegistration();

private:
    bool _isServer;
    unordered_map<string_view, PropertyRegistrator*> _registrators {};
    bool _finalized {};
};

class GameProperties : public EntityProperties
{
public:
    explicit GameProperties(Properties& props) : EntityProperties(props) { }

    static constexpr string_view ENTITY_CLASS_NAME = "Game";
    static void FillProperties(PropertyRegistrator* registrator);

    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Year, 0)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Month, 1)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Day, 2)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Hour, 3)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Minute, 4)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, Second, 5)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, TimeMultiplier, 6)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, LastEntityId, 7)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, LastDeferredCallId, 8)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, HistoryRecordsId, 9)
};

class PlayerProperties : public EntityProperties
{
public:
    explicit PlayerProperties(Properties& props) : EntityProperties(props) { }

    static constexpr string_view ENTITY_CLASS_NAME = "Player";
    static void FillProperties(PropertyRegistrator* registrator);

    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<uint>, ConnectionIp, 0)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<ushort>, ConnectionPort, 1)
};

class ItemProperties : public EntityProperties
{
public:
    explicit ItemProperties(Properties& props) : EntityProperties(props) { }

    static constexpr string_view ENTITY_CLASS_NAME = "Item";
    static void FillProperties(PropertyRegistrator* registrator);

    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ItemOwnership, Ownership, 0)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uint, MapId, 1)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ushort, HexX, 2)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ushort, HexY, 3)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uint, CritId, 4)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, CritSlot, 5)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uint, ContainerId, 6)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uint, ContainerStack, 7)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, float, FlyEffectSpeed, 8)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, hstring, PicMap, 9)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, hstring, PicInv, 10)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, short, OffsetX, 11)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, short, OffsetY, 12)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, bool, Stackable, 13)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, GroundLevel, 14)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, Opened, 15)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, CornerType, Corner, 16)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, Slot, 17)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, Weight, 18)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, Volume, 19)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, bool, DisableEgg, 20)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ushort, AnimWaitBase, 21)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ushort, AnimWaitRndMin, 22)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, ushort, AnimWaitRndMax, 23)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimStay0, 24)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimStay1, 25)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimShow0, 26)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimShow1, 27)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimHide0, 28)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, uchar, AnimHide1, 29)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, char, DrawOrderOffsetHexY, 30)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, vector<uchar>, BlockLines, 31)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Public, bool, IsStatic, 32)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsScenery, 33)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsWall, 34)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsCanOpen, 35)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsScrollBlock, 36)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsHidden, 37)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsHiddenPicture, 38)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsHiddenInStatic, 39)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsFlat, 40)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoBlock, 41)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsShootThru, 42)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsLightThru, 43)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsAlwaysView, 44)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsBadItem, 45)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoHighlight, 46)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsShowAnim, 47)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsShowAnimExt, 48)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsLight, 49)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsGeck, 50)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsTrap, 51)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsTrigger, 52)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoLightInfluence, 53)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsGag, 54)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsColorize, 55)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsColorizeInv, 56)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsCanTalk, 57)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsRadio, 58)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, string, Lexems, 59)
    ///@ ExportProperty
    ENTITY_PROPERTY(PublicModifiable, short, SortValue, 60)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uchar, Info, 61)
    ///@ ExportProperty
    ENTITY_PROPERTY(PublicModifiable, uchar, Mode, 62)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, char, LightIntensity, 63)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uchar, LightDistance, 64)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uchar, LightFlags, 65)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, LightColor, 66)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, hstring, ScriptId, 67)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, Count, 68)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, short, TrapValue, 69)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, ushort, RadioChannel, 70)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, ushort, RadioFlags, 71)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, uchar, RadioBroadcastSend, 72)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, uchar, RadioBroadcastRecv, 73)
};

class CritterProperties : public EntityProperties
{
public:
    explicit CritterProperties(Properties& props) : EntityProperties(props) { }

    static constexpr string_view ENTITY_CLASS_NAME = "Critter";
    static void FillProperties(PropertyRegistrator* registrator);

    ///@ ExportProperty
    ENTITY_PROPERTY(Public, hstring, ModelName, 0)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, uint, WalkTime, 1)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, uint, RunTime, 2)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Protected, uint, Multihex, 3)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, MapId, 4)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, RefMapId, 5)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, hstring, RefMapPid, 6)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, RefLocationId, 7)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, hstring, RefLocationPid, 8)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, HexX, 9)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, ushort, HexY, 10)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uchar, Dir, 11)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, string, Password, 12)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, CritterCondition, Cond, 13)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, bool, ClientToDelete, 14)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, ushort, WorldX, 15)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, ushort, WorldY, 16)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(Protected, uint, GlobalMapLeaderId, 17)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, GlobalMapTripId, 18)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, RefGlobalMapTripId, 19)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, RefGlobalMapLeaderId, 20)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, LastMapHexX, 21)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, LastMapHexY, 22)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim1Life, 23)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim1Knockout, 24)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim1Dead, 25)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim2Life, 26)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim2Knockout, 27)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateCommon, uint, Anim2Dead, 28)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<uchar>, GlobalMapFog, 29)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<hstring>, TE_FuncNum, 30)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<uint>, TE_Rate, 31)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<uint>, TE_NextTime, 32)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<int>, TE_Identifier, 33)
    ///@ ExportProperty
    ENTITY_PROPERTY(VirtualPrivateServer, uint, SneakCoefficient, 34)
    ///@ ExportProperty
    ENTITY_PROPERTY(VirtualProtected, uint, LookDistance, 35)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, char, Gender, 36)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, hstring, NpcRole, 37)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, int, ReplicationTime, 38)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, TalkDistance, 39)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, int, ScaleFactor, 40)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, int, CurrentHp, 41)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, MaxTalkers, 42)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, hstring, DialogId, 43)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, string, Lexems, 44)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, HomeMapId, 45)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, HomeHexX, 46)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, HomeHexY, 47)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uchar, HomeDir, 48)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<uint>, KnownLocations, 49)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, ShowCritterDist1, 50)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, ShowCritterDist2, 51)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, ShowCritterDist3, 52)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, hstring, ScriptId, 53)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, vector<hstring>, KnownLocProtoId, 54)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateClient, vector<int>, ModelLayers, 55)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, bool, IsHide, 56)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, bool, IsNoHome, 57)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, bool, IsGeck, 58)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, bool, IsNoUnarmed, 59)
    ///@ ExportProperty
    ENTITY_PROPERTY(VirtualProtected, bool, IsNoWalk, 60)
    ///@ ExportProperty
    ENTITY_PROPERTY(VirtualProtected, bool, IsNoRun, 61)
    ///@ ExportProperty
    ENTITY_PROPERTY(Protected, bool, IsNoRotate, 62)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoTalk, 63)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, bool, IsNoFlatten, 64)
    ///@ ExportProperty
    ENTITY_PROPERTY(Public, uint, TimeoutBattle, 65)
    ///@ ExportProperty Temporary
    ENTITY_PROPERTY(Protected, uint, TimeoutTransfer, 66)
    ///@ ExportProperty Temporary
    ENTITY_PROPERTY(Protected, uint, TimeoutRemoveFromGame, 67)
};

class MapProperties : public EntityProperties
{
public:
    explicit MapProperties(Properties& props) : EntityProperties(props) { }

    static constexpr string_view ENTITY_CLASS_NAME = "Map";
    static void FillProperties(PropertyRegistrator* registrator);

    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, LoopTime1, 0)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, LoopTime2, 1)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, LoopTime3, 2)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, LoopTime4, 3)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, LoopTime5, 4)
    ///@ ExportProperty ReadOnly Temporary
    ENTITY_PROPERTY(PrivateServer, string, FileDir, 5)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, Width, 6)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, Height, 7)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, WorkHexX, 8)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, ushort, WorkHexY, 9)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, LocId, 10)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, uint, LocMapIndex, 11)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uchar, RainCapacity, 12)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, int, CurDayTime, 13)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, hstring, ScriptId, 14)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<int>, DayTime, 15)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, vector<uchar>, DayColor, 16)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, IsNoLogOut, 17)
};

class LocationProperties : public EntityProperties
{
public:
    explicit LocationProperties(Properties& props) : EntityProperties(props) { }

    static constexpr string_view ENTITY_CLASS_NAME = "Location";
    static void FillProperties(PropertyRegistrator* registrator);

    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<hstring>, MapProtos, 0)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<hstring>, MapEntrances, 1)
    ///@ ExportProperty ReadOnly
    ENTITY_PROPERTY(PrivateServer, vector<hstring>, Automaps, 2)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, MaxPlayers, 3)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, AutoGarbage, 4)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, GeckVisible, 5)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, hstring, EntranceScript, 6)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, WorldX, 7)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, WorldY, 8)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, ushort, Radius, 9)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, Hidden, 10)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, ToGarbage, 11)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, uint, Color, 12)
    ///@ ExportProperty
    ENTITY_PROPERTY(PrivateServer, bool, IsEncounter, 13)
};
