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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: automatically reconnect on network failtures

#pragma once

#include "Common.h"

#include "NetBuffer.h"
#include "Settings.h"

#define CHECK_SERVER_IN_BUF_ERROR(conn) \
    do { \
        if ((conn).InBuf.IsError()) { \
            WriteLog("Wrong network data!"); \
            (conn).Disconnect(); \
            return; \
        } \
    } while (0)

// Proxy types
constexpr auto PROXY_SOCKS4 = 1;
constexpr auto PROXY_SOCKS5 = 2;
constexpr auto PROXY_HTTP = 3;

class ServerConnection final
{
public:
    using ConnectCallback = std::function<void(bool success)>;
    using DisconnectCallback = std::function<void()>;
    using MessageCallback = std::function<void()>;

    ServerConnection() = delete;
    explicit ServerConnection(ClientNetworkSettings& settings);
    ServerConnection(const ServerConnection&) = delete;
    ServerConnection(ServerConnection&&) noexcept = delete;
    auto operator=(const ServerConnection&) = delete;
    auto operator=(ServerConnection&&) noexcept = delete;
    ~ServerConnection();

    [[nodiscard]] auto IsConnecting() const -> bool { return _isConnecting; }
    [[nodiscard]] auto IsConnected() const -> bool { return _isConnected; }
    [[nodiscard]] auto GetBytesSend() const -> size_t { return _bytesSend; }
    [[nodiscard]] auto GetBytesReceived() const -> size_t { return _bytesReceived; }
    [[nodiscard]] auto GetUnpackedBytesReceived() const -> size_t { return _bytesRealReceived; }

    void AddConnectHandler(ConnectCallback handler);
    void AddDisconnectHandler(DisconnectCallback handler);
    void AddMessageHandler(uint msg, MessageCallback handler);
    void Connect();
    void Process();
    void Disconnect();

    NetInBuffer& InBuf {_netIn};
    NetOutBuffer& OutBuf {_netOut};

private:
    struct Impl;

    auto ConnectToHost(string_view host, uint16 port) -> bool;
    auto ReceiveData(bool unpack) -> int;
    auto DispatchData() -> bool;
    auto CheckSocketStatus(bool for_write) -> bool;

    void Net_SendHandshake();
    void Net_OnPing();

    ClientNetworkSettings& _settings;
    Impl* _impl;
    ConnectCallback _connectCallback {};
    DisconnectCallback _disconnectCallback {};
    vector<uint8> _incomeBuf {};
    NetInBuffer _netIn;
    NetOutBuffer _netOut;
    bool _isConnecting {};
    bool _isConnected {};
    size_t _bytesSend {};
    size_t _bytesReceived {};
    size_t _bytesRealReceived {};
    unordered_map<uint, MessageCallback> _handlers {};
    time_point _pingTime {};
    time_point _pingCallTime {};
    size_t _msgCount {};
    bool _interthreadCommunication {};
    InterthreadDataCallback _interthreadSend {};
    vector<uint8> _interthreadReceived {};
    std::mutex _interthreadReceivedLocker {};
    std::atomic_bool _interthreadRequestDisconnect {};
    optional<time_point> _artificalLagTime {};
};
