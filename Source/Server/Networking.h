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

#pragma once

#include "Common.h"

#include "NetBuffer.h"
#include "Settings.h"

DECLARE_EXCEPTION(NetworkException);

struct z_stream_s;

class NetConnection : public std::enable_shared_from_this<NetConnection>
{
public:
    NetConnection() = delete;
    NetConnection(const NetConnection&) = delete;
    NetConnection(NetConnection&&) noexcept = delete;
    auto operator=(const NetConnection&) = delete;
    auto operator=(NetConnection&&) noexcept = delete;
    virtual ~NetConnection() = default;

    [[nodiscard]] auto GetIp() const noexcept -> uint { return _ip; }
    [[nodiscard]] auto GetHost() const noexcept -> string_view { return _host; }
    [[nodiscard]] auto GetPort() const noexcept -> uint16 { return _port; }
    [[nodiscard]] auto IsDisconnected() const noexcept -> bool { return _isDisconnected; }
    [[nodiscard]] virtual auto IsWebConnection() const noexcept -> bool { return false; }
    [[nodiscard]] virtual auto IsInterthreadConnection() const noexcept -> bool { return false; }

    void DisableCompression();
    void Dispatch();
    void Disconnect();

    NetInBuffer InBuf;
    std::mutex InBufLocker {};
    NetOutBuffer OutBuf;
    std::mutex OutBufLocker {};

protected:
    explicit NetConnection(ServerNetworkSettings& settings);
    virtual void DispatchImpl() = 0;
    virtual void DisconnectImpl() = 0;

    auto SendCallback() -> const_span<uint8>;
    void ReceiveCallback(const uint8* buf, size_t len);

    ServerNetworkSettings& _settings;
    uint _ip {};
    string _host {};
    uint16 _port {};
    std::atomic_bool _isDisconnected {};

private:
    unique_del_ptr<z_stream_s> _zStream {};
    vector<uint8> _outBuf {};
};

class DummyNetConnection : public NetConnection
{
public:
    explicit DummyNetConnection(ServerNetworkSettings& settings) :
        NetConnection(settings)
    {
        _host = "Dummy";
        _isDisconnected = true;
    }
    DummyNetConnection(const DummyNetConnection&) = delete;
    DummyNetConnection(DummyNetConnection&&) noexcept = delete;
    auto operator=(const DummyNetConnection&) = delete;
    auto operator=(DummyNetConnection&&) noexcept = delete;
    ~DummyNetConnection() override = default;

protected:
    void DispatchImpl() override { }
    void DisconnectImpl() override { }
};

class NetServerBase
{
public:
    using ConnectionCallback = std::function<void(shared_ptr<NetConnection>)>;

    NetServerBase() = default;
    NetServerBase(const NetServerBase&) = delete;
    NetServerBase(NetServerBase&&) noexcept = delete;
    auto operator=(const NetServerBase&) = delete;
    auto operator=(NetServerBase&&) noexcept = delete;
    virtual ~NetServerBase() = default;

    virtual void Shutdown() = 0;

    [[nodiscard]] static auto StartTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> unique_ptr<NetServerBase>;
    [[nodiscard]] static auto StartWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> unique_ptr<NetServerBase>;
    [[nodiscard]] static auto StartInterthreadServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> unique_ptr<NetServerBase>;
};
