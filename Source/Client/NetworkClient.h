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

// #pragma once

#include "Common.h"

#include "Settings.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(NetworkClientException);

class NetworkClientConnection
{
public:
    explicit NetworkClientConnection(ClientNetworkSettings& settings);
    NetworkClientConnection(const NetworkClientConnection&) = delete;
    NetworkClientConnection(NetworkClientConnection&&) noexcept = delete;
    auto operator=(const NetworkClientConnection&) = delete;
    auto operator=(NetworkClientConnection&&) noexcept = delete;
    virtual ~NetworkClientConnection() = default;

    [[nodiscard]] auto IsConnecting() const noexcept -> bool { return _isConnecting; }
    [[nodiscard]] auto IsConnected() const noexcept -> bool { return _isConnected; }
    [[nodiscard]] auto GetBytesSend() const noexcept -> size_t { return _bytesSend; }
    [[nodiscard]] auto GetBytesReceived() const noexcept -> size_t { return _bytesReceived; }

    auto CheckStatus(bool for_write) -> bool;
    auto SendData(span<const uint8> buf) -> size_t;
    auto ReceiveData() -> span<const uint8>;
    void Disconnect() noexcept;

    [[nodiscard]] static auto CreateInterthreadConnection(ClientNetworkSettings& settings) -> unique_ptr<NetworkClientConnection>;
    [[nodiscard]] static auto CreateSocketsConnection(ClientNetworkSettings& settings) -> unique_ptr<NetworkClientConnection>;

protected:
    virtual auto CheckStatusImpl(bool for_write) -> bool = 0;
    virtual auto SendDataImpl(span<const uint8> buf) -> size_t = 0;
    virtual auto ReceiveDataImpl(vector<uint8>& buf) -> size_t = 0;
    virtual void DisconnectImpl() noexcept = 0;

    raw_ptr<ClientNetworkSettings> _settings;
    bool _isConnecting {};
    bool _isConnected {};

private:
    size_t _bytesSend {};
    size_t _bytesReceived {};
    vector<uint8> _incomeBuf {};
};

FO_END_NAMESPACE();
