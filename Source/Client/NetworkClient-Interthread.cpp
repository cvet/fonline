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

#include "NetworkClient.h"

FO_BEGIN_NAMESPACE();

class NetworkClientConnection_Interthread final : public NetworkClientConnection
{
public:
    explicit NetworkClientConnection_Interthread(ClientNetworkSettings& settings);
    NetworkClientConnection_Interthread(const NetworkClientConnection_Interthread&) = delete;
    NetworkClientConnection_Interthread(NetworkClientConnection_Interthread&&) noexcept = delete;
    auto operator=(const NetworkClientConnection_Interthread&) = delete;
    auto operator=(NetworkClientConnection_Interthread&&) noexcept = delete;
    ~NetworkClientConnection_Interthread() override = default;

    auto CheckStatusImpl(bool for_write) -> bool override;
    auto SendDataImpl(span<const uint8> buf) -> size_t override;
    auto ReceiveDataImpl(vector<uint8>& buf) -> size_t override;
    void DisconnectImpl() noexcept override;

private:
    InterthreadDataCallback _interthreadSend {};
    vector<uint8> _interthreadReceived {};
    std::mutex _interthreadReceivedLocker {};
    std::atomic_bool _interthreadRequestDisconnect {};
};

auto NetworkClientConnection::CreateInterthreadConnection(ClientNetworkSettings& settings) -> unique_ptr<NetworkClientConnection>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<NetworkClientConnection_Interthread>(settings);
}

NetworkClientConnection_Interthread::NetworkClientConnection_Interthread(ClientNetworkSettings& settings) :
    NetworkClientConnection(settings)
{
    FO_STACK_TRACE_ENTRY();

    _interthreadReceived.clear();

    const auto port = numeric_cast<uint16>(_settings->ServerPort);

    _interthreadSend = InterthreadListeners[port]([this](span<const uint8> buf) {
        if (!buf.empty()) {
            auto locker = std::unique_lock {_interthreadReceivedLocker};

            _interthreadReceived.insert(_interthreadReceived.end(), buf.begin(), buf.end());
        }
        else {
            _interthreadRequestDisconnect = true;
        }
    });

    WriteLog("Connected to server via interthread communication");

    _isConnecting = false;
    _isConnected = true;
}

auto NetworkClientConnection_Interthread::CheckStatusImpl(bool for_write) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_interthreadRequestDisconnect) {
        Disconnect();
        return false;
    }

    auto locker = std::unique_lock {_interthreadReceivedLocker};

    return for_write ? true : !_interthreadReceived.empty();
}

auto NetworkClientConnection_Interthread::SendDataImpl(span<const uint8> buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    _interthreadSend(buf);

    return buf.size();
}

auto NetworkClientConnection_Interthread::ReceiveDataImpl(vector<uint8>& buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    auto locker = std::unique_lock {_interthreadReceivedLocker};

    FO_RUNTIME_ASSERT(!_interthreadReceived.empty());
    const auto recv_size = _interthreadReceived.size();

    while (buf.size() < recv_size) {
        buf.resize(buf.size() * 2);
    }

    MemCopy(buf.data(), _interthreadReceived.data(), recv_size);
    _interthreadReceived.clear();

    return recv_size;
}

void NetworkClientConnection_Interthread::DisconnectImpl() noexcept
{
    FO_STACK_TRACE_ENTRY();

    InterthreadDataCallback interthread_send;

    if (!_interthreadRequestDisconnect) {
        interthread_send = std::move(_interthreadSend);
    }

    _interthreadSend = nullptr;
    _interthreadRequestDisconnect = false;

    if (interthread_send) {
        safe_call([&] { interthread_send({}); });
    }
}

FO_END_NAMESPACE();
