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

#include "Entity.h"
#include "StringUtils.h"

Entity::Entity(EntityType type, PropertyRegistrator* registrator, const ProtoEntity* proto) : Props {registrator}, Type {type}, Proto {proto}
{
    if (Proto != nullptr) {
        Proto->AddRef();
        Props = Proto->Props;
    }
}

Entity::~Entity()
{
    if (Proto != nullptr) {
        Proto->Release();
    }
}

auto Entity::GetProtoId() const -> hash
{
    return Proto != nullptr ? Proto->ProtoId : 0;
}

auto Entity::GetName() const -> string
{
    switch (Type) {
    case EntityType::ItemProto:
        [[fallthrough]];
    case EntityType::CritterProto:
        [[fallthrough]];
    case EntityType::MapProto:
        [[fallthrough]];
    case EntityType::LocationProto:
        // return _str().parseHash(const_cast<ProtoEntity*>(this)->ProtoId);
        return "TODO"; // Todo: fix proto name recognition
    default:
        break;
    }
    // return Proto != nullptr ? _str().parseHash(Proto->ProtoId) : "Unnamed";
    return "";
}

void Entity::AddRef() const
{
    ++RefCounter;
}

void Entity::Release() const
{
    if (--RefCounter == 0) {
        delete this;
    }
}

ProtoEntity::ProtoEntity(hash proto_id, EntityType type, PropertyRegistrator* registrator) : Entity(type, registrator, nullptr), ProtoId(proto_id)
{
    RUNTIME_ASSERT(ProtoId);
}

auto ProtoEntity::HaveComponent(hash name) const -> bool
{
    return Components.count(name) > 0;
}

PROPERTIES_IMPL(ProtoPlayer, "Player", true);
#define PLAYER_PROPERTY(access, type, name, ...) CLASS_PROPERTY_IMPL(ProtoPlayer, access, type, name)
#include "Properties-Include.h"

ProtoPlayer::ProtoPlayer(hash pid) : ProtoEntity(pid, EntityType::PlayerProto, PropertiesRegistrator)
{
}

PROPERTIES_IMPL(ProtoItem, "Item", true);
#define ITEM_PROPERTY(access, type, name, ...) CLASS_PROPERTY_IMPL(ProtoItem, access, type, name)
#include "Properties-Include.h"

ProtoItem::ProtoItem(hash pid) : ProtoEntity(pid, EntityType::ItemProto, PropertiesRegistrator)
{
}

PROPERTIES_IMPL(ProtoCritter, "Critter", true);
#define CRITTER_PROPERTY(access, type, name, ...) CLASS_PROPERTY_IMPL(ProtoCritter, access, type, name)
#include "Properties-Include.h"

ProtoCritter::ProtoCritter(hash pid) : ProtoEntity(pid, EntityType::CritterProto, PropertiesRegistrator)
{
}

PROPERTIES_IMPL(ProtoMap, "Map", true);
#define MAP_PROPERTY(access, type, name, ...) CLASS_PROPERTY_IMPL(ProtoMap, access, type, name)
#include "Properties-Include.h"

ProtoMap::ProtoMap(hash pid) : ProtoEntity(pid, EntityType::MapProto, PropertiesRegistrator)
{
}

PROPERTIES_IMPL(ProtoLocation, "Location", true);
#define LOCATION_PROPERTY(access, type, name, ...) CLASS_PROPERTY_IMPL(ProtoLocation, access, type, name)
#include "Properties-Include.h"

ProtoLocation::ProtoLocation(hash pid) : ProtoEntity(pid, EntityType::LocationProto, PropertiesRegistrator)
{
}
