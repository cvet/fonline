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
//

#pragma once

#include "Containers.h"
#include "HashedString.h"
#include "TextConversions.h"

FO_BEGIN_NAMESPACE

class strex;
class strvex;
class u8strex;
class u8strvex;

namespace text_format_detail
{
    template<typename T>
    using plain_format_arg_t = std::remove_cvref_t<T>;

    template<typename T>
    struct is_native_char_text_arg : std::false_type
    {
    };

    template<typename Traits, typename Allocator>
    struct is_native_char_text_arg<std::basic_string<char, Traits, Allocator>> : std::true_type
    {
    };

    template<typename Traits>
    struct is_native_char_text_arg<std::basic_string_view<char, Traits>> : std::true_type
    {
    };

    template<typename T>
    inline constexpr bool IsUtf8StrictArg = std::same_as<plain_format_arg_t<T>, u8string> || std::same_as<plain_format_arg_t<T>, u8string_view> || std::same_as<plain_format_arg_t<T>, u8string_view_nt> || std::same_as<plain_format_arg_t<T>, hstring>;

    template<typename T>
    inline constexpr bool IsStrictArg = IsUtf8StrictArg<T>;

    template<typename T>
    inline constexpr bool IsVolatileStrictArg = IsStrictArg<T> && std::is_volatile_v<std::remove_reference_t<T>>;

    template<typename T>
    inline constexpr bool IsTextCodeUnit = std::same_as<std::remove_cv_t<T>, char> || std::same_as<std::remove_cv_t<T>, char8_t> || std::same_as<std::remove_cv_t<T>, wchar_t> || std::same_as<std::remove_cv_t<T>, char16_t> || std::same_as<std::remove_cv_t<T>, char32_t>;

    template<typename T>
    inline constexpr bool IsRawTextArg = (std::is_pointer_v<plain_format_arg_t<T>> && IsTextCodeUnit<std::remove_pointer_t<plain_format_arg_t<T>>>) || (std::is_array_v<std::remove_reference_t<T>> && IsTextCodeUnit<std::remove_extent_t<std::remove_reference_t<T>>>);

    template<typename T>
    inline constexpr bool IsNativeTextArg = specialization_of<plain_format_arg_t<T>, std::basic_string> || specialization_of<plain_format_arg_t<T>, std::basic_string_view>;

    template<typename T>
    inline constexpr bool IsNativeCharTextArg = is_native_char_text_arg<plain_format_arg_t<T>>::value || std::same_as<plain_format_arg_t<T>, string_view_nt>;

    template<typename T>
    inline constexpr bool IsRawCharTextArg = (std::is_pointer_v<plain_format_arg_t<T>> && !std::is_volatile_v<std::remove_pointer_t<plain_format_arg_t<T>>> && std::same_as<std::remove_cv_t<std::remove_pointer_t<plain_format_arg_t<T>>>, char>) || (std::is_array_v<std::remove_reference_t<T>> && !std::is_volatile_v<std::remove_extent_t<std::remove_reference_t<T>>> && std::same_as<std::remove_cv_t<std::remove_extent_t<std::remove_reference_t<T>>>, char>);

    template<typename T>
    inline constexpr bool IsNarrowCharTextArg = IsNativeCharTextArg<T> || IsRawCharTextArg<T>;

    template<typename T>
    inline constexpr bool IsStringProxyArg = std::same_as<plain_format_arg_t<T>, strex> || std::same_as<plain_format_arg_t<T>, strvex> || std::same_as<plain_format_arg_t<T>, u8strex> || std::same_as<plain_format_arg_t<T>, u8strvex>;

    template<typename T>
    inline constexpr bool IsUtf8StringProxyArg = std::same_as<plain_format_arg_t<T>, u8strex> || std::same_as<plain_format_arg_t<T>, u8strvex>;

    template<typename T>
    inline constexpr bool HasCharFormatter = std::is_default_constructible_v<std::formatter<plain_format_arg_t<T>, char>>;

    template<typename T>
    inline constexpr bool IsImplicitTextArg = std::convertible_to<T, string_view> || std::convertible_to<T, std::u8string_view> || std::convertible_to<T, std::wstring_view> || std::convertible_to<T, std::u16string_view> || std::convertible_to<T, std::u32string_view> || std::convertible_to<T, const char*> || std::convertible_to<T, const char8_t*> || std::convertible_to<T, const wchar_t*> || std::convertible_to<T, const char16_t*> || std::convertible_to<T, const char32_t*>;

    template<typename T>
    inline constexpr bool IsAmbiguousTextArg = !IsStrictArg<T> && (IsRawTextArg<T> || IsNativeTextArg<T> || IsImplicitTextArg<T>);

    template<typename T>
    inline constexpr bool IsUtf8FormatArg = !IsVolatileStrictArg<T> && (!IsAmbiguousTextArg<T> || IsNarrowCharTextArg<T> || IsStringProxyArg<T> || HasCharFormatter<T>);

    template<typename T>
    inline constexpr bool IsAsciiFormatArg = IsUtf8FormatArg<T> && !IsUtf8StrictArg<T> && !IsUtf8StringProxyArg<T>;

    template<typename T>
    using mapped_format_arg_t = std::conditional_t<IsUtf8StrictArg<T> || IsNarrowCharTextArg<T> || IsStringProxyArg<T>, string_view, T>;

    [[nodiscard]] auto AdaptUtf8FormatArg(u8string_view value) -> string_view;
    [[nodiscard]] auto VFormatToString(string_view format, std::format_args args) -> string;

    template<typename T>
    [[nodiscard]] constexpr decltype(auto) AdaptFormatArg(T&& value)
    {
        using value_type = plain_format_arg_t<T>;

        if constexpr (std::same_as<value_type, u8string>) {
            return AdaptUtf8FormatArg(value);
        }
        else if constexpr (std::same_as<value_type, u8string_view>) {
            return AdaptUtf8FormatArg(value);
        }
        else if constexpr (std::same_as<value_type, u8string_view_nt>) {
            return AdaptUtf8FormatArg(value.view());
        }
        else if constexpr (std::same_as<value_type, hstring>) {
            return value.as_str();
        }
        else if constexpr (std::same_as<value_type, strex> || std::same_as<value_type, strvex>) {
            return static_cast<string_view>(value);
        }
        else if constexpr (std::same_as<value_type, u8strex> || std::same_as<value_type, u8strvex>) {
            return AdaptUtf8FormatArg(static_cast<u8string_view>(value));
        }
        else if constexpr (IsNativeCharTextArg<T>) {
            return string_view {value.data(), value.size()};
        }
        else if constexpr (std::is_pointer_v<value_type> && IsRawCharTextArg<T>) {
            if (value == nullptr) {
                throw std::invalid_argument("Null UTF-8 format argument");
            }

            return string_view {value};
        }
        else if constexpr (std::is_array_v<std::remove_reference_t<T>> && IsRawCharTextArg<T>) {
            constexpr size_t extent = std::extent_v<std::remove_reference_t<T>>;
            const size_t size = extent != 0 && value[extent - 1] == '\0' ? extent - 1 : extent;
            return string_view {value, size};
        }
        else {
            return std::forward<T>(value);
        }
    }

    template<typename... Args>
    [[nodiscard]] auto FormatAdapted(string_view format, Args&&... args) -> string
    {
        std::tuple<decltype(AdaptFormatArg(std::forward<Args>(args)))...> adapted_args {AdaptFormatArg(std::forward<Args>(args))...};

        return std::apply([format](auto&... values) -> string { return VFormatToString(format, std::make_format_args(values...)); }, adapted_args);
    }
}

template<typename... Args>
class format_string final
{
public:
    template<size_t N>
        requires((text_format_detail::IsUtf8FormatArg<Args>) && ...)
    consteval format_string(const char (&literal)[N]) : // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
        _view {literal, N - 1}
    {
        for (size_t i = 0; i + 1 < N; i++) {
            if (static_cast<unsigned char>(literal[i]) > 0x7F) {
                throw "Non-ASCII format string literal";
            }
        }

        const std::format_string<text_format_detail::mapped_format_arg_t<Args>...> checked_format {literal};
        ignore_unused(checked_format);
    }

    [[nodiscard]] constexpr auto view() const noexcept -> string_view { return _view; }

private:
    string_view _view;
};

template<typename... Args>
class u8format_string final
{
public:
    template<size_t N>
        requires((text_format_detail::IsUtf8FormatArg<Args>) && ...)
    consteval u8format_string(const char8_t (&literal)[N]) : // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
        _view {literal}
    {
        std::array<char8_t, N> utf8_format {};

        for (size_t i = 0; i < N; i++) {
            utf8_format[i] = literal[i];
        }

        const std::array<char, N> char_format = std::bit_cast<std::array<char, N>>(utf8_format);
        const std::format_string<text_format_detail::mapped_format_arg_t<Args>...> checked_format {string_view {char_format.data(), N - 1}};
        ignore_unused(checked_format);
    }

    [[nodiscard]] constexpr auto view() const noexcept -> u8string_view { return _view; }

private:
    u8string_view _view;
};

template<typename... Args>
    requires((text_format_detail::IsUtf8FormatArg<Args>) && ...)
[[nodiscard]] auto FormatUtf8(format_string<std::type_identity_t<Args>...> format, Args&&... args) -> u8string
{
    const string result = text_format_detail::FormatAdapted(format.view(), std::forward<Args>(args)...);
    return utf8_from_char_span(const_span<char> {result.data(), result.size()});
}

template<typename... Args>
    requires((text_format_detail::IsUtf8FormatArg<Args>) && ...)
[[nodiscard]] auto FormatUtf8(u8format_string<std::type_identity_t<Args>...> format, Args&&... args) -> u8string
{
    const string_view format_view = text_format_detail::AdaptUtf8FormatArg(format.view());
    const string result = text_format_detail::FormatAdapted(format_view, std::forward<Args>(args)...);
    return utf8_from_char_span(const_span<char> {result.data(), result.size()});
}

FO_END_NAMESPACE
