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
#include "EntityProperties.h"
#include "Properties.h"

FO_BEGIN_NAMESPACE();

class ProtoEntity : public Entity
{
public:
    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _protoId.as_str(); }
    [[nodiscard]] auto GetProtoId() const noexcept -> hstring { return _protoId; }
    [[nodiscard]] auto HasComponent(hstring name) const noexcept -> bool { return _components.contains(name); }
    [[nodiscard]] auto HasComponent(hstring::hash_t hash) const noexcept -> bool { return _componentHashes.contains(hash); }
    [[nodiscard]] auto GetComponents() const noexcept -> const unordered_set<hstring>& { return _components; }

    void EnableComponent(hstring component);

    string CollectionName {};

protected:
    ProtoEntity(hstring proto_id, const PropertyRegistrator* registrator, const Properties* props) noexcept;

    const hstring _protoId;
    unordered_set<hstring> _components {};
    unordered_set<hstring::hash_t> _componentHashes {};
};

class EntityWithProto
{
public:
    EntityWithProto() = delete;
    EntityWithProto(const EntityWithProto&) = delete;
    EntityWithProto(EntityWithProto&&) noexcept = delete;
    auto operator=(const EntityWithProto&) = delete;
    auto operator=(EntityWithProto&&) noexcept = delete;

    [[nodiscard]] auto GetProtoId() const noexcept -> hstring { return _proto->GetProtoId(); }
    [[nodiscard]] auto GetProto() const noexcept -> const ProtoEntity* { return _proto.get(); }

protected:
    explicit EntityWithProto(const ProtoEntity* proto) noexcept;
    virtual ~EntityWithProto() = default;

    refcount_ptr<const ProtoEntity> _proto;
};

class ProtoItem final : public ProtoEntity, public ItemProperties
{
public:
    ProtoItem(hstring proto_id, const PropertyRegistrator* registrator, const Properties* props = nullptr);
};

class ProtoCritter final : public ProtoEntity, public CritterProperties
{
public:
    ProtoCritter(hstring proto_id, const PropertyRegistrator* registrator, const Properties* props = nullptr);
};

class ProtoMap final : public ProtoEntity, public MapProperties
{
public:
    ProtoMap(hstring proto_id, const PropertyRegistrator* registrator, const Properties* props = nullptr);
};

class ProtoLocation final : public ProtoEntity, public LocationProperties
{
public:
    ProtoLocation(hstring proto_id, const PropertyRegistrator* registrator, const Properties* props = nullptr);
};

class ProtoCustomEntity final : public ProtoEntity, public EntityProperties
{
public:
    ProtoCustomEntity(hstring proto_id, const PropertyRegistrator* registrator, const Properties* props = nullptr);
};

FO_END_NAMESPACE();
