// FOnline Engine
// SPDX-License-Identifier: MIT

#include "PlayerView.h"

#include "Client.h"

FO_BEGIN_NAMESPACE

PlayerView::PlayerView(ptr<ClientEngine> engine, ident_t id, nptr<const Properties> props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrar(ENTITY_TYPE_NAME), props, nullptr),
    PlayerProperties(*GetInitRef())
{
}

void PlayerView::OnDestroySelf()
{
}

FO_END_NAMESPACE
