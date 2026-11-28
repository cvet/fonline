//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#pragma once

#include "Containers.h"

FO_BEGIN_NAMESPACE

constexpr size_t Sha256DigestSize = 32;
using Sha256Digest = array<uint8_t, Sha256DigestSize>;

class Sha256Hasher final
{
public:
    Sha256Hasher() noexcept;

    void Reset() noexcept;
    void Update(const_span<uint8_t> data) noexcept;
    [[nodiscard]] auto Finalize() const noexcept -> Sha256Digest;

private:
    void Transform(const_span<uint8_t> block) noexcept;

    array<uint32_t, 8> _state {};
    array<uint8_t, 64> _buffer {};
    size_t _bufferSize {};
    uint64_t _totalSize {};
};

[[nodiscard]] auto ComputeSha256(const_span<uint8_t> data) noexcept -> Sha256Digest;
[[nodiscard]] auto ComputeHmacSha256(const_span<uint8_t> key, const_span<uint8_t> data) noexcept -> Sha256Digest;
[[nodiscard]] auto Sha256DigestToHex(const Sha256Digest& digest) -> string;
[[nodiscard]] auto TryParseSha256Digest(string_view value, Sha256Digest& digest) noexcept -> bool;

FO_END_NAMESPACE
