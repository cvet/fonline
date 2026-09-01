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

#include "BasicCore.h"
#include "Containers.h"
#include "ExceptionHandling.h"
#include "SmartPointers.h"
#include "Threading.h"

FO_BEGIN_NAMESPACE

struct hstring
{
    using hash_t = uint64_t;

    struct entry
    {
        hash_t hash {};
        string str {};
    };

    constexpr hstring() noexcept = default;
    constexpr explicit hstring(ptr<const entry> static_storage_entry) noexcept :
        _entry {static_storage_entry}
    {
    }
    constexpr hstring(const hstring& other) noexcept :
        _entry {other._entry}
    {
    }
    constexpr hstring(hstring&& other) noexcept :
        _entry {other._entry}
    {
    }
    constexpr auto operator=(const hstring& other) noexcept -> hstring&
    {
        _entry = other._entry;
        return *this;
    }
    constexpr auto operator=(hstring&& other) noexcept -> hstring&
    {
        _entry = other._entry;
        return *this;
    }
    ~hstring() = default;

    // ReSharper disable once CppNonExplicitConversionOperator
    [[nodiscard]] operator string_view() const noexcept { return _entry->str; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _entry->hash != 0; }
    [[nodiscard]] constexpr auto operator==(const hstring& other) const noexcept -> bool { return _entry->hash == other._entry->hash; }
    [[nodiscard]] constexpr auto operator<(const hstring& other) const noexcept -> bool { return _entry->hash < other._entry->hash; }
    [[nodiscard]] constexpr auto as_hash() const noexcept -> hash_t { return _entry->hash; }
    [[nodiscard]] constexpr auto as_int64() const noexcept -> int64_t { return std::bit_cast<int64_t>(_entry->hash); }
    [[nodiscard]] constexpr auto as_uint64() const noexcept -> uint64_t { return _entry->hash; }
    [[nodiscard]] auto as_str() const noexcept -> string_view { return _entry->str; }
    [[nodiscard]] auto as_str_ptr() const noexcept -> ptr<const string> { return &_entry->str; }
    [[nodiscard]] constexpr auto c_str() const noexcept -> const char* { return _entry->str.c_str(); }

private:
    static entry _zero_entry;

    ptr<const entry> _entry {&_zero_entry};
#if UINTPTR_MAX == UINT32_MAX
    // hstring participates in fixed value-type layouts whose slots are hash-sized
    [[maybe_unused]] uint32_t _padding {};
#endif
};
static_assert(sizeof(hstring::hash_t) == 8);
static_assert(sizeof(hstring) == sizeof(hstring::hash_t));
static_assert(std::is_standard_layout_v<hstring>);
FO_DECLARE_TYPE_HASHER_EXT(FO_NAMESPACE hstring, v.as_hash());

FO_END_NAMESPACE
template<>
struct std::formatter<FO_NAMESPACE hstring> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE hstring& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE string_view>::format(value.as_str(), ctx);
    }
};
FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(HashResolveException);
FO_DECLARE_EXCEPTION(HashCollisionException);

class hash_resolver
{
public:
    [[nodiscard]] virtual auto to_hashed_string(string_view s) -> hstring = 0;
    [[nodiscard]] virtual auto resolve_hash(hstring::hash_t h) const -> hstring = 0;
    [[nodiscard]] virtual auto resolve_hash(hstring::hash_t h, nptr<bool> failed) const noexcept -> hstring = 0;
    virtual ~hash_resolver() = default;
};

class hash_storage : public hash_resolver
{
public:
    using hash_func = uint64_t (*)(const_span<uint8_t> data);
    using resolve_hash_failure_handler = function<void(hstring::hash_t hash)>;

    static auto default_hash(const_span<uint8_t> data) noexcept -> uint64_t;
    explicit hash_storage(hash_func func = default_hash);
    auto check_hashed_string(string_view s) const noexcept -> bool;
    auto to_hashed_string(string_view s) -> hstring override;
    auto resolve_hash(hstring::hash_t h) const -> hstring override;
    auto resolve_hash(hstring::hash_t h, nptr<bool> failed) const noexcept -> hstring override;
    void set_resolve_hash_failure_handler(resolve_hash_failure_handler handler);

private:
    void handle_resolve_hash_failure(hstring::hash_t h) const noexcept;

    hash_func _hash_func;
    mutable shared_mutex _hash_storage_locker {};
    unordered_map<hstring::hash_t, unique_ptr<hstring::entry>> _hash_storage FO_TSA_GUARDED_BY(_hash_storage_locker) {};
    mutable shared_mutex _resolve_hash_failure_handler_locker {};
    resolve_hash_failure_handler _resolve_hash_failure_handler FO_TSA_GUARDED_BY(_resolve_hash_failure_handler_locker) {};
};

FO_END_NAMESPACE
