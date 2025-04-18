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
#include "GenericUtils.h"
#include "Log.h"
#include "NetCommand.h"
#include "StringUtils.h"
#include "Version-Include.h"

ClientConnection::ClientConnection(ClientNetworkSettings& settings) :
    _settings {settings},
    _netIn(_settings.NetBufferSize),
    _netOut(_settings.NetBufferSize)
{
    STACK_TRACE_ENTRY();

    AddMessageHandler(NetMessage::Disconnect, [this] { Disconnect(); });
    AddMessageHandler(NetMessage::Ping, [this] { Net_OnPing(); });
}

void ClientConnection::SetConnectHandler(ConnectCallback handler)
{
    STACK_TRACE_ENTRY();

    _connectCallback = std::move(handler);
}

void ClientConnection::SetDisconnectHandler(DisconnectCallback handler)
{
    STACK_TRACE_ENTRY();

    _disconnectCallback = std::move(handler);
}

void ClientConnection::AddMessageHandler(NetMessage msg, MessageCallback handler)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_handlers.count(msg) == 0);

    _handlers.emplace(msg, std::move(handler));
}

void ClientConnection::Connect()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_netConnection);

    try {
        // First try interthread communication
        const auto port = numeric_cast<uint16>(_settings.ServerPort);

        if (InterthreadListeners.count(port) != 0) {
            _netConnection = NetworkClientConnection::CreateInterthreadConnection(_settings);
        }
        else {
            _netConnection = NetworkClientConnection::CreateSocketsConnection(_settings);
        }
    }
    catch (const ClientConnectionException& ex) {
        WriteLog("Connecting error: {}", ex.what());

        if (_connectCallback) {
            _connectCallback(false);
        }
    }
    catch (const NetBufferException& ex) {
        WriteLog("Connecting error: {}", ex.what());

        if (_connectCallback) {
            _connectCallback(false);
        }
    }
    catch (...) {
        if (_connectCallback) {
            safe_call([this] { _connectCallback(false); });
        }

        throw;
    }
}

void ClientConnection::Process()
{
    STACK_TRACE_ENTRY();

    try {
        ProcessConnection();
    }
    catch (const ClientConnectionException& ex) {
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
    STACK_TRACE_ENTRY();

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

        if (_connectCallback) {
            _connectCallback(_netConnection->IsConnected());
        }

        if (!_netConnection || !_netConnection->IsConnected()) {
            Disconnect();
            return;
        }
    }

    // Lags emulation
    if (_settings.ArtificalLags != 0 && !_artificalLagTime.has_value()) {
        _artificalLagTime = nanotime::now() + std::chrono::milliseconds {GenericUtils::Random(_settings.ArtificalLags / 2, _settings.ArtificalLags)};
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

            if (_settings.DebugNet) {
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

    if (_netOut.IsEmpty() && !_pingTime && _settings.PingPeriod != 0 && nanotime::now() >= _pingCallTime) {
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
    STACK_TRACE_ENTRY();

    if (!_netConnection) {
        return;
    }

    const auto was_connecting = _netConnection->IsConnecting();

    _netConnection->Disconnect();
    _netConnection.reset();

    _connectingHandled = false;
    _netIn.ResetBuf();
    _netOut.ResetBuf();
    _decompressor.Reset();

    _netIn.SetEncryptKey(0);
    _netOut.SetEncryptKey(0);

    if (was_connecting) {
        if (_connectCallback) {
            _connectCallback(false);
        }
    }
    else {
        if (_disconnectCallback) {
            _disconnectCallback();
        }
    }
}

void ClientConnection::SendData()
{
    STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

    if (_netConnection->CheckStatus(false)) {
        const auto recv_buf = _netConnection->ReceiveData();
        RUNTIME_ASSERT(!recv_buf.empty());

        _netIn.ShrinkReadBuf();

        if (!_settings.DisableZlibCompression) {
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
    STACK_TRACE_ENTRY();

    const auto encrypt_key = NetBuffer::GenerateEncryptKey();

    _netOut.StartMsg(NetMessage::Handshake);
    _netOut.Write(static_cast<uint>(FO_COMPATIBILITY_VERSION));
    _netOut.Write(encrypt_key);
    _netOut.EndMsg();

    _netOut.SetEncryptKey(encrypt_key);
    _netIn.SetEncryptKey(encrypt_key);
}

void ClientConnection::Net_OnPing()
{
    STACK_TRACE_ENTRY();

    const auto answer = _netIn.Read<bool>();

    if (answer) {
        const auto time = nanotime::now();
        _settings.Ping = (time - _pingTime).to_ms<uint>();
        _pingTime = nanotime::zero;
        _pingCallTime = time + std::chrono::milliseconds {_settings.PingPeriod};
    }
    else {
        _netOut.StartMsg(NetMessage::Ping);
        _netOut.Write(true);
        _netOut.EndMsg();
    }
}
