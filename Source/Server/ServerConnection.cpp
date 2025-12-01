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
#include "TextPack.h"

FO_BEGIN_NAMESPACE();

ServerConnection::OutBufAccessor::OutBufAccessor(ServerConnection* owner, optional<NetMessage> msg) :
    _owner {owner},
    _outBuf {&_owner->_outBuf},
    _msg {msg}
{
    FO_STACK_TRACE_ENTRY();

    _owner->_outBufLocker.lock();

    if (_msg) {
        _outBuf->StartMsg(_msg.value());
    }
}

ServerConnection::OutBufAccessor::~OutBufAccessor() noexcept(false)
{
    FO_STACK_TRACE_ENTRY();

    if (_outBuf != nullptr) {
        if (!_isStackUnwinding) {
            if (_msg) {
                try {
                    _outBuf->EndMsg();
                }
                catch (...) {
                    _owner->_outBufLocker.unlock();
                    throw;
                }
            }

            _owner->_outBufLocker.unlock();
            _owner->StartAsyncSend();
        }
        else {
            _owner->_outBufLocker.unlock();
        }

        _outBuf = nullptr;
    }
}

ServerConnection::InBufAccessor::InBufAccessor(ServerConnection* owner) :
    _owner {owner}
{
    FO_STACK_TRACE_ENTRY();

    Lock();
}

ServerConnection::InBufAccessor::~InBufAccessor()
{
    FO_STACK_TRACE_ENTRY();

    Unlock();
}

void ServerConnection::InBufAccessor::Lock()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_inBuf);
    _owner->_inBufLocker.lock();
    _inBuf = &_owner->_inBuf;
}

void ServerConnection::InBufAccessor::Unlock() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_inBuf != nullptr) {
        _owner->_inBufLocker.unlock();
        _inBuf = nullptr;
    }
}

ServerConnection::ServerConnection(ServerNetworkSettings& settings, shared_ptr<NetworkServerConnection> net_connection) :
    _settings {&settings},
    _netConnection {std::move(net_connection)},
    _inBuf(_settings->NetBufferSize),
    _outBuf(_settings->NetBufferSize, _settings->NetDebugHashes)
{
    FO_STACK_TRACE_ENTRY();

    auto send = [this]() -> span<const uint8> { return AsyncSendData(); };
    auto receive = [this](span<const uint8> buf) { AsyncReceiveData(buf); };
    auto disconnect = [this]() { WriteLog("Closed connection from {}:{}", _netConnection->GetHost(), _netConnection->GetPort()); };
    _netConnection->SetAsyncCallbacks(send, receive, disconnect);

    WriteLog("New connection from {}:{}", _netConnection->GetHost(), _netConnection->GetPort());
}

ServerConnection::~ServerConnection()
{
    FO_STACK_TRACE_ENTRY();

    _netConnection->Disconnect();
}

auto ServerConnection::GetHost() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    return _netConnection->GetHost();
}

auto ServerConnection::GetPort() const noexcept -> uint16
{
    FO_NO_STACK_TRACE_ENTRY();

    return _netConnection->GetPort();
}

auto ServerConnection::IsHardDisconnected() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _netConnection->IsDisconnected();
}

auto ServerConnection::IsGracefulDisconnected() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _gracefulDisconnected;
}

void ServerConnection::StartAsyncSend()
{
    FO_STACK_TRACE_ENTRY();

    _netConnection->Dispatch();
}

auto ServerConnection::AsyncSendData() -> span<const uint8>
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker(_outBufLocker);

    if (_outBuf.IsEmpty()) {
        return {};
    }

    const auto raw_buf = _outBuf.GetData();

    if (!_settings->DisableZlibCompression) {
        _compressor.Compress(raw_buf, _sendBuf);
    }
    else {
        _sendBuf.assign(raw_buf.begin(), raw_buf.end());
    }

    _outBuf.DiscardWriteBuf(raw_buf.size());

    FO_RUNTIME_ASSERT(!_sendBuf.empty());
    return _sendBuf;
}

void ServerConnection::AsyncReceiveData(span<const uint8> buf)
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker(_inBufLocker);

    _inBuf.AddData(buf);
}

void ServerConnection::HardDisconnect()
{
    FO_STACK_TRACE_ENTRY();

    _netConnection->Disconnect();
}

void ServerConnection::GracefulDisconnect()
{
    FO_STACK_TRACE_ENTRY();

    _gracefulDisconnected = true;

    WriteMsg(NetMessage::Disconnect);
}

FO_END_NAMESPACE();
