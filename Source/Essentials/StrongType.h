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

FO_BEGIN_NAMESPACE();

struct strong_type_bool_test_tag
{
};

struct strong_type_arithmetics_tag
{
};

template<typename T, typename Tag, class... Opts>
    requires(std::is_arithmetic_v<T>)
struct strong_type
{
    using underlying_type = T;
    static constexpr bool has_bool_test = sizeof...(Opts) != 0 && (std::same_as<Opts, strong_type_bool_test_tag> || ...);
    static constexpr bool has_arithmetics = sizeof...(Opts) != 0 && (std::same_as<Opts, strong_type_arithmetics_tag> || ...);

    constexpr strong_type() noexcept = default;
    constexpr explicit strong_type(underlying_type v) noexcept :
        _value {std::move(v)}
    {
    }
    strong_type(const strong_type&) noexcept = default;
    strong_type(strong_type&&) noexcept = default;
    auto operator=(const strong_type&) noexcept -> strong_type& = default;
    auto operator=(strong_type&&) noexcept -> strong_type& = default;
    ~strong_type() = default;

    [[nodiscard]] constexpr explicit operator bool() const noexcept
        requires(has_bool_test)
    {
        return _value != underlying_type {};
    }

    [[nodiscard]] constexpr auto operator==(const strong_type& other) const noexcept -> bool { return _value == other._value; }
    [[nodiscard]] constexpr auto operator<(const strong_type& other) const noexcept -> bool { return _value < other._value; }
    [[nodiscard]] constexpr auto operator<=(const strong_type& other) const noexcept -> bool { return _value <= other._value; }
    [[nodiscard]] constexpr auto operator>(const strong_type& other) const noexcept -> bool { return _value > other._value; }
    [[nodiscard]] constexpr auto operator>=(const strong_type& other) const noexcept -> bool { return _value >= other._value; }

    [[nodiscard]] constexpr auto operator+(const strong_type& other) const noexcept -> strong_type
        requires(has_arithmetics)
    {
        return strong_type(_value + other._value);
    }
    [[nodiscard]] constexpr auto operator-(const strong_type& other) const noexcept -> strong_type
        requires(has_arithmetics)
    {
        return strong_type(_value - other._value);
    }
    [[nodiscard]] constexpr auto operator*(const strong_type& other) const noexcept -> strong_type
        requires(has_arithmetics)
    {
        return strong_type(_value * other._value);
    }
    [[nodiscard]] constexpr auto operator/(const strong_type& other) const noexcept -> strong_type
        requires(has_arithmetics)
    {
        return strong_type(_value / other._value);
    }
    [[nodiscard]] constexpr auto operator%(const strong_type& other) const noexcept -> strong_type
        requires(has_arithmetics)
    {
        return strong_type(_value % other._value);
    }

    [[nodiscard]] constexpr auto underlying_value() noexcept -> underlying_type& { return _value; }
    [[nodiscard]] constexpr auto underlying_value() const noexcept -> const underlying_type& { return _value; }

private:
    underlying_type _value {};
};

template<typename T>
concept is_strong_type = requires {
    typename T::underlying_type;
    std::has_unique_object_representations_v<T>;
    std::is_standard_layout_v<T>;
    sizeof(T) == sizeof(typename T::underlying_type);
};

FO_END_NAMESPACE();
template<typename T>
    requires(FO_NAMESPACE is_strong_type<T>)
struct std::formatter<T> : formatter<typename T::underlying_type> // NOLINT(cert-dcl58-cpp)
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        return formatter<typename T::underlying_type>::format(value.underlying_value(), ctx);
    }
};
FO_BEGIN_NAMESPACE();

FO_END_NAMESPACE();
template<typename T>
    requires(FO_NAMESPACE is_strong_type<T>)
inline auto operator>>(std::istream& istr, T& value) -> std::istream&
{
    typename T::underlying_type uv;

    if (istr >> uv) {
        value = T(uv);
    }
    else {
        value = T();
    }

    return istr;
}
FO_BEGIN_NAMESPACE();

FO_END_NAMESPACE();
template<typename T>
    requires(FO_NAMESPACE is_strong_type<T>)
struct FO_HASH_NAMESPACE hash<T>
{
    using is_avalanching = void;
    auto operator()(const T& v) const noexcept -> size_t
    {
        static_assert(std::has_unique_object_representations_v<T>);
        if constexpr (sizeof(v) <= sizeof(uint64_t)) {
            return static_cast<size_t>(detail::wyhash::hash(v.underlying_value()));
        }
        else {
            return static_cast<size_t>(detail::wyhash::hash(&v, sizeof(v)));
        }
    }
};
FO_BEGIN_NAMESPACE();

FO_END_NAMESPACE();
