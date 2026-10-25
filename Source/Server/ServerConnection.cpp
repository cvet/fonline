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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
#include "ScriptSystem.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE

FO_TEMPORARY_COMPAT(PreChannelClient, "2026-12-31");

// A client from before the secure channel opens its stream with the plaintext message signature 0x011E9422, little-endian
static constexpr std::array<uint8_t, 4> PRE_CHANNEL_SIGNATURE {0x22, 0x94, 0x1E, 0x01};

// No channel offer comes near 0x2200 bytes, so a current client's stream never even begins like the signature
static_assert(1 + SecureChannel::MAX_OFFERED_KEYS * NoiseHandshakeNK::MESSAGE_OVERHEAD < 0x2200);

// The handshake answer in the frozen layout such a client reads, telling it that its updater is outdated
static constexpr std::array<uint8_t, 20> PRE_CHANNEL_REFUSAL {
    0x22, 0x94, 0x1E, 0x01, // Signature
    0x14, 0x00, 0x00, 0x00, // Message length
    0x03, // NetMessage::HandshakeAnswer
    0x01, 0x01, 0x00, // Compatibility, updater and metadata outdated
    0x00, 0x00, 0x00, 0x00, // Empty metadata version
    0x00, 0x00, 0x00, 0x00, // Zero encryption key
};

class ServerConnection::RuntimeState final
{
    struct MethodCounters
    {
        uint64_t Started {};
        uint64_t Finished {};
        size_t Pending {};
    };

public:
    RuntimeState(RpcRuntimeLimits limits, shared_ptr<NetworkServerConnection> connection) :
        _tracker {limits},
        _connection {connection},
        _host {connection->GetHost()},
        _port {connection->GetPort()}
    {
    }

    auto GetDisconnectReason() const -> DisconnectReason { return _reason.load(std::memory_order_relaxed); }

    auto IsActive() const -> bool
    {
        scoped_lock locker {_locker};

        return !_stopped && _tracker.IsEnabled();
    }

    auto GetDiagnostics() const -> Diagnostics
    {
        scoped_lock locker {_locker};

        Diagnostics result;
        result.PendingRemoteCalls = _tracker.GetPendingCount();
        result.BufferedInputBytes = _tracker.GetBufferedInputSize();
        result.InputArrivalSegments = _tracker.GetArrivalSegmentCount();
        result.RuntimeWarnings = _warnings;
        result.RuntimeDisconnects = _disconnects;
        result.AbandonedRemoteCalls = _abandoned;
        return result;
    }

    auto Begin(string_view name, ident_t caller_id, nanotime now) -> RpcRuntimeStart
    {
        scoped_lock locker {_locker};

        if (_stopped) {
            return {false, {}};
        }

        RpcRuntimeStart started = _tracker.Begin(name, caller_id, now);

        if (started.Accepted && started.Id) {
            MethodCounters& counters = _methods[string {name}];
            counters.Started++;
            counters.Pending++;
            _lastDispatch = string {name};
        }

        return started;
    }

    void Finish(RpcRuntimeCallId id, string_view name)
    {
        scoped_lock locker {_locker};

        if (_stopped) {
            return;
        }

        auto counters = _methods.find(string {name});
        FO_STRONG_ASSERT(counters != _methods.end() && counters->second.Pending != 0, "RPC method completion lost its pending call", name, id);
        _tracker.Finish(id);
        counters->second.Finished++;
        counters->second.Pending--;
    }

    void UpdateInput(size_t consumed, size_t added, optional<NetInBuffer::QueuedMessage> queued, size_t unread, nanotime now)
    {
        scoped_lock locker {_locker};

        if (_stopped || !_tracker.IsEnabled()) {
            return;
        }

        _tracker.AddInput(added, now);
        _tracker.ConsumeInput(consumed);

        // Once a frame is ready, later history coalescing must not postpone its deadline
        if (_queuedPrefix && consumed < *_queuedPrefix) {
            *_queuedPrefix -= consumed;
        }
        else {
            _queuedPrefix.reset();
            _queuedReadyAt.reset();
        }

        if (queued) {
            if (!_queuedPrefix || *_queuedPrefix != queued->PrefixBytes) {
                _queuedReadyAt = _tracker.GetInputReadyTime(queued->PrefixBytes);
            }

            _queuedPrefix = queued->PrefixBytes;
        }
        else {
            _queuedPrefix.reset();
            _queuedReadyAt.reset();
        }

        _tracker.SetQueuedMessage(_queuedReadyAt, unread, queued ? queued->RemoteCallHash : 0);
    }

    auto RecordDisconnectReason(DisconnectReason reason) -> bool
    {
        DisconnectReason expected = DisconnectReason::None;
        return _reason.compare_exchange_strong(expected, reason, std::memory_order_relaxed);
    }

    void Check(nanotime now)
    {
        auto connection = _connection.lock();

        if (!connection || connection->IsDisconnected()) {
            return;
        }

        RpcRuntimeReport report;
        string last_dispatch;
        string counters_call;
        MethodCounters counters;
        size_t arrival_segments {};

        {
            scoped_lock locker {_locker};

            if (_stopped) {
                return;
            }

            report = _tracker.Evaluate(now);

            if (report.Action == RpcRuntimeAction::None) {
                return;
            }

            last_dispatch = _lastDispatch;
            counters_call = report.CallName.empty() ? last_dispatch : report.CallName;
            arrival_segments = _tracker.GetArrivalSegmentCount();
            auto method = _methods.find(counters_call);

            if (method != _methods.end()) {
                counters = method->second;
            }

            if (report.Action == RpcRuntimeAction::Warning) {
                _warnings++;
            }
        }

        bool disconnect = report.Action == RpcRuntimeAction::Disconnect;

        if (disconnect && !RecordDisconnectReason(DisconnectReason::RpcRuntimeLimit)) {
            return;
        }

        if (disconnect) {
            {
                scoped_lock locker {_locker};

                _disconnects++;
            }

            auto stop = scope_exit([this]() noexcept { Stop(); });
            connection->RequestDisconnect();
        }

        string_view cause = report.Cause == RpcRuntimeCause::InputQueue ? "input-queue" : report.Cause == RpcRuntimeCause::PendingLimit ? "pending-limit" : "pending-call";
        logging::write(logging::type::warning, "RPC runtime {}: peer={}:{}, caller={}, call={}, id={}, cause={}, age_ms={}, pending={}, buffered_bytes={}, queued_call_hash={}, started={}, finished={}, last_dispatch={}, stats_call={}, method_pending={}, method_started={}, method_finished={}, arrival_segments={}", disconnect ? "disconnect" : "warning", _host, _port, report.CallerId, report.CallName, report.Id, cause, report.Age.milliseconds(), report.PendingCalls, report.BufferedBytes, report.QueuedCallHash, report.StartedCalls, report.FinishedCalls, last_dispatch, counters_call, counters.Pending, counters.Started, counters.Finished, arrival_segments);
    }

    void Stop()
    {
        scoped_lock locker {_locker};

        if (!_stopped) {
            _abandoned = _tracker.GetPendingCount();
            _tracker = RpcRuntimeTracker {{0, 0, 0, 0}};
            _methods.clear();
            _lastDispatch.clear();
            _queuedPrefix.reset();
            _queuedReadyAt.reset();
            _stopped = true;
        }
    }

private:
    mutable mutex _locker {};
    RpcRuntimeTracker _tracker FO_TSA_GUARDED_BY(_locker);
    weak_ptr<NetworkServerConnection> _connection;
    string _host;
    uint16_t _port;
    std::atomic<DisconnectReason> _reason {};
    uint64_t _warnings FO_TSA_GUARDED_BY(_locker) {};
    uint64_t _disconnects FO_TSA_GUARDED_BY(_locker) {};
    size_t _abandoned FO_TSA_GUARDED_BY(_locker) {};
    bool _stopped FO_TSA_GUARDED_BY(_locker) {};
    unordered_map<string, MethodCounters> _methods FO_TSA_GUARDED_BY(_locker) {};
    string _lastDispatch FO_TSA_GUARDED_BY(_locker) {};
    optional<size_t> _queuedPrefix FO_TSA_GUARDED_BY(_locker) {};
    optional<nanotime> _queuedReadyAt FO_TSA_GUARDED_BY(_locker) {};
};

auto GetDisconnectReasonName(DisconnectReason reason) noexcept -> string_view
{
    switch (reason) {
    case DisconnectReason::None:
        return "none";
    case DisconnectReason::ClientClosed:
        return "client closed";
    case DisconnectReason::InactivityTimeout:
        return "inactivity timeout";
    case DisconnectReason::PingTimeout:
        return "ping timeout";
    case DisconnectReason::LoginTimeout:
        return "login timeout";
    case DisconnectReason::ProtocolError:
        return "protocol error";
    case DisconnectReason::UpdaterError:
        return "updater error";
    case DisconnectReason::ServerShutdown:
        return "server shutdown";
    case DisconnectReason::ScriptRequest:
        return "script request";
    case DisconnectReason::LoginFailed:
        return "login failed";
    case DisconnectReason::ReplacedByReconnect:
        return "replaced by reconnect";
    case DisconnectReason::RpcRuntimeLimit:
        return "RPC runtime limit";
    }

    return "unknown";
}

ServerConnection::OutBufAccessor::OutBufAccessor(ptr<ServerConnection> owner, optional<NetMessage> msg) :
    _owner {owner},
    _outBuf {&_owner->_outBuf},
    _msg {msg}
{
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
    Lock();
}

ServerConnection::InBufAccessor::~InBufAccessor()
{
    Unlock();
}

void ServerConnection::InBufAccessor::Lock()
{
    FO_VERIFY_AND_THROW(!_inBuf, "In buf is already set");
    _owner->_inBufLocker.lock();
    _inBuf = &_owner->_inBuf;
}

void ServerConnection::InBufAccessor::Unlock() noexcept
{
    if (_inBuf) {
        _owner->UpdateInputDiagnostics();
        _owner->_inBufLocker.unlock();
        _inBuf = nullptr;
    }
}

ServerConnection::ServerConnection(ptr<ServerNetworkSettings> settings, shared_ptr<NetworkServerConnection> net_connection, const SecureChannelIdentity& channel_identity) :
    _settings {settings},
    _netConnection {std::move(net_connection)},
    _inBuf(_settings->Network.NetBufferSize),
    _outBuf(_settings->Network.NetBufferSize),
    _channel {channel_identity},
    _uplinkDelay {std::chrono::milliseconds {_settings->Network.LinkDelayWindowMs}, std::chrono::milliseconds {_settings->Network.MoveLateCatchUpMinMs}, std::chrono::milliseconds {_settings->Network.LinkDelayRebaseMs}},
    _runtimeState {safe_alloc::make_shared<RuntimeState>(RpcRuntimeLimits {_settings->ServerNetwork.RemoteCallWarningTimeMs, _settings->ServerNetwork.RemoteCallDisconnectTimeMs, _settings->ServerNetwork.RemoteCallWarningIntervalMs, numeric_cast<size_t>(_settings->ServerNetwork.MaxPendingRemoteCalls)}, _netConnection)}
{
    auto send = [this]() FO_DEFERRED -> vector<uint8_t> { return AsyncSendData(); };
    auto receive = [this](const_span<uint8_t> buf) FO_DEFERRED { AsyncReceiveData(buf); };
    auto disconnect = [this]() FO_DEFERRED {
        RecordDisconnectReason(DisconnectReason::ClientClosed);
        _runtimeState->Stop();
        logging::write("Closed connection from {}:{} ({})", _netConnection->GetHost(), _netConnection->GetPort(), GetDisconnectReasonName(GetDisconnectReason()));
        AsyncReceiveData({});
    };

    if (_settings->ServerNetwork.MaxMessageSize != 0) {
        _inBuf.SetMaxMsgLen(numeric_cast<size_t>(_settings->ServerNetwork.MaxMessageSize));
    }
    if (_settings->ServerNetwork.MaxBufferedInputSize != 0) {
        _inBuf.SetMaxBufLen(numeric_cast<size_t>(_settings->ServerNetwork.MaxBufferedInputSize));
    }

    logging::write("New connection from {}:{}", _netConnection->GetHost(), _netConnection->GetPort());

    _netConnection->SetAsyncCallbacks(send, receive, disconnect);
}

ServerConnection::~ServerConnection()
{
    _runtimeState->Stop();

    safe_call([this] {
        scoped_lock locker {_inBufLocker};

        _dataArrivedCallback = {};
    });

    _netConnection->Disconnect();

    // A transport thread that won Disconnect() first may not have reported yet, and its report must not reach a dead owner
    _netConnection->DropAsyncCallbacks();
}

auto ServerConnection::GetHost() const noexcept -> string_view
{
    return _netConnection->GetHost();
}

auto ServerConnection::GetPort() const noexcept -> uint16_t
{
    return _netConnection->GetPort();
}

auto ServerConnection::IsHardDisconnected() const noexcept -> bool
{
    return _netConnection->IsDisconnected();
}

auto ServerConnection::IsGracefulDisconnected() const noexcept -> bool
{
    return _gracefulDisconnected;
}

auto ServerConnection::IsInputRejected() const noexcept -> bool
{
    return _inputRejected.load(std::memory_order_relaxed);
}

auto ServerConnection::GetDisconnectReason() const noexcept -> DisconnectReason
{
    return GetRuntimeDisconnectReason(_runtimeState);
}

void ServerConnection::RecordDisconnectReason(DisconnectReason reason) noexcept
{
    (void)_runtimeState->RecordDisconnectReason(reason);
}

auto ServerConnection::GetDiagnostics() const -> Diagnostics
{
    Diagnostics result = GetRuntimeDiagnostics(_runtimeState);
    result.HandshakeComplete = _activity.HandshakeComplete;
    result.LastActivityTime = _activity.LastActivityTime;
    result.LastLoginProgressTime = _activity.LastLoginProgressTime;
    result.PingAnswerReceived = _activity.PingAnswerReceived;
    result.RoundTrip = _activity.RoundTrip;

    if (_updateFileTransfer.PendingFileIndex) {
        result.PendingUpdateFileIndex = numeric_cast<int32_t>(*_updateFileTransfer.PendingFileIndex);
        result.PendingUpdateFilePortion = numeric_cast<int32_t>(_updateFileTransfer.PortionIndex);
    }

    return result;
}

auto ServerConnection::GetRuntimeState() const -> shared_ptr<RuntimeState>
{
    return _runtimeState;
}

auto ServerConnection::GetRuntimeDisconnectReason(ptr<const RuntimeState> state) -> DisconnectReason
{
    return state->GetDisconnectReason();
}

auto ServerConnection::GetRuntimeDiagnostics(ptr<const RuntimeState> state) -> Diagnostics
{
    return state->GetDiagnostics();
}

auto ServerConnection::IsRuntimeStateActive(ptr<const RuntimeState> state) -> bool
{
    return state->IsActive();
}

auto ServerConnection::BeginRemoteCall(hstring name, ident_t caller_id) -> InboundCall
{
    RpcRuntimeStart started = _runtimeState->Begin(name.as_str(), caller_id, nanotime::now());

    if (!started.Accepted) {
        _runtimeState->Check(nanotime::now());
        return {false, {}};
    }
    if (!started.Id) {
        return {};
    }

    auto completion = safe_alloc::make_shared<ScriptCallCompletion>([state = _runtimeState, id = started.Id, call_name = string {name.as_str()}]() mutable noexcept { state->Finish(id, call_name); });
    return {true, std::move(completion)};
}

void ServerConnection::CheckRuntimeState(ptr<RuntimeState> state, nanotime now)
{
    state->Check(now);
}

void ServerConnection::UpdateInputDiagnostics(size_t added_bytes)
{
    size_t unread = _inBuf.GetBufferedUnreadSize();
    FO_STRONG_ASSERT(_accountedInputBytes <= std::numeric_limits<size_t>::max() - added_bytes, "Input accounting overflow");
    size_t available = _accountedInputBytes + added_bytes;
    FO_STRONG_ASSERT(unread <= available, "Input accounting lost received bytes", unread, available);
    _runtimeState->UpdateInput(available - unread, added_bytes, _inBuf.PeekQueuedMessage(), unread, nanotime::now());
    _accountedInputBytes = unread;
}

auto ServerConnection::IsHandshakeComplete() const noexcept -> bool
{
    return _activity.HandshakeComplete;
}

auto ServerConnection::IsInactive(nanotime time) const noexcept -> bool
{
    return _settings->ServerNetwork.InactivityDisconnectTime != 0 && time - _activity.LastActivityTime >= std::chrono::milliseconds {_settings->ServerNetwork.InactivityDisconnectTime};
}

auto ServerConnection::IsLoginTimedOut(nanotime time) const noexcept -> bool
{
    return _settings->ServerNetwork.LoginTimeout != 0 && time - _activity.LastLoginProgressTime >= std::chrono::milliseconds {_settings->ServerNetwork.LoginTimeout};
}

auto ServerConnection::NeedPing(nanotime time) const noexcept -> bool
{
    return _activity.HandshakeComplete && (!_activity.NextPingTime || time >= _activity.NextPingTime);
}

auto ServerConnection::NeedsPingWatchdog() const noexcept -> bool
{
    return _netConnection->NeedsPingWatchdog();
}

auto ServerConnection::HasPendingPing() const noexcept -> bool
{
    return !_activity.PingAnswerReceived;
}

auto ServerConnection::GetRoundTrip() const noexcept -> timespan
{
    return _activity.RoundTrip;
}

auto ServerConnection::GetUpdateFileTransferIndex() const noexcept -> optional<size_t>
{
    return _updateFileTransfer.PendingFileIndex;
}

void ServerConnection::MarkHandshakeComplete() noexcept
{
    _activity.HandshakeComplete = true;
}

void ServerConnection::EnsureActivityTime(nanotime time) noexcept
{
    if (!_activity.LastActivityTime) {
        _activity.LastActivityTime = time;
    }
    if (!_activity.LastLoginProgressTime) {
        _activity.LastLoginProgressTime = time;
    }
}

void ServerConnection::RegisterActivity(nanotime time) noexcept
{
    _activity.LastActivityTime = time;
}

void ServerConnection::RegisterLoginProgress(nanotime time) noexcept
{
    _activity.LastLoginProgressTime = time;
}

void ServerConnection::RegisterPingRequest(nanotime time) noexcept
{
    _activity.NextPingTime = time + std::chrono::milliseconds {_settings->ServerNetwork.ClientPingTime};
    _activity.PingAnswerReceived = false;
    _activity.PingRequestTime = time;
}

auto ServerConnection::RegisterSenderTime(int64_t sender_ms, nanotime receive_time) -> timespan
{
    return _uplinkDelay.AddSample(sender_ms, receive_time);
}

void ServerConnection::RegisterPingAnswer(nanotime time) noexcept
{
    _activity.NextPingTime = time + std::chrono::milliseconds {_settings->ServerNetwork.ClientPingTime};
    _activity.PingAnswerReceived = true;

    // A sample carries the client's frame time on top of the transport delay, so it is smoothed into an upper
    // bound one late answer cannot move far; an unpaired answer is ignored outright
    if (_activity.PingRequestTime && time > _activity.PingRequestTime) {
        timespan sample = time - _activity.PingRequestTime;

        if (_activity.RoundTrip) {
            _activity.RoundTrip = timespan {(_activity.RoundTrip.nanoseconds() * 3 + sample.nanoseconds()) / 4};
        }
        else {
            _activity.RoundTrip = sample;
        }
    }

    _activity.PingRequestTime = {};
}

void ServerConnection::BeginUpdateFileTransfer(size_t file_index) noexcept
{
    _updateFileTransfer.PendingFileIndex = file_index;
    _updateFileTransfer.PortionIndex = 0;
}

auto ServerConnection::PullUpdateFilePortion(size_t file_size, size_t max_portion_size) -> UpdateFilePortion
{
    FO_VERIFY_AND_THROW(_updateFileTransfer.PendingFileIndex, "Updater file portion requested without an active pending file", _updateFileTransfer.PortionIndex, file_size, max_portion_size);
    FO_VERIFY_AND_THROW(max_portion_size != 0, "Max portion size must be non-zero", max_portion_size);

    size_t offset = _updateFileTransfer.PortionIndex * max_portion_size;
    FO_VERIFY_AND_THROW(offset <= file_size, "Updater file transfer portion offset is outside the file bounds", _updateFileTransfer.PendingFileIndex.value(), _updateFileTransfer.PortionIndex, max_portion_size, offset, file_size);

    size_t remaining_size = file_size - offset;
    auto portion_size = std::min(remaining_size, max_portion_size);

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
    _netConnection->Dispatch();
}

auto ServerConnection::AsyncSendData() -> vector<uint8_t>
{
    FO_TRACE_ZONE(Network);

    scoped_lock locker {_outBufLocker};
    scoped_lock channel_locker {_channelLocker};

    vector<uint8_t> send_buf;

    // The one plaintext a connection ever sends, and all that a client from before the channel receives
    if (_preChannelRefusalPending) {
        if (!_settings->Network.DisableZlibCompression) {
            _compressor.compress(PRE_CHANNEL_REFUSAL, send_buf);
        }
        else {
            send_buf.assign(PRE_CHANNEL_REFUSAL.begin(), PRE_CHANNEL_REFUSAL.end());
        }

        _preChannelRefusalPending = false;
        return send_buf;
    }

    _channel.TakeHandshakeOutput(send_buf);

    // Messages written before the client's channel stands wait in the buffer rather than leave in the clear
    if (_channel.IsEstablished() && !_outBuf.IsEmpty()) {
        auto raw_buf = _outBuf.GetData();

        if (!_settings->Network.DisableZlibCompression) {
            _compressor.compress(raw_buf, _compressedBuf);
            _channel.Seal(_compressedBuf, send_buf);
        }
        else {
            _channel.Seal(raw_buf, send_buf);
        }

        _outBuf.DiscardWriteBuf(raw_buf.size());
    }

    return send_buf;
}

void ServerConnection::AsyncReceiveData(const_span<uint8_t> buf)
{
    DataArrivedCallback callback;
    bool has_handshake_output = false;

    {
        scoped_lock locker {_inBufLocker};

        // A client from before the channel writes its handshake at once, so its first read opens with the plaintext signature
        if (!_inputStarted && buf.size() >= PRE_CHANNEL_SIGNATURE.size() && memory::compare(buf.data(), PRE_CHANNEL_SIGNATURE.data(), PRE_CHANNEL_SIGNATURE.size())) {
            logging::write("Client {}:{} predates the secure channel and is told to install the latest client", _netConnection->GetHost(), _netConnection->GetPort());

            // Not rejected: a disconnect would discard the answer unsent, and the client closes the connection on reading it
            _preChannelClient = true;
            _preChannelRefusalPending = true;
            has_handshake_output = true;
        }

        _inputStarted = true;

        // Runs on the network thread, inside the same transport receive lock that Disconnect() takes,
        // so a rejection is only latched here and the owning worker job performs the disconnect
        if (!buf.empty() && !IsInputRejected() && !_preChannelClient) {
            try {
                {
                    scoped_lock channel_locker {_channelLocker};

                    _channelPlaintext.clear();
                    _channel.Receive(buf, _channelPlaintext);
                    has_handshake_output = _channel.HasHandshakeOutput();
                }

                _inBuf.AddData(_channelPlaintext);
                UpdateInputDiagnostics(_channelPlaintext.size());
            }
            catch (const NetBufferException& ex) {
                UpdateInputDiagnostics();
                RejectInput(ex, true);
            }
            catch (const NoiseException& ex) {
                RejectInput(ex, false);
            }
        }

        callback = _dataArrivedCallback;
    }

    // The answer to the client's offer, or the refusal of a client from before the channel, leaves at once
    if (has_handshake_output) {
        StartAsyncSend();
    }

    if (callback) {
        callback();
    }
}

void ServerConnection::RejectInput(const std::exception& ex, bool report) noexcept
{
    if (_inputRejected.exchange(true, std::memory_order_relaxed)) {
        return;
    }

    // A failed channel is what a stranger or a stale client produces, so it is logged rather than reported
    if (report) {
        exceptions::report_and_continue(ex);
    }
    else {
        safe_call([&] { logging::write(logging::type::warning, "Secure channel rejected input from {}:{}: {}", _netConnection->GetHost(), _netConnection->GetPort(), ex.what()); });
    }
}

void ServerConnection::HardDisconnect(DisconnectReason reason)
{
    RecordDisconnectReason(reason);
    SetDataArrivedCallback({});
    _netConnection->Disconnect();
}

void ServerConnection::GracefulDisconnect()
{
    _gracefulDisconnected = true;

    WriteMsg(NetMessage::Disconnect);
}

void ServerConnection::SetDataArrivedCallback(DataArrivedCallback callback)
{
    // Same lock as AsyncReceiveData: that runs on the network thread and may read this callback
    // concurrently, so the assignment must be synchronized to avoid a torn std::function move
    scoped_lock locker {_inBufLocker};

    _dataArrivedCallback = std::move(callback);
}

FO_END_NAMESPACE

