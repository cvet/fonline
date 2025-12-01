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

NetworkClientConnection::NetworkClientConnection(ClientNetworkSettings& settings) :
    _settings {&settings}
{
    FO_STACK_TRACE_ENTRY();

    _incomeBuf.resize(_settings->NetBufferSize);
    _isConnecting = true;
}

auto NetworkClientConnection::CheckStatus(bool for_write) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_isConnecting && !_isConnected) {
        return false;
    }

    try {
        return CheckStatusImpl(for_write);
    }
    catch (...) {
        Disconnect();
        throw;
    }
}

auto NetworkClientConnection::SendData(span<const uint8> buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (!_isConnecting && !_isConnected) {
        return 0;
    }

    try {
        const auto send_len = SendDataImpl(buf);
        _bytesSend += send_len;
        return send_len;
    }
    catch (...) {
        Disconnect();
        throw;
    }
}

auto NetworkClientConnection::ReceiveData() -> span<const uint8>
{
    if (!_isConnecting && !_isConnected) {
        return {};
    }

    try {
        const auto recv_len = ReceiveDataImpl(_incomeBuf);
        _bytesReceived += recv_len;
        return {_incomeBuf.data(), recv_len};
    }
    catch (...) {
        Disconnect();
        throw;
    }
}

void NetworkClientConnection::Disconnect() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (!_isConnecting && !_isConnected) {
        return;
    }

    if (_isConnecting) {
        WriteLog("Can't connect to the server");

        _isConnecting = false;
    }

    if (_isConnected) {
        WriteLog("Disconnect from the server");

        _isConnected = false;
    }

    DisconnectImpl();
}

FO_END_NAMESPACE();
