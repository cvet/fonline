// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "NetworkClient.h"

#include "NetSockets.h"
#include "WebRelated.h"

FO_BEGIN_NAMESPACE

// Proxy types
constexpr auto PROXY_SOCKS4 {1};
constexpr auto PROXY_SOCKS5 {2};
constexpr auto PROXY_HTTP {3};

static constexpr timespan PROXY_ANSWER_TIMEOUT {std::chrono::milliseconds {10000}};
static constexpr std::chrono::milliseconds PROXY_ANSWER_POLL_INTERVAL {1};

class NetworkClientConnection_Sockets final : public NetworkClientConnection
{
public:
    explicit NetworkClientConnection_Sockets(ptr<ClientNetworkSettings> settings);
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
    void ApplyTcpNoDelay();

    tcp_socket _sock {};

    // Game server endpoint that needs to travel inside the SOCKS / HTTP CONNECT payload (proxy mode only)
    uint32_t _gameAddrIp {};
    uint16_t _gameAddrPort {};
};

auto NetworkClientConnection::CreateSocketsConnection(ptr<ClientNetworkSettings> settings) -> unique_ptr<NetworkClientConnection>
{
    return safe_alloc::make_unique<NetworkClientConnection_Sockets>(settings);
}

NetworkClientConnection_Sockets::NetworkClientConnection_Sockets(ptr<ClientNetworkSettings> settings) :
    NetworkClientConnection(settings)
{
    FO_TRACE_ZONE(Network);

#if !FO_WEB
    string_view host {static_cast<string_view>(_settings->ClientNetwork.ServerHost)};
    uint16_t port {numeric_cast<uint16_t>(_settings->Network.ServerPort)};

    logging::write("Connecting to server '{}:{}'", host, port);

#else
    const string_view host {static_cast<string_view>(_settings->ClientNetwork.WebSocketHost)};
    const uint16_t port {numeric_cast<uint16_t>(_settings->Network.WebSocketPort)};

    if (!_settings->Network.SecuredWebSockets) {
        WebRelated::SetWebSocketScheme(false);
        logging::write("Connecting to server 'ws://{}:{}'", host, port);
    }
    else {
        WebRelated::SetWebSocketScheme(true);
        logging::write("Connecting to server 'wss://{}:{}'", host, port);
    }
#endif

    if (!net_sockets::startup()) {
        throw NetworkClientException("Socket subsystem startup failed", net_sockets::last_error_text());
    }

    // Direct connect path
    if (_settings->ClientNetwork.ProxyType == 0) {
        if (!_sock.connect_async(host, port)) {
            throw NetworkClientException("Can't connect to the game server", host, port, net_sockets::last_error_text());
        }

        // No TCP_NODELAY here: this connect is still in flight, and Winsock answers a socket option on a
        // connecting socket with WSAEINVAL. CheckStatusImpl sets it the moment the connection completes
        return;
    }

    // Proxy connect path
#if !FO_IOS && !FO_ANDROID && !FO_WEB
    // SOCKS4/5 payloads embed the destination as raw IPv4 + port, HTTP CONNECT embeds them as text.
    // Resolve the game host once and stash the parts; tcp_socket does its own DNS for connect targets
    auto resolved {net_sockets::resolve_ipv4(host)};

    if (!resolved.has_value()) {
        throw NetworkClientException("Can't resolve game host for proxy", host);
    }

    _gameAddrIp = *resolved;
    _gameAddrPort = port;

    if (!_sock.connect(_settings->ClientNetwork.ProxyHost, numeric_cast<uint16_t>(_settings->ClientNetwork.ProxyPort))) {
        throw NetworkClientException("Can't connect to proxy server", _settings->ClientNetwork.ProxyHost, _settings->ClientNetwork.ProxyPort, net_sockets::last_error_text());
    }

    ApplyTcpNoDelay();

    // After proxy connect succeeds, the network layer expects the same notion of "connected"
    _isConnecting = false;
    _isConnected = true;

    auto send_recv {[this](const_span<uint8_t> buf) -> vector<uint8_t> FO_DEFERRED {
        if (!SendData(buf)) {
            throw NetworkClientException("Net output error");
        }

        nanotime time {nanotime::now()};

        while (true) {
            if (CheckStatus(false)) {
                auto result_buf {ReceiveData()};
                return vector<uint8_t>(result_buf.begin(), result_buf.end());
            }

            if (nanotime::now() - time >= PROXY_ANSWER_TIMEOUT) {
                throw NetworkClientException("Proxy answer timeout");
            }

            coarse_sleep(PROXY_ANSWER_POLL_INTERVAL);
        }
    }};

    vector<uint8_t> send_buf {};
    vector<uint8_t> recv_buf {};
    uint8_t b1 {0};
    uint8_t b2 {0};
    ignore_unused(b1);

    // Authentication
    if (_settings->ClientNetwork.ProxyType == PROXY_SOCKS4) {
        // Connect
        data_writer writer {data_writer(send_buf)};
        writer.write<uint8_t>(numeric_cast<uint8_t>(4)); // Socks version
        writer.write<uint8_t>(numeric_cast<uint8_t>(1)); // Connect command
        writer.write<uint16_t>(net_sockets::host_to_net_u16(_gameAddrPort));
        writer.write<uint32_t>(_gameAddrIp);
        writer.write<uint8_t>(numeric_cast<uint8_t>(0));

        recv_buf = send_recv(send_buf);

        data_reader reader {data_reader(recv_buf)};
        b1 = reader.read<uint8_t>(); // Null byte
        b2 = reader.read<uint8_t>(); // Answer code

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
    else if (_settings->ClientNetwork.ProxyType == PROXY_SOCKS5) {
        data_writer writer {data_writer(send_buf)};
        writer.write<uint8_t>(numeric_cast<uint8_t>(5)); // Socks version
        writer.write<uint8_t>(numeric_cast<uint8_t>(1)); // Count methods
        writer.write<uint8_t>(numeric_cast<uint8_t>(2)); // Method

        recv_buf = send_recv(send_buf);

        data_reader reader {data_reader(recv_buf)};
        b1 = reader.read<uint8_t>(); // Socks version
        b2 = reader.read<uint8_t>(); // Method

        if (b2 == 2) { // User/Password
            send_buf.clear();

            {
                data_writer auth_writer {data_writer(send_buf)};
                auth_writer.write<uint8_t>(numeric_cast<uint8_t>(1)); // Subnegotiation version
                auth_writer.write<uint8_t>(numeric_cast<uint8_t>(_settings->ClientNetwork.ProxyUser.length())); // Name length
                auth_writer.write_string_bytes(_settings->ClientNetwork.ProxyUser); // Name
                auth_writer.write<uint8_t>(numeric_cast<uint8_t>(_settings->ClientNetwork.ProxyPass.length())); // Pass length
                auth_writer.write_string_bytes(_settings->ClientNetwork.ProxyPass); // Pass
            }

            recv_buf = send_recv(send_buf);

            reader = data_reader(recv_buf);
            b1 = reader.read<uint8_t>(); // Subnegotiation version
            b2 = reader.read<uint8_t>(); // Status

            if (b2 != 0) {
                throw NetworkClientException("Invalid proxy user or password");
            }
        }
        else if (b2 != 0) { // Other authorization
            throw NetworkClientException("Socks server connect fail");
        }

        // Connect
        send_buf.clear();

        {
            data_writer connect_writer {data_writer(send_buf)};
            connect_writer.write<uint8_t>(numeric_cast<uint8_t>(5)); // Socks version
            connect_writer.write<uint8_t>(numeric_cast<uint8_t>(1)); // Connect command
            connect_writer.write<uint8_t>(numeric_cast<uint8_t>(0)); // Reserved
            connect_writer.write<uint8_t>(numeric_cast<uint8_t>(1)); // IP v4 address
            connect_writer.write<uint32_t>(_gameAddrIp);
            connect_writer.write<uint16_t>(net_sockets::host_to_net_u16(_gameAddrPort));
        }

        recv_buf = send_recv(send_buf);

        reader = data_reader(recv_buf);
        b1 = reader.read<uint8_t>(); // Socks version
        b2 = reader.read<uint8_t>(); // Answer code

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
    else if (_settings->ClientNetwork.ProxyType == PROXY_HTTP) {
        string request {static_cast<string&&>(strex("CONNECT {}:{} HTTP/1.0\r\n\r\n", net_sockets::ipv4_to_string(_gameAddrIp), _gameAddrPort))};
        vector<uint8_t> result {send_recv(make_const_span(request))};
        string result_str {span_to_string(result)};

        if (result_str.find(" 200 ") == string::npos) {
            throw NetworkClientException("Proxy connection error", request);
        }
    }
    else {
        throw NetworkClientException("Unknown proxy type", _settings->ClientNetwork.ProxyType);
    }
#else
    throw NetworkClientException("Proxy connection is not supported on this platform");
#endif
}

auto NetworkClientConnection_Sockets::CheckStatusImpl(bool for_write) -> bool
{
    FO_TRACE_ZONE(Network);

    bool ready {for_write ? _sock.can_write({}) : _sock.can_read({})};

    if (ready) {
        if (_isConnecting) {
            int32_t sock_error {_sock.peek_socket_error()};

            if (sock_error != 0) {
                throw NetworkClientException("Socket error during async connect", sock_error);
            }

            logging::write("Connection established");

            _isConnecting = false;
            _isConnected = true;

            ApplyTcpNoDelay();
        }

        return true;
    }

    if (_isConnecting) {
        // While connect is in flight, peek_socket_error reports any pending error even before select wakes
        int32_t sock_error {_sock.peek_socket_error()};

        if (sock_error == 0) {
            return false;
        }

        throw NetworkClientException("Socket error", sock_error);
    }

    return false;
}

void NetworkClientConnection_Sockets::ApplyTcpNoDelay()
{
#if !FO_WEB
    if (!_settings->Network.DisableTcpNagle) {
        return;
    }

    if (!_sock.set_nodelay(true)) {
        logging::write(logging::type::warning, "Can't set TCP_NODELAY (disable Nagle) to socket, error '{}'", net_sockets::last_error_text());
    }
#endif
}

auto NetworkClientConnection_Sockets::SendDataImpl(const_span<uint8_t> buf) -> size_t
{
    FO_TRACE_ZONE(Network);

    int32_t sent {_sock.send(buf)};

    if (sent <= 0) {
        throw NetworkClientException("Socket error while send to server", net_sockets::last_error_text());
    }

    return numeric_cast<size_t>(sent);
}

auto NetworkClientConnection_Sockets::ReceiveDataImpl(vector<uint8_t>& buf) -> size_t
{
    FO_TRACE_ZONE(Network);

    FO_VERIFY_AND_THROW(_sock.is_valid(), "Socket is not valid");

    int32_t len {_sock.receive(buf)};

    if (len < 0) {
        throw NetworkClientException("Socket error while receive from server", net_sockets::last_error_text());
    }

    if (len == 0) {
        throw NetworkClientException("Socket is closed");
    }

    auto whole_len {numeric_cast<size_t>(len)};

    // Drain whatever else is currently buffered in the kernel before yielding to the upper layer
    while (whole_len == buf.size()) {
        buf.resize(buf.size() * 2);

        auto tail {make_span(buf.data() + whole_len, buf.size() - whole_len)};
        len = _sock.receive(tail);

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
    FO_TRACE_ZONE(Network);

    _sock.close();
}

FO_END_NAMESPACE
