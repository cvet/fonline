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

FO_BEGIN_NAMESPACE();

class strex final
{
public:
    static constexpr size_t MAX_NUMBER_STRING_LENGTH = 80;

    struct safe_format_tag
    {
    } static constexpr safe_format {};
    struct dynamic_format_tag
    {
    } static constexpr dynamic_format {};

    constexpr strex() noexcept = default;

    constexpr explicit strex(const char* s) noexcept :
        _sv {s}
    {
    }

    constexpr explicit strex(string_view s) noexcept :
        _sv {s}
    {
    }

    constexpr explicit strex(string&& s) noexcept :
        _s {std::move(s)},
        _sv {_s}
    {
    }

    template<typename... Args>
    explicit strex(std::format_string<Args...>&& format, Args&&... args) :
        _s {std::format(std::move(format), std::forward<Args>(args)...)},
        _sv {_s}
    {
    }

    template<typename... Args>
    explicit strex(safe_format_tag /*tag*/, std::format_string<Args...>&& format, Args&&... args) noexcept
    {
        try {
            _s = std::format(std::move(format), std::forward<Args>(args)...);
        }
        catch (const std::exception& ex) {
            BreakIntoDebugger();

            try {
                _s.append("Format error: ");
                _s.append(ex.what());
            }
            catch (...) { // NOLINT(bugprone-empty-catch)
                // Bad alloc
            }
        }
        catch (...) { // NOLINT(bugprone-empty-catch)
        }

        _sv = _s;
    }

    template<typename... Args>
    explicit strex(dynamic_format_tag /*tag*/, string_view format, Args&&... args) :
        _s {std::vformat(format, std::make_format_args(std::forward<Args>(args)...))},
        _sv {_s}
    {
    }

    strex(const strex&) = delete;
    strex(strex&&) noexcept = delete;
    auto operator=(const strex&) -> strex& = delete;
    auto operator=(strex&&) noexcept -> strex& = delete;
    constexpr ~strex() = default;

    // ReSharper disable once CppNonExplicitConversionOperator
    operator string&&() noexcept;
    // ReSharper disable once CppNonExplicitConversionOperator
    constexpr operator string_view() const noexcept { return _sv; }

    constexpr auto operator==(const strex& other) const noexcept -> bool { return _sv == other._sv; }
    constexpr auto operator==(string_view other) const noexcept -> bool { return _sv == other; }

    // ReSharper disable once CppInconsistentNaming
    [[nodiscard]] auto c_str() noexcept -> const char*;
    [[nodiscard]] auto str() noexcept -> string&&;
    [[nodiscard]] auto strv() const noexcept -> string_view { return _sv; }

    [[nodiscard]] auto length() const noexcept -> size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;
    [[nodiscard]] auto compare_ignore_case(string_view other) const noexcept -> bool;
    [[nodiscard]] auto compare_ignore_case_utf8(string_view other) const -> bool;
    [[nodiscard]] auto starts_with(char r) const noexcept -> bool;
    [[nodiscard]] auto starts_with(string_view r) const noexcept -> bool;
    [[nodiscard]] auto ends_with(char r) const noexcept -> bool;
    [[nodiscard]] auto ends_with(string_view r) const noexcept -> bool;
    [[nodiscard]] auto is_valid_utf8() const noexcept -> bool;
    [[nodiscard]] auto length_utf8() const noexcept -> size_t;

    [[nodiscard]] auto is_number() const noexcept -> bool;
    [[nodiscard]] auto is_explicit_bool() const noexcept -> bool;
    [[nodiscard]] auto to_int32() const noexcept -> int32;
    [[nodiscard]] auto to_uint32() const noexcept -> uint32;
    [[nodiscard]] auto to_int64() const noexcept -> int64;
    [[nodiscard]] auto to_float32() const noexcept -> float32;
    [[nodiscard]] auto to_float64() const noexcept -> float64;
    [[nodiscard]] auto to_bool() const noexcept -> bool;

    [[nodiscard]] auto split(char delimiter) const -> vector<string>;
    [[nodiscard]] auto split_to_int32(char delimiter) const -> vector<int32>;

    auto substring_until(char separator) noexcept -> strex&;
    auto substring_until(string_view separator) noexcept -> strex&;
    auto substring_after(char separator) noexcept -> strex&;
    auto substring_after(string_view separator) noexcept -> strex&;
    auto trim() noexcept -> strex&;
    auto trim(string_view chars) noexcept -> strex&;
    auto ltrim(string_view chars) noexcept -> strex&;
    auto rtrim(string_view chars) noexcept -> strex&;

    auto erase(char what) -> strex&;
    auto erase(char begin, char end) -> strex&;
    auto replace(char from, char to) -> strex&;
    auto replace(char from1, char from2, char to) -> strex&;
    auto replace(string_view from, string_view to) -> strex&;
    auto lower() -> strex&;
    auto lower_utf8() -> strex&;
    auto upper() -> strex&;
    auto upper_utf8() -> strex&;
    auto assignVolatile(const volatile char* str, size_t len) -> strex&;

    auto format_path() -> strex&;
    auto extract_dir() -> strex&;
    auto extract_file_name() noexcept -> strex&;
    auto get_file_extension() -> strex&; // Extension without dot and lowered
    auto erase_file_extension() noexcept -> strex&; // Erase extension with dot
    auto change_file_name(string_view new_name) -> strex&;
    auto change_file_extension(string_view new_ext) -> strex&;
    auto combine_path(string_view path) -> strex&;
    auto normalize_path_slashes() -> strex&;
    auto normalize_line_endings() -> strex&;

#if FO_WINDOWS
    auto parse_wide_char(const wchar_t* str) noexcept -> strex&;
    [[nodiscard]] auto to_wide_char() const noexcept -> wstring;
#endif

private:
    void own_storage() noexcept;

    string _s {};
    string_view _sv {};
};
static_assert(!std::is_polymorphic_v<strex>);

// ReSharper restore CppInconsistentNaming

namespace utf8
{
    auto IsValid(uint32 ucs) noexcept -> bool;
    auto DecodeStrNtLen(const char* str) noexcept -> size_t;
    auto Decode(const char* str, size_t& length) noexcept -> uint32;
    auto Encode(uint32 ucs, char (&buf)[4]) noexcept -> size_t;
    auto Lower(uint32 ucs) noexcept -> uint32;
    auto Upper(uint32 ucs) noexcept -> uint32;
}

FO_END_NAMESPACE();

template<>
struct std::formatter<FO_NAMESPACE strex> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    // ReSharper disable once CppInconsistentNaming
    auto format(const FO_NAMESPACE strex& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE string_view>::format(value.strv(), ctx);
    }
};
