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

FO_BEGIN_NAMESPACE();

class HashStorage : public HashResolver
{
public:
    auto ToHashedString(string_view s) -> hstring override;
    auto ResolveHash(hstring::hash_t h) const -> hstring override;
    auto ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring override;

private:
    unordered_map<hstring::hash_t, hstring::entry> _hashStorage {};
    mutable std::shared_mutex _hashStorageLocker {};
};

class Hashing final
{
public:
    Hashing() = delete;

    [[nodiscard]] static auto MurmurHash2(const void* data, size_t len) noexcept -> uint32;
    [[nodiscard]] static auto MurmurHash2_64(const void* data, size_t len) noexcept -> uint64;
};

class Compressor final
{
public:
    Compressor() = delete;

    [[nodiscard]] static auto CalculateMaxCompressedBufSize(size_t initial_size) noexcept -> size_t;
    [[nodiscard]] static auto Compress(const_span<uint8> data) -> vector<uint8>;
    [[nodiscard]] static auto Decompress(const_span<uint8> data, size_t mul_approx) -> vector<uint8>;
};

class GenericUtils final
{
public:
    GenericUtils() = delete;

    [[nodiscard]] static auto Random(int32 minimum, int32 maximum) -> int32;
    [[nodiscard]] static auto Percent(int32 full, int32 peace) -> int32;
    [[nodiscard]] static auto NumericalNumber(int32 num) noexcept -> int32;
    [[nodiscard]] static auto IntersectCircleLine(int32 cx, int32 cy, int32 radius, int32 x1, int32 y1, int32 x2, int32 y2) noexcept -> bool;
    [[nodiscard]] static auto GetColorDay(const vector<int32>& day_time, const vector<uint8>& colors, int32 game_time, int32* light) -> ucolor;
    [[nodiscard]] static auto DistSqrt(ipos pos1, ipos pos2) -> int32;
    [[nodiscard]] static auto GetStepsCoords(ipos from_pos, ipos to_pos) noexcept -> fpos;
    [[nodiscard]] static auto ChangeStepsCoords(fpos pos, float32 deq) noexcept -> fpos;

    static void SetRandomSeed(int32 seed);
    static void WriteSimpleTga(string_view fname, isize size, vector<ucolor> data);
};

class MatrixHelper final
{
public:
    MatrixHelper() = delete;

    static auto MatrixProject(float32 objx, float32 objy, float32 objz, const float32 model_matrix[16], const float32 proj_matrix[16], const int32 viewport[4], float32* winx, float32* winy, float32* winz) -> bool;
    static auto MatrixUnproject(float32 winx, float32 winy, float32 winz, const float32 model_matrix[16], const float32 proj_matrix[16], const int32 viewport[4], float32* objx, float32* objy, float32* objz) -> bool;
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

    void Compress(const_span<uint8> buf, vector<uint8>& result);
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

    void Decompress(const_span<uint8> buf, vector<uint8>& result);
    void Reset() noexcept;

private:
    struct Impl;
    unique_ptr<Impl> _impl;
};

FO_END_NAMESPACE();
