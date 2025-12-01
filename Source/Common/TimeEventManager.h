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

#include "ScriptSystem.h"
#include "Timer.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(TimeEventException);

class TimeEventManager
{
public:
    static const timespan MIN_REPEAT_TIME;

    explicit TimeEventManager(GameTimer& game_time, ScriptSystem& script_sys);
    TimeEventManager(const TimeEventManager&) = delete;
    TimeEventManager(TimeEventManager&&) noexcept = delete;
    auto operator=(const TimeEventManager&) = delete;
    auto operator=(TimeEventManager&&) noexcept = delete;
    ~TimeEventManager() = default;

    [[nodiscard]] auto GetCurTimeEvent() -> pair<Entity*, const Entity::TimeEventData*> { return {_curTimeEventEntity.get(), _curTimeEvent.get()}; }
    [[nodiscard]] auto CountTimeEvent(Entity* entity, hstring func_name, uint32 id) const -> size_t;

    auto StartTimeEvent(Entity* entity, hstring func_name, timespan delay, timespan repeat, vector<any_t> data) -> uint32;
    void ModifyTimeEvent(Entity* entity, hstring func_name, uint32 id, optional<timespan> repeat, optional<vector<any_t>> data);
    void StopTimeEvent(Entity* entity, hstring func_name, uint32 id);
    void ProcessTimeEvents();

private:
    void AddEntityTimeEventPolling(Entity* entity);
    void RemoveEntityTimeEventPolling(Entity* entity);
    void ProcessEntityTimeEvents(Entity* entity);
    auto FireTimeEvent(Entity* entity, shared_ptr<Entity::TimeEventData> te) -> bool;

    raw_ptr<GameTimer> _gameTime;
    raw_ptr<ScriptSystem> _scriptSys;
    unordered_set<refcount_ptr<Entity>> _timeEventEntities {};
    raw_ptr<Entity> _curTimeEventEntity {};
    raw_ptr<const Entity::TimeEventData> _curTimeEvent {};
    uint32 _timeEventCounter {};
    any_t _emptyAnyValue {};
    bool _nonConstHelper {};
};

FO_END_NAMESPACE();
