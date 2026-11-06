// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "BasicCore.h"
#include "Containers.h"
#include "ExceptionHandling.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(CryptographyException);

// One audited library behind engine types, so every build target runs the same primitives
namespace crypto
{
    constexpr size_t key_size {32};
    constexpr size_t hash_size {64};
    constexpr size_t hash_block_size {128};
    constexpr size_t aead_nonce_size {12};
    constexpr size_t aead_tag_size {16};

    using key_bytes = array<uint8_t, key_size>;
    using hash_bytes = array<uint8_t, hash_size>;
    using aead_nonce = array<uint8_t, aead_nonce_size>;

    // Operating-system randomness, the only source fit for key material
    void fill_random(span<uint8_t> buf);
    [[nodiscard]] auto generate_secret_key() -> key_bytes;

    // X25519 of RFC 7748; the secret key is clamped inside, so any 32 random bytes are a valid one
    [[nodiscard]] auto derive_public_key(const key_bytes& secret_key) noexcept -> key_bytes;
    [[nodiscard]] auto x25519(const key_bytes& secret_key, const key_bytes& public_key) noexcept -> key_bytes;

    // BLAKE2b-512 over the concatenation of the parts
    [[nodiscard]] auto hash_data(initializer_list<const_span<uint8_t>> parts) noexcept -> hash_bytes;
    // HMAC of RFC 2104 over BLAKE2b-512 and its 128-byte block, not BLAKE2b's own keyed mode
    [[nodiscard]] auto hmac(const_span<uint8_t> key, initializer_list<const_span<uint8_t>> parts) noexcept -> hash_bytes;

    // ChaCha20-Poly1305 of RFC 8439; the sealed form is the ciphertext followed by the tag
    void aead_seal(const key_bytes& key, const aead_nonce& nonce, const_span<uint8_t> ad, const_span<uint8_t> plaintext, span<uint8_t> sealed);
    [[nodiscard]] auto aead_open(const key_bytes& key, const aead_nonce& nonce, const_span<uint8_t> ad, const_span<uint8_t> sealed, span<uint8_t> plaintext) -> bool;

    // Constant time, so a comparison reveals nothing of where two keys differ
    [[nodiscard]] auto is_equal(const key_bytes& first, const key_bytes& second) noexcept -> bool;
    void wipe(span<uint8_t> buf) noexcept;

    [[nodiscard]] auto parse_key(string_view hex) noexcept -> optional<key_bytes>;
    [[nodiscard]] auto format_key(const key_bytes& value) -> string;
}

FO_END_NAMESPACE
