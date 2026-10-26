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

#include "Common.h"

#include "catch_amalgamated.hpp"
#include "RpcRuntimeTracker.h"

FO_BEGIN_NAMESPACE

static_assert(some_strong_type<RpcRuntimeCallId>);
static_assert(!std::is_convertible_v<uint64_t, RpcRuntimeCallId>);
static_assert(!std::is_convertible_v<RpcRuntimeCallId, uint64_t>);
static_assert(!std::is_convertible_v<ident_t, RpcRuntimeCallId>);
static_assert(!std::is_invocable_v<decltype(&RpcRuntimeTracker::Finish), RpcRuntimeTracker*, ident_t>);
static_assert(std::same_as<decltype(RpcRuntimeStart::Id), RpcRuntimeCallId>);
static_assert(std::same_as<decltype(RpcRuntimeReport::Id), RpcRuntimeCallId>);
static_assert(std::same_as<decltype(RpcRuntimeReport::CallerId), ident_t>);
static_assert(std::same_as<decltype(&RpcRuntimeTracker::Begin), RpcRuntimeStart (RpcRuntimeTracker::*)(string_view, ident_t, nanotime)>);

static auto RpcAt(int64_t milliseconds) -> nanotime
{
    return nanotime {timespan {std::chrono::milliseconds {milliseconds}}};
}

TEST_CASE("RpcRuntimeTracker", "[rpc-runtime]")
{
    SECTION("An unfinished call warns at the inclusive deadline and disconnects without completion")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 256}};
        RpcRuntimeStart started = tracker.Begin("RequestCounterSurveillanceObservation", ident_t {17}, RpcAt(0));
        REQUIRE(started.Accepted);
        REQUIRE(started.Id != RpcRuntimeCallId {});
        CHECK(tracker.Evaluate(RpcAt(999)).Action == RpcRuntimeAction::None);
        RpcRuntimeReport warning = tracker.Evaluate(RpcAt(1000));
        CHECK(warning.Action == RpcRuntimeAction::Warning);
        CHECK(warning.Cause == RpcRuntimeCause::PendingCall);
        CHECK(warning.CallerId == ident_t {17});
        CHECK(warning.Id == started.Id);
        CHECK(warning.PendingCalls == 1);
        CHECK(warning.StartedCalls == 1);
        CHECK(warning.FinishedCalls == 0);
        CHECK(warning.CallName == "RequestCounterSurveillanceObservation");
        CHECK(warning.Age == timespan {std::chrono::milliseconds {1000}});
        CHECK(tracker.Evaluate(RpcAt(29999)).Action != RpcRuntimeAction::Disconnect);
        CHECK(tracker.Evaluate(RpcAt(30000)).Action == RpcRuntimeAction::Disconnect);
        CHECK(tracker.Evaluate(RpcAt(30001)).Action == RpcRuntimeAction::None);
    }
}

TEST_CASE("RpcRuntimeWarningAndDisconnect", "[rpc-runtime][C1][C2]")
{
    SECTION("Warning cooldown never delays the inclusive disconnect")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 256}};
        RpcRuntimeStart started = tracker.Begin("Watch", ident_t {7}, RpcAt(0));
        REQUIRE(started.Accepted);

        CHECK(tracker.Evaluate(RpcAt(999)).Action == RpcRuntimeAction::None);
        CHECK(tracker.Evaluate(RpcAt(1000)).Action == RpcRuntimeAction::Warning);
        CHECK(tracker.Evaluate(RpcAt(1001)).Action == RpcRuntimeAction::None);
        CHECK(tracker.Evaluate(RpcAt(11000)).Action == RpcRuntimeAction::Warning);

        RpcRuntimeReport before_disconnect = tracker.Evaluate(RpcAt(29999));
        CHECK(before_disconnect.Action != RpcRuntimeAction::Disconnect);

        RpcRuntimeReport disconnected = tracker.Evaluate(RpcAt(30000));
        CHECK(disconnected.Action == RpcRuntimeAction::Disconnect);
        CHECK(disconnected.Cause == RpcRuntimeCause::PendingCall);
        CHECK(disconnected.CallerId == ident_t {7});
        CHECK(disconnected.Age == timespan {std::chrono::milliseconds {30000}});
        CHECK(tracker.Evaluate(RpcAt(30001)).Action == RpcRuntimeAction::None);
    }

    SECTION("Warning-only mode warns once per violating episode")
    {
        RpcRuntimeTracker tracker {{1000, 0, 0, 0}};
        CHECK(tracker.IsEnabled());

        RpcRuntimeStart first = tracker.Begin("WarningOnly", ident_t {11}, RpcAt(0));
        REQUIRE(first.Accepted);
        CHECK(tracker.Evaluate(RpcAt(1000)).Action == RpcRuntimeAction::Warning);
        CHECK(tracker.Evaluate(RpcAt(1001)).Action == RpcRuntimeAction::None);

        tracker.Finish(first.Id);
        CHECK(tracker.Evaluate(RpcAt(2000)).Action == RpcRuntimeAction::None);

        RpcRuntimeStart second = tracker.Begin("WarningOnly", ident_t {11}, RpcAt(3000));
        REQUIRE(second.Accepted);
        CHECK(tracker.Evaluate(RpcAt(4000)).Action == RpcRuntimeAction::Warning);
    }

    SECTION("A quiet poll resets a positive warning interval")
    {
        RpcRuntimeTracker tracker {{1000, 0, 10000, 0}};
        CHECK(tracker.IsEnabled());

        RpcRuntimeStart first = tracker.Begin("Interval", ident_t {12}, RpcAt(0));
        REQUIRE(first.Accepted);
        CHECK(tracker.Evaluate(RpcAt(1000)).Action == RpcRuntimeAction::Warning);
        CHECK(tracker.Evaluate(RpcAt(1001)).Action == RpcRuntimeAction::None);

        tracker.Finish(first.Id);
        CHECK(tracker.Evaluate(RpcAt(2000)).Action == RpcRuntimeAction::None);

        RpcRuntimeStart second = tracker.Begin("Interval", ident_t {12}, RpcAt(3000));
        REQUIRE(second.Accepted);
        CHECK(tracker.Evaluate(RpcAt(4000)).Action == RpcRuntimeAction::Warning);
    }

    SECTION("Disconnect-only mode has no warning reaction")
    {
        RpcRuntimeTracker tracker {{0, 30000, 10000, 0}};
        CHECK(tracker.IsEnabled());

        RpcRuntimeStart started = tracker.Begin("DisconnectOnly", ident_t {13}, RpcAt(0));
        REQUIRE(started.Accepted);
        CHECK(tracker.Evaluate(RpcAt(29999)).Action == RpcRuntimeAction::None);
        CHECK(tracker.Evaluate(RpcAt(30000)).Action == RpcRuntimeAction::Disconnect);
        CHECK(tracker.Evaluate(RpcAt(30001)).Action == RpcRuntimeAction::None);

        RpcRuntimeStart after_disconnect = tracker.Begin("After", ident_t {14}, RpcAt(30001));
        CHECK_FALSE(after_disconnect.Accepted);
        CHECK(after_disconnect.Id == RpcRuntimeCallId {});
    }
}

TEST_CASE("RpcRuntimeCallAccounting", "[rpc-runtime][C3]")
{
    SECTION("Finishing newer duplicate and unknown IDs leaves the older call active")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 256}};
        RpcRuntimeStart older = tracker.Begin("RemoteCall", ident_t {11}, RpcAt(0));
        RpcRuntimeStart newer = tracker.Begin("RemoteCall", ident_t {22}, RpcAt(500));
        REQUIRE(older.Accepted);
        REQUIRE(newer.Accepted);
        CHECK(older.Id != RpcRuntimeCallId {});
        CHECK(newer.Id != RpcRuntimeCallId {});
        CHECK(older.Id != newer.Id);

        RpcRuntimeCallId unknown_id {newer.Id.underlying_value() + 1000};
        tracker.Finish(newer.Id);
        tracker.Finish(newer.Id);
        tracker.Finish(unknown_id);

        RpcRuntimeReport warning = tracker.Evaluate(RpcAt(1000));
        CHECK(warning.Action == RpcRuntimeAction::Warning);
        CHECK(warning.Cause == RpcRuntimeCause::PendingCall);
        CHECK(warning.CallName == "RemoteCall");
        CHECK(warning.CallerId == ident_t {11});
        CHECK(warning.Id == older.Id);
        CHECK(warning.Age == timespan {std::chrono::milliseconds {1000}});
        CHECK(warning.PendingCalls == 1);
        CHECK(warning.StartedCalls == 2);
        CHECK(warning.FinishedCalls == 1);

        tracker.Finish(older.Id);
        RpcRuntimeReport quiet = tracker.Evaluate(RpcAt(2000));
        CHECK(quiet.Action == RpcRuntimeAction::None);
        CHECK(quiet.PendingCalls == 0);
        CHECK(quiet.StartedCalls == 2);
        CHECK(quiet.FinishedCalls == 2);

        RpcRuntimeStart next = tracker.Begin("Next", ident_t {33}, RpcAt(2000));
        REQUIRE(next.Accepted);
        CHECK(next.Id != RpcRuntimeCallId {});
        CHECK(next.Id != older.Id);
        CHECK(next.Id != newer.Id);
    }

    SECTION("A separate tracker keeps its own call state")
    {
        RpcRuntimeTracker first {{1000, 30000, 10000, 256}};
        RpcRuntimeTracker second {{1000, 30000, 10000, 256}};
        RpcRuntimeStart first_call = first.Begin("First", ident_t {41}, RpcAt(0));
        RpcRuntimeStart second_call = second.Begin("Second", ident_t {42}, RpcAt(0));
        REQUIRE(first_call.Accepted);
        REQUIRE(second_call.Accepted);

        first.Finish(first_call.Id);

        RpcRuntimeReport report = second.Evaluate(RpcAt(1000));
        CHECK(report.Action == RpcRuntimeAction::Warning);
        CHECK(report.CallerId == ident_t {42});
        CHECK(report.PendingCalls == 1);
        CHECK(report.StartedCalls == 1);
        CHECK(report.FinishedCalls == 0);
    }
}

TEST_CASE("RpcRuntimePendingLimit", "[rpc-runtime][C4]")
{
    SECTION("A positive cap rejects without growth and latches one pending-limit disconnect")
    {
        RpcRuntimeTracker tracker {{0, 0, 0, 2}};
        CHECK(tracker.IsEnabled());

        RpcRuntimeStart first = tracker.Begin("First", ident_t {1}, RpcAt(0));
        RpcRuntimeStart second = tracker.Begin("Second", ident_t {2}, RpcAt(0));
        REQUIRE(first.Accepted);
        REQUIRE(second.Accepted);
        CHECK(tracker.GetPendingCount() == 2);

        RpcRuntimeStart refused = tracker.Begin("Refused", ident_t {3}, RpcAt(1));
        CHECK_FALSE(refused.Accepted);
        CHECK(refused.Id == RpcRuntimeCallId {});
        CHECK(tracker.GetPendingCount() == 2);

        RpcRuntimeReport limit_report = tracker.Evaluate(RpcAt(1));
        CHECK(limit_report.Action == RpcRuntimeAction::Disconnect);
        CHECK(limit_report.Cause == RpcRuntimeCause::PendingLimit);
        CHECK(limit_report.CallName == "Refused");
        CHECK(limit_report.CallerId == ident_t {3});
        CHECK(limit_report.PendingCalls == 2);
        CHECK(limit_report.StartedCalls == 2);
        CHECK(limit_report.FinishedCalls == 0);

        CHECK(tracker.Evaluate(RpcAt(2)).Action == RpcRuntimeAction::None);

        RpcRuntimeStart after_disconnect = tracker.Begin("After", ident_t {4}, RpcAt(2));
        CHECK_FALSE(after_disconnect.Accepted);
        CHECK(after_disconnect.Id == RpcRuntimeCallId {});
    }

    SECTION("All disabled limits disable tracking")
    {
        RpcRuntimeTracker tracker {{0, 0, 0, 0}};
        RpcRuntimeTracker interval_only {{0, 0, 10000, 0}};
        CHECK_FALSE(tracker.IsEnabled());
        CHECK_FALSE(interval_only.IsEnabled());

        RpcRuntimeStart ignored = tracker.Begin("Ignored", ident_t {9}, RpcAt(0));
        CHECK(ignored.Accepted);
        CHECK(ignored.Id == RpcRuntimeCallId {});
        CHECK(tracker.GetPendingCount() == 0);

        RpcRuntimeReport report = tracker.Evaluate(RpcAt(0));
        CHECK(report.Action == RpcRuntimeAction::None);
        CHECK(report.Cause == RpcRuntimeCause::None);
        CHECK(report.PendingCalls == 0);
        CHECK(report.BufferedBytes == 0);
        CHECK(report.FinishedCalls == 0);
    }
}

TEST_CASE("RpcRuntimeInputReadiness", "[rpc-runtime][C5]")
{
    SECTION("Prefix readiness uses the arrival of the requested last byte")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 256}};
        tracker.AddInput(5, RpcAt(0));
        tracker.AddInput(7, RpcAt(200));

        CHECK(tracker.GetBufferedInputSize() == 12);
        CHECK(tracker.GetArrivalSegmentCount() == 2);
        CHECK_FALSE(tracker.GetInputReadyTime(0).has_value());
        REQUIRE(tracker.GetInputReadyTime(5).has_value());
        CHECK(*tracker.GetInputReadyTime(5) == RpcAt(0));
        REQUIRE(tracker.GetInputReadyTime(6).has_value());
        CHECK(*tracker.GetInputReadyTime(6) == RpcAt(200));
        REQUIRE(tracker.GetInputReadyTime(12).has_value());
        CHECK(*tracker.GetInputReadyTime(12) == RpcAt(200));
        CHECK_FALSE(tracker.GetInputReadyTime(13).has_value());

        tracker.ConsumeInput(5);
        CHECK(tracker.GetBufferedInputSize() == 7);
        CHECK(tracker.GetArrivalSegmentCount() == 1);
        REQUIRE(tracker.GetInputReadyTime(1).has_value());
        CHECK(*tracker.GetInputReadyTime(1) == RpcAt(200));
        REQUIRE(tracker.GetInputReadyTime(7).has_value());
        CHECK(*tracker.GetInputReadyTime(7) == RpcAt(200));
        CHECK_FALSE(tracker.GetInputReadyTime(8).has_value());

        CHECK_THROWS(tracker.ConsumeInput(8));
        CHECK(tracker.GetBufferedInputSize() == 7);
        CHECK(tracker.GetArrivalSegmentCount() == 1);
        REQUIRE(tracker.GetInputReadyTime(1).has_value());
        CHECK(*tracker.GetInputReadyTime(1) == RpcAt(200));
        CHECK_FALSE(tracker.GetInputReadyTime(8).has_value());

        CHECK_NOTHROW(tracker.AddInput(0, RpcAt(999)));
        CHECK_NOTHROW(tracker.ConsumeInput(0));
        CHECK(tracker.GetBufferedInputSize() == 7);
        CHECK(tracker.GetArrivalSegmentCount() == 1);
    }

    SECTION("Adjacent arrivals at the same timestamp are coalesced")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 0}};
        tracker.AddInput(5, RpcAt(0));
        tracker.AddInput(7, RpcAt(0));

        CHECK(tracker.GetBufferedInputSize() == 12);
        CHECK(tracker.GetArrivalSegmentCount() == 1);
        REQUIRE(tracker.GetInputReadyTime(12).has_value());
        CHECK(*tracker.GetInputReadyTime(12) == RpcAt(0));

        tracker.AddInput(3, RpcAt(200));
        CHECK(tracker.GetBufferedInputSize() == 15);
        CHECK(tracker.GetArrivalSegmentCount() == 2);
        REQUIRE(tracker.GetInputReadyTime(12).has_value());
        CHECK(*tracker.GetInputReadyTime(12) == RpcAt(0));
        REQUIRE(tracker.GetInputReadyTime(13).has_value());
        CHECK(*tracker.GetInputReadyTime(13) == RpcAt(200));
        REQUIRE(tracker.GetInputReadyTime(15).has_value());
        CHECK(*tracker.GetInputReadyTime(15) == RpcAt(200));
    }

    SECTION("An unready queue has no deadline while a zero timestamp does")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 0}};
        tracker.SetQueuedMessage({}, 7, 23);

        RpcRuntimeReport unready = tracker.Evaluate(RpcAt(50000));
        CHECK(unready.Action == RpcRuntimeAction::None);
        CHECK(unready.BufferedBytes == 7);
        CHECK(unready.PendingCalls == 0);

        tracker.SetQueuedMessage(RpcAt(0), 7, 23);
        RpcRuntimeReport ready = tracker.Evaluate(RpcAt(1000));
        CHECK(ready.Action == RpcRuntimeAction::Warning);
        CHECK(ready.Cause == RpcRuntimeCause::InputQueue);
        CHECK(ready.Age == timespan {std::chrono::milliseconds {1000}});
        CHECK(ready.QueuedCallHash == 23);
        CHECK(ready.PendingCalls == 0);
        CHECK(ready.StartedCalls == 0);
        CHECK(ready.FinishedCalls == 0);
    }

    SECTION("The full queued hash survives the report")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 0}};
        uint64_t queued_hash = 0x100000017ULL;
        tracker.SetQueuedMessage(RpcAt(0), 8, queued_hash);

        RpcRuntimeReport report = tracker.Evaluate(RpcAt(1000));
        CHECK(report.Action == RpcRuntimeAction::Warning);
        CHECK(report.Cause == RpcRuntimeCause::InputQueue);
        CHECK(report.QueuedCallHash == 0x100000017ULL);
    }

    SECTION("Arrival segments stay bounded without reporting an early completion time")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 0}};
        for (int64_t milliseconds = 0; milliseconds < 4097; ++milliseconds) {
            tracker.AddInput(1, RpcAt(milliseconds));
        }

        CHECK(tracker.GetBufferedInputSize() == 4097);
        CHECK(tracker.GetArrivalSegmentCount() <= 4096);
        CHECK(tracker.GetArrivalSegmentCount() > 0);

        for (int64_t prefix_milliseconds = 0; prefix_milliseconds < 4097; ++prefix_milliseconds) {
            optional<nanotime> ready = tracker.GetInputReadyTime(numeric_cast<size_t>(prefix_milliseconds + 1));
            REQUIRE(ready.has_value());
            CHECK(*ready >= RpcAt(prefix_milliseconds));
        }

        REQUIRE(tracker.GetInputReadyTime(4097).has_value());
        CHECK(*tracker.GetInputReadyTime(4097) == RpcAt(4096));

        tracker.ConsumeInput(4096);
        CHECK(tracker.GetBufferedInputSize() == 1);
        CHECK(tracker.GetArrivalSegmentCount() == 1);
        REQUIRE(tracker.GetInputReadyTime(1).has_value());
        CHECK(*tracker.GetInputReadyTime(1) == RpcAt(4096));
        CHECK_FALSE(tracker.GetInputReadyTime(2).has_value());
    }

    SECTION("ResetInput clears byte and queue history but keeps pending calls")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 256}};
        RpcRuntimeStart held = tracker.Begin("Held", ident_t {55}, RpcAt(0));
        REQUIRE(held.Accepted);
        tracker.AddInput(4, RpcAt(50));
        tracker.SetQueuedMessage(RpcAt(0), 7, 77);

        tracker.ResetInput();
        CHECK(tracker.GetBufferedInputSize() == 0);
        CHECK(tracker.GetArrivalSegmentCount() == 0);
        CHECK_FALSE(tracker.GetInputReadyTime(0).has_value());
        CHECK_FALSE(tracker.GetInputReadyTime(1).has_value());
        CHECK(tracker.GetPendingCount() == 1);

        RpcRuntimeReport report = tracker.Evaluate(RpcAt(1000));
        CHECK(report.Action == RpcRuntimeAction::Warning);
        CHECK(report.Cause == RpcRuntimeCause::PendingCall);
        CHECK(report.CallerId == ident_t {55});
        CHECK(report.PendingCalls == 1);
        CHECK(report.BufferedBytes == 0);
        CHECK(report.QueuedCallHash == 0);
    }
}

TEST_CASE("RpcRuntimeQueueArbitration", "[rpc-runtime][C6]")
{
    SECTION("An older ready queue wins over a fresh pending call")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 256}};
        uint64_t queued_hash = 0x100000017ULL;
        tracker.SetQueuedMessage(RpcAt(0), 5, queued_hash);
        RpcRuntimeStart call = tracker.Begin("FreshCall", ident_t {77}, RpcAt(1000));
        REQUIRE(call.Accepted);

        RpcRuntimeReport report = tracker.Evaluate(RpcAt(1000));
        CHECK(report.Action == RpcRuntimeAction::Warning);
        CHECK(report.Cause == RpcRuntimeCause::InputQueue);
        CHECK(report.Age == timespan {std::chrono::milliseconds {1000}});
        CHECK(report.QueuedCallHash == queued_hash);
        CHECK(report.Id == RpcRuntimeCallId {});
        CHECK(report.CallName.empty());
        CHECK(report.CallerId == ident_t {});
        CHECK(report.PendingCalls == 1);
        CHECK(report.BufferedBytes == 5);
        CHECK(report.StartedCalls == 1);
        CHECK(report.FinishedCalls == 0);

        tracker.SetQueuedMessage({}, 0, 0);
        RpcRuntimeReport after_clear = tracker.Evaluate(RpcAt(1001));
        CHECK(after_clear.Action == RpcRuntimeAction::None);
        CHECK(after_clear.Cause == RpcRuntimeCause::None);
        CHECK(after_clear.PendingCalls == 1);
        CHECK(after_clear.BufferedBytes == 0);
    }

    SECTION("A completed call has no active age deadline")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 256}};
        RpcRuntimeStart call = tracker.Begin("Completed", ident_t {88}, RpcAt(0));
        REQUIRE(call.Accepted);
        tracker.Finish(call.Id);

        RpcRuntimeReport report = tracker.Evaluate(RpcAt(30000));
        CHECK(report.Action == RpcRuntimeAction::None);
        CHECK(report.PendingCalls == 0);
        CHECK(report.FinishedCalls == 1);
    }

    SECTION("An unready queue alone has no age deadline")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 0}};
        tracker.SetQueuedMessage({}, 7, 23);

        RpcRuntimeReport report = tracker.Evaluate(RpcAt(50000));
        CHECK(report.Action == RpcRuntimeAction::None);
        CHECK(report.Cause == RpcRuntimeCause::None);
        CHECK(report.BufferedBytes == 7);
    }
}

TEST_CASE("RpcRuntimeValidation", "[rpc-runtime][C7]")
{
    SECTION("Negative limits are rejected at construction")
    {
        CHECK_THROWS((RpcRuntimeTracker {RpcRuntimeLimits {-1, 30000, 10000, 256}}));
        CHECK_THROWS((RpcRuntimeTracker {RpcRuntimeLimits {1000, -1, 10000, 256}}));
        CHECK_THROWS((RpcRuntimeTracker {RpcRuntimeLimits {1000, 30000, -1, 256}}));
    }

    SECTION("Input byte count overflow is rejected before mutation")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 0}};
        size_t max_bytes = std::numeric_limits<size_t>::max();

        tracker.AddInput(max_bytes, RpcAt(0));
        CHECK(tracker.GetBufferedInputSize() == max_bytes);
        CHECK(tracker.GetArrivalSegmentCount() == 1);

        CHECK_THROWS(tracker.AddInput(1, RpcAt(1)));
        CHECK(tracker.GetBufferedInputSize() == max_bytes);
        CHECK(tracker.GetArrivalSegmentCount() == 1);
        REQUIRE(tracker.GetInputReadyTime(max_bytes).has_value());
        CHECK(*tracker.GetInputReadyTime(max_bytes) == RpcAt(0));
    }

    SECTION("Invalid consumption leaves retained input and calls unchanged")
    {
        RpcRuntimeTracker tracker {{1000, 30000, 10000, 256}};
        RpcRuntimeStart held = tracker.Begin("Held", ident_t {7}, RpcAt(0));
        REQUIRE(held.Accepted);
        tracker.AddInput(5, RpcAt(0));
        tracker.AddInput(7, RpcAt(200));
        tracker.ConsumeInput(5);

        CHECK(tracker.GetBufferedInputSize() == 7);
        CHECK(tracker.GetArrivalSegmentCount() == 1);
        CHECK_THROWS(tracker.ConsumeInput(8));
        CHECK(tracker.GetBufferedInputSize() == 7);
        CHECK(tracker.GetArrivalSegmentCount() == 1);
        REQUIRE(tracker.GetInputReadyTime(1).has_value());
        CHECK(*tracker.GetInputReadyTime(1) == RpcAt(200));
        REQUIRE(tracker.GetInputReadyTime(7).has_value());
        CHECK(*tracker.GetInputReadyTime(7) == RpcAt(200));
        CHECK_FALSE(tracker.GetInputReadyTime(8).has_value());
        CHECK(tracker.GetPendingCount() == 1);

        CHECK_NOTHROW(tracker.ConsumeInput(0));
        CHECK_NOTHROW(tracker.AddInput(0, RpcAt(999)));

        RpcRuntimeReport warning = tracker.Evaluate(RpcAt(1000));
        CHECK(warning.Action == RpcRuntimeAction::Warning);
        CHECK(warning.Cause == RpcRuntimeCause::PendingCall);
        CHECK(warning.CallerId == ident_t {7});
        CHECK(warning.PendingCalls == 1);
        CHECK(warning.StartedCalls == 1);
        CHECK(warning.FinishedCalls == 0);
        CHECK(warning.BufferedBytes == 0);
    }
}

FO_END_NAMESPACE
