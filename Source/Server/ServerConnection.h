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

#pragma once

#include "Common.h"

#include "NetBuffer.h"
#include "NetworkServer.h"

FO_BEGIN_NAMESPACE

class ServerConnection final
{
public:
    using DataArrivedCallback = function<void()>;

    class FO_TSA_SCOPED_CAPABILITY OutBufAccessor
    {
    public:
        explicit OutBufAccessor(ptr<ServerConnection> owner, optional<NetMessage> msg) FO_TSA_ACQUIRE(owner->_outBufLocker);
        OutBufAccessor() = delete;
        OutBufAccessor(const OutBufAccessor&) = delete;
        // Move ctor cannot express capability transfer for the analyzer; the moved-from guard is left inert by the move
        OutBufAccessor(OutBufAccessor&&) noexcept FO_TSA_NO_ANALYSIS = default;
        auto operator=(const OutBufAccessor&) = delete;
        auto operator=(OutBufAccessor&&) noexcept = delete;
        ~OutBufAccessor() noexcept(false) FO_TSA_RELEASE();
        auto operator->() noexcept -> ptr<NetOutBuffer>
        {
            FO_NO_STACK_TRACE_ENTRY();

            return _outBuf.as_ptr();
        }
        auto operator*() noexcept -> NetOutBuffer& { return *_outBuf; }

    private:
        ptr<ServerConnection> _owner;
        nptr<NetOutBuffer> _outBuf {};
        optional<NetMessage> _msg;
        stack_unwind_detector _isStackUnwinding {};
    };

    class FO_TSA_SCOPED_CAPABILITY InBufAccessor
    {
    public:
        explicit InBufAccessor(ptr<ServerConnection> owner) FO_TSA_ACQUIRE(owner->_inBufLocker);
        InBufAccessor() = delete;
        InBufAccessor(const InBufAccessor&) = delete;
        // Move ctor cannot express capability transfer for the analyzer; the moved-from guard is left inert by the move
        InBufAccessor(InBufAccessor&&) noexcept FO_TSA_NO_ANALYSIS = default;
        auto operator=(const InBufAccessor&) = delete;
        auto operator=(InBufAccessor&&) noexcept = delete;
        ~InBufAccessor() FO_TSA_RELEASE();
        auto operator->() noexcept -> ptr<NetInBuffer>
        {
            FO_NO_STACK_TRACE_ENTRY();

            return _inBuf.as_ptr();
        }
        auto operator*() noexcept -> NetInBuffer& { return *_inBuf; }
        void Lock() FO_TSA_ACQUIRE();
        void Unlock() noexcept FO_TSA_RELEASE();

    private:
        ptr<ServerConnection> _owner;
        nptr<NetInBuffer> _inBuf {};
    };

    struct Diagnostics
    {
        bool HandshakeComplete {};
        nanotime LastActivityTime {};
        bool PingAnswerReceived {true};
        int32_t PendingUpdateFileIndex {-1};
        int32_t PendingUpdateFilePortion {};
    };

    struct UpdateFilePortion
    {
        size_t Offset {};
        size_t Size {};
    };

    ServerConnection() = delete;
    explicit ServerConnection(ptr<ServerNetworkSettings> settings, shared_ptr<NetworkServerConnection> net_connection);
    ServerConnection(const ServerConnection&) = delete;
    ServerConnection(ServerConnection&&) noexcept = delete;
    auto operator=(const ServerConnection&) = delete;
    auto operator=(ServerConnection&&) noexcept = delete;
    ~ServerConnection();

    [[nodiscard]] auto GetHost() const noexcept -> string_view;
    [[nodiscard]] auto GetPort() const noexcept -> uint16_t;
    [[nodiscard]] auto IsHardDisconnected() const noexcept -> bool;
    [[nodiscard]] auto IsGracefulDisconnected() const noexcept -> bool;
    [[nodiscard]] auto GetDiagnostics() const -> Diagnostics;
    [[nodiscard]] auto IsHandshakeComplete() const noexcept -> bool;
    [[nodiscard]] auto IsInactive(nanotime time) const noexcept -> bool;
    [[nodiscard]] auto NeedPing(nanotime time) const noexcept -> bool;
    [[nodiscard]] auto HasPendingPing() const noexcept -> bool;
    [[nodiscard]] auto GetUpdateFileTransferIndex() const noexcept -> optional<size_t>;

    void SetDataArrivedCallback(DataArrivedCallback callback);
    void MarkHandshakeComplete() noexcept;
    void EnsureActivityTime(nanotime time) noexcept;
    void RegisterActivity(nanotime time) noexcept;
    void RegisterPingRequest(nanotime time) noexcept;
    void RegisterPingAnswer(nanotime time) noexcept;
    void BeginUpdateFileTransfer(size_t file_index) noexcept;
    auto PullUpdateFilePortion(size_t file_size, size_t max_portion_size) -> UpdateFilePortion;

    // These factories deliberately return a guard that still holds the buffer lock (released when the caller's
    // accessor leaves scope); TSA cannot express "returns holding a lock", so the trivial bodies are exempt.
    OutBufAccessor WriteMsg(NetMessage msg) FO_TSA_NO_ANALYSIS { return OutBufAccessor(ptr<ServerConnection>(this), msg); }
    OutBufAccessor WriteBuf() FO_TSA_NO_ANALYSIS { return OutBufAccessor(ptr<ServerConnection>(this), std::nullopt); }
    InBufAccessor ReadBuf() FO_TSA_NO_ANALYSIS { return InBufAccessor(ptr<ServerConnection>(this)); }

    void HardDisconnect();
    void GracefulDisconnect();

private:
    struct ActivityState
    {
        bool HandshakeComplete {};
        nanotime NextPingTime {};
        bool PingAnswerReceived {true};
        nanotime LastActivityTime {};
    };

    struct UpdateFileTransferState
    {
        optional<size_t> PendingFileIndex {};
        size_t PortionIndex {};
    };

    void StartAsyncSend();
    auto AsyncSendData() -> const_span<uint8_t>;
    void AsyncReceiveData(const_span<uint8_t> buf);

    ptr<ServerNetworkSettings> _settings;
    shared_ptr<NetworkServerConnection> _netConnection;
    mutex _inBufLocker {};
    NetInBuffer _inBuf;
    mutex _outBufLocker {};
    NetOutBuffer _outBuf;
    vector<uint8_t> _sendBuf {};
    StreamCompressor _compressor {};
    ActivityState _activity {};
    UpdateFileTransferState _updateFileTransfer {};
    DataArrivedCallback _dataArrivedCallback {};
    bool _gracefulDisconnected {};
};

FO_END_NAMESPACE
