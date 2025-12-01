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

// Todo: automatically reconnect on network failures

#pragma once

#include "Common.h"

#include "NetBuffer.h"
#include "NetworkClient.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(ClientConnectionException);

class ClientConnection final
{
public:
    enum class ConnectResult : uint8
    {
        Success,
        Outdated,
        Failed,
    };

    using ConnectCallback = function<void(ConnectResult result)>;
    using DisconnectCallback = function<void()>;
    using MessageCallback = function<void()>;

    ClientConnection() = delete;
    explicit ClientConnection(ClientNetworkSettings& settings);
    ClientConnection(const ClientConnection&) = delete;
    ClientConnection(ClientConnection&&) noexcept = delete;
    auto operator=(const ClientConnection&) = delete;
    auto operator=(ClientConnection&&) noexcept = delete;
    ~ClientConnection() = default;

    [[nodiscard]] auto IsConnecting() const noexcept -> bool { return _netConnection && !_wasHandshake; }
    [[nodiscard]] auto IsConnected() const noexcept -> bool { return _netConnection && _wasHandshake; }
    [[nodiscard]] auto GetBytesSend() const noexcept -> size_t { return _bytesSend; }
    [[nodiscard]] auto GetBytesReceived() const noexcept -> size_t { return _bytesReceived; }
    [[nodiscard]] auto GetUnpackedBytesReceived() const noexcept -> size_t { return _bytesRealReceived; }

    void SetConnectHandler(ConnectCallback handler);
    void SetDisconnectHandler(DisconnectCallback handler);
    void AddMessageHandler(NetMessage msg, MessageCallback handler);
    void Connect();
    void Process();
    void Disconnect();

    NetInBuffer& InBuf {_netIn};
    NetOutBuffer& OutBuf {_netOut};

private:
    void ProcessConnection();
    auto ReceiveData() -> bool;
    void SendData();

    void Net_SendHandshake();
    void Net_OnHandshakeAnswer();
    void Net_OnPing();

    raw_ptr<ClientNetworkSettings> _settings;
    unique_ptr<NetworkClientConnection> _netConnection {};
    bool _connectingHandled {};
    bool _wasHandshake {};
    ConnectCallback _connectCallback {};
    DisconnectCallback _disconnectCallback {};
    NetInBuffer _netIn;
    NetOutBuffer _netOut;
    StreamDecompressor _decompressor {};
    vector<uint8> _unpackedReceivedBuf {};
    size_t _bytesSend {};
    size_t _bytesReceived {};
    size_t _bytesRealReceived {};
    unordered_map<NetMessage, MessageCallback> _handlers {};
    optional<nanotime> _artificalLagTime {};
    nanotime _pingTime {};
    nanotime _pingCallTime {};
    size_t _msgCount {};
#if FO_DEBUG
    vector<NetMessage> _msgHistory {};
#endif
};

FO_END_NAMESPACE();
