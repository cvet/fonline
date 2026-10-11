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

#include "Cryptography.h"
#include "Platform.h"
#include "SafeArithmetics.h"
#include "StackTrace.h"

#include "monocypher.h"

FO_BEGIN_NAMESPACE

static auto parse_hex_digit(char ch) noexcept -> optional<uint8_t>;

void crypto::fill_random(span<uint8_t> buf)
{
    if (!platform::fill_system_random(buf)) {
        throw CryptographyException("Operating system randomness is unavailable", buf.size());
    }
}

auto crypto::generate_secret_key() -> key_bytes
{
    key_bytes secret_key {};
    fill_random(secret_key);
    return secret_key;
}

auto crypto::derive_public_key(const key_bytes& secret_key) noexcept -> key_bytes
{
    key_bytes public_key {};
    crypto_x25519_public_key(public_key.data(), secret_key.data());
    return public_key;
}

auto crypto::x25519(const key_bytes& secret_key, const key_bytes& public_key) noexcept -> key_bytes
{
    key_bytes shared_secret {};
    crypto_x25519(shared_secret.data(), secret_key.data(), public_key.data());
    return shared_secret;
}

auto crypto::hash_data(initializer_list<const_span<uint8_t>> parts) noexcept -> hash_bytes
{
    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, hash_size);

    for (const_span<uint8_t> part : parts) {
        crypto_blake2b_update(&ctx, part.data(), part.size());
    }

    hash_bytes result {};
    crypto_blake2b_final(&ctx, result.data());
    return result;
}

auto crypto::hmac(const_span<uint8_t> key, initializer_list<const_span<uint8_t>> parts) noexcept -> hash_bytes
{
    array<uint8_t, hash_block_size> block_key {};

    if (key.size() > hash_block_size) {
        hash_bytes key_hash = hash_data({key});
        memory::copy(block_key.data(), key_hash.data(), key_hash.size());
        wipe(key_hash);
    }
    else {
        memory::copy(block_key.data(), key.data(), key.size());
    }

    array<uint8_t, hash_block_size> pad {};

    for (size_t i = 0; i < hash_block_size; i++) {
        pad[i] = numeric_cast<uint8_t>(block_key[i] ^ 0x36);
    }

    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, hash_size);
    crypto_blake2b_update(&ctx, pad.data(), pad.size());

    for (const_span<uint8_t> part : parts) {
        crypto_blake2b_update(&ctx, part.data(), part.size());
    }

    hash_bytes inner_hash {};
    crypto_blake2b_final(&ctx, inner_hash.data());

    for (size_t i = 0; i < hash_block_size; i++) {
        pad[i] = numeric_cast<uint8_t>(block_key[i] ^ 0x5C);
    }

    hash_bytes result = hash_data({pad, inner_hash});

    wipe(block_key);
    wipe(pad);
    wipe(inner_hash);
    return result;
}

void crypto::aead_seal(const key_bytes& key, const aead_nonce& nonce, const_span<uint8_t> ad, const_span<uint8_t> plaintext, span<uint8_t> sealed)
{
    FO_VERIFY_AND_THROW(sealed.size() == plaintext.size() + aead_tag_size, "Sealed buffer must hold the ciphertext and its tag", plaintext.size(), sealed.size());

    // The streaming context rekeys after its first message, so a fresh one per message is what keeps this RFC 8439
    crypto_aead_ctx ctx;
    crypto_aead_init_ietf(&ctx, key.data(), nonce.data());
    crypto_aead_write(&ctx, sealed.data(), sealed.data() + plaintext.size(), ad.data(), ad.size(), plaintext.data(), plaintext.size());
    crypto_wipe(&ctx, sizeof(ctx));
}

auto crypto::aead_open(const key_bytes& key, const aead_nonce& nonce, const_span<uint8_t> ad, const_span<uint8_t> sealed, span<uint8_t> plaintext) -> bool
{
    FO_VERIFY_AND_THROW(sealed.size() >= aead_tag_size, "Sealed message is shorter than its tag", sealed.size());
    FO_VERIFY_AND_THROW(plaintext.size() == sealed.size() - aead_tag_size, "Plaintext buffer must match the sealed ciphertext", sealed.size(), plaintext.size());

    size_t text_size = plaintext.size();
    crypto_aead_ctx ctx;
    crypto_aead_init_ietf(&ctx, key.data(), nonce.data());
    int32_t mismatch = crypto_aead_read(&ctx, plaintext.data(), sealed.data() + text_size, ad.data(), ad.size(), sealed.data(), text_size);
    crypto_wipe(&ctx, sizeof(ctx));
    return mismatch == 0;
}

auto crypto::is_equal(const key_bytes& first, const key_bytes& second) noexcept -> bool
{
    return crypto_verify32(first.data(), second.data()) == 0;
}

void crypto::wipe(span<uint8_t> buf) noexcept
{
    if (!buf.empty()) {
        crypto_wipe(buf.data(), buf.size());
    }
}

auto crypto::parse_key(string_view hex) noexcept -> optional<key_bytes>
{
    if (hex.size() != key_size * 2) {
        return std::nullopt;
    }

    key_bytes result {};

    for (size_t i = 0; i < key_size; i++) {
        optional<uint8_t> high = parse_hex_digit(hex[i * 2]);
        optional<uint8_t> low = parse_hex_digit(hex[i * 2 + 1]);

        if (!high.has_value() || !low.has_value()) {
            wipe(result);
            return std::nullopt;
        }

        result[i] = numeric_cast<uint8_t>((high.value() << 4) | low.value());
    }

    return result;
}

auto crypto::format_key(const key_bytes& value) -> string
{
    constexpr string_view digits = "0123456789abcdef";

    string result;
    result.reserve(key_size * 2);

    for (uint8_t byte : value) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0F]);
    }

    return result;
}

static auto parse_hex_digit(char ch) noexcept -> optional<uint8_t>
{
    if (ch >= '0' && ch <= '9') {
        return numeric_cast<uint8_t>(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f') {
        return numeric_cast<uint8_t>(ch - 'a' + 10);
    }
    if (ch >= 'A' && ch <= 'F') {
        return numeric_cast<uint8_t>(ch - 'A' + 10);
    }

    return std::nullopt;
}

FO_END_NAMESPACE
