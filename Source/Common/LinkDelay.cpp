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

#include "LinkDelay.h"

FO_BEGIN_NAMESPACE

// A burst of stamped messages keeps the window bounded even when the window time alone would not
static constexpr size_t MAX_LINK_DELAY_SAMPLES = 256;
// The transit a link had before it fell silent is still its transit: this many samples outlive the window
static constexpr size_t MIN_LINK_DELAY_SAMPLES = 8;

LinkDelayEstimator::LinkDelayEstimator(timespan window, timespan exceptional, timespan rebase) noexcept :
    _windowMs {window.milliseconds()},
    _exceptionalMs {exceptional.milliseconds()},
    _rebaseMs {rebase.milliseconds()}
{
}

auto LinkDelayEstimator::AddSample(int64_t sender_ms, nanotime receive_time) -> timespan
{
    int64_t receive_ms = receive_time.milliseconds();
    Sample sample {.ReceiveMs = receive_ms, .OffsetMs = receive_ms - sender_ms};

    while (_usual.size() > MIN_LINK_DELAY_SAMPLES && (_usual.front().ReceiveMs < receive_ms - _windowMs || _usual.size() >= MAX_LINK_DELAY_SAMPLES)) {
        _usual.pop_front();
    }

    if (_usual.empty()) {
        _usual.emplace_back(sample);
        return timespan::zero;
    }

    int64_t late_ms = sample.OffsetMs - EvaluateUsualOffset();

    if (late_ms < _exceptionalMs) {
        _exceptionalRun.clear();
        _exceptionalSinceMs.reset();
        _usual.emplace_back(sample);
        return std::chrono::milliseconds {std::max(late_ms, int64_t {0})};
    }

    if (!_exceptionalSinceMs.has_value()) {
        _exceptionalSinceMs = receive_ms;
    }

    if (_exceptionalRun.size() >= MAX_LINK_DELAY_SAMPLES) {
        _exceptionalRun.pop_front();
    }

    _exceptionalRun.emplace_back(sample);

    // Late for this long without a break is no stall: the route itself got slower, and its transit is the usual one now
    if (receive_ms - _exceptionalSinceMs.value() >= _rebaseMs) {
        _usual = std::move(_exceptionalRun);
        _exceptionalRun.clear();
        _exceptionalSinceMs.reset();
        late_ms = sample.OffsetMs - EvaluateUsualOffset();
    }

    return std::chrono::milliseconds {std::max(late_ms, int64_t {0})};
}

void LinkDelayEstimator::Reset() noexcept
{
    _usual.clear();
    _exceptionalRun.clear();
    _exceptionalSinceMs.reset();
}

auto LinkDelayEstimator::EvaluateUsualOffset() const -> int64_t
{
    FO_VERIFY_AND_THROW(!_usual.empty(), "Usual link transit needs a sample");

    small_vector<int64_t, MAX_LINK_DELAY_SAMPLES> offsets;

    for (const Sample& usual_sample : _usual) {
        offsets.emplace_back(usual_sample.OffsetMs);
    }

    auto middle = offsets.begin() + numeric_cast<ptrdiff_t>(offsets.size() / 2);
    std::nth_element(offsets.begin(), middle, offsets.end());
    return *middle;
}

auto EvaluateLateCatchUp(timespan late_time, timespan min_time, timespan max_time) noexcept -> timespan
{
    if (late_time < min_time) {
        return timespan::zero;
    }

    return std::min(late_time, max_time);
}

FO_END_NAMESPACE
