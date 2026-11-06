// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(NetworkClientException);

class NetworkClientConnection
{
public:
    explicit NetworkClientConnection(ptr<ClientNetworkSettings> settings);
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
    auto SendData(const_span<uint8_t> buf) -> size_t;
    auto ReceiveData() -> const_span<uint8_t>;
    void Disconnect() noexcept;

    static auto CreateInterthreadConnection(ptr<ClientNetworkSettings> settings) -> unique_ptr<NetworkClientConnection>;
    static auto CreateSocketsConnection(ptr<ClientNetworkSettings> settings) -> unique_ptr<NetworkClientConnection>;
    static auto CreateUdpSocketsConnection(ptr<ClientNetworkSettings> settings) -> unique_ptr<NetworkClientConnection>;

protected:
    virtual auto CheckStatusImpl(bool for_write) -> bool = 0;
    virtual auto SendDataImpl(const_span<uint8_t> buf) -> size_t = 0;
    virtual auto ReceiveDataImpl(vector<uint8_t>& buf) -> size_t = 0;
    virtual void DisconnectImpl() noexcept = 0;

    ptr<ClientNetworkSettings> _settings;
    bool _isConnecting {};
    bool _isConnected {};

private:
    size_t _bytesSend {};
    size_t _bytesReceived {};
    vector<uint8_t> _incomeBuf {};
};

FO_END_NAMESPACE
