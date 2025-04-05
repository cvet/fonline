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

#include "TimeEventManager.h"
#include "EngineBase.h"
#include "ScriptSystem.h"

TimeEventManager::TimeEventManager(FOEngineBase* engine) :
    _engine(engine)
{
    STACK_TRACE_ENTRY();
}

void TimeEventManager::InitPersistentTimeEvents(Entity* entity)
{
    STACK_TRACE_ENTRY();

    auto& persistentTimeEvents = entity->GetRawPeristentTimeEvents();
    persistentTimeEvents.reset();

    const auto props = EntityProperties(entity->GetPropertiesForEdit());

    if (!props.GetPropertyTE_FuncName()->IsDisabled()) {
        if (props.IsNonEmptyTE_FuncName()) {
            const auto te_func_name = props.GetTE_FuncName();
            const auto te_fire_time = props.GetTE_FireTime();
            const auto te_repeat_duration = props.GetTE_RepeatDuration();
            auto te_data = props.GetTE_Data();
            RUNTIME_ASSERT(te_func_name.size() == te_fire_time.size());
            RUNTIME_ASSERT(te_func_name.size() == te_repeat_duration.size());
            RUNTIME_ASSERT(te_func_name.size() == te_data.size());

            make_if_not_exists(persistentTimeEvents);

            for (size_t i = 0; i < te_func_name.size(); i++) {
                auto data = vector<any_t> {std::move(te_data[i])};
                auto te = Entity::TimeEventData {++_timeEventCounter, te_func_name[i], te_fire_time[i], te_repeat_duration[i], std::move(data)};
                persistentTimeEvents->emplace_back(SafeAlloc::MakeShared<Entity::TimeEventData>(std::move(te)));
            }
        }
        else {
            RUNTIME_ASSERT(!props.IsNonEmptyTE_FireTime());
            RUNTIME_ASSERT(!props.IsNonEmptyTE_RepeatDuration());
            RUNTIME_ASSERT(!props.IsNonEmptyTE_Data());
        }
    }
}

auto TimeEventManager::StartTimeEvent(Entity* entity, bool persistent, hstring func_name, tick_t delay, tick_t repeat, vector<any_t> data) -> uint
{
    STACK_TRACE_ENTRY();

    const auto fire_time = tick_t {_engine->GameTime.GetServerTime().underlying_value() + std::max(delay.underlying_value(), 1u)};
    auto te = SafeAlloc::MakeShared<Entity::TimeEventData>(Entity::TimeEventData {++_timeEventCounter, func_name, fire_time, repeat, std::move(data)});

    if (persistent) {
        RUNTIME_ASSERT(te->Data.size() <= 1);

        auto props = EntityProperties(entity->GetPropertiesForEdit());
        auto te_func_name = props.GetTE_FuncName();
        auto te_fire_time = props.GetTE_FireTime();
        auto te_repeat_duration = props.GetTE_RepeatDuration();
        auto te_data = props.GetTE_Data();
        RUNTIME_ASSERT(te_func_name.size() == te_fire_time.size());
        RUNTIME_ASSERT(te_func_name.size() == te_repeat_duration.size());
        RUNTIME_ASSERT(te_func_name.size() == te_data.size());

        te_func_name.emplace_back(te->FuncName);
        te_fire_time.emplace_back(te->FireTime);
        te_repeat_duration.emplace_back(te->RepeatDuration);
        te_data.emplace_back(!te->Data.empty() ? te->Data.front() : any_t {});

        props.SetTE_FuncName(te_func_name);
        props.SetTE_FireTime(te_fire_time);
        props.SetTE_RepeatDuration(te_repeat_duration);
        props.SetTE_Data(te_data);

        auto& persistentTimeEvents = entity->GetRawPeristentTimeEvents();
        make_if_not_exists(persistentTimeEvents);
        persistentTimeEvents->emplace_back(std::move(te));
    }
    else {
        auto& timeEvents = entity->GetRawTimeEvents();
        make_if_not_exists(timeEvents);
        timeEvents->emplace_back(std::move(te));
    }

    AddEntityTimeEventPolling(entity);
    return _timeEventCounter;
}

auto TimeEventManager::CountTimeEvent(Entity* entity, hstring func_name, uint id) const -> size_t
{
    STACK_TRACE_ENTRY();

    const auto& timeEvents = entity->GetRawTimeEvents();
    const auto& persistentTimeEvents = entity->GetRawPeristentTimeEvents();

    if (id != 0) {
        RUNTIME_ASSERT(!func_name);

        if (timeEvents && !timeEvents->empty()) {
            for (const auto& te : *timeEvents) {
                if (te->Id == id) {
                    return 1;
                }
            }
        }

        if (persistentTimeEvents && !persistentTimeEvents->empty()) {
            for (const auto& te : *persistentTimeEvents) {
                if (te->Id == id) {
                    return 1;
                }
            }
        }
    }

    if (func_name) {
        size_t count = 0;

        if (timeEvents && !timeEvents->empty()) {
            for (const auto& te : *timeEvents) {
                if (te->FuncName == func_name) {
                    count++;
                }
            }
        }

        if (persistentTimeEvents && !persistentTimeEvents->empty()) {
            for (const auto& te : *persistentTimeEvents) {
                if (te->FuncName == func_name) {
                    count++;
                }
            }
        }

        return count;
    }

    return 0;
}

void TimeEventManager::ModifyTimeEvent(Entity* entity, hstring func_name, uint id, optional<tick_t> repeat, optional<vector<any_t>> data)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto fire_time = repeat.has_value() ? tick_t {_engine->GameTime.GetServerTime().underlying_value() + std::max(repeat.value().underlying_value(), 1u)} : tick_t {};

    if (auto& timeEvents = entity->GetRawTimeEvents(); timeEvents && !timeEvents->empty()) {
        for (size_t i = 0; i < timeEvents->size(); i++) {
            auto& te = (*timeEvents)[i];

            if (func_name && te->FuncName != func_name) {
                continue;
            }
            if (id != 0 && te->Id != id) {
                continue;
            }

            if (repeat.has_value()) {
                te->FireTime = fire_time;
            }
            if (repeat.has_value()) {
                te->RepeatDuration = repeat.value();
            }
            if (data.has_value()) {
                te->Data = data.value();
            }

            // Identifier may be only one
            if (id != 0) {
                return;
            }
        }
    }

    if (auto& persistentTimeEvents = entity->GetRawPeristentTimeEvents(); persistentTimeEvents && !persistentTimeEvents->empty()) {
        bool te_loaded = false;
        vector<tick_t> te_fire_time;
        vector<tick_t> te_repeat_duration;
        vector<any_t> te_data;

        for (size_t i = 0; i < persistentTimeEvents->size(); i++) {
            auto& te = (*persistentTimeEvents)[i];

            if (func_name && te->FuncName != func_name) {
                continue;
            }
            if (id != 0 && te->Id != id) {
                continue;
            }

            if (repeat.has_value()) {
                te->FireTime = fire_time;
            }
            if (repeat.has_value()) {
                te->RepeatDuration = repeat.value();
            }
            if (data.has_value()) {
                RUNTIME_ASSERT(data.value().size() <= 1);
                te->Data = data.value();
            }

            if (!te_loaded) {
                auto props = EntityProperties(entity->GetPropertiesForEdit());
                te_fire_time = props.GetTE_FireTime();
                te_repeat_duration = props.GetTE_RepeatDuration();
                te_data = props.GetTE_Data();
                RUNTIME_ASSERT(te_fire_time.size() == te_repeat_duration.size());
                RUNTIME_ASSERT(te_fire_time.size() == te_data.size());
                te_loaded = true;
            }

            RUNTIME_ASSERT(i < te_fire_time.size());
            te_fire_time[i] = te->FireTime;
            te_repeat_duration[i] = te->RepeatDuration;
            te_data[i] = !te->Data.empty() ? te->Data.front() : any_t {};

            // Identifier may be only one
            if (id != 0) {
                break;
            }
        }

        if (te_loaded) {
            auto props = EntityProperties(entity->GetPropertiesForEdit());
            props.SetTE_RepeatDuration(te_repeat_duration);
            props.SetTE_Data(te_data);
            props.SetTE_FireTime(te_fire_time);
        }
    }
}

void TimeEventManager::StopTimeEvent(Entity* entity, hstring func_name, uint id)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (auto& timeEvents = entity->GetRawTimeEvents(); timeEvents && !timeEvents->empty()) {
        for (size_t i = 0; i < timeEvents->size();) {
            auto& te = (*timeEvents)[i];

            if (func_name && te->FuncName != func_name) {
                i++;
                continue;
            }
            if (id != 0 && te->Id != id) {
                i++;
                continue;
            }

            te->Id = 0;
            timeEvents->erase(timeEvents->begin() + static_cast<ptrdiff_t>(i)); // te is not valid anymore

            // Identifier may be only one
            if (id != 0) {
                return;
            }
        }
    }

    if (auto& persistentTimeEvents = entity->GetRawPeristentTimeEvents(); persistentTimeEvents && !persistentTimeEvents->empty()) {
        bool te_loaded = false;
        vector<hstring> te_func_name;
        vector<tick_t> te_fire_time;
        vector<tick_t> te_repeat_duration;
        vector<string> te_name;
        vector<any_t> te_data;

        for (size_t i = 0; i < persistentTimeEvents->size();) {
            auto& te = (*persistentTimeEvents)[i];

            if (func_name && te->FuncName != func_name) {
                i++;
                continue;
            }
            if (id != 0 && te->Id != id) {
                i++;
                continue;
            }

            if (!te_loaded) {
                auto props = EntityProperties(entity->GetPropertiesForEdit());
                te_func_name = props.GetTE_FuncName();
                te_fire_time = props.GetTE_FireTime();
                te_repeat_duration = props.GetTE_RepeatDuration();
                te_data = props.GetTE_Data();
                RUNTIME_ASSERT(te_func_name.size() == persistentTimeEvents->size());
                RUNTIME_ASSERT(te_func_name.size() == te_fire_time.size());
                RUNTIME_ASSERT(te_func_name.size() == te_repeat_duration.size());
                RUNTIME_ASSERT(te_func_name.size() == te_name.size());
                RUNTIME_ASSERT(te_func_name.size() == te_data.size());
                te_loaded = true;
            }

            te_func_name.erase(te_func_name.begin() + static_cast<ptrdiff_t>(i));
            te_fire_time.erase(te_fire_time.begin() + static_cast<ptrdiff_t>(i));
            te_repeat_duration.erase(te_repeat_duration.begin() + static_cast<ptrdiff_t>(i));
            te_name.erase(te_name.begin() + static_cast<ptrdiff_t>(i));
            te_data.erase(te_data.begin() + static_cast<ptrdiff_t>(i));

            te->Id = 0;
            persistentTimeEvents->erase(persistentTimeEvents->begin() + static_cast<ptrdiff_t>(i)); // te is not valid anymore

            // Identifier may be only one
            if (id != 0) {
                break;
            }
        }

        if (te_loaded) {
            auto props = EntityProperties(entity->GetPropertiesForEdit());
            props.SetTE_FuncName(te_func_name);
            props.SetTE_FireTime(te_fire_time);
            props.SetTE_RepeatDuration(te_repeat_duration);
            props.SetTE_Data(te_data);
        }
    }
}

void TimeEventManager::AddEntityTimeEventPolling(Entity* entity)
{
    STACK_TRACE_ENTRY();

    if (_timeEventEntities.emplace(entity).second) {
        entity->AddRef();
    }
}

void TimeEventManager::RemoveEntityTimeEventPolling(Entity* entity)
{
    STACK_TRACE_ENTRY();

    if (_timeEventEntities.erase(entity) != 0) {
        entity->Release();
    }
}

void TimeEventManager::ProcessTimeEvents()
{
    STACK_TRACE_ENTRY();

    const auto time = _engine->GameTime.GetServerTime();

    for (auto* entity : copy_hold_ref(_timeEventEntities)) {
        if (entity->IsDestroyed()) {
            RemoveEntityTimeEventPolling(entity);
            continue;
        }

        ProcessEntityTimeEvents(entity, time);

        if (entity->IsDestroyed() || !entity->HasTimeEvents()) {
            RemoveEntityTimeEventPolling(entity);
        }
    }
}

void TimeEventManager::ProcessEntityTimeEvents(Entity* entity, tick_t time)
{
    STACK_TRACE_ENTRY();

    if (auto& timeEvents = entity->GetRawTimeEvents(); timeEvents && !timeEvents->empty()) {
        for (size_t i = 0; i < timeEvents->size(); i++) {
            auto te = (*timeEvents)[i];
            const auto id = te->Id;

            if (te->FireTime.underlying_value() > time.underlying_value()) {
                continue;
            }

            FireTimeEvent(entity, te);

            if (entity->IsDestroyed()) {
                return;
            }

            if (te->Id == 0) {
                // Event was stopped
                continue;
            }

            if (te->FireTime.underlying_value() > time.underlying_value()) {
                // Event was already prolonged
                continue;
            }

            if (te->RepeatDuration) {
                // Prolong event
                const auto next_fire_time = tick_t {std::max(te->FireTime.underlying_value() + te->RepeatDuration.underlying_value(), time.underlying_value() + 1)};

                te->FireTime = next_fire_time;
            }
            else {
                // Remove event
                const auto it = std::find_if(timeEvents->begin(), timeEvents->end(), [id](const shared_ptr<Entity::TimeEventData>& te2) { return te2->Id == id; });
                RUNTIME_ASSERT(it != timeEvents->end());
                const auto actual_index = static_cast<size_t>(std::distance(timeEvents->begin(), it));

                timeEvents->erase(timeEvents->begin() + static_cast<ptrdiff_t>(actual_index));
                te->Id = 0;
            }
        }
    }

    if (auto& persistentTimeEvents = entity->GetRawPeristentTimeEvents(); persistentTimeEvents && !persistentTimeEvents->empty()) {
        for (size_t i = 0; i < persistentTimeEvents->size(); i++) {
            auto te = (*persistentTimeEvents)[i];
            const auto id = te->Id;

            if (te->FireTime.underlying_value() > time.underlying_value()) {
                continue;
            }

            FireTimeEvent(entity, te);

            if (entity->IsDestroyed()) {
                return;
            }

            if (te->Id == 0) {
                // Event was stopped
                continue;
            }

            if (te->FireTime.underlying_value() > time.underlying_value()) {
                // Event was already prolonged
                continue;
            }

            const auto it = std::find_if(persistentTimeEvents->begin(), persistentTimeEvents->end(), [id](const shared_ptr<Entity::TimeEventData>& te2) { return te2->Id == id; });
            RUNTIME_ASSERT(it != persistentTimeEvents->end());
            const auto actual_index = static_cast<size_t>(std::distance(persistentTimeEvents->begin(), it));

            if (te->RepeatDuration) {
                // Prolong event
                const auto next_fire_time = tick_t {std::max(te->FireTime.underlying_value() + te->RepeatDuration.underlying_value(), time.underlying_value() + 1)};

                auto props = EntityProperties(entity->GetPropertiesForEdit());
                auto te_fire_time = props.GetTE_FireTime();
                RUNTIME_ASSERT(te_fire_time.size() == persistentTimeEvents->size());

                te_fire_time[actual_index] = next_fire_time;

                props.SetTE_FireTime(te_fire_time);

                te->FireTime = next_fire_time;
            }
            else {
                // Remove event
                auto props = EntityProperties(entity->GetPropertiesForEdit());
                auto te_func_name = props.GetTE_FuncName();
                auto te_fire_time = props.GetTE_FireTime();
                auto te_repeat_duration = props.GetTE_RepeatDuration();
                auto te_data = props.GetTE_Data();
                RUNTIME_ASSERT(te_func_name.size() == persistentTimeEvents->size());
                RUNTIME_ASSERT(te_func_name.size() == te_fire_time.size());
                RUNTIME_ASSERT(te_func_name.size() == te_repeat_duration.size());
                RUNTIME_ASSERT(te_func_name.size() == te_data.size());

                te_func_name.erase(te_func_name.begin() + static_cast<ptrdiff_t>(actual_index));
                te_fire_time.erase(te_fire_time.begin() + static_cast<ptrdiff_t>(actual_index));
                te_repeat_duration.erase(te_repeat_duration.begin() + static_cast<ptrdiff_t>(actual_index));
                te_data.erase(te_data.begin() + static_cast<ptrdiff_t>(actual_index));

                props.SetTE_FuncName(te_func_name);
                props.SetTE_FireTime(te_fire_time);
                props.SetTE_RepeatDuration(te_repeat_duration);
                props.SetTE_Data(te_data);

                persistentTimeEvents->erase(persistentTimeEvents->begin() + static_cast<ptrdiff_t>(actual_index));
                te->Id = 0;
            }
        }
    }
}

void TimeEventManager::FireTimeEvent(Entity* entity, shared_ptr<Entity::TimeEventData> te) // NOLINT(performance-unnecessary-value-param)
{
    STACK_TRACE_ENTRY();

    _curTimeEventEntity = entity;
    auto revert_cur_entity = ScopeCallback([this]() noexcept { _curTimeEventEntity = nullptr; });
    _curTimeEvent = te.get();
    auto revert_cur_event = ScopeCallback([this]() noexcept { _curTimeEvent = nullptr; });

    if (entity->IsGlobal()) {
        if (auto func = _engine->ScriptSys->FindFunc<void>(te->FuncName)) {
            func();
        }
        else if (auto func2 = _engine->ScriptSys->FindFunc<void, any_t>(te->FuncName)) {
            func2(te->Data.empty() ? _emptyAnyValue : te->Data.front());
        }
        else if (auto func3 = _engine->ScriptSys->FindFunc<void, vector<any_t>>(te->FuncName)) {
            func3(te->Data);
        }
        else {
            throw TimeEventException("Time event func not found", te->FuncName);
        }
    }
    else {
        const auto cast_to_void = [](auto* ptr) -> void* { return const_cast<void*>(static_cast<const void*>(ptr)); };

        if (const auto* func = _engine->ScriptSys->FindFunc(te->FuncName, initializer_list<std::type_index> {std::type_index(typeid(*entity))})) {
            func->Call(initializer_list<void*> {cast_to_void(&entity)}, nullptr);
        }
        else if (const auto* func2 = _engine->ScriptSys->FindFunc(te->FuncName, initializer_list<std::type_index> {std::type_index(typeid(*entity)), std::type_index(typeid(any_t))})) {
            func2->Call(initializer_list<void*> {cast_to_void(&entity), cast_to_void(!te->Data.empty() ? &te->Data.front() : &_emptyAnyValue)}, nullptr);
        }
        else if (const auto* func3 = _engine->ScriptSys->FindFunc(te->FuncName, initializer_list<std::type_index> {std::type_index(typeid(*entity)), std::type_index(typeid(vector<any_t>))})) {
            func3->Call(initializer_list<void*> {cast_to_void(&entity), cast_to_void(&te->Data)}, nullptr);
        }
        else {
            throw TimeEventException("Time event func not found", te->FuncName);
        }
    }
}
