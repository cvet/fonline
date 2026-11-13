// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ClientEntity.h"
#include "EntityProperties.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

class PlayerView final : public ClientEntity, public PlayerProperties
{
public:
    PlayerView() = delete;
    PlayerView(ptr<ClientEngine> engine, ident_t id, nptr<const Properties> props = nullptr);
    PlayerView(const PlayerView&) = delete;
    PlayerView(PlayerView&&) noexcept = delete;
    auto operator=(const PlayerView&) = delete;
    auto operator=(PlayerView&&) noexcept = delete;
    ~PlayerView() override = default;

private:
    void OnDestroySelf() override;
};

FO_END_NAMESPACE
