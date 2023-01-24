//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Application.h"
#include "Log.h"

Entity::Entity(const PropertyRegistrator* registrator) : _props {registrator}
{
    STACK_TRACE_ENTRY();

    _props.SetEntity(this);
}

void Entity::AddRef() const noexcept
{
    STACK_TRACE_ENTRY();

    ++_refCounter;
}

void Entity::Release() const noexcept
{
    STACK_TRACE_ENTRY();

    if (--_refCounter == 0) {
        delete this;
    }
}

auto Entity::GetClassName() const -> string_view
{
    STACK_TRACE_ENTRY();

    return _props.GetRegistrator()->GetClassName();
}

auto Entity::GetProperties() const -> const Properties&
{
    STACK_TRACE_ENTRY();

    return _props;
}

auto Entity::GetPropertiesForEdit() -> Properties&
{
    STACK_TRACE_ENTRY();

    return _props;
}

auto Entity::GetEventCallbacks(const string& event_name) -> vector<EventCallbackData>*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _events.find(event_name); it != _events.end()) {
        return &it->second;
    }

    return &_events.emplace(event_name, vector<EventCallbackData>()).first->second;
}

void Entity::SubscribeEvent(const string& event_name, EventCallbackData callback)
{
    STACK_TRACE_ENTRY();

    SubscribeEvent(GetEventCallbacks(event_name), std::move(callback));
}

void Entity::UnsubscribeEvent(const string& event_name, const void* subscription_ptr)
{
    STACK_TRACE_ENTRY();

    if (const auto it = _events.find(event_name); it != _events.end()) {
        UnsubscribeEvent(&it->second, subscription_ptr);
    }
}

void Entity::UnsubscribeAllEvent(const string& event_name)
{
    STACK_TRACE_ENTRY();

    if (const auto it = _events.find(event_name); it != _events.end()) {
        it->second.clear();
    }
}

auto Entity::FireEvent(const string& event_name, const initializer_list<void*>& args) -> bool
{
    STACK_TRACE_ENTRY();

    if (const auto it = _events.find(event_name); it != _events.end()) {
        return FireEvent(&it->second, args);
    }
    return false;
}

void Entity::SubscribeEvent(vector<EventCallbackData>* callbacks, EventCallbackData callback)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(callbacks);

    callbacks->push_back(std::move(callback));
}

void Entity::UnsubscribeEvent(vector<EventCallbackData>* callbacks, const void* subscription_ptr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(callbacks);

    if (const auto it = std::find_if(callbacks->begin(), callbacks->end(), [subscription_ptr](const auto& cb) { return cb.SubscribtionPtr == subscription_ptr; }); it != callbacks->end()) {
        callbacks->erase(it);
    }
}

auto Entity::FireEvent(vector<EventCallbackData>* callbacks, const initializer_list<void*>& args) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(callbacks);

    for (const auto& cb : copy(*callbacks)) {
        const auto ex_policy = cb.ExPolicy;

        try {
            if (!cb.Callback(args)) {
                return false;
            }
        }
        catch (const std::exception& ex) {
            if (ex_policy == EventExceptionPolicy::PropogateException) {
                throw;
            }

            ReportExceptionAndContinue(ex);

            if (ex_policy == EventExceptionPolicy::StopChainAndReturnTrue) {
                return true;
            }
            if (ex_policy == EventExceptionPolicy::StopChainAndReturnFalse) {
                return false;
            }
        }
    }

    return true;
}

auto Entity::IsDestroying() const -> bool
{
    STACK_TRACE_ENTRY();

    return _isDestroying;
}

auto Entity::IsDestroyed() const -> bool
{
    STACK_TRACE_ENTRY();

    return _isDestroyed;
}

void Entity::MarkAsDestroying()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_isDestroying);
    RUNTIME_ASSERT(!_isDestroyed);

    _isDestroying = true;
}

void Entity::MarkAsDestroyed()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_isDestroyed);

    _isDestroying = true;
    _isDestroyed = true;
}

void Entity::SetProperties(const Properties& props)
{
    STACK_TRACE_ENTRY();

    _props = props;
}

auto Entity::StoreData(bool with_protected, vector<uchar*>** all_data, vector<uint>** all_data_sizes) const -> uint
{
    STACK_TRACE_ENTRY();

    return _props.StoreData(with_protected, all_data, all_data_sizes);
}

void Entity::RestoreData(const vector<const uchar*>& all_data, const vector<uint>& all_data_sizes)
{
    STACK_TRACE_ENTRY();

    _props.RestoreData(all_data, all_data_sizes);
}

void Entity::RestoreData(const vector<vector<uchar>>& properties_data)
{
    STACK_TRACE_ENTRY();

    _props.RestoreData(properties_data);
}

auto Entity::LoadFromText(const map<string, string>& key_values) -> bool
{
    STACK_TRACE_ENTRY();

    return _props.LoadFromText(key_values);
}

void Entity::SetValueFromData(const Property* prop, PropertyRawData& prop_data)
{
    STACK_TRACE_ENTRY();

    _props.SetValueFromData(prop, prop_data);
}

auto Entity::GetValueAsInt(const Property* prop) const -> int
{
    STACK_TRACE_ENTRY();

    return _props.GetPlainDataValueAsInt(prop);
}

auto Entity::GetValueAsInt(int prop_index) const -> int
{
    STACK_TRACE_ENTRY();

    return _props.GetValueAsInt(prop_index);
}

auto Entity::GetValueAsFloat(const Property* prop) const -> float
{
    STACK_TRACE_ENTRY();

    return _props.GetPlainDataValueAsFloat(prop);
}

auto Entity::GetValueAsFloat(int prop_index) const -> float
{
    STACK_TRACE_ENTRY();

    return _props.GetValueAsFloat(prop_index);
}

void Entity::SetValueAsInt(const Property* prop, int value)
{
    STACK_TRACE_ENTRY();

    _props.SetPlainDataValueAsInt(prop, value);
}

void Entity::SetValueAsInt(int prop_index, int value)
{
    STACK_TRACE_ENTRY();

    _props.SetValueAsInt(prop_index, value);
}

void Entity::SetValueAsFloat(const Property* prop, float value)
{
    STACK_TRACE_ENTRY();

    _props.SetPlainDataValueAsFloat(prop, value);
}

void Entity::SetValueAsFloat(int prop_index, float value)
{
    STACK_TRACE_ENTRY();

    _props.SetValueAsFloat(prop_index, value);
}

ProtoEntity::ProtoEntity(hstring proto_id, const PropertyRegistrator* registrator) : Entity(registrator), _protoId {proto_id}
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_protoId);
}

auto ProtoEntity::GetName() const -> string_view
{
    STACK_TRACE_ENTRY();

    return _protoId.as_str();
}

auto ProtoEntity::GetProtoId() const -> hstring
{
    STACK_TRACE_ENTRY();

    return _protoId;
}

void ProtoEntity::EnableComponent(hstring component)
{
    STACK_TRACE_ENTRY();

    _components.emplace(component);
    _componentHashes.emplace(component.as_hash());
}

auto ProtoEntity::HasComponent(hstring name) const -> bool
{
    STACK_TRACE_ENTRY();

    return _components.count(name) != 0u;
}

auto ProtoEntity::HasComponent(hstring::hash_t hash) const -> bool
{
    STACK_TRACE_ENTRY();

    return _componentHashes.count(hash) != 0u;
}

EntityWithProto::EntityWithProto(Entity* owner, const ProtoEntity* proto) : _proto {proto}
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_proto);

    _proto->AddRef();
    owner->SetProperties(_proto->GetProperties());
}

EntityWithProto::~EntityWithProto()
{
    STACK_TRACE_ENTRY();

    _proto->Release();
}

auto EntityWithProto::GetProtoId() const -> hstring
{
    STACK_TRACE_ENTRY();

    return _proto->GetProtoId();
}

auto EntityWithProto::GetProto() const -> const ProtoEntity*
{
    STACK_TRACE_ENTRY();

    return _proto;
}

EntityEventBase::EntityEventBase(Entity* entity, const char* callback_name) : _entity {entity}, _callbackName {callback_name}
{
    STACK_TRACE_ENTRY();
}

void EntityEventBase::Subscribe(Entity::EventCallbackData callback)
{
    STACK_TRACE_ENTRY();

    if (_callbacks == nullptr) {
        _callbacks = _entity->GetEventCallbacks(_callbackName);
    }

    _entity->SubscribeEvent(_callbacks, std::move(callback));
}

// ReSharper disable once CppMemberFunctionMayBeConst
void EntityEventBase::Unsubscribe(const void* subscription_ptr)
{
    STACK_TRACE_ENTRY();

    if (_callbacks == nullptr) {
        return;
    }

    _entity->UnsubscribeEvent(_callbacks, subscription_ptr);
}

void EntityEventBase::UnsubscribeAll()
{
    STACK_TRACE_ENTRY();

    if (_callbacks == nullptr) {
        return;
    }

    _entity->UnsubscribeAllEvent(_callbackName);
    _callbacks = nullptr;
}
