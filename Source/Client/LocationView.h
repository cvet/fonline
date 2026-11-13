// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ClientEntity.h"
#include "EntityProperties.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

class LocationView final : public ClientEntity, public EntityWithProto, public LocationProperties
{
public:
    LocationView(ptr<ClientEngine> engine, ident_t id, ptr<const ProtoLocation> proto, nptr<const Properties> props = nullptr);
    LocationView(const LocationView&) = delete;
    LocationView(LocationView&&) noexcept = delete;
    auto operator=(const LocationView&) = delete;
    auto operator=(LocationView&&) noexcept = delete;
    ~LocationView() override = default;

private:
    void OnDestroySelf() override;
};

FO_END_NAMESPACE
