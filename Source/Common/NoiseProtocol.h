// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(NoiseException);

// CipherState of the Noise Protocol Framework over ChaChaPoly
class NoiseCipherState final
{
public:
    NoiseCipherState() noexcept = default;
    explicit NoiseCipherState(const crypto::key_bytes& key) noexcept;
    NoiseCipherState(const NoiseCipherState&) = delete;
    NoiseCipherState(NoiseCipherState&&) noexcept = default;
    auto operator=(const NoiseCipherState&) = delete;
    auto operator=(NoiseCipherState&&) noexcept -> NoiseCipherState& = default;
    ~NoiseCipherState();

    [[nodiscard]] auto HasKey() const noexcept -> bool { return _hasKey; }
    [[nodiscard]] auto GetNonce() const noexcept -> uint64_t { return _nonce; }

    // Both append; a failed authentication leaves the nonce where it was
    void EncryptWithAd(const_span<uint8_t> ad, const_span<uint8_t> plaintext, vector<uint8_t>& ciphertext);
    auto DecryptWithAd(const_span<uint8_t> ad, const_span<uint8_t> ciphertext, vector<uint8_t>& plaintext) -> bool;

private:
    auto MakeNonce() const -> crypto::aead_nonce;

    crypto::key_bytes _key {};
    bool _hasKey {};
    uint64_t _nonce {};
};

// Noise_NK_25519_ChaChaPoly_BLAKE2b: the initiator knows the responder's static key beforehand, the responder learns no
// identity of the initiator
class NoiseHandshakeNK final
{
public:
    static constexpr string_view PROTOCOL_NAME {"Noise_NK_25519_ChaChaPoly_BLAKE2b"};
    // Each of the two messages carries an ephemeral key and the tag of its encrypted payload
    static constexpr size_t MESSAGE_OVERHEAD {crypto::key_size + crypto::aead_tag_size};

    struct TransportCiphers
    {
        NoiseCipherState Send {};
        NoiseCipherState Receive {};
    };

    NoiseHandshakeNK() = delete;
    NoiseHandshakeNK(const NoiseHandshakeNK&) = delete;
    NoiseHandshakeNK(NoiseHandshakeNK&&) noexcept = default;
    auto operator=(const NoiseHandshakeNK&) = delete;
    auto operator=(NoiseHandshakeNK&&) noexcept -> NoiseHandshakeNK& = default;
    ~NoiseHandshakeNK();

    [[nodiscard]] auto IsInitiator() const noexcept -> bool { return _initiator; }
    [[nodiscard]] auto IsComplete() const noexcept -> bool { return _messageIndex == MESSAGE_COUNT; }
    [[nodiscard]] auto IsWriteTurn() const noexcept -> bool;
    [[nodiscard]] auto GetHandshakeHash() const noexcept -> const crypto::hash_bytes& { return _handshakeHash; }

    // The ephemeral secret is a parameter so the specification's test vectors can fix it
    static auto CreateInitiator(const_span<uint8_t> prologue, const crypto::key_bytes& responder_public_key, const crypto::key_bytes& ephemeral_secret_key) -> NoiseHandshakeNK;
    static auto CreateResponder(const_span<uint8_t> prologue, const crypto::key_bytes& static_secret_key, const crypto::key_bytes& ephemeral_secret_key) -> NoiseHandshakeNK;

    void WriteMessage(const_span<uint8_t> payload, vector<uint8_t>& message);
    // A malformed or unauthenticated message ends the handshake, which then accepts nothing more
    auto ReadMessage(const_span<uint8_t> message, vector<uint8_t>& payload) -> bool;
    auto Split() -> TransportCiphers;

private:
    static constexpr size_t MESSAGE_COUNT {2};

    NoiseHandshakeNK(bool initiator, const_span<uint8_t> prologue, const crypto::key_bytes& static_secret_key, const crypto::key_bytes& responder_public_key, const crypto::key_bytes& ephemeral_secret_key);

    void MixHash(const_span<uint8_t> data) noexcept;
    void MixKey(const_span<uint8_t> input_key_material);
    void EncryptAndHash(const_span<uint8_t> plaintext, vector<uint8_t>& ciphertext);
    auto DecryptAndHash(const_span<uint8_t> ciphertext, vector<uint8_t>& plaintext) -> bool;
    void WipeHandshakeSecrets() noexcept;

    bool _initiator;
    size_t _messageIndex {};
    bool _failed {};
    bool _split {};
    crypto::hash_bytes _chainingKey {};
    crypto::hash_bytes _handshakeHash {};
    NoiseCipherState _cipher {};
    crypto::key_bytes _staticSecretKey {};
    crypto::key_bytes _responderPublicKey {};
    crypto::key_bytes _ephemeralSecretKey {};
    crypto::key_bytes _ephemeralPublicKey {};
    crypto::key_bytes _remoteEphemeralKey {};
};

FO_END_NAMESPACE
