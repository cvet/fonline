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

#include "HashedString.h"

FO_BEGIN_NAMESPACE();

hstring::entry hstring::_zeroEntry;

auto HashStorage::ToHashedString(string_view s) -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(std::same_as<hstring::hash_t, decltype(Hashing::MurmurHash2({}, {}))>);

    if (s.empty()) {
        return {};
    }

    const auto hash_value = Hashing::MurmurHash2(s.data(), s.length());
    FO_RUNTIME_ASSERT(hash_value != 0);

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

        hstring::entry entry = {.Hash = hash_value, .Str = string(s)};
        const auto [it, inserted] = _hashStorage.emplace(hash_value, std::move(entry));
        ignore_unused(inserted); // Do not assert because somebody else can insert it already

        return hstring(&it->second);
    }
}

auto HashStorage::ResolveHash(hstring::hash_t h) const -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

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

    throw HashResolveException("Can't resolve hash", h);
}

auto HashStorage::ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

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

auto Hashing::MurmurHash2(const void* data, size_t len) noexcept -> uint32
{
    FO_NO_STACK_TRACE_ENTRY();

    if (len == 0) {
        return 0;
    }

    constexpr uint32 seed = 0;
    constexpr uint32 m = 0x5BD1E995;
    constexpr auto r = 24;
    const auto* pdata = static_cast<const uint8*>(data);
    auto h = seed ^ static_cast<uint32>(len);

    while (len >= 4) {
        uint32 k = pdata[0];
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
    FO_NO_STACK_TRACE_ENTRY();

    if (len == 0) {
        return 0;
    }

    constexpr uint32 seed = 0;
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

FO_END_NAMESPACE();
