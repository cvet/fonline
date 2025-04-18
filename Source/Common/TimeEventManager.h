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

#pragma once

#include "Common.h"

#include "Entity.h"

DECLARE_EXCEPTION(TimeEventException);

class FOEngineBase;

class TimeEventManager
{
public:
    explicit TimeEventManager(FOEngineBase* engine);
    TimeEventManager(const TimeEventManager&) = delete;
    TimeEventManager(TimeEventManager&&) noexcept = delete;
    auto operator=(const TimeEventManager&) = delete;
    auto operator=(TimeEventManager&&) noexcept = delete;
    ~TimeEventManager() = default;

    [[nodiscard]] auto GetCurTimeEvent() -> pair<Entity*, const Entity::TimeEventData*> { return {_curTimeEventEntity, _curTimeEvent}; }
    [[nodiscard]] auto CountTimeEvent(Entity* entity, hstring func_name, uint id) const -> size_t;

    void InitPersistentTimeEvents(Entity* entity);
    auto StartTimeEvent(Entity* entity, bool persistent, hstring func_name, time_duration_t delay, time_duration_t repeat, vector<any_t> data) -> uint;
    void ModifyTimeEvent(Entity* entity, hstring func_name, uint id, optional<time_duration_t> repeat, optional<vector<any_t>> data);
    void StopTimeEvent(Entity* entity, hstring func_name, uint id);
    void ProcessTimeEvents();

private:
    void AddEntityTimeEventPolling(Entity* entity);
    void RemoveEntityTimeEventPolling(Entity* entity);
    void ProcessEntityTimeEvents(Entity* entity, server_time_t time);
    void FireTimeEvent(Entity* entity, shared_ptr<Entity::TimeEventData> te);

    FOEngineBase* _engine;
    unordered_set<Entity*> _timeEventEntities {};
    Entity* _curTimeEventEntity {};
    const Entity::TimeEventData* _curTimeEvent {};
    uint _timeEventCounter {};
    const any_t _emptyAnyValue {};
    bool _nonConstHelper {};
};
