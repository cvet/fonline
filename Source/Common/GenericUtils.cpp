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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "GenericUtils.h"
#include "Application.h"
#include "DiskFileSystem.h"
#include "Log.h"

#include "zlib.h"

auto HashStorage::ToHashedString(string_view s) -> hstring
{
    NO_STACK_TRACE_ENTRY();

    static_assert(std::is_same_v<hstring::hash_t, decltype(Hashing::MurmurHash2({}, {}))>);

    if (s.empty()) {
        return {};
    }

    const auto hash_value = Hashing::MurmurHash2(s.data(), s.length());
    RUNTIME_ASSERT(hash_value != 0);

    {
        auto locker = std::shared_lock {_hashStorageLocker};

        if (const auto it = _hashStorage.find(hash_value); it != _hashStorage.end()) {
#if FO_DEBUG
            const auto collision_detected = s != it->second.Str;
#else
            const auto collision_detected = s.length() != it->second.Str.length();
#endif

            if (collision_detected) {
                throw HashCollisionException("Hash collision", s, it->second.Str, hash_value);
            }

            return hstring(&it->second);
        }
    }

    {
        // Add new entry
        auto locker = std::unique_lock {_hashStorageLocker};

        const auto [it, inserted] = _hashStorage.emplace(hash_value, hstring::entry {hash_value, string(s)});
        UNUSED_VARIABLE(inserted); // Do not assert because somebody else can insert it already

        return hstring(&it->second);
    }
}

auto HashStorage::ToHashedStringMustExists(string_view s) const -> hstring
{
    NO_STACK_TRACE_ENTRY();

    static_assert(std::is_same_v<hstring::hash_t, decltype(Hashing::MurmurHash2({}, {}))>);

    if (s.empty()) {
        return {};
    }

    const auto hash_value = Hashing::MurmurHash2(s.data(), s.length());
    RUNTIME_ASSERT(hash_value != 0);

    {
        auto locker = std::shared_lock {_hashStorageLocker};

        if (const auto it = _hashStorage.find(hash_value); it != _hashStorage.end()) {
#if FO_DEBUG
            const auto collision_detected = s != it->second.Str;
#else
            const auto collision_detected = s.length() != it->second.Str.length();
#endif

            if (collision_detected) {
                throw HashCollisionException("Hash collision", s, it->second.Str, hash_value);
            }

            return hstring(&it->second);
        }
    }

    throw HashInsertException("String value is not in hash storage", s);
}

auto HashStorage::ResolveHash(hstring::hash_t h) const -> hstring
{
    NO_STACK_TRACE_ENTRY();

    if (h == 0) {
        return {};
    }

    {
        auto locker = std::shared_lock {_hashStorageLocker};

        if (const auto it = _hashStorage.find(h); it != _hashStorage.end()) {
            return hstring(&it->second);
        }
    }

    BreakIntoDebugger("Can't resolve hash");

    throw HashResolveException("Can't resolve hash", h);
}

auto HashStorage::ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring
{
    NO_STACK_TRACE_ENTRY();

    if (h == 0) {
        return {};
    }

    {
        auto locker = std::shared_lock {_hashStorageLocker};

        if (const auto it = _hashStorage.find(h); it != _hashStorage.end()) {
            return hstring(&it->second);
        }
    }

    BreakIntoDebugger();

    if (failed != nullptr) {
        *failed = true;
    }

    return {};
}

auto Hashing::MurmurHash2(const void* data, size_t len) noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    if (len == 0) {
        return 0;
    }

    constexpr uint seed = 0;
    constexpr uint m = 0x5BD1E995;
    constexpr auto r = 24;
    const auto* pdata = static_cast<const uint8*>(data);
    auto h = seed ^ static_cast<uint>(len);

    while (len >= 4) {
        uint k = pdata[0];
        k |= pdata[1] << 8;
        k |= pdata[2] << 16;
        k |= pdata[3] << 24;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        pdata += 4;
        len -= 4;
    }

    switch (len) {
    case 3:
        h ^= pdata[2] << 16;
        [[fallthrough]];
    case 2:
        h ^= pdata[1] << 8;
        [[fallthrough]];
    case 1:
        h ^= pdata[0];
        h *= m;
        [[fallthrough]];
    default:
        break;
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

auto Hashing::MurmurHash2_64(const void* data, size_t len) noexcept -> uint64
{
    NO_STACK_TRACE_ENTRY();

    if (len == 0) {
        return 0;
    }

    constexpr uint seed = 0;
    constexpr auto m = 0xc6a4a7935bd1e995ULL;
    constexpr auto r = 47;
    const auto* pdata = static_cast<const uint8*>(data);
    const auto* pdata2 = reinterpret_cast<const uint64*>(pdata);
    const auto* end = pdata2 + len / 8;
    auto h = seed ^ len * m;

    while (pdata2 != end) {
        auto k = *pdata2++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const auto* data3 = reinterpret_cast<const uint8*>(pdata2);

    switch (len & 7) {
    case 7:
        h ^= static_cast<uint64>(data3[6]) << 48;
        [[fallthrough]];
    case 6:
        h ^= static_cast<uint64>(data3[5]) << 40;
        [[fallthrough]];
    case 5:
        h ^= static_cast<uint64>(data3[4]) << 32;
        [[fallthrough]];
    case 4:
        h ^= static_cast<uint64>(data3[3]) << 24;
        [[fallthrough]];
    case 3:
        h ^= static_cast<uint64>(data3[2]) << 16;
        [[fallthrough]];
    case 2:
        h ^= static_cast<uint64>(data3[1]) << 8;
        [[fallthrough]];
    case 1:
        h ^= static_cast<uint64>(data3[0]);
        h *= m;
        [[fallthrough]];
    default:
        break;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

auto Compressor::CalculateMaxCompressedBufSize(size_t initial_size) noexcept -> size_t
{
    NO_STACK_TRACE_ENTRY();

    return initial_size * 110 / 100 + 12;
}

auto Compressor::Compress(const_span<uint8> data) -> vector<uint8>
{
    STACK_TRACE_ENTRY();

    auto buf_len = static_cast<uLongf>(CalculateMaxCompressedBufSize(data.size()));
    auto buf = vector<uint8>(buf_len);

    if (compress2(buf.data(), &buf_len, data.data(), static_cast<uLong>(data.size()), Z_BEST_SPEED) != Z_OK) {
        return {};
    }

    buf.resize(buf_len);

    return buf;
}

auto Compressor::Decompress(const_span<uint8> data, size_t mul_approx) -> vector<uint8>
{
    STACK_TRACE_ENTRY();

    auto buf_len = static_cast<uLongf>(data.size() * mul_approx);
    auto buf = vector<uint8>(buf_len);

    while (true) {
        const auto result = uncompress(buf.data(), &buf_len, data.data(), static_cast<uLong>(data.size()));

        if (result == Z_BUF_ERROR) {
            buf_len *= 2;
            buf.resize(buf_len);
        }
        else if (result != Z_OK) {
            WriteLog("Unpack error {}", result);
            return {};
        }
        else {
            break;
        }
    }

    buf.resize(buf_len);

    return buf;
}

void GenericUtils::WriteSimpleTga(string_view fname, isize size, vector<ucolor> data)
{
    STACK_TRACE_ENTRY();

    auto file = DiskFileSystem::OpenFile(fname, true);
    RUNTIME_ASSERT(file);

    const uint8 header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
        static_cast<uint8>(size.width % 256), static_cast<uint8>(size.width / 256), //
        static_cast<uint8>(size.height % 256), static_cast<uint8>(size.height / 256), 4 * 8, 0x20};
    file.Write(header);

    file.Write(data.data(), data.size() * sizeof(uint));
}

// Default randomizer
static std::mt19937 RandomGenerator(std::random_device {}());
void GenericUtils::SetRandomSeed(int seed)
{
    STACK_TRACE_ENTRY();

    RandomGenerator = std::mt19937(seed);
}

auto GenericUtils::Random(int minimum, int maximum) -> int
{
    NO_STACK_TRACE_ENTRY();

    return std::uniform_int_distribution {minimum, maximum}(RandomGenerator);
}

auto GenericUtils::Random(uint minimum, uint maximum) -> uint
{
    NO_STACK_TRACE_ENTRY();

    return static_cast<uint>(Random(static_cast<int>(minimum), static_cast<int>(maximum)));
}

auto GenericUtils::Percent(int full, int peace) -> int
{
    NO_STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(full >= 0);

    if (full == 0) {
        return 0;
    }

    const auto percent = peace * 100 / full;

    return std::clamp(percent, 0, 100);
}

auto GenericUtils::Percent(uint full, uint peace) -> uint
{
    NO_STACK_TRACE_ENTRY();

    if (full == 0) {
        return 0;
    }

    const auto percent = peace * 100 / full;

    return std::clamp(percent, 0u, 100u);
}

auto GenericUtils::NumericalNumber(uint num) noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    if (num % 2 != 0) {
        return num * (num / 2 + 1);
    }

    return num * num / 2 + num / 2;
}

auto GenericUtils::IntersectCircleLine(int cx, int cy, int radius, int x1, int y1, int x2, int y2) noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto x01 = x1 - cx;
    const auto y01 = y1 - cy;
    const auto x02 = x2 - cx;
    const auto y02 = y2 - cy;
    const auto dx = x02 - x01;
    const auto dy = y02 - y01;
    const auto a = dx * dx + dy * dy;
    const auto b = 2 * (x01 * dx + y01 * dy);
    const auto c = x01 * x01 + y01 * y01 - radius * radius;

    if (-b < 0) {
        return c < 0;
    }
    if (-b < 2 * a) {
        return 4 * a * c - b * b < 0;
    }

    return a + b + c < 0;
}

auto GenericUtils::GetColorDay(const vector<int>& day_time, const vector<uint8>& colors, int game_time, int* light) -> ucolor
{
    NO_STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(day_time.size() == 4);
    RUNTIME_ASSERT(colors.size() == 12);

    uint8 result[3];
    const int color_r[4] = {colors[0], colors[1], colors[2], colors[3]};
    const int color_g[4] = {colors[4], colors[5], colors[6], colors[7]};
    const int color_b[4] = {colors[8], colors[9], colors[10], colors[11]};

    game_time %= 1440;
    int time;
    int duration;
    if (game_time >= day_time[0] && game_time < day_time[1]) {
        time = 0;
        game_time -= day_time[0];
        duration = day_time[1] - day_time[0];
    }
    else if (game_time >= day_time[1] && game_time < day_time[2]) {
        time = 1;
        game_time -= day_time[1];
        duration = day_time[2] - day_time[1];
    }
    else if (game_time >= day_time[2] && game_time < day_time[3]) {
        time = 2;
        game_time -= day_time[2];
        duration = day_time[3] - day_time[2];
    }
    else {
        time = 3;
        if (game_time >= day_time[3]) {
            game_time -= day_time[3];
        }
        else {
            game_time += 1440 - day_time[3];
        }
        duration = 1440 - day_time[3] + day_time[0];
    }

    if (duration == 0) {
        duration = 1;
    }

    result[0] = static_cast<uint8>(color_r[time] + (color_r[time < 3 ? time + 1 : 0] - color_r[time]) * game_time / duration);
    result[1] = static_cast<uint8>(color_g[time] + (color_g[time < 3 ? time + 1 : 0] - color_g[time]) * game_time / duration);
    result[2] = static_cast<uint8>(color_b[time] + (color_b[time < 3 ? time + 1 : 0] - color_b[time]) * game_time / duration);

    if (light != nullptr) {
        const auto max_light = (std::max(std::max(std::max(color_r[0], color_r[1]), color_r[2]), color_r[3]) + std::max(std::max(std::max(color_g[0], color_g[1]), color_g[2]), color_g[3]) + std::max(std::max(std::max(color_b[0], color_b[1]), color_b[2]), color_b[3])) / 3;
        const auto min_light = (std::min(std::min(std::min(color_r[0], color_r[1]), color_r[2]), color_r[3]) + std::min(std::min(std::min(color_g[0], color_g[1]), color_g[2]), color_g[3]) + std::min(std::min(std::min(color_b[0], color_b[1]), color_b[2]), color_b[3])) / 3;
        const auto cur_light = (result[0] + result[1] + result[2]) / 3;

        *light = Percent(max_light - min_light, max_light - cur_light);
        *light = std::clamp(*light, 0, 100);
    }

    return ucolor {result[0], result[1], result[2], 255};
}

auto GenericUtils::DistSqrt(ipos pos1, ipos pos2) noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    const auto dx = pos1.x - pos2.x;
    const auto dy = pos1.y - pos2.y;

    return static_cast<uint>(std::sqrt(static_cast<double>(dx * dx + dy * dy)));
}

auto GenericUtils::GetStepsCoords(ipos from_pos, ipos to_pos) noexcept -> fpos
{
    NO_STACK_TRACE_ENTRY();

    const auto dx = static_cast<float>(std::abs(to_pos.x - from_pos.x));
    const auto dy = static_cast<float>(std::abs(to_pos.y - from_pos.y));

    auto sx = 1.0f;
    auto sy = 1.0f;

    dx < dy ? sx = dx / dy : sy = dy / dx;

    if (to_pos.x < from_pos.x) {
        sx = -sx;
    }
    if (to_pos.y < from_pos.y) {
        sy = -sy;
    }

    return {sx, sy};
}

auto GenericUtils::ChangeStepsCoords(fpos pos, float deq) noexcept -> fpos
{
    NO_STACK_TRACE_ENTRY();

    const auto rad = deq * PI_FLOAT / 180.0f;
    const auto x = pos.x * std::cos(rad) - pos.y * std::sin(rad);
    const auto y = pos.x * std::sin(rad) + pos.y * std::cos(rad);

    return {x, y};
}

static void MultMatricesf(const float a[16], const float b[16], float r[16]) noexcept;
static void MultMatrixVecf(const float matrix[16], const float in[4], float out[4]) noexcept;
static auto InvertMatrixf(const float m[16], float inv_out[16]) noexcept -> bool;

auto MatrixHelper::MatrixProject(float objx, float objy, float objz, const float model_matrix[16], const float proj_matrix[16], const int viewport[4], float* winx, float* winy, float* winz) noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    float in[4];
    in[0] = objx;
    in[1] = objy;
    in[2] = objz;
    in[3] = 1.0f;

    float out[4];
    MultMatrixVecf(model_matrix, in, out);
    MultMatrixVecf(proj_matrix, out, in);

    if (in[3] == 0.0f) {
        return false;
    }

    in[0] /= in[3];
    in[1] /= in[3];
    in[2] /= in[3];

    in[0] = in[0] * 0.5f + 0.5f;
    in[1] = in[1] * 0.5f + 0.5f;
    in[2] = in[2] * 0.5f + 0.5f;

    in[0] = in[0] * static_cast<float>(viewport[2] + viewport[0]);
    in[1] = in[1] * static_cast<float>(viewport[3] + viewport[1]);

    *winx = in[0];
    *winy = in[1];
    *winz = in[2];

    return true;
}

auto MatrixHelper::MatrixUnproject(float winx, float winy, float winz, const float model_matrix[16], const float proj_matrix[16], const int viewport[4], float* objx, float* objy, float* objz) noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    float final_matrix[16];
    MultMatricesf(model_matrix, proj_matrix, final_matrix);

    if (!InvertMatrixf(final_matrix, final_matrix)) {
        return false;
    }

    float in[4];
    in[0] = winx;
    in[1] = winy;
    in[2] = winz;
    in[3] = 1.0f;

    in[0] = (in[0] - static_cast<float>(viewport[0])) / static_cast<float>(viewport[2]);
    in[1] = (in[1] - static_cast<float>(viewport[1])) / static_cast<float>(viewport[3]);

    in[0] = in[0] * 2 - 1;
    in[1] = in[1] * 2 - 1;
    in[2] = in[2] * 2 - 1;

    float out[4];
    MultMatrixVecf(final_matrix, in, out);

    if (out[3] == 0.0f) {
        return false;
    }

    out[0] /= out[3];
    out[1] /= out[3];
    out[2] /= out[3];
    *objx = out[0];
    *objy = out[1];
    *objz = out[2];

    return true;
}

static void MultMatricesf(const float a[16], const float b[16], float r[16]) noexcept
{
    NO_STACK_TRACE_ENTRY();

    for (auto i = 0; i < 4; i++) {
        for (auto j = 0; j < 4; j++) {
            r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] + a[i * 4 + 2] * b[2 * 4 + j] + a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
}

static void MultMatrixVecf(const float matrix[16], const float in[4], float out[4]) noexcept
{
    NO_STACK_TRACE_ENTRY();

    for (auto i = 0; i < 4; i++) {
        out[i] = in[0] * matrix[0 * 4 + i] + in[1] * matrix[1 * 4 + i] + in[2] * matrix[2 * 4 + i] + in[3] * matrix[3 * 4 + i];
    }
}

static auto InvertMatrixf(const float m[16], float inv_out[16]) noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    auto det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0.0f) {
        return false;
    }

    det = 1.0f / det;

    for (auto i = 0; i < 16; i++) {
        inv_out[i] = inv[i] * det;
    }

    return true;
}

struct StreamCompressor::Impl
{
    z_stream ZStream {};
};

StreamCompressor::StreamCompressor() noexcept = default;
StreamCompressor::StreamCompressor(StreamCompressor&&) noexcept = default;
auto StreamCompressor::operator=(StreamCompressor&&) noexcept -> StreamCompressor& = default;

StreamCompressor::~StreamCompressor()
{
    STACK_TRACE_ENTRY();

    Reset();
}

auto StreamCompressor::Compress(const_span<uint8> buf, vector<uint8>& temp_buf) -> const_span<uint8>
{
    STACK_TRACE_ENTRY();

    if (!_impl) {
        _impl = SafeAlloc::MakeUnique<Impl>();
        MemFill(&_impl->ZStream, 0, sizeof(z_stream));

        _impl->ZStream.zalloc = [](voidpf, uInt items, uInt size) -> void* {
            constexpr SafeAllocator<uint8> allocator;
            return allocator.allocate(static_cast<size_t>(items) * size);
        };
        _impl->ZStream.zfree = [](voidpf, voidpf address) {
            constexpr SafeAllocator<uint8> allocator;
            allocator.deallocate(static_cast<uint8*>(address), 0);
        };

        const auto deflate_init = deflateInit(&_impl->ZStream, Z_BEST_SPEED);
        RUNTIME_ASSERT(deflate_init == Z_OK);
    }

    if (temp_buf.size() < 1024) {
        temp_buf.resize(1024);
    }
    while (temp_buf.size() < Compressor::CalculateMaxCompressedBufSize(buf.size())) {
        temp_buf.resize(temp_buf.size() * 2);
    }

    _impl->ZStream.next_in = static_cast<Bytef*>(const_cast<uint8*>(buf.data()));
    _impl->ZStream.avail_in = static_cast<uInt>(buf.size());
    _impl->ZStream.next_out = static_cast<Bytef*>(temp_buf.data());
    _impl->ZStream.avail_out = static_cast<uInt>(temp_buf.size());

    const auto deflate_result = deflate(&_impl->ZStream, Z_SYNC_FLUSH);
    RUNTIME_ASSERT(deflate_result == Z_OK);

    const auto writed_len = static_cast<size_t>(_impl->ZStream.next_in - buf.data());
    RUNTIME_ASSERT(writed_len == buf.size());

    const auto compr_len = static_cast<size_t>(_impl->ZStream.next_out - temp_buf.data());
    return {temp_buf.data(), compr_len};
}

void StreamCompressor::Reset() noexcept
{
    STACK_TRACE_ENTRY();

    if (_impl) {
        deflateEnd(&_impl->ZStream);
        _impl.reset();
    }
}

struct StreamDecompressor::Impl
{
    z_stream ZStream {};
};

StreamDecompressor::StreamDecompressor() noexcept = default;
StreamDecompressor::StreamDecompressor(StreamDecompressor&&) noexcept = default;
auto StreamDecompressor::operator=(StreamDecompressor&&) noexcept -> StreamDecompressor& = default;

StreamDecompressor::~StreamDecompressor()
{
    STACK_TRACE_ENTRY();

    Reset();
}

auto StreamDecompressor::Uncompress(const_span<uint8> buf, vector<uint8>& temp_buf) -> const_span<uint8>
{
    STACK_TRACE_ENTRY();

    if (!_impl) {
        _impl = SafeAlloc::MakeUnique<Impl>();
        MemFill(&_impl->ZStream, 0, sizeof(z_stream));

        _impl->ZStream.zalloc = [](voidpf, uInt items, uInt size) -> void* {
            constexpr SafeAllocator<uint8> allocator;
            return allocator.allocate(static_cast<size_t>(items) * size);
        };
        _impl->ZStream.zfree = [](voidpf, voidpf address) {
            constexpr SafeAllocator<uint8> allocator;
            allocator.deallocate(static_cast<uint8*>(address), 0);
        };

        const auto inflate_init = inflateInit(&_impl->ZStream);
        RUNTIME_ASSERT(inflate_init == Z_OK);
    }

    if (temp_buf.size() < 1024) {
        temp_buf.resize(1024);
    }
    while (temp_buf.size() < buf.size() * 2) {
        temp_buf.resize(temp_buf.size() * 2);
    }

    _impl->ZStream.next_in = static_cast<Bytef*>(const_cast<uint8*>(buf.data()));
    _impl->ZStream.avail_in = numeric_cast<uInt>(buf.size());
    _impl->ZStream.next_out = static_cast<Bytef*>(temp_buf.data());
    _impl->ZStream.avail_out = numeric_cast<uInt>(temp_buf.size());

    const auto first_inflate = ::inflate(&_impl->ZStream, Z_SYNC_FLUSH);
    RUNTIME_ASSERT(first_inflate == Z_OK);

    auto uncompr_len = reinterpret_cast<size_t>(_impl->ZStream.next_out) - reinterpret_cast<size_t>(temp_buf.data());

    while (_impl->ZStream.avail_in != 0) {
        temp_buf.resize(temp_buf.size() * 2);

        _impl->ZStream.next_out = static_cast<Bytef*>(temp_buf.data() + uncompr_len);
        _impl->ZStream.avail_out = numeric_cast<uInt>(temp_buf.size() - uncompr_len);

        const auto next_inflate = ::inflate(&_impl->ZStream, Z_SYNC_FLUSH);
        RUNTIME_ASSERT(next_inflate == Z_OK);

        uncompr_len = reinterpret_cast<size_t>(_impl->ZStream.next_out) - reinterpret_cast<size_t>(temp_buf.data());
    }

    return {temp_buf.data(), uncompr_len};
}

void StreamDecompressor::Reset() noexcept
{
    STACK_TRACE_ENTRY();

    if (_impl) {
        inflateEnd(&_impl->ZStream);
        _impl.reset();
    }
}
