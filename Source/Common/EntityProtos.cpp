// FOnline Engine
// SPDX-License-Identifier: MIT

#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

ProtoEntity::ProtoEntity(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props) noexcept :
    Entity(registrar, props, nullptr),
    _protoId {proto_id}
{
    FO_STRONG_ASSERT(_protoId, "Proto entity id is empty");
}

EntityWithProto::EntityWithProto(ptr<const ProtoEntity> proto) noexcept :
    _proto {proto.hold_ref()}
{
}

ProtoItem::ProtoItem(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props) :
    ProtoEntity(proto_id, registrar, props),
    ItemProperties(*GetInitRef())
{
}

ProtoCritter::ProtoCritter(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props) :
    ProtoEntity(proto_id, registrar, props),
    CritterProperties(*GetInitRef())
{
}

ProtoMap::ProtoMap(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props) :
    ProtoEntity(proto_id, registrar, props),
    MapProperties(*GetInitRef())
{
}

ProtoLocation::ProtoLocation(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props) :
    ProtoEntity(proto_id, registrar, props),
    LocationProperties(*GetInitRef())
{
}

ProtoCustomEntity::ProtoCustomEntity(hstring proto_id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props) :
    ProtoEntity(proto_id, registrar, props),
    EntityProperties(*GetInitRef())
{
}

FO_END_NAMESPACE
