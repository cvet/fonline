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

#include "catch_amalgamated.hpp"

#include "Common.h"

FO_BEGIN_NAMESPACE

namespace
{
    auto FromHex(string_view hex) -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(hex.size() % 2 == 0, "Hex text must hold whole bytes", hex.size());

        vector<uint8_t> result;
        result.reserve(hex.size() / 2);

        for (size_t i = 0; i < hex.size(); i += 2) {
            uint8_t byte = 0;
            auto parse_result = std::from_chars(hex.data() + i, hex.data() + i + 2, byte, 16);
            FO_VERIFY_AND_THROW(parse_result.ec == std::errc {}, "Invalid hex digit in test data", hex.substr(i, 2));
            result.emplace_back(byte);
        }

        return result;
    }

    auto KeyFromHex(string_view hex) -> crypto::key_bytes
    {
        FO_STACK_TRACE_ENTRY();

        optional<crypto::key_bytes> key = crypto::parse_key(hex);
        FO_VERIFY_AND_THROW(key.has_value(), "Test key must be 64 hex digits", hex);
        return key.value();
    }

    auto ToHex(const_span<uint8_t> data) -> string
    {
        FO_STACK_TRACE_ENTRY();

        string result;

        for (uint8_t byte : data) {
            result += strex("{:02x}", byte).str();
        }

        return result;
    }

    auto TextBytes(string_view text) -> const_span<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        return make_const_span(text.data(), text.size());
    }
}

TEST_CASE("CryptographyX25519MatchesRfc7748")
{
    crypto::key_bytes alice_secret = KeyFromHex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
    crypto::key_bytes bob_secret = KeyFromHex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb");
    crypto::key_bytes alice_public = crypto::derive_public_key(alice_secret);
    crypto::key_bytes bob_public = crypto::derive_public_key(bob_secret);

    CHECK(crypto::format_key(alice_public) == "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
    CHECK(crypto::format_key(bob_public) == "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
    CHECK(crypto::format_key(crypto::x25519(alice_secret, bob_public)) == "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
    CHECK(crypto::format_key(crypto::x25519(bob_secret, alice_public)) == "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
}

TEST_CASE("CryptographyBlake2bAndHmac")
{
    SECTION("the hash of parts is the hash of their concatenation")
    {
        CHECK(ToHex(crypto::hash_data({})) == "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce");
        CHECK(ToHex(crypto::hash_data({TextBytes("abc")})) == "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923");
        CHECK(crypto::hash_data({TextBytes("a"), TextBytes(""), TextBytes("bc")}) == crypto::hash_data({TextBytes("abc")}));
    }

    // Reference values from the standard library HMAC over hashlib.blake2b, an independent implementation
    SECTION("HMAC uses the 128-byte block and hashes an overlong key first")
    {
        CHECK(ToHex(crypto::hmac(TextBytes("key"), {TextBytes("The quick brown fox "), TextBytes("jumps over the lazy dog")})) == "92294f92c0dfb9b00ec9ae8bd94d7e7d8a036b885a499f149dfe2fd2199394aaaf6b8894a1730cccb2cd050f9bcf5062a38b51b0dab33207f8ef35ae2c9df51b");

        vector<uint8_t> long_key(200);

        for (size_t i = 0; i < long_key.size(); i++) {
            long_key[i] = numeric_cast<uint8_t>(i);
        }

        CHECK(ToHex(crypto::hmac(long_key, {TextBytes("data")})) == "dd83a68ea9b851310510d607d0313b685f724931a2a647eb5cd2c13189622293b8beb6035ca8b00005f251bb5ab9483e9c93bcbc10430e91fbbf9cd946c461c3");
    }
}

TEST_CASE("CryptographyAeadMatchesRfc8439")
{
    crypto::key_bytes key {};

    for (size_t i = 0; i < key.size(); i++) {
        key[i] = numeric_cast<uint8_t>(0x80 + i);
    }

    crypto::aead_nonce nonce {};
    vector<uint8_t> nonce_bytes = FromHex("070000004041424344454647");
    std::ranges::copy(nonce_bytes, nonce.begin());

    vector<uint8_t> ad = FromHex("50515253c0c1c2c3c4c5c6c7");
    const_span<uint8_t> plaintext = TextBytes("Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.");
    string expected_ciphertext = "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116";
    string expected_tag = "1ae10b594f09e26a7e902ecbd0600691";

    vector<uint8_t> sealed(plaintext.size() + crypto::aead_tag_size);
    crypto::aead_seal(key, nonce, ad, plaintext, sealed);

    CHECK(ToHex(const_span<uint8_t> {sealed}.first(plaintext.size())) == expected_ciphertext);
    CHECK(ToHex(const_span<uint8_t> {sealed}.last(crypto::aead_tag_size)) == expected_tag);

    vector<uint8_t> opened(plaintext.size());
    REQUIRE(crypto::aead_open(key, nonce, ad, sealed, opened));
    CHECK(std::ranges::equal(opened, plaintext));

    SECTION("any changed byte fails authentication and leaves the output untouched")
    {
        for (size_t i = 0; i < sealed.size(); i += 7) {
            vector<uint8_t> tampered = sealed;
            tampered[i] ^= 0x01;
            vector<uint8_t> rejected(plaintext.size(), uint8_t {0xEE});

            CHECK_FALSE(crypto::aead_open(key, nonce, ad, tampered, rejected));
            CHECK(std::ranges::all_of(rejected, [](uint8_t byte) { return byte == 0xEE; }));
        }
    }

    SECTION("other associated data fails authentication")
    {
        vector<uint8_t> other_ad = ad;
        other_ad.back() ^= 0x80;

        CHECK_FALSE(crypto::aead_open(key, nonce, other_ad, sealed, opened));
    }
}

TEST_CASE("CryptographyKeyText")
{
    string hex = "00112233445566778899aabbccddeeff00112233445566778899AABBCCDDEEFF";
    optional<crypto::key_bytes> key = crypto::parse_key(hex);

    REQUIRE(key.has_value());
    CHECK(key->at(0) == 0x00);
    CHECK(key->at(15) == 0xFF);
    CHECK(key->at(31) == 0xFF);
    CHECK(crypto::format_key(key.value()) == strex(hex).lower().str());

    CHECK_FALSE(crypto::parse_key("").has_value());
    CHECK_FALSE(crypto::parse_key(string_view {hex}.substr(1)).has_value());
    CHECK_FALSE(crypto::parse_key(hex + "0").has_value());
    CHECK_FALSE(crypto::parse_key(string_view {"g0112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"}).has_value());
    CHECK_FALSE(crypto::parse_key(string_view {" 0112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"}).has_value());
}

TEST_CASE("CryptographySystemRandomness")
{
    crypto::key_bytes first = crypto::generate_secret_key();
    crypto::key_bytes second = crypto::generate_secret_key();

    CHECK_FALSE(crypto::is_equal(first, second));
    CHECK(crypto::is_equal(first, first));

    // Larger than one getentropy request, so the chunked path runs
    vector<uint8_t> large(1000);
    crypto::fill_random(large);

    CHECK(std::ranges::count(large, uint8_t {0}) < 50);

    crypto::wipe(first);
    CHECK(std::ranges::all_of(first, [](uint8_t byte) { return byte == 0; }));
}

FO_END_NAMESPACE
