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

#include "NetworkServer.h"

NetworkServerConnection::NetworkServerConnection(ServerNetworkSettings& settings) :
    _settings {settings},
    _inBuf(settings.NetBufferSize),
    _outBuf(settings.NetBufferSize)
{
    STACK_TRACE_ENTRY();

    _outFinalBuf.resize(_settings.NetBufferSize);
}

void NetworkServerConnection::DispatchOutBuf()
{
    STACK_TRACE_ENTRY();

    if (_isDisconnected) {
        return;
    }

    // Nothing to send
    {
        std::scoped_lock locker(_outBufLocker);

        if (_outBuf.IsEmpty()) {
            return;
        }
    }

    DispatchImpl();
}

void NetworkServerConnection::Disconnect()
{
    STACK_TRACE_ENTRY();

    bool expected = false;

    if (_isDisconnected.compare_exchange_strong(expected, true)) {
        DisconnectImpl();
    }
}

auto NetworkServerConnection::SendCallback() -> const_span<uint8>
{
    STACK_TRACE_ENTRY();

    std::scoped_lock locker(_outBufLocker);

    if (_outBuf.IsEmpty()) {
        return {};
    }

    size_t out_len;

    // Compress
    if (!_settings.DisableZlibCompression) {
        const auto compr_buf = _compressor.Compress({_outBuf.GetData(), _outBuf.GetEndPos()}, _outFinalBuf);
        out_len = compr_buf.size();

        _outBuf.DiscardWriteBuf(_outBuf.GetEndPos());
    }
    // Without compressing
    else {
        const auto len = _outBuf.GetEndPos();

        while (_outFinalBuf.size() < len) {
            _outFinalBuf.resize(_outFinalBuf.size() * 2);
        }

        MemCopy(_outFinalBuf.data(), _outBuf.GetData(), len);
        out_len = len;

        _outBuf.DiscardWriteBuf(len);
    }

    // Normalize buffer size
    if (_outBuf.IsEmpty()) {
        _outBuf.ResetBuf();
    }

    RUNTIME_ASSERT(out_len > 0);
    return {_outFinalBuf.data(), out_len};
}

void NetworkServerConnection::ReceiveCallback(const uint8* buf, size_t len)
{
    STACK_TRACE_ENTRY();

    std::scoped_lock locker(_inBufLocker);

    if (_inBuf.GetReadPos() + len < _settings.FloodSize) {
        _inBuf.AddData(buf, len);
    }
    else {
        _inBuf.ResetBuf();
        Disconnect();
    }
}

class DummyNetConnection : public NetworkServerConnection
{
public:
    explicit DummyNetConnection(ServerNetworkSettings& settings) :
        NetworkServerConnection(settings)
    {
        _host = "Dummy";
        Disconnect();
    }
    DummyNetConnection(const DummyNetConnection&) = delete;
    DummyNetConnection(DummyNetConnection&&) noexcept = delete;
    auto operator=(const DummyNetConnection&) = delete;
    auto operator=(DummyNetConnection&&) noexcept = delete;
    ~DummyNetConnection() override = default;

protected:
    void DispatchImpl() override { }
    void DisconnectImpl() override { }
};

auto NetworkServer::CreateDummyConnection(ServerNetworkSettings& settings) -> shared_ptr<NetworkServerConnection>
{
    STACK_TRACE_ENTRY();

    return SafeAlloc::MakeShared<DummyNetConnection>(settings);
}
