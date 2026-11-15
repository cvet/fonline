// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

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
    return safe_alloc::make_unique<NetworkClientConnection_Interthread>(settings);
}

NetworkClientConnection_Interthread::NetworkClientConnection_Interthread(ptr<ClientNetworkSettings> settings) :
    NetworkClientConnection(settings)
{
    FO_TRACE_ZONE(Network);

    uint16_t port {numeric_cast<uint16_t>(_settings->Network.ServerPort)};

    optional<InterthreadListener> listener {FindInterthreadListener(port)};

    if (!listener.has_value()) {
        throw NetworkClientException("Interthread listener is not available", port);
    }

    _interthreadState = safe_alloc::make_shared<NetworkClientInterthreadState>();
    auto state {_interthreadState};

    _interthreadSend = (*listener)([state](const_span<uint8_t> buf) mutable FO_DEFERRED {
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

    logging::write("Connected to server via interthread communication");

    _isConnecting = false;
    _isConnected = true;
}

NetworkClientConnection_Interthread::~NetworkClientConnection_Interthread()
{
    DisconnectImpl();
}

auto NetworkClientConnection_Interthread::CheckStatusImpl(bool for_write) -> bool
{
    FO_TRACE_ZONE(Network);

    auto state {_interthreadState};

    if (state->RequestDisconnect) {
        Disconnect();
        return false;
    }

    scoped_lock locker {state->ReceivedLocker};

    return for_write ? true : !state->Received.empty();
}

auto NetworkClientConnection_Interthread::SendDataImpl(const_span<uint8_t> buf) -> size_t
{
    FO_TRACE_ZONE(Network);

    _interthreadSend(buf);

    return buf.size();
}

auto NetworkClientConnection_Interthread::ReceiveDataImpl(vector<uint8_t>& buf) -> size_t
{
    FO_TRACE_ZONE(Network);

    auto state {_interthreadState};

    scoped_lock locker {state->ReceivedLocker};

    FO_VERIFY_AND_THROW(!state->Received.empty(), "Interthread client receive was called without a pending packet", buf.size());
    size_t recv_size {state->Received.size()};

    while (buf.size() < recv_size) {
        buf.resize(buf.size() * 2);
    }

    memory::copy(buf.data(), state->Received.data(), recv_size);
    state->Received.clear();

    return recv_size;
}

void NetworkClientConnection_Interthread::DisconnectImpl() noexcept
{
    FO_TRACE_ZONE(Network);

    InterthreadDataCallback interthread_send {};
    auto state {_interthreadState};

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
