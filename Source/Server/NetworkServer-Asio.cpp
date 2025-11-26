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

#if FO_HAVE_ASIO

#define ASIO_STANDALONE 1
#define ASIO_NO_DEPRECATED 1
#include "asio.hpp"

FO_BEGIN_NAMESPACE();

class NetworkServerConnection_Asio final : public NetworkServerConnection
{
public:
    explicit NetworkServerConnection_Asio(ServerNetworkSettings& settings, unique_ptr<asio::ip::tcp::socket> socket);
    NetworkServerConnection_Asio(const NetworkServerConnection_Asio&) = delete;
    NetworkServerConnection_Asio(NetworkServerConnection_Asio&&) noexcept = delete;
    auto operator=(const NetworkServerConnection_Asio&) = delete;
    auto operator=(NetworkServerConnection_Asio&&) noexcept = delete;
    ~NetworkServerConnection_Asio() override;

    void StartAsyncRead();

private:
    void AsyncReadComplete(std::error_code error, size_t bytes);
    void NextAsyncRead();
    void StartAsyncWrite();
    void AsyncWriteComplete(std::error_code error, size_t bytes);
    void NextAsyncWrite();

    void DispatchImpl() override;
    void DisconnectImpl() override;

    unique_ptr<asio::ip::tcp::socket> _socket;
    std::atomic_bool _writePending {};
    std::vector<uint8> _inBufData {};
};

class NetworkServer_Asio : public NetworkServer
{
public:
    explicit NetworkServer_Asio(ServerNetworkSettings& settings, NewConnectionCallback callback);
    NetworkServer_Asio(const NetworkServer_Asio&) = delete;
    NetworkServer_Asio(NetworkServer_Asio&&) noexcept = delete;
    auto operator=(const NetworkServer_Asio&) = delete;
    auto operator=(NetworkServer_Asio&&) noexcept = delete;
    ~NetworkServer_Asio() override = default;

    void Shutdown() override;

private:
    void Run();
    void AcceptNext();
    void AcceptConnection(std::error_code error, unique_ptr<asio::ip::tcp::socket> socket);

    ServerNetworkSettings& _settings;
    asio::io_context _context {};
    asio::ip::tcp::acceptor _acceptor;
    NewConnectionCallback _connectionCallback;
    std::thread _runThread {};
};

auto NetworkServer::StartAsioServer(ServerNetworkSettings& settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Listen TCP connections on port {}", settings.ServerPort);

    return SafeAlloc::MakeUnique<NetworkServer_Asio>(settings, std::move(callback));
}

NetworkServerConnection_Asio::NetworkServerConnection_Asio(ServerNetworkSettings& settings, unique_ptr<asio::ip::tcp::socket> socket) :
    NetworkServerConnection(settings),
    _socket {std::move(socket)}
{
    FO_STACK_TRACE_ENTRY();

    _host = _socket->remote_endpoint().address().to_string();
    _port = _socket->remote_endpoint().port();

    if (settings.DisableTcpNagle) {
        _socket->set_option(asio::ip::tcp::no_delay(true));
    }

    _inBufData.resize(_settings.NetBufferSize);
}

NetworkServerConnection_Asio::~NetworkServerConnection_Asio()
{
    FO_STACK_TRACE_ENTRY();

    try {
        if (_socket->is_open()) {
            _socket->close();
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

void NetworkServerConnection_Asio::StartAsyncRead()
{
    FO_STACK_TRACE_ENTRY();

    NextAsyncRead();
}

void NetworkServerConnection_Asio::AsyncReadComplete(std::error_code error, size_t bytes)
{
    FO_STACK_TRACE_ENTRY();

    if (!error) {
        ReceiveCallback({_inBufData.data(), bytes});
        NextAsyncRead();
    }
    else {
        Disconnect();
    }
}

void NetworkServerConnection_Asio::NextAsyncRead()
{
    FO_STACK_TRACE_ENTRY();

    const auto read_handler = [thiz = shared_from_this()](std::error_code error, size_t bytes) {
        auto* thiz_ = static_cast<NetworkServerConnection_Asio*>(thiz.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        thiz_->AsyncReadComplete(error, bytes);
    };

    async_read(*_socket, asio::buffer(_inBufData), asio::transfer_at_least(1), read_handler);
}

void NetworkServerConnection_Asio::StartAsyncWrite()
{
    FO_STACK_TRACE_ENTRY();

    bool expected = false;

    if (_writePending.compare_exchange_strong(expected, true)) {
        NextAsyncWrite();
    }
}

void NetworkServerConnection_Asio::AsyncWriteComplete(std::error_code error, size_t bytes)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(bytes);

    if (!error) {
        NextAsyncWrite();
    }
    else {
        _writePending = false;
        Disconnect();
    }
}

void NetworkServerConnection_Asio::NextAsyncWrite()
{
    FO_STACK_TRACE_ENTRY();

    const auto buf = SendCallback();

    if (!buf.empty()) {
        const auto write_handler = [thiz = shared_from_this()](std::error_code error, size_t bytes) {
            auto* thiz_ = static_cast<NetworkServerConnection_Asio*>(thiz.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            thiz_->AsyncWriteComplete(error, bytes);
        };

        async_write(*_socket, asio::buffer(buf.data(), buf.size()), write_handler);
    }
    else {
        _writePending = false;
    }
}

void NetworkServerConnection_Asio::DispatchImpl()
{
    FO_STACK_TRACE_ENTRY();

    StartAsyncWrite();
}

void NetworkServerConnection_Asio::DisconnectImpl()
{
    FO_STACK_TRACE_ENTRY();

    _socket->shutdown(asio::ip::tcp::socket::shutdown_both);
    _socket->close();
}

NetworkServer_Asio::NetworkServer_Asio(ServerNetworkSettings& settings, NewConnectionCallback callback) :
    _settings {settings},
    _acceptor(_context, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), numeric_cast<uint16>(settings.ServerPort))),
    _connectionCallback {std::move(callback)}
{
    FO_STACK_TRACE_ENTRY();

    AcceptNext();
    _runThread = std::thread(&NetworkServer_Asio::Run, this);
}

void NetworkServer_Asio::Shutdown()
{
    FO_STACK_TRACE_ENTRY();

    _context.stop();
    _runThread.join();
}

void NetworkServer_Asio::Run()
{
    FO_STACK_TRACE_ENTRY();

    while (true) {
        try {
            _context.run();
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

void NetworkServer_Asio::AcceptNext()
{
    FO_STACK_TRACE_ENTRY();

    auto* socket = SafeAlloc::MakeRaw<asio::ip::tcp::socket>(_context);
    _acceptor.async_accept(*socket, [this, socket](std::error_code error) { AcceptConnection(error, unique_ptr<asio::ip::tcp::socket>(socket)); });
}

void NetworkServer_Asio::AcceptConnection(std::error_code error, unique_ptr<asio::ip::tcp::socket> socket)
{
    FO_STACK_TRACE_ENTRY();

    if (!error) {
        auto connection = SafeAlloc::MakeShared<NetworkServerConnection_Asio>(_settings, std::move(socket));
        connection->StartAsyncRead(); // shared_from_this() is not available in constructor so StartRead/NextAsyncRead is called after
        _connectionCallback(std::move(connection));
    }
    else {
        WriteLog("Accept error: {}", error.message());
    }

    AcceptNext();
}

FO_END_NAMESPACE();

#endif
