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

#include "Sha256.h"
#include "MemorySystem.h"
#include "SafeArithmetics.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

static constexpr array<uint32_t, 64> Sha256RoundConstants = {
    UINT32_C(0x428a2f98),
    UINT32_C(0x71374491),
    UINT32_C(0xb5c0fbcf),
    UINT32_C(0xe9b5dba5),
    UINT32_C(0x3956c25b),
    UINT32_C(0x59f111f1),
    UINT32_C(0x923f82a4),
    UINT32_C(0xab1c5ed5),
    UINT32_C(0xd807aa98),
    UINT32_C(0x12835b01),
    UINT32_C(0x243185be),
    UINT32_C(0x550c7dc3),
    UINT32_C(0x72be5d74),
    UINT32_C(0x80deb1fe),
    UINT32_C(0x9bdc06a7),
    UINT32_C(0xc19bf174),
    UINT32_C(0xe49b69c1),
    UINT32_C(0xefbe4786),
    UINT32_C(0x0fc19dc6),
    UINT32_C(0x240ca1cc),
    UINT32_C(0x2de92c6f),
    UINT32_C(0x4a7484aa),
    UINT32_C(0x5cb0a9dc),
    UINT32_C(0x76f988da),
    UINT32_C(0x983e5152),
    UINT32_C(0xa831c66d),
    UINT32_C(0xb00327c8),
    UINT32_C(0xbf597fc7),
    UINT32_C(0xc6e00bf3),
    UINT32_C(0xd5a79147),
    UINT32_C(0x06ca6351),
    UINT32_C(0x14292967),
    UINT32_C(0x27b70a85),
    UINT32_C(0x2e1b2138),
    UINT32_C(0x4d2c6dfc),
    UINT32_C(0x53380d13),
    UINT32_C(0x650a7354),
    UINT32_C(0x766a0abb),
    UINT32_C(0x81c2c92e),
    UINT32_C(0x92722c85),
    UINT32_C(0xa2bfe8a1),
    UINT32_C(0xa81a664b),
    UINT32_C(0xc24b8b70),
    UINT32_C(0xc76c51a3),
    UINT32_C(0xd192e819),
    UINT32_C(0xd6990624),
    UINT32_C(0xf40e3585),
    UINT32_C(0x106aa070),
    UINT32_C(0x19a4c116),
    UINT32_C(0x1e376c08),
    UINT32_C(0x2748774c),
    UINT32_C(0x34b0bcb5),
    UINT32_C(0x391c0cb3),
    UINT32_C(0x4ed8aa4a),
    UINT32_C(0x5b9cca4f),
    UINT32_C(0x682e6ff3),
    UINT32_C(0x748f82ee),
    UINT32_C(0x78a5636f),
    UINT32_C(0x84c87814),
    UINT32_C(0x8cc70208),
    UINT32_C(0x90befffa),
    UINT32_C(0xa4506ceb),
    UINT32_C(0xbef9a3f7),
    UINT32_C(0xc67178f2),
};

Sha256Hasher::Sha256Hasher() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    Reset();
}

void Sha256Hasher::Reset() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _state = {
        UINT32_C(0x6a09e667),
        UINT32_C(0xbb67ae85),
        UINT32_C(0x3c6ef372),
        UINT32_C(0xa54ff53a),
        UINT32_C(0x510e527f),
        UINT32_C(0x9b05688c),
        UINT32_C(0x1f83d9ab),
        UINT32_C(0x5be0cd19),
    };
    _buffer.fill(0);
    _bufferSize = 0;
    _totalSize = 0;
}

void Sha256Hasher::Update(const_span<uint8_t> data) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (data.empty()) {
        return;
    }

    _totalSize += numeric_cast<uint64_t>(data.size());

    if (_bufferSize != 0) {
        const size_t copy_size = std::min(data.size(), _buffer.size() - _bufferSize);
        MemCopy(_buffer.data() + _bufferSize, data.data(), copy_size);
        _bufferSize += copy_size;
        data = data.subspan(copy_size);

        if (_bufferSize == _buffer.size()) {
            Transform(_buffer);
            _bufferSize = 0;
        }
    }

    while (data.size() >= _buffer.size()) {
        Transform(data.first(_buffer.size()));
        data = data.subspan(_buffer.size());
    }

    if (!data.empty()) {
        MemCopy(_buffer.data(), data.data(), data.size());
        _bufferSize = data.size();
    }
}

auto Sha256Hasher::Finalize() const noexcept -> Sha256Digest
{
    FO_NO_STACK_TRACE_ENTRY();

    Sha256Hasher finalized {*this};
    const uint64_t total_bits = finalized._totalSize * 8;

    array<uint8_t, 128> padding {};
    padding[0] = 0x80;
    const size_t padding_size = finalized._bufferSize < 56 ? 56 - finalized._bufferSize : 120 - finalized._bufferSize;
    finalized.Update({padding.data(), padding_size});

    array<uint8_t, 8> length_bytes {};

    for (size_t index = 0; index != length_bytes.size(); ++index) {
        length_bytes[index] = numeric_cast<uint8_t>((total_bits >> ((length_bytes.size() - index - 1) * 8)) & UINT64_C(0xFF));
    }

    finalized.Update(length_bytes);

    Sha256Digest digest {};

    for (size_t state_index = 0; state_index != finalized._state.size(); ++state_index) {
        const uint32_t value = finalized._state[state_index];
        const size_t digest_offset = state_index * sizeof(value);
        digest[digest_offset] = numeric_cast<uint8_t>((value >> 24) & UINT32_C(0xFF));
        digest[digest_offset + 1] = numeric_cast<uint8_t>((value >> 16) & UINT32_C(0xFF));
        digest[digest_offset + 2] = numeric_cast<uint8_t>((value >> 8) & UINT32_C(0xFF));
        digest[digest_offset + 3] = numeric_cast<uint8_t>(value & UINT32_C(0xFF));
    }

    return digest;
}

void Sha256Hasher::Transform(const_span<uint8_t> block) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    array<uint32_t, 64> schedule {};

    for (size_t index = 0; index != 16; ++index) {
        const size_t block_offset = index * sizeof(uint32_t);
        schedule[index] = (uint32_t {block[block_offset]} << 24) | (uint32_t {block[block_offset + 1]} << 16) | (uint32_t {block[block_offset + 2]} << 8) | uint32_t {block[block_offset + 3]};
    }

    for (size_t index = 16; index != schedule.size(); ++index) {
        const uint32_t sigma0 = std::rotr(schedule[index - 15], 7) ^ std::rotr(schedule[index - 15], 18) ^ (schedule[index - 15] >> 3);
        const uint32_t sigma1 = std::rotr(schedule[index - 2], 17) ^ std::rotr(schedule[index - 2], 19) ^ (schedule[index - 2] >> 10);
        schedule[index] = schedule[index - 16] + sigma0 + schedule[index - 7] + sigma1;
    }

    uint32_t a = _state[0];
    uint32_t b = _state[1];
    uint32_t c = _state[2];
    uint32_t d = _state[3];
    uint32_t e = _state[4];
    uint32_t f = _state[5];
    uint32_t g = _state[6];
    uint32_t h = _state[7];

    for (size_t index = 0; index != schedule.size(); ++index) {
        const uint32_t sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const uint32_t choice = (e & f) ^ (~e & g);
        const uint32_t temp1 = h + sum1 + choice + Sha256RoundConstants[index] + schedule[index];
        const uint32_t sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    _state[0] += a;
    _state[1] += b;
    _state[2] += c;
    _state[3] += d;
    _state[4] += e;
    _state[5] += f;
    _state[6] += g;
    _state[7] += h;
}

auto ComputeSha256(const_span<uint8_t> data) noexcept -> Sha256Digest
{
    FO_NO_STACK_TRACE_ENTRY();

    Sha256Hasher hasher;
    hasher.Update(data);
    return hasher.Finalize();
}

auto ComputeHmacSha256(const_span<uint8_t> key, const_span<uint8_t> data) noexcept -> Sha256Digest
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr size_t block_size = 64;
    array<uint8_t, block_size> normalized_key {};

    if (key.size() > normalized_key.size()) {
        const Sha256Digest key_digest = ComputeSha256(key);
        MemCopy(normalized_key.data(), key_digest.data(), key_digest.size());
    }
    else if (!key.empty()) {
        MemCopy(normalized_key.data(), key.data(), key.size());
    }

    array<uint8_t, block_size> inner_pad {};
    array<uint8_t, block_size> outer_pad {};

    for (size_t index = 0; index != block_size; ++index) {
        inner_pad[index] = numeric_cast<uint8_t>(normalized_key[index] ^ UINT8_C(0x36));
        outer_pad[index] = numeric_cast<uint8_t>(normalized_key[index] ^ UINT8_C(0x5C));
    }

    Sha256Hasher inner_hasher;
    inner_hasher.Update(inner_pad);
    inner_hasher.Update(data);
    const Sha256Digest inner_digest = inner_hasher.Finalize();

    Sha256Hasher outer_hasher;
    outer_hasher.Update(outer_pad);
    outer_hasher.Update(inner_digest);
    return outer_hasher.Finalize();
}

auto Sha256DigestToHex(const Sha256Digest& digest) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr array<char, 16> hex_digits = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    string result;
    result.resize(digest.size() * 2);

    for (size_t index = 0; index != digest.size(); ++index) {
        result[index * 2] = hex_digits[digest[index] >> 4];
        result[index * 2 + 1] = hex_digits[digest[index] & 0x0F];
    }

    return result;
}

auto TryParseSha256Digest(string_view value, Sha256Digest& digest) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (value.size() != digest.size() * 2) {
        return false;
    }

    const auto parse_digit = [](char ch, uint8_t& digit) noexcept -> bool {
        if (ch >= '0' && ch <= '9') {
            digit = numeric_cast<uint8_t>(ch - '0');
            return true;
        }
        if (ch >= 'a' && ch <= 'f') {
            digit = numeric_cast<uint8_t>(ch - 'a' + 10);
            return true;
        }
        if (ch >= 'A' && ch <= 'F') {
            digit = numeric_cast<uint8_t>(ch - 'A' + 10);
            return true;
        }
        return false;
    };

    Sha256Digest parsed {};

    for (size_t index = 0; index != parsed.size(); ++index) {
        uint8_t high = 0;
        uint8_t low = 0;

        if (!parse_digit(value[index * 2], high) || !parse_digit(value[index * 2 + 1], low)) {
            return false;
        }

        parsed[index] = numeric_cast<uint8_t>((high << 4) | low);
    }

    digest = parsed;
    return true;
}

FO_END_NAMESPACE
