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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Application.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Timer.h"

#if FO_HAVE_ASIO
#ifdef __clang__
#pragma clang diagnostic ignored "-Wreorder-ctor"
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#define ASIO_STANDALONE 1
#define _WIN32_WINNT 0x0601
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
#endif

#include "zlib.h"

#if FO_HAVE_ASIO
#include "WinApi-Include.h" // After all because ASIO using WinAPI
#endif

NetConnection::NetConnection(ServerNetworkSettings& settings) : Bin(settings.NetBufferSize), Bout(settings.NetBufferSize)
{
}

void NetConnection::AddRef() const
{
    ++_refCount;
}

void NetConnection::Release() const
{
    if (--_refCount == 0) {
        delete this;
    }
}

class NetConnectionImpl : public NetConnection
{
public:
    explicit NetConnectionImpl(ServerNetworkSettings& settings) : NetConnection(settings), _settings {settings}
    {
        _outBuf.resize(_settings.NetBufferSize);

        if (!settings.DisableZlibCompression) {
            _zStream.zalloc = [](void*, unsigned int items, unsigned int size) { return std::calloc(items, size); };
            _zStream.zfree = [](void*, void* address) { std::free(address); };
            _zStream.opaque = nullptr;

            const auto result = deflateInit(&_zStream, Z_BEST_SPEED);
            RUNTIME_ASSERT(result == Z_OK);

            _zStreamActive = true;
        }
    }

    NetConnectionImpl() = delete;
    NetConnectionImpl(const NetConnectionImpl&) = delete;
    NetConnectionImpl(NetConnectionImpl&&) noexcept = delete;
    auto operator=(const NetConnectionImpl&) = delete;
    auto operator=(NetConnectionImpl&&) noexcept = delete;

    ~NetConnectionImpl() override
    {
        if (_zStreamActive) {
            deflateEnd(&_zStream);
        }
    }

    [[nodiscard]] auto GetIp() const -> uint override { return _ip; }
    [[nodiscard]] auto GetHost() const -> string_view override { return _host; }
    [[nodiscard]] auto GetPort() const -> ushort override { return _port; }
    [[nodiscard]] auto IsDisconnected() const -> bool override { return _isDisconnected; }

    void DisableCompression() override
    {
        if (_zStreamActive) {
            deflateEnd(&_zStream);
            _zStreamActive = false;
        }
    }

    void Dispatch() override
    {
        if (_isDisconnected) {
            return;
        }

        // Nothing to send
        {
            std::lock_guard locker(BoutLocker);
            if (Bout.IsEmpty()) {
                return;
            }
        }

        DispatchImpl();
    }

    void Disconnect() override
    {
        auto expected = false;
        if (_isDisconnected.compare_exchange_strong(expected, true)) {
            DisconnectImpl();
        }
    }

protected:
    virtual void DispatchImpl() = 0;
    virtual void DisconnectImpl() = 0;

    auto SendCallback(size_t& out_len) -> const uchar*
    {
        std::lock_guard locker(BoutLocker);

        if (Bout.IsEmpty()) {
            return nullptr;
        }

        // Compress
        if (_zStreamActive) {
            const auto to_compr = Bout.GetEndPos();

            _outBuf.resize(to_compr + 32);

            if (_outBuf.size() <= _settings.NetBufferSize && _outBuf.capacity() > _settings.NetBufferSize) {
                _outBuf.shrink_to_fit();
            }

            _zStream.next_in = static_cast<Bytef*>(Bout.GetData());
            _zStream.avail_in = static_cast<uInt>(to_compr);
            _zStream.next_out = static_cast<Bytef*>(_outBuf.data());
            _zStream.avail_out = static_cast<uInt>(_outBuf.size());

            const auto result = deflate(&_zStream, Z_SYNC_FLUSH);
            RUNTIME_ASSERT(result == Z_OK);

            const auto compr = static_cast<size_t>(_zStream.next_out - _outBuf.data());
            const auto real = static_cast<size_t>(_zStream.next_in - Bout.GetData());
            out_len = compr;

            Bout.Cut(real);
        }
        // Without compressing
        else {
            const auto len = Bout.GetEndPos();

            _outBuf.resize(len);

            if (_outBuf.size() <= _settings.NetBufferSize && _outBuf.capacity() > _settings.NetBufferSize) {
                _outBuf.shrink_to_fit();
            }

            std::memcpy(_outBuf.data(), Bout.GetData(), len);
            out_len = len;

            Bout.Cut(len);
        }

        // Normalize buffer size
        if (Bout.IsEmpty()) {
            Bout.ResetBuf();
        }

        RUNTIME_ASSERT(out_len > 0);
        return _outBuf.data();
    }

    void ReceiveCallback(const uchar* buf, size_t len)
    {
        std::lock_guard locker(BinLocker);

        if (Bin.GetReadPos() + len < _settings.FloodSize) {
            Bin.AddData(buf, len);
        }
        else {
            Bin.ResetBuf();
            Disconnect();
        }
    }

    ServerNetworkSettings& _settings;
    uint _ip {};
    string _host {};
    ushort _port {};
    std::atomic_bool _isDisconnected {};

private:
    bool _zStreamActive {};
    z_stream _zStream {};
    vector<uchar> _outBuf {};
};

#if FO_HAVE_ASIO
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
    asio::io_context _ioService {};
    asio::ip::tcp::acceptor _acceptor;
    ConnectionCallback _connectionCallback {};
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
    auto OnTlsInit(websocketpp::connection_hdl hdl) const -> shared_ptr<ssl_context>;

    ServerNetworkSettings& _settings;
    ConnectionCallback _connectionCallback {};
    web_sockets_tls _server {};
    std::thread _runThread {};
};

class NetConnectionAsio final : public NetConnectionImpl
{
public:
    NetConnectionAsio(ServerNetworkSettings& settings, asio::ip::tcp::socket* socket) : NetConnectionImpl(settings), _socket {socket}
    {
        const auto& address = socket->remote_endpoint().address();
        _ip = address.is_v4() ? address.to_v4().to_uint() : static_cast<uint>(-1);
        _host = address.to_string();
        _port = socket->remote_endpoint().port();

        if (settings.DisableTcpNagle) {
            socket->set_option(asio::ip::tcp::no_delay(true), _dummyError);
        }

        _inBuf.resize(_settings.NetBufferSize);
        _writePending = false;
        NextAsyncRead();
    }

    NetConnectionAsio() = delete;
    NetConnectionAsio(const NetConnectionAsio&) = delete;
    NetConnectionAsio(NetConnectionAsio&&) noexcept = delete;
    auto operator=(const NetConnectionAsio&) = delete;
    auto operator=(NetConnectionAsio&&) noexcept = delete;
    ~NetConnectionAsio() override
    {
        if (!_isDisconnected) {
            _socket->shutdown(asio::ip::tcp::socket::shutdown_both, _dummyError);
            _socket->close(_dummyError);
        }
    }

    [[nodiscard]] auto IsWebConnection() const -> bool override { return false; }
    [[nodiscard]] auto IsInterthreadConnection() const -> bool override { return false; }

private:
    void NextAsyncRead()
    {
        async_read(*_socket, asio::buffer(_inBuf), asio::transfer_at_least(1), [this_ = this, socket = _socket](std::error_code error, size_t bytes) { AsyncRead(this_, socket, error, bytes); });
    }

    static void AsyncRead(NetConnectionAsio* this_, asio::ip::tcp::socket* socket, std::error_code error, size_t bytes)
    {
        if (!error) {
            this_->ReceiveCallback(this_->_inBuf.data(), bytes);
            this_->NextAsyncRead();
        }
        else {
            if (socket->is_open()) {
                this_->Disconnect();
            }

            delete socket;
        }
    }

    void AsyncWrite(std::error_code error, size_t bytes)
    {
        UNUSED_VARIABLE(bytes);

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
            size_t len = 0;
            const auto* buf = SendCallback(len);
            if (buf != nullptr) {
                async_write(*_socket, asio::buffer(buf, len), [this](std::error_code error, size_t bytes) { AsyncWrite(error, bytes); });
            }
            else {
                if (_isDisconnected) {
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
    vector<uchar> _inBuf {};
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
        _ip = address.is_v4() ? address.to_v4().to_ulong() : static_cast<uint>(-1);
        _host = address.to_string();
        _port = connection->get_raw_socket().remote_endpoint().port();

        if (settings.DisableTcpNagle) {
            asio::error_code error;
            connection->get_raw_socket().set_option(asio::ip::tcp::no_delay(true), error);
        }

        connection->set_message_handler([this](auto&&, message_ptr msg) { OnMessage(msg); });
        connection->set_fail_handler([this](websocketpp::connection_hdl) { OnFail(); });
        connection->set_close_handler([this](websocketpp::connection_hdl) { OnClose(); });
        connection->set_http_handler([this](websocketpp::connection_hdl) { OnHttp(); });
    }

    NetConnectionWebSocket() = delete;
    NetConnectionWebSocket(const NetConnectionWebSocket&) = delete;
    NetConnectionWebSocket(NetConnectionWebSocket&&) noexcept = delete;
    auto operator=(const NetConnectionWebSocket&) = delete;
    auto operator=(NetConnectionWebSocket&&) noexcept = delete;

    ~NetConnectionWebSocket() override
    {
        if (_isDisconnected) {
            std::error_code error;
            _connection->terminate(error);
        }
    }

    [[nodiscard]] auto IsWebConnection() const -> bool override { return true; }
    [[nodiscard]] auto IsInterthreadConnection() const -> bool override { return false; }

private:
    void OnMessage(message_ptr msg)
    {
        const auto& payload = msg->get_payload();
        RUNTIME_ASSERT(!payload.empty());
        ReceiveCallback(reinterpret_cast<const uchar*>(payload.data()), payload.length());
    }

    void OnFail()
    {
        WriteLog("Failed: {}", _connection->get_ec().message());
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
        size_t len = 0;
        const auto* buf = SendCallback(len);
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

NetTcpServer::NetTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback) : _settings {settings}, _acceptor(_ioService, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), static_cast<ushort>(settings.ServerPort)))
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
    try {
        _ioService.run();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
}

void NetTcpServer::AcceptNext()
{
    auto* socket = new asio::ip::tcp::socket(_ioService);
    _acceptor.async_accept(*socket, [this, socket](std::error_code error) { AcceptConnection(error, socket); });
}

void NetTcpServer::AcceptConnection(std::error_code error, asio::ip::tcp::socket* socket)
{
    if (!error) {
        _connectionCallback(new NetConnectionAsio(_settings, socket));
    }
    else {
        WriteLog("Accept error: {}", error.message());
        delete socket;
    }

    AcceptNext();
}

NetNoTlsWebSocketsServer::NetNoTlsWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback) : _settings {settings}
{
    _connectionCallback = std::move(callback);

    _server.init_asio();
    _server.set_open_handler([this](websocketpp::connection_hdl hdl) { OnOpen(hdl); });
    _server.set_validate_handler([this](websocketpp::connection_hdl hdl) { return OnValidate(hdl); });
    _server.listen(asio::ip::tcp::v6(), static_cast<uint16_t>(settings.ServerPort + 1));
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
    try {
        _server.run();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
}

void NetNoTlsWebSocketsServer::OnOpen(websocketpp::connection_hdl hdl)
{
    const auto connection = _server.get_con_from_hdl(std::move(hdl));
    _connectionCallback(new NetConnectionWebSocket<web_sockets_no_tls>(_settings, &_server, connection));
}

auto NetNoTlsWebSocketsServer::OnValidate(websocketpp::connection_hdl hdl) -> bool
{
    auto&& connection = _server.get_con_from_hdl(std::move(hdl));
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
    _server.set_open_handler([this](websocketpp::connection_hdl hdl) { OnOpen(hdl); });
    _server.set_validate_handler([this](websocketpp::connection_hdl hdl) { return OnValidate(hdl); });
    _server.set_tls_init_handler([this](websocketpp::connection_hdl hdl) { return OnTlsInit(hdl); });
    _server.listen(asio::ip::tcp::v6(), static_cast<uint16_t>(settings.ServerPort + 1));
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
    try {
        _server.run();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
}

void NetTlsWebSocketsServer::OnOpen(websocketpp::connection_hdl hdl)
{
    const auto connection = _server.get_con_from_hdl(std::move(hdl));
    _connectionCallback(new NetConnectionWebSocket<web_sockets_tls>(_settings, &_server, connection));
}

auto NetTlsWebSocketsServer::OnValidate(websocketpp::connection_hdl hdl) -> bool
{
    auto&& connection = _server.get_con_from_hdl(std::move(hdl));
    std::error_code error;
    connection->select_subprotocol("binary", error);
    return !error;
}

auto NetTlsWebSocketsServer::OnTlsInit(const websocketpp::connection_hdl hdl) const -> shared_ptr<ssl_context>
{
    UNUSED_VARIABLE(hdl);

    shared_ptr<ssl_context> ctx(new ssl_context(ssl_context::tlsv1));
    ctx->set_options(ssl_context::default_workarounds | ssl_context::no_sslv2 | ssl_context::no_sslv3 | ssl_context::single_dh_use);
    ctx->use_certificate_chain_file(_settings.WssCertificate);
    ctx->use_private_key_file(_settings.WssPrivateKey, ssl_context::pem);
    return ctx;
}
#endif // FO_HAVE_ASIO

auto NetServerBase::StartTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> NetServerBase*
{
#if FO_HAVE_ASIO
    WriteLog("Listen TCP connections on port {}", settings.ServerPort);

    return new NetTcpServer(settings, std::move(callback));
#else
    throw NotSupportedException("NetServerBase::StartTcpServer");
#endif
}

auto NetServerBase::StartWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> NetServerBase*
{
#if FO_HAVE_ASIO
    if (settings.SecuredWebSockets) {
        WriteLog("Listen WebSockets (with TLS) connections on port {}", settings.ServerPort + 1);

        return new NetTlsWebSocketsServer(settings, std::move(callback));
    }
    else {
        WriteLog("Listen WebSockets (no TLS) connections on port {}", settings.ServerPort + 1);

        return new NetNoTlsWebSocketsServer(settings, std::move(callback));
    }
#else
    throw NotSupportedException("NetServerBase::StartWebSocketsServer");
#endif
}

class InterthreadConnection : public NetConnectionImpl
{
public:
    InterthreadConnection(ServerNetworkSettings& settings, InterthreadDataCallback send) : NetConnectionImpl(settings), _send {std::move(send)} { }

    InterthreadConnection(const InterthreadConnection&) = delete;
    InterthreadConnection(InterthreadConnection&&) noexcept = delete;
    auto operator=(const InterthreadConnection&) = delete;
    auto operator=(InterthreadConnection&&) noexcept = delete;
    ~InterthreadConnection() override = default;

    [[nodiscard]] auto IsWebConnection() const -> bool override { return false; }
    [[nodiscard]] auto IsInterthreadConnection() const -> bool override { return true; }

    void Receive(const_span<uchar> buf)
    {
        if (!buf.empty()) {
            ReceiveCallback(buf.data(), buf.size());
        }
        else {
            _send = nullptr;
            Disconnect();
        }
    }

private:
    void DispatchImpl() override
    {
        size_t len = 0;
        const auto* buf = SendCallback(len);
        if (buf != nullptr) {
            _send({buf, len});
        }
    }

    void DisconnectImpl() override
    {
        if (_send) {
            _send({});
            _send = nullptr;
        }
    }

    InterthreadDataCallback _send;
};

class InterthreadServer : public NetServerBase
{
public:
    InterthreadServer() = delete;
    InterthreadServer(const InterthreadServer&) = delete;
    InterthreadServer(InterthreadServer&&) noexcept = delete;
    auto operator=(const InterthreadServer&) = delete;
    auto operator=(InterthreadServer&&) noexcept = delete;
    ~InterthreadServer() override = default;

    InterthreadServer(ServerNetworkSettings& settings, ConnectionCallback callback) : _virtualPort {static_cast<ushort>(settings.ServerPort)}
    {
        if (InterthreadListeners.count(_virtualPort) != 0u) {
            throw NetworkException("Port is busy", _virtualPort);
        }

        InterthreadListeners.emplace(_virtualPort, [&settings, callback = std::move(callback)](InterthreadDataCallback client_send) -> InterthreadDataCallback {
            auto* conn = new InterthreadConnection(settings, std::move(client_send));
            callback(conn);
            return [conn](const_span<uchar> buf) { conn->Receive(buf); };
        });
    }

    void Shutdown() override
    {
        RUNTIME_ASSERT(InterthreadListeners.count(_virtualPort) != 0u);
        InterthreadListeners.erase(_virtualPort);
    }

private:
    ushort _virtualPort;
};

auto NetServerBase::StartInterthreadServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> NetServerBase*
{
    return new InterthreadServer(settings, std::move(callback));
}
