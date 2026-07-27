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

#include "TextTypes.h"

FO_BEGIN_NAMESPACE

using utf16_string = std::basic_string<char16_t, std::char_traits<char16_t>, SafeAllocator<char16_t>>;

#if FO_WINDOWS
using wide_string = std::basic_string<wchar_t, std::char_traits<wchar_t>, SafeAllocator<wchar_t>>;
#endif

[[nodiscard]] auto utf8_from_char_span(const_span<char> value) -> u8string;
[[nodiscard]] auto utf8_from_byte_span(const_span<byte> value) -> u8string;
[[nodiscard]] auto string_from_byte_span(const_span<byte> value) -> string;
[[nodiscard]] auto utf8_from_terminated_char_span(const_span<char> storage) -> u8string;

[[nodiscard]] auto utf8_to_char_span(u8string_view value) noexcept -> const_span<char>;
[[nodiscard]] auto utf8_to_byte_span(u8string_view value) noexcept -> const_span<byte>;
[[nodiscard]] auto string_to_byte_span(string_view value) noexcept -> const_span<byte>;
[[nodiscard]] auto utf8_to_c_str(u8string_view_nt value) noexcept -> ptr<const char>;
[[nodiscard]] auto return_utf8_c_str(u8string_view_nt value) noexcept -> const char*;

[[nodiscard]] auto utf8_as_char_view(u8string_view value) noexcept -> string_view;
[[nodiscard]] auto utf8_to_char_string(u8string_view value) -> string;
[[nodiscard]] auto utf8_to_char_string(const u8string& value) -> string;
[[nodiscard]] auto utf8_to_string(u8string_view value) -> string;
[[nodiscard]] auto utf8_to_string(std::u8string_view value) -> string;

template<size_t N>
[[nodiscard]] auto utf8_to_string(const char8_t (&value)[N]) -> string
{
    static_assert(N > 0);

    if (value[N - 1] != char8_t {}) {
        throw TextValidationException(TextEncoding::Utf8, TextValidationError::MissingTerminator, N - 1);
    }

    return utf8_to_string(std::u8string_view {value, N - 1});
}

template<typename T>
    requires(std::same_as<std::remove_cvref_t<T>, char8_t*> || std::same_as<std::remove_cvref_t<T>, const char8_t*>)
auto utf8_to_string(T&& value) -> string = delete;

[[nodiscard]] auto validate_utf16_text(std::u16string_view value) noexcept -> optional<TextValidationIssue>;
auto validate_utf16_text(const char16_t* value) -> optional<TextValidationIssue> = delete;
[[nodiscard]] auto utf8_to_utf16(u8string_view value) -> utf16_string;
[[nodiscard]] auto utf16_to_utf8(std::u16string_view value) -> u8string;
auto utf16_to_utf8(const char16_t* value) -> u8string = delete;

#if FO_WINDOWS
[[nodiscard]] auto utf16_to_wide(std::u16string_view value) -> wide_string;
[[nodiscard]] auto wide_to_utf16(std::wstring_view value) -> utf16_string;
[[nodiscard]] auto string_to_wide_string(string_view_nt value) -> wide_string;
[[nodiscard]] auto utf8_to_wide_string(u8string_view value) -> wide_string;
#endif

FO_END_NAMESPACE
