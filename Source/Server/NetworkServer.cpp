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

NetworkServerConnection::NetworkServerConnection(ServerNetworkSettings& settings) :
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();
}

void NetworkServerConnection::SetAsyncCallbacks(AsyncSendCallback send, AsyncReceiveCallback receive, DisconnectCallback disconnect)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_sendCallbackSet);
    FO_RUNTIME_ASSERT(!_disconnectCallbackSet);
    FO_RUNTIME_ASSERT(send);
    FO_RUNTIME_ASSERT(receive);
    FO_RUNTIME_ASSERT(disconnect);

    if (_isDisconnected) {
        return;
    }

    _sendCallback = std::move(send);
    _sendCallbackSet = true;

    _disconnectCallback = std::move(disconnect);
    _disconnectCallbackSet = true;

    {
        std::scoped_lock locker(_receiveLocker);

        _receiveCallback = std::move(receive);

        if (!_initReceiveBuf.empty()) {
            _receiveCallback(_initReceiveBuf);
            _initReceiveBuf.clear();
            _initReceiveBuf.shrink_to_fit();
        }
    }
}

void NetworkServerConnection::Dispatch()
{
    FO_STACK_TRACE_ENTRY();

    if (_isDisconnected) {
        return;
    }

    DispatchImpl();
}

void NetworkServerConnection::Disconnect()
{
    FO_STACK_TRACE_ENTRY();

    bool expected = false;

    if (_isDisconnected.compare_exchange_strong(expected, true)) {
        DisconnectImpl();

        if (_disconnectCallbackSet) {
            _disconnectCallback();
        }
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
auto NetworkServerConnection::SendCallback() -> span<const uint8>
{
    FO_STACK_TRACE_ENTRY();

    if (!_sendCallbackSet) {
        return {};
    }

    return _sendCallback();
}

void NetworkServerConnection::ReceiveCallback(span<const uint8> buf)
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker(_receiveLocker);

    if (_receiveCallback) {
        _receiveCallback(buf);
    }
    else {
        _initReceiveBuf.insert(_initReceiveBuf.end(), buf.begin(), buf.end());
    }
}

class DummyNetConnection : public NetworkServerConnection
{
public:
    explicit DummyNetConnection(ServerNetworkSettings& settings) :
        NetworkServerConnection(settings)
    {
        _host = "Dummy";
        Disconnect();
    }
    DummyNetConnection(const DummyNetConnection&) = delete;
    DummyNetConnection(DummyNetConnection&&) noexcept = delete;
    auto operator=(const DummyNetConnection&) = delete;
    auto operator=(DummyNetConnection&&) noexcept = delete;
    ~DummyNetConnection() override = default;

protected:
    void DispatchImpl() override { }
    void DisconnectImpl() override { }
};

auto NetworkServer::CreateDummyConnection(ServerNetworkSettings& settings) -> shared_ptr<NetworkServerConnection>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeShared<DummyNetConnection>(settings);
}

FO_END_NAMESPACE();
