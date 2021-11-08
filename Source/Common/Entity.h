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

#define PROPERTIES_HEADER() static PropertyRegistrator* PropertiesRegistrator

#define PROPERTIES_IMPL(class_name, script_name, is_server) PropertyRegistrator* class_name::PropertiesRegistrator = new PropertyRegistrator(is_server)

#define CLASS_PROPERTY(access_type, prop_type, prop, ...) \
    static Property* Property##prop; \
    inline prop_type Get##prop() const { return Props.GetValue<prop_type>(Property##prop); } \
    inline void Set##prop(prop_type value) { Props.SetValue<prop_type>(Property##prop, value); } \
    inline bool IsNonEmpty##prop() const { return Props.GetRawDataSize(Property##prop) > 0; }

#define CLASS_PROPERTY_IMPL(class_name, access_type, prop_type, prop, ...) Property* class_name::Property##prop = class_name::PropertiesRegistrator->Register(Property::AccessType::access_type, typeid(prop_type), #prop)

// Todo: remove EntityType enum, use dynamic cast
enum class EntityType
{
    Player,
    PlayerProto,
    ItemProto,
    CritterProto,
    MapProto,
    LocationProto,
    Item,
    Critter,
    Map,
    Location,
    PlayerView,
    ItemView,
    ItemHexView,
    CritterView,
    MapView,
    LocationView,
    Custom,
    Global,
    Max,
};

class ProtoEntity;

class Entity
{
public:
    Entity() = delete;
    Entity(const Entity&) = delete;
    Entity(Entity&&) noexcept = default;
    auto operator=(const Entity&) = delete;
    auto operator=(Entity&&) noexcept = delete;

    [[nodiscard]] auto GetId() const -> uint;
    [[nodiscard]] auto GetProtoId() const -> hash;
    [[nodiscard]] auto GetName() const -> string;

    void SetId(uint id); // Todo: use passkey for SetId
    void AddRef() const;
    void Release() const;

    Properties Props;
    const uint Id;
    const EntityType Type;
    const ProtoEntity* Proto;
    mutable int RefCounter {1};
    bool IsDestroyed {};
    bool IsDestroying {};

protected:
    Entity(uint id, EntityType type, PropertyRegistrator* registartor, const ProtoEntity* proto);
    virtual ~Entity();

    bool _nonConstHelper {};
};

class ProtoEntity : public Entity
{
public:
    [[nodiscard]] auto HaveComponent(hash name) const -> bool;

    const hash ProtoId;
    vector<uint> TextsLang {};
    vector<FOMsg*> Texts {};
    set<hash> Components {};
    string CollectionName {};

protected:
    ProtoEntity(hash proto_id, EntityType type, PropertyRegistrator* registrator);
};

class CustomEntity final : public Entity
{
public:
    CustomEntity(uint id, uint sub_type, PropertyRegistrator* registrator);

    const uint SubType;
};

class GlobalVars final : public Entity
{
public:
    GlobalVars();

    PROPERTIES_HEADER();
#define FO_API_GLOBAL_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"
};

class ProtoPlayer final : public ProtoEntity
{
public:
    explicit ProtoPlayer(hash pid);

    PROPERTIES_HEADER();
#define FO_API_PLAYER_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"
};

class ProtoItem final : public ProtoEntity
{
public:
    explicit ProtoItem(hash pid);

    [[nodiscard]] auto IsStatic() const -> bool { return GetIsStatic(); }
    [[nodiscard]] auto IsAnyScenery() const -> bool { return IsScenery() || IsWall(); }
    [[nodiscard]] auto IsScenery() const -> bool { return GetIsScenery(); }
    [[nodiscard]] auto IsWall() const -> bool { return GetIsWall(); }

    mutable int64 InstanceCount {};

    PROPERTIES_HEADER();
#define FO_API_ITEM_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"
};

class ProtoCritter final : public ProtoEntity
{
public:
    explicit ProtoCritter(hash pid);

    PROPERTIES_HEADER();
#define FO_API_CRITTER_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"
};

class ProtoMap final : public ProtoEntity
{
public:
    explicit ProtoMap(hash pid);

    PROPERTIES_HEADER();
#define FO_API_MAP_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"
};

class ProtoLocation final : public ProtoEntity
{
public:
    explicit ProtoLocation(hash pid);

    PROPERTIES_HEADER();
#define FO_API_LOCATION_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"
};
