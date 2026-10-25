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

#include "RpcRuntimeTracker.h"

FO_BEGIN_NAMESPACE

RpcRuntimeTracker::RpcRuntimeTracker(RpcRuntimeLimits limits) :
    _limits {limits}
{
    if (_limits.WarningTimeMs < 0) {
        throw std::invalid_argument("RpcRuntimeTracker warning time must be non-negative");
    }
    if (_limits.DisconnectTimeMs < 0) {
        throw std::invalid_argument("RpcRuntimeTracker disconnect time must be non-negative");
    }
    if (_limits.WarningIntervalMs < 0) {
        throw std::invalid_argument("RpcRuntimeTracker warning interval must be non-negative");
    }
}

auto RpcRuntimeTracker::IsEnabled() const -> bool
{
    return _limits.WarningTimeMs > 0 || _limits.DisconnectTimeMs > 0 || _limits.MaxPendingCalls > 0;
}

auto RpcRuntimeTracker::GetPendingCount() const -> size_t
{
    return _calls.size();
}

auto RpcRuntimeTracker::GetBufferedInputSize() const -> size_t
{
    return _inputBytes;
}

auto RpcRuntimeTracker::GetArrivalSegmentCount() const -> size_t
{
    return _arrivals.size();
}

auto RpcRuntimeTracker::GetInputReadyTime(size_t prefix_bytes) const -> optional<nanotime>
{
    if (prefix_bytes == 0 || prefix_bytes > _inputBytes) {
        return {};
    }

    size_t remaining_bytes = prefix_bytes;

    for (const Arrival& arrival : _arrivals) {
        if (remaining_bytes <= arrival.Bytes) {
            return arrival.ReceivedAt;
        }

        remaining_bytes -= arrival.Bytes;
    }

    return {};
}

auto RpcRuntimeTracker::Begin(string_view name, ident_t caller_id, nanotime now) -> RpcRuntimeStart
{
    if (!IsEnabled()) {
        return {true, {}};
    }

    if (_disconnected) {
        return {false, {}};
    }

    if (_limits.MaxPendingCalls != 0 && _calls.size() >= _limits.MaxPendingCalls) {
        if (!_limitHit) {
            Call rejected;

            if (!name.empty()) {
                rejected.Name.assign(name.data(), name.size());
            }

            rejected.CallerId = caller_id;
            rejected.StartedAt = now;
            _rejectedCall = std::move(rejected);
            _limitHit = true;
        }

        return {false, {}};
    }

    Call call;

    if (!name.empty()) {
        call.Name.assign(name.data(), name.size());
    }

    call.CallerId = caller_id;
    call.StartedAt = now;

    RpcRuntimeCallId id = AllocateCallId();
    _calls.emplace(id, std::move(call));
    ++_startedCalls;

    return {true, id};
}

void RpcRuntimeTracker::Finish(RpcRuntimeCallId id)
{
    auto call = _calls.find(id);

    if (call == _calls.end()) {
        return;
    }

    _calls.erase(call);
    ++_finishedCalls;
}

void RpcRuntimeTracker::AddInput(size_t bytes, nanotime now)
{
    if (bytes == 0) {
        return;
    }

    size_t max_bytes = std::numeric_limits<size_t>::max();

    if (bytes > max_bytes - _inputBytes) {
        throw std::overflow_error("RpcRuntimeTracker input byte count overflow");
    }

    if (!_arrivals.empty() && _arrivals.back().ReceivedAt == now) {
        _arrivals.back().Bytes += bytes;
    }
    else {
        if (_arrivals.size() >= MaxArrivalSegments) {
            size_t oldest_bytes = _arrivals.front().Bytes;
            _arrivals.pop_front();
            _arrivals.front().Bytes += oldest_bytes;
        }

        _arrivals.push_back(Arrival {bytes, now});
    }

    _inputBytes += bytes;
}

void RpcRuntimeTracker::ConsumeInput(size_t bytes)
{
    if (bytes > _inputBytes) {
        throw std::out_of_range("RpcRuntimeTracker consumed input exceeds retained input");
    }

    if (bytes == 0) {
        return;
    }

    size_t remaining_bytes = bytes;

    while (remaining_bytes != 0) {
        Arrival& arrival = _arrivals.front();

        if (remaining_bytes < arrival.Bytes) {
            arrival.Bytes -= remaining_bytes;
            remaining_bytes = 0;
        }
        else {
            remaining_bytes -= arrival.Bytes;
            _arrivals.pop_front();
        }
    }

    _inputBytes -= bytes;
}

void RpcRuntimeTracker::ResetInput()
{
    _arrivals.clear();
    _inputBytes = 0;
    _queuedReadyAt.reset();
    _bufferedBytes = 0;
    _queuedCallHash = 0;
}

void RpcRuntimeTracker::SetQueuedMessage(optional<nanotime> ready_at, size_t bytes, uint64_t call_hash)
{
    _queuedReadyAt = ready_at;
    _bufferedBytes = bytes;
    _queuedCallHash = call_hash;
}

auto RpcRuntimeTracker::Evaluate(nanotime now) -> RpcRuntimeReport
{
    RpcRuntimeReport report;
    report.PendingCalls = _calls.size();
    report.BufferedBytes = _bufferedBytes;
    report.StartedCalls = _startedCalls;
    report.FinishedCalls = _finishedCalls;

    if (_bufferedBytes != 0) {
        report.QueuedCallHash = _queuedCallHash;
    }

    if (_disconnected) {
        return report;
    }

    if (_limitHit) {
        report.Action = RpcRuntimeAction::Disconnect;
        report.Cause = RpcRuntimeCause::PendingLimit;
        report.CallName = _rejectedCall.Name;
        report.CallerId = _rejectedCall.CallerId;
        report.Age = now - _rejectedCall.StartedAt;
        _disconnected = true;

        return report;
    }

    optional<CallTarget> oldest_call = FindOldestCall();
    bool queued_message_ready = IsQueuedMessageReady();
    bool target_is_queue = false;
    nanotime target_time {};

    if (oldest_call.has_value()) {
        target_time = oldest_call->StartedAt;
    }

    if (queued_message_ready && (!oldest_call.has_value() || *_queuedReadyAt < target_time)) {
        target_is_queue = true;
        target_time = *_queuedReadyAt;
    }

    bool has_target = oldest_call.has_value() || queued_message_ready;

    if (has_target && _limits.DisconnectTimeMs > 0) {
        timespan disconnect_age {std::chrono::milliseconds {_limits.DisconnectTimeMs}};

        if (now - target_time >= disconnect_age) {
            report.Action = RpcRuntimeAction::Disconnect;

            if (target_is_queue) {
                FillInputQueueReport(report, now);
            }
            else {
                FillPendingCallReport(report, oldest_call->Id, now);
            }

            _disconnected = true;

            return report;
        }
    }

    bool warning_violation = false;

    if (has_target && _limits.WarningTimeMs > 0) {
        timespan warning_age {std::chrono::milliseconds {_limits.WarningTimeMs}};
        warning_violation = now - target_time >= warning_age;
    }

    if (!warning_violation) {
        _lastWarningAt.reset();

        return report;
    }

    bool emit_warning = !_lastWarningAt.has_value();

    if (!emit_warning && _limits.WarningIntervalMs > 0) {
        timespan warning_interval {std::chrono::milliseconds {_limits.WarningIntervalMs}};
        emit_warning = now - *_lastWarningAt >= warning_interval;
    }

    if (!emit_warning) {
        return report;
    }

    report.Action = RpcRuntimeAction::Warning;

    if (target_is_queue) {
        FillInputQueueReport(report, now);
    }
    else {
        FillPendingCallReport(report, oldest_call->Id, now);
    }

    _lastWarningAt = now;

    return report;
}

auto RpcRuntimeTracker::IsQueuedMessageReady() const -> bool
{
    return _queuedReadyAt.has_value() && _bufferedBytes != 0;
}

auto RpcRuntimeTracker::FindOldestCall() const -> optional<CallTarget>
{
    optional<CallTarget> oldest_call;

    for (const auto& call_entry : _calls) {
        const Call& call = call_entry.second;

        if (!oldest_call.has_value() || call.StartedAt < oldest_call->StartedAt || (call.StartedAt == oldest_call->StartedAt && call_entry.first < oldest_call->Id)) {
            oldest_call = CallTarget {call_entry.first, call.StartedAt};
        }
    }

    return oldest_call;
}

auto RpcRuntimeTracker::AllocateCallId() -> RpcRuntimeCallId
{
    uint64_t id = _lastId.underlying_value();

    do {
        ++id;

        if (id == 0) {
            ++id;
        }
    } while (_calls.find(RpcRuntimeCallId {id}) != _calls.end());

    _lastId = RpcRuntimeCallId {id};

    return _lastId;
}

void RpcRuntimeTracker::FillPendingCallReport(RpcRuntimeReport& report, RpcRuntimeCallId id, nanotime now) const
{
    auto call = _calls.find(id);

    if (call == _calls.end()) {
        return;
    }

    report.Cause = RpcRuntimeCause::PendingCall;
    report.CallName = call->second.Name;
    report.CallerId = call->second.CallerId;
    report.Id = call->first;
    report.Age = now - call->second.StartedAt;
}

void RpcRuntimeTracker::FillInputQueueReport(RpcRuntimeReport& report, nanotime now) const
{
    if (!_queuedReadyAt.has_value()) {
        return;
    }

    report.Cause = RpcRuntimeCause::InputQueue;
    report.Age = now - *_queuedReadyAt;
    report.QueuedCallHash = _queuedCallHash;
}

FO_END_NAMESPACE

