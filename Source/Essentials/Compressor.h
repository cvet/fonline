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

#include "BasicCore.h"
#include "Containers.h"
#include "ExceptionHadling.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(CompressionException);
FO_DECLARE_EXCEPTION(DecompressException);

class Compressor final
{
public:
    Compressor() = delete;

    [[nodiscard]] static auto CalculateMaxCompressedBufSize(size_t initial_size) noexcept -> size_t;
    [[nodiscard]] static auto Compress(span<const uint8> data) -> vector<uint8>;
    [[nodiscard]] static auto Decompress(span<const uint8> data, size_t mul_approx) -> vector<uint8>;
};

class StreamCompressor final
{
public:
    StreamCompressor() noexcept;
    StreamCompressor(const StreamCompressor&) = delete;
    StreamCompressor(StreamCompressor&&) noexcept;
    auto operator=(const StreamCompressor&) = delete;
    auto operator=(StreamCompressor&&) noexcept -> StreamCompressor&;
    ~StreamCompressor();

    void Compress(span<const uint8> buf, vector<uint8>& result);
    void Reset() noexcept;

private:
    struct Impl;
    unique_ptr<Impl> _impl;
};

class StreamDecompressor final
{
public:
    StreamDecompressor() noexcept;
    StreamDecompressor(const StreamDecompressor&) = delete;
    StreamDecompressor(StreamDecompressor&&) noexcept;
    auto operator=(const StreamDecompressor&) = delete;
    auto operator=(StreamDecompressor&&) noexcept -> StreamDecompressor&;
    ~StreamDecompressor();

    void Decompress(span<const uint8> buf, vector<uint8>& result);
    void Reset() noexcept;

private:
    struct Impl;
    unique_ptr<Impl> _impl;
};

FO_END_NAMESPACE();
