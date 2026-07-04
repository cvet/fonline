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

#pragma once

#include "Common.h"

#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(TimeNotSyncException);

class GameTimer final
{
public:
    explicit GameTimer(ptr<TimerSettings> settings);
    GameTimer(const GameTimer&) = delete;
    GameTimer(GameTimer&&) noexcept = delete;
    auto operator=(const GameTimer&) = delete;
    auto operator=(GameTimer&&) noexcept = delete;
    ~GameTimer() = default;

    [[nodiscard]] auto GetFrameTime() const noexcept -> nanotime { return _frameTime.load(std::memory_order_relaxed); }
    [[nodiscard]] auto GetFrameDeltaTime() const noexcept -> timespan { return _frameDeltaTime.load(std::memory_order_relaxed); }
    [[nodiscard]] auto IsTimeSynchronized() const -> bool;
    [[nodiscard]] auto GetSynchronizedTime() const -> synctime;
    [[nodiscard]] auto GetFramesPerSecond() const noexcept -> int32_t { return _fps.load(std::memory_order_relaxed); }

    void SetSynchronizedTime(synctime time);
    void SetSynchronizedTimeMonotonic(synctime time);
    void FrameAdvance(bool clamp_to_cap);

private:
    ptr<TimerSettings> _settings;

    // Advanced on the main worker (FrameAdvance) but read from WorkerPool/network threads (entity
    // activity timestamps, GetSynchronizedTime), so these are atomic to avoid a data race.
    std::atomic<nanotime> _frameTime {};
    std::atomic<timespan> _frameDeltaTime {};
    timespan _debuggingOffset {}; // main-worker only

    // The synchronized-time projection reads this triple together, so it needs a consistent snapshot
    // against the main-worker writers — guarded by a mutex (the frame time is folded in via the atomic).
    mutable mutex _syncTimeLocker {};
    synctime _syncTimeBase FO_TSA_GUARDED_BY(_syncTimeLocker) {};
    synctime _syncTimeFloor FO_TSA_GUARDED_BY(_syncTimeLocker) {};
    nanotime _syncTimeSet FO_TSA_GUARDED_BY(_syncTimeLocker) {};

    std::atomic<int32_t> _fps {};
    nanotime _fpsMeasureTime {}; // main-worker only
    int32_t _fpsMeasureCounter {}; // main-worker only
};

FO_END_NAMESPACE
