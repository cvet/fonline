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

#pragma once

#include "Common.h"

#include "NetBuffer.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(NetworkServerException);

class NetworkServerConnection : public enable_shared_from_this<NetworkServerConnection>
{
public:
    using AsyncSendCallback = function<const_span<uint8_t>()>;
    using AsyncReceiveCallback = function<void(const_span<uint8_t>)>;
    using DisconnectCallback = function<void()>;

    NetworkServerConnection(const NetworkServerConnection&) = delete;
    NetworkServerConnection(NetworkServerConnection&&) noexcept = delete;
    auto operator=(const NetworkServerConnection&) = delete;
    auto operator=(NetworkServerConnection&&) noexcept = delete;
    virtual ~NetworkServerConnection() = default;

    [[nodiscard]] virtual auto GetHost() const noexcept -> string_view { return _host; }
    [[nodiscard]] virtual auto GetPort() const noexcept -> uint16_t { return _port; }
    [[nodiscard]] auto IsDisconnected() const noexcept -> bool { return _isDisconnected; }

    void SetAsyncCallbacks(AsyncSendCallback send, AsyncReceiveCallback receive, DisconnectCallback disconnect);
    void Dispatch();
    void Disconnect();

protected:
    explicit NetworkServerConnection(ptr<ServerNetworkSettings> settings);

    virtual void DispatchImpl() = 0;
    virtual void DisconnectImpl() = 0;

    auto SendCallback() -> const_span<uint8_t>;
    void ReceiveCallback(const_span<uint8_t> buf);

    ptr<ServerNetworkSettings> _settings;
    string _host {};
    uint16_t _port {};

private:
    AsyncSendCallback _sendCallback {};
    std::atomic_bool _sendCallbackSet {};
    mutex _receiveLocker {};
    AsyncReceiveCallback _receiveCallback FO_TSA_GUARDED_BY(_receiveLocker) {};
    vector<uint8_t> _initReceiveBuf FO_TSA_GUARDED_BY(_receiveLocker) {};
    DisconnectCallback _disconnectCallback {};
    std::atomic_bool _disconnectCallbackSet {};
    std::atomic_bool _isDisconnected {};
};

class NetworkServer
{
public:
    using NewConnectionCallback = function<void(shared_ptr<NetworkServerConnection>)>;

    enum class DummyConnectionState : uint8_t
    {
        Connected,
        Disconnected,
    };

    NetworkServer();
    NetworkServer(const NetworkServer&) = delete;
    NetworkServer(NetworkServer&&) noexcept = delete;
    auto operator=(const NetworkServer&) = delete;
    auto operator=(NetworkServer&&) noexcept = delete;
    virtual ~NetworkServer() = default;

    void Shutdown();

    static auto StartInterthreadServer(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>;
    static auto StartUdpSocketsServer(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>;
#if FO_HAVE_ASIO
    static auto StartAsioServer(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>;
#endif
#if FO_HAVE_WEB_SOCKETS
    static auto StartWebSocketsServer(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>;
#endif

    static auto CreateDummyConnection(ptr<ServerNetworkSettings> settings, DummyConnectionState state = DummyConnectionState::Disconnected) -> shared_ptr<NetworkServerConnection>;

protected:
    class ConnectionRegistry final
    {
    public:
        auto BeginShutdown() -> optional<vector<shared_ptr<NetworkServerConnection>>>;
        auto TrackConnection(shared_ptr<NetworkServerConnection> connection) -> bool;

    private:
        mutex _connectionsLocker {};
        vector<weak_ptr<NetworkServerConnection>> _connections FO_TSA_GUARDED_BY(_connectionsLocker) {};
        bool _shutdownStarted FO_TSA_GUARDED_BY(_connectionsLocker) {};
    };

    auto GetConnectionRegistry() const noexcept -> shared_ptr<ConnectionRegistry>;
    auto TrackConnection(shared_ptr<NetworkServerConnection> connection) -> bool;
    virtual void ShutdownImpl() = 0;

private:
    shared_ptr<ConnectionRegistry> _connectionRegistry;
};

FO_END_NAMESPACE
