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

class NetworkServerConnection_Interthread : public NetworkServerConnection
{
public:
    NetworkServerConnection_Interthread(ServerNetworkSettings& settings, InterthreadDataCallback send) :
        NetworkServerConnection(settings),
        _send {std::move(send)}
    {
        STACK_TRACE_ENTRY();
    }

    NetworkServerConnection_Interthread(const NetworkServerConnection_Interthread&) = delete;
    NetworkServerConnection_Interthread(NetworkServerConnection_Interthread&&) noexcept = delete;
    auto operator=(const NetworkServerConnection_Interthread&) = delete;
    auto operator=(NetworkServerConnection_Interthread&&) noexcept = delete;
    ~NetworkServerConnection_Interthread() override = default;

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

class InterthreadServer : public NetworkServer
{
public:
    InterthreadServer() = delete;
    InterthreadServer(const InterthreadServer&) = delete;
    InterthreadServer(InterthreadServer&&) noexcept = delete;
    auto operator=(const InterthreadServer&) = delete;
    auto operator=(InterthreadServer&&) noexcept = delete;
    ~InterthreadServer() override = default;

    InterthreadServer(ServerNetworkSettings& settings, NewConnectionCallback callback) :
        _virtualPort {static_cast<uint16>(settings.ServerPort)}
    {
        STACK_TRACE_ENTRY();

        if (InterthreadListeners.count(_virtualPort) != 0) {
            throw NetworkException("Port is busy", _virtualPort);
        }

        InterthreadListeners.emplace(_virtualPort, [&settings, callback_ = std::move(callback)](InterthreadDataCallback client_send) -> InterthreadDataCallback {
            auto conn = SafeAlloc::MakeShared<NetworkServerConnection_Interthread>(settings, std::move(client_send));
            callback_(conn);
            return [conn_ = conn](const_span<uint8> buf) mutable { conn_->Receive(buf); };
        });
    }

    void Shutdown() override
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(InterthreadListeners.count(_virtualPort) != 0);
        InterthreadListeners.erase(_virtualPort);
    }

private:
    uint16 _virtualPort;
};

auto NetworkServer::StartInterthreadServer(ServerNetworkSettings& settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>
{
    STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<InterthreadServer>(settings, std::move(callback));
}
