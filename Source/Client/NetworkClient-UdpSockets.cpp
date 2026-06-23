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

#include "NetworkClient.h"

#include "NetSockets.h"
#include "NetworkUdp.h"

FO_BEGIN_NAMESPACE

static auto PacketBufferBytes(vector<uint8_t>& packet_buf) noexcept -> ptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!packet_buf.empty(), "Packet buffer is empty");

    return packet_buf.data();
}

static auto ReceivedPacketSpan(vector<uint8_t>& packet_buf, size_t received) noexcept -> const_span<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(received <= packet_buf.size(), "Received byte count exceeds the packet buffer size");

    auto packet_buf_data = PacketBufferBytes(packet_buf);
    return {packet_buf_data.get(), received};
}

class NetworkClientConnection_UdpSockets final : public NetworkClientConnection
{
public:
    explicit NetworkClientConnection_UdpSockets(ptr<ClientNetworkSettings> settings);
    NetworkClientConnection_UdpSockets(const NetworkClientConnection_UdpSockets&) = delete;
    NetworkClientConnection_UdpSockets(NetworkClientConnection_UdpSockets&&) noexcept = delete;
    auto operator=(const NetworkClientConnection_UdpSockets&) = delete;
    auto operator=(NetworkClientConnection_UdpSockets&&) noexcept = delete;
    ~NetworkClientConnection_UdpSockets() override;

protected:
    auto CheckStatusImpl(bool for_write) -> bool override;
    auto SendDataImpl(const_span<uint8_t> buf) -> size_t override;
    auto ReceiveDataImpl(vector<uint8_t>& buf) -> size_t override;
    void DisconnectImpl() noexcept override;

private:
    [[nodiscard]] auto MakeOptions() const -> UdpTransportOptions;
    void PumpInput();
    void SendPackets(const vector<vector<uint8_t>>& packets);
    void ServiceConnect(nanotime now);

    udp_socket _socket {};
    UdpOrderedChannel _channel;
    string _requestHost {};
    string _remoteHost {};
    uint16_t _remotePort {};
    uint32_t _clientSalt {};
    nanotime _connectStartTime {};
    nanotime _lastConnectSend {};
    vector<uint8_t> _packetBuf {};
    vector<uint8_t> _readyBuf {};
};

auto NetworkClientConnection::CreateUdpSocketsConnection(ptr<ClientNetworkSettings> settings) -> unique_ptr<NetworkClientConnection>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<NetworkClientConnection_UdpSockets>(settings);
}

NetworkClientConnection_UdpSockets::NetworkClientConnection_UdpSockets(ptr<ClientNetworkSettings> settings) :
    NetworkClientConnection(settings),
    _channel(MakeOptions())
{
    FO_STACK_TRACE_ENTRY();

    if (!net_sockets::startup()) {
        throw NetworkClientException("Socket startup failed for UDP transport");
    }

    _requestHost = _settings->ServerHost;
    _remoteHost = _requestHost;
    _remotePort = numeric_cast<uint16_t>(_settings->ServerPort + _settings->UdpPortOffset);

    std::mt19937 random_generator {MakeSeededRandomGenerator()};
    std::uniform_int_distribution<int32_t> random_distribution {1, 255};
    _clientSalt = (numeric_cast<uint32_t>(random_distribution(random_generator)) << 24) | //
        (numeric_cast<uint32_t>(random_distribution(random_generator)) << 16) | //
        (numeric_cast<uint32_t>(random_distribution(random_generator)) << 8) | //
        (numeric_cast<uint32_t>(random_distribution(random_generator)) << 0);

    const auto packet_capacity = numeric_cast<size_t>(std::max(_settings->UdpPacketSize, 0)) * 2;
    const auto net_capacity = numeric_cast<size_t>(std::max(_settings->NetBufferSize, 0));
    _packetBuf.resize(std::max(packet_capacity, net_capacity));

    if (!_socket.bind("0.0.0.0", 0, false)) {
        throw NetworkClientException("Can't bind UDP client socket");
    }

    _connectStartTime = nanotime::now();
    WriteLog("Connecting to server '{}:{}' over UDP", _requestHost, _remotePort);
}

NetworkClientConnection_UdpSockets::~NetworkClientConnection_UdpSockets()
{
    FO_STACK_TRACE_ENTRY();

    DisconnectImpl();
}

auto NetworkClientConnection_UdpSockets::CheckStatusImpl(bool for_write) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto now = nanotime::now();

    PumpInput();

    if (_isConnecting) {
        ServiceConnect(now);

        if (!_isConnecting && !_isConnected) {
            return false;
        }

        PumpInput();

        if (_isConnecting) {
            return false;
        }

        return for_write ? _socket.can_write() : _channel.HasReadyData();
    }

    if (_channel.NeedSend(now) && _socket.can_write()) {
        vector<vector<uint8_t>> packets;
        _channel.PrepareOutput({}, packets, now);
        SendPackets(packets);
    }

    if (for_write) {
        return _channel.CanAcceptPayload() && _socket.can_write();
    }

    return _channel.HasReadyData();
}

auto NetworkClientConnection_UdpSockets::SendDataImpl(const_span<uint8_t> buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    PumpInput();

    if (!_isConnected) {
        return 0;
    }

    vector<vector<uint8_t>> packets;
    const auto consumed = _channel.PrepareOutput(buf, packets, nanotime::now());
    SendPackets(packets);
    return consumed;
}

auto NetworkClientConnection_UdpSockets::ReceiveDataImpl(vector<uint8_t>& buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    PumpInput();

    const auto ready_size = _channel.ExtractReadyData(_readyBuf);

    if (ready_size == 0) {
        return 0;
    }

    if (buf.size() < ready_size) {
        buf.resize(ready_size);
    }

    std::copy(_readyBuf.begin(), _readyBuf.end(), buf.begin());
    return ready_size;
}

void NetworkClientConnection_UdpSockets::DisconnectImpl() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_socket.is_valid() && _channel.HasSession() && _socket.can_write()) {
        const auto packet = _channel.MakeDisconnectPacket();
        _socket.send_to(_remoteHost, _remotePort, packet);
    }

    _channel.Reset();
    _readyBuf.clear();
    _packetBuf.clear();
    _socket.close();
    _connectStartTime = nanotime::zero;
    _lastConnectSend = nanotime::zero;
}

auto NetworkClientConnection_UdpSockets::MakeOptions() const -> UdpTransportOptions
{
    FO_STACK_TRACE_ENTRY();

    UdpTransportOptions options;
    options.MaxPayload = numeric_cast<size_t>(std::max(_settings->UdpPacketSize, 256));
    options.MaxPendingBytes = std::max(numeric_cast<size_t>(std::max(_settings->UdpWindowSize, 0)), options.MaxPayload);
    options.ResendTimeoutMs = numeric_cast<uint32_t>(std::max(_settings->UdpResendTimeout, 1));
    options.ConnectRetryMs = numeric_cast<uint32_t>(std::max(_settings->UdpConnectRetry, 1));
    options.Redundancy = numeric_cast<uint32_t>(std::max(_settings->UdpRedundancy, 0));
    return options;
}

void NetworkClientConnection_UdpSockets::PumpInput()
{
    FO_STACK_TRACE_ENTRY();

    while (_socket.can_read()) {
        string host;
        uint16_t port = 0;
        span<uint8_t> packet_buf {_packetBuf};
        const auto received = _socket.receive_from(packet_buf, host, port);

        if (received <= 0) {
            break;
        }

        UdpPacketInfo packet;
        const_span<uint8_t> received_packet = ReceivedPacketSpan(_packetBuf, numeric_cast<size_t>(received));

        if (!TryParseUdpPacket(received_packet, packet)) {
            continue;
        }

        if (_isConnecting) {
            if (packet.Type == UdpPacketType::Accept && packet.Value == _clientSalt && packet.SessionId != 0) {
                _channel.SetSessionId(packet.SessionId);
                _remoteHost = host;
                _remotePort = port;
                _isConnecting = false;
                _isConnected = true;
                WriteLog("Connected to server '{}:{}' over UDP", _remoteHost, _remotePort);
            }

            continue;
        }

        if (packet.SessionId != _channel.GetSessionId()) {
            continue;
        }

        if (packet.Type == UdpPacketType::Disconnect) {
            Disconnect();
            return;
        }

        _channel.HandleIncomingPayload(packet);
    }
}

void NetworkClientConnection_UdpSockets::SendPackets(const vector<vector<uint8_t>>& packets)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& packet : packets) {
        if (packet.empty()) {
            continue;
        }

        if (!_socket.can_write()) {
            break;
        }

        const auto sent = _socket.send_to(_remoteHost, _remotePort, packet);

        if (sent <= 0) {
            throw NetworkClientException("UDP send error");
        }
    }
}

void NetworkClientConnection_UdpSockets::ServiceConnect(nanotime now)
{
    FO_STACK_TRACE_ENTRY();

    const uint32_t connect_timeout_ms = numeric_cast<uint32_t>(std::max(_settings->UdpConnectTimeout, _settings->UdpConnectRetry));

    if (_connectStartTime != nanotime::zero && now - _connectStartTime >= std::chrono::milliseconds {connect_timeout_ms}) {
        WriteLog("UDP connect timeout to server '{}:{}'", _requestHost, _remotePort);
        Disconnect();
        return;
    }

    if (_lastConnectSend != nanotime::zero && now - _lastConnectSend < std::chrono::milliseconds {MakeOptions().ConnectRetryMs}) {
        return;
    }

    if (_socket.can_write()) {
        const auto packet = MakeUdpConnectPacket(_clientSalt);

        if (_socket.send_to(_requestHost, _remotePort, packet) <= 0) {
            throw NetworkClientException("Can't send UDP connect packet");
        }

        _lastConnectSend = now;
    }
}

FO_END_NAMESPACE
