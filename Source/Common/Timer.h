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

#include "Settings.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(TimeNotSyncException);

class GameTimer final
{
public:
    explicit GameTimer(TimerSettings& settings);
    GameTimer(const GameTimer&) = delete;
    GameTimer(GameTimer&&) noexcept = default;
    auto operator=(const GameTimer&) = delete;
    auto operator=(GameTimer&&) noexcept = delete;
    ~GameTimer() = default;

    [[nodiscard]] auto GetFrameTime() const noexcept -> nanotime { return _frameTime; }
    [[nodiscard]] auto GetFrameDeltaTime() const noexcept -> timespan { return _frameDeltaTime; }
    [[nodiscard]] auto IsTimeSynchronized() const noexcept -> bool { return !!_syncTimeBase; }
    [[nodiscard]] auto GetSynchronizedTime() const -> synctime;
    [[nodiscard]] auto GetFramesPerSecond() const noexcept -> int32 { return _fps; }

    void SetSynchronizedTime(synctime time) noexcept;
    void FrameAdvance();

private:
    raw_ptr<TimerSettings> _settings;

    nanotime _frameTime {};
    timespan _frameDeltaTime {};
    timespan _debuggingOffset {};

    synctime _syncTimeBase {};
    nanotime _syncTimeSet {};

    int32 _fps {};
    nanotime _fpsMeasureTime {};
    int32 _fpsMeasureCounter {};
};

FO_END_NAMESPACE();
