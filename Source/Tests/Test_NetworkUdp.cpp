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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "catch_amalgamated.hpp"

#include "Common.h"
#include "NetworkUdp.h"

FO_BEGIN_NAMESPACE

namespace
{
    auto MakeOptions(size_t max_payload = 64, size_t max_pending = 1024, uint32_t resend_ms = 100, uint32_t connect_retry_ms = 200, uint32_t redundancy = 0) -> UdpTransportOptions
    {
        UdpTransportOptions options;
        options.MaxPayload = max_payload;
        options.MaxPendingBytes = max_pending;
        options.ResendTimeoutMs = resend_ms;
        options.ConnectRetryMs = connect_retry_ms;
        options.Redundancy = redundancy;
        return options;
    }

    auto Bytes(string_view s) -> vector<uint8_t>
    {
        return vector<uint8_t>(reinterpret_cast<const uint8_t*>(s.data()), reinterpret_cast<const uint8_t*>(s.data()) + s.size());
    }

    auto BytesEq(const vector<uint8_t>& data, string_view s) -> bool
    {
        return data.size() == s.size() && std::equal(data.begin(), data.end(), reinterpret_cast<const uint8_t*>(s.data()));
    }

    auto BytesTail(const vector<uint8_t>& data, size_t offset) -> const_span<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(offset <= data.size(), "Tail offset past end of buffer");

        const size_t size = data.size() - offset;

        if (size == 0) {
            return {};
        }

        ptr<const uint8_t> tail_ptr = &data[offset];
        return {tail_ptr.get(), size};
    }

    auto FeedAll(const vector<vector<uint8_t>>& wire, UdpOrderedChannel& receiver) -> int32_t
    {
        int32_t parsed = 0;

        for (const auto& packet : wire) {
            UdpPacketInfo info;

            if (TryParseUdpPacket(packet, info)) {
                receiver.HandleIncomingPayload(info);
                parsed++;
            }
        }

        return parsed;
    }

    auto BumpTime(nanotime base, int64_t ms) -> nanotime
    {
        return base + std::chrono::milliseconds {ms};
    }
}

TEST_CASE("NetworkUdp::Packets")
{
    SECTION("ConnectAndAcceptRoundTrip")
    {
        const auto connect = MakeUdpConnectPacket(0xDEADBEEFU);
        UdpPacketInfo connect_info;
        REQUIRE(TryParseUdpPacket(connect, connect_info));
        CHECK(connect_info.Type == UdpPacketType::Connect);
        CHECK(connect_info.SessionId == 0U);
        CHECK(connect_info.Value == 0xDEADBEEFU);
        CHECK(connect_info.Payload.empty());

        const auto accept = MakeUdpAcceptPacket(0xCAFEBABEU, 0xDEADBEEFU);
        UdpPacketInfo accept_info;
        REQUIRE(TryParseUdpPacket(accept, accept_info));
        CHECK(accept_info.Type == UdpPacketType::Accept);
        CHECK(accept_info.SessionId == 0xCAFEBABEU);
        CHECK(accept_info.Value == 0xDEADBEEFU);
        CHECK(accept_info.Payload.empty());
    }

    SECTION("RejectsMalformedPackets")
    {
        UdpPacketInfo info;

        // Empty
        CHECK_FALSE(TryParseUdpPacket({}, info));

        // Truncated header
        const vector<uint8_t> tiny(4, 0xFF);
        CHECK_FALSE(TryParseUdpPacket(tiny, info));

        // Bad magic
        auto pkt = MakeUdpConnectPacket(123);
        pkt[0] ^= 0xFF;
        CHECK_FALSE(TryParseUdpPacket(pkt, info));

        // Trailing junk after declared payload
        auto good = MakeUdpAcceptPacket(7, 11);
        good.push_back(0xAB);
        CHECK_FALSE(TryParseUdpPacket(good, info));
    }
}

TEST_CASE("NetworkUdp::OrderedChannel")
{
    const auto base_time = nanotime(int64_t {1});

    SECTION("DeliversInOrderAndChunksByMaxPayload")
    {
        UdpOrderedChannel sender(MakeOptions(/*max_payload*/ 4));
        UdpOrderedChannel receiver(MakeOptions(/*max_payload*/ 4));
        sender.SetSessionId(42);
        receiver.SetSessionId(42);

        const auto data = Bytes("hello world!");

        vector<vector<uint8_t>> wire;
        const auto consumed = sender.PrepareOutput(data, wire, base_time);

        CHECK(consumed == data.size());
        // 12 bytes / 4 per packet => 3 packets
        CHECK(wire.size() == 3);

        REQUIRE(FeedAll(wire, receiver) == 3);

        vector<uint8_t> ready;
        CHECK(receiver.ExtractReadyData(ready) == data.size());
        CHECK(BytesEq(ready, "hello world!"));
        CHECK_FALSE(receiver.HasReadyData());
    }

    SECTION("OutOfOrderPacketsAreReorderedBeforeDelivery")
    {
        UdpOrderedChannel sender(MakeOptions(4));
        UdpOrderedChannel receiver(MakeOptions(4));

        const auto data = Bytes("ABCDEFGH");

        vector<vector<uint8_t>> wire;
        REQUIRE(sender.PrepareOutput(data, wire, base_time) == data.size());
        REQUIRE(wire.size() == 2);

        // Deliver second packet first.
        UdpPacketInfo info1;
        UdpPacketInfo info0;
        REQUIRE(TryParseUdpPacket(wire[1], info1));
        REQUIRE(TryParseUdpPacket(wire[0], info0));

        receiver.HandleIncomingPayload(info1);
        CHECK_FALSE(receiver.HasReadyData());

        receiver.HandleIncomingPayload(info0);
        REQUIRE(receiver.HasReadyData());

        vector<uint8_t> ready;
        CHECK(receiver.ExtractReadyData(ready) == data.size());
        CHECK(BytesEq(ready, "ABCDEFGH"));
    }

    SECTION("DuplicatePayloadIsDeliveredOnce")
    {
        UdpOrderedChannel sender(MakeOptions(8));
        UdpOrderedChannel receiver(MakeOptions(8));

        const auto data = Bytes("dupdupdu");

        vector<vector<uint8_t>> wire;
        REQUIRE(sender.PrepareOutput(data, wire, base_time) == data.size());
        REQUIRE(wire.size() == 1);

        UdpPacketInfo info;
        REQUIRE(TryParseUdpPacket(wire[0], info));

        receiver.HandleIncomingPayload(info);
        receiver.HandleIncomingPayload(info);

        vector<uint8_t> ready;
        CHECK(receiver.ExtractReadyData(ready) == data.size());
        CHECK(BytesEq(ready, "dupdupdu"));
    }

    SECTION("AcknowledgementsClearPendingPackets")
    {
        UdpOrderedChannel sender(MakeOptions(4, 1024, 100));
        UdpOrderedChannel receiver(MakeOptions(4));

        const auto data = Bytes("packet1+packet2+packet3-");
        vector<vector<uint8_t>> wire;
        REQUIRE(sender.PrepareOutput(data, wire, base_time) == data.size());
        REQUIRE(wire.size() == 6); // 24 bytes / 4

        REQUIRE(FeedAll(wire, receiver) == 6);

        // Receiver flushes ack.
        vector<vector<uint8_t>> ack_wire;
        receiver.PrepareOutput({}, ack_wire, base_time);
        REQUIRE_FALSE(ack_wire.empty());

        REQUIRE(FeedAll(ack_wire, sender) > 0);

        // After ack, sender past the resend timeout should NOT resend any payload (everything acked).
        vector<vector<uint8_t>> after_ack;
        const auto t = BumpTime(base_time, 1000);
        sender.PrepareOutput({}, after_ack, t);
        CHECK(after_ack.empty());
        CHECK_FALSE(sender.NeedSend(t));
    }

    SECTION("ResendsAfterTimeoutWhenAckMissing")
    {
        UdpOrderedChannel sender(MakeOptions(8, 1024, /*resend*/ 50));
        UdpOrderedChannel receiver(MakeOptions(8));

        const auto data = Bytes("resendme");
        vector<vector<uint8_t>> first_wire;
        REQUIRE(sender.PrepareOutput(data, first_wire, base_time) == data.size());
        REQUIRE(first_wire.size() == 1);

        // Pretend the wire packet was lost. Bump time past the resend timeout.
        const auto later = BumpTime(base_time, 75);
        REQUIRE(sender.NeedSend(later));

        vector<vector<uint8_t>> resend_wire;
        sender.PrepareOutput({}, resend_wire, later);
        REQUIRE(resend_wire.size() == 1);

        // Receiver finally gets it.
        REQUIRE(FeedAll(resend_wire, receiver) == 1);

        vector<uint8_t> ready;
        CHECK(receiver.ExtractReadyData(ready) == data.size());
        CHECK(BytesEq(ready, "resendme"));
    }

    SECTION("WindowBlocksFurtherPayloadUntilAcknowledged")
    {
        // Window of 16 bytes, payload of 8 → only two packets fit.
        UdpOrderedChannel sender(MakeOptions(/*max_payload*/ 8, /*window*/ 16));
        UdpOrderedChannel receiver(MakeOptions(8));

        const auto data = Bytes("AAAAAAAA"
                                "BBBBBBBB"
                                "CCCCCCCC");
        vector<vector<uint8_t>> wire;
        const auto consumed_first = sender.PrepareOutput(data, wire, base_time);
        CHECK(consumed_first == 16);
        CHECK(wire.size() == 2);
        CHECK_FALSE(sender.CanAcceptPayload());

        // Try sending more — nothing should fit until window opens.
        const auto leftover = BytesTail(data, consumed_first);
        vector<vector<uint8_t>> wire_blocked;
        const auto consumed_blocked = sender.PrepareOutput(leftover, wire_blocked, BumpTime(base_time, 5));
        CHECK(consumed_blocked == 0);

        // Receiver acks both.
        REQUIRE(FeedAll(wire, receiver) == 2);
        vector<vector<uint8_t>> ack_wire;
        receiver.PrepareOutput({}, ack_wire, base_time);
        REQUIRE(FeedAll(ack_wire, sender) > 0);

        CHECK(sender.CanAcceptPayload());

        vector<vector<uint8_t>> wire_after;
        const auto consumed_after = sender.PrepareOutput(leftover, wire_after, BumpTime(base_time, 10));
        CHECK(consumed_after == leftover.size());
        CHECK(wire_after.size() == 1);
    }

    SECTION("AckOnlyKeepAliveDoesNotCascade")
    {
        // Regression: ack-only KeepAlive must not arm another ack on the receiver,
        // otherwise both peers would ping-pong KeepAlive packets forever.
        UdpOrderedChannel sender(MakeOptions(8));
        UdpOrderedChannel receiver(MakeOptions(8));

        const auto data = Bytes("oneshot!");
        vector<vector<uint8_t>> wire;
        REQUIRE(sender.PrepareOutput(data, wire, base_time) == data.size());
        REQUIRE(FeedAll(wire, receiver) == 1);

        // Receiver emits ack-only KeepAlive.
        vector<vector<uint8_t>> ack_wire;
        receiver.PrepareOutput({}, ack_wire, base_time);
        REQUIRE(ack_wire.size() == 1);
        UdpPacketInfo ack_info;
        REQUIRE(TryParseUdpPacket(ack_wire[0], ack_info));
        CHECK(ack_info.Type == UdpPacketType::KeepAlive);

        // Sender consumes the ack — its pending list should clear.
        FeedAll(ack_wire, sender);

        // Critical: sender must NOT now need to send anything in response to the ack-only KeepAlive.
        CHECK_FALSE(sender.NeedSend(BumpTime(base_time, 1)));

        vector<vector<uint8_t>> nothing;
        sender.PrepareOutput({}, nothing, BumpTime(base_time, 1));
        CHECK(nothing.empty());
    }

    SECTION("EmitsAckOnlyKeepAliveWhenNoPayloadPending")
    {
        UdpOrderedChannel sender(MakeOptions(8));
        UdpOrderedChannel receiver(MakeOptions(8));

        const auto data = Bytes("ackonly!");
        vector<vector<uint8_t>> wire;
        REQUIRE(sender.PrepareOutput(data, wire, base_time) == data.size());
        REQUIRE(FeedAll(wire, receiver) == 1);

        CHECK(receiver.NeedSend(base_time));

        vector<vector<uint8_t>> ack_wire;
        receiver.PrepareOutput({}, ack_wire, base_time);
        REQUIRE(ack_wire.size() == 1);

        UdpPacketInfo info;
        REQUIRE(TryParseUdpPacket(ack_wire[0], info));
        CHECK(info.Type == UdpPacketType::KeepAlive);
        CHECK(info.Payload.empty());

        // Second call must not re-emit (ack already consumed).
        vector<vector<uint8_t>> ack_wire2;
        receiver.PrepareOutput({}, ack_wire2, base_time);
        CHECK(ack_wire2.empty());
        CHECK_FALSE(receiver.NeedSend(base_time));
    }

    SECTION("TailRedundancyResendsRecentPackets")
    {
        // Redundancy=2: every new send round also re-emits up to 2 previously-sent packets.
        UdpOrderedChannel sender(MakeOptions(4, 1024, 100, 200, /*redundancy*/ 2));
        UdpOrderedChannel receiver(MakeOptions(4));

        // Send 3 packets first, then a 4th that should trigger redundancy of 2 of the first 3.
        const auto data1 = Bytes("AAAA"
                                 "BBBB"
                                 "CCCC");
        vector<vector<uint8_t>> wire1;
        REQUIRE(sender.PrepareOutput(data1, wire1, base_time) == data1.size());
        REQUIRE(wire1.size() == 3);

        const auto data2 = Bytes("DDDD");
        vector<vector<uint8_t>> wire2;
        REQUIRE(sender.PrepareOutput(data2, wire2, BumpTime(base_time, 5)) == data2.size());
        // 1 new + 2 redundant = 3 packets.
        CHECK(wire2.size() == 3);

        // Drop wire1 entirely; only wire2 reaches the receiver. Channel should still recover
        // because wire2 carries the redundant copies of B and C, plus the new D.
        REQUIRE(FeedAll(wire2, receiver) == 3);

        // Receiver still missing A → no ready data yet.
        CHECK_FALSE(receiver.HasReadyData());

        // Sender retries A on resend timeout.
        const auto retry_time = BumpTime(base_time, 200);
        vector<vector<uint8_t>> wire3;
        sender.PrepareOutput({}, wire3, retry_time);
        REQUIRE_FALSE(wire3.empty());
        REQUIRE(FeedAll(wire3, receiver) >= 1);

        vector<uint8_t> ready;
        CHECK(receiver.ExtractReadyData(ready) == data1.size() + data2.size());
        CHECK(BytesEq(ready, "AAAABBBBCCCCDDDD"));
    }

    SECTION("DisconnectPacketBuildsCorrectType")
    {
        UdpOrderedChannel ch(MakeOptions());
        ch.SetSessionId(0xABCDU);

        const auto raw = ch.MakeDisconnectPacket();
        UdpPacketInfo info;
        REQUIRE(TryParseUdpPacket(raw, info));
        CHECK(info.Type == UdpPacketType::Disconnect);
        CHECK(info.SessionId == 0xABCDU);
        CHECK(info.Payload.empty());
    }

    SECTION("ResetClearsState")
    {
        UdpOrderedChannel sender(MakeOptions(8));
        sender.SetSessionId(0x55U);
        CHECK(sender.HasSession());

        const auto data = Bytes("resetme!");
        vector<vector<uint8_t>> wire;
        REQUIRE(sender.PrepareOutput(data, wire, base_time) == data.size());
        // After sending, NeedSend at base_time is false (just sent), but pending is non-empty.
        CHECK(sender.NeedSend(BumpTime(base_time, 1000)));

        sender.Reset();
        CHECK(sender.GetSessionId() == 0U);
        CHECK_FALSE(sender.HasSession());
        CHECK(sender.CanAcceptPayload());
        CHECK_FALSE(sender.HasReadyData());
        CHECK_FALSE(sender.NeedSend(BumpTime(base_time, 1000)));
    }
}

FO_END_NAMESPACE
