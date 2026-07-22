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

#include "NetworkUdp.h"

FO_BEGIN_NAMESPACE

constexpr uint32_t UDP_PACKET_MAGIC = 0x31445055;
constexpr uint16_t UDP_PACKET_VERSION = 1;
constexpr size_t UDP_PACKET_HEADER_SIZE = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t);

static auto MakeRawPacket(UdpPacketType type, uint32_t session_id, uint32_t sequence, uint32_t ack_sequence, uint32_t ack_bits, uint32_t value, const_span<uint8_t> payload) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    data.reserve(UDP_PACKET_HEADER_SIZE + payload.size());

    const auto append_scalar = [&data](auto scalar) {
        static_assert(std::is_trivially_copyable_v<decltype(scalar)>);
        array<uint8_t, sizeof(scalar)> scalar_bytes {};
        auto source = make_ptr(&scalar).template reinterpret_as<const uint8_t>();
        MemCopy(scalar_bytes.data(), source, sizeof(scalar));
        data.insert(data.end(), scalar_bytes.begin(), scalar_bytes.end());
    };

    append_scalar(UDP_PACKET_MAGIC);
    append_scalar(UDP_PACKET_VERSION);
    append_scalar(static_cast<uint8_t>(type));
    append_scalar(static_cast<uint8_t>(payload.empty() ? 0U : 1U));
    append_scalar(session_id);
    append_scalar(sequence);
    append_scalar(ack_sequence);
    append_scalar(ack_bits);
    append_scalar(value);
    append_scalar(numeric_cast<uint16_t>(payload.size()));
    append_scalar(static_cast<uint16_t>(0U));
    data.insert(data.end(), payload.begin(), payload.end());

    return data;
}

UdpOrderedChannel::UdpOrderedChannel(UdpTransportOptions options) :
    _options {options}
{
    FO_STACK_TRACE_ENTRY();
}

void UdpOrderedChannel::Reset() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _sessionId = 0;
    _nextOutgoingSequence = 1;
    _nextIncomingSequence = 1;
    _ackBits = 0;
    _ackPending = false;
    _pendingBytes = 0;
    _pendingPackets.clear();
    _receivedPackets.clear();
    _readyData.clear();
}

void UdpOrderedChannel::SetSessionId(uint32_t session_id) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _sessionId = session_id;
}

auto UdpOrderedChannel::GetSessionId() const noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _sessionId;
}

auto UdpOrderedChannel::HasSession() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _sessionId != 0;
}

auto UdpOrderedChannel::HasReadyData() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_readyData.empty();
}

auto UdpOrderedChannel::CanAcceptPayload() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _pendingBytes < _options.MaxPendingBytes;
}

auto UdpOrderedChannel::NeedSend(nanotime now) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_ackPending) {
        return true;
    }

    for (const auto& packet : _pendingPackets) {
        if (packet.SendCount == 0 || now - packet.LastSend >= std::chrono::milliseconds {_options.ResendTimeoutMs}) {
            return true;
        }
    }

    return false;
}

auto UdpOrderedChannel::PrepareOutput(const_span<uint8_t> new_data, vector<vector<uint8_t>>& packets, nanotime now) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    size_t consumed = 0;

    for (auto& packet : _pendingPackets) {
        if (packet.SendCount == 0 || now - packet.LastSend >= std::chrono::milliseconds {_options.ResendTimeoutMs}) {
            packet.LastSend = now;
            packet.SendCount++;
            EmitPendingPacket(packet, packets);
        }
    }

    const auto first_new_sequence = _nextOutgoingSequence;

    while (consumed < new_data.size() && _pendingBytes < _options.MaxPendingBytes) {
        const auto free_window = _options.MaxPendingBytes - _pendingBytes;
        const auto chunk_size = std::min({new_data.size() - consumed, _options.MaxPayload, free_window});

        if (chunk_size == 0) {
            break;
        }

        PendingPacket packet;
        packet.Sequence = _nextOutgoingSequence;
        packet.Payload.assign(new_data.begin() + consumed, new_data.begin() + consumed + chunk_size);
        packet.LastSend = now;
        packet.SendCount = 1;
        EmitPendingPacket(packet, packets);
        _nextOutgoingSequence++;
        _pendingBytes += chunk_size;
        _pendingPackets.emplace_back(std::move(packet));
        consumed += chunk_size;
    }

    if (consumed != 0 && _options.Redundancy != 0) {
        QueueTailRedundancy(packets, first_new_sequence);
    }

    if (_ackPending && packets.empty()) {
        EmitAckPacket(packets);
    }

    if (!packets.empty()) {
        _ackPending = false;
    }

    return consumed;
}

void UdpOrderedChannel::HandleIncomingPayload(const UdpPacketInfo& packet)
{
    FO_STACK_TRACE_ENTRY();

    ApplyAcknowledgements(packet.AckSequence, packet.AckBits);

    if (packet.Type != UdpPacketType::Payload || packet.Payload.empty()) {
        return;
    }

    // A duplicate Payload still arms an ack so the sender can drop it from its pending list,
    // but ack-only KeepAlive packets must not arm one — otherwise both peers would ping-pong
    // ack-only packets forever once any payload has been acknowledged.
    _ackPending = true;

    if (packet.Sequence < _nextIncomingSequence) {
        return;
    }

    if (packet.Sequence == _nextIncomingSequence) {
        _readyData.insert(_readyData.end(), packet.Payload.begin(), packet.Payload.end());
        _nextIncomingSequence++;

        while (true) {
            const auto it = _receivedPackets.find(_nextIncomingSequence);

            if (it == _receivedPackets.end()) {
                break;
            }

            _readyData.insert(_readyData.end(), it->second.begin(), it->second.end());
            _receivedPackets.erase(it);
            _nextIncomingSequence++;
        }

        RebuildAckBits();
        return;
    }

    // Bound the out-of-order reorder window: drop payloads too far ahead of the next expected
    // sequence so a peer that never sends the in-order packet cannot grow _receivedPackets without
    // limit. The sender retransmits dropped packets, so an in-window gap still reassembles.
    if (_options.MaxReorderAhead != 0 && packet.Sequence - _nextIncomingSequence > _options.MaxReorderAhead) {
        return;
    }

    if (_receivedPackets.count(packet.Sequence) == 0) {
        _receivedPackets.emplace(packet.Sequence, vector<uint8_t>(packet.Payload.begin(), packet.Payload.end()));
        RebuildAckBits();
    }
}

auto UdpOrderedChannel::ExtractReadyData(vector<uint8_t>& data) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    data.swap(_readyData);
    _readyData.clear();
    return data.size();
}

auto UdpOrderedChannel::MakeDisconnectPacket() const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    return MakePacket(UdpPacketType::Disconnect, 0, {});
}

void UdpOrderedChannel::ApplyAcknowledgements(uint32_t ack_sequence, uint32_t ack_bits)
{
    FO_STACK_TRACE_ENTRY();

    for (auto it = _pendingPackets.begin(); it != _pendingPackets.end();) {
        if (IsPacketAcknowledged(it->Sequence, ack_sequence, ack_bits)) {
            FO_VERIFY_AND_THROW(_pendingBytes >= it->Payload.size(), "UDP ordered channel pending byte counter is smaller than an acknowledged packet payload", _pendingBytes, it->Payload.size(), it->Sequence, ack_sequence, ack_bits);
            _pendingBytes -= it->Payload.size();
            it = _pendingPackets.erase(it);
        }
        else {
            ++it;
        }
    }
}

void UdpOrderedChannel::EmitPendingPacket(const PendingPacket& packet, vector<vector<uint8_t>>& packets) const
{
    FO_STACK_TRACE_ENTRY();

    packets.emplace_back(MakePacket(UdpPacketType::Payload, packet.Sequence, packet.Payload));
}

void UdpOrderedChannel::EmitAckPacket(vector<vector<uint8_t>>& packets) const
{
    FO_STACK_TRACE_ENTRY();

    packets.emplace_back(MakePacket(UdpPacketType::KeepAlive, 0, {}));
}

void UdpOrderedChannel::RebuildAckBits() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _ackBits = 0;

    for (const auto& [sequence, _] : _receivedPackets) {
        if (sequence <= _nextIncomingSequence) {
            continue;
        }

        const auto diff = sequence - _nextIncomingSequence;

        if (diff < 32U) {
            _ackBits |= 1U << diff;
        }
    }
}

void UdpOrderedChannel::QueueTailRedundancy(vector<vector<uint8_t>>& packets, uint32_t first_new_sequence) const
{
    FO_STACK_TRACE_ENTRY();

    uint32_t resent = 0;

    for (auto it = _pendingPackets.rbegin(); it != _pendingPackets.rend() && resent < _options.Redundancy; ++it) {
        if (it->Sequence >= first_new_sequence) {
            continue;
        }

        EmitPendingPacket(*it, packets);
        resent++;
    }
}

auto UdpOrderedChannel::IsPacketAcknowledged(uint32_t sequence, uint32_t ack_sequence, uint32_t ack_bits) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (sequence == 0) {
        return true;
    }

    if (sequence <= ack_sequence) {
        return true;
    }

    const auto diff = sequence - ack_sequence - 1;
    return diff < 32U && (ack_bits & (1U << diff)) != 0;
}

auto UdpOrderedChannel::MakePacket(UdpPacketType type, uint32_t sequence, const_span<uint8_t> payload, uint32_t value) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    return MakeRawPacket(type, _sessionId, sequence, _nextIncomingSequence != 0 ? _nextIncomingSequence - 1 : 0U, _ackBits, value, payload);
}

auto MakeUdpConnectPacket(uint32_t client_salt) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    return MakeRawPacket(UdpPacketType::Connect, 0, 0, 0, 0, client_salt, {});
}

auto MakeUdpAcceptPacket(uint32_t session_id, uint32_t client_salt) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    return MakeRawPacket(UdpPacketType::Accept, session_id, 0, 0, 0, client_salt, {});
}

auto TryParseUdpPacket(const_span<uint8_t> data, UdpPacketInfo& packet) -> bool
{
    FO_STACK_TRACE_ENTRY();

    size_t pos = 0;
    uint32_t magic = 0;
    uint16_t version = 0;
    uint8_t type = 0;
    uint8_t flags = 0;
    uint16_t payload_size = 0;
    uint16_t reserved = 0;

    const auto read_scalar = [&data, &pos](auto& scalar) {
        using ScalarType = std::remove_reference_t<decltype(scalar)>;
        static_assert(std::is_trivially_copyable_v<ScalarType>);

        if (pos + sizeof(ScalarType) > data.size()) {
            return false;
        }

        auto scalar_ptr = make_ptr(&scalar);
        auto target = scalar_ptr.template reinterpret_as<uint8_t>();
        auto source = make_ptr(data.data() + pos);
        MemCopy(target, source, sizeof(ScalarType));
        pos += sizeof(ScalarType);
        return true;
    };

    if (!read_scalar(magic) || magic != UDP_PACKET_MAGIC) {
        return false;
    }

    if (!read_scalar(version) || version != UDP_PACKET_VERSION) {
        return false;
    }

    if (!read_scalar(type) || !read_scalar(flags)) {
        return false;
    }

    switch (type) {
    case static_cast<uint8_t>(UdpPacketType::Connect):
    case static_cast<uint8_t>(UdpPacketType::Accept):
    case static_cast<uint8_t>(UdpPacketType::Payload):
    case static_cast<uint8_t>(UdpPacketType::KeepAlive):
    case static_cast<uint8_t>(UdpPacketType::Disconnect):
        break;
    default:
        return false;
    }

    if (!read_scalar(packet.SessionId) || !read_scalar(packet.Sequence) || //
        !read_scalar(packet.AckSequence) || !read_scalar(packet.AckBits) || //
        !read_scalar(packet.Value) || !read_scalar(payload_size) || !read_scalar(reserved)) {
        return false;
    }

    ignore_unused(reserved);

    if (pos + payload_size != data.size()) {
        return false;
    }

    const bool payload_flag_set = (flags & 0x01) != 0;

    if (payload_flag_set != (payload_size != 0)) {
        return false;
    }

    packet.Type = static_cast<UdpPacketType>(type);

    if (payload_size == 0) {
        packet.Payload = {};
    }
    else {
        FO_STRONG_ASSERT(pos < data.size(), "UDP buffer offset is past the end of the data");
        auto payload_begin = make_ptr(data.data() + pos);
        packet.Payload = {payload_begin.get(), payload_size};
    }

    return true;
}

FO_END_NAMESPACE
