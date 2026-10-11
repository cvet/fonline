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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
#include "ExceptionHandling.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(CryptographyException);

// One audited library behind engine types, so every build target runs the same primitives
namespace crypto
{
    constexpr size_t key_size = 32;
    constexpr size_t hash_size = 64;
    constexpr size_t hash_block_size = 128;
    constexpr size_t aead_nonce_size = 12;
    constexpr size_t aead_tag_size = 16;

    using key_bytes = array<uint8_t, key_size>;
    using hash_bytes = array<uint8_t, hash_size>;
    using aead_nonce = array<uint8_t, aead_nonce_size>;

    // Operating-system randomness, the only source fit for key material
    void fill_random(span<uint8_t> buf);
    auto generate_secret_key() -> key_bytes;

    // X25519 of RFC 7748; the secret key is clamped inside, so any 32 random bytes are a valid one
    auto derive_public_key(const key_bytes& secret_key) noexcept -> key_bytes;
    auto x25519(const key_bytes& secret_key, const key_bytes& public_key) noexcept -> key_bytes;

    // BLAKE2b-512 over the concatenation of the parts
    auto hash_data(initializer_list<const_span<uint8_t>> parts) noexcept -> hash_bytes;
    // HMAC of RFC 2104 over BLAKE2b-512 and its 128-byte block, not BLAKE2b's own keyed mode
    auto hmac(const_span<uint8_t> key, initializer_list<const_span<uint8_t>> parts) noexcept -> hash_bytes;

    // ChaCha20-Poly1305 of RFC 8439; the sealed form is the ciphertext followed by the tag
    void aead_seal(const key_bytes& key, const aead_nonce& nonce, const_span<uint8_t> ad, const_span<uint8_t> plaintext, span<uint8_t> sealed);
    [[nodiscard]] auto aead_open(const key_bytes& key, const aead_nonce& nonce, const_span<uint8_t> ad, const_span<uint8_t> sealed, span<uint8_t> plaintext) -> bool;

    // Constant time, so a comparison reveals nothing of where two keys differ
    auto is_equal(const key_bytes& first, const key_bytes& second) noexcept -> bool;
    void wipe(span<uint8_t> buf) noexcept;

    auto parse_key(string_view hex) noexcept -> optional<key_bytes>;
    auto format_key(const key_bytes& value) -> string;
}

FO_END_NAMESPACE
