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

#include "NetworkServer.h"

#if FO_HAVE_WEB_SOCKETS

#define ASIO_STANDALONE 1
#define ASIO_NO_DEPRECATED 1
#include "asio.hpp"
// ReSharper disable CppInconsistentNaming
#define _WEBSOCKETPP_CPP11_FUNCTIONAL_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_MEMORY_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_STL_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
// ReSharper restore CppInconsistentNaming
FO_DISABLE_WARNINGS_PUSH()
#include "websocketpp/config/asio.hpp"
#include "websocketpp/server.hpp"
FO_DISABLE_WARNINGS_POP()
using web_sockets_tls = websocketpp::server<websocketpp::config::asio_tls>;
using web_sockets_no_tls = websocketpp::server<websocketpp::config::asio>;
using ssl_context = asio::ssl::context;

FO_BEGIN_NAMESPACE

template<bool Secured>
class NetworkServerConnection_WebSockets final : public NetworkServerConnection
{
    using web_server_t = std::conditional_t<Secured, web_sockets_tls, web_sockets_no_tls>;
    using connection_ptr = web_server_t::connection_ptr;
    using connection_weak_ptr = websocketpp::lib::weak_ptr<typename connection_ptr::element_type>;
    using message_ptr = web_server_t::message_ptr;

public:
    explicit NetworkServerConnection_WebSockets(ptr<ServerNetworkSettings> settings, connection_ptr connection);
    NetworkServerConnection_WebSockets(const NetworkServerConnection_WebSockets&) = delete;
    NetworkServerConnection_WebSockets(NetworkServerConnection_WebSockets&&) noexcept = delete;
    auto operator=(const NetworkServerConnection_WebSockets&) = delete;
    auto operator=(NetworkServerConnection_WebSockets&&) noexcept = delete;
    ~NetworkServerConnection_WebSockets() override;

    // Runs after the owning shared_ptr exists, because weak_from_this() is unusable in the constructor
    void Start();

private:
    void LogSocketOperationError(string_view operation, const std::error_code& error);

    void OnMessage(const message_ptr& msg);
    void OnFail(const websocketpp::connection_hdl& hdl);
    void OnClose(const websocketpp::connection_hdl& hdl);
    void OnHttp(const websocketpp::connection_hdl& hdl);

    void DispatchImpl() override;
    void DisconnectImpl() override;

    // Weak because the endpoint owns the connection and destroys it on the io thread: a strong ref here could
    // outlive the io_context and destroy it afterwards
    connection_weak_ptr _connection {};
};

template<bool Secured>
class NetworkServer_WebSockets : public NetworkServer
{
    using web_server_t = std::conditional_t<Secured, web_sockets_tls, web_sockets_no_tls>;

public:
    explicit NetworkServer_WebSockets(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback);
    NetworkServer_WebSockets(const NetworkServer_WebSockets&) = delete;
    NetworkServer_WebSockets(NetworkServer_WebSockets&&) noexcept = delete;
    auto operator=(const NetworkServer_WebSockets&) = delete;
    auto operator=(NetworkServer_WebSockets&&) noexcept = delete;
    ~NetworkServer_WebSockets() override = default;

    void ShutdownImpl() override;

private:
    void Run();
    void OnOpen(const websocketpp::connection_hdl& hdl);
    void OnFail(const websocketpp::connection_hdl& hdl);
    auto OnValidate(const websocketpp::connection_hdl& hdl) -> bool;
    auto OnTlsInit(const websocketpp::connection_hdl& hdl) const -> websocketpp::lib::shared_ptr<ssl_context>;

    ptr<ServerNetworkSettings> _settings;
    NewConnectionCallback _connectionCallback {};
    web_server_t _server {};
    thread _runThread {};
};

auto NetworkServer::StartWebSocketsServer(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>
{
    FO_STACK_TRACE_ENTRY();

    uint16_t ws_port = numeric_cast<uint16_t>(settings->WebSocketPort);

    if (settings->SecuredWebSockets) {
        logging::write("Listen WebSockets (with TLS) connections on port {}", ws_port);

        return safe_alloc::make_unique<NetworkServer_WebSockets<true>>(settings, std::move(callback));
    }
    else {
        logging::write("Listen WebSockets (no TLS) connections on port {}", ws_port);

        return safe_alloc::make_unique<NetworkServer_WebSockets<false>>(settings, std::move(callback));
    }
}

template<bool Secured>
NetworkServerConnection_WebSockets<Secured>::NetworkServerConnection_WebSockets(ptr<ServerNetworkSettings> settings, connection_ptr connection) :
    NetworkServerConnection(settings),
    _connection {connection}
{
    FO_STACK_TRACE_ENTRY();

    auto& raw_socket = connection->get_raw_socket();

    std::error_code endpoint_error;
    auto endpoint = raw_socket.remote_endpoint(endpoint_error);

    if (!endpoint_error) {
        _host = endpoint.address().to_string();
        _port = endpoint.port();
    }
    else {
        _host = "Unknown";
        _port = 0;
    }

    if (settings->DisableTcpNagle) {
        std::error_code no_delay_error;
        raw_socket.set_option(asio::ip::tcp::no_delay(true), no_delay_error);
        LogSocketOperationError("set TCP_NODELAY", no_delay_error);
    }
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::Start()
{
    FO_STACK_TRACE_ENTRY();

    // The connection outlives this wrapper, so each handler locks the weak ref to keep it alive; a failed lock
    // means the wrapper is gone and the callback is a no-op
    auto connection = _connection.lock();

    if (!connection) {
        return;
    }

    auto weak_self = weak_from_this();

    connection->set_message_handler([weak_self, this](auto&&, auto&& msg) FO_DEFERRED {
        auto self = weak_self.lock();

        if (self) {
            OnMessage(msg);
        }
    });
    connection->set_fail_handler([weak_self, this](auto&& hdl) FO_DEFERRED {
        auto self = weak_self.lock();

        if (self) {
            OnFail(hdl);
        }
    });
    connection->set_close_handler([weak_self, this](auto&& hdl) FO_DEFERRED {
        auto self = weak_self.lock();

        if (self) {
            OnClose(hdl);
        }
    });
    connection->set_http_handler([weak_self, this](auto&& hdl) FO_DEFERRED {
        auto self = weak_self.lock();

        if (self) {
            OnHttp(hdl);
        }
    });
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::LogSocketOperationError(string_view operation, const std::error_code& error)
{
    if (!error || error == asio::error::not_connected || error == asio::error::bad_descriptor || error == asio::error::operation_aborted) {
        return;
    }

    if (_port != 0) {
        logging::write(logging::type::warning, "WebSocket socket {} failed for {}:{}: {}", operation, _host, _port, GetAsioErrorText(error));
    }
    else {
        logging::write(logging::type::warning, "WebSocket socket {} failed for {}: {}", operation, _host, GetAsioErrorText(error));
    }
}

template<bool Secured>
NetworkServerConnection_WebSockets<Secured>::~NetworkServerConnection_WebSockets()
{
    FO_STACK_TRACE_ENTRY();

    try {
        // close() posts to the io service and is the only thread-safe teardown: terminate() is io-thread-only
        // and would race the run loop from here
        if (auto connection = _connection.lock()) {
            std::error_code close_error;
            connection->close(websocketpp::close::status::going_away, "", close_error);
            ignore_unused(close_error);
        }
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
    }
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::OnMessage(const message_ptr& msg)
{
    FO_STACK_TRACE_ENTRY();

    const auto& payload = msg->get_payload();

    if (!payload.empty()) {
        ReceiveCallback(make_const_span(payload));
    }
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::OnFail(const websocketpp::connection_hdl& hdl)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(hdl);

    Disconnect();
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::OnClose(const websocketpp::connection_hdl& hdl)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(hdl);

    Disconnect();
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::OnHttp(const websocketpp::connection_hdl& hdl)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(hdl);

    // Prevent use this feature
    Disconnect();
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::DispatchImpl()
{
    FO_STACK_TRACE_ENTRY();

    auto connection = _connection.lock();

    if (!connection) {
        return;
    }

    auto buf = SendCallback();

    if (!buf.empty()) {
        auto error = connection->send(buf.data(), buf.size(), websocketpp::frame::opcode::binary);

        if (!error) {
            DispatchImpl();
        }
        else {
            logging::write(logging::type::warning, "WebSocket send failed to {}:{}: {}", _host, _port, GetAsioErrorText(error));
            Disconnect();
        }
    }
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::DisconnectImpl()
{
    FO_STACK_TRACE_ENTRY();

    // Runs on the engine thread, so teardown goes through the thread-safe close() rather than the io-thread-only
    // terminate(), which would race the run loop
    if (auto connection = _connection.lock()) {
        std::error_code close_error;
        connection->close(websocketpp::close::status::going_away, "", close_error);
        ignore_unused(close_error);
    }
}

template<bool Secured>
NetworkServer_WebSockets<Secured>::NetworkServer_WebSockets(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) :
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();

    if constexpr (Secured) {
        if (_settings->WssPrivateKey.empty()) {
            throw GenericException("'WssPrivateKey' not provided");
        }
        if (_settings->WssCertificate.empty()) {
            throw GenericException("'WssCertificate' not provided");
        }
    }

    _connectionCallback = std::move(callback);

    _server.init_asio();
    _server.clear_access_channels(websocketpp::log::alevel::all);
    _server.clear_error_channels(websocketpp::log::elevel::all);
    _server.set_access_channels(websocketpp::log::alevel::access_core);
    _server.set_open_handler([this](auto&& hdl) FO_DEFERRED { OnOpen(hdl); });
    _server.set_fail_handler([this](auto&& hdl) FO_DEFERRED { OnFail(hdl); });
    _server.set_validate_handler([this](auto&& hdl) FO_DEFERRED { return OnValidate(hdl); });

    if constexpr (Secured) {
        _server.set_tls_init_handler([this](auto&& hdl) FO_DEFERRED { return OnTlsInit(hdl); });
    }

    websocketpp::lib::error_code listen_error;
    _server.listen(asio::ip::tcp::v6(), numeric_cast<uint16_t>(settings->WebSocketPort), listen_error);

    if (listen_error) {
        throw NetworkServerException("Can't listen for WebSocket connections", settings->WebSocketPort, GetAsioErrorText(listen_error));
    }

    _server.start_accept();

    _runThread = run_thread("Network-WebSockets", [this] { Run(); });
}

template<bool Secured>
void NetworkServer_WebSockets<Secured>::ShutdownImpl()
{
    FO_STACK_TRACE_ENTRY();

    _server.stop();
    _runThread.join();
}

template<bool Secured>
void NetworkServer_WebSockets<Secured>::Run()
{
    FO_STACK_TRACE_ENTRY();

    while (true) {
        try {
            _server.run();
            break;
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }
    }
}

template<bool Secured>
void NetworkServer_WebSockets<Secured>::OnOpen(const websocketpp::connection_hdl& hdl)
{
    FO_STACK_TRACE_ENTRY();

    try {
        auto connection = _server.get_con_from_hdl(hdl);

        try {
            auto ws_connection = safe_alloc::make_shared<NetworkServerConnection_WebSockets<Secured>>(_settings, connection);
            ws_connection->Start();

            if (TrackConnection(ws_connection)) {
                _connectionCallback(std::move(ws_connection));
            }
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);

            asio::error_code terminate_error;
            connection->terminate(terminate_error);
        }
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
    }
}

template<bool Secured>
void NetworkServer_WebSockets<Secured>::OnFail(const websocketpp::connection_hdl& hdl)
{
    FO_STACK_TRACE_ENTRY();

    auto&& connection = _server.get_con_from_hdl(hdl);
    const auto& ec = connection->get_ec();
    auto remote_endpoint = connection->get_remote_endpoint();
    logging::write(logging::type::warning, "WebSocket handshake failed from {}: {}", string_view(remote_endpoint), GetAsioErrorText(ec));
}

template<bool Secured>
auto NetworkServer_WebSockets<Secured>::OnValidate(const websocketpp::connection_hdl& hdl) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto&& connection = _server.get_con_from_hdl(hdl);

    if (_settings->MaxBufferedInputSize > 0) {
        connection->set_max_message_size(numeric_cast<size_t>(_settings->MaxBufferedInputSize));
    }

    connection->select_subprotocol("binary");
    return true;
}

template<bool Secured>
auto NetworkServer_WebSockets<Secured>::OnTlsInit(const websocketpp::connection_hdl& hdl) const -> websocketpp::lib::shared_ptr<ssl_context>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(hdl);

    websocketpp::lib::shared_ptr<ssl_context> ctx = websocketpp::lib::make_shared<ssl_context>(ssl_context::tls_server);
    ctx->set_options(ssl_context::default_workarounds | ssl_context::no_sslv2 | ssl_context::no_sslv3 | ssl_context::no_tlsv1 | ssl_context::no_tlsv1_1 | ssl_context::single_dh_use);
    SSL_CTX_set_ecdh_auto(ctx->native_handle(), 1);
    ctx->use_certificate_chain_file(std::string(_settings->WssCertificate));
    ctx->use_private_key_file(std::string(_settings->WssPrivateKey), ssl_context::pem);
    return ctx;
}

FO_END_NAMESPACE

#endif
