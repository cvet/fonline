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
    class OutBufAccessor
    {
    public:
        explicit OutBufAccessor(ServerConnection* owner, optional<NetMessage> msg);
        OutBufAccessor() = delete;
        OutBufAccessor(const OutBufAccessor&) = delete;
        OutBufAccessor(OutBufAccessor&&) noexcept = default;
        auto operator=(const OutBufAccessor&) = delete;
        auto operator=(OutBufAccessor&&) noexcept = delete;
        ~OutBufAccessor() noexcept(false);
        auto operator->() noexcept -> NetOutBuffer* { return _outBuf.get(); }
        auto operator*() noexcept -> NetOutBuffer& { return *_outBuf; }

    private:
        raw_ptr<ServerConnection> _owner;
        raw_ptr<NetOutBuffer> _outBuf;
        optional<NetMessage> _msg;
        stack_unwind_detector _isStackUnwinding {};
    };

    class InBufAccessor
    {
    public:
        explicit InBufAccessor(ServerConnection* owner);
        InBufAccessor() = delete;
        InBufAccessor(const InBufAccessor&) = delete;
        InBufAccessor(InBufAccessor&&) noexcept = default;
        auto operator=(const InBufAccessor&) = delete;
        auto operator=(InBufAccessor&&) noexcept = delete;
        ~InBufAccessor();
        auto operator->() noexcept -> NetInBuffer* { return _inBuf.get(); }
        auto operator*() noexcept -> NetInBuffer& { return *_inBuf; }
        void Lock();
        void Unlock() noexcept;

    private:
        raw_ptr<ServerConnection> _owner;
        raw_ptr<NetInBuffer> _inBuf {};
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
    explicit ServerConnection(ServerNetworkSettings& settings, shared_ptr<NetworkServerConnection> net_connection);
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

    void MarkHandshakeComplete() noexcept;
    void EnsureActivityTime(nanotime time) noexcept;
    void RegisterActivity(nanotime time) noexcept;
    void RegisterPingRequest(nanotime time) noexcept;
    void RegisterPingAnswer(nanotime time) noexcept;
    void BeginUpdateFileTransfer(size_t file_index) noexcept;
    auto PullUpdateFilePortion(size_t file_size, size_t max_portion_size) -> UpdateFilePortion;

    auto WriteMsg(NetMessage msg) -> OutBufAccessor { return OutBufAccessor(this, msg); }
    auto WriteBuf() -> OutBufAccessor { return OutBufAccessor(this, std::nullopt); }
    auto ReadBuf() -> InBufAccessor { return InBufAccessor(this); }

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

    raw_ptr<ServerNetworkSettings> _settings;
    shared_ptr<NetworkServerConnection> _netConnection;
    NetInBuffer _inBuf;
    std::mutex _inBufLocker {};
    NetOutBuffer _outBuf;
    std::mutex _outBufLocker {};
    vector<uint8_t> _sendBuf {};
    StreamCompressor _compressor {};
    ActivityState _activity {};
    UpdateFileTransferState _updateFileTransfer {};
    bool _gracefulDisconnected {};
};

FO_END_NAMESPACE
