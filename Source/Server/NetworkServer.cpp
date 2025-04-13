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

#include "zlib.h"

struct NetworkServerConnection::Impl
{
    z_stream ZStream {};
};

NetworkServerConnection::NetworkServerConnection(ServerNetworkSettings& settings) :
    _settings {settings},
    _inBuf(settings.NetBufferSize),
    _outBuf(settings.NetBufferSize)
{
    STACK_TRACE_ENTRY();

    _outFinalBuf.resize(_settings.NetBufferSize);

    if (!settings.DisableZlibCompression) {
        _impl = unique_del_ptr<Impl>(SafeAlloc::MakeRaw<Impl>(), [](Impl* impl) {
            deflateEnd(&impl->ZStream);
            delete impl;
        });

        MemFill(&_impl->ZStream, 0, sizeof(z_stream));

        _impl->ZStream.zalloc = [](voidpf, uInt items, uInt size) -> void* {
            constexpr SafeAllocator<uint8> allocator;
            return allocator.allocate(static_cast<size_t>(items) * size);
        };
        _impl->ZStream.zfree = [](voidpf, voidpf address) {
            constexpr SafeAllocator<uint8> allocator;
            allocator.deallocate(static_cast<uint8*>(address), 0);
        };

        const auto result = deflateInit(&_impl->ZStream, Z_BEST_SPEED);
        RUNTIME_ASSERT(result == Z_OK);
    }
}

void NetworkServerConnection::DisableOutBufCompression()
{
    STACK_TRACE_ENTRY();

    _impl.reset();
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
    if (_impl) {
        const auto to_compr = _outBuf.GetEndPos();

        _outFinalBuf.resize(to_compr + 32);

        if (_outFinalBuf.size() <= _settings.NetBufferSize && _outFinalBuf.capacity() > _settings.NetBufferSize) {
            _outFinalBuf.shrink_to_fit();
        }

        _impl->ZStream.next_in = static_cast<Bytef*>(_outBuf.GetData());
        _impl->ZStream.avail_in = static_cast<uInt>(to_compr);
        _impl->ZStream.next_out = static_cast<Bytef*>(_outFinalBuf.data());
        _impl->ZStream.avail_out = static_cast<uInt>(_outFinalBuf.size());

        const auto result = deflate(&_impl->ZStream, Z_SYNC_FLUSH);
        RUNTIME_ASSERT(result == Z_OK);

        const auto compr = static_cast<size_t>(_impl->ZStream.next_out - _outFinalBuf.data());
        const auto real = static_cast<size_t>(_impl->ZStream.next_in - _outBuf.GetData());
        out_len = compr;

        _outBuf.DiscardWriteBuf(real);
    }
    // Without compressing
    else {
        const auto len = _outBuf.GetEndPos();

        _outFinalBuf.resize(len);

        if (_outFinalBuf.size() <= _settings.NetBufferSize && _outFinalBuf.capacity() > _settings.NetBufferSize) {
            _outFinalBuf.shrink_to_fit();
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
