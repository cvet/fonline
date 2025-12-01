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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "websocketpp/config/asio.hpp"
#include "websocketpp/server.hpp"
using web_sockets_tls = websocketpp::server<websocketpp::config::asio_tls>;
using web_sockets_no_tls = websocketpp::server<websocketpp::config::asio>;
using ssl_context = asio::ssl::context;

FO_BEGIN_NAMESPACE();

template<bool Secured>
class NetworkServerConnection_WebSockets final : public NetworkServerConnection
{
    using web_server_t = std::conditional_t<Secured, web_sockets_tls, web_sockets_no_tls>;
    using connection_ptr = web_server_t::connection_ptr;
    using message_ptr = web_server_t::message_ptr;

public:
    explicit NetworkServerConnection_WebSockets(ServerNetworkSettings& settings, web_server_t* server, connection_ptr connection);
    NetworkServerConnection_WebSockets(const NetworkServerConnection_WebSockets&) = delete;
    NetworkServerConnection_WebSockets(NetworkServerConnection_WebSockets&&) noexcept = delete;
    auto operator=(const NetworkServerConnection_WebSockets&) = delete;
    auto operator=(NetworkServerConnection_WebSockets&&) noexcept = delete;
    ~NetworkServerConnection_WebSockets() override;

private:
    void OnMessage(const message_ptr& msg);
    void OnFail(const websocketpp::connection_hdl& hdl);
    void OnClose(const websocketpp::connection_hdl& hdl);
    void OnHttp(const websocketpp::connection_hdl& hdl);

    void DispatchImpl() override;
    void DisconnectImpl() override;

    raw_ptr<web_server_t> _server {};
    connection_ptr _connection {};
};

template<bool Secured>
class NetworkServer_WebSockets : public NetworkServer
{
    using web_server_t = std::conditional_t<Secured, web_sockets_tls, web_sockets_no_tls>;

public:
    explicit NetworkServer_WebSockets(ServerNetworkSettings& settings, NewConnectionCallback callback);
    NetworkServer_WebSockets(const NetworkServer_WebSockets&) = delete;
    NetworkServer_WebSockets(NetworkServer_WebSockets&&) noexcept = delete;
    auto operator=(const NetworkServer_WebSockets&) = delete;
    auto operator=(NetworkServer_WebSockets&&) noexcept = delete;
    ~NetworkServer_WebSockets() override = default;

    void Shutdown() override;

private:
    void Run();
    void OnOpen(const websocketpp::connection_hdl& hdl);

    [[nodiscard]] auto OnValidate(const websocketpp::connection_hdl& hdl) -> bool;
    [[nodiscard]] auto OnTlsInit(const websocketpp::connection_hdl& hdl) const -> websocketpp::lib::shared_ptr<ssl_context>;

    ServerNetworkSettings& _settings;
    NewConnectionCallback _connectionCallback {};
    web_server_t _server {};
    std::thread _runThread {};
};

auto NetworkServer::StartWebSocketsServer(ServerNetworkSettings& settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>
{
    FO_STACK_TRACE_ENTRY();

    if (settings.SecuredWebSockets) {
        WriteLog("Listen WebSockets (with TLS) connections on port {}", settings.ServerPort + 1);

        return SafeAlloc::MakeUnique<NetworkServer_WebSockets<true>>(settings, std::move(callback));
    }
    else {
        WriteLog("Listen WebSockets (no TLS) connections on port {}", settings.ServerPort + 1);

        return SafeAlloc::MakeUnique<NetworkServer_WebSockets<false>>(settings, std::move(callback));
    }
}

template<bool Secured>
NetworkServerConnection_WebSockets<Secured>::NetworkServerConnection_WebSockets(ServerNetworkSettings& settings, web_server_t* server, connection_ptr connection) :
    NetworkServerConnection(settings),
    _server {server},
    _connection {std::move(connection)}
{
    FO_STACK_TRACE_ENTRY();

    _host = _connection->get_raw_socket().remote_endpoint().address().to_string();
    _port = _connection->get_raw_socket().remote_endpoint().port();

    if (settings.DisableTcpNagle) {
        _connection->get_raw_socket().set_option(asio::ip::tcp::no_delay(true));
    }

    _connection->set_message_handler([this](auto&&, auto&& msg) { OnMessage(msg); });
    _connection->set_fail_handler([this](auto&& hdl) { OnFail(hdl); });
    _connection->set_close_handler([this](auto&& hdl) { OnClose(hdl); });
    _connection->set_http_handler([this](auto&& hdl) { OnHttp(hdl); });
}

template<bool Secured>
NetworkServerConnection_WebSockets<Secured>::~NetworkServerConnection_WebSockets()
{
    FO_STACK_TRACE_ENTRY();

    try {
        asio::error_code error;
        _connection->terminate(error);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::OnMessage(const message_ptr& msg)
{
    FO_STACK_TRACE_ENTRY();

    const auto& payload = msg->get_payload();

    if (!payload.empty()) {
        ReceiveCallback({reinterpret_cast<const uint8*>(payload.data()), payload.length()});
    }
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::OnFail(const websocketpp::connection_hdl& hdl)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(hdl);

    WriteLog("Failed: {}", _connection->get_ec().message());
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

    const auto buf = SendCallback();

    if (!buf.empty()) {
        const auto error = _connection->send(buf.data(), buf.size(), websocketpp::frame::opcode::binary);

        if (!error) {
            DispatchImpl();
        }
        else {
            Disconnect();
        }
    }
}

template<bool Secured>
void NetworkServerConnection_WebSockets<Secured>::DisconnectImpl()
{
    FO_STACK_TRACE_ENTRY();

    asio::error_code error;
    _connection->terminate(error);
}

template<bool Secured>
NetworkServer_WebSockets<Secured>::NetworkServer_WebSockets(ServerNetworkSettings& settings, NewConnectionCallback callback) :
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();

    if constexpr (Secured) {
        if (settings.WssPrivateKey.empty()) {
            throw GenericException("'WssPrivateKey' not provided");
        }
        if (settings.WssCertificate.empty()) {
            throw GenericException("'WssCertificate' not provided");
        }
    }

    _connectionCallback = std::move(callback);

    _server.init_asio();
    _server.set_open_handler([this](auto&& hdl) { OnOpen(hdl); });
    _server.set_validate_handler([this](auto&& hdl) { return OnValidate(hdl); });

    if constexpr (Secured) {
        _server.set_tls_init_handler([this](auto&& hdl) { return OnTlsInit(hdl); });
    }

    _server.listen(asio::ip::tcp::v6(), numeric_cast<uint16>(settings.ServerPort + 1));
    _server.start_accept();

    _runThread = std::thread(&NetworkServer_WebSockets::Run, this);
}

template<bool Secured>
void NetworkServer_WebSockets<Secured>::Shutdown()
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
            ReportExceptionAndContinue(ex);
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }
}

template<bool Secured>
void NetworkServer_WebSockets<Secured>::OnOpen(const websocketpp::connection_hdl& hdl)
{
    FO_STACK_TRACE_ENTRY();

    auto&& connection = _server.get_con_from_hdl(hdl);
    _connectionCallback(SafeAlloc::MakeShared<NetworkServerConnection_WebSockets<Secured>>(_settings, &_server, std::move(connection)));
}

template<bool Secured>
auto NetworkServer_WebSockets<Secured>::OnValidate(const websocketpp::connection_hdl& hdl) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto&& connection = _server.get_con_from_hdl(hdl);
    connection->select_subprotocol("binary");
    return true;
}

template<bool Secured>
auto NetworkServer_WebSockets<Secured>::OnTlsInit(const websocketpp::connection_hdl& hdl) const -> websocketpp::lib::shared_ptr<ssl_context>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(hdl);

    auto ctx = websocketpp::lib::shared_ptr<ssl_context>(SafeAlloc::MakeRaw<ssl_context>(ssl_context::tlsv1));
    ctx->set_options(ssl_context::default_workarounds | ssl_context::no_sslv2 | ssl_context::no_sslv3 | ssl_context::single_dh_use);
    ctx->use_certificate_chain_file(std::string(_settings.WssCertificate));
    ctx->use_private_key_file(std::string(_settings.WssPrivateKey), ssl_context::pem);
    return ctx;
}

FO_END_NAMESPACE();

#endif
