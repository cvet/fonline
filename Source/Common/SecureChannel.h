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

#include "Common.h"

#include "NoiseProtocol.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION_EXT(SecureChannelException, NoiseException);

auto ParseSecureChannelKey(string_view hex, string_view setting_name) -> crypto::key_bytes;

// The server's static key pair; clients pin its public half
class SecureChannelIdentity final
{
public:
    explicit SecureChannelIdentity(const crypto::key_bytes& secret_key) noexcept;
    SecureChannelIdentity(const SecureChannelIdentity&) = delete;
    SecureChannelIdentity(SecureChannelIdentity&&) noexcept = delete;
    auto operator=(const SecureChannelIdentity&) = delete;
    auto operator=(SecureChannelIdentity&&) noexcept = delete;
    ~SecureChannelIdentity();

    [[nodiscard]] auto GetSecretKey() const noexcept -> const crypto::key_bytes& { return _secretKey; }
    [[nodiscard]] auto GetPublicKey() const noexcept -> const crypto::key_bytes& { return _publicKey; }

private:
    crypto::key_bytes _secretKey;
    crypto::key_bytes _publicKey;
};

// Noise NK over an ordered byte stream, framed by a big-endian 16-bit length. The client offers one handshake per pinned
// key, the server answers the one its key opens, and every later frame is one transport message
class SecureChannel final
{
public:
    static constexpr string_view PROLOGUE = "FOnline secure channel 1";
    static constexpr size_t MAX_FRAME_SIZE = 0xFFFF;
    static constexpr size_t MAX_TRANSPORT_PAYLOAD = MAX_FRAME_SIZE - crypto::aead_tag_size;
    static constexpr size_t MAX_OFFERED_KEYS = 4;

    explicit SecureChannel(const_span<crypto::key_bytes> server_keys);
    explicit SecureChannel(const SecureChannelIdentity& identity);
    SecureChannel(const SecureChannel&) = delete;
    SecureChannel(SecureChannel&&) noexcept = delete;
    auto operator=(const SecureChannel&) = delete;
    auto operator=(SecureChannel&&) noexcept = delete;
    ~SecureChannel();

    [[nodiscard]] auto IsServer() const noexcept -> bool { return _isServer; }
    [[nodiscard]] auto IsEstablished() const noexcept -> bool { return _established; }
    [[nodiscard]] auto HasHandshakeOutput() const noexcept -> bool { return !_handshakeOutput.empty(); }

    // Takes the peer's bytes in any chunking and appends the plaintext of every completed transport frame
    void Receive(const_span<uint8_t> data, vector<uint8_t>& plaintext);
    void TakeHandshakeOutput(vector<uint8_t>& out);
    // Pending handshake frames go out ahead of the sealed ones
    void Seal(const_span<uint8_t> plaintext, vector<uint8_t>& out);

private:
    auto IsValidFrameSize(size_t frame_size) const noexcept -> bool;
    void ProcessFrame(const_span<uint8_t> frame, vector<uint8_t>& plaintext);
    void AcceptOffer(const_span<uint8_t> frame);
    void AcceptAnswer(const_span<uint8_t> frame);
    void CompleteHandshake(NoiseHandshakeNK& handshake);

    bool _isServer;
    bool _established {};
    bool _failed {};
    crypto::key_bytes _serverSecretKey {};
    vector<NoiseHandshakeNK> _offers {};
    NoiseCipherState _sendCipher {};
    NoiseCipherState _receiveCipher {};
    vector<uint8_t> _handshakeOutput {};
    vector<uint8_t> _inbound {};
    size_t _inboundPos {};
};

FO_END_NAMESPACE
