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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE

ServerConnection::OutBufAccessor::OutBufAccessor(ptr<ServerConnection> owner, optional<NetMessage> msg) :
    _owner {owner},
    _outBuf {&_owner->_outBuf},
    _msg {msg}
{
    FO_STACK_TRACE_ENTRY();

    _owner->_outBufLocker.lock();

    if (_msg) {
        try {
            _outBuf->StartMsg(_msg.value());
        }
        catch (...) {
            _owner->_outBufLocker.unlock();
            throw;
        }
    }
}

ServerConnection::OutBufAccessor::~OutBufAccessor() noexcept(false)
{
    FO_STACK_TRACE_ENTRY();

    if (_outBuf) {
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

ServerConnection::InBufAccessor::InBufAccessor(ptr<ServerConnection> owner) :
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

    FO_VERIFY_AND_THROW(!_inBuf, "In buf is already set");
    _owner->_inBufLocker.lock();
    _inBuf = &_owner->_inBuf;
}

void ServerConnection::InBufAccessor::Unlock() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_inBuf) {
        _owner->_inBufLocker.unlock();
        _inBuf = nullptr;
    }
}

ServerConnection::ServerConnection(ptr<ServerNetworkSettings> settings, shared_ptr<NetworkServerConnection> net_connection) :
    _settings {settings},
    _netConnection {std::move(net_connection)},
    _inBuf(_settings->NetBufferSize),
    _outBuf(_settings->NetBufferSize)
{
    FO_STACK_TRACE_ENTRY();

    auto send = [this]() FO_DEFERRED -> const_span<uint8_t> { return AsyncSendData(); };
    auto receive = [this](const_span<uint8_t> buf) FO_DEFERRED { AsyncReceiveData(buf); };
    auto disconnect = [this]() FO_DEFERRED {
        WriteLog("Closed connection from {}:{}", _netConnection->GetHost(), _netConnection->GetPort());
        AsyncReceiveData({});
    };

    if (_settings->MaxMessageSize != 0) {
        _inBuf.SetMaxMsgLen(numeric_cast<size_t>(_settings->MaxMessageSize));
    }

    WriteLog("New connection from {}:{}", _netConnection->GetHost(), _netConnection->GetPort());

    _netConnection->SetAsyncCallbacks(send, receive, disconnect);
}

ServerConnection::~ServerConnection()
{
    FO_STACK_TRACE_ENTRY();

    safe_call([this] {
        scoped_lock locker {_inBufLocker};

        _dataArrivedCallback = {};
    });

    _netConnection->Disconnect();
}

auto ServerConnection::GetHost() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    return _netConnection->GetHost();
}

auto ServerConnection::GetPort() const noexcept -> uint16_t
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

auto ServerConnection::GetDiagnostics() const -> Diagnostics
{
    FO_STACK_TRACE_ENTRY();

    Diagnostics result;
    result.HandshakeComplete = _activity.HandshakeComplete;
    result.LastActivityTime = _activity.LastActivityTime;
    result.LastLoginProgressTime = _activity.LastLoginProgressTime;
    result.PingAnswerReceived = _activity.PingAnswerReceived;

    if (_updateFileTransfer.PendingFileIndex) {
        result.PendingUpdateFileIndex = numeric_cast<int32_t>(*_updateFileTransfer.PendingFileIndex);
        result.PendingUpdateFilePortion = numeric_cast<int32_t>(_updateFileTransfer.PortionIndex);
    }

    return result;
}

auto ServerConnection::IsHandshakeComplete() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _activity.HandshakeComplete;
}

auto ServerConnection::IsInactive(nanotime time) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _settings->InactivityDisconnectTime != 0 && time - _activity.LastActivityTime >= std::chrono::milliseconds {_settings->InactivityDisconnectTime};
}

auto ServerConnection::IsLoginTimedOut(nanotime time) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _settings->LoginTimeout != 0 && time - _activity.LastLoginProgressTime >= std::chrono::milliseconds {_settings->LoginTimeout};
}

auto ServerConnection::NeedPing(nanotime time) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _activity.HandshakeComplete && (!_activity.NextPingTime || time >= _activity.NextPingTime);
}

auto ServerConnection::HasPendingPing() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_activity.PingAnswerReceived;
}

auto ServerConnection::GetUpdateFileTransferIndex() const noexcept -> optional<size_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _updateFileTransfer.PendingFileIndex;
}

void ServerConnection::MarkHandshakeComplete() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _activity.HandshakeComplete = true;
}

void ServerConnection::EnsureActivityTime(nanotime time) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_activity.LastActivityTime) {
        _activity.LastActivityTime = time;
    }
    if (!_activity.LastLoginProgressTime) {
        _activity.LastLoginProgressTime = time;
    }
}

void ServerConnection::RegisterActivity(nanotime time) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _activity.LastActivityTime = time;
}

void ServerConnection::RegisterLoginProgress(nanotime time) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _activity.LastLoginProgressTime = time;
}

void ServerConnection::RegisterPingRequest(nanotime time) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _activity.NextPingTime = time + std::chrono::milliseconds {_settings->ClientPingTime};
    _activity.PingAnswerReceived = false;
}

void ServerConnection::RegisterPingAnswer(nanotime time) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _activity.NextPingTime = time + std::chrono::milliseconds {_settings->ClientPingTime};
    _activity.PingAnswerReceived = true;
}

void ServerConnection::BeginUpdateFileTransfer(size_t file_index) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _updateFileTransfer.PendingFileIndex = file_index;
    _updateFileTransfer.PortionIndex = 0;
}

auto ServerConnection::PullUpdateFilePortion(size_t file_size, size_t max_portion_size) -> UpdateFilePortion
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_updateFileTransfer.PendingFileIndex, "Updater file portion requested without an active pending file", _updateFileTransfer.PortionIndex, file_size, max_portion_size);
    FO_VERIFY_AND_THROW(max_portion_size != 0, "Max portion size must be non-zero", max_portion_size);

    const auto offset = _updateFileTransfer.PortionIndex * max_portion_size;
    FO_VERIFY_AND_THROW(offset <= file_size, "Updater file transfer portion offset is outside the file bounds", _updateFileTransfer.PendingFileIndex.value(), _updateFileTransfer.PortionIndex, max_portion_size, offset, file_size);

    const auto remaining_size = file_size - offset;
    const auto portion_size = std::min(remaining_size, max_portion_size);

    if (offset + portion_size < file_size) {
        _updateFileTransfer.PortionIndex++;
    }
    else {
        _updateFileTransfer.PendingFileIndex.reset();
        _updateFileTransfer.PortionIndex = 0;
    }

    return {offset, portion_size};
}

void ServerConnection::StartAsyncSend()
{
    FO_STACK_TRACE_ENTRY();

    _netConnection->Dispatch();
}

auto ServerConnection::AsyncSendData() -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_outBufLocker};

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

    FO_VERIFY_AND_THROW(!_sendBuf.empty(), "Server connection encoded an empty outgoing packet from a non-empty output buffer", raw_buf.size(), _settings->DisableZlibCompression);
    return _sendBuf;
}

void ServerConnection::AsyncReceiveData(const_span<uint8_t> buf)
{
    FO_STACK_TRACE_ENTRY();

    DataArrivedCallback callback;

    {
        scoped_lock locker {_inBufLocker};

        if (!buf.empty()) {
            _inBuf.AddData(buf);
        }

        callback = _dataArrivedCallback;
    }

    if (callback) {
        callback();
    }
}

void ServerConnection::HardDisconnect()
{
    FO_STACK_TRACE_ENTRY();

    SetDataArrivedCallback({});
    _netConnection->Disconnect();
}

void ServerConnection::GracefulDisconnect()
{
    FO_STACK_TRACE_ENTRY();

    _gracefulDisconnected = true;

    WriteMsg(NetMessage::Disconnect);
}

void ServerConnection::SetDataArrivedCallback(DataArrivedCallback callback)
{
    FO_STACK_TRACE_ENTRY();

    // Same lock as AsyncReceiveData: that runs on the network thread and may read this callback
    // concurrently, so the assignment must be synchronized to avoid a torn std::function move.
    scoped_lock locker {_inBufLocker};

    _dataArrivedCallback = std::move(callback);
}

FO_END_NAMESPACE
