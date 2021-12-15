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

Entity::Entity(const PropertyRegistrator* registrator, const ProtoEntity* proto) : Props {registrator}, Proto {proto}
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
    return Proto != nullptr ? _str().parseHash(Proto->ProtoId).str() : "Unnamed";
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

ProtoEntity::ProtoEntity(hash proto_id, const PropertyRegistrator* registrator) : Entity(registrator, nullptr), ProtoId(proto_id)
{
    RUNTIME_ASSERT(ProtoId);
}

auto ProtoEntity::HaveComponent(hash name) const -> bool
{
    return Components.count(name) > 0;
}

ProtoPlayer::ProtoPlayer(hash proto_id, const PropertyRegistrator* registrator) : ProtoEntity(proto_id, registrator)
{
}

ProtoItem::ProtoItem(hash proto_id, const PropertyRegistrator* registrator) : ProtoEntity(proto_id, registrator)
{
}

ProtoCritter::ProtoCritter(hash proto_id, const PropertyRegistrator* registrator) : ProtoEntity(proto_id, registrator)
{
}

ProtoMap::ProtoMap(hash proto_id, const PropertyRegistrator* registrator) : ProtoEntity(proto_id, registrator)
{
}

ProtoLocation::ProtoLocation(hash proto_id, const PropertyRegistrator* registrator) : ProtoEntity(proto_id, registrator)
{
}
