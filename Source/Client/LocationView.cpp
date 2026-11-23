// FOnline Engine
// SPDX-License-Identifier: MIT

#include "LocationView.h"

#include "Client.h"

FO_BEGIN_NAMESPACE

LocationView::LocationView(ptr<ClientEngine> engine, ident_t id, ptr<const ProtoLocation> proto, nptr<const Properties> props) :
    ClientEntity(engine, id, engine->GetPropertyRegistrar(ENTITY_TYPE_NAME), props ? props : nptr<const Properties> {proto->GetProperties()}, proto->GetProperties()),
    EntityWithProto(proto),
    LocationProperties(*GetInitRef())
{
    _name = strex("{}_{}", proto->GetName(), id);
}

void LocationView::OnDestroySelf()
{
}

FO_END_NAMESPACE
