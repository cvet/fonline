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

#include "SecureChannel.h"

FO_BEGIN_NAMESPACE

static auto GetPrologue() noexcept -> const_span<uint8_t>;
static void WriteFrameSize(size_t frame_size, vector<uint8_t>& out);

auto ParseSecureChannelKey(string_view hex, string_view setting_name) -> crypto::key_bytes
{
    optional<crypto::key_bytes> key = crypto::parse_key(hex);

    // Only the length travels in the report: the text may be a secret key a digit away from valid
    if (!key.has_value()) {
        throw SecureChannelException("Secure channel key must be 64 hexadecimal digits", setting_name, hex.size());
    }

    return key.value();
}

SecureChannelIdentity::SecureChannelIdentity(const crypto::key_bytes& secret_key) noexcept :
    _secretKey {secret_key},
    _publicKey {crypto::derive_public_key(secret_key)}
{
}

SecureChannelIdentity::~SecureChannelIdentity()
{
    crypto::wipe(_secretKey);
}

SecureChannel::SecureChannel(const_span<crypto::key_bytes> server_keys) :
    _isServer {false}
{
    if (server_keys.empty()) {
        throw SecureChannelException("No server key is pinned");
    }
    if (server_keys.size() > MAX_OFFERED_KEYS) {
        throw SecureChannelException("Too many pinned server keys", server_keys.size(), MAX_OFFERED_KEYS);
    }

    vector<uint8_t> offer;
    offer.reserve(1 + server_keys.size() * NoiseHandshakeNK::MESSAGE_OVERHEAD);
    offer.emplace_back(numeric_cast<uint8_t>(server_keys.size()));
    _offers.reserve(server_keys.size());

    for (const crypto::key_bytes& server_key : server_keys) {
        crypto::key_bytes ephemeral_key = crypto::generate_secret_key();
        NoiseHandshakeNK handshake = NoiseHandshakeNK::CreateInitiator(GetPrologue(), server_key, ephemeral_key);
        crypto::wipe(ephemeral_key);

        handshake.WriteMessage({}, offer);
        _offers.emplace_back(std::move(handshake));
    }

    WriteFrameSize(offer.size(), _handshakeOutput);
    _handshakeOutput.insert(_handshakeOutput.end(), offer.begin(), offer.end());
}

SecureChannel::SecureChannel(const SecureChannelIdentity& identity) :
    _isServer {true},
    _serverSecretKey {identity.GetSecretKey()}
{
}

SecureChannel::~SecureChannel()
{
    crypto::wipe(_serverSecretKey);
}

void SecureChannel::Receive(const_span<uint8_t> data, vector<uint8_t>& plaintext)
{
    FO_TRACE_ZONE(Network);

    if (_failed) {
        throw SecureChannelException("Secure channel received data after it failed", data.size());
    }

    auto fail_guard = scope_fail([this]() noexcept {
        _failed = true;
        _inbound.clear();
        _inboundPos = 0;
    });

    _inbound.insert(_inbound.end(), data.begin(), data.end());

    while (true) {
        size_t available = _inbound.size() - _inboundPos;

        if (available < sizeof(uint16_t)) {
            break;
        }

        size_t frame_size = (numeric_cast<size_t>(_inbound[_inboundPos]) << 8) | numeric_cast<size_t>(_inbound[_inboundPos + 1]);

        // Judged at the header, so a peer cannot make the channel buffer a frame it will reject anyway
        if (!IsValidFrameSize(frame_size)) {
            throw SecureChannelException("Secure channel frame has an invalid size", frame_size, _isServer, _established);
        }

        if (available < sizeof(uint16_t) + frame_size) {
            break;
        }

        auto frame = const_span<uint8_t> {_inbound}.subspan(_inboundPos + sizeof(uint16_t), frame_size);
        _inboundPos += sizeof(uint16_t) + frame_size;

        ProcessFrame(frame, plaintext);
    }

    if (_inboundPos == _inbound.size()) {
        _inbound.clear();
        _inboundPos = 0;
    }
    else if (_inboundPos != 0) {
        _inbound.erase(_inbound.begin(), _inbound.begin() + numeric_cast<ptrdiff_t>(_inboundPos));
        _inboundPos = 0;
    }
}

void SecureChannel::TakeHandshakeOutput(vector<uint8_t>& out)
{
    out.insert(out.end(), _handshakeOutput.begin(), _handshakeOutput.end());
    _handshakeOutput.clear();
}

void SecureChannel::Seal(const_span<uint8_t> plaintext, vector<uint8_t>& out)
{
    FO_TRACE_ZONE(Network);

    FO_VERIFY_AND_THROW(_established, "Secure channel sealed data before its handshake completed", _isServer, plaintext.size());

    TakeHandshakeOutput(out);

    size_t frames_count = (plaintext.size() + MAX_TRANSPORT_PAYLOAD - 1) / MAX_TRANSPORT_PAYLOAD;
    out.reserve(out.size() + plaintext.size() + frames_count * (sizeof(uint16_t) + crypto::aead_tag_size));

    for (size_t offset = 0; offset < plaintext.size();) {
        size_t payload_size = std::min(plaintext.size() - offset, MAX_TRANSPORT_PAYLOAD);

        WriteFrameSize(payload_size + crypto::aead_tag_size, out);
        _sendCipher.EncryptWithAd({}, plaintext.subspan(offset, payload_size), out);

        offset += payload_size;
    }
}

auto SecureChannel::IsValidFrameSize(size_t frame_size) const noexcept -> bool
{
    if (_established) {
        return frame_size >= crypto::aead_tag_size;
    }

    // The offer is a count and that many first messages, the answer an offer index and one second message
    if (!_isServer) {
        return frame_size == 1 + NoiseHandshakeNK::MESSAGE_OVERHEAD;
    }
    if (frame_size <= 1) {
        return false;
    }

    size_t offers_size = frame_size - 1;
    return offers_size % NoiseHandshakeNK::MESSAGE_OVERHEAD == 0 && offers_size / NoiseHandshakeNK::MESSAGE_OVERHEAD <= MAX_OFFERED_KEYS;
}

void SecureChannel::ProcessFrame(const_span<uint8_t> frame, vector<uint8_t>& plaintext)
{
    if (_established) {
        if (!_receiveCipher.DecryptWithAd({}, frame, plaintext)) {
            throw SecureChannelException("Secure channel frame failed authentication", frame.size(), _receiveCipher.GetNonce());
        }
    }
    else if (_isServer) {
        AcceptOffer(frame);
    }
    else {
        AcceptAnswer(frame);
    }
}

void SecureChannel::AcceptOffer(const_span<uint8_t> frame)
{
    FO_TRACE_ZONE(Network);

    size_t offers_count = frame[0];

    if (frame.size() != 1 + offers_count * NoiseHandshakeNK::MESSAGE_OVERHEAD) {
        throw SecureChannelException("Secure channel offer count does not match its size", offers_count, frame.size());
    }

    // One ephemeral key serves every attempt: only the attempt that opens is ever answered, so only it uses the key
    crypto::key_bytes ephemeral_key = crypto::generate_secret_key();
    auto wipe_ephemeral_key = scope_exit([&ephemeral_key]() noexcept { crypto::wipe(ephemeral_key); });

    for (size_t i = 0; i < offers_count; i++) {
        NoiseHandshakeNK handshake = NoiseHandshakeNK::CreateResponder(GetPrologue(), _serverSecretKey, ephemeral_key);
        vector<uint8_t> payload;

        if (!handshake.ReadMessage(frame.subspan(1 + i * NoiseHandshakeNK::MESSAGE_OVERHEAD, NoiseHandshakeNK::MESSAGE_OVERHEAD), payload)) {
            continue;
        }

        vector<uint8_t> answer;
        answer.emplace_back(numeric_cast<uint8_t>(i));
        handshake.WriteMessage({}, answer);

        WriteFrameSize(answer.size(), _handshakeOutput);
        _handshakeOutput.insert(_handshakeOutput.end(), answer.begin(), answer.end());

        CompleteHandshake(handshake);
        return;
    }

    throw SecureChannelException("No offered handshake matches the server key", offers_count);
}

void SecureChannel::AcceptAnswer(const_span<uint8_t> frame)
{
    FO_TRACE_ZONE(Network);

    size_t offer_index = frame[0];

    if (offer_index >= _offers.size()) {
        throw SecureChannelException("Server answered an offer that was not made", offer_index, _offers.size());
    }

    vector<uint8_t> payload;

    if (!_offers[offer_index].ReadMessage(frame.subspan(1), payload)) {
        throw SecureChannelException("Server answer failed authentication", offer_index);
    }

    CompleteHandshake(_offers[offer_index]);
    _offers.clear();
}

void SecureChannel::CompleteHandshake(NoiseHandshakeNK& handshake)
{
    NoiseHandshakeNK::TransportCiphers ciphers = handshake.Split();
    _sendCipher = std::move(ciphers.Send);
    _receiveCipher = std::move(ciphers.Receive);
    _established = true;

    crypto::wipe(_serverSecretKey);
}

static auto GetPrologue() noexcept -> const_span<uint8_t>
{
    return make_const_span(SecureChannel::PROLOGUE.data(), SecureChannel::PROLOGUE.size());
}

static void WriteFrameSize(size_t frame_size, vector<uint8_t>& out)
{
    FO_VERIFY_AND_THROW(frame_size <= SecureChannel::MAX_FRAME_SIZE, "Secure channel frame exceeds the 16-bit length field", frame_size);

    out.emplace_back(numeric_cast<uint8_t>(frame_size >> 8));
    out.emplace_back(numeric_cast<uint8_t>(frame_size & 0xFF));
}

FO_END_NAMESPACE
