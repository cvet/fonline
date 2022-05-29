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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

class NetConnection
{
public:
    NetConnection() = default;
    NetConnection(const NetConnection&) = delete;
    NetConnection(NetConnection&&) noexcept = delete;
    auto operator=(const NetConnection&) = delete;
    auto operator=(NetConnection&&) noexcept = delete;
    virtual ~NetConnection() = default;

    [[nodiscard]] virtual auto GetIp() const -> uint = 0;
    [[nodiscard]] virtual auto GetHost() const -> string_view = 0;
    [[nodiscard]] virtual auto GetPort() const -> ushort = 0;
    [[nodiscard]] virtual auto IsDisconnected() const -> bool = 0;
    [[nodiscard]] virtual auto IsWebConnection() const -> bool = 0;

    virtual void DisableCompression() = 0;
    virtual void Dispatch() = 0;
    virtual void Disconnect() = 0;

    void AddRef() const;
    void Release() const;

    NetInBuffer Bin {};
    std::mutex BinLocker {};
    NetOutBuffer Bout {};
    std::mutex BoutLocker {};

private:
    mutable std::atomic_int _refCount {1};
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

    [[nodiscard]] static auto StartTcpServer(ServerNetworkSettings& settings, const ConnectionCallback& callback) -> NetServerBase*;
    [[nodiscard]] static auto StartWebSocketsServer(ServerNetworkSettings& settings, const ConnectionCallback& callback) -> NetServerBase*;
};
