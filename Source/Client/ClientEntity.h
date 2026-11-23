// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Entity.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

class ClientEngine;

class ClientEntity : public Entity
{
public:
    ClientEntity() = delete;
    ClientEntity(const ClientEntity&) = delete;
    ClientEntity(ClientEntity&&) noexcept = delete;
    auto operator=(const ClientEntity&) = delete;
    auto operator=(ClientEntity&&) noexcept = delete;
    ~ClientEntity() override;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _name; }
    [[nodiscard]] auto GetId() const noexcept -> ident_t override { return _id; }
    [[nodiscard]] auto GetEngine() const noexcept -> ptr<const ClientEngine> { return _engine; }
    [[nodiscard]] auto GetEngine() noexcept -> ptr<ClientEngine> { return _engine; }

    void SetId(ident_t id, bool register_entity);
    void DestroySelf();

protected:
    ClientEntity(ptr<ClientEngine> engine, ident_t id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props, nptr<const Properties> base_props);

    virtual void OnDestroySelf() = 0;

    ptr<ClientEngine> _engine;
    string _name {};

private:
    ident_t _id;
    bool _registered {};
};

class CustomEntityView : public ClientEntity, public EntityProperties
{
public:
    CustomEntityView(ptr<ClientEngine> engine, ident_t id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props, nptr<const Properties> base_props) :
        ClientEntity(engine, id, registrar, props, base_props),
        EntityProperties(*GetInitRef())
    {
    }

    void OnDestroySelf() override;
};

class CustomEntityWithProtoView : public CustomEntityView, public EntityWithProto
{
public:
    CustomEntityWithProtoView(ptr<ClientEngine> engine, ident_t id, ptr<const PropertyRegistrar> registrar, ptr<const ProtoEntity> proto) :
        CustomEntityView(engine, id, registrar, proto->GetProperties(), proto->GetProperties()),
        EntityWithProto(proto)
    {
    }
};

FO_END_NAMESPACE
