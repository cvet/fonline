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

#include "ClientConnection.h"
#include "Updater.h"

FO_BEGIN_NAMESPACE

ClientConnection::ClientConnection(ptr<ClientNetworkSettings> settings) :
    _settings {settings},
    _netIn(_settings->Network.NetBufferSize),
    _netOut(_settings->Network.NetBufferSize)
{
    FO_STACK_TRACE_ENTRY();

    _connectCallback = [](auto&&) FO_DEFERRED { };
    _disconnectCallback = []() FO_DEFERRED { };

    AddMessageHandler(NetMessage::Disconnect, [this]() FO_DEFERRED { Disconnect(); });
    AddMessageHandler(NetMessage::Ping, [this]() FO_DEFERRED { Net_OnPing(); });
    AddMessageHandler(NetMessage::HandshakeAnswer, [this]() FO_DEFERRED { Net_OnHandshakeAnswer(); });
}

void ClientConnection::SetConnectHandler(ConnectCallback handler)
{
    FO_STACK_TRACE_ENTRY();

    _connectCallback = handler ? std::move(handler) : [](auto&&) FO_DEFERRED { };
}

void ClientConnection::SetDisconnectHandler(DisconnectCallback handler)
{
    FO_STACK_TRACE_ENTRY();

    _disconnectCallback = handler ? std::move(handler) : []() FO_DEFERRED { };
}

void ClientConnection::AddMessageHandler(NetMessage msg, MessageCallback handler)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_handlers.count(msg) == 0, "Duplicate client network message handler registration", msg, _handlers.size());

    _handlers.emplace(msg, std::move(handler));
}

void ClientConnection::CreateNetworkConnection(bool use_udp)
{
    FO_STACK_TRACE_ENTRY();

    auto connection = use_udp ? NetworkClientConnection::CreateUdpSocketsConnection(_settings) : NetworkClientConnection::CreateSocketsConnection(_settings);

    _connectingOverUdp = use_udp;
    _netConnection = std::move(connection);
}

void ClientConnection::Connect()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_netConnection, "Net connection is already set");

    try {
        // First try interthread communication
        auto port = numeric_cast<uint16_t>(_settings->Network.ServerPort);

        if (HasInterthreadListener(port)) {
            _netConnection = NetworkClientConnection::CreateInterthreadConnection(_settings);
            _connectingOverUdp = false;
            _udpFallbackTried = false;
        }
        else if (_settings->ClientNetwork.UseUdp && build_condition<!FO_WEB>()) {
            try {
                CreateNetworkConnection(true);
                _udpFallbackTried = false;
            }
            catch (const std::exception& ex) {
                exceptions::report_and_continue(ex);
                _udpFallbackTried = true;
                CreateNetworkConnection(false);
            }
        }
        else {
            CreateNetworkConnection(false);
            _udpFallbackTried = false;
        }
    }
    catch (const ClientConnectionException& ex) {
        logging::write("Connecting error: {}", ex.what());
        _connectCallback(ConnectResult::Failed);
    }
    catch (const NetworkClientException& ex) {
        logging::write("Connection error: {}", ex.what());
        _connectCallback(ConnectResult::Failed);
    }
    catch (const NetBufferException& ex) {
        logging::write("Connecting error: {}", ex.what());
        _connectCallback(ConnectResult::Failed);
    }
    catch (...) {
        safe_call([this] { _connectCallback(ConnectResult::Failed); });
        throw;
    }
}

void ClientConnection::Process()
{
    FO_STACK_TRACE_ENTRY();

    try {
        ProcessConnection();
    }
    catch (const ClientConnectionException& ex) {
        logging::write("Connection error: {}", ex.what());
        Disconnect();
    }
    catch (const NetworkClientException& ex) {
        logging::write("Connection error: {}", ex.what());

        if (!TryFallbackToTcp()) {
            Disconnect();
        }
    }
    catch (const NetBufferException& ex) {
        logging::write("Connection error: {}", ex.what());
        Disconnect();
    }
    catch (const DecompressException& ex) {
        logging::write("Connection error: {}", ex.what());
        Disconnect();
    }
    catch (const NoiseException& ex) {
        logging::write("Secure channel error: {}", ex.what());
        Disconnect();
    }
    catch (...) {
        safe_call([this] { Disconnect(); });
        throw;
    }
}

void ClientConnection::ProcessConnection()
{
    FO_STACK_TRACE_ENTRY();

    if (!_netConnection) {
        return;
    }

    // Wait for connecting result
    if (_netConnection->IsConnecting()) {
        _netConnection->CheckStatus(true);

        // Connecting status may change
        if (_netConnection->IsConnecting()) {
            return;
        }
    }

    // Handle connecting result
    if (!_connectingHandled) {
        _connectingHandled = true;

        // The handshake message follows once the channel stands, from ReceiveData
        if (_netConnection->IsConnected()) {
            StartSecureChannel();
        }
        else if (TryFallbackToTcp()) {
            return;
        }
        else {
            Disconnect();
            return;
        }
    }

    _netConnection->CheckStatus(false);
    _netConnection->CheckStatus(true);

    // Receive and send data
    if (ReceiveData()) {
        _lastReceiveTime = nanotime::now();
    }

    // A server that vanished without closing the connection never answers again, and UDP or a half-open TCP link has
    // no other way to tell. Any arriving data proves it alive, so a large portion ahead of the answer is no silence
    bool awaits_answer = _pingTime || (_channel && !_channel->IsEstablished());

    if (awaits_answer && _settings->ClientNetwork.PingTimeout != 0 && !is_run_in_debugger()) {
        nanotime silent_since = std::max(_pingTime, _lastReceiveTime);

        if (nanotime::now() - silent_since >= std::chrono::milliseconds {_settings->ClientNetwork.PingTimeout}) {
            logging::write("Connection lost: the server has sent nothing for {} ms", (nanotime::now() - silent_since).to_ms<int32_t>());
            Disconnect();
            return;
        }
    }

    if (!IsInboundLagged()) {
        while (_netIn.NeedProcess()) {
            auto msg = _netIn.ReadMsg();

#if FO_DEBUG
            _msgHistory.push_front(msg);

            if (_msgHistory.size() > NET_MESSAGE_HISTORY_LIMIT) {
                _msgHistory.pop_back();
            }
#endif

            if (_settings->ClientNetwork.DebugNet) {
                _msgCount++;
                logging::write("{}) Input net message {}", _msgCount, msg);
            }

            auto it = _handlers.find(msg);

            if (it != _handlers.end()) {
                if (it->second) {
                    it->second();
                }
            }
            else {
                throw ClientConnectionException("No handler for message", msg);
            }

            // State may change during message processing
            if (!_netConnection) {
                return;
            }
        }
    }

    bool handshake_sent = _channel && _channel->IsEstablished();

    if (handshake_sent && _netOut.IsEmpty() && !_pingTime && _settings->ClientNetwork.PingPeriod != 0 && nanotime::now() >= _pingCallTime) {
        _netOut.StartMsg(NetMessage::Ping);
        _netOut.Write(false);
        _netOut.EndMsg();
        _pingTime = nanotime::now();
    }

    if (!IsOutboundLagged()) {
        SendData();
    }

    // Handle disconnect
    if (!_netConnection->IsConnected()) {
        Disconnect();
    }
}

void ClientConnection::Disconnect()
{
    FO_STACK_TRACE_ENTRY();

    if (!_netConnection) {
        return;
    }

    _netConnection->Disconnect();
    _netConnection.reset();

    _connectingOverUdp = false;
    _connectingHandled = false;
    _udpFallbackTried = false;
    ResetConnectionState();

    if (!_wasHandshake) {
        _connectCallback(ConnectResult::Failed);
    }
    else {
        _wasHandshake = false;
        _disconnectCallback();
    }
}

void ClientConnection::FlushPendingData()
{
    FO_STACK_TRACE_ENTRY();

    SendData();
}

auto ClientConnection::TryFallbackToTcp() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_connectingOverUdp || _udpFallbackTried) {
        return false;
    }

    logging::write("UDP connect failed, fallback to TCP for server '{}:{}'", _settings->ClientNetwork.ServerHost, _settings->Network.ServerPort);

    _udpFallbackTried = true;
    _connectingHandled = false;
    ResetConnectionState();
    CreateNetworkConnection(false);
    return true;
}

void ClientConnection::StartSecureChannel()
{
    FO_STACK_TRACE_ENTRY();

    vector<crypto::key_bytes> server_keys;
    server_keys.reserve(_settings->ClientNetwork.ChannelServerKeys.size());

    for (const string& server_key : _settings->ClientNetwork.ChannelServerKeys) {
        server_keys.emplace_back(ParseSecureChannelKey(server_key, "ClientNetwork.ChannelServerKeys"));
    }

    _channel.emplace(server_keys);

    // The transport answering is the last sign of life until the channel handshake is answered
    _lastReceiveTime = nanotime::now();
}

void ClientConnection::ResetConnectionState() noexcept
{
    FO_STACK_TRACE_ENTRY();

    // Every connection starts its own channel and compressed stream, so nothing buffered for the last one survives
    _channel.reset();
    _channelPlaintext.clear();
    _sealedOut.clear();
    _pingTime = nanotime::zero;
    _pingCallTime = nanotime::zero;
    _lastReceiveTime = nanotime::zero;
    _artificalInboundLagTime.reset();
    _artificalOutboundLagTime.reset();
    _netIn.ResetBuf();
    _netOut.ResetBuf();
    _decompressor.reset();
}

void ClientConnection::SendData()
{
    FO_STACK_TRACE_ENTRY();

    // Until the transport connects there is no channel, and nothing may leave unsealed
    if (!_channel) {
        return;
    }

    _channel->TakeHandshakeOutput(_sealedOut);

    if (_channel->IsEstablished() && !_netOut.IsEmpty()) {
        auto plain_buf = _netOut.GetData();
        _channel->Seal(plain_buf, _sealedOut);
        _netOut.DiscardWriteBuf(plain_buf.size());
    }

    while (!_sealedOut.empty()) {
        FO_VERIFY_AND_THROW(_netConnection, "Network connection is not established");

        if (!_netConnection->CheckStatus(true)) {
            break;
        }

        size_t actual_send = _netConnection->SendData(_sealedOut);

        _sealedOut.erase(_sealedOut.begin(), _sealedOut.begin() + numeric_cast<ptrdiff_t>(actual_send));
        _bytesSend += actual_send;
    }
}

// Symmetric by design: only the outbound half makes the server's copy of the world trail the client's,
// and neither half throttles throughput. Delay distribution and its effect: Docs/Debugging.md

auto ClientConnection::IsInboundLagged() -> bool
{
    FO_STACK_TRACE_ENTRY();

    return IsArtificalLagPending(_artificalInboundLagTime, _netIn.NeedProcess());
}

auto ClientConnection::IsOutboundLagged() -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool has_data = !_netOut.IsEmpty() || !_sealedOut.empty() || (_channel && _channel->HasHandshakeOutput());
    return IsArtificalLagPending(_artificalOutboundLagTime, has_data);
}

auto ClientConnection::IsArtificalLagPending(optional<nanotime>& deadline, bool has_data) -> bool
{
    FO_STACK_TRACE_ENTRY();

    // A negative value builds a distribution whose lower bound exceeds its upper bound, which is undefined
    // rather than merely odd, and the zero early-out below does not screen it
    FO_VERIFY_AND_THROW(_settings->Network.ArtificalLags >= 0 && _settings->Network.ArtificalLagsJitter >= 0, "Artifical lag settings must not be negative", _settings->Network.ArtificalLags, _settings->Network.ArtificalLagsJitter);

    if ((_settings->Network.ArtificalLags == 0 && _settings->Network.ArtificalLagsJitter == 0) || !has_data) {
        deadline.reset();
        return false;
    }

    if (!deadline.has_value()) {
        // Both settings are free-form milliseconds, so accumulate wide enough that their sum cannot
        // overflow before the duration is built
        int64_t lag_ms = _randomGenerator.next_between(_settings->Network.ArtificalLags / 2, _settings->Network.ArtificalLags);

        if (_settings->Network.ArtificalLagsJitter != 0) {
            lag_ms += _randomGenerator.next_between(0, _settings->Network.ArtificalLagsJitter);
        }

        deadline = nanotime::now() + std::chrono::milliseconds {lag_ms};
    }

    if (nanotime::now() >= deadline.value()) {
        deadline.reset();
        return false;
    }

    return true;
}

auto ClientConnection::ReceiveData() -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_netConnection, "Network connection is not established");
    FO_VERIFY_AND_THROW(_channel, "Secure channel is not started on a connected transport");

    if (!_netConnection->CheckStatus(false)) {
        return false;
    }

    auto recv_buf = _netConnection->ReceiveData();
    FO_VERIFY_AND_THROW(!recv_buf.empty(), "Client connection reported readable network data but returned an empty receive buffer", _bytesReceived, _bytesRealReceived);
    _bytesReceived += recv_buf.size();

    _netIn.ShrinkReadBuf();

    bool was_established = _channel->IsEstablished();

    _channelPlaintext.clear();
    _channel->Receive(recv_buf, _channelPlaintext);

    // The server answers nothing before the handshake message, so it goes out the moment the channel stands
    if (!was_established && _channel->IsEstablished()) {
        Net_SendHandshake();
    }

    // Handshake frames and a partial frame leave nothing for the stream yet
    if (_channelPlaintext.empty()) {
        return true;
    }

    if (!_settings->Network.DisableZlibCompression) {
        _decompressor.decompress(_channelPlaintext, _unpackedReceivedBuf);
        _netIn.AddData(_unpackedReceivedBuf);
        _bytesRealReceived += _unpackedReceivedBuf.size();
    }
    else {
        _netIn.AddData(_channelPlaintext);
        _bytesRealReceived += _channelPlaintext.size();
    }

    return true;
}

void ClientConnection::SetMetadataVersion(string_view version)
{
    FO_STACK_TRACE_ENTRY();

    _metadataVersion = version;
}

void ClientConnection::Net_SendHandshake()
{
    FO_STACK_TRACE_ENTRY();

    uint32_t updater_version = FO_UPDATER_VERSION;
    string binary_update_target_name {GetCurrentBinaryUpdateTargetName()};

    _netOut.StartMsg(NetMessage::Handshake);
    _netOut.Write(_settings->Network.CompatibilityVersion);
    _netOut.Write(_metadataVersion);
    _netOut.Write(updater_version);
    _netOut.Write(binary_update_target_name);
    _netOut.EndMsg();
}

void ClientConnection::Net_OnHandshakeAnswer()
{
    FO_STACK_TRACE_ENTRY();

    bool compatibility_outdated = _netIn.Read<bool>();
    bool updater_outdated = _netIn.Read<bool>();
    bool metadata_outdated = _netIn.Read<bool>();
    _serverMetadataVersion = _netIn.Read<string>();

    _wasHandshake = true;

    if (updater_outdated) {
        _connectCallback(ConnectResult::UpdaterOutdated);
    }
    else if (compatibility_outdated) {
        _connectCallback(ConnectResult::CompatibilityOutdated);
    }
    else if (metadata_outdated) {
        _connectCallback(ConnectResult::MetadataOutdated);
    }
    else {
        _connectCallback(ConnectResult::Success);
    }
}

void ClientConnection::Net_OnPing()
{
    FO_STACK_TRACE_ENTRY();

    bool answer = _netIn.Read<bool>();

    if (answer) {
        nanotime time = nanotime::now();
        _ping = (time - _pingTime).to_ms<int32_t>();
        _pingTime = nanotime::zero;
        _pingCallTime = time + std::chrono::milliseconds(_settings->ClientNetwork.PingPeriod);
    }
    else {
        _netOut.StartMsg(NetMessage::Ping);
        _netOut.Write(true);
        _netOut.EndMsg();
    }
}

FO_END_NAMESPACE
