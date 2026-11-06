// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "LinkDelay.h"
#include "NetBuffer.h"
#include "NetworkClient.h"
#include "SecureChannel.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ClientConnectionException);

class ClientConnection final
{
public:
    enum class ConnectResult : uint8_t
    {
        Success,
        CompatibilityOutdated,
        MetadataOutdated,
        UpdaterOutdated,
        Failed,
    };

    enum class ResourceMode : uint8_t
    {
        Platform,
        CommonForInterthread,
    };

    using ConnectCallback = function<void(ConnectResult result)>;
    using DisconnectCallback = function<void()>;
    using MessageCallback = function<void()>;

    ClientConnection() = delete;
    explicit ClientConnection(ptr<ClientNetworkSettings> settings, ResourceMode resource_mode);
    ClientConnection(const ClientConnection&) = delete;
    ClientConnection(ClientConnection&&) noexcept = delete;
    auto operator=(const ClientConnection&) = delete;
    auto operator=(ClientConnection&&) noexcept = delete;
    ~ClientConnection() = default;

    [[nodiscard]] auto IsConnecting() const noexcept -> bool { return _netConnection && !_wasHandshake; }
    [[nodiscard]] auto IsConnected() const noexcept -> bool { return _netConnection && _wasHandshake; }
    [[nodiscard]] auto GetBytesSend() const noexcept -> size_t { return _bytesSend; }
    [[nodiscard]] auto GetBytesReceived() const noexcept -> size_t { return _bytesReceived; }
    [[nodiscard]] auto GetPing() const noexcept -> int32_t { return _ping; }
    [[nodiscard]] auto GetUnpackedBytesReceived() const noexcept -> size_t { return _bytesRealReceived; }
    [[nodiscard]] auto GetServerMetadataVersion() const noexcept -> string_view { return _serverMetadataVersion; }

    void SetMetadataVersion(string_view version);
    void SetConnectHandler(ConnectCallback handler);
    void SetDisconnectHandler(DisconnectCallback handler);
    void AddMessageHandler(NetMessage msg, MessageCallback handler);
    void Connect();
    void Process();
    void Disconnect();
    void FlushPendingData();
    // A server message stamped with the server's clock: how much later than the link usually takes it arrived
    auto RegisterSenderTime(int64_t sender_ms, nanotime receive_time) -> timespan;

    ptr<NetInBuffer> InBuf {&_netIn};
    ptr<NetOutBuffer> OutBuf {&_netOut};

private:
    void CreateNetworkConnection(bool use_udp);
    void StartSecureChannel();
    void ResetConnectionState() noexcept;
    void ProcessConnection();
    auto ReceiveData() -> bool;
    void SendData();
    auto IsInboundLagged() -> bool;
    auto IsOutboundLagged() -> bool;
    auto IsArtificalLagPending(optional<nanotime>& deadline, bool has_data) -> bool;
    auto TryFallbackToTcp() -> bool;

    void Net_SendHandshake();
    void Net_OnHandshakeAnswer();
    void Net_OnPing();

    ptr<ClientNetworkSettings> _settings;
    unique_nptr<NetworkClientConnection> _netConnection {};
    optional<SecureChannel> _channel {};
    vector<uint8_t> _channelPlaintext {};
    vector<uint8_t> _sealedOut {};
    ResourceMode _resourceMode;
    bool _connectingOverInterthread {};
    bool _connectingOverUdp {};
    bool _connectingHandled {};
    bool _udpFallbackTried {};
    bool _wasHandshake {};
    string _metadataVersion {};
    string _serverMetadataVersion {};
    ConnectCallback _connectCallback {};
    DisconnectCallback _disconnectCallback {};
    NetInBuffer _netIn;
    NetOutBuffer _netOut;
    stream_decompressor _decompressor {};
    vector<uint8_t> _unpackedReceivedBuf {};
    size_t _bytesSend {};
    size_t _bytesReceived {};
    size_t _bytesRealReceived {};
    unordered_map<NetMessage, MessageCallback> _handlers {};
    optional<nanotime> _artificalInboundLagTime {};
    optional<nanotime> _artificalOutboundLagTime {};
    random_generator _randomGenerator {};
    int32_t _ping {};
    LinkDelayEstimator _downlinkDelay;
    nanotime _pingTime {};
    nanotime _pingCallTime {};
    nanotime _lastReceiveTime {};
    size_t _msgCount {};
#if FO_DEBUG
    static constexpr size_t NET_MESSAGE_HISTORY_LIMIT {1024};
    deque<NetMessage> _msgHistory {};
#endif
};

FO_END_NAMESPACE
