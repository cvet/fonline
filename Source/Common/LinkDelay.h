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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

FO_BEGIN_NAMESPACE

// How much longer than a link usually takes a stamped message took; only differences between samples of one link are
// used, so the two ends need no shared clock
class LinkDelayEstimator final
{
public:
    LinkDelayEstimator(timespan window, timespan exceptional, timespan rebase) noexcept;

    [[nodiscard]] auto GetSampleCount() const noexcept -> size_t { return _usual.size() + _exceptionalRun.size(); }

    // A message sent at sender_ms by the sender's clock and received at receive_time: how much longer than the median
    // usual transit it travelled, zero when it was not later than that
    auto AddSample(int64_t sender_ms, nanotime receive_time) -> timespan;
    // A new connection is a new link: its transit is measured afresh
    void Reset() noexcept;

private:
    struct Sample
    {
        int64_t ReceiveMs {};
        int64_t OffsetMs {};
    };

    [[nodiscard]] auto EvaluateUsualOffset() const -> int64_t;

    int64_t _windowMs;
    int64_t _exceptionalMs;
    int64_t _rebaseMs;
    // A stall's burst stays out of what the link usually takes, or it would hide how late its own later messages were
    deque<Sample> _usual {};
    deque<Sample> _exceptionalRun {};
    optional<int64_t> _exceptionalSinceMs {};
};

// How far ahead a late movement plan is played: not at all below the lateness an ordinary jittery link produces, the
// lateness itself above it, never more than the upper bound
[[nodiscard]] auto EvaluateLateCatchUp(timespan late_time, timespan min_time, timespan max_time) noexcept -> timespan;

FO_END_NAMESPACE
