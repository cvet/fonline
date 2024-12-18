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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Log.h"
#include "Networking.h"
#include "TextPack.h"

ClientConnection::ClientConnection(NetConnection* net_connection) :
    InBuf {net_connection->InBuf},
    InBufLocker {net_connection->InBufLocker},
    OutBuf {net_connection->OutBuf},
    OutBufLocker {net_connection->OutBufLocker},
    _netConnection {net_connection}
{
    STACK_TRACE_ENTRY();

    _netConnection->AddRef();
}

ClientConnection::~ClientConnection()
{
    STACK_TRACE_ENTRY();

    _netConnection->Disconnect();
    _netConnection->Release();
}

auto ClientConnection::GetIp() const noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->GetIp();
}

auto ClientConnection::GetHost() const noexcept -> string_view
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->GetHost();
}

auto ClientConnection::GetPort() const noexcept -> uint16
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->GetPort();
}

auto ClientConnection::IsHardDisconnected() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->IsDisconnected();
}

auto ClientConnection::IsGracefulDisconnected() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _gracefulDisconnected;
}

auto ClientConnection::IsWebConnection() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->IsWebConnection();
}

auto ClientConnection::IsInterthreadConnection() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _netConnection->IsInterthreadConnection();
}

void ClientConnection::DisableCompression()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _netConnection->Dispatch();
}

void ClientConnection::Dispatch()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _netConnection->Dispatch();
}

void ClientConnection::HardDisconnect()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _netConnection->Disconnect();
}

void ClientConnection::GracefulDisconnect()
{
    STACK_TRACE_ENTRY();

    _gracefulDisconnected = true;

    CONNECTION_OUTPUT_BEGIN(this);
    OutBuf.StartMsg(NETMSG_DISCONNECT);
    OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(this);
}
