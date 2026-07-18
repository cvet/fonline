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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE

Entity::Entity(ptr<const PropertyRegistrator> registrator, nptr<const Properties> init_props, nptr<const Properties> base_props) noexcept :
    _props {registrator, base_props}
{
    FO_STACK_TRACE_ENTRY();

    _props.SetEntity(this);

    if (init_props) {
        _props.CopyFrom(*init_props);
    }
}

void Entity::AddRef() const noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto old = _refCounter.fetch_add(1, std::memory_order_relaxed);
    FO_STRONG_ASSERT(old > 0, "AddRef called for expired entity", old);
}

void Entity::Release() const noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    int32_t old = _refCounter.fetch_sub(1, std::memory_order_acq_rel);
    FO_STRONG_ASSERT(old > 0, "Release called for expired entity", old);

    if (old == 1) {
        delete this;
    }
}

auto Entity::TryAddRef() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    int32_t count = _refCounter.load(std::memory_order_relaxed);

    while (count > 0) {
        if (_refCounter.compare_exchange_weak(count, count + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
            return true;
        }
    }

    return false;
}

auto Entity::HasEventCallbacks(string_view event_name) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_events) {
        if (auto it = _events->find(event_name); it != _events->end() && !it->second.empty()) {
            return true;
        }
    }

    return false;
}

auto Entity::FindEventCallbacks(string_view event_name) noexcept -> nptr<vector<EventCallbackData>>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_events) {
        if (auto it = _events->find(event_name); it != _events->end()) {
            return &it->second;
        }
    }

    return nullptr;
}

auto Entity::EnsureEventCallbacks(string_view event_name) -> ptr<vector<EventCallbackData>>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");

    if (!_events) {
        _events.emplace();
    }

    if (auto it = _events->find(event_name); it != _events->end()) {
        return &it->second;
    }

    return &_events->emplace(event_name, vector<EventCallbackData>()).first->second;
}

void Entity::SubscribeEvent(string_view event_name, EventCallbackData&& callback)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");

    auto callbacks = EnsureEventCallbacks(event_name);
    SubscribeEvent(callbacks, std::move(callback));
}

void Entity::UnsubscribeEvent(string_view event_name, uintptr_t subscription_ptr) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_events) {
        if (auto it = _events->find(event_name); it != _events->end()) {
            UnsubscribeEvent(&it->second, subscription_ptr);
        }
    }
}

void Entity::UnsubscribeAllEvent(string_view event_name) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_events) {
        if (auto it = _events->find(event_name); it != _events->end()) {
            it->second.clear();
        }
    }
}

void Entity::UnsubscribeAllEvents() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _events.reset();
}

void Entity::ClearAllTimeEvents() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _timeEvents.reset();
}

auto Entity::FireEvent(string_view event_name, FuncCallData& call) noexcept -> EventResult
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_RETURN_VALUE(!IsDestroyed(), EventResult::ContinueChain, "Destroyed entity tried to fire an event", GetName(), GetTypeName(), GetId(), event_name);

    if (_events) {
        if (auto it = _events->find(event_name); it != _events->end()) {
            return FireEvent(it->second, call);
        }
    }

    return EventResult::ContinueChain;
}

void Entity::SubscribeEvent(ptr<vector<EventCallbackData>> callbacks, EventCallbackData&& callback)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");

    if (callback.Priority >= EventPriority::Highest && std::ranges::find_if(*callbacks, [](const EventCallbackData& cb) { return cb.Priority >= EventPriority::Highest; }) != callbacks->end()) {
        throw GenericException("Highest callback already added");
    }

    if (callback.Priority <= EventPriority::Lowest && std::ranges::find_if(*callbacks, [](const EventCallbackData& cb) { return cb.Priority <= EventPriority::Lowest; }) != callbacks->end()) {
        throw GenericException("Lowest callback already added");
    }

    callbacks->emplace_back(std::move(callback));

    std::ranges::stable_sort(*callbacks, [](const EventCallbackData& cb1, const EventCallbackData& cb2) {
        // From highest to lowest
        return cb1.Priority > cb2.Priority;
    });
}

void Entity::UnsubscribeEvent(ptr<vector<EventCallbackData>> callbacks, uintptr_t subscription_ptr) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (auto it = std::ranges::find_if(*callbacks, [subscription_ptr](const auto& cb) { return cb.SubscriptionPtr == subscription_ptr; }); it != callbacks->end()) {
        callbacks->erase(it);
    }
}

auto Entity::FireEvent(const vector<EventCallbackData>& callbacks, FuncCallData& call) noexcept -> EventResult
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_RETURN_VALUE(!IsDestroyed(), EventResult::ContinueChain, "Destroyed entity tried to fire cached event callbacks", GetName(), GetTypeName(), GetId());

    if (callbacks.empty()) {
        return EventResult::ContinueChain;
    }

    bool had_exception = false;

    for (const auto& cb : copy(callbacks)) {
        EventResult result = EventResult::ContinueChain;

        try {
            result = cb.Callback(call);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            had_exception = true;

            // If callback has explicit result, then exception means that it failed to process event, so we should stop chain
            if (cb.HasExplicitResult) {
                return EventResult::StopChain;
            }
        }

        if (result == EventResult::StopChain) {
            return EventResult::StopChain;
        }
    }

    return had_exception ? EventResult::StopChain : EventResult::ContinueChain;
}

void Entity::MarkAsDestroying() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_CONTINUE(!IsDestroying(), "Entity is already marked as destroying", GetName(), GetTypeName(), GetId());
    FO_VERIFY_AND_CONTINUE(!IsDestroyed(), "Entity is already destroyed before MarkAsDestroying", GetName(), GetTypeName(), GetId());

    _isDestroying.store(true, std::memory_order_release);
}

void Entity::MarkAsDestroyed() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_CONTINUE(!IsDestroyed(), "Entity is already destroyed before MarkAsDestroyed", GetName(), GetTypeName(), GetId());

    UnsubscribeAllEvents();
    ClearAllTimeEvents();

    _isDestroying.store(true, std::memory_order_release);
    _isDestroyed.store(true, std::memory_order_release);
}

auto Entity::StoreData(bool with_protected) const -> Properties::StoredData
{
    FO_STACK_TRACE_ENTRY();

    return _props.StoreData(with_protected);
}

void Entity::RestoreData(const vector<vector<uint8_t>>& props_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");

    _props.RestoreData(props_data);
}

void Entity::SetValueFromData(ptr<const Property> prop, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");

    _props.SetValueFromData(prop, prop_data);
}

auto Entity::GetValueAsInt(ptr<const Property> prop) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return _props.GetPlainDataValueAsInt(prop);
}

auto Entity::GetValueAsInt(int32_t prop_index) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return _props.GetValueAsInt(prop_index);
}

auto Entity::GetValueAsAny(ptr<const Property> prop) const -> any_t
{
    FO_STACK_TRACE_ENTRY();

    return _props.GetPlainDataValueAsAny(prop);
}

auto Entity::GetValueAsAny(int32_t prop_index) const -> any_t
{
    FO_STACK_TRACE_ENTRY();

    return _props.GetValueAsAny(prop_index);
}

void Entity::SetValueAsInt(ptr<const Property> prop, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");

    _props.SetPlainDataValueAsInt(prop, value);
}

void Entity::SetValueAsInt(int32_t prop_index, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");

    _props.SetValueAsInt(prop_index, value);
}

void Entity::SetValueAsAny(ptr<const Property> prop, const any_t& value)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");

    _props.SetPlainDataValueAsAny(prop, value);
}

void Entity::SetValueAsAny(int32_t prop_index, const any_t& value)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");

    _props.SetValueAsAny(prop_index, value);
}

auto Entity::GetInnerEntities(hstring entry) const noexcept -> nptr<const vector<refcount_ptr<Entity>>>
{
    FO_STACK_TRACE_ENTRY();

    if (!_innerEntities) {
        return nullptr;
    }

    auto it_entry = _innerEntities->find(entry);

    if (it_entry == _innerEntities->end()) {
        return nullptr;
    }

    return &it_entry->second;
}

auto Entity::GetInnerEntities(hstring entry) noexcept -> nptr<vector<refcount_ptr<Entity>>>
{
    FO_STACK_TRACE_ENTRY();

    if (!_innerEntities) {
        return nullptr;
    }

    auto it_entry = _innerEntities->find(entry);

    if (it_entry == _innerEntities->end()) {
        return nullptr;
    }

    return &it_entry->second;
}

auto Entity::GetInnerEntitiesCount() const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    size_t count = 0;

    if (_innerEntities) {
        for (const auto& entities : *_innerEntities | std::views::values) {
            count += entities.size();
        }
    }

    return count;
}

void Entity::AddInnerEntity(hstring entry, ptr<Entity> entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Cannot add an inner entity to an already destroyed object");
    FO_VERIFY_AND_THROW(!IsDestroying(), "Cannot add an inner entity to an object that is being destroyed");
    FO_VERIFY_AND_THROW(!entity->IsDestroyed(), "Entity is already destroyed");

    if (!_innerEntities) {
        _innerEntities.emplace();
    }

    auto entity_ref_holder = entity.hold_ref();

    if (auto it = _innerEntities->find(entry); it == _innerEntities->end()) {
        _innerEntities->emplace(entry, vector<refcount_ptr<Entity>> {entity_ref_holder});
    }
    else {
        vec_add_unique_value(it->second, entity_ref_holder);
    }
}

void Entity::RemoveInnerEntity(hstring entry, ptr<Entity> entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");
    FO_VERIFY_AND_THROW(_innerEntities, "Missing required inner entities");
    FO_VERIFY_AND_THROW(entry, "Missing required entry");
    FO_VERIFY_AND_THROW(!entity->IsDestroyed(), "Entity is already destroyed");
    FO_VERIFY_AND_THROW(_innerEntities->count(entry), "Inner entity entry is not registered in holder index");

    auto& entities = _innerEntities->at(entry);

    auto entity_ref_holder = entity.hold_ref();
    vec_remove_unique_value(entities, entity_ref_holder);

    if (entities.empty()) {
        _innerEntities->erase(entry);

        if (_innerEntities->empty()) {
            _innerEntities.reset();
        }
    }
}

void Entity::ClearInnerEntities()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!IsDestroyed(), "Object is already destroyed");

    _innerEntities.reset();
}

auto Entity::HasTimeEvents() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _timeEvents && !_timeEvents->empty();
}

auto Entity::EnsureTimeEvents() -> ptr<TimeEventList>
{
    FO_STACK_TRACE_ENTRY();

    if (!_timeEvents) {
        _timeEvents.emplace();
    }

    return make_ptr(&*_timeEvents);
}

EntityEvent::EntityEvent(ptr<Entity> entity, string_view callback_name) noexcept :
    _entity {entity},
    _callbackName {callback_name}
{
    FO_NO_STACK_TRACE_ENTRY();
}

auto EntityEvent::FireEvent(FuncCallData& call) noexcept -> Entity::EventResult
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_RETURN_VALUE(!_entity->IsDestroyed(), Entity::EventResult::ContinueChain, "Destroyed entity tried to fire an entity event", _entity->GetName(), _entity->GetTypeName(), _entity->GetId(), _callbackName);

    nptr<vector<Entity::EventCallbackData>> callbacks = _callbacks.load(std::memory_order_acquire);
    FO_STRONG_ASSERT(callbacks, "Entity event fired without cached callbacks", _callbackName);
    return _entity->FireEvent(*callbacks, call);
}

auto EntityEvent::CheckCallbacks() -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_RETURN_VALUE(!_entity->IsDestroyed(), false, "Destroyed entity tried to check callbacks for an event", _entity->GetName(), _entity->GetTypeName(), _entity->GetId(), _callbackName);

    if (auto callbacks = _callbacks.load(std::memory_order_acquire); callbacks) {
        return !callbacks->empty();
    }

    if (auto callbacks = _entity->FindEventCallbacks(_callbackName)) {
        _callbacks.store(callbacks.get(), std::memory_order_release);
        return !callbacks->empty();
    }

    return false;
}

void EntityEvent::Subscribe(Entity::EventCallbackData&& callback)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_entity->IsDestroyed(), "Entity event wrapper target is already destroyed");

    _entity->SubscribeEvent(_callbackName, std::move(callback));
}

void EntityEvent::Unsubscribe(uintptr_t subscription_ptr) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_RETURN(!_entity->IsDestroyed(), "Destroyed entity tried to unsubscribe an event callback", _entity->GetName(), _entity->GetTypeName(), _entity->GetId(), _callbackName, subscription_ptr);

    _entity->UnsubscribeEvent(_callbackName, subscription_ptr);
}

void EntityEvent::UnsubscribeAll() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_RETURN(!_entity->IsDestroyed(), "Destroyed entity tried to unsubscribe all callbacks for an event", _entity->GetName(), _entity->GetTypeName(), _entity->GetId(), _callbackName);

    _entity->UnsubscribeAllEvent(_callbackName);
    _callbacks.store(nullptr, std::memory_order_release);
}

Entity::~Entity() = default;

FO_END_NAMESPACE
