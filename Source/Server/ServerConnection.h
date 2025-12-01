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

#pragma once

#include "Common.h"

#include "NetBuffer.h"
#include "NetworkServer.h"

FO_BEGIN_NAMESPACE();

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
        FO_FORCE_INLINE auto operator->() noexcept -> NetOutBuffer* { return _outBuf.get(); }
        FO_FORCE_INLINE auto operator*() noexcept -> NetOutBuffer& { return *_outBuf; }

    private:
        raw_ptr<ServerConnection> _owner;
        raw_ptr<NetOutBuffer> _outBuf;
        optional<NetMessage> _msg;
        StackUnwindDetector _isStackUnwinding {};
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
        FO_FORCE_INLINE auto operator->() noexcept -> NetInBuffer* { return _inBuf.get(); }
        FO_FORCE_INLINE auto operator*() noexcept -> NetInBuffer& { return *_inBuf; }
        void Lock();
        void Unlock() noexcept;

    private:
        raw_ptr<ServerConnection> _owner;
        raw_ptr<NetInBuffer> _inBuf {};
    };

    ServerConnection() = delete;
    explicit ServerConnection(ServerNetworkSettings& settings, shared_ptr<NetworkServerConnection> net_connection);
    ServerConnection(const ServerConnection&) = delete;
    ServerConnection(ServerConnection&&) noexcept = delete;
    auto operator=(const ServerConnection&) = delete;
    auto operator=(ServerConnection&&) noexcept = delete;
    ~ServerConnection();

    [[nodiscard]] auto GetHost() const noexcept -> string_view;
    [[nodiscard]] auto GetPort() const noexcept -> uint16;
    [[nodiscard]] auto IsHardDisconnected() const noexcept -> bool;
    [[nodiscard]] auto IsGracefulDisconnected() const noexcept -> bool;

    auto WriteMsg(NetMessage msg) -> OutBufAccessor { return OutBufAccessor(this, msg); }
    auto WriteBuf() -> OutBufAccessor { return OutBufAccessor(this, std::nullopt); }
    auto ReadBuf() -> InBufAccessor { return InBufAccessor(this); }

    void HardDisconnect();
    void GracefulDisconnect();

    // Todo: incapsulate ServerConnection data
    bool WasHandshake {};
    nanotime PingNextTime {};
    bool PingOk {true};
    nanotime LastActivityTime {};
    int32 UpdateFileIndex {-1};
    int32 UpdateFilePortion {};

private:
    void StartAsyncSend();
    auto AsyncSendData() -> span<const uint8>;
    void AsyncReceiveData(span<const uint8> buf);

    raw_ptr<ServerNetworkSettings> _settings;
    shared_ptr<NetworkServerConnection> _netConnection;
    NetInBuffer _inBuf;
    std::mutex _inBufLocker {};
    NetOutBuffer _outBuf;
    std::mutex _outBufLocker {};
    vector<uint8> _sendBuf {};
    StreamCompressor _compressor {};
    bool _gracefulDisconnected {};
};

FO_END_NAMESPACE();
