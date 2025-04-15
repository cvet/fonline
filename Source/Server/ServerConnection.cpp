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

ServerConnection::OutBufAccessor::OutBufAccessor(ServerConnection* owner, optional<NetMessage> msg) :
    _owner {owner},
    _outBuf {&_owner->_netConnection->GetOutBuf()},
    _msg {msg}
{
    STACK_TRACE_ENTRY();

    _owner->_netConnection->LockOutBuf();

    if (_msg) {
        _outBuf->StartMsg(_msg.value());
    }
}

ServerConnection::OutBufAccessor::~OutBufAccessor()
{
    STACK_TRACE_ENTRY();

    Unlock();
}

void ServerConnection::OutBufAccessor::Unlock() noexcept
{
    STACK_TRACE_ENTRY();

    if (_outBuf != nullptr) {
        if (!_isStackUnwinding) {
            if (_msg) {
                _outBuf->EndMsg();
            }

            _owner->_netConnection->UnlockOutBuf();
            _owner->_netConnection->DispatchOutBuf();
        }
        else {
            _owner->_netConnection->UnlockOutBuf();
        }

        _outBuf = nullptr;
    }
}

ServerConnection::InBufAccessor::InBufAccessor(ServerConnection* owner) :
    _owner {owner}
{
    STACK_TRACE_ENTRY();

    Lock();
}

ServerConnection::InBufAccessor::~InBufAccessor()
{
    STACK_TRACE_ENTRY();

    Unlock();
}

void ServerConnection::InBufAccessor::Lock()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_inBuf);
    _owner->_netConnection->LockInBuf();
    _inBuf = &_owner->_netConnection->GetInBuf();
}

void ServerConnection::InBufAccessor::Unlock() noexcept
{
    STACK_TRACE_ENTRY();

    if (_inBuf != nullptr) {
        _owner->_netConnection->UnlockInBuf();
        _inBuf = nullptr;
    }
}

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

    WriteMsg(NetMessage::DISCONNECT);
}
