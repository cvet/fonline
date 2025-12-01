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

#include "ClientConnection.h"
#include "NetCommand.h"
#include "Version-Include.h"

FO_BEGIN_NAMESPACE();

ClientConnection::ClientConnection(ClientNetworkSettings& settings) :
    _settings {&settings},
    _netIn(_settings->NetBufferSize),
    _netOut(_settings->NetBufferSize, _settings->NetDebugHashes)
{
    FO_STACK_TRACE_ENTRY();

    _connectCallback = [](auto&&) { };
    _disconnectCallback = [] { };

    AddMessageHandler(NetMessage::Disconnect, [this] { Disconnect(); });
    AddMessageHandler(NetMessage::Ping, [this] { Net_OnPing(); });
    AddMessageHandler(NetMessage::HandshakeAnswer, [this] { Net_OnHandshakeAnswer(); });
}

void ClientConnection::SetConnectHandler(ConnectCallback handler)
{
    FO_STACK_TRACE_ENTRY();

    _connectCallback = handler ? std::move(handler) : [](auto&&) { };
}

void ClientConnection::SetDisconnectHandler(DisconnectCallback handler)
{
    FO_STACK_TRACE_ENTRY();

    _disconnectCallback = handler ? std::move(handler) : [] { };
}

void ClientConnection::AddMessageHandler(NetMessage msg, MessageCallback handler)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_handlers.count(msg) == 0);

    _handlers.emplace(msg, std::move(handler));
}

void ClientConnection::Connect()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_netConnection);

    try {
        // First try interthread communication
        const auto port = numeric_cast<uint16>(_settings->ServerPort);

        if (InterthreadListeners.count(port) != 0) {
            _netConnection = NetworkClientConnection::CreateInterthreadConnection(*_settings);
        }
        else {
            _netConnection = NetworkClientConnection::CreateSocketsConnection(*_settings);
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
        Disconnect();
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
        else {
            Disconnect();
            return;
        }
    }

    // Lags emulation
    if (_settings->ArtificalLags != 0 && !_artificalLagTime.has_value()) {
        _artificalLagTime = nanotime::now() + std::chrono::milliseconds {GenericUtils::Random(_settings->ArtificalLags / 2, _settings->ArtificalLags)};
    }

    if (_artificalLagTime.has_value()) {
        if (nanotime::now() >= _artificalLagTime.value()) {
            _artificalLagTime.reset();
        }
        else {
            return;
        }
    }

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

    _connectingHandled = false;
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

void ClientConnection::SendData()
{
    FO_STACK_TRACE_ENTRY();

    while (true) {
        if (_netOut.IsEmpty()) {
            break;
        }
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

    if (_netConnection->CheckStatus(false)) {
        const auto recv_buf = _netConnection->ReceiveData();
        FO_RUNTIME_ASSERT(!recv_buf.empty());

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

    const auto comp_version = numeric_cast<uint32>(_settings->BypassCompatibilityCheck ? 0 : FO_COMPATIBILITY_VERSION);
    const auto encrypt_key = NetBuffer::GenerateEncryptKey();

    _netOut.StartMsg(NetMessage::Handshake);
    _netOut.Write(comp_version);
    _netOut.Write(encrypt_key);
    _netOut.EndMsg();

    _netOut.SetEncryptKey(encrypt_key);
}

void ClientConnection::Net_OnHandshakeAnswer()
{
    FO_STACK_TRACE_ENTRY();

    const auto outdated = _netIn.Read<bool>();
    const auto encrypt_key = _netIn.Read<uint32>();

    _netIn.SetEncryptKey(encrypt_key);

    _wasHandshake = true;
    _connectCallback(outdated ? ConnectResult::Outdated : ConnectResult::Success);
}

void ClientConnection::Net_OnPing()
{
    FO_STACK_TRACE_ENTRY();

    const auto answer = _netIn.Read<bool>();

    if (answer) {
        const auto time = nanotime::now();
        _settings->Ping = (time - _pingTime).to_ms<int32>();
        _pingTime = nanotime::zero;
        _pingCallTime = time + std::chrono::milliseconds(_settings->PingPeriod);
    }
    else {
        _netOut.StartMsg(NetMessage::Ping);
        _netOut.Write(true);
        _netOut.EndMsg();
    }
}

FO_END_NAMESPACE();
