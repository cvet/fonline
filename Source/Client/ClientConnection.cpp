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

#include "ClientConnection.h"
#include "Updater.h"

FO_BEGIN_NAMESPACE

ClientConnection::ClientConnection(ptr<ClientNetworkSettings> settings) :
    _settings {settings},
    _netIn(_settings->NetBufferSize),
    _netOut(_settings->NetBufferSize)
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
        const auto port = numeric_cast<uint16_t>(_settings->ServerPort);

        bool has_interthread_listener = false;

        {
            scoped_lock locker {InterthreadListenersLocker};

            has_interthread_listener = InterthreadListeners.count(port) != 0;
        }

        if (has_interthread_listener) {
            _netConnection = NetworkClientConnection::CreateInterthreadConnection(_settings);
            _connectingOverUdp = false;
            _udpFallbackTried = false;
        }
        else if (_settings->UseUdp && build_condition<!FO_WEB>()) {
            try {
                CreateNetworkConnection(true);
                _udpFallbackTried = false;
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
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
        WriteLog("Connecting error: {}", ex.what());
        _connectCallback(ConnectResult::Failed);
    }
    catch (const NetworkClientException& ex) {
        WriteLog("Connection error: {}", ex.what());
        _connectCallback(ConnectResult::Failed);
    }
    catch (const NetBufferException& ex) {
        WriteLog("Connecting error: {}", ex.what());
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
        WriteLog("Connection error: {}", ex.what());
        Disconnect();
    }
    catch (const NetworkClientException& ex) {
        WriteLog("Connection error: {}", ex.what());

        if (!TryFallbackToTcp()) {
            Disconnect();
        }
    }
    catch (const NetBufferException& ex) {
        WriteLog("Connection error: {}", ex.what());
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

        if (_netConnection->IsConnected()) {
            Net_SendHandshake();
        }
        else if (TryFallbackToTcp()) {
            return;
        }
        else {
            Disconnect();
            return;
        }
    }

    // Lags emulation
    if (_settings->ArtificalLags != 0 && !_artificalLagTime.has_value()) {
        const auto lag_ms = std::uniform_int_distribution<int32_t> {_settings->ArtificalLags / 2, _settings->ArtificalLags}(_randomGenerator);
        _artificalLagTime = nanotime::now() + std::chrono::milliseconds {lag_ms};
    }

    if (_artificalLagTime.has_value()) {
        if (nanotime::now() >= _artificalLagTime.value()) {
            _artificalLagTime.reset();
        }
        else {
            return;
        }
    }

    _netConnection->CheckStatus(false);
    _netConnection->CheckStatus(true);

    // Receive and send data
    if (ReceiveData()) {
        while (_netIn.NeedProcess()) {
            const auto msg = _netIn.ReadMsg();

#if FO_DEBUG
            _msgHistory.insert(_msgHistory.begin(), msg);
#endif

            if (_settings->DebugNet) {
                _msgCount++;
                WriteLog("{}) Input net message {}", _msgCount, msg);
            }

            const auto it = _handlers.find(msg);

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

    if (_netOut.IsEmpty() && !_pingTime && _settings->PingPeriod != 0 && nanotime::now() >= _pingCallTime) {
        _netOut.StartMsg(NetMessage::Ping);
        _netOut.Write(false);
        _netOut.EndMsg();
        _pingTime = nanotime::now();
    }

    SendData();

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
    _netIn.ResetBuf();
    _netOut.ResetBuf();
    _decompressor.Reset();

    _netIn.SetEncryptKey(0);
    _netOut.SetEncryptKey(0);

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

    WriteLog("UDP connect failed, fallback to TCP for server '{}:{}'", _settings->ServerHost, _settings->ServerPort);

    _udpFallbackTried = true;
    _connectingHandled = false;
    CreateNetworkConnection(false);
    return true;
}

void ClientConnection::SendData()
{
    FO_STACK_TRACE_ENTRY();

    while (true) {
        if (_netOut.IsEmpty()) {
            break;
        }

        FO_VERIFY_AND_THROW(_netConnection, "Network connection is not established");

        if (!_netConnection->CheckStatus(true)) {
            break;
        }

        const auto send_buf = _netOut.GetData();
        const auto actual_send = _netConnection->SendData(send_buf);

        _netOut.DiscardWriteBuf(actual_send);
        _bytesSend += actual_send;
    }
}

auto ClientConnection::ReceiveData() -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_netConnection, "Network connection is not established");

    if (_netConnection->CheckStatus(false)) {
        const auto recv_buf = _netConnection->ReceiveData();
        FO_VERIFY_AND_THROW(!recv_buf.empty(), "Client connection reported readable network data but returned an empty receive buffer", _bytesReceived, _bytesRealReceived);

        _netIn.ShrinkReadBuf();

        if (!_settings->DisableZlibCompression) {
            _decompressor.Decompress(recv_buf, _unpackedReceivedBuf);
            _netIn.AddData(_unpackedReceivedBuf);
            _bytesReceived += recv_buf.size();
            _bytesRealReceived += _unpackedReceivedBuf.size();
        }
        else {
            _netIn.AddData(recv_buf);
            _bytesReceived += recv_buf.size();
            _bytesRealReceived += recv_buf.size();
        }

        return true;
    }

    return false;
}

void ClientConnection::Net_SendHandshake()
{
    FO_STACK_TRACE_ENTRY();

    std::uniform_int_distribution<int32_t> random_distribution {1, 255};
    const uint32_t encrypt_key = //
        (numeric_cast<uint32_t>(random_distribution(_randomGenerator)) << 24) | //
        (numeric_cast<uint32_t>(random_distribution(_randomGenerator)) << 16) | //
        (numeric_cast<uint32_t>(random_distribution(_randomGenerator)) << 8) | //
        (numeric_cast<uint32_t>(random_distribution(_randomGenerator)) << 0);
    const uint32_t updater_version = FO_UPDATER_VERSION;
    const string binary_update_target_name {GetCurrentBinaryUpdateTargetName()};

    _netOut.StartMsg(NetMessage::Handshake);
    _netOut.Write(_settings->CompatibilityVersion);
    _netOut.Write(updater_version);
    _netOut.Write(binary_update_target_name);
    _netOut.Write(encrypt_key);
    _netOut.EndMsg();

    _netOut.SetEncryptKey(encrypt_key);
}

void ClientConnection::Net_OnHandshakeAnswer()
{
    FO_STACK_TRACE_ENTRY();

    const auto compatibility_outdated = _netIn.Read<bool>();
    const auto updater_outdated = _netIn.Read<bool>();
    const auto encrypt_key = _netIn.Read<uint32_t>();

    _netIn.SetEncryptKey(encrypt_key);

    _wasHandshake = true;

    if (updater_outdated) {
        _connectCallback(ConnectResult::UpdaterOutdated);
    }
    else if (compatibility_outdated) {
        _connectCallback(ConnectResult::CompatibilityOutdated);
    }
    else {
        _connectCallback(ConnectResult::Success);
    }
}

void ClientConnection::Net_OnPing()
{
    FO_STACK_TRACE_ENTRY();

    const auto answer = _netIn.Read<bool>();

    if (answer) {
        const auto time = nanotime::now();
        _settings->Ping = (time - _pingTime).to_ms<int32_t>();
        _pingTime = nanotime::zero;
        _pingCallTime = time + std::chrono::milliseconds(_settings->PingPeriod);
    }
    else {
        _netOut.StartMsg(NetMessage::Ping);
        _netOut.Write(true);
        _netOut.EndMsg();
    }
}

FO_END_NAMESPACE
