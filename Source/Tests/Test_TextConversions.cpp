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

#include "catch_amalgamated.hpp"

#include "Common.h"

FO_BEGIN_NAMESPACE

template<size_t N>
[[nodiscard]] static constexpr auto ToCharStorage(const char8_t (&value)[N]) noexcept -> array<char, N>
{
    FO_NO_STACK_TRACE_ENTRY();

    array<char, N> result {};

    for (size_t i = 0; i < N; i++) {
        result[i] = std::bit_cast<char>(value[i]);
    }

    return result;
}

template<size_t N>
[[nodiscard]] static constexpr auto ToByteStorage(const char8_t (&value)[N]) noexcept -> array<byte, N>
{
    FO_NO_STACK_TRACE_ENTRY();

    array<byte, N> result {};

    for (size_t i = 0; i < N; i++) {
        result[i] = byte {static_cast<uint8_t>(value[i])};
    }

    return result;
}

template<typename T>
concept CanConvertToCStr = requires(T&& value) { utf8_to_c_str(std::forward<T>(value)); };

template<typename T>
concept CanValidateUtf16 = requires(T&& value) { validate_utf16_text(std::forward<T>(value)); };

template<typename T>
concept CanConvertUtf16 = requires(T&& value) { utf16_to_utf8(std::forward<T>(value)); };

template<typename T>
concept CanBridgeUtf8ToCharView = requires(T&& value) { utf8_as_char_view(std::forward<T>(value)); };

template<typename T>
concept CanCreateAsciiCharString = requires(T&& value) { utf8_to_string(std::forward<T>(value)); };

static_assert(!CanConvertToCStr<string_view_nt>);
static_assert(CanConvertToCStr<u8string_view_nt>);
static_assert(!CanConvertToCStr<string_view>);
static_assert(!CanConvertToCStr<u8string_view>);
static_assert(!CanConvertToCStr<const char*>);
static_assert(!CanConvertToCStr<const char8_t*>);
static_assert(CanValidateUtf16<std::u16string_view>);
static_assert(CanConvertUtf16<std::u16string_view>);
static_assert(!CanValidateUtf16<const char16_t*>);
static_assert(!CanConvertUtf16<const char16_t*>);
static_assert(CanBridgeUtf8ToCharView<u8string_view>);
static_assert(!CanBridgeUtf8ToCharView<string_view>);
static_assert(CanCreateAsciiCharString<u8string_view>);
static_assert(CanCreateAsciiCharString<u8string&>);
static_assert(!CanCreateAsciiCharString<u8string>);
static_assert(CanCreateAsciiCharString<std::u8string_view>);
static_assert(CanCreateAsciiCharString<const char8_t (&)[5]>);
static_assert(!CanCreateAsciiCharString<string_view>);
static_assert(!CanCreateAsciiCharString<const char8_t*>);

TEST_CASE("TextConversions")
{
    SECTION("CharAndByteInputsRoundTripEveryRequiredUtf8Shape")
    {
        constexpr char8_t source[] = u8"ASCII|Привет|e\u0301|🌍|�|\0|end";
        constexpr array<char, std::size(source)> source_chars = ToCharStorage(source);
        constexpr array<byte, std::size(source)> source_bytes = ToByteStorage(source);
        const u8string_view expected = u8string_view::FromChecked(std::u8string_view {source, std::size(source) - 1});

        const const_span<char> char_input {source_chars.data(), source_chars.size() - 1};
        const const_span<byte> byte_input {source_bytes.data(), source_bytes.size() - 1};
        const u8string from_chars = utf8_from_char_span(char_input);
        const u8string from_bytes = utf8_from_byte_span(byte_input);

        CHECK(from_chars.view().native_view() == expected.native_view());
        CHECK(from_bytes.view().native_view() == expected.native_view());
        CHECK(from_chars.size() == char_input.size());
        CHECK(from_bytes.size() == byte_input.size());

        const const_span<char> chars_again = utf8_to_char_span(from_chars.view());
        const const_span<byte> bytes_again = utf8_to_byte_span(from_bytes.view());
        REQUIRE(chars_again.size() == char_input.size());
        REQUIRE(bytes_again.size() == byte_input.size());
        CHECK(string_view {chars_again.data(), chars_again.size()} == string_view {char_input.data(), char_input.size()});
        CHECK(std::ranges::equal(bytes_again, byte_input));
        CHECK(utf8_from_char_span(chars_again) == from_chars);
        CHECK(utf8_from_byte_span(bytes_again) == from_bytes);

        const array<byte, 7> ascii_bytes = {byte {'A'}, byte {'S'}, byte {'C'}, byte {0}, byte {'I'}, byte {'I'}, byte {0x7F}};
        const string ascii = string_from_byte_span(ascii_bytes);
        CHECK(ascii == string_view {"ASC\0II\x7F", 7});
        CHECK(std::ranges::equal(string_to_byte_span(ascii), ascii_bytes));
        CHECK(string_from_byte_span(string_to_byte_span(ascii)) == ascii);
    }

    SECTION("GlobalCharBridgesPreserveTextAndCheckAsciiNarrowing")
    {
        constexpr char8_t source[] = u8"ASCII|Привет|é|🌍|\0|end";
        constexpr array<char, std::size(source)> source_chars = ToCharStorage(source);
        const const_span<char> char_source {source_chars.data(), source_chars.size() - 1};
        const u8string utf8 = utf8_from_char_span(char_source);
        const string_view borrowed = utf8_as_char_view(utf8.view());
        const string owned = utf8_to_char_string(utf8.view());

        CHECK(std::ranges::equal(borrowed, char_source));
        CHECK(std::ranges::equal(owned, char_source));
        CHECK(static_cast<const void*>(borrowed.data()) == static_cast<const void*>(utf8.view().data()));

        const u8string map_value_one {u8"Первое"};
        const u8string map_value_two {u8"Second"};
        const map<string_view, u8string_view> strict_values = {{"One", map_value_one.view()}, {"Two", map_value_two.view()}};
        const map<string_view, string_view> char_values = utf8_map_as_char_views(strict_values);
        CHECK(char_values.at("One") == utf8_as_char_view(map_value_one.view()));
        CHECK(char_values.at("Two") == "Second");
        CHECK(static_cast<const void*>(char_values.at("One").data()) == static_cast<const void*>(map_value_one.view().data()));

        const string ascii {"Config.Key-7"};
        CHECK(ascii == "Config.Key-7");
        CHECK(utf8_to_string(u8"Config.Key-7") == "Config.Key-7");
        CHECK(utf8_to_string(std::u8string_view {u8"Section"}) == "Section");

        try {
            (void)utf8_to_string(u8"секция");
            FAIL("Non-ASCII UTF-8 was accepted as an ASCII char string");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Ascii);
            CHECK(ex.error() == TextValidationError::NonAsciiByte);
            CHECK(ex.offset() == 0);
        }

        const std::u8string malformed = {char8_t {'A'}, char8_t {0x80}};

        try {
            (void)utf8_to_string(std::u8string_view {malformed});
            FAIL("Malformed UTF-8 was accepted as an ASCII char string");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::InvalidLeadByte);
            CHECK(ex.offset() == 1);
        }

        std::u8string stale_storage = u8"valid";
        const u8string_view stale = u8string_view::FromChecked(stale_storage);
        stale_storage[1] = char8_t {0x80};

        CHECK_THROWS_AS(utf8_to_char_string(stale), TextValidationException);
        CHECK_THROWS_AS(utf8_to_string(stale), TextValidationException);
    }

    SECTION("MalformedUtf8ReportsTheExactReasonAndOffset")
    {
        const array<byte, 3> invalid_lead = {byte {'A'}, byte {'B'}, byte {0x80}};
        const array<byte, 4> invalid_continuation = {byte {'A'}, byte {0xE2}, byte {0x28}, byte {0xA1}};
        const array<byte, 3> truncated = {byte {'A'}, byte {0xF0}, byte {0x9F}};
        const array<byte, 2> overlong = {byte {0xC0}, byte {0xAF}};
        const array<byte, 3> surrogate = {byte {0xED}, byte {0xA0}, byte {0x80}};
        const array<byte, 5> out_of_range = {byte {'A'}, byte {0xF4}, byte {0x90}, byte {0x80}, byte {0x80}};
        const array<pair<const_span<byte>, TextValidationIssue>, 6> cases = {{
            {invalid_lead, {TextValidationError::InvalidLeadByte, 2}},
            {invalid_continuation, {TextValidationError::InvalidContinuationByte, 2}},
            {truncated, {TextValidationError::TruncatedSequence, 1}},
            {overlong, {TextValidationError::OverlongSequence, 0}},
            {surrogate, {TextValidationError::SurrogateScalar, 0}},
            {out_of_range, {TextValidationError::ScalarOutOfRange, 1}},
        }};

        for (const auto& [value, expected] : cases) {
            try {
                (void)utf8_from_byte_span(value);
                FAIL("Malformed UTF-8 byte input was accepted");
            }
            catch (const TextValidationException& ex) {
                CHECK(ex.encoding() == TextEncoding::Utf8);
                CHECK(ex.error() == expected.Error);
                CHECK(ex.offset() == expected.Offset);
            }
        }

        const array<char, 4> invalid_chars = {'A', std::bit_cast<char>(uint8_t {0xE2}), '(', std::bit_cast<char>(uint8_t {0xA1})};

        try {
            (void)utf8_from_char_span(invalid_chars);
            FAIL("Malformed UTF-8 char input was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::InvalidContinuationByte);
            CHECK(ex.offset() == 2);
        }

        const array<byte, 2> invalid_ascii = {byte {'A'}, byte {0x80}};

        try {
            (void)string_from_byte_span(invalid_ascii);
            FAIL("Non-ASCII byte input was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Ascii);
            CHECK(ex.error() == TextValidationError::NonAsciiByte);
            CHECK(ex.offset() == 1);
        }
    }

    SECTION("TerminatedCharInputIsBoundedAndChecked")
    {
        constexpr char8_t source[] = u8"Привет 🌍";
        constexpr array<char, std::size(source)> source_chars = ToCharStorage(source);
        const u8string converted = utf8_from_terminated_char_span(source_chars);
        CHECK(converted.view().native_view() == std::u8string_view {source, std::size(source) - 1});

        const array<char, 2> missing_terminator = {'n', 'o'};

        try {
            (void)utf8_from_terminated_char_span(missing_terminator);
            FAIL("A missing char terminator was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::MissingTerminator);
            CHECK(ex.offset() == 1);
        }

        const array<char, 4> embedded_null = {'A', char {}, 'B', char {}};

        try {
            (void)utf8_from_terminated_char_span(embedded_null);
            FAIL("An embedded char null was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::EmbeddedNull);
            CHECK(ex.offset() == 1);
        }

        const const_span<char> empty_storage {};

        try {
            (void)utf8_from_terminated_char_span(empty_storage);
            FAIL("Empty storage was accepted as terminated text");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::MissingTerminator);
            CHECK(ex.offset() == 0);
        }
    }

    SECTION("OutgoingSpansAreZeroCopyAndCStrsAreExact")
    {
        constexpr char8_t utf8_source[] = u8"Привет 🌍";
        constexpr array<char, std::size(utf8_source)> utf8_expected_chars = ToCharStorage(utf8_source);
        const u8string utf8 {utf8_source};
        const u8string_view utf8_view = utf8.view();
        const const_span<char> utf8_chars = utf8_to_char_span(utf8_view);
        const const_span<byte> utf8_bytes = utf8_to_byte_span(utf8_view);

        CHECK(utf8_chars.size() == utf8_view.size());
        CHECK(utf8_bytes.size() == utf8_view.size());
        CHECK(static_cast<const void*>(utf8_chars.data()) == static_cast<const void*>(utf8_view.data()));
        CHECK(static_cast<const void*>(utf8_bytes.data()) == static_cast<const void*>(utf8_view.data()));

        const ptr<const char> utf8_cstr = utf8_to_c_str(utf8.view_nt());
        const const_span<char> utf8_cstr_content = make_span(utf8_cstr, utf8_chars.size());
        const const_span<char> utf8_expected_content {utf8_expected_chars.data(), utf8_expected_chars.size() - 1};
        CHECK(utf8_cstr == make_ptr(utf8_chars.data()));
        CHECK(std::ranges::equal(utf8_cstr_content, utf8_expected_content));
        CHECK(utf8_cstr[utf8_chars.size()] == char {});

        const string ascii {"SDK-name"};
        const string_view ascii_view = ascii;
        const const_span<byte> ascii_bytes = string_to_byte_span(ascii_view);
        CHECK(ascii_bytes.size() == ascii_view.size());
        CHECK(static_cast<const void*>(ascii_bytes.data()) == static_cast<const void*>(ascii_view.data()));

        const ptr<const char> ascii_cstr {ascii.c_str()};
        const const_span<char> ascii_cstr_content = make_span(ascii_cstr, ascii_view.size());
        CHECK(ascii_cstr == make_ptr(ascii_view.data()));
        CHECK(ascii_view == "SDK-name");
        CHECK(std::ranges::equal(ascii_cstr_content, ascii_view));
        CHECK(ascii_cstr[ascii_view.size()] == char {});
    }

    SECTION("Utf16RoundTripsUnicodeScalarBoundaries")
    {
        const array<char16_t, 11> boundaries = {
            char16_t {0x0000},
            char16_t {0x007F},
            char16_t {0x0080},
            char16_t {0xD7FF},
            char16_t {0xE000},
            char16_t {0xFFFD},
            char16_t {0xFFFF},
            char16_t {0xD800},
            char16_t {0xDC00},
            char16_t {0xDBFF},
            char16_t {0xDFFF},
        };
        const std::u16string_view source {boundaries.data(), boundaries.size()};

        CHECK_FALSE(validate_utf16_text(source));

        const u8string utf8 = utf16_to_utf8(source);
        const utf16_string decoded = utf8_to_utf16(utf8.view());
        CHECK(std::u16string_view {decoded.data(), decoded.size()} == source);
        CHECK(utf16_to_utf8(std::u16string_view {decoded.data(), decoded.size()}) == utf8);
    }

    SECTION("Utf16RejectsLoneSurrogatesAtTheirExactOffsets")
    {
        const array<char16_t, 2> trailing_high = {u'A', char16_t {0xD800}};
        const array<char16_t, 3> high_before_scalar = {u'A', char16_t {0xD800}, u'B'};
        const array<char16_t, 3> lone_low = {u'A', char16_t {0xDC00}, u'B'};
        const array<pair<std::u16string_view, TextValidationIssue>, 3> cases = {{
            {{trailing_high.data(), trailing_high.size()}, {TextValidationError::UnpairedHighSurrogate, 1}},
            {{high_before_scalar.data(), high_before_scalar.size()}, {TextValidationError::UnpairedHighSurrogate, 1}},
            {{lone_low.data(), lone_low.size()}, {TextValidationError::UnpairedLowSurrogate, 1}},
        }};

        for (const auto& [value, expected] : cases) {
            CHECK(validate_utf16_text(value) == optional<TextValidationIssue> {expected});

            try {
                (void)utf16_to_utf8(value);
                FAIL("An unpaired UTF-16 surrogate was accepted");
            }
            catch (const TextValidationException& ex) {
                CHECK(ex.encoding() == TextEncoding::Utf16);
                CHECK(ex.error() == expected.Error);
                CHECK(ex.offset() == expected.Offset);
            }
        }
    }

    SECTION("utf8_to_utf16RevalidatesBrandedViews")
    {
        std::u8string storage = u8"valid";
        const optional<u8string_view> checked = u8string_view::TryFrom(storage);
        REQUIRE(checked);
        const u8string_view stale = checked.value();
        storage[0] = char8_t {0x80};

        try {
            (void)utf8_to_utf16(stale);
            FAIL("A stale invalid UTF-8 view was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::InvalidLeadByte);
            CHECK(ex.offset() == 0);
        }
    }

#if FO_WINDOWS
    SECTION("WindowsWideTextPreservesStrictUtf16CodeUnits")
    {
        const array<char16_t, 5> source = {u'A', char16_t {0x0416}, char16_t {0xD83C}, char16_t {0xDF0D}, u'Z'};
        const std::u16string_view source_view {source.data(), source.size()};
        const wide_string wide = utf16_to_wide(source_view);
        const utf16_string decoded = wide_to_utf16(std::wstring_view {wide.data(), wide.size()});

        CHECK(std::u16string_view {decoded.data(), decoded.size()} == source_view);

        const string ascii {"SDK-name"};
        const wide_string ascii_wide = string_to_wide_string(string_view_nt_from_span(const_span<char> {ascii.data(), ascii.size() + 1}));
        CHECK(std::wstring_view {ascii_wide.data(), ascii_wide.size()} == L"SDK-name");

        const u8string utf8 {u8"Привет 🌍"};
        const wide_string utf8_wide = utf8_to_wide_string(utf8.view());
        CHECK(wide_to_utf16(std::wstring_view {utf8_wide.data(), utf8_wide.size()}) == utf8_to_utf16(utf8.view()));

        const array<char16_t, 2> invalid_source = {u'A', char16_t {0xD800}};
        CHECK_THROWS_AS(utf16_to_wide(std::u16string_view {invalid_source.data(), invalid_source.size()}), TextValidationException);

        wide_string invalid_wide;
        invalid_wide.push_back(L'A');
        invalid_wide.push_back(std::bit_cast<wchar_t>(char16_t {0xDC00}));
        CHECK_THROWS_AS(wide_to_utf16(std::wstring_view {invalid_wide.data(), invalid_wide.size()}), TextValidationException);
    }
#endif
}

FO_END_NAMESPACE
