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

FO_BEGIN_NAMESPACE();

class NetworkServerConnection_Interthread : public NetworkServerConnection
{
public:
    explicit NetworkServerConnection_Interthread(ServerNetworkSettings& settings, InterthreadDataCallback send);
    NetworkServerConnection_Interthread(const NetworkServerConnection_Interthread&) = delete;
    NetworkServerConnection_Interthread(NetworkServerConnection_Interthread&&) noexcept = delete;
    auto operator=(const NetworkServerConnection_Interthread&) = delete;
    auto operator=(NetworkServerConnection_Interthread&&) noexcept = delete;
    ~NetworkServerConnection_Interthread() override = default;

    void Receive(span<const uint8> buf);

private:
    void DispatchImpl() override;
    void DisconnectImpl() override;

    InterthreadDataCallback _send;
};

class InterthreadServer : public NetworkServer
{
public:
    explicit InterthreadServer(ServerNetworkSettings& settings, NewConnectionCallback callback);
    InterthreadServer() = delete;
    InterthreadServer(const InterthreadServer&) = delete;
    InterthreadServer(InterthreadServer&&) noexcept = delete;
    auto operator=(const InterthreadServer&) = delete;
    auto operator=(InterthreadServer&&) noexcept = delete;
    ~InterthreadServer() override = default;

    void Shutdown() override;

private:
    uint16 _virtualPort;
};

auto NetworkServer::StartInterthreadServer(ServerNetworkSettings& settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<InterthreadServer>(settings, std::move(callback));
}

NetworkServerConnection_Interthread::NetworkServerConnection_Interthread(ServerNetworkSettings& settings, InterthreadDataCallback send) :
    NetworkServerConnection(settings),
    _send {std::move(send)}
{
    FO_STACK_TRACE_ENTRY();
}

void NetworkServerConnection_Interthread::Receive(span<const uint8> buf)
{
    FO_STACK_TRACE_ENTRY();

    if (!buf.empty()) {
        ReceiveCallback(buf);
    }
    else {
        _send = nullptr;
        Disconnect();
    }
}

void NetworkServerConnection_Interthread::DispatchImpl()
{
    FO_STACK_TRACE_ENTRY();

    const auto buf = SendCallback();

    if (!buf.empty()) {
        _send(buf);
    }
}

void NetworkServerConnection_Interthread::DisconnectImpl()
{
    FO_STACK_TRACE_ENTRY();

    if (_send) {
        _send({});
        _send = nullptr;
    }
}

InterthreadServer::InterthreadServer(ServerNetworkSettings& settings, NewConnectionCallback callback) :
    _virtualPort {numeric_cast<uint16>(settings.ServerPort)}
{
    FO_STACK_TRACE_ENTRY();

    if (InterthreadListeners.count(_virtualPort) != 0) {
        throw NetworkServerException("Port is busy", _virtualPort);
    }

    InterthreadListeners.emplace(_virtualPort, [&settings, callback_ = std::move(callback)](InterthreadDataCallback client_send) -> InterthreadDataCallback {
        auto conn = SafeAlloc::MakeShared<NetworkServerConnection_Interthread>(settings, std::move(client_send));
        callback_(conn);
        return [conn_ = conn](span<const uint8> buf) mutable { conn_->Receive(buf); };
    });
}

void InterthreadServer::Shutdown()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(InterthreadListeners.count(_virtualPort) != 0);
    InterthreadListeners.erase(_virtualPort);
}

FO_END_NAMESPACE();
