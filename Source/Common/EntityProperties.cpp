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

#include "EntityProperties.h"

PropertyRegistratorsHolder::PropertyRegistratorsHolder(bool is_server) : _isServer {is_server}
{
    ResetPropertyRegistrators();
}

auto PropertyRegistratorsHolder::GetPropertyRegistrator(string_view class_name) const -> const PropertyRegistrator*
{
    const auto it = _registrators.find(class_name);
    RUNTIME_ASSERT(it != _registrators.end());
    return it->second;
}

auto PropertyRegistratorsHolder::GetPropertyRegistratorForEdit(string_view class_name) -> PropertyRegistrator*
{
    RUNTIME_ASSERT(!_finalized);

    if (_finalized) {
        return nullptr;
    }

    const auto it = _registrators.find(class_name);
    if (it != _registrators.end()) {
        return it->second;
    }

    auto* registrator = new PropertyRegistrator(class_name, _isServer);
    _registrators.emplace(class_name, registrator);
    return registrator;
}

void PropertyRegistratorsHolder::ResetPropertyRegistrators()
{
    _finalized = false;

    CreatePropertyRegistrator<GameProperties>();
    CreatePropertyRegistrator<PlayerProperties>();
    CreatePropertyRegistrator<ItemProperties>();
    CreatePropertyRegistrator<CritterProperties>();
    CreatePropertyRegistrator<MapProperties>();
    CreatePropertyRegistrator<LocationProperties>();
}

void PropertyRegistratorsHolder::FinalizePropertyRegistration()
{
    RUNTIME_ASSERT(!_finalized);

    _finalized = true;
}

EntityProperties::EntityProperties(Properties& props) : _propsRef {props}
{
}

void GameProperties::FillProperties(PropertyRegistrator* registrator)
{
    REGISTER_ENTITY_PROPERTY(Year);
    REGISTER_ENTITY_PROPERTY(Month);
    REGISTER_ENTITY_PROPERTY(Day);
    REGISTER_ENTITY_PROPERTY(Hour);
    REGISTER_ENTITY_PROPERTY(Minute);
    REGISTER_ENTITY_PROPERTY(Second);
    REGISTER_ENTITY_PROPERTY(TimeMultiplier);
    REGISTER_ENTITY_PROPERTY(LastEntityId);
    REGISTER_ENTITY_PROPERTY(LastDeferredCallId);
    REGISTER_ENTITY_PROPERTY(HistoryRecordsId);
}

void PlayerProperties::FillProperties(PropertyRegistrator* registrator)
{
    REGISTER_ENTITY_PROPERTY(ConnectionIp);
    REGISTER_ENTITY_PROPERTY(ConnectionPort);
}

void ItemProperties::FillProperties(PropertyRegistrator* registrator)
{
    REGISTER_ENTITY_PROPERTY(Ownership);
    REGISTER_ENTITY_PROPERTY(MapId);
    REGISTER_ENTITY_PROPERTY(HexX);
    REGISTER_ENTITY_PROPERTY(HexY);
    REGISTER_ENTITY_PROPERTY(CritId);
    REGISTER_ENTITY_PROPERTY(CritSlot);
    REGISTER_ENTITY_PROPERTY(ContainerId);
    REGISTER_ENTITY_PROPERTY(ContainerStack);
    REGISTER_ENTITY_PROPERTY(FlyEffectSpeed);
    REGISTER_ENTITY_PROPERTY(PicMap);
    REGISTER_ENTITY_PROPERTY(PicInv);
    REGISTER_ENTITY_PROPERTY(OffsetX);
    REGISTER_ENTITY_PROPERTY(OffsetY);
    REGISTER_ENTITY_PROPERTY(Stackable);
    REGISTER_ENTITY_PROPERTY(GroundLevel);
    REGISTER_ENTITY_PROPERTY(Opened);
    REGISTER_ENTITY_PROPERTY(Corner);
    REGISTER_ENTITY_PROPERTY(Slot);
    REGISTER_ENTITY_PROPERTY(Weight);
    REGISTER_ENTITY_PROPERTY(Volume);
    REGISTER_ENTITY_PROPERTY(DisableEgg);
    REGISTER_ENTITY_PROPERTY(AnimWaitBase);
    REGISTER_ENTITY_PROPERTY(AnimWaitRndMin);
    REGISTER_ENTITY_PROPERTY(AnimWaitRndMax);
    REGISTER_ENTITY_PROPERTY(AnimStay0);
    REGISTER_ENTITY_PROPERTY(AnimStay1);
    REGISTER_ENTITY_PROPERTY(AnimShow0);
    REGISTER_ENTITY_PROPERTY(AnimShow1);
    REGISTER_ENTITY_PROPERTY(AnimHide0);
    REGISTER_ENTITY_PROPERTY(AnimHide1);
    REGISTER_ENTITY_PROPERTY(DrawOrderOffsetHexY);
    REGISTER_ENTITY_PROPERTY(BlockLines);
    REGISTER_ENTITY_PROPERTY(IsStatic);
    REGISTER_ENTITY_PROPERTY(IsScenery);
    REGISTER_ENTITY_PROPERTY(IsWall);
    REGISTER_ENTITY_PROPERTY(IsCanOpen);
    REGISTER_ENTITY_PROPERTY(IsScrollBlock);
    REGISTER_ENTITY_PROPERTY(IsHidden);
    REGISTER_ENTITY_PROPERTY(IsHiddenPicture);
    REGISTER_ENTITY_PROPERTY(IsHiddenInStatic);
    REGISTER_ENTITY_PROPERTY(IsFlat);
    REGISTER_ENTITY_PROPERTY(IsNoBlock);
    REGISTER_ENTITY_PROPERTY(IsShootThru);
    REGISTER_ENTITY_PROPERTY(IsLightThru);
    REGISTER_ENTITY_PROPERTY(IsAlwaysView);
    REGISTER_ENTITY_PROPERTY(IsBadItem);
    REGISTER_ENTITY_PROPERTY(IsNoHighlight);
    REGISTER_ENTITY_PROPERTY(IsShowAnim);
    REGISTER_ENTITY_PROPERTY(IsShowAnimExt);
    REGISTER_ENTITY_PROPERTY(IsLight);
    REGISTER_ENTITY_PROPERTY(IsGeck);
    REGISTER_ENTITY_PROPERTY(IsTrap);
    REGISTER_ENTITY_PROPERTY(IsTrigger);
    REGISTER_ENTITY_PROPERTY(IsNoLightInfluence);
    REGISTER_ENTITY_PROPERTY(IsGag);
    REGISTER_ENTITY_PROPERTY(IsColorize);
    REGISTER_ENTITY_PROPERTY(IsColorizeInv);
    REGISTER_ENTITY_PROPERTY(IsCanTalk);
    REGISTER_ENTITY_PROPERTY(IsRadio);
    REGISTER_ENTITY_PROPERTY(Lexems);
    REGISTER_ENTITY_PROPERTY(SortValue);
    REGISTER_ENTITY_PROPERTY(Info);
    REGISTER_ENTITY_PROPERTY(Mode);
    REGISTER_ENTITY_PROPERTY(LightIntensity);
    REGISTER_ENTITY_PROPERTY(LightDistance);
    REGISTER_ENTITY_PROPERTY(LightFlags);
    REGISTER_ENTITY_PROPERTY(LightColor);
    REGISTER_ENTITY_PROPERTY(ScriptId);
    REGISTER_ENTITY_PROPERTY(Count);
    REGISTER_ENTITY_PROPERTY(TrapValue);
    REGISTER_ENTITY_PROPERTY(RadioChannel);
    REGISTER_ENTITY_PROPERTY(RadioFlags);
    REGISTER_ENTITY_PROPERTY(RadioBroadcastSend);
    REGISTER_ENTITY_PROPERTY(RadioBroadcastRecv);
}

void CritterProperties::FillProperties(PropertyRegistrator* registrator)
{
    REGISTER_ENTITY_PROPERTY(ModelName);
    REGISTER_ENTITY_PROPERTY(WalkTime);
    REGISTER_ENTITY_PROPERTY(RunTime);
    REGISTER_ENTITY_PROPERTY(Multihex);
    REGISTER_ENTITY_PROPERTY(MapId);
    REGISTER_ENTITY_PROPERTY(RefMapId);
    REGISTER_ENTITY_PROPERTY(RefMapPid);
    REGISTER_ENTITY_PROPERTY(RefLocationId);
    REGISTER_ENTITY_PROPERTY(RefLocationPid);
    REGISTER_ENTITY_PROPERTY(HexX);
    REGISTER_ENTITY_PROPERTY(HexY);
    REGISTER_ENTITY_PROPERTY(Dir);
    REGISTER_ENTITY_PROPERTY(Password);
    REGISTER_ENTITY_PROPERTY(Cond);
    REGISTER_ENTITY_PROPERTY(ClientToDelete);
    REGISTER_ENTITY_PROPERTY(WorldX);
    REGISTER_ENTITY_PROPERTY(WorldY);
    REGISTER_ENTITY_PROPERTY(GlobalMapLeaderId);
    REGISTER_ENTITY_PROPERTY(GlobalMapTripId);
    REGISTER_ENTITY_PROPERTY(RefGlobalMapTripId);
    REGISTER_ENTITY_PROPERTY(RefGlobalMapLeaderId);
    REGISTER_ENTITY_PROPERTY(LastMapHexX);
    REGISTER_ENTITY_PROPERTY(LastMapHexY);
    REGISTER_ENTITY_PROPERTY(Anim1Life);
    REGISTER_ENTITY_PROPERTY(Anim1Knockout);
    REGISTER_ENTITY_PROPERTY(Anim1Dead);
    REGISTER_ENTITY_PROPERTY(Anim2Life);
    REGISTER_ENTITY_PROPERTY(Anim2Knockout);
    REGISTER_ENTITY_PROPERTY(Anim2Dead);
    REGISTER_ENTITY_PROPERTY(GlobalMapFog);
    REGISTER_ENTITY_PROPERTY(TE_FuncNum);
    REGISTER_ENTITY_PROPERTY(TE_Rate);
    REGISTER_ENTITY_PROPERTY(TE_NextTime);
    REGISTER_ENTITY_PROPERTY(TE_Identifier);
    REGISTER_ENTITY_PROPERTY(SneakCoefficient);
    REGISTER_ENTITY_PROPERTY(LookDistance);
    REGISTER_ENTITY_PROPERTY(Gender);
    REGISTER_ENTITY_PROPERTY(NpcRole);
    REGISTER_ENTITY_PROPERTY(ReplicationTime);
    REGISTER_ENTITY_PROPERTY(TalkDistance);
    REGISTER_ENTITY_PROPERTY(ScaleFactor);
    REGISTER_ENTITY_PROPERTY(CurrentHp);
    REGISTER_ENTITY_PROPERTY(MaxTalkers);
    REGISTER_ENTITY_PROPERTY(DialogId);
    REGISTER_ENTITY_PROPERTY(Lexems);
    REGISTER_ENTITY_PROPERTY(HomeMapId);
    REGISTER_ENTITY_PROPERTY(HomeHexX);
    REGISTER_ENTITY_PROPERTY(HomeHexY);
    REGISTER_ENTITY_PROPERTY(HomeDir);
    REGISTER_ENTITY_PROPERTY(KnownLocations);
    REGISTER_ENTITY_PROPERTY(ShowCritterDist1);
    REGISTER_ENTITY_PROPERTY(ShowCritterDist2);
    REGISTER_ENTITY_PROPERTY(ShowCritterDist3);
    REGISTER_ENTITY_PROPERTY(ScriptId);
    REGISTER_ENTITY_PROPERTY(KnownLocProtoId);
    REGISTER_ENTITY_PROPERTY(ModelLayers);
    REGISTER_ENTITY_PROPERTY(IsHide);
    REGISTER_ENTITY_PROPERTY(IsNoHome);
    REGISTER_ENTITY_PROPERTY(IsGeck);
    REGISTER_ENTITY_PROPERTY(IsNoUnarmed);
    REGISTER_ENTITY_PROPERTY(IsNoWalk);
    REGISTER_ENTITY_PROPERTY(IsNoRun);
    REGISTER_ENTITY_PROPERTY(IsNoRotate);
    REGISTER_ENTITY_PROPERTY(IsNoTalk);
    REGISTER_ENTITY_PROPERTY(IsNoFlatten);
    REGISTER_ENTITY_PROPERTY(TimeoutBattle);
    REGISTER_ENTITY_PROPERTY(TimeoutTransfer);
    REGISTER_ENTITY_PROPERTY(TimeoutRemoveFromGame);
}

void MapProperties::FillProperties(PropertyRegistrator* registrator)
{
    REGISTER_ENTITY_PROPERTY(LoopTime1);
    REGISTER_ENTITY_PROPERTY(LoopTime2);
    REGISTER_ENTITY_PROPERTY(LoopTime3);
    REGISTER_ENTITY_PROPERTY(LoopTime4);
    REGISTER_ENTITY_PROPERTY(LoopTime5);
    REGISTER_ENTITY_PROPERTY(FileDir);
    REGISTER_ENTITY_PROPERTY(Width);
    REGISTER_ENTITY_PROPERTY(Height);
    REGISTER_ENTITY_PROPERTY(WorkHexX);
    REGISTER_ENTITY_PROPERTY(WorkHexY);
    REGISTER_ENTITY_PROPERTY(LocId);
    REGISTER_ENTITY_PROPERTY(LocMapIndex);
    REGISTER_ENTITY_PROPERTY(RainCapacity);
    REGISTER_ENTITY_PROPERTY(CurDayTime);
    REGISTER_ENTITY_PROPERTY(ScriptId);
    REGISTER_ENTITY_PROPERTY(DayTime);
    REGISTER_ENTITY_PROPERTY(DayColor);
    REGISTER_ENTITY_PROPERTY(IsNoLogOut);
}

void LocationProperties::FillProperties(PropertyRegistrator* registrator)
{
    REGISTER_ENTITY_PROPERTY(MapProtos);
    REGISTER_ENTITY_PROPERTY(MapEntrances);
    REGISTER_ENTITY_PROPERTY(Automaps);
    REGISTER_ENTITY_PROPERTY(MaxPlayers);
    REGISTER_ENTITY_PROPERTY(AutoGarbage);
    REGISTER_ENTITY_PROPERTY(GeckVisible);
    REGISTER_ENTITY_PROPERTY(EntranceScript);
    REGISTER_ENTITY_PROPERTY(WorldX);
    REGISTER_ENTITY_PROPERTY(WorldY);
    REGISTER_ENTITY_PROPERTY(Radius);
    REGISTER_ENTITY_PROPERTY(Hidden);
    REGISTER_ENTITY_PROPERTY(ToGarbage);
    REGISTER_ENTITY_PROPERTY(Color);
    REGISTER_ENTITY_PROPERTY(IsEncounter);
}
