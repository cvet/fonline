// FOnline Engine
// SPDX-License-Identifier: MIT

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
        nanotime ReceivedAt {};
        int64_t OffsetMs {};
    };

    auto EvaluateUsualOffset() const -> int64_t;

    timespan _window {};
    timespan _exceptional {};
    timespan _rebase {};
    // A stall's burst stays out of what the link usually takes, or it would hide how late its own later messages were
    deque<Sample> _usual {};
    deque<Sample> _exceptionalRun {};
    optional<nanotime> _exceptionalSince {};
};

// How far ahead a late movement plan is played: not at all below the lateness an ordinary jittery link produces, the
// lateness itself above it, never more than the upper bound
auto EvaluateLateCatchUp(timespan late_time, timespan min_time, timespan max_time) noexcept -> timespan;

FO_END_NAMESPACE
