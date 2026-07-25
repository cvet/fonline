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

#include "MemorySystem.h"

FO_BEGIN_NAMESPACE

using string = std::basic_string<char, std::char_traits<char>, SafeAllocator<char>>;

enum class TextEncoding : uint8_t
{
    Ascii,
    Utf8,
    Utf16,
};

enum class TextValidationError : uint8_t
{
    NonAsciiByte,
    InvalidLeadByte,
    InvalidContinuationByte,
    TruncatedSequence,
    OverlongSequence,
    SurrogateScalar,
    ScalarOutOfRange,
    UnpairedHighSurrogate,
    UnpairedLowSurrogate,
    EmbeddedNull,
    MissingTerminator,
};

struct TextValidationIssue
{
    TextValidationError Error {};
    size_t Offset {};

    constexpr auto operator==(const TextValidationIssue&) const noexcept -> bool = default;
};

class TextValidationException final : public std::exception
{
public:
    TextValidationException(TextEncoding encoding, TextValidationError error, size_t offset) noexcept;

    [[nodiscard]] constexpr auto encoding() const noexcept -> TextEncoding { return _encoding; }
    [[nodiscard]] constexpr auto error() const noexcept -> TextValidationError { return _error; }
    [[nodiscard]] constexpr auto offset() const noexcept -> size_t { return _offset; }
    [[nodiscard]] auto what() const noexcept -> const char* override;

private:
    TextEncoding _encoding;
    TextValidationError _error;
    size_t _offset;
};

namespace text_detail
{
    [[nodiscard]] constexpr auto ToByte(char value) noexcept -> uint8_t
    {
        return static_cast<uint8_t>(static_cast<unsigned char>(value));
    }

    [[nodiscard]] constexpr auto ToByte(char8_t value) noexcept -> uint8_t
    {
        return static_cast<uint8_t>(value);
    }

    [[nodiscard]] constexpr auto ToByte(byte value) noexcept -> uint8_t
    {
        return std::to_integer<uint8_t>(value);
    }

    template<typename T>
    [[nodiscard]] constexpr auto ValidateAscii(const_span<T> value) noexcept -> optional<TextValidationIssue>
    {
        for (size_t i = 0; i < value.size(); i++) {
            if (ToByte(value[i]) > 0x7F) {
                return TextValidationIssue {TextValidationError::NonAsciiByte, i};
            }
        }

        return std::nullopt;
    }

    template<typename T>
    [[nodiscard]] constexpr auto ValidateUtf8(const_span<T> value) noexcept -> optional<TextValidationIssue>
    {
        size_t pos = 0;

        while (pos < value.size()) {
            const uint8_t lead = ToByte(value[pos]);

            if (lead < 0x80) {
                pos++;
                continue;
            }

            size_t sequence_size = 0;

            if (lead >= 0xC2 && lead <= 0xDF) {
                sequence_size = 2;
            }
            else if (lead >= 0xE0 && lead <= 0xEF) {
                sequence_size = 3;
            }
            else if (lead >= 0xF0 && lead <= 0xF4) {
                sequence_size = 4;
            }
            else if (lead == 0xC0 || lead == 0xC1) {
                return TextValidationIssue {TextValidationError::OverlongSequence, pos};
            }
            else if (lead >= 0xF5) {
                return TextValidationIssue {TextValidationError::ScalarOutOfRange, pos};
            }
            else {
                return TextValidationIssue {TextValidationError::InvalidLeadByte, pos};
            }

            for (size_t i = 1; i < sequence_size; i++) {
                if (pos + i >= value.size()) {
                    return TextValidationIssue {TextValidationError::TruncatedSequence, pos};
                }

                const uint8_t continuation = ToByte(value[pos + i]);

                if ((continuation & 0xC0) != 0x80) {
                    return TextValidationIssue {TextValidationError::InvalidContinuationByte, pos + i};
                }

                if (i == 1) {
                    if (lead == 0xE0 && continuation < 0xA0) {
                        return TextValidationIssue {TextValidationError::OverlongSequence, pos};
                    }
                    if (lead == 0xED && continuation >= 0xA0) {
                        return TextValidationIssue {TextValidationError::SurrogateScalar, pos};
                    }
                    if (lead == 0xF0 && continuation < 0x90) {
                        return TextValidationIssue {TextValidationError::OverlongSequence, pos};
                    }
                    if (lead == 0xF4 && continuation > 0x8F) {
                        return TextValidationIssue {TextValidationError::ScalarOutOfRange, pos};
                    }
                }
            }

            pos += sequence_size;
        }

        return std::nullopt;
    }
}

[[nodiscard]] constexpr auto validate_ascii_text(string_view value) noexcept -> optional<TextValidationIssue>
{
    return text_detail::ValidateAscii(const_span<char> {value.data(), value.size()});
}

[[nodiscard]] constexpr auto validate_ascii_text(std::u8string_view value) noexcept -> optional<TextValidationIssue>
{
    return text_detail::ValidateAscii(const_span<char8_t> {value.data(), value.size()});
}

[[nodiscard]] constexpr auto validate_ascii_text(const_span<byte> value) noexcept -> optional<TextValidationIssue>
{
    return text_detail::ValidateAscii(value);
}

auto validate_ascii_text(const char* value) -> optional<TextValidationIssue> = delete;
auto validate_ascii_text(const char8_t* value) -> optional<TextValidationIssue> = delete;

[[nodiscard]] constexpr auto validate_utf8_text(string_view value) noexcept -> optional<TextValidationIssue>
{
    return text_detail::ValidateUtf8(const_span<char> {value.data(), value.size()});
}

[[nodiscard]] constexpr auto validate_utf8_text(std::u8string_view value) noexcept -> optional<TextValidationIssue>
{
    return text_detail::ValidateUtf8(const_span<char8_t> {value.data(), value.size()});
}

[[nodiscard]] constexpr auto validate_utf8_text(const_span<byte> value) noexcept -> optional<TextValidationIssue>
{
    return text_detail::ValidateUtf8(value);
}

auto validate_utf8_text(const char* value) -> optional<TextValidationIssue> = delete;
auto validate_utf8_text(const char8_t* value) -> optional<TextValidationIssue> = delete;

[[nodiscard]] auto try_string_view_nt_from_span(const_span<char> storage) noexcept -> optional<string_view_nt>;
[[nodiscard]] auto string_view_nt_from_span(const_span<char> storage) -> string_view_nt;

class u8string;
class u8string_view_nt;

class u8string_view final
{
public:
    using value_type = char8_t;
    using native_view_type = std::u8string_view;

    constexpr u8string_view() noexcept = default;

    template<size_t N>
    consteval u8string_view(const char8_t (&literal)[N]) : // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
        _view {literal, N - 1}
    {
        static_assert(N > 0);

        if (literal[N - 1] != 0 || validate_utf8_text(_view)) {
            throw "Invalid UTF-8 string literal";
        }
    }

    [[nodiscard]] static constexpr auto TryFrom(native_view_type value) noexcept -> optional<u8string_view>
    {
        if (validate_utf8_text(value)) {
            return std::nullopt;
        }

        return u8string_view {value, ValidatedTag {}};
    }

    template<typename Traits, typename Allocator>
    [[nodiscard]] static constexpr auto TryFrom(const std::basic_string<char8_t, Traits, Allocator>& value) noexcept -> optional<u8string_view>
    {
        return TryFrom(native_view_type {value.data(), value.size()});
    }
    template<typename Traits, typename Allocator>
    static auto TryFrom(std::basic_string<char8_t, Traits, Allocator>&& value) -> optional<u8string_view> = delete;
    template<typename Traits, typename Allocator>
    static auto TryFrom(const std::basic_string<char8_t, Traits, Allocator>&& value) -> optional<u8string_view> = delete;
    template<typename T>
        requires(!std::same_as<std::remove_cvref_t<T>, native_view_type> && !specialization_of<std::remove_cvref_t<T>, std::basic_string>)
    static auto TryFrom(T&& value) -> optional<u8string_view> = delete;

    [[nodiscard]] static auto FromChecked(native_view_type value) -> u8string_view;
    template<typename Traits, typename Allocator>
    [[nodiscard]] static auto FromChecked(const std::basic_string<char8_t, Traits, Allocator>& value) -> u8string_view
    {
        return FromChecked(native_view_type {value.data(), value.size()});
    }
    template<typename Traits, typename Allocator>
    static auto FromChecked(std::basic_string<char8_t, Traits, Allocator>&& value) -> u8string_view = delete;
    template<typename Traits, typename Allocator>
    static auto FromChecked(const std::basic_string<char8_t, Traits, Allocator>&& value) -> u8string_view = delete;
    template<typename T>
        requires(!std::same_as<std::remove_cvref_t<T>, native_view_type> && !specialization_of<std::remove_cvref_t<T>, std::basic_string>)
    static auto FromChecked(T&& value) -> u8string_view = delete;

    [[nodiscard]] constexpr auto size() const noexcept -> size_t { return _view.size(); }
    [[nodiscard]] constexpr auto empty() const noexcept -> bool { return _view.empty(); }
    [[nodiscard]] constexpr auto data() const noexcept -> const value_type* { return _view.data(); }
    [[nodiscard]] constexpr auto native_view() const noexcept -> native_view_type { return _view; }

    constexpr auto operator==(const u8string_view&) const noexcept -> bool = default;
    constexpr auto operator<=>(const u8string_view&) const noexcept = default;
    template<size_t N>
    [[nodiscard]] constexpr auto operator==(const char8_t (&literal)[N]) const noexcept -> bool
    {
        static_assert(N > 0);

        const native_view_type literal_view {literal, N - 1};
        return literal[N - 1] == 0 && !validate_utf8_text(literal_view) && _view == literal_view;
    }

    template<size_t N>
    friend constexpr auto operator==(const char8_t (&literal)[N], u8string_view value) noexcept -> bool
    {
        return value == literal;
    }

private:
    struct ValidatedTag
    {
    };

    constexpr u8string_view(native_view_type value, ValidatedTag) noexcept :
        _view {value}
    {
    }

    native_view_type _view {};

    friend class u8string;
    friend class u8string_view_nt;
};

class u8string_view_nt final
{
public:
    u8string_view_nt() = delete;

    template<size_t N>
    consteval explicit u8string_view_nt(const char8_t (&literal)[N]) :
        _view {literal}
    {
        for (size_t i = 0; i + 1 < N; i++) {
            if (literal[i] == 0) {
                throw "UTF-8 NT string literal contains an embedded null";
            }
        }
    }

    [[nodiscard]] static auto TryFrom(const const_span<char8_t>& storage) noexcept -> optional<u8string_view_nt>;
    static auto TryFrom(const_span<char8_t>&& storage) -> optional<u8string_view_nt> = delete;
    static auto TryFrom(const const_span<char8_t>&& storage) -> optional<u8string_view_nt> = delete;
    template<typename T>
        requires(!std::same_as<std::remove_cvref_t<T>, const_span<char8_t>>)
    static auto TryFrom(T&& storage) -> optional<u8string_view_nt> = delete;
    [[nodiscard]] static auto FromChecked(const const_span<char8_t>& storage) -> u8string_view_nt;
    static auto FromChecked(const_span<char8_t>&& storage) -> u8string_view_nt = delete;
    static auto FromChecked(const const_span<char8_t>&& storage) -> u8string_view_nt = delete;
    template<typename T>
        requires(!std::same_as<std::remove_cvref_t<T>, const_span<char8_t>>)
    static auto FromChecked(T&& storage) -> u8string_view_nt = delete;

    [[nodiscard]] constexpr auto view() const noexcept -> u8string_view { return _view; }
    [[nodiscard]] constexpr auto c_str() const noexcept -> const char8_t* { return _view.data(); }
    [[nodiscard]] constexpr auto size() const noexcept -> size_t { return _view.size(); }

private:
    struct ValidatedTag
    {
    };

    constexpr u8string_view_nt(u8string_view value, ValidatedTag) noexcept :
        _view {value}
    {
    }

    u8string_view _view;

    friend class u8string;
};

class u8string final
{
public:
    u8string() noexcept = default;

    template<size_t N>
    explicit u8string(const char8_t (&literal)[N]) :
        u8string {CheckedLiteral(literal)}
    {
    }

    explicit u8string(u8string_view value);
    u8string(string_view value); // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
    template<typename Traits, typename Allocator>
    u8string(const std::basic_string<char, Traits, Allocator>& value) : // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
        u8string {string_view {value.data(), value.size()}}
    {
    }
    u8string(const u8string&) = default;
    u8string(u8string&& other) noexcept;
    auto operator=(const u8string&) -> u8string& = default;
    auto operator=(u8string&& other) noexcept -> u8string&;
    template<size_t N>
    auto operator=(const char (&literal)[N]) -> u8string&
    {
        return assign(CheckedAsciiLiteral(literal));
    }
    ~u8string() = default;

    [[nodiscard]] static auto FromChecked(std::u8string_view value) -> u8string;
    template<typename Traits, typename Allocator>
    [[nodiscard]] static auto FromChecked(const std::basic_string<char8_t, Traits, Allocator>& value) -> u8string
    {
        return FromChecked(std::u8string_view {value.data(), value.size()});
    }
    template<typename T>
        requires(!std::same_as<std::remove_cvref_t<T>, std::u8string_view> && !specialization_of<std::remove_cvref_t<T>, std::basic_string>)
    static auto FromChecked(T&& value) -> u8string = delete;

    [[nodiscard]] auto size() const noexcept -> size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;
    operator u8string_view() const& noexcept { return view(); } // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
    operator u8string_view() const&& = delete;
    [[nodiscard]] auto view() const& noexcept -> u8string_view;
    auto view() const&& -> u8string_view = delete;
    [[nodiscard]] auto try_view_nt() const& noexcept -> optional<u8string_view_nt>;
    auto try_view_nt() const&& -> optional<u8string_view_nt> = delete;
    [[nodiscard]] auto view_nt() const& -> u8string_view_nt;
    auto view_nt() const&& -> u8string_view_nt = delete;

    void clear() noexcept;
    void reserve(size_t capacity);
    auto assign(u8string_view value) -> u8string&;
    auto assign(string_view value) -> u8string&;
    auto append(u8string_view value) -> u8string&;
    auto append(string_view value) -> u8string&;
    void swap(u8string& other) noexcept;

    auto operator==(const u8string&) const noexcept -> bool = default;
    [[nodiscard]] auto operator==(u8string_view other) const noexcept -> bool { return view() == other; }
    template<size_t N>
    [[nodiscard]] auto operator==(const char8_t (&literal)[N]) const noexcept -> bool
    {
        return view() == literal;
    }

    template<size_t N>
    friend auto operator==(const char8_t (&literal)[N], const u8string& value) noexcept -> bool
    {
        return value == literal;
    }
    auto operator<=>(const u8string&) const noexcept = default;

private:
    using storage_type = std::basic_string<char8_t, std::char_traits<char8_t>, SafeAllocator<char8_t>>;

    template<size_t N>
    [[nodiscard]] static auto CheckedAsciiLiteral(const char (&literal)[N]) -> string_view
    {
        static_assert(N > 0);

        if (literal[N - 1] != 0) {
            throw TextValidationException(TextEncoding::Ascii, TextValidationError::MissingTerminator, N - 1);
        }

        const string_view value {literal, N - 1};
        if (const auto issue = validate_ascii_text(value)) {
            throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
        }

        return value;
    }

    template<size_t N>
    [[nodiscard]] static auto CheckedLiteral(const char8_t (&literal)[N]) -> u8string_view
    {
        static_assert(N > 0);

        if (literal[N - 1] != 0) {
            throw TextValidationException(TextEncoding::Utf8, TextValidationError::MissingTerminator, N - 1);
        }

        return u8string_view::FromChecked(std::u8string_view {literal, N - 1});
    }

    storage_type _storage {};
};

FO_END_NAMESPACE
