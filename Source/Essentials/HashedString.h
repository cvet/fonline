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

#pragma once

#include "BasicCore.h"
#include "Containers.h"
#include "ExceptionHadling.h"

FO_BEGIN_NAMESPACE();

// Hashing
struct hstring
{
    using hash_t = uint32;

    struct entry
    {
        hash_t Hash {};
        string Str {};
    };

    constexpr hstring() noexcept = default;
    constexpr explicit hstring(const entry* static_storage_entry) noexcept :
        _entry {static_storage_entry}
    {
    }
    constexpr hstring(const hstring&) noexcept = default;
    constexpr hstring(hstring&&) noexcept = default;
    constexpr auto operator=(const hstring&) noexcept -> hstring& = default;
    constexpr auto operator=(hstring&&) noexcept -> hstring& = default;
    ~hstring() = default;

    // ReSharper disable once CppNonExplicitConversionOperator
    [[nodiscard]] operator string_view() const noexcept { return _entry->Str; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _entry->Hash != 0; }
    [[nodiscard]] constexpr auto operator==(const hstring& other) const noexcept -> bool { return _entry->Hash == other._entry->Hash; }
    [[nodiscard]] constexpr auto operator<(const hstring& other) const noexcept -> bool { return _entry->Hash < other._entry->Hash; }
    [[nodiscard]] constexpr auto as_hash() const noexcept -> hash_t { return _entry->Hash; }
    [[nodiscard]] constexpr auto as_int32() const noexcept -> int32 { return std::bit_cast<int32>(_entry->Hash); }
    [[nodiscard]] constexpr auto as_uint32() const noexcept -> uint32 { return _entry->Hash; }
    [[nodiscard]] constexpr auto as_str() const noexcept -> const string& { return _entry->Str; }

private:
    static entry _zeroEntry;

    const entry* _entry {&_zeroEntry};
};
static_assert(sizeof(hstring::hash_t) == 4);
static_assert(std::is_standard_layout_v<hstring>);
FO_DECLARE_TYPE_HASHER_EXT(FO_NAMESPACE hstring, v.as_hash());

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE hstring> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE hstring& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE string_view>::format(value.as_str(), ctx);
    }
};
FO_BEGIN_NAMESPACE();

// Hashing
FO_DECLARE_EXCEPTION(HashResolveException);
FO_DECLARE_EXCEPTION(HashCollisionException);

class HashResolver
{
public:
    virtual ~HashResolver() = default;
    [[nodiscard]] virtual auto ToHashedString(string_view s) -> hstring = 0;
    [[nodiscard]] virtual auto ResolveHash(hstring::hash_t h) const -> hstring = 0;
    [[nodiscard]] virtual auto ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring = 0;
};

class HashStorage : public HashResolver
{
public:
    auto ToHashedString(string_view s) -> hstring override;
    auto ResolveHash(hstring::hash_t h) const -> hstring override;
    auto ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring override;

private:
    unordered_map<hstring::hash_t, hstring::entry> _hashStorage {};
    mutable std::shared_mutex _hashStorageLocker {};
};

class Hashing final
{
public:
    Hashing() = delete;

    [[nodiscard]] static auto MurmurHash2(const void* data, size_t len) noexcept -> uint32;
    [[nodiscard]] static auto MurmurHash2_64(const void* data, size_t len) noexcept -> uint64;
};

FO_END_NAMESPACE();
