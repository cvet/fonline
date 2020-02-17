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

#include "MsgFiles.h"
#include "Properties.h"

enum class EntityType
{
    ItemProto,
    CritterProto,
    MapProto,
    LocationProto,
    Item,
    Client,
    Npc,
    Map,
    Location,
    ItemView,
    ItemHexView,
    CritterView,
    MapView,
    LocationView,
    Custom,
    Global,
    Max,
};

class CScriptArray;
class Entity;
using EntityVec = vector<Entity*>;
using EntityMap = map<uint, Entity*>;
class ProtoEntity;
using ProtoEntityVec = vector<ProtoEntity*>;
using ProtoEntityMap = map<hash, ProtoEntity*>;

class Entity : public NonCopyable
{
public:
    uint GetId() const;
    void SetId(uint id);
    hash GetProtoId() const;
    string GetName() const;
    void AddRef() const;
    void Release() const;

    Properties Props;
    const uint Id;
    const EntityType Type;
    ProtoEntity* Proto {};
    mutable int RefCounter {1};
    bool IsDestroyed {};
    bool IsDestroying {};

protected:
    Entity(uint id, EntityType type, PropertyRegistrator* registartor, ProtoEntity* proto);
    virtual ~Entity();
};

class ProtoEntity : public Entity
{
public:
    bool HaveComponent(hash name) const;

    const hash ProtoId;
    UIntVec TextsLang {};
    FOMsgVec Texts {};
    HashSet Components {};
    string CollectionName {};

protected:
    ProtoEntity(hash proto_id, EntityType type, PropertyRegistrator* registrator);
};

class CustomEntity : public Entity
{
public:
    CustomEntity(uint id, uint sub_type, PropertyRegistrator* registrator);

    const uint SubType;
};

class GlobalVars : public Entity
{
public:
    GlobalVars();

    PROPERTIES_HEADER();
#include "GlobalProperties.h"
    CLASS_PROPERTY(ushort, YearStart);
    CLASS_PROPERTY(ushort, Year);
    CLASS_PROPERTY(ushort, Month);
    CLASS_PROPERTY(ushort, Day);
    CLASS_PROPERTY(ushort, Hour);
    CLASS_PROPERTY(ushort, Minute);
    CLASS_PROPERTY(ushort, Second);
    CLASS_PROPERTY(ushort, TimeMultiplier);
    CLASS_PROPERTY(uint, LastEntityId);
    CLASS_PROPERTY(uint, LastDeferredCallId);
    CLASS_PROPERTY(uint, HistoryRecordsId);
};
extern GlobalVars* Globals;

class ProtoItem : public ProtoEntity
{
public:
    ProtoItem(hash pid);
    bool IsStatic() { return GetIsStatic(); }
    bool IsAnyScenery() { return IsScenery() || IsWall(); }
    bool IsScenery() { return GetIsScenery(); }
    bool IsWall() { return GetIsWall(); }

    int64 InstanceCount {};

    PROPERTIES_HEADER();
#include "ItemProperties.h"
    CLASS_PROPERTY(hash, PicMap);
    CLASS_PROPERTY(hash, PicInv);
    CLASS_PROPERTY(bool, Stackable);
    CLASS_PROPERTY(short, OffsetX);
    CLASS_PROPERTY(short, OffsetY);
    CLASS_PROPERTY(uchar, Slot);
    CLASS_PROPERTY(char, LightIntensity);
    CLASS_PROPERTY(uchar, LightDistance);
    CLASS_PROPERTY(uchar, LightFlags);
    CLASS_PROPERTY(uint, LightColor);
    CLASS_PROPERTY(uint, Count);
    CLASS_PROPERTY(bool, IsFlat);
    CLASS_PROPERTY(char, DrawOrderOffsetHexY);
    CLASS_PROPERTY(int, Corner);
    CLASS_PROPERTY(bool, DisableEgg);
    CLASS_PROPERTY(bool, IsStatic);
    CLASS_PROPERTY(bool, IsScenery);
    CLASS_PROPERTY(bool, IsWall);
    CLASS_PROPERTY(bool, IsBadItem);
    CLASS_PROPERTY(bool, IsColorize);
    CLASS_PROPERTY(bool, IsShowAnim);
    CLASS_PROPERTY(bool, IsShowAnimExt);
    CLASS_PROPERTY(uchar, AnimStay0);
    CLASS_PROPERTY(uchar, AnimStay1);
    CLASS_PROPERTY(CScriptArray*, BlockLines);
};
using ProtoItemVec = vector<ProtoItem*>;
using ProtoItemMap = map<hash, ProtoItem*>;

class ProtoCritter : public ProtoEntity
{
public:
    ProtoCritter(hash pid);

    PROPERTIES_HEADER();
#include "CritterProperties.h"
    CLASS_PROPERTY(uint, Multihex);
};
using ProtoCritterMap = map<hash, ProtoCritter*>;
using ProtoCritterVec = vector<ProtoCritter*>;

class ProtoMap : public ProtoEntity
{
public:
    ProtoMap(hash pid);

    PROPERTIES_HEADER();
#include "MapProperties.h"
    CLASS_PROPERTY(string, FilePath);
    CLASS_PROPERTY(ushort, Width);
    CLASS_PROPERTY(ushort, Height);
    CLASS_PROPERTY(ushort, WorkHexX);
    CLASS_PROPERTY(ushort, WorkHexY);
    CLASS_PROPERTY(int, CurDayTime);
    CLASS_PROPERTY(hash, ScriptId);
    CLASS_PROPERTY(CScriptArray*, DayTime); // 4 int
    CLASS_PROPERTY(CScriptArray*, DayColor); // 12 uchar
    CLASS_PROPERTY(bool, IsNoLogOut);
};
using ProtoMapVec = vector<ProtoMap*>;
using ProtoMapMap = map<hash, ProtoMap*>;

class ProtoLocation : public ProtoEntity
{
public:
    ProtoLocation(hash pid);

    PROPERTIES_HEADER();
#include "LocationProperties.h"
    CLASS_PROPERTY(CScriptArray*, MapProtos);
};
using ProtoLocationVec = vector<ProtoLocation*>;
using ProtoLocationMap = map<hash, ProtoLocation*>;
