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
#include "WebRelated.h"

FO_BEGIN_NAMESPACE

// Proxy types
constexpr auto PROXY_SOCKS4 = 1;
constexpr auto PROXY_SOCKS5 = 2;
constexpr auto PROXY_HTTP = 3;

class NetworkClientConnection_Sockets final : public NetworkClientConnection
{
public:
    explicit NetworkClientConnection_Sockets(ClientNetworkSettings& settings);
    NetworkClientConnection_Sockets(const NetworkClientConnection_Sockets&) = delete;
    NetworkClientConnection_Sockets(NetworkClientConnection_Sockets&&) noexcept = delete;
    auto operator=(const NetworkClientConnection_Sockets&) = delete;
    auto operator=(NetworkClientConnection_Sockets&&) noexcept = delete;
    ~NetworkClientConnection_Sockets() override = default;

protected:
    auto CheckStatusImpl(bool for_write) -> bool override;
    auto SendDataImpl(const_span<uint8_t> buf) -> size_t override;
    auto ReceiveDataImpl(vector<uint8_t>& buf) -> size_t override;
    void DisconnectImpl() noexcept override;

private:
    tcp_socket _sock;

    // Game server endpoint that needs to travel inside the SOCKS / HTTP CONNECT payload (proxy mode only).
    uint32_t _gameAddrIp {};
    uint16_t _gameAddrPort {};
};

auto NetworkClientConnection::CreateSocketsConnection(ClientNetworkSettings& settings) -> unique_ptr<NetworkClientConnection>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<NetworkClientConnection_Sockets>(settings);
}

NetworkClientConnection_Sockets::NetworkClientConnection_Sockets(ClientNetworkSettings& settings) :
    NetworkClientConnection(settings)
{
    FO_STACK_TRACE_ENTRY();

    string_view host = _settings->ServerHost;
    auto port = numeric_cast<uint16_t>(_settings->ServerPort);

#if FO_WEB
    port++;

    if (!_settings->SecuredWebSockets) {
        WebRelated::SetWebSocketScheme(false);
        WriteLog("Connecting to server 'ws://{}:{}'", host, port);
    }
    else {
        WebRelated::SetWebSocketScheme(true);
        WriteLog("Connecting to server 'wss://{}:{}'", host, port);
    }
#else
    WriteLog("Connecting to server '{}:{}'", host, port);
#endif

    if (!net_sockets::startup()) {
        throw NetworkClientException("Socket subsystem startup failed", net_sockets::last_error_text());
    }

    // Direct connect path
    if (_settings->ProxyType == 0) {
        if (!_sock.connect_async(host, port)) {
            throw NetworkClientException("Can't connect to the game server", host, port, net_sockets::last_error_text());
        }

#if !FO_WEB
        if (_settings->DisableTcpNagle && !_sock.set_nodelay(true)) {
            WriteLog("Can't set TCP_NODELAY (disable Nagle) to socket, error '{}'", net_sockets::last_error_text());
        }
#endif

        return;
    }

    // Proxy connect path
#if !FO_IOS && !FO_ANDROID && !FO_WEB
    // SOCKS4/5 payloads embed the destination as raw IPv4 + port, HTTP CONNECT embeds them as text.
    // Resolve the game host once and stash the parts; tcp_socket does its own DNS for connect targets.
    const auto resolved = net_sockets::resolve_ipv4(host);

    if (!resolved.has_value()) {
        throw NetworkClientException("Can't resolve game host for proxy", host);
    }

    _gameAddrIp = *resolved;
    _gameAddrPort = port;

    if (!_sock.connect(_settings->ProxyHost, numeric_cast<uint16_t>(_settings->ProxyPort))) {
        throw NetworkClientException("Can't connect to proxy server", _settings->ProxyHost, _settings->ProxyPort, net_sockets::last_error_text());
    }

    if (_settings->DisableTcpNagle && !_sock.set_nodelay(true)) {
        WriteLog("Can't set TCP_NODELAY (disable Nagle) to socket, error '{}'", net_sockets::last_error_text());
    }

    // After proxy connect succeeds, the network layer expects the same notion of "connected".
    _isConnecting = false;
    _isConnected = true;

    auto send_recv = [this](const_span<uint8_t> buf) -> vector<uint8_t> FO_DEFERRED {
        if (!SendData(buf)) {
            throw NetworkClientException("Net output error");
        }

        const auto time = nanotime::now();

        while (true) {
            if (CheckStatus(false)) {
                const auto result_buf = ReceiveData();
                return vector<uint8_t>(result_buf.begin(), result_buf.end());
            }

            if (nanotime::now() - time >= std::chrono::milliseconds {10000}) {
                throw NetworkClientException("Proxy answer timeout");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    vector<uint8_t> send_buf;
    vector<uint8_t> recv_buf;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    ignore_unused(b1);

    // Authentication
    if (_settings->ProxyType == PROXY_SOCKS4) {
        // Connect
        auto writer = DataWriter(send_buf);
        writer.Write<uint8_t>(numeric_cast<uint8_t>(4)); // Socks version
        writer.Write<uint8_t>(numeric_cast<uint8_t>(1)); // Connect command
        writer.Write<uint16_t>(net_sockets::host_to_net_u16(_gameAddrPort));
        writer.Write<uint32_t>(_gameAddrIp);
        writer.Write<uint8_t>(numeric_cast<uint8_t>(0));

        recv_buf = send_recv(send_buf);

        auto reader = DataReader(recv_buf);
        b1 = reader.Read<uint8_t>(); // Null byte
        b2 = reader.Read<uint8_t>(); // Answer code

        if (b2 != 0x5A) {
            switch (b2) {
            case 0x5B:
                throw NetworkClientException("Proxy connection error, request rejected or failed");
            case 0x5C:
                throw NetworkClientException("Proxy connection error, request failed because client is not running identd (or not reachable from the server)");
            case 0x5D:
                throw NetworkClientException("Proxy connection error, request failed because client's identd could not confirm the user ID string in the request");
            default:
                throw NetworkClientException("Proxy connection error, unknown error", b2);
            }
        }
    }
    else if (_settings->ProxyType == PROXY_SOCKS5) {
        auto writer = DataWriter(send_buf);
        writer.Write<uint8_t>(numeric_cast<uint8_t>(5)); // Socks version
        writer.Write<uint8_t>(numeric_cast<uint8_t>(1)); // Count methods
        writer.Write<uint8_t>(numeric_cast<uint8_t>(2)); // Method

        recv_buf = send_recv(send_buf);

        auto reader = DataReader(recv_buf);
        b1 = reader.Read<uint8_t>(); // Socks version
        b2 = reader.Read<uint8_t>(); // Method

        if (b2 == 2) { // User/Password
            send_buf.clear();
            writer = DataWriter(send_buf);
            writer.Write<uint8_t>(numeric_cast<uint8_t>(1)); // Subnegotiation version
            writer.Write<uint8_t>(numeric_cast<uint8_t>(_settings->ProxyUser.length())); // Name length
            writer.WritePtr(_settings->ProxyUser.c_str(), _settings->ProxyUser.length()); // Name
            writer.Write<uint8_t>(numeric_cast<uint8_t>(_settings->ProxyPass.length())); // Pass length
            writer.WritePtr(_settings->ProxyPass.c_str(), _settings->ProxyPass.length()); // Pass

            recv_buf = send_recv(send_buf);

            reader = DataReader(recv_buf);
            b1 = reader.Read<uint8_t>(); // Subnegotiation version
            b2 = reader.Read<uint8_t>(); // Status

            if (b2 != 0) {
                throw NetworkClientException("Invalid proxy user or password");
            }
        }
        else if (b2 != 0) { // Other authorization
            throw NetworkClientException("Socks server connect fail");
        }

        // Connect
        send_buf.clear();
        writer = DataWriter(send_buf);
        writer.Write<uint8_t>(numeric_cast<uint8_t>(5)); // Socks version
        writer.Write<uint8_t>(numeric_cast<uint8_t>(1)); // Connect command
        writer.Write<uint8_t>(numeric_cast<uint8_t>(0)); // Reserved
        writer.Write<uint8_t>(numeric_cast<uint8_t>(1)); // IP v4 address
        writer.Write<uint32_t>(_gameAddrIp);
        writer.Write<uint16_t>(net_sockets::host_to_net_u16(_gameAddrPort));

        recv_buf = send_recv(send_buf);

        reader = DataReader(recv_buf);
        b1 = reader.Read<uint8_t>(); // Socks version
        b2 = reader.Read<uint8_t>(); // Answer code

        if (b2 != 0) {
            switch (b2) {
            case 1:
                throw NetworkClientException("Proxy connection error, SOCKS-server error");
            case 2:
                throw NetworkClientException("Proxy connection error, connections fail by proxy rules");
            case 3:
                throw NetworkClientException("Proxy connection error, network is not aviable");
            case 4:
                throw NetworkClientException("Proxy connection error, host is not aviable");
            case 5:
                throw NetworkClientException("Proxy connection error, connection denied");
            case 6:
                throw NetworkClientException("Proxy connection error, TTL expired");
            case 7:
                throw NetworkClientException("Proxy connection error, command not supported");
            case 8:
                throw NetworkClientException("Proxy connection error, address type not supported");
            default:
                throw NetworkClientException("Proxy connection error, unknown error", b2);
            }
        }
    }
    else if (_settings->ProxyType == PROXY_HTTP) {
        const string request = strex("CONNECT {}:{} HTTP/1.0\r\n\r\n", net_sockets::ipv4_to_string(_gameAddrIp), _gameAddrPort);
        const auto result = send_recv({reinterpret_cast<const uint8_t*>(request.data()), request.length()});
        const auto result_str = string(reinterpret_cast<const char*>(result.data()), result.size());

        if (result_str.find(" 200 ") == string::npos) {
            throw NetworkClientException("Proxy connection error", request);
        }
    }
    else {
        throw NetworkClientException("Unknown proxy type", _settings->ProxyType);
    }
#else
    throw NetworkClientException("Proxy connection is not supported on this platform");
#endif
}

auto NetworkClientConnection_Sockets::CheckStatusImpl(bool for_write) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const bool ready = for_write ? _sock.can_write({}) : _sock.can_read({});

    if (ready) {
        if (_isConnecting) {
            const auto sock_error = _sock.peek_socket_error();

            if (sock_error != 0) {
                throw NetworkClientException("Socket error during async connect", sock_error);
            }

            WriteLog("Connection established");

            _isConnecting = false;
            _isConnected = true;
        }

        return true;
    }

    if (_isConnecting) {
        // While connect is in flight, peek_socket_error reports any pending error even before select wakes.
        const auto sock_error = _sock.peek_socket_error();

        if (sock_error == 0) {
            return false;
        }

        throw NetworkClientException("Socket error", sock_error);
    }

    return false;
}

auto NetworkClientConnection_Sockets::SendDataImpl(const_span<uint8_t> buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    const auto sent = _sock.send(buf);

    if (sent <= 0) {
        throw NetworkClientException("Socket error while send to server", net_sockets::last_error_text());
    }

    return numeric_cast<size_t>(sent);
}

auto NetworkClientConnection_Sockets::ReceiveDataImpl(vector<uint8_t>& buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_sock.is_valid(), "Missing required sock is valid");

    auto len = _sock.receive(buf);

    if (len < 0) {
        throw NetworkClientException("Socket error while receive from server", net_sockets::last_error_text());
    }

    if (len == 0) {
        throw NetworkClientException("Socket is closed");
    }

    auto whole_len = numeric_cast<size_t>(len);

    // Drain whatever else is currently buffered in the kernel before yielding to the upper layer.
    while (whole_len == buf.size()) {
        buf.resize(buf.size() * 2);

        len = _sock.receive({buf.data() + whole_len, buf.size() - whole_len});

        if (len < 0) {
            if (net_sockets::last_recv_was_would_block()) {
                break;
            }

            throw NetworkClientException("Socket error (2) while receive from server", net_sockets::last_error_text());
        }

        if (len == 0) {
            throw NetworkClientException("Socket is closed (2)");
        }

        whole_len += numeric_cast<size_t>(len);
    }

    return whole_len;
}

void NetworkClientConnection_Sockets::DisconnectImpl() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _sock.close();
}

FO_END_NAMESPACE
