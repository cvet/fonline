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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

Entity::Entity(const PropertyRegistrator* registrator, const Properties* props) noexcept :
    _props {registrator}
{
    FO_STACK_TRACE_ENTRY();

    _props.SetEntity(this);

    if (props != nullptr) {
        _props.CopyFrom(*props);
    }
}

void Entity::AddRef() const noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto old = _refCounter.fetch_add(1, std::memory_order_relaxed);
    FO_STRONG_ASSERT(old > 0);
}

void Entity::Release() const noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto old = _refCounter.fetch_sub(1, std::memory_order_acq_rel);
    FO_STRONG_ASSERT(old > 0);

    if (old == 1) {
        delete this;
    }
}

auto Entity::GetEventCallbacks(string_view event_name) -> vector<EventCallbackData>&
{
    FO_STACK_TRACE_ENTRY();

    make_if_not_exists(_events);

    if (const auto it = _events->find(event_name); it != _events->end()) {
        return it->second;
    }

    return _events->emplace(event_name, vector<EventCallbackData>()).first->second;
}

void Entity::SubscribeEvent(string_view event_name, EventCallbackData&& callback)
{
    FO_STACK_TRACE_ENTRY();

    SubscribeEvent(GetEventCallbacks(event_name), std::move(callback));
}

void Entity::UnsubscribeEvent(string_view event_name, const void* subscription_ptr) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_events) {
        if (const auto it = _events->find(event_name); it != _events->end()) {
            UnsubscribeEvent(it->second, subscription_ptr);
        }
    }
}

void Entity::UnsubscribeAllEvent(string_view event_name) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_events) {
        if (const auto it = _events->find(event_name); it != _events->end()) {
            it->second.clear();
        }
    }
}

auto Entity::FireEvent(string_view event_name, const initializer_list<void*>& args) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_events) {
        if (const auto it = _events->find(event_name); it != _events->end()) {
            return FireEvent(it->second, args);
        }
    }

    return true;
}

void Entity::SubscribeEvent(vector<EventCallbackData>& callbacks, EventCallbackData&& callback)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (callback.Priority >= EventPriority::Highest && std::ranges::find_if(callbacks, [](const EventCallbackData& cb) { return cb.Priority >= EventPriority::Highest; }) != callbacks.end()) {
        throw GenericException("Highest callback already added");
    }

    if (callback.Priority <= EventPriority::Lowest && std::ranges::find_if(callbacks, [](const EventCallbackData& cb) { return cb.Priority <= EventPriority::Lowest; }) != callbacks.end()) {
        throw GenericException("Lowest callback already added");
    }

    callbacks.emplace_back(std::move(callback));

    std::ranges::stable_sort(callbacks, [](const EventCallbackData& cb1, const EventCallbackData& cb2) {
        // From highest to lowest
        return cb1.Priority > cb2.Priority;
    });
}

void Entity::UnsubscribeEvent(vector<EventCallbackData>& callbacks, const void* subscription_ptr) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (const auto it = std::ranges::find_if(callbacks, [subscription_ptr](const auto& cb) { return cb.SubscribtionPtr == subscription_ptr; }); it != callbacks.end()) {
        callbacks.erase(it);
    }
}

auto Entity::FireEvent(vector<EventCallbackData>& callbacks, const initializer_list<void*>& args) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (callbacks.empty()) {
        return true;
    }

    // Callbacks vector may be changed/invalidated during cycle work
    for (const auto& cb : copy(callbacks)) {
        const auto ex_policy = cb.ExPolicy;

        try {
            if (!cb.Callback(args)) {
                return false;
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);

            if (ex_policy == EventExceptionPolicy::IgnoreAndContinueChain) {
                continue;
            }
            if (ex_policy == EventExceptionPolicy::StopChainAndReturnTrue) {
                return true;
            }
            if (ex_policy == EventExceptionPolicy::StopChainAndReturnFalse) {
                return false;
            }
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    return true;
}

void Entity::MarkAsDestroying() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_VERIFY(!_isDestroying);
    FO_RUNTIME_VERIFY(!_isDestroyed);

    _isDestroying = true;
}

void Entity::MarkAsDestroyed() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_VERIFY(!_isDestroyed);

    _isDestroying = true;
    _isDestroyed = true;
}

void Entity::StoreData(bool with_protected, vector<const uint8*>** all_data, vector<uint32>** all_data_sizes) const
{
    FO_STACK_TRACE_ENTRY();

    _props.StoreData(with_protected, all_data, all_data_sizes);
}

void Entity::RestoreData(const vector<vector<uint8>>& props_data)
{
    FO_STACK_TRACE_ENTRY();

    _props.RestoreData(props_data);
}

void Entity::SetValueFromData(const Property* prop, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    _props.SetValueFromData(prop, prop_data);
}

auto Entity::GetValueAsInt(const Property* prop) const -> int32
{
    FO_STACK_TRACE_ENTRY();

    return _props.GetPlainDataValueAsInt(prop);
}

auto Entity::GetValueAsInt(int32 prop_index) const -> int32
{
    FO_STACK_TRACE_ENTRY();

    return _props.GetValueAsInt(prop_index);
}

auto Entity::GetValueAsAny(const Property* prop) const -> any_t
{
    FO_STACK_TRACE_ENTRY();

    return _props.GetPlainDataValueAsAny(prop);
}

auto Entity::GetValueAsAny(int32 prop_index) const -> any_t
{
    FO_STACK_TRACE_ENTRY();

    return _props.GetValueAsAny(prop_index);
}

void Entity::SetValueAsInt(const Property* prop, int32 value)
{
    FO_STACK_TRACE_ENTRY();

    _props.SetPlainDataValueAsInt(prop, value);
}

void Entity::SetValueAsInt(int32 prop_index, int32 value)
{
    FO_STACK_TRACE_ENTRY();

    _props.SetValueAsInt(prop_index, value);
}

void Entity::SetValueAsAny(const Property* prop, const any_t& value)
{
    FO_STACK_TRACE_ENTRY();

    _props.SetPlainDataValueAsAny(prop, value);
}

void Entity::SetValueAsAny(int32 prop_index, const any_t& value)
{
    FO_STACK_TRACE_ENTRY();

    _props.SetValueAsAny(prop_index, value);
}

auto Entity::GetInnerEntities(hstring entry) const noexcept -> const vector<refcount_ptr<Entity>>*
{
    FO_STACK_TRACE_ENTRY();

    if (!_innerEntities) {
        return nullptr;
    }

    const auto it_entry = _innerEntities->find(entry);

    if (it_entry == _innerEntities->end()) {
        return nullptr;
    }

    return &it_entry->second;
}

auto Entity::GetInnerEntities(hstring entry) noexcept -> vector<refcount_ptr<Entity>>*
{
    FO_STACK_TRACE_ENTRY();

    if (!_innerEntities) {
        return nullptr;
    }

    const auto it_entry = _innerEntities->find(entry);

    if (it_entry == _innerEntities->end()) {
        return nullptr;
    }

    return &it_entry->second;
}

void Entity::AddInnerEntity(hstring entry, Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    make_if_not_exists(_innerEntities);

    if (const auto it = _innerEntities->find(entry); it == _innerEntities->end()) {
        _innerEntities->emplace(entry, vector<refcount_ptr<Entity>> {entity});
    }
    else {
        vec_add_unique_value(it->second, entity);
    }
}

void Entity::RemoveInnerEntity(hstring entry, Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_innerEntities);
    FO_RUNTIME_ASSERT(entry);
    FO_RUNTIME_ASSERT(_innerEntities->count(entry));

    auto& entities = _innerEntities->at(entry);

    refcount_ptr entity_ref_holder = entity;
    vec_remove_unique_value(entities, entity);

    if (entities.empty()) {
        _innerEntities->erase(entry);
        destroy_if_empty(_innerEntities);
    }
}

void Entity::ClearInnerEntities()
{
    FO_STACK_TRACE_ENTRY();

    _innerEntities.reset();
}

auto Entity::HasTimeEvents() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _timeEvents && !_timeEvents->empty();
}

EntityEventBase::EntityEventBase(Entity* entity, const char* callback_name) noexcept :
    _entity {entity},
    _callbackName {callback_name}
{
    FO_STACK_TRACE_ENTRY();
}

void EntityEventBase::Subscribe(Entity::EventCallbackData&& callback)
{
    FO_STACK_TRACE_ENTRY();

    if (_callbacks == nullptr) {
        _callbacks = &_entity->GetEventCallbacks(_callbackName);
    }

    _entity->SubscribeEvent(*_callbacks, std::move(callback));
}

// ReSharper disable once CppMemberFunctionMayBeConst
void EntityEventBase::Unsubscribe(const void* subscription_ptr) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_callbacks == nullptr) {
        return;
    }

    _entity->UnsubscribeEvent(*_callbacks, subscription_ptr);
}

void EntityEventBase::UnsubscribeAll() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_callbacks == nullptr) {
        return;
    }

    _entity->UnsubscribeAllEvent(_callbackName);
    _callbacks = nullptr;
}

FO_END_NAMESPACE();
