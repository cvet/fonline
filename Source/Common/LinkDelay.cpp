// FOnline Engine
// SPDX-License-Identifier: MIT

#include "LinkDelay.h"

FO_BEGIN_NAMESPACE

// A burst of stamped messages keeps the window bounded even when the window time alone would not
static constexpr size_t MAX_LINK_DELAY_SAMPLES {256};
// The transit a link had before it fell silent is still its transit: this many samples outlive the window
static constexpr size_t MIN_LINK_DELAY_SAMPLES {8};

LinkDelayEstimator::LinkDelayEstimator(timespan window, timespan exceptional, timespan rebase) noexcept :
    _window {window},
    _exceptional {exceptional},
    _rebase {rebase}
{
}

auto LinkDelayEstimator::AddSample(int64_t sender_ms, nanotime receive_time) -> timespan
{
    constexpr int64_t max_stamp_ms {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds::max()).count()};
    constexpr int64_t min_stamp_ms {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds::min()).count()};

    FO_VERIFY_AND_THROW(sender_ms >= min_stamp_ms && sender_ms <= max_stamp_ms, "Sender timestamp is outside the native clock range", sender_ms);

    int64_t receive_ms {receive_time.milliseconds()};
    Sample sample {.ReceivedAt = receive_time, .OffsetMs = receive_ms - sender_ms};

    while (_usual.size() > MIN_LINK_DELAY_SAMPLES && (receive_time - _usual.front().ReceivedAt > _window || _usual.size() >= MAX_LINK_DELAY_SAMPLES)) {
        _usual.pop_front();
    }

    if (_usual.empty()) {
        _usual.emplace_back(sample);
        return timespan::zero;
    }

    int64_t late_ms {sample.OffsetMs - EvaluateUsualOffset()};

    timespan late_time {std::chrono::milliseconds {std::clamp(late_ms, -max_stamp_ms, max_stamp_ms)}};

    if (late_time < _exceptional) {
        _exceptionalRun.clear();
        _exceptionalSince.reset();
        _usual.emplace_back(sample);
        return std::chrono::milliseconds {std::clamp(late_ms, int64_t {0}, max_stamp_ms)};
    }

    if (!_exceptionalSince.has_value()) {
        _exceptionalSince = receive_time;
    }

    if (_exceptionalRun.size() >= MAX_LINK_DELAY_SAMPLES) {
        _exceptionalRun.pop_front();
    }

    _exceptionalRun.emplace_back(sample);

    // Late for this long without a break is no stall: the route itself got slower, and its transit is the usual one now
    if (receive_time - _exceptionalSince.value() >= _rebase) {
        _usual = std::move(_exceptionalRun);
        _exceptionalRun.clear();
        _exceptionalSince.reset();
        late_ms = sample.OffsetMs - EvaluateUsualOffset();
    }

    return std::chrono::milliseconds {std::clamp(late_ms, int64_t {0}, max_stamp_ms)};
}

void LinkDelayEstimator::Reset() noexcept
{
    _usual.clear();
    _exceptionalRun.clear();
    _exceptionalSince.reset();
}

auto LinkDelayEstimator::EvaluateUsualOffset() const -> int64_t
{
    FO_VERIFY_AND_THROW(!_usual.empty(), "Usual link transit needs a sample");

    small_vector<int64_t, MAX_LINK_DELAY_SAMPLES> offsets {};

    for (const Sample& usual_sample : _usual) {
        offsets.emplace_back(usual_sample.OffsetMs);
    }

    auto middle {offsets.begin() + numeric_cast<ptrdiff_t>(offsets.size() / 2)};
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
