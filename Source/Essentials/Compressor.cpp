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

#include "Compressor.h"
#include "SafeArithmetics.h"
#include "StackTrace.h"

#include "zlib.h"

FO_BEGIN_NAMESPACE();

auto Compressor::CalculateMaxCompressedBufSize(size_t initial_size) noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return initial_size * 110 / 100 + 12;
}

auto Compressor::Compress(span<const uint8> data) -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    auto buf_len = numeric_cast<uLongf>(CalculateMaxCompressedBufSize(data.size()));
    auto buf = vector<uint8>(buf_len);

    const auto result = compress2(buf.data(), &buf_len, data.data(), numeric_cast<uLong>(data.size()), Z_BEST_SPEED);

    if (result != Z_OK) {
        throw CompressionException("Compression failed", result);
    }

    buf.resize(buf_len);
    return buf;
}

auto Compressor::Decompress(span<const uint8> data, size_t mul_approx) -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    auto buf_len = numeric_cast<uLongf>(data.size() * mul_approx);
    auto buf = vector<uint8>(buf_len);

    while (true) {
        const auto result = uncompress(buf.data(), &buf_len, data.data(), numeric_cast<uLong>(data.size()));

        if (result == Z_BUF_ERROR) {
            buf_len *= 2;
            buf.resize(buf_len);
        }
        else if (result != Z_OK) {
            throw DecompressException("Decompression failed", result);
        }
        else {
            break;
        }
    }

    buf.resize(buf_len);
    return buf;
}

struct StreamCompressor::Impl
{
    z_stream ZStream {};
};

StreamCompressor::StreamCompressor() noexcept = default;
StreamCompressor::StreamCompressor(StreamCompressor&&) noexcept = default;
auto StreamCompressor::operator=(StreamCompressor&&) noexcept -> StreamCompressor& = default;

StreamCompressor::~StreamCompressor()
{
    FO_STACK_TRACE_ENTRY();

    Reset();
}

void StreamCompressor::Compress(span<const uint8> buf, vector<uint8>& result)
{
    FO_STACK_TRACE_ENTRY();

    if (!_impl) {
        _impl = SafeAlloc::MakeUnique<Impl>();
        MemFill(&_impl->ZStream, 0, sizeof(z_stream));

        _impl->ZStream.zalloc = [](voidpf, uInt items, uInt size) -> void* {
            constexpr SafeAllocator<uint8> allocator;
            return allocator.allocate(numeric_cast<size_t>(items) * size);
        };
        _impl->ZStream.zfree = [](voidpf, voidpf address) {
            constexpr SafeAllocator<uint8> allocator;
            allocator.deallocate(static_cast<uint8*>(address), 0);
        };

        const auto deflate_init = deflateInit(&_impl->ZStream, Z_BEST_SPEED);
        FO_RUNTIME_ASSERT(deflate_init == Z_OK);
    }

    result.resize(std::max(result.capacity(), Compressor::CalculateMaxCompressedBufSize(buf.size())));

    _impl->ZStream.next_in = static_cast<Bytef*>(const_cast<uint8*>(buf.data()));
    _impl->ZStream.avail_in = numeric_cast<uInt>(buf.size());
    _impl->ZStream.next_out = static_cast<Bytef*>(result.data());
    _impl->ZStream.avail_out = numeric_cast<uInt>(result.size());

    const auto deflate_result = deflate(&_impl->ZStream, Z_SYNC_FLUSH);
    FO_RUNTIME_ASSERT(deflate_result == Z_OK);

    const auto writed_len = numeric_cast<size_t>(_impl->ZStream.next_in - buf.data());
    FO_RUNTIME_ASSERT(writed_len == buf.size());

    const auto compr_len = numeric_cast<size_t>(_impl->ZStream.next_out - result.data());
    result.resize(compr_len);
}

void StreamCompressor::Reset() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_impl) {
        deflateEnd(&_impl->ZStream);
        _impl.reset();
    }
}

struct StreamDecompressor::Impl
{
    z_stream ZStream {};
};

StreamDecompressor::StreamDecompressor() noexcept = default;
StreamDecompressor::StreamDecompressor(StreamDecompressor&&) noexcept = default;
auto StreamDecompressor::operator=(StreamDecompressor&&) noexcept -> StreamDecompressor& = default;

StreamDecompressor::~StreamDecompressor()
{
    FO_STACK_TRACE_ENTRY();

    Reset();
}

void StreamDecompressor::Decompress(span<const uint8> buf, vector<uint8>& result)
{
    FO_STACK_TRACE_ENTRY();

    if (!_impl) {
        _impl = SafeAlloc::MakeUnique<Impl>();
        MemFill(&_impl->ZStream, 0, sizeof(z_stream));

        _impl->ZStream.zalloc = [](voidpf, uInt items, uInt size) -> void* {
            constexpr SafeAllocator<uint8> allocator;
            return allocator.allocate(numeric_cast<size_t>(items) * size);
        };
        _impl->ZStream.zfree = [](voidpf, voidpf address) {
            constexpr SafeAllocator<uint8> allocator;
            allocator.deallocate(static_cast<uint8*>(address), 0);
        };

        const auto inflate_init = inflateInit(&_impl->ZStream);
        FO_RUNTIME_ASSERT(inflate_init == Z_OK);
    }

    result.resize(std::max(result.capacity(), buf.size() * 2));

    _impl->ZStream.next_in = static_cast<Bytef*>(const_cast<uint8*>(buf.data()));
    _impl->ZStream.avail_in = numeric_cast<uInt>(buf.size());
    _impl->ZStream.next_out = static_cast<Bytef*>(result.data());
    _impl->ZStream.avail_out = numeric_cast<uInt>(result.size());

    const auto first_inflate = ::inflate(&_impl->ZStream, Z_SYNC_FLUSH);
    FO_RUNTIME_ASSERT(first_inflate == Z_OK);

    auto uncompr_len = reinterpret_cast<size_t>(_impl->ZStream.next_out) - reinterpret_cast<size_t>(result.data());

    while (_impl->ZStream.avail_in != 0) {
        result.resize(result.size() * 2);

        _impl->ZStream.next_out = static_cast<Bytef*>(result.data() + uncompr_len);
        _impl->ZStream.avail_out = numeric_cast<uInt>(result.size() - uncompr_len);

        const auto next_inflate = ::inflate(&_impl->ZStream, Z_SYNC_FLUSH);
        FO_RUNTIME_ASSERT(next_inflate == Z_OK);

        uncompr_len = reinterpret_cast<size_t>(_impl->ZStream.next_out) - reinterpret_cast<size_t>(result.data());
    }

    result.resize(uncompr_len);
}

void StreamDecompressor::Reset() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_impl) {
        inflateEnd(&_impl->ZStream);
        _impl.reset();
    }
}

FO_END_NAMESPACE();
