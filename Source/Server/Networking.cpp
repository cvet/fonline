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

// Todo: catch exceptions in network servers

#include "Networking.h"
#include "Application.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Timer.h"

#if FO_HAVE_ASIO
DISABLE_WARNINGS_PUSH()
#define ASIO_STANDALONE 1
// ReSharper disable once CppInconsistentNaming
#define _WIN32_WINNT 0x0601 // NOLINT(clang-diagnostic-reserved-macro-identifier)
#include "asio.hpp"
// ReSharper disable CppInconsistentNaming
#define _WEBSOCKETPP_CPP11_FUNCTIONAL_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_MEMORY_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_STL_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
// ReSharper restore CppInconsistentNaming
#pragma warning(push)
#pragma warning(disable : 4267)
#include "websocketpp/config/asio.hpp"
#include "websocketpp/server.hpp"
#pragma warning(pop)
using web_sockets_tls = websocketpp::server<websocketpp::config::asio_tls>;
using web_sockets_no_tls = websocketpp::server<websocketpp::config::asio>;
using ssl_context = asio::ssl::context;
DISABLE_WARNINGS_POP()
#endif

#include "zlib.h"

#include "WinApiUndef-Include.h"

NetConnection::NetConnection(ServerNetworkSettings& settings) :
    InBuf(settings.NetBufferSize),
    OutBuf(settings.NetBufferSize)
{
    STACK_TRACE_ENTRY();
}

class NetConnectionImpl : public NetConnection
{
public:
    explicit NetConnectionImpl(ServerNetworkSettings& settings) :
        NetConnection(settings),
        _settings {settings}
    {
        STACK_TRACE_ENTRY();

        _outBuf.resize(_settings.NetBufferSize);

        if (!settings.DisableZlibCompression) {
            _zStream = {};
            _zStream.zalloc = [](voidpf, uInt items, uInt size) -> void* {
                constexpr SafeAllocator<uint8> allocator;
                return allocator.allocate(static_cast<size_t>(items) * size);
            };
            _zStream.zfree = [](voidpf, voidpf address) {
                constexpr SafeAllocator<uint8> allocator;
                allocator.deallocate(static_cast<uint8*>(address), 0);
            };

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
        STACK_TRACE_ENTRY();

        if (_zStreamActive) {
            deflateEnd(&_zStream);
        }
    }

    [[nodiscard]] auto GetIp() const noexcept -> uint override
    {
        NO_STACK_TRACE_ENTRY();

        return _ip;
    }

    [[nodiscard]] auto GetHost() const noexcept -> string_view override
    {
        NO_STACK_TRACE_ENTRY();

        return _host;
    }

    [[nodiscard]] auto GetPort() const noexcept -> uint16 override
    {
        NO_STACK_TRACE_ENTRY();

        return _port;
    }

    [[nodiscard]] auto IsDisconnected() const noexcept -> bool override
    {
        NO_STACK_TRACE_ENTRY();

        return _isDisconnected;
    }

    void DisableCompression() override
    {
        STACK_TRACE_ENTRY();

        if (_zStreamActive) {
            deflateEnd(&_zStream);
            _zStreamActive = false;
        }
    }

    void Dispatch() override
    {
        STACK_TRACE_ENTRY();

        if (_isDisconnected) {
            return;
        }

        // Nothing to send
        {
            std::scoped_lock locker(OutBufLocker);

            if (OutBuf.IsEmpty()) {
                return;
            }
        }

        DispatchImpl();
    }

    void Disconnect() override
    {
        STACK_TRACE_ENTRY();

        bool expected = false;

        if (_isDisconnected.compare_exchange_strong(expected, true)) {
            DisconnectImpl();
        }
    }

protected:
    virtual void DispatchImpl() = 0;
    virtual void DisconnectImpl() = 0;

    auto SendCallback() -> const_span<uint8>
    {
        STACK_TRACE_ENTRY();

        std::scoped_lock locker(OutBufLocker);

        if (OutBuf.IsEmpty()) {
            return {};
        }

        size_t out_len;

        // Compress
        if (_zStreamActive) {
            const auto to_compr = OutBuf.GetEndPos();

            _outBuf.resize(to_compr + 32);

            if (_outBuf.size() <= _settings.NetBufferSize && _outBuf.capacity() > _settings.NetBufferSize) {
                _outBuf.shrink_to_fit();
            }

            _zStream.next_in = static_cast<Bytef*>(OutBuf.GetData());
            _zStream.avail_in = static_cast<uInt>(to_compr);
            _zStream.next_out = static_cast<Bytef*>(_outBuf.data());
            _zStream.avail_out = static_cast<uInt>(_outBuf.size());

            const auto result = deflate(&_zStream, Z_SYNC_FLUSH);
            RUNTIME_ASSERT(result == Z_OK);

            const auto compr = static_cast<size_t>(_zStream.next_out - _outBuf.data());
            const auto real = static_cast<size_t>(_zStream.next_in - OutBuf.GetData());
            out_len = compr;

            OutBuf.DiscardWriteBuf(real);
        }
        // Without compressing
        else {
            const auto len = OutBuf.GetEndPos();

            _outBuf.resize(len);

            if (_outBuf.size() <= _settings.NetBufferSize && _outBuf.capacity() > _settings.NetBufferSize) {
                _outBuf.shrink_to_fit();
            }

            MemCopy(_outBuf.data(), OutBuf.GetData(), len);
            out_len = len;

            OutBuf.DiscardWriteBuf(len);
        }

        // Normalize buffer size
        if (OutBuf.IsEmpty()) {
            OutBuf.ResetBuf();
        }

        RUNTIME_ASSERT(out_len > 0);
        return {_outBuf.data(), out_len};
    }

    void ReceiveCallback(const uint8* buf, size_t len)
    {
        STACK_TRACE_ENTRY();

        std::scoped_lock locker(InBufLocker);

        if (InBuf.GetReadPos() + len < _settings.FloodSize) {
            InBuf.AddData(buf, len);
        }
        else {
            InBuf.ResetBuf();
            Disconnect();
        }
    }

    ServerNetworkSettings& _settings;
    uint _ip {};
    string _host {};
    uint16 _port {};
    std::atomic_bool _isDisconnected {};

private:
    bool _zStreamActive {};
    z_stream _zStream {};
    vector<uint8> _outBuf {};
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
    void AcceptConnection(std::error_code error, unique_ptr<asio::ip::tcp::socket> socket);

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
    void OnOpen(const websocketpp::connection_hdl& hdl);
    auto OnValidate(const websocketpp::connection_hdl& hdl) -> bool;

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
    void OnOpen(const websocketpp::connection_hdl& hdl);
    auto OnValidate(const websocketpp::connection_hdl& hdl) -> bool;
    auto OnTlsInit(const websocketpp::connection_hdl& hdl) const -> shared_ptr<ssl_context>;

    ServerNetworkSettings& _settings;
    ConnectionCallback _connectionCallback {};
    web_sockets_tls _server {};
    std::thread _runThread {};
};

class NetConnectionAsio final : public NetConnectionImpl
{
public:
    NetConnectionAsio(ServerNetworkSettings& settings, unique_ptr<asio::ip::tcp::socket> socket) :
        NetConnectionImpl(settings),
        _socket {std::move(socket)}
    {
        STACK_TRACE_ENTRY();

        const auto& address = _socket->remote_endpoint().address();
        _ip = address.is_v4() ? address.to_v4().to_uint() : static_cast<uint>(-1);
        _host = address.to_string();
        _port = _socket->remote_endpoint().port();

        if (settings.DisableTcpNagle) {
            asio::error_code error;
            error = _socket->set_option(asio::ip::tcp::no_delay(true), error);
        }

        _inBuf.resize(_settings.NetBufferSize);
    }

    NetConnectionAsio() = delete;
    NetConnectionAsio(const NetConnectionAsio&) = delete;
    NetConnectionAsio(NetConnectionAsio&&) noexcept = delete;
    auto operator=(const NetConnectionAsio&) = delete;
    auto operator=(NetConnectionAsio&&) noexcept = delete;

    ~NetConnectionAsio() override
    {
        STACK_TRACE_ENTRY();

        try {
            asio::error_code error;
            error = _socket->shutdown(asio::ip::tcp::socket::shutdown_both, error);
            error = _socket->close(error);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }
    }

    [[nodiscard]] auto IsWebConnection() const noexcept -> bool override
    {
        NO_STACK_TRACE_ENTRY();

        return false;
    }

    [[nodiscard]] auto IsInterthreadConnection() const noexcept -> bool override
    {
        NO_STACK_TRACE_ENTRY();

        return false;
    }

    void StartRead()
    {
        STACK_TRACE_ENTRY();

        NextAsyncRead();
    }

private:
    void NextAsyncRead()
    {
        STACK_TRACE_ENTRY();

        const auto read_handler = [thiz = shared_from_this()](std::error_code error, size_t bytes) {
            auto* thiz_ = static_cast<NetConnectionAsio*>(thiz.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            thiz_->AsyncReadComplete(error, bytes);
        };

        async_read(*_socket, asio::buffer(_inBuf), asio::transfer_at_least(1), read_handler);
    }

    void AsyncReadComplete(std::error_code error, size_t bytes)
    {
        STACK_TRACE_ENTRY();

        if (!error) {
            ReceiveCallback(_inBuf.data(), bytes);
            NextAsyncRead();
        }
        else {
            Disconnect();
        }
    }

    void AsyncWriteComplete(std::error_code error, size_t bytes)
    {
        STACK_TRACE_ENTRY();

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
        STACK_TRACE_ENTRY();

        bool expected = false;

        if (_writePending.compare_exchange_strong(expected, true)) {
            const auto buf = SendCallback();

            if (!buf.empty()) {
                const auto write_handler = [thiz = shared_from_this()](std::error_code error, size_t bytes) {
                    auto* thiz_ = static_cast<NetConnectionAsio*>(thiz.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                    thiz_->AsyncWriteComplete(error, bytes);
                };

                async_write(*_socket, asio::buffer(buf.data(), buf.size()), write_handler);
            }
            else {
                _writePending = false;
            }
        }
    }

    void DisconnectImpl() override
    {
        STACK_TRACE_ENTRY();

        asio::error_code error;
        error = _socket->shutdown(asio::ip::tcp::socket::shutdown_both, error);
        error = _socket->close(error);
    }

    unique_ptr<asio::ip::tcp::socket> _socket {};
    std::atomic_bool _writePending {};
    std::vector<uint8> _inBuf {};
};

template<typename WebSockets>
class NetConnectionWebSocket final : public NetConnectionImpl
{
    using connection_ptr = typename WebSockets::connection_ptr;
    using message_ptr = typename WebSockets::message_ptr;

public:
    NetConnectionWebSocket(ServerNetworkSettings& settings, WebSockets* server, connection_ptr connection) :
        NetConnectionImpl(settings),
        _server {server},
        _connection {std::move(connection)}
    {
        STACK_TRACE_ENTRY();

        const auto& address = _connection->get_raw_socket().remote_endpoint().address();
        _ip = address.is_v4() ? address.to_v4().to_ulong() : static_cast<uint>(-1);
        _host = address.to_string();
        _port = _connection->get_raw_socket().remote_endpoint().port();

        if (settings.DisableTcpNagle) {
            asio::error_code error;
            _connection->get_raw_socket().set_option(asio::ip::tcp::no_delay(true), error);
        }

        _connection->set_message_handler([this](auto&&, auto&& msg) { OnMessage(msg); });
        _connection->set_fail_handler([this](auto&& hdl) { OnFail(hdl); });
        _connection->set_close_handler([this](auto&& hdl) { OnClose(hdl); });
        _connection->set_http_handler([this](auto&& hdl) { OnHttp(hdl); });
    }

    NetConnectionWebSocket() = delete;
    NetConnectionWebSocket(const NetConnectionWebSocket&) = delete;
    NetConnectionWebSocket(NetConnectionWebSocket&&) noexcept = delete;
    auto operator=(const NetConnectionWebSocket&) = delete;
    auto operator=(NetConnectionWebSocket&&) noexcept = delete;

    ~NetConnectionWebSocket() override
    {
        STACK_TRACE_ENTRY();

        try {
            std::error_code error;
            _connection->terminate(error);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }
    }

    [[nodiscard]] auto IsWebConnection() const noexcept -> bool override
    {
        NO_STACK_TRACE_ENTRY();

        return true;
    }

    [[nodiscard]] auto IsInterthreadConnection() const noexcept -> bool override
    {
        NO_STACK_TRACE_ENTRY();

        return false;
    }

private:
    void OnMessage(const message_ptr& msg)
    {
        STACK_TRACE_ENTRY();

        const auto& payload = msg->get_payload();

        if (!payload.empty()) {
            ReceiveCallback(reinterpret_cast<const uint8*>(payload.data()), payload.length());
        }
    }

    void OnFail(const websocketpp::connection_hdl& hdl)
    {
        STACK_TRACE_ENTRY();

        UNUSED_VARIABLE(hdl);

        WriteLog("Failed: {}", _connection->get_ec().message());
        Disconnect();
    }

    void OnClose(const websocketpp::connection_hdl& hdl)
    {
        STACK_TRACE_ENTRY();

        UNUSED_VARIABLE(hdl);

        Disconnect();
    }

    void OnHttp(const websocketpp::connection_hdl& hdl)
    {
        STACK_TRACE_ENTRY();

        UNUSED_VARIABLE(hdl);

        // Prevent use this feature
        Disconnect();
    }

    void DispatchImpl() override
    {
        STACK_TRACE_ENTRY();

        const auto buf = SendCallback();

        if (!buf.empty()) {
            const std::error_code error = _connection->send(buf.data(), buf.size(), websocketpp::frame::opcode::binary);

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
        STACK_TRACE_ENTRY();

        std::error_code error;
        _connection->terminate(error);
    }

    WebSockets* _server {};
    connection_ptr _connection {};
};

NetTcpServer::NetTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback) :
    _settings {settings},
    _acceptor(_ioService, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), static_cast<uint16>(settings.ServerPort)))
{
    STACK_TRACE_ENTRY();

    _connectionCallback = std::move(callback);
    AcceptNext();
    _runThread = std::thread(&NetTcpServer::Run, this);
}

void NetTcpServer::Shutdown()
{
    STACK_TRACE_ENTRY();

    _ioService.stop();
    _runThread.join();
}

void NetTcpServer::Run()
{
    STACK_TRACE_ENTRY();

    try {
        _ioService.run();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        UNKNOWN_EXCEPTION();
    }
}

void NetTcpServer::AcceptNext()
{
    STACK_TRACE_ENTRY();

    auto* socket = SafeAlloc::MakeRaw<asio::ip::tcp::socket>(_ioService);
    _acceptor.async_accept(*socket, [this, socket](std::error_code error) { AcceptConnection(error, unique_ptr<asio::ip::tcp::socket>(socket)); });
}

void NetTcpServer::AcceptConnection(std::error_code error, unique_ptr<asio::ip::tcp::socket> socket)
{
    STACK_TRACE_ENTRY();

    if (!error) {
        auto&& connection = SafeAlloc::MakeShared<NetConnectionAsio>(_settings, std::move(socket));
        connection->StartRead(); // shared_from_this() is not available in constructor so StartRead/NextAsyncRead is called after
        _connectionCallback(std::move(connection));
    }
    else {
        WriteLog("Accept error: {}", error.message());
    }

    AcceptNext();
}

NetNoTlsWebSocketsServer::NetNoTlsWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback) :
    _settings {settings}
{
    STACK_TRACE_ENTRY();

    _connectionCallback = std::move(callback);

    _server.init_asio();
    _server.set_open_handler([this](auto&& hdl) { OnOpen(hdl); });
    _server.set_validate_handler([this](auto&& hdl) { return OnValidate(hdl); });
    _server.listen(asio::ip::tcp::v6(), static_cast<uint16>(settings.ServerPort + 1));
    _server.start_accept();

    _runThread = std::thread(&NetNoTlsWebSocketsServer::Run, this);
}

void NetNoTlsWebSocketsServer::Shutdown()
{
    STACK_TRACE_ENTRY();

    _server.stop();
    _runThread.join();
}

void NetNoTlsWebSocketsServer::Run()
{
    STACK_TRACE_ENTRY();

    try {
        _server.run();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        UNKNOWN_EXCEPTION();
    }
}

void NetNoTlsWebSocketsServer::OnOpen(const websocketpp::connection_hdl& hdl)
{
    STACK_TRACE_ENTRY();

    std::error_code error;
    auto&& connection = _server.get_con_from_hdl(hdl, error);

    if (!error) {
        _connectionCallback(SafeAlloc::MakeShared<NetConnectionWebSocket<web_sockets_no_tls>>(_settings, &_server, std::move(connection)));
    }
}

auto NetNoTlsWebSocketsServer::OnValidate(const websocketpp::connection_hdl& hdl) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code error;
    auto&& connection = _server.get_con_from_hdl(hdl, error);

    if (error) {
        return false;
    }

    connection->select_subprotocol("binary", error);
    return !error;
}

NetTlsWebSocketsServer::NetTlsWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback) :
    _settings {settings}
{
    STACK_TRACE_ENTRY();

    if (settings.WssPrivateKey.empty()) {
        throw GenericException("'WssPrivateKey' not provided");
    }
    if (settings.WssCertificate.empty()) {
        throw GenericException("'WssCertificate' not provided");
    }

    _connectionCallback = std::move(callback);

    _server.init_asio();
    _server.set_open_handler([this](auto&& hdl) { OnOpen(hdl); });
    _server.set_validate_handler([this](auto&& hdl) { return OnValidate(hdl); });
    _server.set_tls_init_handler([this](auto&& hdl) { return OnTlsInit(hdl); });
    _server.listen(asio::ip::tcp::v6(), static_cast<uint16>(settings.ServerPort + 1));
    _server.start_accept();

    _runThread = std::thread(&NetTlsWebSocketsServer::Run, this);
}

void NetTlsWebSocketsServer::Shutdown()
{
    STACK_TRACE_ENTRY();

    _server.stop();
    _runThread.join();
}

void NetTlsWebSocketsServer::Run()
{
    STACK_TRACE_ENTRY();

    try {
        _server.run();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        UNKNOWN_EXCEPTION();
    }
}

void NetTlsWebSocketsServer::OnOpen(const websocketpp::connection_hdl& hdl)
{
    STACK_TRACE_ENTRY();

    std::error_code error;
    auto&& connection = _server.get_con_from_hdl(hdl, error);

    if (!error) {
        _connectionCallback(SafeAlloc::MakeShared<NetConnectionWebSocket<web_sockets_tls>>(_settings, &_server, std::move(connection)));
    }
}

auto NetTlsWebSocketsServer::OnValidate(const websocketpp::connection_hdl& hdl) -> bool
{
    STACK_TRACE_ENTRY();

    std::error_code error;
    auto&& connection = _server.get_con_from_hdl(hdl, error);

    if (error) {
        return false;
    }

    connection->select_subprotocol("binary", error);
    return !error;
}

auto NetTlsWebSocketsServer::OnTlsInit(const websocketpp::connection_hdl& hdl) const -> shared_ptr<ssl_context>
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(hdl);

    auto&& ctx = SafeAlloc::MakeShared<ssl_context>(ssl_context::tlsv1);
    ctx->set_options(ssl_context::default_workarounds | ssl_context::no_sslv2 | ssl_context::no_sslv3 | ssl_context::single_dh_use);
    ctx->use_certificate_chain_file(std::string(_settings.WssCertificate));
    ctx->use_private_key_file(std::string(_settings.WssPrivateKey), ssl_context::pem);
    return ctx;
}
#endif // FO_HAVE_ASIO

auto NetServerBase::StartTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> unique_ptr<NetServerBase>
{
    STACK_TRACE_ENTRY();

#if FO_HAVE_ASIO
    WriteLog("Listen TCP connections on port {}", settings.ServerPort);

    return SafeAlloc::MakeUnique<NetTcpServer>(settings, std::move(callback));
#else
    throw NotSupportedException("NetServerBase::StartTcpServer");
#endif
}

auto NetServerBase::StartWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> unique_ptr<NetServerBase>
{
    STACK_TRACE_ENTRY();

#if FO_HAVE_ASIO
    if (settings.SecuredWebSockets) {
        WriteLog("Listen WebSockets (with TLS) connections on port {}", settings.ServerPort + 1);

        return SafeAlloc::MakeUnique<NetTlsWebSocketsServer>(settings, std::move(callback));
    }
    else {
        WriteLog("Listen WebSockets (no TLS) connections on port {}", settings.ServerPort + 1);

        return SafeAlloc::MakeUnique<NetNoTlsWebSocketsServer>(settings, std::move(callback));
    }
#else
    throw NotSupportedException("NetServerBase::StartWebSocketsServer");
#endif
}

class InterthreadConnection : public NetConnectionImpl
{
public:
    InterthreadConnection(ServerNetworkSettings& settings, InterthreadDataCallback send) :
        NetConnectionImpl(settings),
        _send {std::move(send)}
    {
        STACK_TRACE_ENTRY();

        //
    }

    InterthreadConnection(const InterthreadConnection&) = delete;
    InterthreadConnection(InterthreadConnection&&) noexcept = delete;
    auto operator=(const InterthreadConnection&) = delete;
    auto operator=(InterthreadConnection&&) noexcept = delete;
    ~InterthreadConnection() override = default;

    [[nodiscard]] auto IsWebConnection() const noexcept -> bool override
    {
        NO_STACK_TRACE_ENTRY();

        return false;
    }

    [[nodiscard]] auto IsInterthreadConnection() const noexcept -> bool override
    {
        NO_STACK_TRACE_ENTRY();

        return true;
    }

    void Receive(const_span<uint8> buf)
    {
        STACK_TRACE_ENTRY();

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
        STACK_TRACE_ENTRY();

        const auto buf = SendCallback();

        if (!buf.empty()) {
            _send(buf);
        }
    }

    void DisconnectImpl() override
    {
        STACK_TRACE_ENTRY();

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

    InterthreadServer(ServerNetworkSettings& settings, ConnectionCallback callback) :
        _virtualPort {static_cast<uint16>(settings.ServerPort)}
    {
        STACK_TRACE_ENTRY();

        if (InterthreadListeners.count(_virtualPort) != 0u) {
            throw NetworkException("Port is busy", _virtualPort);
        }

        InterthreadListeners.emplace(_virtualPort, [&settings, callback_ = std::move(callback)](InterthreadDataCallback client_send) -> InterthreadDataCallback {
            auto conn = SafeAlloc::MakeShared<InterthreadConnection>(settings, std::move(client_send));
            callback_(conn);
            return [conn](const_span<uint8> buf) { conn->Receive(buf); };
        });
    }

    void Shutdown() override
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(InterthreadListeners.count(_virtualPort) != 0u);
        InterthreadListeners.erase(_virtualPort);
    }

private:
    uint16 _virtualPort;
};

auto NetServerBase::StartInterthreadServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> unique_ptr<NetServerBase>
{
    STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<InterthreadServer>(settings, std::move(callback));
}
