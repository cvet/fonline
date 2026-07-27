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

template<typename T>
concept HasRvalueView = requires(T&& value) { std::move(value).view(); };

template<typename T>
concept CanTryUtf8ViewFrom = requires(T&& value) { u8string_view::TryFrom(std::forward<T>(value)); };

template<typename T>
concept CanCheckUtf8ViewFrom = requires(T&& value) { u8string_view::FromChecked(std::forward<T>(value)); };

template<typename T>
concept CanTryUtf8NtViewFrom = requires(T&& value) { u8string_view_nt::TryFrom(std::forward<T>(value)); };

template<typename T>
concept CanCheckUtf8NtViewFrom = requires(T&& value) { u8string_view_nt::FromChecked(std::forward<T>(value)); };

template<typename T>
concept CanValidateAsciiText = requires(T&& value) { validate_ascii_text(std::forward<T>(value)); };

template<typename T>
concept CanValidateUtf8Text = requires(T&& value) { validate_utf8_text(std::forward<T>(value)); };

static_assert(std::same_as<string_view, std::string_view>);
static_assert(!std::same_as<u8string, string>);
static_assert(std::convertible_to<string, u8string>);
static_assert(std::convertible_to<std::string, u8string>);
static_assert(std::convertible_to<u8string&, u8string_view>);
static_assert(std::convertible_to<const u8string&, u8string_view>);
static_assert(!std::convertible_to<u8string, u8string_view>);
static_assert(!std::convertible_to<const u8string, u8string_view>);
static_assert(requires(u8string& value) { value = "ASCII"; });
static_assert(std::convertible_to<const char*, u8string>);
static_assert(!std::convertible_to<u8string, string>);
static_assert(std::constructible_from<string, string_view>);
static_assert(std::convertible_to<decltype("ASCII"), string_view>);
static_assert(std::convertible_to<decltype(u8"UTF-8 🌍"), u8string_view>);
static_assert(!std::convertible_to<string_view, string>);
static_assert(!std::convertible_to<std::u8string_view, u8string>);
static_assert(!HasRvalueView<string>);
static_assert(!HasRvalueView<u8string>);
static_assert(!std::default_initializable<string_view_nt>);
static_assert(!std::default_initializable<u8string_view_nt>);
static_assert(CanTryUtf8ViewFrom<std::u8string&>);
static_assert(CanTryUtf8ViewFrom<const std::u8string&>);
static_assert(CanCheckUtf8ViewFrom<std::u8string&>);
static_assert(CanCheckUtf8ViewFrom<const std::u8string&>);
static_assert(!CanTryUtf8ViewFrom<std::u8string>);
static_assert(!CanCheckUtf8ViewFrom<std::u8string>);
static_assert(CanTryUtf8ViewFrom<std::u8string_view>);
static_assert(CanCheckUtf8ViewFrom<std::u8string_view&>);
static_assert(!CanTryUtf8ViewFrom<const char8_t*>);
static_assert(!CanCheckUtf8ViewFrom<const char8_t*>);
static_assert(!CanTryUtf8NtViewFrom<const_span<char8_t>>);
static_assert(CanTryUtf8NtViewFrom<const_span<char8_t>&>);
static_assert(CanCheckUtf8NtViewFrom<const_span<char8_t>&>);
static_assert(!CanCheckUtf8NtViewFrom<const_span<char8_t>>);
static_assert(CanValidateAsciiText<string_view>);
static_assert(CanValidateAsciiText<std::u8string_view>);
static_assert(!CanValidateAsciiText<const char*>);
static_assert(!CanValidateAsciiText<const char8_t*>);
static_assert(CanValidateUtf8Text<string_view>);
static_assert(CanValidateUtf8Text<std::u8string_view>);
static_assert(!CanValidateUtf8Text<const char*>);
static_assert(!CanValidateUtf8Text<const char8_t*>);

constexpr char8_t ValidUtf8WithNull[] = {char8_t {0x00}, char8_t {0xEF}, char8_t {0xBF}, char8_t {0xBD}, char8_t {0xF4}, char8_t {0x8F}, char8_t {0xBF}, char8_t {0xBF}};
constexpr char8_t InvalidUtf8Overlong[] = {char8_t {0xC0}, char8_t {0x80}};
constexpr char8_t InvalidUtf8Surrogate[] = {char8_t {0xED}, char8_t {0xA0}, char8_t {0x80}};
constexpr char8_t InvalidUtf8Truncated[] = {char8_t {0xF0}, char8_t {0x9F}, char8_t {0x92}};

static_assert(u8string_view::TryFrom(std::u8string_view {ValidUtf8WithNull, std::size(ValidUtf8WithNull)}));
static_assert(!u8string_view::TryFrom(std::u8string_view {InvalidUtf8Overlong, std::size(InvalidUtf8Overlong)}));
static_assert(!u8string_view::TryFrom(std::u8string_view {InvalidUtf8Surrogate, std::size(InvalidUtf8Surrogate)}));
static_assert(!u8string_view::TryFrom(std::u8string_view {InvalidUtf8Truncated, std::size(InvalidUtf8Truncated)}));

TEST_CASE("TextTypes")
{
    SECTION("ValidationReportsExactReasonAndOffset")
    {
        const array<char8_t, 1> invalid_lead = {char8_t {0x80}};
        const array<char8_t, 3> invalid_continuation = {char8_t {0xE2}, char8_t {0x28}, char8_t {0xA1}};
        const array<char8_t, 3> truncated = {char8_t {0xF0}, char8_t {0x9F}, char8_t {0x92}};
        const array<char8_t, 2> overlong = {char8_t {0xC0}, char8_t {0xAF}};
        const array<char8_t, 3> surrogate = {char8_t {0xED}, char8_t {0xA0}, char8_t {0x80}};
        const array<char8_t, 4> out_of_range = {char8_t {0xF4}, char8_t {0x90}, char8_t {0x80}, char8_t {0x80}};
        const array<char8_t, 2> invalid_continuation_before_truncation = {char8_t {0xF0}, char8_t {0x28}};
        const array<char8_t, 3> overlong_before_later_invalid = {char8_t {0xE0}, char8_t {0x80}, char8_t {0x41}};
        const array<char8_t, 3> surrogate_before_later_invalid = {char8_t {0xED}, char8_t {0xA0}, char8_t {0x41}};
        const array<char8_t, 3> out_of_range_before_later_invalid = {char8_t {0xF4}, char8_t {0x90}, char8_t {0x41}};

        const array<pair<std::u8string_view, TextValidationIssue>, 10> cases = {{
            {{invalid_lead.data(), invalid_lead.size()}, {TextValidationError::InvalidLeadByte, 0}},
            {{invalid_continuation.data(), invalid_continuation.size()}, {TextValidationError::InvalidContinuationByte, 1}},
            {{truncated.data(), truncated.size()}, {TextValidationError::TruncatedSequence, 0}},
            {{overlong.data(), overlong.size()}, {TextValidationError::OverlongSequence, 0}},
            {{surrogate.data(), surrogate.size()}, {TextValidationError::SurrogateScalar, 0}},
            {{out_of_range.data(), out_of_range.size()}, {TextValidationError::ScalarOutOfRange, 0}},
            {{invalid_continuation_before_truncation.data(), invalid_continuation_before_truncation.size()}, {TextValidationError::InvalidContinuationByte, 1}},
            {{overlong_before_later_invalid.data(), overlong_before_later_invalid.size()}, {TextValidationError::OverlongSequence, 0}},
            {{surrogate_before_later_invalid.data(), surrogate_before_later_invalid.size()}, {TextValidationError::SurrogateScalar, 0}},
            {{out_of_range_before_later_invalid.data(), out_of_range_before_later_invalid.size()}, {TextValidationError::ScalarOutOfRange, 0}},
        }};

        for (const auto& [value, expected] : cases) {
            const auto issue = validate_utf8_text(value);
            REQUIRE(issue);
            CHECK(issue.value() == expected);
            CHECK_FALSE(u8string_view::TryFrom(value));

            array<char, 4> char_bytes {};
            for (size_t i = 0; i < value.size(); i++) {
                char_bytes[i] = static_cast<char>(value[i]);
            }

            const string_view value_chars {char_bytes.data(), value.size()};
            CHECK(validate_utf8_text(value_chars) == validate_utf8_text(value));
        }

        try {
            (void)u8string_view::FromChecked(cases[1].first);
            FAIL("Invalid UTF-8 was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::InvalidContinuationByte);
            CHECK(ex.offset() == 1);
            CHECK(string_view {ex.what()} == "Invalid UTF-8 continuation byte");
        }
    }

    SECTION("StringPromotesAndUtf8DemotionIsChecked")
    {
        const string ascii {"Alpha-127\x7F"};
        const u8string promoted = ascii;
        CHECK(promoted.view().native_view() == u8"Alpha-127\x7F");

        const string demoted = utf8_to_string(promoted);
        CHECK(demoted == "Alpha-127\x7F");

        const u8string localized {u8"Привет 🌍"};
        CHECK_THROWS_AS((void)utf8_to_string(localized), TextValidationException);
    }

    SECTION("StringUsesTheAsciiContractWithoutAnotherWrapper")
    {
        string ascii {"schema"};
        ascii.append("-name");
        CHECK(ascii == "schema-name");
        ascii.assign("reset");
        CHECK(ascii == "reset");

        u8string utf8 {u8"Привет"};
        utf8.append(" ");
        utf8.append(u8"мир 🌍");
        CHECK(utf8.view().native_view() == u8"Привет мир 🌍");

        utf8.assign("ASCII");
        CHECK(utf8.view().native_view() == u8"ASCII");
        utf8.assign(u8"U+FFFD: �");
        CHECK(utf8.view().native_view() == u8"U+FFFD: �");

        string invalid_ascii;
        invalid_ascii.push_back(std::bit_cast<char>(uint8_t {0x80}));
        CHECK_THROWS_AS((void)u8string {invalid_ascii}, TextValidationException);

        const u8string stable {u8"stable"};
        u8string target = stable;
        CHECK_THROWS_AS((void)target.assign(invalid_ascii), TextValidationException);
        CHECK(target == stable);
        CHECK_THROWS_AS((void)target.append(invalid_ascii), TextValidationException);
        CHECK(target == stable);
    }

    SECTION("Utf8OwnersRevalidateViewsWhoseBackingStorageChanged")
    {
        std::u8string raw_utf8 = u8"valid";
        const optional<u8string_view> checked_utf8 = u8string_view::TryFrom(raw_utf8);
        REQUIRE(checked_utf8);
        const u8string_view stale_utf8 = checked_utf8.value();
        raw_utf8[0] = char8_t {0x80};

        CHECK_THROWS_AS((void)u8string {stale_utf8}, TextValidationException);

        u8string utf8_target {u8"stable"};
        CHECK_THROWS_AS((void)utf8_target.assign(stale_utf8), TextValidationException);
        CHECK(utf8_target.view().native_view() == u8"stable");
        CHECK_THROWS_AS((void)utf8_target.append(stale_utf8), TextValidationException);
        CHECK(utf8_target.view().native_view() == u8"stable");
        CHECK_THROWS_AS((void)utf8_to_string(stale_utf8), TextValidationException);
    }

    SECTION("Utf8MoveOperationsLeaveBothObjectsValid")
    {
        u8string source {u8"источник 🌍"};
        u8string moved {std::move(source)};
        CHECK(source.empty());
        CHECK_FALSE(validate_utf8_text(source.view().native_view()));
        CHECK(moved.view().native_view() == u8"источник 🌍");

        u8string assign_source {u8"назначено"};
        u8string target {u8"старое"};
        target = std::move(assign_source);
        CHECK(assign_source.empty());
        CHECK_FALSE(validate_utf8_text(assign_source.view().native_view()));
        CHECK(target.view().native_view() == u8"назначено");
        target = std::move(target);
        CHECK(target.view().native_view() == u8"назначено");
    }

    SECTION("EmbeddedNullNeedsAnExplicitTerminatedView")
    {
        const string ascii_with_null {"A\0B", 3};
        const const_span<char> ascii_with_null_storage {ascii_with_null.data(), ascii_with_null.size() + 1};
        CHECK_FALSE(try_string_view_nt_from_span(ascii_with_null_storage));
        CHECK_THROWS_AS((void)string_view_nt_from_span(ascii_with_null_storage), TextValidationException);

        const u8string utf8_with_null {u8"Я\0Z"};
        CHECK_FALSE(utf8_with_null.try_view_nt());
        CHECK_THROWS_AS((void)utf8_with_null.view_nt(), TextValidationException);

        const string ascii_plain {"plain"};
        const string_view_nt ascii_nt = string_view_nt_from_span(const_span<char> {ascii_plain.data(), ascii_plain.size() + 1});
        CHECK(ascii_nt == "plain");
        CHECK(ascii_nt.c_str()[ascii_nt.size()] == 0);

        const u8string utf8_plain {u8"текст"};
        const u8string_view_nt utf8_nt = utf8_plain.view_nt();
        CHECK(utf8_nt.c_str()[utf8_nt.size()] == 0);
        CHECK(utf8_nt.view().native_view() == u8"текст");
    }

    SECTION("BoundedTerminatedFactoriesValidateTheirWholeStorage")
    {
        const array<char, 6> valid_ascii = {'p', 'l', 'a', 'i', 'n', char {}};
        const optional<string_view_nt> ascii_nt = try_string_view_nt_from_span(const_span<char> {valid_ascii});
        REQUIRE(ascii_nt);
        CHECK(*ascii_nt == "plain");

        const array<char, 2> missing_ascii_terminator = {'n', 'o'};
        const const_span<char> missing_ascii_storage {missing_ascii_terminator};
        CHECK_FALSE(try_string_view_nt_from_span(missing_ascii_storage));

        try {
            (void)string_view_nt_from_span(missing_ascii_storage);
            FAIL("Missing string terminator was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.error() == TextValidationError::MissingTerminator);
            CHECK(ex.offset() == 1);
        }

        const array<char, 4> embedded_ascii_null = {'A', char {}, 'B', char {}};
        const const_span<char> embedded_ascii_storage {embedded_ascii_null};
        CHECK_FALSE(try_string_view_nt_from_span(embedded_ascii_storage));

        try {
            (void)string_view_nt_from_span(embedded_ascii_storage);
            FAIL("Embedded null was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.error() == TextValidationError::EmbeddedNull);
            CHECK(ex.offset() == 1);
        }

        const const_span<char> empty_ascii_storage {};
        CHECK_FALSE(try_string_view_nt_from_span(empty_ascii_storage));
        CHECK_THROWS_AS((void)string_view_nt_from_span(empty_ascii_storage), TextValidationException);

        constexpr char8_t ValidUtf8Nt[] = u8"текст";
        const const_span<char8_t> valid_utf8_storage {ValidUtf8Nt};
        const optional<u8string_view_nt> utf8_nt = u8string_view_nt::TryFrom(valid_utf8_storage);
        REQUIRE(utf8_nt);
        CHECK(utf8_nt->view().native_view() == u8"текст");

        const array<char8_t, 4> invalid_utf8_nt = {char8_t {0xE2}, char8_t {0x28}, char8_t {0xA1}, char8_t {}};
        const const_span<char8_t> invalid_utf8_storage {invalid_utf8_nt};
        CHECK_FALSE(u8string_view_nt::TryFrom(invalid_utf8_storage));
        CHECK_THROWS_AS((void)u8string_view_nt::FromChecked(invalid_utf8_storage), TextValidationException);
    }

    SECTION("ValidatorOverloadsStayInParity")
    {
        for (uint16_t raw = 0; raw <= 0xFF; raw++) {
            const char code_unit = static_cast<char>(static_cast<uint8_t>(raw));
            const string_view value {&code_unit, 1};
            CHECK(validate_utf8_text(value).has_value() == (raw >= 0x80));
        }

        const array<byte, 4> valid_bytes = {byte {0xF0}, byte {0x9F}, byte {0x8C}, byte {0x8D}};
        CHECK_FALSE(validate_utf8_text(valid_bytes));

        const array<byte, 3> invalid_bytes = {byte {0xED}, byte {0xA0}, byte {0x80}};
        CHECK((validate_utf8_text(invalid_bytes) == TextValidationIssue {TextValidationError::SurrogateScalar, 0}));

        const array<byte, 3> ascii_bytes = {byte {0x41}, byte {0x7F}, byte {0x5A}};
        CHECK_FALSE(validate_ascii_text(ascii_bytes));

        const array<byte, 2> non_ascii_bytes = {byte {0x41}, byte {0x80}};
        CHECK((validate_ascii_text(non_ascii_bytes) == TextValidationIssue {TextValidationError::NonAsciiByte, 1}));
    }

    SECTION("ValidUnicodeBoundariesKeepTheirExactCodeUnits")
    {
        constexpr u8string_view boundary_view {u8"\0\x7F\xC2\x80\xDF\xBF\xE0\xA0\x80\xED\x9F\xBF\xEE\x80\x80\xEF\xBF\xBD\xEF\xBF\xBF\xF0\x90\x80\x80\xF4\x8F\xBF\xBF"};
        const u8string boundaries {boundary_view};

        CHECK(boundaries.view().native_view() == boundary_view.native_view());
        CHECK_FALSE(boundaries.try_view_nt());

        constexpr string_view_nt ascii_literal {"literal"};
        constexpr u8string_view_nt utf8_literal {u8"литерал"};
        CHECK(string_view {ascii_literal.data(), ascii_literal.size()} == "literal");
        CHECK(std::u8string_view {utf8_literal.c_str(), utf8_literal.size()} == u8"литерал");
    }
}

FO_END_NAMESPACE
