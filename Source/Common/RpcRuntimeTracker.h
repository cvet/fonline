// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

enum class RpcRuntimeAction : uint8_t
{
    None,
    Warning,
    Disconnect,
};

enum class RpcRuntimeCause : uint8_t
{
    None,
    PendingCall,
    InputQueue,
    PendingLimit,
};

// Opaque RPC invocation identity; the default value marks a start that did not allocate a call
using RpcRuntimeCallId = strong_type<uint64_t, struct RpcRuntimeCallId_, strong_type_bool_test_tag, strong_type_sortings_tag>;

struct RpcRuntimeLimits
{
    int32_t WarningTimeMs {1000};
    int32_t DisconnectTimeMs {30000};
    int32_t WarningIntervalMs {10000};
    size_t MaxPendingCalls {256};
};

struct RpcRuntimeStart
{
    bool Accepted {true};
    RpcRuntimeCallId Id {};
};

struct RpcRuntimeReport
{
    RpcRuntimeAction Action {};
    RpcRuntimeCause Cause {};
    string CallName {};
    ident_t CallerId {};
    RpcRuntimeCallId Id {};
    timespan Age {};
    size_t PendingCalls {};
    size_t BufferedBytes {};
    uint64_t QueuedCallHash {};
    uint64_t StartedCalls {};
    uint64_t FinishedCalls {};
};

// The owning connection serializes updates and watchdog evaluation with its own mutex
class RpcRuntimeTracker final
{
public:
    explicit RpcRuntimeTracker(RpcRuntimeLimits limits);

    [[nodiscard]] auto IsEnabled() const -> bool;
    [[nodiscard]] auto GetPendingCount() const -> size_t;
    [[nodiscard]] auto GetBufferedInputSize() const -> size_t;
    [[nodiscard]] auto GetArrivalSegmentCount() const -> size_t;
    // The last byte completes a frame; receiving an earlier fragment did not make it queued work
    [[nodiscard]] auto GetInputReadyTime(size_t prefix_bytes) const -> optional<nanotime>;

    auto Begin(string_view name, ident_t caller_id, nanotime now) -> RpcRuntimeStart;
    void Finish(RpcRuntimeCallId id);
    void AddInput(size_t bytes, nanotime now);
    void ConsumeInput(size_t bytes);
    void ResetInput();
    void SetQueuedMessage(optional<nanotime> ready_at, size_t bytes, uint64_t call_hash = 0);
    auto Evaluate(nanotime now) -> RpcRuntimeReport;

private:
    struct Call
    {
        string Name {};
        ident_t CallerId {};
        nanotime StartedAt {};
    };

    struct Arrival
    {
        size_t Bytes {};
        nanotime ReceivedAt {};
    };

    struct CallTarget
    {
        RpcRuntimeCallId Id {};
        nanotime StartedAt {};
    };

    static constexpr size_t MaxArrivalSegments {4096};

    [[nodiscard]] auto IsQueuedMessageReady() const -> bool;
    [[nodiscard]] auto FindOldestCall() const -> optional<CallTarget>;
    auto AllocateCallId() -> RpcRuntimeCallId;
    void FillPendingCallReport(RpcRuntimeReport& report, RpcRuntimeCallId id, nanotime now) const;
    void FillInputQueueReport(RpcRuntimeReport& report, nanotime now) const;

    RpcRuntimeLimits _limits {};
    unordered_map<RpcRuntimeCallId, Call> _calls {};
    deque<Arrival> _arrivals {};
    size_t _inputBytes {};
    optional<nanotime> _queuedReadyAt {};
    size_t _bufferedBytes {};
    uint64_t _queuedCallHash {};
    RpcRuntimeCallId _lastId {};
    uint64_t _startedCalls {};
    uint64_t _finishedCalls {};
    optional<nanotime> _lastWarningAt {};
    bool _limitHit {};
    bool _disconnected {};
    Call _rejectedCall {};
};

FO_END_NAMESPACE
