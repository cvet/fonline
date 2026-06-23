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

#include "Compressor.h"
#include "SafeArithmetics.h"
#include "StackTrace.h"

#include "zlib.h"

FO_BEGIN_NAMESPACE

static auto GetZlibInputBuffer(const_span<uint8_t> buf) noexcept -> nptr<Bytef>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (buf.empty()) {
        return nullptr;
    }

    ptr<const uint8_t> data = buf.data();
    return cast_from_void<Bytef*>(cast_to_void(data.get_no_const()));
}

static auto GetZlibOutputBuffer(vector<uint8_t>& buf, size_t offset = 0) noexcept -> nptr<Bytef>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(offset <= buf.size(), "Offset exceeds buffer size");

    if (offset == buf.size()) {
        return nullptr;
    }

    ptr<uint8_t> data = buf.data();
    ptr<uint8_t> output = data.get() + offset;
    return output.reinterpret_as<Bytef>();
}

static auto GetZlibOutputSize(nptr<const Bytef> begin, nptr<const Bytef> end) noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!!begin == !!end, "Begin and end pointers must both be set or both null");

    if (!begin || !end) {
        return 0;
    }

    return numeric_cast<size_t>(end.get() - begin.get());
}

static auto GetSafeAllocatorBytePtr(nptr<void> address) noexcept -> nptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!address) {
        return nullptr;
    }

    return cast_from_void<uint8_t*>(address.get());
}

auto Compressor::CalculateMaxCompressedBufSize(size_t initial_size) noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return initial_size * 110 / 100 + 12;
}

auto Compressor::Compress(const_span<uint8_t> data) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    auto buf_len = numeric_cast<uLongf>(CalculateMaxCompressedBufSize(data.size()));
    auto buf = vector<uint8_t>(buf_len);

    auto output_data = GetZlibOutputBuffer(buf);
    FO_VERIFY_AND_THROW(output_data, "Output buffer is null");
    auto input_data = GetZlibInputBuffer(data);
    ptr<uLongf> output_size = &buf_len;

    const auto result = compress2(output_data.get(), output_size.get(), input_data.get(), numeric_cast<uLong>(data.size()), Z_BEST_SPEED);

    if (result != Z_OK) {
        throw CompressionException("Compression failed", result);
    }

    buf.resize(buf_len);
    return buf;
}

auto Compressor::Decompress(const_span<uint8_t> data, size_t mul_approx) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    auto buf_len = numeric_cast<uLongf>(data.size() * mul_approx);
    auto buf = vector<uint8_t>(buf_len);
    auto input_data = GetZlibInputBuffer(data);

    while (true) {
        auto output_data = GetZlibOutputBuffer(buf);
        ptr<uLongf> output_size = &buf_len;

        const auto result = uncompress(output_data.get(), output_size.get(), input_data.get(), numeric_cast<uLong>(data.size()));

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

StreamCompressor::State::State(unique_ptr<Impl>&& instance) noexcept :
    Instance {std::move(instance)}
{
    FO_NO_STACK_TRACE_ENTRY();
}

StreamCompressor::State::State(State&&) noexcept = default;

auto StreamCompressor::State::operator=(State&&) noexcept -> State& = default;

StreamCompressor::State::~State() = default;

auto StreamCompressor::MakeImpl() -> unique_ptr<Impl>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<Impl>();
}

StreamCompressor::StreamCompressor() noexcept = default;

StreamCompressor::StreamCompressor(StreamCompressor&& other) noexcept :
    _impl {std::move(other._impl)}
{
    FO_NO_STACK_TRACE_ENTRY();

    other._impl.reset();
}

auto StreamCompressor::operator=(StreamCompressor&& other) noexcept -> StreamCompressor&
{
    FO_NO_STACK_TRACE_ENTRY();

    if (this != &other) {
        Reset();
        _impl = std::move(other._impl);
        other._impl.reset();
    }

    return *this;
}

StreamCompressor::~StreamCompressor()
{
    FO_STACK_TRACE_ENTRY();

    Reset();
}

auto StreamCompressor::GetImpl() noexcept -> ptr<Impl>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_impl, "Stream implementation is not initialized");
    return _impl->Instance.as_ptr();
}

auto StreamCompressor::GetImpl() const noexcept -> ptr<const Impl>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_impl, "Stream implementation is not initialized");
    return _impl->Instance.as_ptr();
}

void StreamCompressor::Compress(const_span<uint8_t> buf, vector<uint8_t>& result)
{
    FO_STACK_TRACE_ENTRY();

    if (!_impl) {
        _impl.emplace(MakeImpl());
        auto impl = GetImpl();
        MemFill(&impl->ZStream, 0, sizeof(z_stream));

        impl->ZStream.zalloc = [](voidpf, uInt items, uInt size) -> void* {
            constexpr SafeAllocator<uint8_t> allocator;
            return allocator.allocate(numeric_cast<size_t>(items) * size);
        };
        impl->ZStream.zfree = [](voidpf, voidpf address) {
            constexpr SafeAllocator<uint8_t> allocator;
            allocator.deallocate(GetSafeAllocatorBytePtr(address).get(), 0);
        };

        const auto deflate_init = deflateInit(&impl->ZStream, Z_BEST_SPEED);
        FO_VERIFY_AND_THROW(deflate_init == Z_OK, "Failed to initialize zlib deflate stream", deflate_init);
    }

    auto impl = GetImpl();
    result.resize(std::max(result.capacity(), Compressor::CalculateMaxCompressedBufSize(buf.size())));

    impl->ZStream.next_in = GetZlibInputBuffer(buf).get();
    impl->ZStream.avail_in = numeric_cast<uInt>(buf.size());
    auto output_begin = GetZlibOutputBuffer(result);
    impl->ZStream.next_out = output_begin.get();
    impl->ZStream.avail_out = numeric_cast<uInt>(result.size());

    const auto deflate_result = deflate(&impl->ZStream, Z_SYNC_FLUSH);
    FO_VERIFY_AND_THROW(deflate_result == Z_OK, "Zlib deflate did not finish with Z_OK", deflate_result, Z_OK, buf.size(), result.size());

    const auto writed_len = numeric_cast<size_t>(impl->ZStream.next_in - buf.data());
    FO_VERIFY_AND_THROW(writed_len == buf.size(), "Zlib deflate did not consume the full input buffer", writed_len, buf.size());

    const size_t compr_len = GetZlibOutputSize(output_begin, impl->ZStream.next_out);
    result.resize(compr_len);
}

void StreamCompressor::Reset() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_impl) {
        auto impl = GetImpl();
        deflateEnd(&impl->ZStream);
        _impl.reset();
    }
}

struct StreamDecompressor::Impl
{
    z_stream ZStream {};
};

StreamDecompressor::State::State(unique_ptr<Impl>&& instance) noexcept :
    Instance {std::move(instance)}
{
    FO_NO_STACK_TRACE_ENTRY();
}

StreamDecompressor::State::State(State&&) noexcept = default;

auto StreamDecompressor::State::operator=(State&&) noexcept -> State& = default;

StreamDecompressor::State::~State() = default;

auto StreamDecompressor::MakeImpl() -> unique_ptr<Impl>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<Impl>();
}

StreamDecompressor::StreamDecompressor() noexcept = default;

StreamDecompressor::StreamDecompressor(StreamDecompressor&& other) noexcept :
    _impl {std::move(other._impl)}
{
    FO_NO_STACK_TRACE_ENTRY();

    other._impl.reset();
}

auto StreamDecompressor::operator=(StreamDecompressor&& other) noexcept -> StreamDecompressor&
{
    FO_NO_STACK_TRACE_ENTRY();

    if (this != &other) {
        Reset();
        _impl = std::move(other._impl);
        other._impl.reset();
    }

    return *this;
}

StreamDecompressor::~StreamDecompressor()
{
    FO_STACK_TRACE_ENTRY();

    Reset();
}

auto StreamDecompressor::GetImpl() noexcept -> ptr<Impl>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_impl, "Stream implementation is not initialized");
    return _impl->Instance.as_ptr();
}

auto StreamDecompressor::GetImpl() const noexcept -> ptr<const Impl>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_impl, "Stream implementation is not initialized");
    return _impl->Instance.as_ptr();
}

void StreamDecompressor::Decompress(const_span<uint8_t> buf, vector<uint8_t>& result)
{
    FO_STACK_TRACE_ENTRY();

    if (!_impl) {
        _impl.emplace(MakeImpl());
        auto impl = GetImpl();
        MemFill(&impl->ZStream, 0, sizeof(z_stream));

        impl->ZStream.zalloc = [](voidpf, uInt items, uInt size) -> void* {
            constexpr SafeAllocator<uint8_t> allocator;
            return allocator.allocate(numeric_cast<size_t>(items) * size);
        };
        impl->ZStream.zfree = [](voidpf, voidpf address) {
            constexpr SafeAllocator<uint8_t> allocator;
            allocator.deallocate(GetSafeAllocatorBytePtr(address).get(), 0);
        };

        const auto inflate_init = inflateInit(&impl->ZStream);
        FO_VERIFY_AND_THROW(inflate_init == Z_OK, "Failed to initialize zlib inflate stream", inflate_init);
    }

    auto impl = GetImpl();
    result.resize(std::max(result.capacity(), buf.size() * 2));

    impl->ZStream.next_in = GetZlibInputBuffer(buf).get();
    impl->ZStream.avail_in = numeric_cast<uInt>(buf.size());
    auto output_begin = GetZlibOutputBuffer(result);
    impl->ZStream.next_out = output_begin.get();
    impl->ZStream.avail_out = numeric_cast<uInt>(result.size());

    const auto first_inflate = ::inflate(&impl->ZStream, Z_SYNC_FLUSH);
    FO_VERIFY_AND_THROW(first_inflate == Z_OK, "Initial zlib inflate step did not finish with Z_OK", first_inflate, Z_OK, buf.size(), result.size());

    size_t uncompr_len = GetZlibOutputSize(output_begin, impl->ZStream.next_out);

    while (impl->ZStream.avail_in != 0) {
        result.resize(result.size() * 2);

        output_begin = GetZlibOutputBuffer(result);
        impl->ZStream.next_out = GetZlibOutputBuffer(result, uncompr_len).get();
        impl->ZStream.avail_out = numeric_cast<uInt>(result.size() - uncompr_len);

        const auto next_inflate = ::inflate(&impl->ZStream, Z_SYNC_FLUSH);
        FO_VERIFY_AND_THROW(next_inflate == Z_OK, "Continuation zlib inflate step did not finish with Z_OK", next_inflate, Z_OK, impl->ZStream.avail_in, result.size());

        uncompr_len = GetZlibOutputSize(output_begin, impl->ZStream.next_out);
    }

    result.resize(uncompr_len);
}

void StreamDecompressor::Reset() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_impl) {
        auto impl = GetImpl();
        inflateEnd(&impl->ZStream);
        _impl.reset();
    }
}

FO_END_NAMESPACE
