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

#include "UpdaterFastServer.h"
#include "DataSerialization.h"
#include "Logging.h"

FO_BEGIN_NAMESPACE

UpdaterFastServer::UpdaterFastServer(ServerNetworkSettings& settings, const UpdaterBackend& updater_backend) :
    _settings {make_ptr(&settings)},
    _updaterBackend {make_ptr(&updater_backend)}
{
    FO_STACK_TRACE_ENTRY();

    const int32_t max_chunk_size = numeric_cast<int32_t>(ContentUpdateMaxChunkPayloadSize);
    _receiveBuf.resize(numeric_cast<size_t>(std::clamp(_settings->FastUpdateChunkSize, 1, max_chunk_size)) + 64);
    FO_VERIFY_AND_THROW(FillContentUpdateSecureRandom(_cookieSecret), "Can't initialize fast updater cookie secret");
}

auto UpdaterFastServer::Start() -> bool
{
    FO_STACK_TRACE_ENTRY();

    const int32_t bind_port = _settings->FastUpdateBindPort;

    if (!_settings->FastUpdateEnabled || !_settings->FastUpdateServerEnabled || !_updaterBackend->IsFastUpdateEnabled() || bind_port <= 0 || bind_port > numeric_cast<int32_t>(std::numeric_limits<uint16_t>::max())) {
        return false;
    }

    _started = _socket.bind(_settings->FastUpdateBindHost, numeric_cast<uint16_t>(bind_port));

    if (_started) {
        WriteLog("Fast updater UDP mirror started on {}:{}", _settings->FastUpdateBindHost, _settings->FastUpdateBindPort);
    }

    return _started;
}

void UpdaterFastServer::Stop() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _socket.close();
    _started = false;
}

void UpdaterFastServer::Poll()
{
    FO_STACK_TRACE_ENTRY();

    if (!_started) {
        return;
    }

    for (int32_t i = 0; i < 64; i++) {
        if (!_socket.can_read({})) {
            break;
        }

        string remote_host;
        uint16_t remote_port = 0;
        const int32_t received = _socket.receive_from(_receiveBuf, remote_host, remote_port);

        if (received <= 0) {
            break;
        }

        ContentUpdateChunkRequest request;

        if (!TryReadContentUpdateChunkRequestData({_receiveBuf.data(), numeric_cast<size_t>(received)}, request)) {
            continue;
        }

        if (request.SessionId != _updaterBackend->GetFastUpdateSessionId()) {
            continue;
        }

        const int64_t current_time_ms = nanotime::now().milliseconds();

        if (!IsCookieValid(request, remote_host, remote_port, current_time_ms)) {
            if (!CanSendChallenge(current_time_ms)) {
                continue;
            }

            const int32_t cookie_lifetime_seconds = std::clamp(_settings->FastUpdateCookieLifetimeSeconds, 5, 300);
            const int64_t cookie_expires_at = current_time_ms + numeric_cast<int64_t>(cookie_lifetime_seconds) * 1000;
            const ContentUpdateFastCookie cookie = MakeCookie(request, remote_host, remote_port, cookie_expires_at);
            const vector<uint8_t> challenge_data = MakeContentUpdateCookieChallengeData(ContentUpdateCookieChallenge {
                .SessionId = request.SessionId,
                .FileIndex = request.FileIndex,
                .ChunkIndex = request.ChunkIndex,
                .ClientNonce = request.ClientNonce,
                .CookieExpiresAt = cookie_expires_at,
                .Cookie = cookie,
            });

            FO_STRONG_ASSERT(challenge_data.size() <= numeric_cast<size_t>(received), "Fast updater cookie challenge must not amplify an unauthenticated request", challenge_data.size(), received);
            (void)_socket.send_to(remote_host, remote_port, challenge_data);
            continue;
        }

        if (!ConsumeResponseBudget(remote_host, numeric_cast<uint64_t>(std::clamp(_settings->FastUpdateChunkSize, 1, numeric_cast<int32_t>(ContentUpdateMaxChunkPayloadSize))) + 64, current_time_ms)) {
            continue;
        }

        uint64_t chunk_hash = 0;

        if (!_updaterBackend->ReadFastUpdateChunk(request.FileIndex, request.ChunkIndex, _payloadBuf, chunk_hash)) {
            continue;
        }

        MakeContentUpdateChunkData(
            ContentUpdateChunkDataHeader {
                .SessionId = request.SessionId,
                .FileIndex = request.FileIndex,
                .ChunkIndex = request.ChunkIndex,
                .ChunkSize = numeric_cast<uint32_t>(_payloadBuf.size()),
                .ChunkHash = chunk_hash,
            },
            _payloadBuf, _sendBuf);

        (void)_socket.send_to(remote_host, remote_port, _sendBuf);
    }
}

auto UpdaterFastServer::MakeCookie(const ContentUpdateChunkRequest& request, string_view remote_host, uint16_t remote_port, int64_t expires_at) const noexcept -> ContentUpdateFastCookie
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        vector<uint8_t> cookie_data;
        DataWriter writer {cookie_data};
        writer.Write<uint16_t>(numeric_cast<uint16_t>(remote_host.size()));
        writer.WriteStringBytes(remote_host);
        writer.Write<uint16_t>(remote_port);
        writer.Write<uint32_t>(request.SessionId);
        writer.Write<uint64_t>(request.ClientNonce);
        writer.Write<int64_t>(expires_at);

        const Sha256Digest digest = ComputeHmacSha256(_cookieSecret, cookie_data);
        ContentUpdateFastCookie cookie {};
        MemCopy(cookie.data(), digest.data(), cookie.size());
        return cookie;
    }
    catch (const std::exception&) {
        return {};
    }
}

auto UpdaterFastServer::IsCookieValid(const ContentUpdateChunkRequest& request, string_view remote_host, uint16_t remote_port, int64_t current_time_ms) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (request.ClientNonce == 0 || request.CookieExpiresAt <= current_time_ms) {
        return false;
    }

    const int32_t cookie_lifetime_seconds = std::clamp(_settings->FastUpdateCookieLifetimeSeconds, 5, 300);
    const int64_t max_future_ms = numeric_cast<int64_t>(cookie_lifetime_seconds) * 1000;

    if (request.CookieExpiresAt > current_time_ms + max_future_ms) {
        return false;
    }

    const ContentUpdateFastCookie expected = MakeCookie(request, remote_host, remote_port, request.CookieExpiresAt);
    uint8_t difference = 0;

    for (size_t index = 0; index != expected.size(); ++index) {
        difference = numeric_cast<uint8_t>(difference | numeric_cast<uint8_t>(expected[index] ^ request.Cookie[index]));
    }

    return difference == 0;
}

auto UpdaterFastServer::CanSendChallenge(int64_t current_time_ms) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const uint32_t rate_limit = numeric_cast<uint32_t>(std::clamp(_settings->FastUpdateChallengeRateLimit, 1, 1000000));

    if (_challengeWindowStartMs == 0 || current_time_ms < _challengeWindowStartMs || current_time_ms - _challengeWindowStartMs >= 1000) {
        _challengeWindowStartMs = current_time_ms;
        _challengeCount = 0;
    }

    if (_challengeCount >= rate_limit) {
        return false;
    }

    ++_challengeCount;
    return true;
}

auto UpdaterFastServer::ConsumeResponseBudget(string_view remote_host, uint64_t byte_count, int64_t current_time_ms) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const uint64_t global_rate = numeric_cast<uint64_t>(std::clamp(_settings->FastUpdateGlobalRateLimit, 1, std::numeric_limits<int32_t>::max()));
    const uint64_t global_burst = numeric_cast<uint64_t>(std::clamp(_settings->FastUpdateGlobalBurst, 1, std::numeric_limits<int32_t>::max()));
    const uint64_t address_rate = numeric_cast<uint64_t>(std::clamp(_settings->FastUpdatePerAddressRateLimit, 1, std::numeric_limits<int32_t>::max()));
    const uint64_t address_burst = numeric_cast<uint64_t>(std::clamp(_settings->FastUpdatePerAddressBurst, 1, std::numeric_limits<int32_t>::max()));

    const string address {remote_host};
    auto address_it = _addressBuckets.find(address);

    if (address_it == _addressBuckets.end()) {
        constexpr size_t max_address_buckets = 4096;

        if (_addressBuckets.size() >= max_address_buckets) {
            constexpr int64_t stale_bucket_ms = 5 * 60 * 1000;
            std::erase_if(_addressBuckets, [current_time_ms](const auto& entry) { return current_time_ms < entry.second.LastSeenMs || current_time_ms - entry.second.LastSeenMs >= stale_bucket_ms; });
        }

        if (_addressBuckets.size() >= max_address_buckets) {
            return false;
        }

        address_it = _addressBuckets.emplace(address, RateBucket {}).first;
    }

    if (!ConsumeBucket(address_it->second, byte_count, address_rate, address_burst, current_time_ms)) {
        return false;
    }

    return ConsumeBucket(_globalBucket, byte_count, global_rate, global_burst, current_time_ms);
}

auto UpdaterFastServer::ConsumeBucket(RateBucket& bucket, uint64_t byte_count, uint64_t rate_per_second, uint64_t burst, int64_t current_time_ms) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (bucket.LastRefillMs == 0 || current_time_ms < bucket.LastRefillMs) {
        bucket.Tokens = burst;
        bucket.LastRefillMs = current_time_ms;
    }
    else if (current_time_ms > bucket.LastRefillMs) {
        const uint64_t elapsed_ms = numeric_cast<uint64_t>(current_time_ms - bucket.LastRefillMs);
        const uint64_t refill = elapsed_ms > (std::numeric_limits<uint64_t>::max)() / rate_per_second ? burst : std::min(burst, elapsed_ms * rate_per_second / 1000);
        bucket.Tokens = bucket.Tokens > burst - std::min(refill, burst) ? burst : bucket.Tokens + refill;
        bucket.LastRefillMs = current_time_ms;
    }

    bucket.LastSeenMs = current_time_ms;

    if (byte_count > bucket.Tokens) {
        return false;
    }

    bucket.Tokens -= byte_count;
    return true;
}

FO_END_NAMESPACE
