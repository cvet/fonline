// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(NetworkUdpException);

enum class UdpPacketType : uint8_t
{
    Connect = 1,
    Accept = 2,
    Payload = 3,
    KeepAlive = 4,
    Disconnect = 5,
};

struct UdpTransportOptions
{
    size_t MaxPayload {};
    size_t MaxPendingBytes {};
    timespan ResendTimeout {};
    timespan ConnectRetry {};
    uint32_t Redundancy {};
    uint32_t MaxReorderAhead {}; // Max packets buffered ahead of the next expected sequence (0 = unlimited)
};

struct UdpPacketInfo
{
    UdpPacketType Type {};
    uint32_t SessionId {};
    uint32_t Sequence {};
    uint32_t AckSequence {};
    uint32_t AckBits {};
    uint32_t Value {};
    const_span<uint8_t> Payload {};
};

class UdpOrderedChannel final
{
public:
    explicit UdpOrderedChannel(UdpTransportOptions options);
    UdpOrderedChannel(const UdpOrderedChannel&) = delete;
    UdpOrderedChannel(UdpOrderedChannel&&) noexcept = delete;
    auto operator=(const UdpOrderedChannel&) = delete;
    auto operator=(UdpOrderedChannel&&) noexcept = delete;
    ~UdpOrderedChannel() = default;

    [[nodiscard]] auto GetSessionId() const noexcept -> uint32_t;
    [[nodiscard]] auto HasSession() const noexcept -> bool;
    [[nodiscard]] auto HasReadyData() const noexcept -> bool;
    [[nodiscard]] auto CanAcceptPayload() const noexcept -> bool;
    [[nodiscard]] auto NeedSend(nanotime now) const noexcept -> bool;

    void Reset() noexcept;
    void SetSessionId(uint32_t session_id) noexcept;
    auto PrepareOutput(const_span<uint8_t> new_data, vector<vector<uint8_t>>& packets, nanotime now) -> size_t;
    void HandleIncomingPayload(const UdpPacketInfo& packet);
    auto ExtractReadyData(vector<uint8_t>& data) -> size_t;
    auto MakeDisconnectPacket() const -> vector<uint8_t>;

private:
    struct PendingPacket
    {
        uint32_t Sequence {};
        vector<uint8_t> Payload {};
        nanotime LastSend {};
        uint32_t SendCount {};
    };

    auto IsPacketAcknowledged(uint32_t sequence, uint32_t ack_sequence, uint32_t ack_bits) const noexcept -> bool;
    auto MakePacket(UdpPacketType type, uint32_t sequence, const_span<uint8_t> payload, uint32_t value = 0) const -> vector<uint8_t>;

    void ApplyAcknowledgements(uint32_t ack_sequence, uint32_t ack_bits);
    void EmitPendingPacket(const PendingPacket& packet, vector<vector<uint8_t>>& packets) const;
    void EmitAckPacket(vector<vector<uint8_t>>& packets) const;
    void RebuildAckBits() noexcept;
    void QueueTailRedundancy(vector<vector<uint8_t>>& packets, uint32_t first_new_sequence) const;

    UdpTransportOptions _options {};
    uint32_t _sessionId {};
    uint32_t _nextOutgoingSequence {1};
    uint32_t _nextIncomingSequence {1};
    uint32_t _ackBits {};
    bool _ackPending {};
    size_t _pendingBytes {};
    deque<PendingPacket> _pendingPackets {};
    map<uint32_t, vector<uint8_t>> _receivedPackets {};
    vector<uint8_t> _readyData {};
};

auto MakeUdpConnectPacket(uint32_t client_salt) -> vector<uint8_t>;
auto MakeUdpAcceptPacket(uint32_t session_id, uint32_t client_salt) -> vector<uint8_t>;
auto TryParseUdpPacket(const_span<uint8_t> data, UdpPacketInfo& packet) -> bool;

FO_END_NAMESPACE
