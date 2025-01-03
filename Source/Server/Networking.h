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

class NetConnection
{
public:
    explicit NetConnection(ServerNetworkSettings& settings);
    NetConnection(const NetConnection&) = delete;
    NetConnection(NetConnection&&) noexcept = delete;
    auto operator=(const NetConnection&) = delete;
    auto operator=(NetConnection&&) noexcept = delete;
    virtual ~NetConnection() = default;

    [[nodiscard]] virtual auto GetIp() const noexcept -> uint = 0;
    [[nodiscard]] virtual auto GetHost() const noexcept -> string_view = 0;
    [[nodiscard]] virtual auto GetPort() const noexcept -> uint16 = 0;
    [[nodiscard]] virtual auto IsDisconnected() const noexcept -> bool = 0;
    [[nodiscard]] virtual auto IsWebConnection() const noexcept -> bool = 0;
    [[nodiscard]] virtual auto IsInterthreadConnection() const noexcept -> bool = 0;

    virtual void DisableCompression() = 0;
    virtual void Dispatch() = 0;
    virtual void Disconnect() = 0;

    void AddRef() const noexcept;
    void Release() const noexcept;

    NetInBuffer InBuf;
    std::mutex InBufLocker {};
    NetOutBuffer OutBuf;
    std::mutex OutBufLocker {};

private:
    mutable std::atomic_int _refCount {1};
};

class DummyNetConnection : public NetConnection
{
public:
    explicit DummyNetConnection(ServerNetworkSettings& settings) :
        NetConnection(settings)
    {
    }
    DummyNetConnection(const DummyNetConnection&) = delete;
    DummyNetConnection(DummyNetConnection&&) noexcept = delete;
    auto operator=(const DummyNetConnection&) = delete;
    auto operator=(DummyNetConnection&&) noexcept = delete;
    ~DummyNetConnection() override = default;

    auto GetIp() const noexcept -> uint override { return 0; }
    auto GetHost() const noexcept -> string_view override { return "Dummy"; }
    auto GetPort() const noexcept -> uint16 override { return 0; }
    auto IsDisconnected() const noexcept -> bool override { return false; }
    auto IsWebConnection() const noexcept -> bool override { return false; }
    auto IsInterthreadConnection() const noexcept -> bool override { return false; }

    void DisableCompression() override { }
    void Dispatch() override { }
    void Disconnect() override { }
};

class NetServerBase
{
public:
    using ConnectionCallback = std::function<void(NetConnection*)>;

    NetServerBase() = default;
    NetServerBase(const NetServerBase&) = delete;
    NetServerBase(NetServerBase&&) noexcept = delete;
    auto operator=(const NetServerBase&) = delete;
    auto operator=(NetServerBase&&) noexcept = delete;
    virtual ~NetServerBase() = default;

    virtual void Shutdown() = 0;

    [[nodiscard]] static auto StartTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> NetServerBase*;
    [[nodiscard]] static auto StartWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> NetServerBase*;
    [[nodiscard]] static auto StartInterthreadServer(ServerNetworkSettings& settings, ConnectionCallback callback) -> NetServerBase*;
};
