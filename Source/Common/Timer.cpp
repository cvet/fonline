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

#include "Timer.h"

FO_BEGIN_NAMESPACE

GameTimer::GameTimer(TimerSettings& settings) :
    _settings {&settings}
{
    FO_STACK_TRACE_ENTRY();

    const auto start_time = nanotime::now();
    _frameTime.store(start_time, std::memory_order_relaxed);
    _fpsMeasureTime = start_time;
}

auto GameTimer::IsTimeSynchronized() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_syncTimeLocker};

    return !!_syncTimeBase;
}

void GameTimer::SetSynchronizedTime(synctime time)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_syncTimeLocker};

    _syncTimeBase = time;
    _syncTimeFloor = time;
    _syncTimeSet = _frameTime.load(std::memory_order_relaxed);
}

void GameTimer::SetSynchronizedTimeMonotonic(synctime time)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_syncTimeLocker};

    const auto frame_time = _frameTime.load(std::memory_order_relaxed);

    if (!_syncTimeBase) {
        _syncTimeBase = time;
        _syncTimeFloor = time;
        _syncTimeSet = frame_time;
        return;
    }

    const auto projected_time = _syncTimeBase + (frame_time - _syncTimeSet);
    const auto current_time = projected_time < _syncTimeFloor ? _syncTimeFloor : projected_time;
    const bool is_frozen = current_time > projected_time;

    _syncTimeBase = is_frozen && time < projected_time ? projected_time : time;
    _syncTimeFloor = time > current_time ? time : current_time;
    _syncTimeSet = frame_time;
}

void GameTimer::FrameAdvance(bool clamp_to_cap)
{
    FO_STACK_TRACE_ENTRY();

    const auto prev_frame_time = _frameTime.load(std::memory_order_relaxed);
    const auto now_time = nanotime::now();
    nanotime new_frame_time;

    if (clamp_to_cap && _settings->DeltaTimeCap != 0) {
        const auto dt = (now_time - prev_frame_time - _debuggingOffset).to_ms<int32_t>();

        if (dt > _settings->DeltaTimeCap) {
            _debuggingOffset += std::chrono::milliseconds(dt - _settings->DeltaTimeCap);
        }

        new_frame_time = now_time - _debuggingOffset;
    }
    else {
        new_frame_time = now_time;
    }

    _frameTime.store(new_frame_time, std::memory_order_relaxed);
    _frameDeltaTime.store(new_frame_time - prev_frame_time, std::memory_order_relaxed);

    // FPS counter (main-worker-only state)
    if (new_frame_time - _fpsMeasureTime >= std::chrono::seconds {1}) {
        _fps.store(_fpsMeasureCounter, std::memory_order_relaxed);
        _fpsMeasureTime = new_frame_time;
        _fpsMeasureCounter = 0;
    }
    else {
        _fpsMeasureCounter++;
    }
}

auto GameTimer::GetSynchronizedTime() const -> synctime
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_syncTimeLocker};

    if (!_syncTimeBase) {
        throw TimeNotSyncException("Time is not synchronized yet");
    }

    const auto projected_time = _syncTimeBase + (_frameTime.load(std::memory_order_relaxed) - _syncTimeSet);
    return projected_time < _syncTimeFloor ? _syncTimeFloor : projected_time;
}

FO_END_NAMESPACE
