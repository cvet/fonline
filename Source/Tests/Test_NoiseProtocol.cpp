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

#include "NoiseProtocol.h"

FO_BEGIN_NAMESPACE

namespace
{
    struct NoiseVector
    {
        string_view Name {};
        string_view Prologue {};
        string_view InitiatorEphemeral {};
        string_view ResponderPublic {};
        string_view ResponderStatic {};
        string_view ResponderEphemeral {};
        string_view HandshakeHash {};
        vector<pair<string_view, string_view>> Messages {};
    };

    // Noise_NK_25519_ChaChaPoly_BLAKE2b from three independent implementations' published suites
    auto GetNoiseVectors() -> vector<NoiseVector>
    {
        vector<NoiseVector> result;

        result.emplace_back(NoiseVector {
            .Name = "cacophony",
            .Prologue = "4a6f686e2047616c74",
            .InitiatorEphemeral = "893e28b9dc6ca8d611ab664754b8ceb7bac5117349a4439a6b0569da977c464a",
            .ResponderPublic = "31e0303fd6418d2f8c0e78b91f22e8caed0fbe48656dcf4767e4834f701b8f62",
            .ResponderStatic = "4a3acbfdb163dec651dfa3194dece676d437029c62a408b4c5ea9114246e4893",
            .ResponderEphemeral = "bbdb4cdbd309f1a1f2e1456967fe288cadd6f712d65dc7b7793d5e63da6b375b",
            .HandshakeHash = "f87aa4eb6416e5b0d2b6e6f0b7bc41f3c5986a5d32d55c08d67cbd412f3ec2fa04d8e358ab95b3bbfab054a140a98eccf4284bb6309b600981d451ecac484932",
            .Messages =
                {
                    {"4c756477696720766f6e204d69736573", "ca35def5ae56cec33dc2036731ab14896bc4c75dbb07a61f879f8e3afa4c7944f3041e39b0c8ba56008f2d1183fea6ac83564ead0267b0842ec4c521ed1e1407"},
                    {"4d757272617920526f746862617264", "95ebc60d2b1fa672c1f46a8aa265ef51bfe38e7ccb39ec5be34069f1448088432281dcc1835131f305dca14525e15e27d1f32294aa835e40fc18be480c1db9"},
                    {"462e20412e20486179656b", "357e24e9f28ba22080666f7efacc01b2a0a4e358e742aeeff2aaf5"},
                    {"4361726c204d656e676572", "8b23b34ff3169de06a39551e969ca7876cc5122a4acff74bf2ec29"},
                    {"4a65616e2d426170746973746520536179", "5c104779b6f36e59fca73ed94b0ae092eae1d76dd109caf5060aaaedba385d7076"},
                    {"457567656e2042f6686d20766f6e2042617765726b", "34ae0518d0cd3aa641ed372ea94935ceecd87f8c4b422ce21a33d3f6f5493891e3e915d83f"},
                },
        });
        result.emplace_back(NoiseVector {
            .Name = "snow",
            .Prologue = "5468657265206973206e6f20726967687420616e642077726f6e672e2054686572652773206f6e6c792066756e20616e6420626f72696e672e",
            .InitiatorEphemeral = "151f660015d45eea5f28ee50258f33373c75906fb58e4ab59ed52dab535161cb",
            .ResponderPublic = "7ff2c76e27af9ed6b83e0e8987e9a125c2879a990b200823f56c648f5974d41d",
            .ResponderStatic = "0c0cac2c24c7ac76ab07049932d34f3aac2f791fd99ff957e81f685a7756138a",
            .ResponderEphemeral = "dafad8e4c6404375195a18b8f7cd7feb985220f2f2ecc307b381207193298c3b",
            .HandshakeHash = "",
            .Messages =
                {
                    {"a3a6441fd70fea11009bf133cd42402e358ab1024ed36f5199fef05203d6cac6", "d956ead53c15c6e114e9ee181ed233e2287ef647b65f1bdf1c455760ac7f2a7ce376799bd5adb227eb614be9db1d210bbe413a371e18ecfeb45bf4ca5eeb35a9668e8423875e16144100af1f849b5986"},
                    {"dba779f15f86d73d3c10aacecabd0dd1869308dc7f38f8c7f95274844e34618b", "77b149decc3c2339f0794c856db88c7de6ee665b923a43bad41bf5b54d80c51d26cc588eeb0d1a50a055c305f47bc0dde983039526354548b0abb04c2d3c1e5e7cc8d9cef41d0d4b7eb4b3cbfdd6a178"},
                    {"a40e189598732182bd93f25667217b0897cdf1be327f16236046aae1b1a05d02", "6595aea3a643319bb6f139f122d8fa32d7e1165721f5af678f51cf4d104c5b03aee688efa0ab2aa0f6e6309565653f4a"},
                    {"f825cd333af9f37889ecdcbc2f2f4862c8e117a058420074072f0488a90a6443", "9b8d0a0cbad3638d2cbef5bd69d23661e16b0c8692b968190052ac13751f17ac080e01d45668e04a365adaee3f9303d6"},
                },
        });
        result.emplace_back(NoiseVector {
            .Name = "noise-c",
            .Prologue = "50726f6c6f677565313233",
            .InitiatorEphemeral = "893e28b9dc6ca8d611ab664754b8ceb7bac5117349a4439a6b0569da977c464a",
            .ResponderPublic = "31e0303fd6418d2f8c0e78b91f22e8caed0fbe48656dcf4767e4834f701b8f62",
            .ResponderStatic = "4a3acbfdb163dec651dfa3194dece676d437029c62a408b4c5ea9114246e4893",
            .ResponderEphemeral = "bbdb4cdbd309f1a1f2e1456967fe288cadd6f712d65dc7b7793d5e63da6b375b",
            .HandshakeHash = "ae43dd84698159fea33f3638733584bedb74378bd418576f6cbfbb6702c483f7e6ef17408a2aa6a991bd6758dff089253c571816b0340145ed34f6e1844ed03a",
            .Messages =
                {
                    {"4c756477696720766f6e204d69736573", "ca35def5ae56cec33dc2036731ab14896bc4c75dbb07a61f879f8e3afa4c7944f3041e39b0c8ba56008f2d1183fea6acc9a221fce2945fd4e40396e046e0246b"},
                    {"4d757272617920526f746862617264", "95ebc60d2b1fa672c1f46a8aa265ef51bfe38e7ccb39ec5be34069f1448088432281dcc1835131f305dca14525e15e95bbefa674d47a13fdf88fe9c79dfca5"},
                    {"462e20412e20486179656b", "357e24e9f28ba22080666f7efacc01b2a0a4e358e742aeeff2aaf5"},
                    {"4361726c204d656e676572", "8b23b34ff3169de06a39551e969ca7876cc5122a4acff74bf2ec29"},
                    {"4a65616e2d426170746973746520536179", "5c104779b6f36e59fca73ed94b0ae092eae1d76dd109caf5060aaaedba385d7076"},
                    {"457567656e2042f6686d20766f6e2042617765726b", "34ae0518d0cd3aa641ed372ea94935ceecd87f8c4b422ce21a33d3f6f5493891e3e915d83f"},
                },
        });

        return result;
    }

    auto FromHex(string_view hex) -> vector<uint8_t>
    {
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
        optional<crypto::key_bytes> key = crypto::parse_key(hex);
        FO_VERIFY_AND_THROW(key.has_value(), "Test key must be 64 hex digits", hex);
        return key.value();
    }

    auto MakeHandshakePair(const_span<uint8_t> prologue) -> pair<NoiseHandshakeNK, NoiseHandshakeNK>
    {
        crypto::key_bytes responder_static = crypto::generate_secret_key();
        NoiseHandshakeNK initiator = NoiseHandshakeNK::CreateInitiator(prologue, crypto::derive_public_key(responder_static), crypto::generate_secret_key());
        NoiseHandshakeNK responder = NoiseHandshakeNK::CreateResponder(prologue, responder_static, crypto::generate_secret_key());
        return {std::move(initiator), std::move(responder)};
    }
}

TEST_CASE("NoiseHandshakeNKMatchesPublishedVectors")
{
    for (const NoiseVector& noise_vector : GetNoiseVectors()) {
        INFO(noise_vector.Name);

        vector<uint8_t> prologue = FromHex(noise_vector.Prologue);
        crypto::key_bytes responder_static = KeyFromHex(noise_vector.ResponderStatic);

        REQUIRE(crypto::format_key(crypto::derive_public_key(responder_static)) == noise_vector.ResponderPublic);

        NoiseHandshakeNK initiator = NoiseHandshakeNK::CreateInitiator(prologue, KeyFromHex(noise_vector.ResponderPublic), KeyFromHex(noise_vector.InitiatorEphemeral));
        NoiseHandshakeNK responder = NoiseHandshakeNK::CreateResponder(prologue, responder_static, KeyFromHex(noise_vector.ResponderEphemeral));

        REQUIRE(noise_vector.Messages.size() > 2);

        // Two handshake messages, then transport messages alternating from the initiator
        for (size_t i = 0; i < 2; i++) {
            ptr<NoiseHandshakeNK> writer = i == 0 ? &initiator : &responder;
            ptr<NoiseHandshakeNK> reader = i == 0 ? &responder : &initiator;
            vector<uint8_t> payload = FromHex(noise_vector.Messages[i].first);
            vector<uint8_t> message;
            vector<uint8_t> received_payload;

            REQUIRE(writer->IsWriteTurn());
            writer->WriteMessage(payload, message);
            CHECK(message == FromHex(noise_vector.Messages[i].second));
            REQUIRE(reader->ReadMessage(message, received_payload));
            CHECK(received_payload == payload);
        }

        REQUIRE(initiator.IsComplete());
        REQUIRE(responder.IsComplete());
        CHECK(initiator.GetHandshakeHash() == responder.GetHandshakeHash());

        if (!noise_vector.HandshakeHash.empty()) {
            CHECK(vector<uint8_t>(initiator.GetHandshakeHash().begin(), initiator.GetHandshakeHash().end()) == FromHex(noise_vector.HandshakeHash));
        }

        NoiseHandshakeNK::TransportCiphers initiator_ciphers = initiator.Split();
        NoiseHandshakeNK::TransportCiphers responder_ciphers = responder.Split();

        for (size_t i = 2; i < noise_vector.Messages.size(); i++) {
            bool from_initiator = i % 2 == 0;
            ptr<NoiseCipherState> sender = from_initiator ? &initiator_ciphers.Send : &responder_ciphers.Send;
            ptr<NoiseCipherState> receiver = from_initiator ? &responder_ciphers.Receive : &initiator_ciphers.Receive;
            vector<uint8_t> payload = FromHex(noise_vector.Messages[i].first);
            vector<uint8_t> ciphertext;
            vector<uint8_t> plaintext;

            sender->EncryptWithAd({}, payload, ciphertext);
            CHECK(ciphertext == FromHex(noise_vector.Messages[i].second));
            REQUIRE(receiver->DecryptWithAd({}, ciphertext, plaintext));
            CHECK(plaintext == payload);
        }
    }
}

TEST_CASE("NoiseHandshakeNKRejectsWhatItCannotAuthenticate")
{
    vector<uint8_t> prologue {1, 2, 3};

    SECTION("an initiator holding another responder key is refused at the first message")
    {
        crypto::key_bytes responder_static = crypto::generate_secret_key();
        NoiseHandshakeNK initiator = NoiseHandshakeNK::CreateInitiator(prologue, crypto::derive_public_key(crypto::generate_secret_key()), crypto::generate_secret_key());
        NoiseHandshakeNK responder = NoiseHandshakeNK::CreateResponder(prologue, responder_static, crypto::generate_secret_key());
        vector<uint8_t> message;
        vector<uint8_t> payload;

        initiator.WriteMessage({}, message);

        CHECK_FALSE(responder.ReadMessage(message, payload));
        CHECK_FALSE(responder.IsWriteTurn());
        CHECK_THROWS(responder.ReadMessage(message, payload));
    }

    SECTION("a different prologue is refused")
    {
        crypto::key_bytes responder_static = crypto::generate_secret_key();
        vector<uint8_t> other_prologue {1, 2, 4};
        NoiseHandshakeNK initiator = NoiseHandshakeNK::CreateInitiator(prologue, crypto::derive_public_key(responder_static), crypto::generate_secret_key());
        NoiseHandshakeNK responder = NoiseHandshakeNK::CreateResponder(other_prologue, responder_static, crypto::generate_secret_key());
        vector<uint8_t> message;
        vector<uint8_t> payload;

        initiator.WriteMessage({}, message);

        CHECK_FALSE(responder.ReadMessage(message, payload));
    }

    SECTION("a changed byte anywhere in either message is refused")
    {
        for (size_t message_index = 0; message_index < 2; message_index++) {
            for (size_t byte_index = 0; byte_index < NoiseHandshakeNK::MESSAGE_OVERHEAD; byte_index += 5) {
                auto [initiator, responder] = MakeHandshakePair(prologue);
                vector<uint8_t> first_message;
                vector<uint8_t> payload;

                initiator.WriteMessage({}, first_message);

                if (message_index == 0) {
                    first_message[byte_index] ^= 0x01;
                    CHECK_FALSE(responder.ReadMessage(first_message, payload));
                    continue;
                }

                REQUIRE(responder.ReadMessage(first_message, payload));

                vector<uint8_t> second_message;
                responder.WriteMessage({}, second_message);
                second_message[byte_index] ^= 0x01;

                CHECK_FALSE(initiator.ReadMessage(second_message, payload));
            }
        }
    }

    SECTION("a truncated message is refused")
    {
        auto [initiator, responder] = MakeHandshakePair(prologue);
        vector<uint8_t> message;
        vector<uint8_t> payload;

        initiator.WriteMessage({}, message);
        message.resize(NoiseHandshakeNK::MESSAGE_OVERHEAD - 1);

        CHECK_FALSE(responder.ReadMessage(message, payload));
    }

    SECTION("messages go in turn and split only after both")
    {
        auto [initiator, responder] = MakeHandshakePair(prologue);
        vector<uint8_t> message;
        vector<uint8_t> payload;

        CHECK_THROWS(responder.WriteMessage({}, message));
        CHECK_THROWS(initiator.Split());

        initiator.WriteMessage({}, message);

        CHECK_THROWS(initiator.WriteMessage({}, message));
    }
}

TEST_CASE("NoiseCipherStateNeverReusesANonce")
{
    auto [initiator, responder] = MakeHandshakePair({});
    vector<uint8_t> message;
    vector<uint8_t> payload;

    initiator.WriteMessage({}, message);
    REQUIRE(responder.ReadMessage(message, payload));
    message.clear();
    responder.WriteMessage({}, message);
    REQUIRE(initiator.ReadMessage(message, payload));

    NoiseHandshakeNK::TransportCiphers sender = initiator.Split();
    NoiseHandshakeNK::TransportCiphers receiver = responder.Split();
    vector<uint8_t> text {10, 20, 30};

    vector<uint8_t> first;
    vector<uint8_t> second;
    sender.Send.EncryptWithAd({}, text, first);
    sender.Send.EncryptWithAd({}, text, second);

    CHECK(first != second);

    SECTION("a replayed or reordered message fails and leaves the counter for the genuine one")
    {
        vector<uint8_t> plaintext;

        CHECK_FALSE(receiver.Receive.DecryptWithAd({}, second, plaintext));
        CHECK(receiver.Receive.GetNonce() == 0);
        REQUIRE(receiver.Receive.DecryptWithAd({}, first, plaintext));
        CHECK_FALSE(receiver.Receive.DecryptWithAd({}, first, plaintext));
        REQUIRE(receiver.Receive.DecryptWithAd({}, second, plaintext));
        CHECK(plaintext == vector<uint8_t> {10, 20, 30, 10, 20, 30});
    }

    SECTION("the two directions never share a key")
    {
        vector<uint8_t> plaintext;

        CHECK_FALSE(sender.Receive.DecryptWithAd({}, first, plaintext));
    }
}

FO_END_NAMESPACE
