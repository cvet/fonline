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

#pragma once

#include "Common.h"

#include "NetBuffer.h"
#include "Settings.h"

DECLARE_EXCEPTION(NetworkException);

class NetworkServerConnection : public std::enable_shared_from_this<NetworkServerConnection>
{
public:
    NetworkServerConnection() = delete;
    NetworkServerConnection(const NetworkServerConnection&) = delete;
    NetworkServerConnection(NetworkServerConnection&&) noexcept = delete;
    auto operator=(const NetworkServerConnection&) = delete;
    auto operator=(NetworkServerConnection&&) noexcept = delete;
    virtual ~NetworkServerConnection() = default;

    [[nodiscard]] auto GetIp() const noexcept -> uint { return _ip; }
    [[nodiscard]] auto GetHost() const noexcept -> string_view { return _host; }
    [[nodiscard]] auto GetPort() const noexcept -> uint16 { return _port; }
    [[nodiscard]] auto IsDisconnected() const noexcept -> bool { return _isDisconnected; }
    [[nodiscard]] virtual auto IsWebConnection() const noexcept -> bool { return false; }
    [[nodiscard]] virtual auto IsInterthreadConnection() const noexcept -> bool { return false; }

    void DisableCompression();
    void Dispatch();
    void Disconnect();

    NetInBuffer InBuf;
    std::mutex InBufLocker {};
    NetOutBuffer OutBuf;
    std::mutex OutBufLocker {};

protected:
    explicit NetworkServerConnection(ServerNetworkSettings& settings);
    virtual void DispatchImpl() = 0;
    virtual void DisconnectImpl() = 0;

    auto SendCallback() -> const_span<uint8>;
    void ReceiveCallback(const uint8* buf, size_t len);

    ServerNetworkSettings& _settings;
    uint _ip {};
    string _host {};
    uint16 _port {};
    std::atomic_bool _isDisconnected {};

private:
    struct Impl;
    unique_del_ptr<Impl> _impl {};
    vector<uint8> _outBuf {};
};

class NetworkServer
{
public:
    using NewConnectionCallback = std::function<void(shared_ptr<NetworkServerConnection>)>;

    NetworkServer() = default;
    NetworkServer(const NetworkServer&) = delete;
    NetworkServer(NetworkServer&&) noexcept = delete;
    auto operator=(const NetworkServer&) = delete;
    auto operator=(NetworkServer&&) noexcept = delete;
    virtual ~NetworkServer() = default;

    virtual void Shutdown() = 0;

    [[nodiscard]] static auto StartInterthreadServer(ServerNetworkSettings& settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>;
#if FO_HAVE_ASIO
    [[nodiscard]] static auto StartTcpServer(ServerNetworkSettings& settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>;
#endif
#if FO_HAVE_WEB_SOCKETS
    [[nodiscard]] static auto StartWebSocketsServer(ServerNetworkSettings& settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>;
#endif

    [[nodiscard]] static auto CreateDummyConnection(ServerNetworkSettings& settings) -> shared_ptr<NetworkServerConnection>;
};
