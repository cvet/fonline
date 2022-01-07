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
#include "Log.h"
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

auto Entity::GetEventCallbacks(const string& event_name) -> vector<EventCallbackData>*
{
    if (const auto it = _events.find(event_name); it != _events.end()) {
        return &it->second;
    }

    return &_events.emplace(event_name, vector<EventCallbackData>()).first->second;
}

void Entity::SubscribeEvent(const string& event_name, EventCallbackData callback)
{
    SubscribeEvent(GetEventCallbacks(event_name), std::move(callback));
}

void Entity::UnsubscribeEvent(const string& event_name, const void* subscription_ptr)
{
    if (const auto it = _events.find(event_name); it != _events.end()) {
        UnsubscribeEvent(&it->second, subscription_ptr);
    }
}

void Entity::UnsubscribeAllEvent(const string& event_name)
{
    if (const auto it = _events.find(event_name); it != _events.end()) {
        it->second.clear();
    }
}

auto Entity::FireEvent(const string& event_name, const initializer_list<void*>& args) -> bool
{
    if (const auto it = _events.find(event_name); it != _events.end()) {
        return FireEvent(&it->second, args);
    }
    return false;
}

void Entity::SubscribeEvent(vector<EventCallbackData>* callbacks, EventCallbackData callback)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(callbacks);

    callbacks->push_back(std::move(callback));
}

void Entity::UnsubscribeEvent(vector<EventCallbackData>* callbacks, const void* subscription_ptr)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(callbacks);

    if (const auto it = std::find_if(callbacks->begin(), callbacks->end(), [subscription_ptr](const auto& cb) { return cb.SubscribtionPtr == subscription_ptr; }); it != callbacks->end()) {
        callbacks->erase(it);
    }
}

auto Entity::FireEvent(vector<EventCallbackData>* callbacks, const initializer_list<void*>& args) -> bool
{
    RUNTIME_ASSERT(callbacks);

    // Todo: events array may be modified during call, need take it into account here
    for (size_t i = 0; i < callbacks->size(); i++) {
        const auto& cb = callbacks->at(i);
        const auto ex_policy = cb.ExPolicy;

        try {
            if (!cb.Callback(args)) {
                return false;
            }
        }
        catch (const std::exception& ex) {
            WriteLog("{}\n", ex.what());

            if (ex_policy == EventExceptionPolicy::StopChainAndReturnTrue) {
                return true;
            }
            if (ex_policy == EventExceptionPolicy::StopChainAndReturnFalse) {
                return false;
            }
            if (ex_policy == EventExceptionPolicy::PropogateException) {
                throw ex;
            }
        }
    }

    return true;
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

ProtoEntity::ProtoEntity(hstring proto_id, const PropertyRegistrator* registrator) : Entity(registrator), _protoId {proto_id}
{
    RUNTIME_ASSERT(_protoId);
}

auto ProtoEntity::GetName() const -> string_view
{
    return _protoId;
}

auto ProtoEntity::GetProtoId() const -> hstring
{
    return _protoId;
}

auto ProtoEntity::HaveComponent(hstring name) const -> bool
{
    return _components.count(name) > 0u;
}

EntityWithProto::EntityWithProto(const PropertyRegistrator* registrator, const ProtoEntity* proto) : Entity(registrator), _proto {proto}
{
    RUNTIME_ASSERT(_proto);

    _proto->AddRef();

    SetProperties(_proto->GetProperties());
}

EntityWithProto::~EntityWithProto()
{
    _proto->Release();
}

auto EntityWithProto::GetName() const -> string_view
{
    return _proto->GetName();
}

auto EntityWithProto::GetProtoId() const -> hstring
{
    return _proto->GetProtoId();
}

auto EntityWithProto::GetProto() const -> const ProtoEntity*
{
    return _proto;
}

EntityEventBase::EntityEventBase(Entity* entity, const char* callback_name) : _entity {entity}, _callbackName {callback_name}
{
}

void EntityEventBase::Subscribe(Entity::EventCallbackData callback)
{
    if (_callbacks == nullptr) {
        _callbacks = _entity->GetEventCallbacks(_callbackName);
    }

    _entity->SubscribeEvent(_callbacks, std::move(callback));
}

// ReSharper disable once CppMemberFunctionMayBeConst
void EntityEventBase::Unsubscribe(const void* subscription_ptr)
{
    if (_callbacks == nullptr) {
        return;
    }

    _entity->UnsubscribeEvent(_callbacks, subscription_ptr);
}

void EntityEventBase::UnsubscribeAll()
{
    if (_callbacks == nullptr) {
        return;
    }

    _entity->UnsubscribeAllEvent(_callbackName);
    _callbacks = nullptr;
}

// ReSharper disable once CppMemberFunctionMayBeConst
auto EntityEventBase::FireEx(const initializer_list<void*>& args) -> bool
{
    if (_callbacks == nullptr) {
        return false;
    }

    return _entity->FireEvent(_callbacks, args);
}
