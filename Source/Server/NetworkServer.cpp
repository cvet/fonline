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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

FO_BEGIN_NAMESPACE

NetworkServer::NetworkServer() :
    _connectionRegistry {safe_alloc::make_shared<ConnectionRegistry>()}
{
    FO_STACK_TRACE_ENTRY();
}

void NetworkServer::Shutdown()
{
    FO_STACK_TRACE_ENTRY();

    optional<vector<shared_ptr<NetworkServerConnection>>> connections = _connectionRegistry->BeginShutdown();
    if (!connections) {
        return;
    }

    for (auto& connection : *connections) {
        safe_call([&connection] { connection->Disconnect(); });
    }

    ShutdownImpl();
}

auto NetworkServer::GetConnectionRegistry() const noexcept -> shared_ptr<ConnectionRegistry>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _connectionRegistry;
}

auto NetworkServer::TrackConnection(shared_ptr<NetworkServerConnection> connection) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _connectionRegistry->TrackConnection(std::move(connection));
}

auto NetworkServer::ConnectionRegistry::BeginShutdown() -> optional<vector<shared_ptr<NetworkServerConnection>>>
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_connectionsLocker};

    if (_shutdownStarted) {
        return {};
    }

    vector<shared_ptr<NetworkServerConnection>> connections;
    connections.reserve(_connections.size());

    for (const auto& weak_connection : _connections) {
        if (auto connection = weak_connection.lock()) {
            connections.emplace_back(std::move(connection));
        }
    }

    _shutdownStarted = true;
    _connections.clear();
    return connections;
}

auto NetworkServer::ConnectionRegistry::TrackConnection(shared_ptr<NetworkServerConnection> connection) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(connection, "Missing accepted network connection");

    {
        scoped_lock locker {_connectionsLocker};

        if (!_shutdownStarted) {
            std::erase_if(_connections, [](const auto& tracked_connection) { return !tracked_connection.lock(); });
            _connections.emplace_back(connection);
            return true;
        }
    }

    connection->Disconnect();
    return false;
}

NetworkServerConnection::NetworkServerConnection(ptr<ServerNetworkSettings> settings) :
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();
}

void NetworkServerConnection::SetAsyncCallbacks(AsyncSendCallback send, AsyncReceiveCallback receive, DisconnectCallback disconnect)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(send, "Missing required send callback");
    FO_VERIFY_AND_THROW(receive, "Missing required receive callback");
    FO_VERIFY_AND_THROW(disconnect, "Missing required disconnect callback");

    if (_isDisconnected) {
        return;
    }

    {
        scoped_lock locker {_sendLocker};

        FO_VERIFY_AND_THROW(!_sendCallback, "Send callback is already set");
        _sendCallback = std::move(send);
    }

    {
        scoped_lock locker {_receiveLocker};

        FO_VERIFY_AND_THROW(!_disconnectCallback, "Disconnect callback is already set");

        _disconnectCallback = std::move(disconnect);
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

    {
        scoped_lock locker {_sendLocker};

        _sendCallback = {};
    }

    bool expected = false;
    bool disconnected_here = _isDisconnected.compare_exchange_strong(expected, true);

    if (disconnected_here) {
        DisconnectImpl();
    }

    // Taken on every call, not only the first: the owning ServerConnection disconnects from its own
    // destructor, and waiting here is what lets a running callback finish before its owner is gone
    scoped_lock locker {_receiveLocker};

    _receiveCallback = {};

    // Only the call that won the flag may take the disconnect callback: a nested call from the
    // transport teardown above would otherwise drop it before the winner reports the disconnect
    if (disconnected_here) {
        DisconnectCallback disconnect_callback = std::move(_disconnectCallback);
        _disconnectCallback = {};

        if (disconnect_callback) {
            disconnect_callback();
        }
    }
}

auto NetworkServerConnection::SendCallback() -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    // Held across the call, not just around the lookup: the sender may be destroyed by another thread,
    // and Disconnect() drops this callback under the same lock
    scoped_lock locker {_sendLocker};

    if (!_sendCallback) {
        return {};
    }

    return _sendCallback();
}

void NetworkServerConnection::ReceiveCallback(const_span<uint8_t> buf)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_receiveLocker};

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
    explicit DummyNetConnection(ptr<ServerNetworkSettings> settings, NetworkServer::DummyConnectionState state) :
        NetworkServerConnection(settings)
    {
        FO_STACK_TRACE_ENTRY();

        _host = "Dummy";

        if (state == NetworkServer::DummyConnectionState::Disconnected) {
            Disconnect();
        }
    }
    DummyNetConnection(const DummyNetConnection&) = delete;
    DummyNetConnection(DummyNetConnection&&) noexcept = delete;
    auto operator=(const DummyNetConnection&) = delete;
    auto operator=(DummyNetConnection&&) noexcept = delete;
    ~DummyNetConnection() override = default;

protected:
    void DispatchImpl() override { FO_NO_STACK_TRACE_ENTRY(); }

    void DisconnectImpl() override { FO_NO_STACK_TRACE_ENTRY(); }
};

auto NetworkServer::CreateDummyConnection(ptr<ServerNetworkSettings> settings, DummyConnectionState state) -> shared_ptr<NetworkServerConnection>
{
    FO_STACK_TRACE_ENTRY();

    return safe_alloc::make_shared<DummyNetConnection>(settings, state);
}

FO_END_NAMESPACE
