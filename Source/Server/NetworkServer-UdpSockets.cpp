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

#include "NetworkServer.h"

#include "NetSockets.h"
#include "NetworkUdp.h"

FO_BEGIN_NAMESPACE

class NetworkServer_UdpSockets;

class NetworkServerConnection_UdpSockets final : public NetworkServerConnection
{
public:
    explicit NetworkServerConnection_UdpSockets(ptr<ServerNetworkSettings> settings, string host, uint16_t port, uint32_t session_id);
    NetworkServerConnection_UdpSockets(const NetworkServerConnection_UdpSockets&) = delete;
    NetworkServerConnection_UdpSockets(NetworkServerConnection_UdpSockets&&) noexcept = delete;
    auto operator=(const NetworkServerConnection_UdpSockets&) = delete;
    auto operator=(NetworkServerConnection_UdpSockets&&) noexcept = delete;
    ~NetworkServerConnection_UdpSockets() override = default;

    [[nodiscard]] auto GetSessionId() const noexcept -> uint32_t;
    [[nodiscard]] auto HasPendingDisconnect() const noexcept -> bool;

    void HandlePacket(const UdpPacketInfo& packet);
    void TickSend(udp_socket& socket, nanotime now);

protected:
    void DispatchImpl() override;
    void DisconnectImpl() override;

private:
    auto MakeOptions() const -> UdpTransportOptions;
    void SendPackets(udp_socket& socket, const vector<vector<uint8_t>>& packets);

    UdpOrderedChannel _channel;
    std::atomic_bool _disconnectRequested {};
    std::atomic_bool _sendRequested {true};
    vector<uint8_t> _pendingOutput {};
    vector<uint8_t> _readyData {};
};

class NetworkServer_UdpSockets final : public NetworkServer
{
public:
    explicit NetworkServer_UdpSockets(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback);
    NetworkServer_UdpSockets(const NetworkServer_UdpSockets&) = delete;
    NetworkServer_UdpSockets(NetworkServer_UdpSockets&&) noexcept = delete;
    auto operator=(const NetworkServer_UdpSockets&) = delete;
    auto operator=(NetworkServer_UdpSockets&&) noexcept = delete;
    ~NetworkServer_UdpSockets() override = default;

    void ShutdownImpl() override;

private:
    uint32_t GenerateSessionId() FO_TSA_REQUIRES(_connectionsLocker);
    auto MakeEndpointKey(string_view host, uint16_t port) const -> string;
    void Run();
    void ProcessIncomingPackets();
    void HandleConnectPacket(string host, uint16_t port, const UdpPacketInfo& packet);
    void TickConnections(nanotime now);

    ptr<ServerNetworkSettings> _settings;
    NewConnectionCallback _connectionCallback {};
    udp_socket _socket {};
    std::atomic_bool _stopped {};
    thread _runThread {};
    mutex _connectionsLocker {};
    std::mt19937 _randomGenerator FO_TSA_GUARDED_BY(_connectionsLocker) {MakeSeededRandomGenerator()};
    unordered_map<uint32_t, shared_ptr<NetworkServerConnection_UdpSockets>> _sessions FO_TSA_GUARDED_BY(_connectionsLocker) {};
    unordered_map<string, uint32_t> _endpointToSession FO_TSA_GUARDED_BY(_connectionsLocker) {};
    vector<uint8_t> _packetBuf {};
};

auto NetworkServer::StartUdpSocketsServer(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Listen UDP connections on port {}", settings->ServerPort + settings->UdpPortOffset);

    if (settings->RejectUdpConnections) {
        WriteLog(LogType::Warning, "UDP connect packets are rejected, clients will fall back to TCP after timeout");
    }

    return SafeAlloc::MakeUnique<NetworkServer_UdpSockets>(settings, std::move(callback));
}

NetworkServerConnection_UdpSockets::NetworkServerConnection_UdpSockets(ptr<ServerNetworkSettings> settings, string host, uint16_t port, uint32_t session_id) :
    NetworkServerConnection(settings),
    _channel(MakeOptions())
{
    FO_STACK_TRACE_ENTRY();

    _host = std::move(host);
    _port = port;
    _channel.SetSessionId(session_id);
}

auto NetworkServerConnection_UdpSockets::GetSessionId() const noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _channel.GetSessionId();
}

auto NetworkServerConnection_UdpSockets::HasPendingDisconnect() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _disconnectRequested;
}

void NetworkServerConnection_UdpSockets::HandlePacket(const UdpPacketInfo& packet)
{
    FO_STACK_TRACE_ENTRY();

    if (packet.Type == UdpPacketType::Disconnect) {
        Disconnect();
        return;
    }

    _channel.HandleIncomingPayload(packet);

    if (_channel.HasReadyData()) {
        _channel.ExtractReadyData(_readyData);

        if (!_readyData.empty()) {
            ReceiveCallback(_readyData);
        }
    }
}

void NetworkServerConnection_UdpSockets::TickSend(udp_socket& socket, nanotime now)
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<uint8_t>> packets;

    if (_disconnectRequested) {
        if (socket.can_write()) {
            auto packet = _channel.MakeDisconnectPacket();
            socket.send_to(_host, _port, packet);
            _disconnectRequested = false;
        }

        return;
    }

    if (_sendRequested) {
        auto buf = SendCallback();

        if (!buf.empty()) {
            _pendingOutput.insert(_pendingOutput.end(), buf.begin(), buf.end());
        }

        _sendRequested = false;
    }

    if (!_pendingOutput.empty() || _channel.NeedSend(now)) {
        size_t consumed = _channel.PrepareOutput(_pendingOutput, packets, now);

        if (consumed != 0) {
            _pendingOutput.erase(_pendingOutput.begin(), _pendingOutput.begin() + numeric_cast<ptrdiff_t>(consumed));
        }
    }

    SendPackets(socket, packets);
}

void NetworkServerConnection_UdpSockets::DispatchImpl()
{
    FO_STACK_TRACE_ENTRY();

    _sendRequested = true;
}

void NetworkServerConnection_UdpSockets::DisconnectImpl()
{
    FO_STACK_TRACE_ENTRY();

    _disconnectRequested = true;
    _sendRequested = false;
}

auto NetworkServerConnection_UdpSockets::MakeOptions() const -> UdpTransportOptions
{
    FO_STACK_TRACE_ENTRY();

    UdpTransportOptions options;
    options.MaxPayload = numeric_cast<size_t>(std::max(_settings->UdpPacketSize, 256));
    options.MaxPendingBytes = std::max(numeric_cast<size_t>(std::max(_settings->UdpWindowSize, 0)), options.MaxPayload);
    options.ResendTimeoutMs = numeric_cast<uint32_t>(std::max(_settings->UdpResendTimeout, 1));
    options.ConnectRetryMs = numeric_cast<uint32_t>(std::max(_settings->UdpConnectRetry, 1));
    options.Redundancy = numeric_cast<uint32_t>(std::max(_settings->UdpRedundancy, 0));
    options.MaxReorderAhead = numeric_cast<uint32_t>(std::max(_settings->MaxUdpReorderAhead, 0));
    return options;
}

void NetworkServerConnection_UdpSockets::SendPackets(udp_socket& socket, const vector<vector<uint8_t>>& packets)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& packet : packets) {
        if (packet.empty()) {
            continue;
        }

        if (!socket.can_write()) {
            break;
        }

        socket.send_to(_host, _port, packet);
    }
}

NetworkServer_UdpSockets::NetworkServer_UdpSockets(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) :
    _settings {settings},
    _connectionCallback {std::move(callback)}
{
    FO_STACK_TRACE_ENTRY();

    if (!net_sockets::startup()) {
        throw NetworkServerException("Socket startup failed for UDP transport");
    }
    if (!_socket.bind("0.0.0.0", numeric_cast<uint16_t>(_settings->ServerPort + _settings->UdpPortOffset))) {
        throw NetworkServerException("Can't bind UDP server socket");
    }

    auto packet_capacity = numeric_cast<size_t>(std::max(_settings->UdpPacketSize, 0)) * 2;
    auto net_capacity = numeric_cast<size_t>(std::max(_settings->NetBufferSize, 0));
    _packetBuf.resize(std::max(packet_capacity, net_capacity));
    _runThread = run_thread("Network-Udp", [this] { Run(); });
}

void NetworkServer_UdpSockets::ShutdownImpl()
{
    FO_STACK_TRACE_ENTRY();

    _stopped = true;
    _socket.close();

    if (_runThread.joinable()) {
        _runThread.join();
    }
}

uint32_t NetworkServer_UdpSockets::GenerateSessionId()
{
    FO_STACK_TRACE_ENTRY();

    std::uniform_int_distribution<int32_t> random_distribution {1, 255};
    return (numeric_cast<uint32_t>(random_distribution(_randomGenerator)) << 24) | //
        (numeric_cast<uint32_t>(random_distribution(_randomGenerator)) << 16) | //
        (numeric_cast<uint32_t>(random_distribution(_randomGenerator)) << 8) | //
        (numeric_cast<uint32_t>(random_distribution(_randomGenerator)) << 0);
}

auto NetworkServer_UdpSockets::MakeEndpointKey(string_view host, uint16_t port) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("{}:{}", host, port);
}

void NetworkServer_UdpSockets::Run()
{
    FO_STACK_TRACE_ENTRY();

    auto tick = std::chrono::milliseconds {std::max(_settings->UdpSendUpdateInterval, 1)};

    while (!_stopped) {
        try {
            if (_socket.can_read(tick)) {
                ProcessIncomingPackets();
            }

            TickConnections(nanotime::now());
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
    }
}

void NetworkServer_UdpSockets::ProcessIncomingPackets()
{
    FO_STACK_TRACE_ENTRY();

    while (_socket.can_read()) {
        string host;
        uint16_t port = 0;
        auto packet_buf = make_span(_packetBuf);
        int32_t received = _socket.receive_from(packet_buf, host, port);

        if (received <= 0) {
            break;
        }

        UdpPacketInfo packet;
        FO_STRONG_ASSERT(numeric_cast<size_t>(received) <= _packetBuf.size(), "Received byte count exceeds the packet buffer size");
        auto received_packet = make_const_span(_packetBuf.data(), numeric_cast<size_t>(received));

        if (!TryParseUdpPacket(received_packet, packet)) {
            continue;
        }

        if (packet.Type == UdpPacketType::Connect) {
            if (_settings->RejectUdpConnections) {
                WriteLog("Reject UDP connect packet from {}:{}", host, port);
                continue;
            }

            HandleConnectPacket(std::move(host), port, packet);
            continue;
        }

        shared_ptr<NetworkServerConnection_UdpSockets> connection;

        {
            scoped_lock locker {_connectionsLocker};
            auto it = _sessions.find(packet.SessionId);

            if (it != _sessions.end()) {
                connection = it->second;
            }
        }

        if (connection && connection->GetHost() == host && connection->GetPort() == port) {
            connection->HandlePacket(packet);
        }
    }
}

void NetworkServer_UdpSockets::HandleConnectPacket(string host, uint16_t port, const UdpPacketInfo& packet)
{
    FO_STACK_TRACE_ENTRY();

    shared_ptr<NetworkServerConnection_UdpSockets> connection;
    bool is_new_connection = false;

    {
        scoped_lock locker {_connectionsLocker};

        string endpoint_key = MakeEndpointKey(host, port);
        auto endpoint_it = _endpointToSession.find(endpoint_key);

        if (endpoint_it != _endpointToSession.end()) {
            auto session_it = _sessions.find(endpoint_it->second);

            if (session_it != _sessions.end()) {
                connection = session_it->second;
            }
        }

        if (!connection) {
            uint32_t session_id = GenerateSessionId();

            while (session_id == 0 || _sessions.count(session_id) != 0) {
                session_id = GenerateSessionId();
            }

            connection = SafeAlloc::MakeShared<NetworkServerConnection_UdpSockets>(_settings, host, port, session_id);
            _sessions.emplace(session_id, connection);
            _endpointToSession.emplace(endpoint_key, session_id);
            is_new_connection = true;
        }
    }

    auto accept_packet = MakeUdpAcceptPacket(connection->GetSessionId(), packet.Value);
    _socket.send_to(connection->GetHost(), connection->GetPort(), accept_packet);

    if (is_new_connection) {
        if (TrackConnection(connection)) {
            _connectionCallback(connection);
        }
    }
}

void NetworkServer_UdpSockets::TickConnections(nanotime now)
{
    FO_STACK_TRACE_ENTRY();

    vector<shared_ptr<NetworkServerConnection_UdpSockets>> connections;

    {
        scoped_lock locker {_connectionsLocker};

        for (const auto& [_, connection] : _sessions) {
            connections.emplace_back(connection);
        }
    }

    for (auto& connection : connections) {
        if (!connection->IsDisconnected() || connection->HasPendingDisconnect()) {
            connection->TickSend(_socket, now);
        }
    }

    scoped_lock locker {_connectionsLocker};

    for (auto it = _sessions.begin(); it != _sessions.end();) {
        if (!it->second->IsDisconnected() || it->second->HasPendingDisconnect()) {
            ++it;
            continue;
        }

        _endpointToSession.erase(MakeEndpointKey(it->second->GetHost(), it->second->GetPort()));
        it = _sessions.erase(it);
    }
}

FO_END_NAMESPACE
