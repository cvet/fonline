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

#include "NoiseProtocol.h"

FO_BEGIN_NAMESPACE

NoiseCipherState::NoiseCipherState(const crypto::key_bytes& key) noexcept :
    _key {key},
    _hasKey {true}
{
    FO_STACK_TRACE_ENTRY();
}

NoiseCipherState::~NoiseCipherState()
{
    FO_STACK_TRACE_ENTRY();

    crypto::wipe(_key);
}

void NoiseCipherState::EncryptWithAd(const_span<uint8_t> ad, const_span<uint8_t> plaintext, vector<uint8_t>& ciphertext)
{
    FO_STACK_TRACE_ENTRY();

    size_t offset = ciphertext.size();

    if (!_hasKey) {
        ciphertext.insert(ciphertext.end(), plaintext.begin(), plaintext.end());
        return;
    }

    crypto::aead_nonce nonce = MakeNonce();
    ciphertext.resize(offset + plaintext.size() + crypto::aead_tag_size);
    crypto::aead_seal(_key, nonce, ad, plaintext, span<uint8_t> {ciphertext}.subspan(offset));
    _nonce++;
}

auto NoiseCipherState::DecryptWithAd(const_span<uint8_t> ad, const_span<uint8_t> ciphertext, vector<uint8_t>& plaintext) -> bool
{
    FO_STACK_TRACE_ENTRY();

    size_t offset = plaintext.size();

    if (!_hasKey) {
        plaintext.insert(plaintext.end(), ciphertext.begin(), ciphertext.end());
        return true;
    }

    if (ciphertext.size() < crypto::aead_tag_size) {
        return false;
    }

    crypto::aead_nonce nonce = MakeNonce();
    plaintext.resize(offset + ciphertext.size() - crypto::aead_tag_size);

    if (!crypto::aead_open(_key, nonce, ad, ciphertext, span<uint8_t> {plaintext}.subspan(offset))) {
        plaintext.resize(offset);
        return false;
    }

    _nonce++;
    return true;
}

auto NoiseCipherState::MakeNonce() const -> crypto::aead_nonce
{
    FO_STACK_TRACE_ENTRY();

    // The largest value is reserved, and a counter that reached it must never encrypt again
    if (_nonce == std::numeric_limits<uint64_t>::max()) {
        throw NoiseException("Cipher nonce is exhausted");
    }

    // Four zero bytes, then the counter in little-endian order
    crypto::aead_nonce nonce {};

    for (size_t i = 0; i < sizeof(uint64_t); i++) {
        nonce[4 + i] = numeric_cast<uint8_t>((_nonce >> (i * 8)) & 0xFF);
    }

    return nonce;
}

NoiseHandshakeNK::NoiseHandshakeNK(bool initiator, const_span<uint8_t> prologue, const crypto::key_bytes& static_secret_key, const crypto::key_bytes& responder_public_key, const crypto::key_bytes& ephemeral_secret_key) :
    _initiator {initiator},
    _staticSecretKey {static_secret_key},
    _responderPublicKey {responder_public_key},
    _ephemeralSecretKey {ephemeral_secret_key},
    _ephemeralPublicKey {crypto::derive_public_key(ephemeral_secret_key)}
{
    FO_STACK_TRACE_ENTRY();

    static_assert(PROTOCOL_NAME.size() <= crypto::hash_size);
    memory::copy(_handshakeHash.data(), PROTOCOL_NAME.data(), PROTOCOL_NAME.size());
    _chainingKey = _handshakeHash;

    MixHash(prologue);

    // The pre-message pattern: the responder's static key is known before the first message
    MixHash(_responderPublicKey);
}

NoiseHandshakeNK::~NoiseHandshakeNK()
{
    FO_STACK_TRACE_ENTRY();

    WipeHandshakeSecrets();
}

auto NoiseHandshakeNK::CreateInitiator(const_span<uint8_t> prologue, const crypto::key_bytes& responder_public_key, const crypto::key_bytes& ephemeral_secret_key) -> NoiseHandshakeNK
{
    FO_STACK_TRACE_ENTRY();

    return NoiseHandshakeNK(true, prologue, {}, responder_public_key, ephemeral_secret_key);
}

auto NoiseHandshakeNK::CreateResponder(const_span<uint8_t> prologue, const crypto::key_bytes& static_secret_key, const crypto::key_bytes& ephemeral_secret_key) -> NoiseHandshakeNK
{
    FO_STACK_TRACE_ENTRY();

    return NoiseHandshakeNK(false, prologue, static_secret_key, crypto::derive_public_key(static_secret_key), ephemeral_secret_key);
}

auto NoiseHandshakeNK::IsWriteTurn() const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return !_failed && !IsComplete() && (_messageIndex % 2 == 0) == _initiator;
}

void NoiseHandshakeNK::WriteMessage(const_span<uint8_t> payload, vector<uint8_t>& message)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsWriteTurn(), "Noise handshake message written out of turn", _messageIndex, _initiator, _failed);

    message.insert(message.end(), _ephemeralPublicKey.begin(), _ephemeralPublicKey.end());
    MixHash(_ephemeralPublicKey);

    // The first message mixes es, the answer mixes ee
    const crypto::key_bytes& remote_key = _messageIndex == 0 ? _responderPublicKey : _remoteEphemeralKey;
    crypto::key_bytes shared_secret = crypto::x25519(_ephemeralSecretKey, remote_key);
    MixKey(shared_secret);
    crypto::wipe(shared_secret);

    EncryptAndHash(payload, message);
    _messageIndex++;
}

auto NoiseHandshakeNK::ReadMessage(const_span<uint8_t> message, vector<uint8_t>& payload) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_failed && !IsComplete() && !IsWriteTurn(), "Noise handshake message read out of turn", _messageIndex, _initiator, _failed);

    if (message.size() < MESSAGE_OVERHEAD) {
        _failed = true;
        return false;
    }

    memory::copy(_remoteEphemeralKey.data(), message.data(), crypto::key_size);
    MixHash(_remoteEphemeralKey);

    const crypto::key_bytes& local_key = _messageIndex == 0 ? _staticSecretKey : _ephemeralSecretKey;
    crypto::key_bytes shared_secret = crypto::x25519(local_key, _remoteEphemeralKey);
    MixKey(shared_secret);
    crypto::wipe(shared_secret);

    if (!DecryptAndHash(message.subspan(crypto::key_size), payload)) {
        _failed = true;
        return false;
    }

    _messageIndex++;
    return true;
}

auto NoiseHandshakeNK::Split() -> TransportCiphers
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsComplete() && !_split, "Noise handshake split before both messages were exchanged or split twice", _messageIndex, _split);

    crypto::hash_bytes temp_key = crypto::hmac(_chainingKey, {});
    constexpr uint8_t first_index = 1;
    crypto::hash_bytes first_output = crypto::hmac(temp_key, {make_const_span(&first_index, 1)});
    constexpr uint8_t second_index = 2;
    crypto::hash_bytes second_output = crypto::hmac(temp_key, {first_output, make_const_span(&second_index, 1)});

    // Keys are truncated to the cipher's length when the hash is longer
    crypto::key_bytes first_key {};
    crypto::key_bytes second_key {};
    memory::copy(first_key.data(), first_output.data(), first_key.size());
    memory::copy(second_key.data(), second_output.data(), second_key.size());

    TransportCiphers ciphers;
    ciphers.Send = NoiseCipherState(_initiator ? first_key : second_key);
    ciphers.Receive = NoiseCipherState(_initiator ? second_key : first_key);

    crypto::wipe(temp_key);
    crypto::wipe(first_output);
    crypto::wipe(second_output);
    crypto::wipe(first_key);
    crypto::wipe(second_key);
    WipeHandshakeSecrets();
    _split = true;
    return ciphers;
}

void NoiseHandshakeNK::MixHash(const_span<uint8_t> data) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _handshakeHash = crypto::hash_data({_handshakeHash, data});
}

void NoiseHandshakeNK::MixKey(const_span<uint8_t> input_key_material)
{
    FO_STACK_TRACE_ENTRY();

    crypto::hash_bytes temp_key = crypto::hmac(_chainingKey, {input_key_material});
    constexpr uint8_t first_index = 1;
    _chainingKey = crypto::hmac(temp_key, {make_const_span(&first_index, 1)});
    constexpr uint8_t second_index = 2;
    crypto::hash_bytes second_output = crypto::hmac(temp_key, {_chainingKey, make_const_span(&second_index, 1)});

    crypto::key_bytes cipher_key {};
    memory::copy(cipher_key.data(), second_output.data(), cipher_key.size());
    _cipher = NoiseCipherState(cipher_key);

    crypto::wipe(temp_key);
    crypto::wipe(second_output);
    crypto::wipe(cipher_key);
}

void NoiseHandshakeNK::EncryptAndHash(const_span<uint8_t> plaintext, vector<uint8_t>& ciphertext)
{
    FO_STACK_TRACE_ENTRY();

    size_t offset = ciphertext.size();
    _cipher.EncryptWithAd(_handshakeHash, plaintext, ciphertext);
    MixHash(const_span<uint8_t> {ciphertext}.subspan(offset));
}

auto NoiseHandshakeNK::DecryptAndHash(const_span<uint8_t> ciphertext, vector<uint8_t>& plaintext) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_cipher.DecryptWithAd(_handshakeHash, ciphertext, plaintext)) {
        return false;
    }

    MixHash(ciphertext);
    return true;
}

void NoiseHandshakeNK::WipeHandshakeSecrets() noexcept
{
    FO_STACK_TRACE_ENTRY();

    crypto::wipe(_chainingKey);
    crypto::wipe(_staticSecretKey);
    crypto::wipe(_ephemeralSecretKey);
    _cipher = NoiseCipherState();
}

FO_END_NAMESPACE
