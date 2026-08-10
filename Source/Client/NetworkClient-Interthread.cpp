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

#include "NetworkClient.h"

FO_BEGIN_NAMESPACE

struct NetworkClientInterthreadState
{
    std::atomic_bool Alive {true};
    std::atomic_bool RequestDisconnect {};
    mutex ReceivedLocker {};
    vector<uint8_t> Received FO_TSA_GUARDED_BY(ReceivedLocker) {};
};

class NetworkClientConnection_Interthread final : public NetworkClientConnection
{
public:
    explicit NetworkClientConnection_Interthread(ptr<ClientNetworkSettings> settings);
    NetworkClientConnection_Interthread(const NetworkClientConnection_Interthread&) = delete;
    NetworkClientConnection_Interthread(NetworkClientConnection_Interthread&&) noexcept = delete;
    auto operator=(const NetworkClientConnection_Interthread&) = delete;
    auto operator=(NetworkClientConnection_Interthread&&) noexcept = delete;
    ~NetworkClientConnection_Interthread() override;

    auto CheckStatusImpl(bool for_write) -> bool override;
    auto SendDataImpl(const_span<uint8_t> buf) -> size_t override;
    auto ReceiveDataImpl(vector<uint8_t>& buf) -> size_t override;
    void DisconnectImpl() noexcept override;

private:
    shared_ptr<NetworkClientInterthreadState> _interthreadState {};
    InterthreadDataCallback _interthreadSend {};
};

auto NetworkClientConnection::CreateInterthreadConnection(ptr<ClientNetworkSettings> settings) -> unique_ptr<NetworkClientConnection>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<NetworkClientConnection_Interthread>(settings);
}

NetworkClientConnection_Interthread::NetworkClientConnection_Interthread(ptr<ClientNetworkSettings> settings) :
    NetworkClientConnection(settings)
{
    FO_STACK_TRACE_ENTRY();

    uint16_t port = numeric_cast<uint16_t>(_settings->ServerPort);

    function<InterthreadDataCallback(InterthreadDataCallback)> listener;

    {
        scoped_lock locker {InterthreadListenersLocker};

        auto it = InterthreadListeners.find(port);

        if (it == InterthreadListeners.end()) {
            throw NetworkClientException("Interthread listener is not available", port);
        }

        listener = it->second;
    }

    _interthreadState = SafeAlloc::MakeShared<NetworkClientInterthreadState>();
    auto state = _interthreadState;

    _interthreadSend = listener([state](const_span<uint8_t> buf) mutable FO_DEFERRED {
        if (!state->Alive.load()) {
            return;
        }

        if (!buf.empty()) {
            scoped_lock locker {state->ReceivedLocker};

            state->Received.insert(state->Received.end(), buf.begin(), buf.end());
        }
        else {
            state->RequestDisconnect = true;
        }
    });

    WriteLog("Connected to server via interthread communication");

    _isConnecting = false;
    _isConnected = true;
}

NetworkClientConnection_Interthread::~NetworkClientConnection_Interthread()
{
    FO_STACK_TRACE_ENTRY();

    DisconnectImpl();
}

auto NetworkClientConnection_Interthread::CheckStatusImpl(bool for_write) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto state = _interthreadState;

    if (state->RequestDisconnect) {
        Disconnect();
        return false;
    }

    scoped_lock locker {state->ReceivedLocker};

    return for_write ? true : !state->Received.empty();
}

auto NetworkClientConnection_Interthread::SendDataImpl(const_span<uint8_t> buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    _interthreadSend(buf);

    return buf.size();
}

auto NetworkClientConnection_Interthread::ReceiveDataImpl(vector<uint8_t>& buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    auto state = _interthreadState;

    scoped_lock locker {state->ReceivedLocker};

    FO_VERIFY_AND_THROW(!state->Received.empty(), "Interthread client receive was called without a pending packet", buf.size());
    size_t recv_size = state->Received.size();

    while (buf.size() < recv_size) {
        buf.resize(buf.size() * 2);
    }

    MemCopy(buf.data(), state->Received.data(), recv_size);
    state->Received.clear();

    return recv_size;
}

void NetworkClientConnection_Interthread::DisconnectImpl() noexcept
{
    FO_STACK_TRACE_ENTRY();

    InterthreadDataCallback interthread_send;
    auto state = _interthreadState;

    state->Alive = false;

    if (!state->RequestDisconnect) {
        interthread_send = std::move(_interthreadSend);
    }

    _interthreadSend = nullptr;
    state->RequestDisconnect = false;

    {
        scoped_lock locker {state->ReceivedLocker};

        state->Received.clear();
    }

    if (interthread_send) {
        safe_call([&] { interthread_send({}); });
    }
}

FO_END_NAMESPACE
