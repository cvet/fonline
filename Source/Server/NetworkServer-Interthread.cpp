// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "NetworkServer.h"

FO_BEGIN_NAMESPACE

class NetworkServerConnection_Interthread : public NetworkServerConnection
{
public:
    explicit NetworkServerConnection_Interthread(ptr<ServerNetworkSettings> settings, InterthreadDataCallback send);
    NetworkServerConnection_Interthread(const NetworkServerConnection_Interthread&) = delete;
    NetworkServerConnection_Interthread(NetworkServerConnection_Interthread&&) noexcept = delete;
    auto operator=(const NetworkServerConnection_Interthread&) = delete;
    auto operator=(NetworkServerConnection_Interthread&&) noexcept = delete;
    ~NetworkServerConnection_Interthread() override = default;

    [[nodiscard]] auto NeedsPingWatchdog() const noexcept -> bool override { return false; }

    void Receive(const_span<uint8_t> buf);

private:
    void DispatchImpl() override;
    void DisconnectImpl() override;

    mutex _sendLocker {};
    InterthreadDataCallback _send FO_TSA_GUARDED_BY(_sendLocker);
};

class InterthreadServer : public NetworkServer
{
public:
    explicit InterthreadServer(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback);
    InterthreadServer() = delete;
    InterthreadServer(const InterthreadServer&) = delete;
    InterthreadServer(InterthreadServer&&) noexcept = delete;
    auto operator=(const InterthreadServer&) = delete;
    auto operator=(InterthreadServer&&) noexcept = delete;
    ~InterthreadServer() override = default;

    void ShutdownImpl() override;

private:
    uint16_t _virtualPort;
};

auto NetworkServer::StartInterthreadServer(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) -> unique_ptr<NetworkServer>
{
    FO_TRACE_ZONE(Network);

    return safe_alloc::make_unique<InterthreadServer>(settings, std::move(callback));
}

NetworkServerConnection_Interthread::NetworkServerConnection_Interthread(ptr<ServerNetworkSettings> settings, InterthreadDataCallback send) :
    NetworkServerConnection(settings),
    _send {std::move(send)}
{
}

void NetworkServerConnection_Interthread::Receive(const_span<uint8_t> buf)
{
    FO_TRACE_ZONE(Network);

    if (!buf.empty()) {
        ReceiveCallback(buf);
    }
    else {
        {
            scoped_lock locker {_sendLocker};

            _send = nullptr;
        }

        Disconnect();
    }
}

void NetworkServerConnection_Interthread::DispatchImpl()
{
    auto buf {SendCallback()};

    if (!buf.empty()) {
        scoped_lock locker {_sendLocker};

        if (_send) {
            _send(buf);
        }
    }
}

void NetworkServerConnection_Interthread::DisconnectImpl()
{
    scoped_lock locker {_sendLocker};

    if (_send) {
        _send({});
        _send = nullptr;
    }
}

InterthreadServer::InterthreadServer(ptr<ServerNetworkSettings> settings, NewConnectionCallback callback) :
    _virtualPort {numeric_cast<uint16_t>(settings->Network.ServerPort)}
{
    auto connection_registry {GetConnectionRegistry()};
    // The registry hands the listener out by copy, so the move-only connection callback travels behind a shared owner
    auto shared_callback {safe_alloc::make_shared<NewConnectionCallback>(std::move(callback))};
    bool added {AddInterthreadListener(_virtualPort, [connection_registry_ = std::move(connection_registry), settings, callback_ = std::move(shared_callback)](InterthreadDataCallback client_send) mutable -> InterthreadDataCallback FO_DEFERRED {
        auto conn {safe_alloc::make_shared<NetworkServerConnection_Interthread>(settings, std::move(client_send))};

        if (connection_registry_->TrackConnection(conn)) {
            (*callback_)(conn);
        }

        return [conn_ = conn](const_span<uint8_t> buf) mutable FO_DEFERRED { conn_->Receive(buf); };
    })};

    if (!added) {
        throw NetworkServerException("Port is busy", _virtualPort);
    }
}

void InterthreadServer::ShutdownImpl()
{
    FO_TRACE_ZONE(Network);

    bool listener_removed {RemoveInterthreadListener(_virtualPort)};
    FO_VERIFY_AND_THROW(listener_removed, "Interthread server shutdown cannot find the registered virtual port listener", _virtualPort);
}

FO_END_NAMESPACE
