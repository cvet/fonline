// FOnline Engine
// SPDX-License-Identifier: MIT

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
    [[nodiscard]] auto IsPaused() const noexcept -> bool { return _paused.load(std::memory_order_acquire); }
    [[nodiscard]] auto GetSynchronizedTime() const -> synctime;
    [[nodiscard]] auto IsSynchronizedTimeFrozen() const -> bool;
    [[nodiscard]] auto GetFramesPerSecond() const noexcept -> int32_t { return _fps.load(std::memory_order_relaxed); }

    void SetSynchronizedTime(synctime time);
    void SetSynchronizedTimeMonotonic(synctime time);
    // Stops the synchronized time at the later of its current value and time, while frame time keeps running
    void FreezeSynchronizedTime(synctime time);
    // Lets the synchronized time run again from the later of its frozen value and time
    void ThawSynchronizedTime(synctime time);
    void Pause();
    void Resume();
    void FrameAdvance(bool clamp_to_cap);

private:
    ptr<TimerSettings> _settings;

    // Advanced on the main worker (FrameAdvance) but read from WorkerPool/network threads (entity
    // activity timestamps, GetSynchronizedTime), so these are atomic to avoid a data race
    std::atomic<nanotime> _frameTime {};
    std::atomic<timespan> _frameDeltaTime {};
    timespan _debuggingOffset {}; // main-worker only
    mutable mutex _pauseLocker {};
    std::atomic<timespan> _pauseOffset {};
    nanotime _pauseStartedAt FO_TSA_GUARDED_BY(_pauseLocker) {};
    std::atomic_bool _paused {};

    // The synchronized-time projection reads this triple together, so it needs a consistent snapshot
    // against the main-worker writers — guarded by a mutex (the frame time is folded in via the atomic)
    mutable mutex _syncTimeLocker {};
    synctime _syncTimeBase FO_TSA_GUARDED_BY(_syncTimeLocker) {};
    synctime _syncTimeFloor FO_TSA_GUARDED_BY(_syncTimeLocker) {};
    nanotime _syncTimeSet FO_TSA_GUARDED_BY(_syncTimeLocker) {};
    bool _syncTimeFrozen FO_TSA_GUARDED_BY(_syncTimeLocker) {};

    std::atomic<int32_t> _fps {};
    nanotime _fpsMeasureTime {}; // main-worker only
    int32_t _fpsMeasureCounter {}; // main-worker only
};

FO_END_NAMESPACE
