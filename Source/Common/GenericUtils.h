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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

class HashStorage : public HashResolver
{
public:
    auto ToHashedString(string_view s) -> hstring override;
    auto ToHashedStringMustExists(string_view s) const -> hstring override;
    auto ResolveHash(hstring::hash_t h, bool* failed = nullptr) const -> hstring override;

private:
    unordered_map<hstring::hash_t, hstring::entry> _hashStorage {};
};

class Hashing final
{
public:
    Hashing() = delete;

    [[nodiscard]] static auto MurmurHash2(const void* data, size_t len) -> uint;
    [[nodiscard]] static auto MurmurHash2_64(const void* data, size_t len) -> uint64;
};

class Compressor final
{
public:
    Compressor() = delete;

    [[nodiscard]] static auto Compress(const_span<uint8> data) -> vector<uint8>;
    [[nodiscard]] static auto Uncompress(const_span<uint8> data, size_t mul_approx) -> vector<uint8>;
};

class GenericUtils final
{
public:
    GenericUtils() = delete;

    [[nodiscard]] static auto Random(int minimum, int maximum) -> int;
    [[nodiscard]] static auto Random(uint minimum, uint maximum) -> uint;
    [[nodiscard]] static auto Percent(int full, int peace) -> int;
    [[nodiscard]] static auto Percent(uint full, uint peace) -> uint;
    [[nodiscard]] static auto NumericalNumber(uint num) -> uint;
    [[nodiscard]] static auto IntersectCircleLine(int cx, int cy, int radius, int x1, int y1, int x2, int y2) -> bool;
    [[nodiscard]] static auto GetColorDay(const vector<int>& day_time, const vector<uint8>& colors, int game_time, int* light) -> ucolor;
    [[nodiscard]] static auto DistSqrt(int x1, int y1, int x2, int y2) -> uint;
    [[nodiscard]] static auto GetStepsCoords(int x1, int y1, int x2, int y2) -> tuple<float, float>;
    [[nodiscard]] static auto ChangeStepsCoords(float sx, float sy, float deq) -> tuple<float, float>;

    static void SetRandomSeed(int seed);
    static void ForkProcess();
    static void WriteSimpleTga(string_view fname, int width, int height, vector<ucolor> data);
};

class MatrixHelper final
{
public:
    MatrixHelper() = delete;

    static auto MatrixProject(float objx, float objy, float objz, const float model_matrix[16], const float proj_matrix[16], const int viewport[4], float* winx, float* winy, float* winz) -> bool;
    static auto MatrixUnproject(float winx, float winy, float winz, const float model_matrix[16], const float proj_matrix[16], const int viewport[4], float* objx, float* objy, float* objz) -> bool;
};
