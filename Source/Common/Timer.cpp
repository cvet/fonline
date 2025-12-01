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

#include "Timer.h"

FO_BEGIN_NAMESPACE();

GameTimer::GameTimer(TimerSettings& settings) :
    _settings {&settings}
{
    FO_STACK_TRACE_ENTRY();

    _frameTime = nanotime::now();
    _fpsMeasureTime = _frameTime;
}

void GameTimer::SetSynchronizedTime(synctime time) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _syncTimeBase = time;
    _syncTimeSet = _frameTime;
}

void GameTimer::FrameAdvance()
{
    FO_STACK_TRACE_ENTRY();

    const auto prev_frame_time = _frameTime;
    const auto now_time = nanotime::now();

    // Skip time spent under debugger
    if (IsRunInDebugger() && _settings->DebuggingDeltaTimeCap != 0) {
        const auto dt = (now_time - _frameTime - _debuggingOffset).to_ms<int32>();

        if (dt > _settings->DebuggingDeltaTimeCap) {
            _debuggingOffset += std::chrono::milliseconds(dt - _settings->DebuggingDeltaTimeCap);
        }

        _frameTime = now_time - _debuggingOffset;
    }
    else {
        _frameTime = now_time;
    }

    _frameDeltaTime = _frameTime - prev_frame_time;

    // FPS counter
    if (_frameTime - _fpsMeasureTime >= std::chrono::seconds {1}) {
        _fps = _fpsMeasureCounter;
        _fpsMeasureTime = _frameTime;
        _fpsMeasureCounter = 0;
    }
    else {
        _fpsMeasureCounter++;
    }
}

auto GameTimer::GetSynchronizedTime() const -> synctime
{
    FO_STACK_TRACE_ENTRY();

    if (!_syncTimeBase) {
        throw TimeNotSyncException("Time is not synchronized yet");
    }

    return _syncTimeBase + (_frameTime - _syncTimeSet);
}

FO_END_NAMESPACE();
