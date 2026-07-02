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

#if FO_HAVE_ASIO

#define ASIO_STANDALONE 1
#define ASIO_NO_DEPRECATED 1
#include "asio.hpp"

FO_BEGIN_NAMESPACE

class NetworkServerConnection_Asio final : public NetworkServerConnection
{
public:
    explicit NetworkServerConnection_Asio(ptr<ServerNetworkSettings> settings, unique_ptr<asio::ip::tcp::socket> socket);
    NetworkServerConnection_Asio(const NetworkServerConnection_Asio&) = delete;
    NetworkServerConnection_Asio(NetworkServerConnection_Asio&&) noexcept = delete;
    auto operator=(const NetworkServerConnection_Asio&) = delete;
    auto operator=(NetworkServerConnection_Asio&&) noexcept = delete;
    ~NetworkServerConnection_Asio() override;

    void StartAsyncRead();

private:
    void LogSocketOperationError(string_view operation, const std::error_code& error);
    void AsyncReadComplete(std::error_code error, size_t bytes);
    void NextAsyncRead();
    void StartAsyncWrite();
    void AsyncWriteComplete(std::error_code error, size_t bytes);
    void NextAsyncWrite();

    void DispatchImpl() override;
    void DisconnectImpl() override;

    asio::ip::tcp::socket _socket;
    std::atomic_bool _writePending {};
    std::vector<uint8_t> _inBufData {};
};

class NetworkServer_Asio : public NetworkServer
{
public:
    explicit NetworkServer_Asio(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback);
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

    ptr<ServerNetworkSettings> _settings;
    asio::io_context _context {};
    asio::ip::tcp::acceptor _acceptor;
    NewConnectionCallback _connectionCallback;
    thread _runThread {};
};

auto NetworkServer::StartAsioServer(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Listen TCP connections on port {}", settings->ServerPort);

    return SafeAlloc::MakeUnique<NetworkServer_Asio>(settings, std::move(callback));
}

NetworkServerConnection_Asio::NetworkServerConnection_Asio(ptr<ServerNetworkSettings> settings, unique_ptr<asio::ip::tcp::socket> socket) :
    NetworkServerConnection(settings),
    _socket {std::move(*socket)}
{
    FO_STACK_TRACE_ENTRY();

    std::error_code endpoint_error;
    const auto endpoint = _socket.remote_endpoint(endpoint_error);

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
        _socket.set_option(asio::ip::tcp::no_delay(true), no_delay_error);
        LogSocketOperationError("set TCP_NODELAY", no_delay_error);
    }

    _inBufData.resize(_settings->NetBufferSize);
}

void NetworkServerConnection_Asio::LogSocketOperationError(string_view operation, const std::error_code& error)
{
    if (!error || error == asio::error::not_connected || error == asio::error::bad_descriptor || error == asio::error::operation_aborted) {
        return;
    }

    if (_port != 0) {
        WriteLog(LogType::Warning, "TCP socket {} failed for {}:{}: {}", operation, _host, _port, error.message());
    }
    else {
        WriteLog(LogType::Warning, "TCP socket {} failed for {}: {}", operation, _host, error.message());
    }
}

NetworkServerConnection_Asio::~NetworkServerConnection_Asio()
{
    FO_STACK_TRACE_ENTRY();

    try {
        if (_socket.is_open()) {
            _socket.close();
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
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
        FO_STRONG_ASSERT(bytes <= _inBufData.size(), "Received byte count exceeds the receive buffer size");
        const_span<uint8_t> received_data {_inBufData.data(), bytes};
        ReceiveCallback(received_data);
        NextAsyncRead();
    }
    else {
        Disconnect();
    }
}

void NetworkServerConnection_Asio::NextAsyncRead()
{
    FO_STACK_TRACE_ENTRY();

    const auto read_handler = [lifetime = shared_from_this(), this](std::error_code error, size_t bytes) FO_DEFERRED {
        ignore_unused(lifetime);
        AsyncReadComplete(error, bytes);
    };

    async_read(_socket, asio::buffer(_inBufData), asio::transfer_at_least(1), read_handler);
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
        const auto write_handler = [lifetime = shared_from_this(), this](std::error_code error, size_t bytes) FO_DEFERRED {
            ignore_unused(lifetime);
            AsyncWriteComplete(error, bytes);
        };

        async_write(_socket, asio::buffer(buf.data(), buf.size()), write_handler);
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

    std::error_code shutdown_error;
    _socket.shutdown(asio::ip::tcp::socket::shutdown_both, shutdown_error);
    LogSocketOperationError("shutdown", shutdown_error);

    std::error_code close_error;
    _socket.close(close_error);
    LogSocketOperationError("close", close_error);
}

NetworkServer_Asio::NetworkServer_Asio(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) :
    _settings {settings},
    _acceptor(_context, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), numeric_cast<uint16_t>(settings->ServerPort))),
    _connectionCallback {std::move(callback)}
{
    FO_STACK_TRACE_ENTRY();

    AcceptNext();
    _runThread = run_thread("Network-Asio", [this] { Run(); });
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
    }
}

void NetworkServer_Asio::AcceptNext()
{
    FO_STACK_TRACE_ENTRY();

    auto socket = SafeAlloc::MakeUnique<asio::ip::tcp::socket>(_context);
    auto socket_ptr = socket.as_ptr();
    _acceptor.async_accept(*socket_ptr, [this, socket = std::move(socket)](std::error_code error) mutable FO_DEFERRED { AcceptConnection(error, std::move(socket)); });
}

void NetworkServer_Asio::AcceptConnection(std::error_code error, unique_ptr<asio::ip::tcp::socket> socket)
{
    FO_STACK_TRACE_ENTRY();

    const auto rearm_accept = [this] {
        if (!_context.stopped()) {
            AcceptNext();
        }
    };

    if (!error) {
        try {
            auto connection = SafeAlloc::MakeShared<NetworkServerConnection_Asio>(_settings, std::move(socket));
            connection->StartAsyncRead(); // shared_from_this() is not available in constructor so StartRead/NextAsyncRead is called after
            _connectionCallback(std::move(connection));
        }
        catch (const std::exception&) {
            const auto exception = std::current_exception();
            safe_call(rearm_accept);

            run_thread("Network-Asio-Reporter", [exception = std::move(exception)] {
                try {
                    std::rethrow_exception(exception);
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }
            }).detach();

            return;
        }

        rearm_accept();
    }
    else {
        if (error != asio::error::operation_aborted) {
            WriteLog(LogType::Warning, "Accept error: {}", error.message());
        }
    }
}

FO_END_NAMESPACE

#endif
