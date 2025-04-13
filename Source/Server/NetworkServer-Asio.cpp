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

#include "Application.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Timer.h"

// ReSharper disable once CppInconsistentNaming
#define _WIN32_WINNT 0x0601 // NOLINT(clang-diagnostic-reserved-macro-identifier)
#include "asio.hpp"

class NetworkServerConnection_Asio final : public NetworkServerConnection
{
public:
    NetworkServerConnection_Asio(ServerNetworkSettings& settings, unique_ptr<asio::ip::tcp::socket> socket) :
        NetworkServerConnection(settings),
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

        _inBufData.resize(_settings.NetBufferSize);
    }

    NetworkServerConnection_Asio() = delete;
    NetworkServerConnection_Asio(const NetworkServerConnection_Asio&) = delete;
    NetworkServerConnection_Asio(NetworkServerConnection_Asio&&) noexcept = delete;
    auto operator=(const NetworkServerConnection_Asio&) = delete;
    auto operator=(NetworkServerConnection_Asio&&) noexcept = delete;

    ~NetworkServerConnection_Asio() override
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

    void StartAsyncRead()
    {
        STACK_TRACE_ENTRY();

        NextAsyncRead();
    }

private:
    void AsyncReadComplete(std::error_code error, size_t bytes)
    {
        STACK_TRACE_ENTRY();

        if (!error) {
            ReceiveCallback(_inBufData.data(), bytes);
            NextAsyncRead();
        }
        else {
            Disconnect();
        }
    }

    void NextAsyncRead()
    {
        STACK_TRACE_ENTRY();

        const auto read_handler = [thiz = shared_from_this()](std::error_code error, size_t bytes) {
            auto* thiz_ = static_cast<NetworkServerConnection_Asio*>(thiz.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            thiz_->AsyncReadComplete(error, bytes);
        };

        async_read(*_socket, asio::buffer(_inBufData), asio::transfer_at_least(1), read_handler);
    }

    void StartAsyncWrite()
    {
        STACK_TRACE_ENTRY();

        bool expected = false;

        if (_writePending.compare_exchange_strong(expected, true)) {
            const auto write_handler = [thiz = shared_from_this()](std::error_code error, size_t bytes) {
                auto* thiz_ = static_cast<NetworkServerConnection_Asio*>(thiz.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                thiz_->AsyncWriteComplete(error, bytes);
            };

            // Just initiate async write
            uint8 dummy = 0;
            async_write(*_socket, asio::buffer(&dummy, 0), write_handler);
        }
    }

    void AsyncWriteComplete(std::error_code error, size_t bytes)
    {
        STACK_TRACE_ENTRY();

        UNUSED_VARIABLE(bytes);

        if (!error) {
            NextAsyncWrite();
        }
        else {
            _writePending = false;
            Disconnect();
        }
    }

    void NextAsyncWrite()
    {
        STACK_TRACE_ENTRY();

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

    void DispatchImpl() override
    {
        STACK_TRACE_ENTRY();

        StartAsyncWrite();
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
    std::vector<uint8> _inBufData {};
};

class NetworkServer_Asio : public NetworkServer
{
public:
    NetworkServer_Asio() = delete;
    NetworkServer_Asio(const NetworkServer_Asio&) = delete;
    NetworkServer_Asio(NetworkServer_Asio&&) noexcept = delete;
    auto operator=(const NetworkServer_Asio&) = delete;
    auto operator=(NetworkServer_Asio&&) noexcept = delete;
    ~NetworkServer_Asio() override = default;

    NetworkServer_Asio(ServerNetworkSettings& settings, NewConnectionCallback callback) :
        _settings {settings},
        _acceptor(_ioService, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), static_cast<uint16>(settings.ServerPort)))
    {
        STACK_TRACE_ENTRY();

        _connectionCallback = std::move(callback);
        AcceptNext();
        _runThread = std::thread(&NetworkServer_Asio::Run, this);
    }

    void Shutdown() override
    {
        STACK_TRACE_ENTRY();

        _ioService.stop();
        _runThread.join();
    }

private:
    void Run()
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

    void AcceptNext()
    {
        STACK_TRACE_ENTRY();

        auto* socket = SafeAlloc::MakeRaw<asio::ip::tcp::socket>(_ioService);
        _acceptor.async_accept(*socket, [this, socket](std::error_code error) { AcceptConnection(error, unique_ptr<asio::ip::tcp::socket>(socket)); });
    }

    void AcceptConnection(std::error_code error, unique_ptr<asio::ip::tcp::socket> socket)
    {
        STACK_TRACE_ENTRY();

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

    ServerNetworkSettings& _settings;
    asio::io_context _ioService {};
    asio::ip::tcp::acceptor _acceptor;
    NewConnectionCallback _connectionCallback {};
    std::thread _runThread {};
};

auto NetworkServer::StartAsioServer(ServerNetworkSettings& settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>
{
    STACK_TRACE_ENTRY();

    WriteLog("Listen TCP connections on port {}", settings.ServerPort);

    return SafeAlloc::MakeUnique<NetworkServer_Asio>(settings, std::move(callback));
}

#endif
