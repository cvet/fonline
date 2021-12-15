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

///@ ExportEnum
enum class CritterCondition : uchar
{
    Alive = 0,
    Knockout = 1,
    Dead = 2,
};

#define CLASS_PROPERTY(access_type, prop_type, prop) \
    inline auto GetProperty##prop() const->const Property* { return Props.GetRegistrator()->FindNoComponentCheck(#prop); } \
    inline prop_type Get##prop() const { return Props.GetValue<prop_type>(GetProperty##prop()); } \
    inline void Set##prop(prop_type value) { Props.SetValue<prop_type>(GetProperty##prop(), value); } \
    inline bool IsNonEmpty##prop() const { return Props.GetRawDataSize(GetProperty##prop()) > 0; }

#define CLASS_READONLY_PROPERTY(access_type, prop_type, prop) \
    inline auto GetProperty##prop() const->const Property* { return Props.GetRegistrator()->FindNoComponentCheck(#prop); } \
    inline prop_type Get##prop() const { return Props.GetValue<prop_type>(GetProperty##prop()); } \
    inline void Set##prop##_ReadOnlyWorkaround(prop_type value) { Props.SetValue<prop_type>(GetProperty##prop(), value); } \
    inline bool IsNonEmpty##prop() const { return Props.GetRawDataSize(GetProperty##prop()) > 0; }

class ProtoEntity;

class Entity
{
public:
    Entity() = delete;
    Entity(const Entity&) = delete;
    Entity(Entity&&) noexcept = delete;
    auto operator=(const Entity&) = delete;
    auto operator=(Entity&&) noexcept = delete;

    [[nodiscard]] auto GetProtoId() const -> hash;
    [[nodiscard]] auto GetName() const -> string;

    void AddRef() const;
    void Release() const;

    Properties Props;
    const ProtoEntity* Proto;
    mutable int RefCounter {1};
    bool IsDestroyed {};
    bool IsDestroying {};

    // Scripting handles
    void* MonoObj {};
    void* NativeObj {};

protected:
    Entity(const PropertyRegistrator* registrator, const ProtoEntity* proto);
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
    ProtoEntity(hash proto_id, const PropertyRegistrator* registrator);
};

class ProtoPlayer final : public ProtoEntity
{
public:
    explicit ProtoPlayer(hash proto_id, const PropertyRegistrator* registrator);

#define PLAYER_PROPERTY CLASS_READONLY_PROPERTY
#include "Properties-Include.h"
};

class ProtoItem final : public ProtoEntity
{
public:
    explicit ProtoItem(hash proto_id, const PropertyRegistrator* registrator);

    [[nodiscard]] auto IsStatic() const -> bool { return GetIsStatic(); }
    [[nodiscard]] auto IsAnyScenery() const -> bool { return IsScenery() || IsWall(); }
    [[nodiscard]] auto IsScenery() const -> bool { return GetIsScenery(); }
    [[nodiscard]] auto IsWall() const -> bool { return GetIsWall(); }

    mutable int64 InstanceCount {};

#define ITEM_PROPERTY CLASS_READONLY_PROPERTY
#include "Properties-Include.h"
};

class ProtoCritter final : public ProtoEntity
{
public:
    explicit ProtoCritter(hash proto_id, const PropertyRegistrator* registrator);

#define CRITTER_PROPERTY CLASS_READONLY_PROPERTY
#include "Properties-Include.h"
};

class ProtoMap final : public ProtoEntity
{
public:
    explicit ProtoMap(hash proto_id, const PropertyRegistrator* registrator);

#define MAP_PROPERTY CLASS_READONLY_PROPERTY
#include "Properties-Include.h"
};

class ProtoLocation final : public ProtoEntity
{
public:
    explicit ProtoLocation(hash proto_id, const PropertyRegistrator* registrator);

#define LOCATION_PROPERTY CLASS_READONLY_PROPERTY
#include "Properties-Include.h"
};
