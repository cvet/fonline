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

#include "BasicCore.h"
#include "Containers.h"
#include "SmartPointers.h"
#include "TextConversions.h"
#include "TextFormatting.h"
#include "TextTypes.h"

FO_BEGIN_NAMESPACE

[[nodiscard]] auto is_uri_scheme_letter(char value) noexcept -> bool;
[[nodiscard]] auto is_uri_scheme_tail_character(char value) noexcept -> bool;
[[nodiscard]] auto parse_uri_scheme(string_view value) noexcept -> optional<string_view>;

class strvex
{
public:
    constexpr strvex() noexcept = default;

    constexpr explicit strvex(const char* s) noexcept :
        _sv {s}
    {
    }

    constexpr explicit strvex(string_view s) noexcept :
        _sv {s}
    {
    }

    constexpr explicit strvex(const string& s) noexcept :
        _sv {s}
    {
    }
    strvex(string&& s) = delete;
    strvex(const string&& s) = delete;

    template<typename Traits, typename Allocator>
    constexpr explicit strvex(const std::basic_string<char, Traits, Allocator>& s) noexcept :
        _sv {s.data(), s.size()}
    {
    }
    template<typename Traits, typename Allocator>
    strvex(std::basic_string<char, Traits, Allocator>&& s) = delete;
    template<typename Traits, typename Allocator>
    strvex(const std::basic_string<char, Traits, Allocator>&& s) = delete;

    strvex(const strvex&) = delete;
    strvex(strvex&&) noexcept = delete;
    auto operator=(const strvex&) -> strvex& = delete;
    auto operator=(strvex&&) noexcept -> strvex& = delete;
    constexpr ~strvex() = default;

    // ReSharper disable once CppNonExplicitConversionOperator
    constexpr operator string_view() const noexcept { return _sv; }

    constexpr auto operator==(const strvex& other) const noexcept -> bool { return _sv == other._sv; }
    constexpr auto operator==(string_view other) const noexcept -> bool { return _sv == other; }

    [[nodiscard]] auto strv() const noexcept -> string_view { return _sv; }
    [[nodiscard]] auto str() const noexcept -> string { return string(_sv); }

    [[nodiscard]] auto length() const noexcept -> size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;
    [[nodiscard]] auto compare_ignore_case(string_view other) const noexcept -> bool;
    [[nodiscard]] auto starts_with(char r) const noexcept -> bool;
    [[nodiscard]] auto starts_with(string_view r) const noexcept -> bool;
    [[nodiscard]] auto ends_with(char r) const noexcept -> bool;
    [[nodiscard]] auto ends_with(string_view r) const noexcept -> bool;

    [[nodiscard]] auto is_number() const noexcept -> bool;
    [[nodiscard]] auto is_non_finite_number() const noexcept -> bool;
    [[nodiscard]] auto is_explicit_bool() const noexcept -> bool;
    [[nodiscard]] auto to_int32() const noexcept -> int32_t;
    [[nodiscard]] auto to_uint32() const noexcept -> uint32_t;
    [[nodiscard]] auto to_int64() const noexcept -> int64_t;
    [[nodiscard]] auto to_float32() const noexcept -> float32_t;
    [[nodiscard]] auto to_float64() const noexcept -> float64_t;
    [[nodiscard]] auto to_bool() const noexcept -> bool;

    [[nodiscard]] auto split(char delimiter) const noexcept -> vector<string_view>;
    [[nodiscard]] auto split_to_int32(char delimiter) const noexcept -> vector<int32_t>;
    [[nodiscard]] auto tokenize() const noexcept -> vector<string_view>;

    auto substring_until(char separator) noexcept -> strvex&;
    auto substring_until(string_view separator) noexcept -> strvex&;
    auto substring_after(char separator) noexcept -> strvex&;
    auto substring_after(string_view separator) noexcept -> strvex&;
    auto trim() noexcept -> strvex&;
    auto trim(string_view chars) noexcept -> strvex&;
    auto ltrim(string_view chars) noexcept -> strvex&;
    auto rtrim(string_view chars) noexcept -> strvex&;

    auto extract_file_name() noexcept -> strvex&;
    auto erase_file_extension() noexcept -> strvex&; // Erase extension with dot

protected:
    string_view _sv {};
};
static_assert(!std::is_polymorphic_v<strvex>);

class strex : protected strvex
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
        strvex(s)
    {
    }

    constexpr explicit strex(string_view s) noexcept :
        strvex(s)
    {
    }

    constexpr explicit strex(const string& s) noexcept :
        strvex(s)
    {
    }

    template<typename Traits, typename Allocator>
    constexpr explicit strex(const std::basic_string<char, Traits, Allocator>& s) noexcept :
        strvex(s)
    {
    }

    constexpr explicit strex(string&& s) noexcept :
        strvex(),
        _s {std::move(s)}
    {
        _sv = _s;
    }

    // Formatting writes straight into the engine-allocated buffer. Do not switch these to std::format /
    // std::vformat: those return a std::string built with std::allocator, which both bypasses the
    // SafeAllocator out-of-memory contract and costs an extra copy into _s on every formatted call.
    template<typename... Args>
        requires((text_format_detail::IsAsciiFormatArg<Args>) && ...)
    explicit strex(format_string<std::type_identity_t<Args>...> format, Args&&... args) :
        strvex(),
        _s {text_format_detail::FormatAdapted(format.view(), std::forward<Args>(args)...)}
    {
        if (const auto issue = validate_ascii_text(_s)) {
            throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
        }

        _sv = _s;
    }

    template<typename... Args>
    explicit strex(safe_format_tag /*tag*/, std::format_string<Args...>&& format, Args&&... args) noexcept :
        strvex()
    {
        try {
            (void)std::format_to(std::back_inserter(_s), std::move(format), std::forward<Args>(args)...);
        }
        catch (const std::exception& ex) {
            BreakIntoDebugger();

            try {
                // Formatting appends incrementally, so drop whatever partial output was produced
                _s.clear();
                _s.append("Format error: ");
                _s.append(ex.what());
            }
            catch (...) { // NOLINT(bugprone-empty-catch)
                // Bad alloc
            }
        }
        catch (...) {
            _s.clear();
        }

        if (validate_ascii_text(_s)) {
            BreakIntoDebugger();
            _s = "Format error: non-ASCII result";
        }

        _sv = _s;
    }

    // std::make_format_args binds its arguments as non-const lvalue references, so the pack is passed by
    // name rather than forwarded; forwarding turns a caller's temporary into an xvalue that cannot bind
    template<typename... Args>
    explicit strex(dynamic_format_tag /*tag*/, string_view format, Args&&... args) :
        strvex()
    {
        (void)std::vformat_to(std::back_inserter(_s), format, std::make_format_args(args...));

        if (const auto issue = validate_ascii_text(_s)) {
            throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
        }

        _sv = _s;
    }

    strex(const strex&) = delete;
    strex(strex&&) noexcept = delete;
    auto operator=(const strex&) -> strex& = delete;
    auto operator=(strex&&) noexcept -> strex& = delete;
    constexpr ~strex() = default;

    // ReSharper disable once CppNonExplicitConversionOperator
    operator string&&();
    // ReSharper disable once CppNonExplicitConversionOperator
    operator string() const;
    // ReSharper disable once CppNonExplicitConversionOperator
    operator u8string() const;
    // ReSharper disable once CppNonExplicitConversionOperator
    constexpr operator string_view() const noexcept { return _sv; }

    constexpr auto operator==(const strex& other) const noexcept -> bool { return _sv == other._sv; }
    constexpr auto operator==(string_view other) const noexcept -> bool { return _sv == other; }

    // ReSharper disable once CppInconsistentNaming
    [[nodiscard]] auto c_str() noexcept -> const char*;
    [[nodiscard]] auto str() noexcept -> string&&;
    [[nodiscard]] auto strv() const noexcept -> string_view { return strvex::strv(); }

    [[nodiscard]] auto length() const noexcept -> size_t { return strvex::length(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return strvex::empty(); }
    [[nodiscard]] auto compare_ignore_case(string_view other) const noexcept -> bool { return strvex::compare_ignore_case(other); }
    [[nodiscard]] auto starts_with(char r) const noexcept -> bool { return strvex::starts_with(r); }
    [[nodiscard]] auto starts_with(string_view r) const noexcept -> bool { return strvex::starts_with(r); }
    [[nodiscard]] auto ends_with(char r) const noexcept -> bool { return strvex::ends_with(r); }
    [[nodiscard]] auto ends_with(string_view r) const noexcept -> bool { return strvex::ends_with(r); }

    [[nodiscard]] auto is_number() const noexcept -> bool { return strvex::is_number(); }
    [[nodiscard]] auto is_non_finite_number() const noexcept -> bool { return strvex::is_non_finite_number(); }
    [[nodiscard]] auto is_explicit_bool() const noexcept -> bool { return strvex::is_explicit_bool(); }
    [[nodiscard]] auto to_int32() const noexcept -> int32_t { return strvex::to_int32(); }
    [[nodiscard]] auto to_uint32() const noexcept -> uint32_t { return strvex::to_uint32(); }
    [[nodiscard]] auto to_int64() const noexcept -> int64_t { return strvex::to_int64(); }
    [[nodiscard]] auto to_float32() const noexcept -> float32_t { return strvex::to_float32(); }
    [[nodiscard]] auto to_float64() const noexcept -> float64_t { return strvex::to_float64(); }
    [[nodiscard]] auto to_bool() const noexcept -> bool { return strvex::to_bool(); }

    [[nodiscard]] auto split(char delimiter) const noexcept -> vector<string>;
    [[nodiscard]] auto split_to_int32(char delimiter) const noexcept -> vector<int32_t> { return strvex::split_to_int32(delimiter); }
    [[nodiscard]] auto tokenize() const noexcept -> vector<string>;

    auto substring_until(char separator) noexcept -> strex&;
    auto substring_until(string_view separator) noexcept -> strex&;
    auto substring_after(char separator) noexcept -> strex&;
    auto substring_after(string_view separator) noexcept -> strex&;
    auto trim() noexcept -> strex&;
    auto trim(string_view chars) noexcept -> strex&;
    auto ltrim(string_view chars) noexcept -> strex&;
    auto rtrim(string_view chars) noexcept -> strex&;

    auto erase(char what) noexcept -> strex&;
    auto erase(char begin, char end) noexcept -> strex&;
    auto replace(char from, char to) noexcept -> strex&;
    auto replace(char from1, char from2, char to) noexcept -> strex&;
    auto replace(string_view from, string_view to) noexcept -> strex&;
    auto lower() noexcept -> strex&;
    auto upper() noexcept -> strex&;
    auto assignVolatile(const volatile char* str, size_t len) noexcept -> strex&;
    auto join(const_span<string_view> parts) noexcept -> strex&;
    auto join(const_span<string> parts) noexcept -> strex&;

    auto format_path() noexcept -> strex&;
    auto extract_dir() noexcept -> strex&;
    auto extract_file_name() noexcept -> strex&;
    auto get_file_extension() noexcept -> strex&; // Extension without dot and lowered
    auto erase_file_extension() noexcept -> strex&; // Erase extension with dot
    auto change_file_name(string_view new_name) -> strex&;
    auto change_file_extension(string_view new_ext) noexcept -> strex&;
    auto combine_path(string_view path) noexcept -> strex&;
    auto normalize_path_slashes() noexcept -> strex&;
    auto normalize_line_endings() noexcept -> strex&;

#if FO_WINDOWS
    [[nodiscard]] auto to_wide_char() const noexcept -> wstring;
#endif

private:
    void own_storage() noexcept;

    string _s {};
};
static_assert(!std::is_polymorphic_v<strex>);

class u8strvex
{
public:
    constexpr u8strvex() noexcept = default;
    constexpr explicit u8strvex(u8string_view s) noexcept :
        _sv {s}
    {
    }
    constexpr explicit u8strvex(const u8string& s) noexcept :
        _sv {s}
    {
    }
    u8strvex(u8string&& s) = delete;
    u8strvex(const u8string&& s) = delete;

    u8strvex(const u8strvex&) = delete;
    u8strvex(u8strvex&&) noexcept = delete;
    auto operator=(const u8strvex&) -> u8strvex& = delete;
    auto operator=(u8strvex&&) noexcept -> u8strvex& = delete;
    constexpr ~u8strvex() = default;

    constexpr operator u8string_view() const noexcept { return _sv; } // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    constexpr auto operator==(const u8strvex& other) const noexcept -> bool { return _sv == other._sv; }
    constexpr auto operator==(u8string_view other) const noexcept -> bool { return _sv == other; }
    auto operator==(const u8string& other) const noexcept -> bool { return _sv == other; }
    template<size_t N>
    constexpr auto operator==(const char8_t (&literal)[N]) const noexcept -> bool
    {
        return _sv == literal;
    }

    [[nodiscard]] auto strv() const noexcept -> u8string_view { return _sv; }
    [[nodiscard]] auto str() const -> u8string { return u8string {_sv}; }

    [[nodiscard]] auto length() const noexcept -> size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;
    [[nodiscard]] auto compare_ignore_case(u8string_view other) const noexcept -> bool;
    [[nodiscard]] auto starts_with(char8_t value) const noexcept -> bool;
    [[nodiscard]] auto starts_with(u8string_view value) const noexcept -> bool;
    [[nodiscard]] auto ends_with(char8_t value) const noexcept -> bool;
    [[nodiscard]] auto ends_with(u8string_view value) const noexcept -> bool;
    [[nodiscard]] auto length_utf8() const noexcept -> size_t;

    [[nodiscard]] auto is_number() const -> bool;
    [[nodiscard]] auto is_non_finite_number() const -> bool;
    [[nodiscard]] auto is_explicit_bool() const -> bool;
    [[nodiscard]] auto to_int32() const -> int32_t;
    [[nodiscard]] auto to_uint32() const -> uint32_t;
    [[nodiscard]] auto to_int64() const -> int64_t;
    [[nodiscard]] auto to_float32() const -> float32_t;
    [[nodiscard]] auto to_float64() const -> float64_t;
    [[nodiscard]] auto to_bool() const -> bool;

    [[nodiscard]] auto split(char8_t delimiter) const -> vector<u8string_view>;
    [[nodiscard]] auto tokenize() const noexcept -> vector<u8string_view>;

    auto substring_until(char8_t separator) -> u8strvex&;
    auto substring_until(u8string_view separator) -> u8strvex&;
    auto substring_after(char8_t separator) -> u8strvex&;
    auto substring_after(u8string_view separator) -> u8strvex&;
    auto trim() -> u8strvex&;
    auto trim(u8string_view chars) -> u8strvex&;
    auto ltrim(u8string_view chars) -> u8strvex&;
    auto rtrim(u8string_view chars) -> u8strvex&;

    auto extract_file_name() -> u8strvex&;
    auto erase_file_extension() -> u8strvex&;

protected:
    u8string_view _sv {};
};
static_assert(!std::is_polymorphic_v<u8strvex>);

class u8strex : protected u8strvex
{
public:
    constexpr u8strex() noexcept = default;
    constexpr explicit u8strex(u8string_view s) noexcept :
        u8strvex {s}
    {
    }
    constexpr explicit u8strex(const u8string& s) noexcept :
        u8strvex {s}
    {
    }
    explicit u8strex(string_view s) :
        u8strvex {},
        _s {s}
    {
        _sv = _s;
    }
    template<size_t N>
    explicit u8strex(const char (&literal)[N]) :
        u8strex {string_view {literal, N - 1}}
    {
        static_assert(N > 0);
    }
    explicit u8strex(u8string&& s) noexcept :
        u8strvex {},
        _s {std::move(s)}
    {
        _sv = _s;
    }

    template<typename... Args>
        requires((text_format_detail::IsUtf8FormatArg<Args>) && ...)
    explicit u8strex(format_string<std::type_identity_t<Args>...> format, Args&&... args) :
        u8strvex {},
        _s {FormatUtf8(format, std::forward<Args>(args)...)}
    {
        _sv = _s;
    }

    template<typename... Args>
        requires((text_format_detail::IsUtf8FormatArg<Args>) && ...)
    explicit u8strex(u8format_string<std::type_identity_t<Args>...> format, Args&&... args) :
        u8strvex {},
        _s {FormatUtf8(format, std::forward<Args>(args)...)}
    {
        _sv = _s;
    }

    u8strex(const u8strex&) = delete;
    u8strex(u8strex&&) noexcept = delete;
    auto operator=(const u8strex&) -> u8strex& = delete;
    auto operator=(u8strex&&) noexcept -> u8strex& = delete;
    constexpr ~u8strex() = default;

    operator u8string&&();
    constexpr operator u8string_view() const noexcept { return _sv; } // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    constexpr auto operator==(const u8strex& other) const noexcept -> bool { return _sv == other._sv; }
    constexpr auto operator==(u8string_view other) const noexcept -> bool { return _sv == other; }
    auto operator==(const u8string& other) const noexcept -> bool { return _sv == other; }
    template<size_t N>
    constexpr auto operator==(const char8_t (&literal)[N]) const noexcept -> bool
    {
        return _sv == literal;
    }

    [[nodiscard]] auto str() -> u8string&&;
    [[nodiscard]] auto strv() const noexcept -> u8string_view { return u8strvex::strv(); }

    [[nodiscard]] auto length() const noexcept -> size_t { return u8strvex::length(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return u8strvex::empty(); }
    [[nodiscard]] auto compare_ignore_case(u8string_view other) const noexcept -> bool { return u8strvex::compare_ignore_case(other); }
    [[nodiscard]] auto starts_with(char8_t value) const noexcept -> bool { return u8strvex::starts_with(value); }
    [[nodiscard]] auto starts_with(u8string_view value) const noexcept -> bool { return u8strvex::starts_with(value); }
    [[nodiscard]] auto ends_with(char8_t value) const noexcept -> bool { return u8strvex::ends_with(value); }
    [[nodiscard]] auto ends_with(u8string_view value) const noexcept -> bool { return u8strvex::ends_with(value); }
    [[nodiscard]] auto length_utf8() const noexcept -> size_t { return u8strvex::length_utf8(); }

    [[nodiscard]] auto is_number() const -> bool { return u8strvex::is_number(); }
    [[nodiscard]] auto is_non_finite_number() const -> bool { return u8strvex::is_non_finite_number(); }
    [[nodiscard]] auto is_explicit_bool() const -> bool { return u8strvex::is_explicit_bool(); }
    [[nodiscard]] auto to_int32() const -> int32_t { return u8strvex::to_int32(); }
    [[nodiscard]] auto to_uint32() const -> uint32_t { return u8strvex::to_uint32(); }
    [[nodiscard]] auto to_int64() const -> int64_t { return u8strvex::to_int64(); }
    [[nodiscard]] auto to_float32() const -> float32_t { return u8strvex::to_float32(); }
    [[nodiscard]] auto to_float64() const -> float64_t { return u8strvex::to_float64(); }
    [[nodiscard]] auto to_bool() const -> bool { return u8strvex::to_bool(); }

    [[nodiscard]] auto split(char8_t delimiter) const -> vector<u8string>;
    [[nodiscard]] auto tokenize() const -> vector<u8string>;

    auto substring_until(char8_t separator) -> u8strex&;
    auto substring_until(u8string_view separator) -> u8strex&;
    auto substring_after(char8_t separator) -> u8strex&;
    auto substring_after(u8string_view separator) -> u8strex&;
    auto trim() -> u8strex&;
    auto trim(u8string_view chars) -> u8strex&;
    auto ltrim(u8string_view chars) -> u8strex&;
    auto rtrim(u8string_view chars) -> u8strex&;

    auto erase(char8_t what) -> u8strex&;
    auto erase(char8_t begin, char8_t end) -> u8strex&;
    auto erase_ascii_control_chars() -> u8strex&;
    auto replace(char8_t from, char8_t to) -> u8strex&;
    auto replace(char8_t from1, char8_t from2, char8_t to) -> u8strex&;
    auto replace(u8string_view from, u8string_view to) -> u8strex&;
    auto lower() -> u8strex&;
    auto upper() -> u8strex&;

    auto format_path() -> u8strex&;
    auto extract_dir() -> u8strex&;
    auto extract_file_name() -> u8strex&;
    auto get_file_extension() -> u8strex&;
    auto erase_file_extension() -> u8strex&;
    auto change_file_name(string_view new_name) -> u8strex&;
    auto change_file_name(u8string_view new_name) -> u8strex&;
    auto change_file_extension(string_view new_ext) -> u8strex&;
    auto change_file_extension(u8string_view new_ext) -> u8strex&;
    auto combine_path(string_view path) -> u8strex&;
    auto combine_path(u8string_view path) -> u8strex&;
    auto normalize_path_slashes() -> u8strex&;
    auto normalize_line_endings() -> u8strex&;

#if FO_WINDOWS
    auto parse_wide_char(ptr<const wchar_t> str) -> u8strex&;
    [[nodiscard]] auto to_wide_char() const -> wstring;
#endif

private:
    void own_storage();
    template<typename Mutator>
    void mutate_storage(Mutator&& mutator)
    {
        using storage_type = std::basic_string<char8_t, std::char_traits<char8_t>, SafeAllocator<char8_t>>;
        storage_type storage {_sv.native_view()};
        std::forward<Mutator>(mutator)(storage);
        _s = u8string::FromChecked(storage);
        _sv = _s;
    }

    u8string _s {};
};
static_assert(!std::is_polymorphic_v<u8strex>);

[[nodiscard]] inline auto utf8_to_string(const u8strex& value) -> string
{
    return utf8_to_string(static_cast<u8string_view>(value));
}

// ReSharper restore CppInconsistentNaming

namespace utf8
{
    [[nodiscard]] auto IsValid(uint32_t ucs) noexcept -> bool;
    [[nodiscard]] auto DecodeStrNtLen(ptr<const char> str) noexcept -> size_t;
    [[nodiscard]] auto Decode(ptr<const char> str, size_t& length) noexcept -> optional<uint32_t>;
    [[nodiscard]] auto Decode(ptr<const char8_t> str, size_t& length) noexcept -> optional<uint32_t>;
    [[nodiscard]] auto Encode(uint32_t ucs, char (&buf)[4]) noexcept -> optional<size_t>;
    [[nodiscard]] auto Encode(uint32_t ucs, char8_t (&buf)[4]) noexcept -> optional<size_t>;
    auto Lower(uint32_t ucs) noexcept -> uint32_t;
    auto Upper(uint32_t ucs) noexcept -> uint32_t;
}

FO_END_NAMESPACE

template<>
struct std::formatter<FO_NAMESPACE strvex> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    // ReSharper disable once CppInconsistentNaming
    auto format(const FO_NAMESPACE strvex& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE string_view>::format(value.strv(), ctx);
    }
};

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
