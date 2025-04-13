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

#include "ServerConnection.h"
#include "Log.h"
#include "TextPack.h"

ServerConnection::ServerConnection(shared_ptr<NetworkServerConnection> net_connection) :
    _netConnection {std::move(net_connection)}
{
    STACK_TRACE_ENTRY();
}

ServerConnection::~ServerConnection()
{
    STACK_TRACE_ENTRY();

    _netConnection->Disconnect();
}

auto ServerConnection::GetIp() const noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->GetIp();
}

auto ServerConnection::GetHost() const noexcept -> string_view
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->GetHost();
}

auto ServerConnection::GetPort() const noexcept -> uint16
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->GetPort();
}

auto ServerConnection::IsHardDisconnected() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->IsDisconnected();
}

auto ServerConnection::IsGracefulDisconnected() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _gracefulDisconnected;
}

auto ServerConnection::IsWebConnection() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->IsWebConnection();
}

auto ServerConnection::IsInterthreadConnection() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->IsInterthreadConnection();
}

void ServerConnection::DisableCompression()
{
    STACK_TRACE_ENTRY();

    _netConnection->DispatchOutBuf();
}

void ServerConnection::DispatchOutBuf()
{
    STACK_TRACE_ENTRY();

    _netConnection->DispatchOutBuf();
}

void ServerConnection::HardDisconnect()
{
    STACK_TRACE_ENTRY();

    _netConnection->Disconnect();
}

void ServerConnection::GracefulDisconnect()
{
    STACK_TRACE_ENTRY();

    _gracefulDisconnected = true;

    auto out_buf = WriteBuf();
    out_buf->StartMsg(NETMSG_DISCONNECT);
    out_buf->EndMsg();
}
