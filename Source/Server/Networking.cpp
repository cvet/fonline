//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: catch exceptions in network servers

#include "Networking.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Timer.h"
#include "WinApi-Include.h"

#define ASIO_STANDALONE
#include "asio.hpp"
#define _WEBSOCKETPP_CPP11_FUNCTIONAL_
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_MEMORY_
#define _WEBSOCKETPP_CPP11_STL_
#pragma warning(push)
#pragma warning(disable : 4267)
#include "websocketpp/config/asio.hpp"
#include "websocketpp/server.hpp"
#pragma warning(pop)
using web_sockets_tls = websocketpp::server<websocketpp::config::asio_tls>;
using web_sockets_no_tls = websocketpp::server<websocketpp::config::asio>;
using ssl_context = asio::ssl::context;
#include "zlib.h"

class NetTcpServer : public NetServerBase
{
public:
    NetTcpServer() = delete;
    NetTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback);
    NetTcpServer(const NetTcpServer&) = delete;
    NetTcpServer(NetTcpServer&&) noexcept = delete;
    auto operator=(const NetTcpServer&) = delete;
    auto operator=(NetTcpServer&&) noexcept = delete;
    ~NetTcpServer() override = default;

    void Shutdown() override;

private:
    void Run();
    void AcceptNext();
    void AcceptConnection(std::error_code error, asio::ip::tcp::socket* socket);

    ServerNetworkSettings& _settings;
    asio::ip::tcp::acceptor _acceptor;
    ConnectionCallback _connectionCallback {};
    asio::io_service _ioService {};
    std::thread _runThread {};
};

class NetNoTlsWebSocketsServer : public NetServerBase
{
public:
    NetNoTlsWebSocketsServer() = delete;
    NetNoTlsWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback);
    NetNoTlsWebSocketsServer(const NetNoTlsWebSocketsServer&) = delete;
    NetNoTlsWebSocketsServer(NetNoTlsWebSocketsServer&&) noexcept = delete;
    auto operator=(const NetNoTlsWebSocketsServer&) = delete;
    auto operator=(NetNoTlsWebSocketsServer&&) noexcept = delete;
    ~NetNoTlsWebSocketsServer() override = default;

    void Shutdown() override;

private:
    void Run();
    void OnOpen(websocketpp::connection_hdl hdl);
    auto OnValidate(websocketpp::connection_hdl hdl) -> bool;

    ServerNetworkSettings& _settings;
    ConnectionCallback _connectionCallback {};
    web_sockets_no_tls _server {};
    std::thread _runThread {};
};

class NetTlsWebSocketsServer : public NetServerBase
{
public:
    NetTlsWebSocketsServer() = delete;
    NetTlsWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback);
    NetTlsWebSocketsServer(const NetTlsWebSocketsServer&) = delete;
    NetTlsWebSocketsServer(NetTlsWebSocketsServer&&) noexcept = delete;
    auto operator=(const NetTlsWebSocketsServer&) = delete;
    auto operator=(NetTlsWebSocketsServer&&) noexcept = delete;
    ~NetTlsWebSocketsServer() override = default;

    void Shutdown() override;

private:
    void Run();
    void OnOpen(websocketpp::connection_hdl hdl);
    auto OnValidate(websocketpp::connection_hdl hdl) -> bool;
    auto OnTlsInit(const websocketpp::connection_hdl& hdl) const -> shared_ptr<ssl_context>;

    ServerNetworkSettings& _settings;
    ConnectionCallback _connectionCallback {};
    web_sockets_tls _server {};
    std::thread _runThread {};
};

class NetConnectionImpl : public NetConnection
{
public:
    explicit NetConnectionImpl(ServerNetworkSettings& settings) : _settings {settings}
    {
        IsDisconnected = false;
        DisconnectTick = 0;
        _zStream = nullptr;
        std::memset(_outBuf, 0, sizeof(_outBuf));

        if (!settings.DisableZlibCompression) {
            _zStream = new z_stream();
            _zStream->zalloc = [](void*, unsigned int items, unsigned int size) { return calloc(items, size); };
            _zStream->zfree = [](void*, void* address) { free(address); };
            _zStream->opaque = nullptr;

            const auto result = deflateInit(_zStream, Z_BEST_SPEED);
            RUNTIME_ASSERT(result == Z_OK);
        }
    }

    NetConnectionImpl() = delete;
    NetConnectionImpl(const NetConnectionImpl&) = delete;
    NetConnectionImpl(NetConnectionImpl&&) noexcept = delete;
    auto operator=(const NetConnectionImpl&) = delete;
    auto operator=(NetConnectionImpl&&) noexcept = delete;

    ~NetConnectionImpl() override
    {
        if (_zStream != nullptr) {
            deflateEnd(_zStream);
            delete _zStream;
        }
    }

    void DisableCompression() override
    {
        if (_zStream != nullptr) {
            deflateEnd(_zStream);
            delete _zStream;
        }
    }

    void Dispatch() override
    {
        if (IsDisconnected) {
            return;
        }

        // Nothing to send
        {
            SCOPE_LOCK(BoutLocker);
            if (Bout.IsEmpty()) {
                return;
            }
        }

        DispatchImpl();
    }

    void Disconnect() override
    {
        if (IsDisconnected) {
            return;
        }

        IsDisconnected = true;
        if (DisconnectTick == 0u) {
            DisconnectTick = static_cast<uint>(Timer::RealtimeTick());
        }

        DisconnectImpl();
    }

protected:
    virtual void DispatchImpl() = 0;
    virtual void DisconnectImpl() = 0;

    auto SendCallback(uint& out_len) -> const uchar*
    {
        SCOPE_LOCK(BoutLocker);

        if (Bout.IsEmpty()) {
            return nullptr;
        }

        // Compress
        if (_zStream != nullptr) {
            auto to_compr = Bout.GetEndPos();
            if (to_compr > sizeof(_outBuf) - 32) {
                to_compr = sizeof(_outBuf) - 32;
            }

            _zStream->next_in = Bout.GetCurData();
            _zStream->avail_in = to_compr;
            _zStream->next_out = _outBuf;
            _zStream->avail_out = sizeof(_outBuf);

            const auto result = deflate(_zStream, Z_SYNC_FLUSH);
            RUNTIME_ASSERT(result == Z_OK);

            const auto compr = static_cast<uint>(_zStream->next_out - _outBuf);
            const auto real = static_cast<uint>(_zStream->next_in - Bout.GetCurData());
            out_len = compr;

            Bout.Cut(real);
        }
        // Without compressing
        else {
            auto len = Bout.GetEndPos();
            if (len > sizeof(_outBuf)) {
                len = sizeof(_outBuf);
            }
            std::memcpy(_outBuf, Bout.GetCurData(), len);
            out_len = len;
            Bout.Cut(len);
        }

        // Normalize buffer size
        if (Bout.IsEmpty()) {
            Bout.Reset();
        }

        RUNTIME_ASSERT(out_len > 0);
        return _outBuf;
    }

    void ReceiveCallback(const uchar* buf, uint len)
    {
        SCOPE_LOCK(BinLocker);

        if (Bin.GetCurPos() + len < _settings.FloodSize) {
            Bin.Push(buf, len, true);
        }
        else {
            Bin.Reset();
            Disconnect();
        }
    }

    ServerNetworkSettings& _settings;

private:
    z_stream* _zStream {};
    uchar _outBuf[NetBuffer::DEFAULT_BUF_SIZE] {};
};

class NetConnectionAsio final : public NetConnectionImpl
{
public:
    NetConnectionAsio(ServerNetworkSettings& settings, asio::ip::tcp::socket* socket) : NetConnectionImpl(settings), _socket {socket}
    {
        const auto& address = socket->remote_endpoint().address();
        Ip = address.is_v4() ? address.to_v4().to_ulong() : static_cast<uint>(-1);
        Host = address.to_string();
        Port = socket->remote_endpoint().port();

        if (settings.DisableTcpNagle) {
            socket->set_option(asio::ip::tcp::no_delay(true), _dummyError);
        }

        std::memset(_inBuf, 0, sizeof(_inBuf));
        _writePending = false;
        NextAsyncRead();
    }

    NetConnectionAsio() = delete;
    NetConnectionAsio(const NetConnectionAsio&) = delete;
    NetConnectionAsio(NetConnectionAsio&&) noexcept = delete;
    auto operator=(const NetConnectionAsio&) = delete;
    auto operator=(NetConnectionAsio&&) noexcept = delete;
    ~NetConnectionAsio() override { delete _socket; }

private:
    void NextAsyncRead() { async_read(*_socket, asio::buffer(_inBuf), asio::transfer_at_least(1), std::bind(&NetConnectionAsio::AsyncRead, this, std::placeholders::_1, std::placeholders::_2)); }

    void AsyncRead(std::error_code error, size_t bytes)
    {
        if (!error) {
            ReceiveCallback(_inBuf, static_cast<uint>(bytes));
            NextAsyncRead();
        }
        else {
            Disconnect();
        }
    }

    void AsyncWrite(std::error_code error, size_t /*bytes*/)
    {
        _writePending = false;

        if (!error) {
            DispatchImpl();
        }
        else {
            Disconnect();
        }
    }

    void DispatchImpl() override
    {
        auto expected = false;
        if (_writePending.compare_exchange_strong(expected, true)) {
            uint len = 0;
            const auto* buf = SendCallback(len);
            if (buf != nullptr) {
                async_write(*_socket, asio::buffer(buf, len), std::bind(&NetConnectionAsio::AsyncWrite, this, std::placeholders::_1, std::placeholders::_2));
            }
            else {
                if (IsDisconnected) {
                    _socket->shutdown(asio::ip::tcp::socket::shutdown_both, _dummyError);
                    _socket->close(_dummyError);
                }

                _writePending = false;
            }
        }
    }

    void DisconnectImpl() override
    {
        _socket->shutdown(asio::ip::tcp::socket::shutdown_both, _dummyError);
        _socket->close(_dummyError);
    }

    asio::ip::tcp::socket* _socket {};
    std::atomic_bool _writePending {};
    uchar _inBuf[NetBuffer::DEFAULT_BUF_SIZE] {};
    asio::error_code _dummyError {};
};

template<typename WebSockets>
class NetConnectionWebSocket final : public NetConnectionImpl
{
    using connection_ptr = typename WebSockets::connection_ptr;
    using message_ptr = typename WebSockets::message_ptr;

public:
    NetConnectionWebSocket(ServerNetworkSettings& settings, WebSockets* server, connection_ptr connection) : NetConnectionImpl(settings), _server {server}, _connection {connection}
    {
        const auto& address = connection->get_raw_socket().remote_endpoint().address();
        Ip = address.is_v4() ? address.to_v4().to_ulong() : static_cast<uint>(-1);
        Host = address.to_string();
        Port = connection->get_raw_socket().remote_endpoint().port();

        if (settings.DisableTcpNagle) {
            asio::error_code error;
            connection->get_raw_socket().set_option(asio::ip::tcp::no_delay(true), error);
        }

        connection->set_message_handler(websocketpp::lib::bind(&NetConnectionWebSocket::OnMessage, this, websocketpp::lib::placeholders::_2));
        connection->set_fail_handler(websocketpp::lib::bind(&NetConnectionWebSocket::OnFail, this));
        connection->set_close_handler(websocketpp::lib::bind(&NetConnectionWebSocket::OnClose, this));
        connection->set_http_handler(websocketpp::lib::bind(&NetConnectionWebSocket::OnHttp, this));
    }

    NetConnectionWebSocket() = delete;
    NetConnectionWebSocket(const NetConnectionWebSocket&) = delete;
    NetConnectionWebSocket(NetConnectionWebSocket&&) noexcept = delete;
    auto operator=(const NetConnectionWebSocket&) = delete;
    auto operator=(NetConnectionWebSocket&&) noexcept = delete;
    ~NetConnectionWebSocket() override = default;

private:
    void OnMessage(message_ptr msg)
    {
        const string& payload = msg->get_payload();
        RUNTIME_ASSERT(!payload.empty());
        ReceiveCallback(reinterpret_cast<const uchar*>(payload.c_str()), static_cast<uint>(payload.length()));
    }

    void OnFail()
    {
        WriteLog("Fail: {}.\n", _connection->get_ec().message());
        Disconnect();
    }

    void OnClose() { Disconnect(); }

    void OnHttp()
    {
        // Prevent use this feature
        Disconnect();
    }

    void DispatchImpl() override
    {
        uint len = 0;
        auto buf = SendCallback(len);
        if (buf != nullptr) {
            const std::error_code error = _connection->send(buf, len, websocketpp::frame::opcode::binary);
            if (!error) {
                DispatchImpl();
            }
            else {
                Disconnect();
            }
        }
    }

    void DisconnectImpl() override
    {
        std::error_code error;
        _connection->terminate(error);
    }

    WebSockets* _server {};
    connection_ptr _connection {};
};

NetTcpServer::NetTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback) : _settings {settings}, _acceptor(_ioService, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), static_cast<ushort>(settings.Port)))
{
    _connectionCallback = std::move(callback);
    AcceptNext();
    _runThread = std::thread(&NetTcpServer::Run, this);
}

void NetTcpServer::Shutdown()
{
    _ioService.stop();
    _runThread.join();
}

void NetTcpServer::Run()
{
    asio::error_code error;
    _ioService.run(error);
}

void NetTcpServer::AcceptNext()
{
    auto* socket = new asio::ip::tcp::socket(_ioService);
    _acceptor.async_accept(*socket, std::bind(&NetTcpServer::AcceptConnection, this, std::placeholders::_1, socket));
}

void NetTcpServer::AcceptConnection(std::error_code error, asio::ip::tcp::socket* socket)
{
    if (!error) {
        _connectionCallback(new NetConnectionAsio(_settings, socket));
    }
    else {
        WriteLog("Accept error: {}.\n", error.message());
        delete socket;
    }

    AcceptNext();
}

NetNoTlsWebSocketsServer::NetNoTlsWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback) : _settings {settings}
{
    _connectionCallback = std::move(callback);

    _server.init_asio();
    _server.set_open_handler(websocketpp::lib::bind(&NetNoTlsWebSocketsServer::OnOpen, this, websocketpp::lib::placeholders::_1));
    _server.set_validate_handler(websocketpp::lib::bind(&NetNoTlsWebSocketsServer::OnValidate, this, websocketpp::lib::placeholders::_1));
    _server.listen(asio::ip::tcp::v6(), static_cast<uint16_t>(settings.Port + 1));
    _server.start_accept();

    _runThread = std::thread(&NetNoTlsWebSocketsServer::Run, this);
}

void NetNoTlsWebSocketsServer::Shutdown()
{
    _server.stop();
    _runThread.join();
}

void NetNoTlsWebSocketsServer::Run()
{
    _server.run();
}

void NetNoTlsWebSocketsServer::OnOpen(websocketpp::connection_hdl hdl)
{
    const auto connection = _server.get_con_from_hdl(std::move(hdl));
    _connectionCallback(new NetConnectionWebSocket<web_sockets_no_tls>(_settings, &_server, connection));
}

auto NetNoTlsWebSocketsServer::OnValidate(websocketpp::connection_hdl hdl) -> bool
{
    auto connection = _server.get_con_from_hdl(std::move(hdl));
    std::error_code error;
    connection->select_subprotocol("binary", error);
    return !error;
}

NetTlsWebSocketsServer::NetTlsWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback) : _settings {settings}
{
    if (settings.WssPrivateKey.empty()) {
        throw GenericException("'WssPrivateKey' not provided");
    }
    if (settings.WssCertificate.empty()) {
        throw GenericException("'WssCertificate' not provided");
    }

    _connectionCallback = std::move(callback);

    _server.init_asio();
    _server.set_open_handler(websocketpp::lib::bind(&NetTlsWebSocketsServer::OnOpen, this, websocketpp::lib::placeholders::_1));
    _server.set_validate_handler(websocketpp::lib::bind(&NetTlsWebSocketsServer::OnValidate, this, websocketpp::lib::placeholders::_1));
    _server.set_tls_init_handler(websocketpp::lib::bind(&NetTlsWebSocketsServer::OnTlsInit, this, websocketpp::lib::placeholders::_1));
    _server.listen(asio::ip::tcp::v6(), static_cast<uint16_t>(settings.Port + 1));
    _server.start_accept();

    _runThread = std::thread(&NetTlsWebSocketsServer::Run, this);
}

void NetTlsWebSocketsServer::Shutdown()
{
    _server.stop();
    _runThread.join();
}

void NetTlsWebSocketsServer::Run()
{
    _server.run();
}

void NetTlsWebSocketsServer::OnOpen(websocketpp::connection_hdl hdl)
{
    const auto connection = _server.get_con_from_hdl(std::move(hdl));
    _connectionCallback(new NetConnectionWebSocket<web_sockets_tls>(_settings, &_server, connection));
}

auto NetTlsWebSocketsServer::OnValidate(websocketpp::connection_hdl hdl) -> bool
{
    auto connection = _server.get_con_from_hdl(std::move(hdl));
    std::error_code error;
    connection->select_subprotocol("binary", error);
    return !error;
}

auto NetTlsWebSocketsServer::OnTlsInit(const websocketpp::connection_hdl& /*hdl*/) const -> shared_ptr<ssl_context>
{
    shared_ptr<ssl_context> ctx(new ssl_context(ssl_context::tlsv1));
    ctx->set_options(ssl_context::default_workarounds | ssl_context::no_sslv2 | ssl_context::no_sslv3 | ssl_context::single_dh_use);
    ctx->use_certificate_chain_file(_settings.WssCertificate);
    ctx->use_private_key_file(_settings.WssPrivateKey, ssl_context::pem);
    return ctx;
}

auto NetServerBase::StartTcpServer(ServerNetworkSettings& settings, const ConnectionCallback& callback) -> NetServerBase*
{
    try {
        return new NetTcpServer(settings, callback);
    }
    catch (const std::exception& ex) {
        WriteLog("Can't start Tcp server: {}.\n", ex.what());
        return nullptr;
    }
}

auto NetServerBase::StartWebSocketsServer(ServerNetworkSettings& settings, const ConnectionCallback& callback) -> NetServerBase*
{
    try {
        if (!settings.SecuredWebSockets) {
            return new NetNoTlsWebSocketsServer(settings, callback);
        }

        return new NetTlsWebSocketsServer(settings, callback);
    }
    catch (const std::exception& ex) {
        WriteLog("Can't start Web sockets server: {}.\n", ex.what());
        return nullptr;
    }
}
