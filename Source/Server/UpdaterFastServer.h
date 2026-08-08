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

#pragma once

#include "Common.h"

#include "ContentUpdater.h"
#include "NetSockets.h"
#include "Settings.h"
#include "UpdaterBackend.h"

FO_BEGIN_NAMESPACE

class UpdaterFastServer final
{
public:
    UpdaterFastServer() = delete;
    UpdaterFastServer(ServerNetworkSettings& settings, const UpdaterBackend& updater_backend);
    UpdaterFastServer(const UpdaterFastServer&) = delete;
    UpdaterFastServer(UpdaterFastServer&&) noexcept = delete;
    auto operator=(const UpdaterFastServer&) = delete;
    auto operator=(UpdaterFastServer&&) noexcept = delete;
    ~UpdaterFastServer() = default;

    [[nodiscard]] auto Start() -> bool;
    void Stop() noexcept;
    void Poll();
    [[nodiscard]] auto IsStarted() const noexcept -> bool { return _started; }

private:
    struct RateBucket
    {
        uint64_t Tokens {};
        int64_t LastRefillMs {};
        int64_t LastSeenMs {};
    };

    [[nodiscard]] auto MakeCookie(const ContentUpdateChunkRequest& request, string_view remote_host, uint16_t remote_port, int64_t expires_at) const noexcept -> ContentUpdateFastCookie;
    [[nodiscard]] auto IsCookieValid(const ContentUpdateChunkRequest& request, string_view remote_host, uint16_t remote_port, int64_t current_time_ms) const noexcept -> bool;
    [[nodiscard]] auto CanSendChallenge(int64_t current_time_ms) noexcept -> bool;
    [[nodiscard]] auto ConsumeResponseBudget(string_view remote_host, uint64_t byte_count, int64_t current_time_ms) -> bool;
    [[nodiscard]] static auto ConsumeBucket(RateBucket& bucket, uint64_t byte_count, uint64_t rate_per_second, uint64_t burst, int64_t current_time_ms) noexcept -> bool;

    ptr<ServerNetworkSettings> _settings;
    ptr<const UpdaterBackend> _updaterBackend;
    udp_socket _socket {};
    vector<uint8_t> _receiveBuf {};
    vector<uint8_t> _sendBuf {};
    vector<uint8_t> _payloadBuf {};
    Sha256Digest _cookieSecret {};
    RateBucket _globalBucket {};
    map<string, RateBucket> _addressBuckets {};
    int64_t _challengeWindowStartMs {};
    uint32_t _challengeCount {};
    bool _started {};
};

FO_END_NAMESPACE
