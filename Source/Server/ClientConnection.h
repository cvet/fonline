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

#pragma once

#include "Common.h"

#include "NetBuffer.h"

#define CHECK_CLIENT_IN_BUF_ERROR(conn) \
    do { \
        if ((conn)->InBuf.IsError()) { \
            WriteLog("Wrong network data from host '{}'", (conn)->GetHost()); \
            (conn)->HardDisconnect(); \
            return; \
        } \
    } while (0)

#define CONNECTION_OUTPUT_BEGIN(conn) \
    { \
        std::lock_guard conn_locker__((conn)->OutBufLocker)
#define CONNECTION_OUTPUT_END(conn) \
    } \
    (conn)->Dispatch()

class NetConnection;

class ClientConnection final
{
public:
    ClientConnection() = delete;
    explicit ClientConnection(NetConnection* net_connection);
    ClientConnection(const ClientConnection&) = delete;
    ClientConnection(ClientConnection&&) noexcept = delete;
    auto operator=(const ClientConnection&) = delete;
    auto operator=(ClientConnection&&) noexcept = delete;
    ~ClientConnection();

    [[nodiscard]] auto GetIp() const -> uint;
    [[nodiscard]] auto GetHost() const -> string_view;
    [[nodiscard]] auto GetPort() const -> uint16;
    [[nodiscard]] auto IsHardDisconnected() const -> bool;
    [[nodiscard]] auto IsGracefulDisconnected() const -> bool;
    [[nodiscard]] auto IsWebConnection() const -> bool;
    [[nodiscard]] auto IsInterthreadConnection() const -> bool;

    void DisableCompression();
    void Dispatch();
    void HardDisconnect();
    void GracefulDisconnect();

    NetInBuffer& InBuf;
    std::mutex& InBufLocker;
    NetOutBuffer& OutBuf;
    std::mutex& OutBufLocker;

    bool WasHandshake {};
    time_point PingNextTime {};
    bool PingOk {true};
    time_point LastActivityTime {};
    int UpdateFileIndex {-1};
    uint UpdateFilePortion {};

private:
    NetConnection* _netConnection;
    bool _gracefulDisconnected {};
    bool _nonConstHelper {};
};
