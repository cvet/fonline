// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Entity.h"
#include "EntityProperties.h"
#include "Properties.h"

FO_BEGIN_NAMESPACE

class ProtoEntity : public Entity
{
public:
    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _protoId.as_str(); }
    [[nodiscard]] auto GetProtoId() const noexcept -> hstring { return _protoId; }

    string CollectionName {};

protected:
    ProtoEntity(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props) noexcept;

    const hstring _protoId;
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
    [[nodiscard]] auto GetProto() const noexcept -> ptr<const ProtoEntity> { return _proto; }

protected:
    explicit EntityWithProto(ptr<const ProtoEntity> proto) noexcept;
    virtual ~EntityWithProto() = default;

    refcount_ptr<const ProtoEntity> _proto;
};

class ProtoItem final : public ProtoEntity, public ItemProperties
{
public:
    ProtoItem(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props = nullptr);
};

class ProtoCritter final : public ProtoEntity, public CritterProperties
{
public:
    ProtoCritter(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props = nullptr);
};

class ProtoMap final : public ProtoEntity, public MapProperties
{
public:
    ProtoMap(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props = nullptr);
};

class ProtoLocation final : public ProtoEntity, public LocationProperties
{
public:
    ProtoLocation(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props = nullptr);
};

class ProtoCustomEntity final : public ProtoEntity, public EntityProperties
{
public:
    ProtoCustomEntity(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props = nullptr);
};

FO_END_NAMESPACE
