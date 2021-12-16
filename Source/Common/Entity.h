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

#define ENTITY_PROPERTY(access_type, prop_type, prop) \
    inline auto GetProperty##prop() const->const Property* { return _props.GetRegistrator()->FindNoComponentCheck(#prop); } \
    inline prop_type Get##prop() const { return _props.GetValue<prop_type>(GetProperty##prop()); } \
    inline void Set##prop(prop_type value) { _props.SetValue<prop_type>(GetProperty##prop(), value); } \
    inline bool IsNonEmpty##prop() const { return _props.GetRawDataSize(GetProperty##prop()) > 0; }

class EntityProperties
{
protected:
    explicit EntityProperties(Properties& props);

    Properties& _props;
};

class Entity
{
public:
    Entity() = delete;
    Entity(const Entity&) = delete;
    Entity(Entity&&) noexcept = delete;
    auto operator=(const Entity&) = delete;
    auto operator=(Entity&&) noexcept = delete;

    void AddRef() const;
    void Release() const;

    Properties Props;
    bool IsDestroyed {};
    bool IsDestroying {};

    // Scripting handles
    void* MonoObj {};
    void* NativeObj {};

protected:
    explicit Entity(const PropertyRegistrator* registrator);
    virtual ~Entity() = default;

    mutable int _refCounter {1};
    bool _nonConstHelper {};
};

class ProtoEntity : public Entity
{
public:
    [[nodiscard]] auto GetProtoId() const -> hash;
    [[nodiscard]] auto GetName() const -> string;
    [[nodiscard]] auto HaveComponent(hash name) const -> bool;

    const hash ProtoId;
    vector<uint> TextsLang {};
    vector<FOMsg*> Texts {};
    set<hash> Components {};
    string CollectionName {};

protected:
    ProtoEntity(hash proto_id, const PropertyRegistrator* registrator);
};

class EntityWithProto : public Entity
{
public:
    EntityWithProto() = delete;
    EntityWithProto(const EntityWithProto&) = delete;
    EntityWithProto(EntityWithProto&&) noexcept = delete;
    auto operator=(const EntityWithProto&) = delete;
    auto operator=(EntityWithProto&&) noexcept = delete;

    [[nodiscard]] auto GetProtoId() const -> hash;
    [[nodiscard]] auto GetName() const -> string;

    const ProtoEntity* Proto;

protected:
    EntityWithProto(const PropertyRegistrator* registrator, const ProtoEntity* proto);
    ~EntityWithProto() override;
};
