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

Entity::Entity(const PropertyRegistrator* registrator) : _props {registrator}
{
}

void Entity::AddRef() const
{
    ++_refCounter;
}

void Entity::Release() const
{
    if (--_refCounter == 0) {
        delete this;
    }
}

auto Entity::GetClassName() const -> string_view
{
    return _props.GetRegistrator()->GetClassName();
}

auto Entity::GetProperties() const -> const Properties&
{
    return _props;
}

auto Entity::GetPropertiesForEdit() -> Properties&
{
    return _props;
}

auto Entity::IsDestroying() const -> bool
{
    return _isDestroying;
}

auto Entity::IsDestroyed() const -> bool
{
    return _isDestroyed;
}

void Entity::MarkAsDestroying()
{
    RUNTIME_ASSERT(!_isDestroying);
    RUNTIME_ASSERT(!_isDestroyed);

    _isDestroying = true;
}

void Entity::MarkAsDestroyed()
{
    RUNTIME_ASSERT(!_isDestroyed);

    _isDestroying = true;
    _isDestroyed = true;
}

void Entity::SetProperties(const Properties& props)
{
    _props = props;
}

auto Entity::StoreData(bool with_protected, vector<uchar*>** all_data, vector<uint>** all_data_sizes) const -> uint
{
    return _props.StoreData(with_protected, all_data, all_data_sizes);
}

void Entity::RestoreData(const vector<const uchar*>& all_data, const vector<uint>& all_data_sizes)
{
    _props.RestoreData(all_data, all_data_sizes);
}

void Entity::RestoreData(const vector<vector<uchar>>& properties_data)
{
    _props.RestoreData(properties_data);
}

auto Entity::LoadFromText(const map<string, string>& key_values) -> bool
{
    return _props.LoadFromText(key_values);
}

void Entity::SetValueFromData(const Property* prop, const vector<uchar>& data, bool ignore_send)
{
    // Todo: not exception safe, revert ignore with raii
    if (ignore_send) {
        _props.SetSendIgnore(prop, this);
    }

    _props.SetValueFromData(prop, data.data(), static_cast<uint>(data.size()));

    if (ignore_send) {
        _props.SetSendIgnore(nullptr, nullptr);
    }
}

auto Entity::GetValueAsInt(const Property* prop) const -> int
{
    return _props.GetPlainDataValueAsInt(prop);
}

auto Entity::GetValueAsFloat(const Property* prop) const -> float
{
    return _props.GetPlainDataValueAsFloat(prop);
}

void Entity::SetValueAsInt(const Property* prop, int value)
{
    _props.SetPlainDataValueAsInt(prop, value);
}

void Entity::SetValueAsFloat(const Property* prop, float value)
{
    _props.SetPlainDataValueAsFloat(prop, value);
}

EntityWithProto::EntityWithProto(const PropertyRegistrator* registrator, const ProtoEntity* proto) : Entity(registrator), Proto {proto}
{
    RUNTIME_ASSERT(Proto);

    Proto->AddRef();
    SetProperties(Proto->GetProperties());
}

ProtoEntity::ProtoEntity(hash proto_id, const PropertyRegistrator* registrator) : Entity(registrator), ProtoId {proto_id}
{
    RUNTIME_ASSERT(ProtoId);
}

auto ProtoEntity::GetProtoId() const -> hash
{
    return ProtoId;
}

auto ProtoEntity::GetName() const -> string
{
    return _str().parseHash(ProtoId).str();
}

auto ProtoEntity::HaveComponent(hash name) const -> bool
{
    return Components.count(name) > 0;
}

EntityWithProto::~EntityWithProto()
{
    Proto->Release();
}

auto EntityWithProto::GetProtoId() const -> hash
{
    return Proto->ProtoId;
}

auto EntityWithProto::GetName() const -> string
{
    return Proto->GetName();
}
