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

#include "catch_amalgamated.hpp"

#include "Common.h"

FO_BEGIN_NAMESPACE

namespace
{
    auto RawBytes(std::initializer_list<uint8_t> values) -> vector<byte>
    {
        FO_STACK_TRACE_ENTRY();

        vector<byte> bytes;
        bytes.reserve(values.size());

        for (const uint8_t value : values) {
            bytes.emplace_back(static_cast<byte>(value));
        }

        return bytes;
    }
}

static_assert(std::constructible_from<strvex, string_view>);
static_assert(std::constructible_from<strvex, string&>);
static_assert(!std::constructible_from<strvex, string&&>);
static_assert(!std::constructible_from<strvex, u8string_view>);
static_assert(std::constructible_from<u8strvex, u8string_view>);
static_assert(std::constructible_from<u8strvex, u8string&>);
static_assert(!std::constructible_from<u8strvex, u8string&&>);
static_assert(!std::constructible_from<u8strvex, string_view>);
static_assert(std::convertible_to<strex, string>);
static_assert(std::convertible_to<strex, u8string>);
static_assert(std::convertible_to<u8strex, u8string_view>);
static_assert(!std::convertible_to<u8strex, string_view>);
static_assert(!std::convertible_to<u8strex, string>);

// Formatting this value always throws, so the safe_format path can be checked after it has already
// appended part of its output
struct ThrowingFormatValue
{
};

FO_END_NAMESPACE

template<>
struct std::formatter<FO_NAMESPACE ThrowingFormatValue> : formatter<FO_NAMESPACE string_view> // NOLINT(cert-dcl58-cpp)
{
    // The return type has to be spelled out: the body only throws, so deduction would pick void and the
    // type would stop satisfying the formattable concept
    template<typename FormatContext>
    auto format(const FO_NAMESPACE ThrowingFormatValue& /*value*/, FormatContext& ctx) const -> decltype(ctx.out())
    {
        throw std::format_error("Intentional formatting failure");
    }
};

FO_BEGIN_NAMESPACE

TEST_CASE("StringUtils")
{
    SECTION("Storage")
    {
        CHECK(strex("hllo").erase('h').trim().str() == "llo");
        CHECK(strex("h\rllo").erase('h').trim().str() == "llo");
        CHECK(strex("hllo\t\n").erase('h').trim().str() == "llo");
        CHECK(strex("h    llo    ").erase('h').trim().str() == "llo");
        CHECK(strex("h d     g      llo                            gg  ").erase('h').trim().erase('d').trim().str() == "g      llo                            gg");
    }

    SECTION("Length")
    {
        CHECK(strex().length() == 0);
        CHECK(strex(" hello ").length() == 7);
        CHECK(strex("").empty());
        CHECK_FALSE(strex("fff").empty());
    }

    SECTION("Parse")
    {
        CHECK(strex("  {} World {}", "Hello", "!").str() == "  Hello World !");
        CHECK(strex("{}{}{}", 1, 2, 3).str() == "123");

        const u8string strict_value {u8"Привет"};
        const u8string ascii_format_result = u8strex("{} {}", strict_value, 42);
        const u8string utf8_format_result = u8strex(u8"{} мир", strict_value);
        CHECK(ascii_format_result == u8"Привет 42");
        CHECK(utf8_format_result == u8"Привет мир");
    }

    SECTION("UriScheme")
    {
        CHECK(is_uri_scheme_letter('A'));
        CHECK(is_uri_scheme_letter('z'));
        CHECK_FALSE(is_uri_scheme_letter('0'));
        CHECK_FALSE(is_uri_scheme_letter('-'));

        CHECK(is_uri_scheme_tail_character('A'));
        CHECK(is_uri_scheme_tail_character('0'));
        CHECK(is_uri_scheme_tail_character('+'));
        CHECK(is_uri_scheme_tail_character('-'));
        CHECK(is_uri_scheme_tail_character('.'));
        CHECK_FALSE(is_uri_scheme_tail_character('_'));
        CHECK_FALSE(is_uri_scheme_tail_character(':'));

        CHECK(parse_uri_scheme("a") == optional<string_view> {"a"});
        CHECK(parse_uri_scheme("LastFrontier+login-1.0") == optional<string_view> {"LastFrontier+login-1.0"});
        CHECK_FALSE(parse_uri_scheme("").has_value());
        CHECK_FALSE(parse_uri_scheme("1scheme").has_value());
        CHECK_FALSE(parse_uri_scheme("scheme_name").has_value());
        CHECK_FALSE(parse_uri_scheme("scheme:").has_value());
        CHECK_FALSE(parse_uri_scheme("scheme://host").has_value());
    }

    SECTION("Format")
    {
        CHECK(strex("{} {} {}", "text", 42, 1.5).str() == "text 42 1.5");
        CHECK(strex(strex::safe_format, "{} {} {}", "text", 42, 1.5).str() == "text 42 1.5");
        CHECK(strex(strex::dynamic_format, "{} {} {}", "text", 42, 1.5).str() == "text 42 1.5");
        CHECK(strex("{}", string("engine string")).str() == "engine string");
        CHECK(strex("no placeholders").str() == "no placeholders");
    }

    SECTION("SafeFormatReportsErrorInsteadOfPartialOutput")
    {
        // The formatter appends directly into the result buffer, so a mid-format throw must not leave
        // the already-written prefix behind
        strex formatted {strex::safe_format, "written prefix {} unreachable suffix", ThrowingFormatValue {}};
        CHECK(formatted.strv().starts_with("Format error: "));
        CHECK_FALSE(formatted.strv().starts_with("written prefix"));
    }

    SECTION("Trim")
    {
        CHECK(strex() == "");
        CHECK(strex().str().empty());
        CHECK(*strex().c_str() == 0);
        CHECK(strex("Hello").str() == "Hello");
        CHECK(strex("Hello   ").trim().str() == "Hello");
        CHECK(strex("  Hello").trim().str() == "Hello");
        CHECK(strex("\t\nHel lo\t \r\t").trim().str() == "Hel lo");
        CHECK(strex("\t\nHel lo\t \r\t").trim("\t\r ").str() == "\nHel lo");
        CHECK(strex("\t\nHel lo\t \r\t").ltrim("\t\n").str() == "Hel lo\t \r\t");
        CHECK(strex("\t\nHel lo\t \r\t").rtrim("\t\n").str() == "\t\nHel lo\t \r");
    }

    SECTION("GetResult")
    {
        const string direct_ascii_result = strex("Dump_{}", 17);
        const u8string direct_utf8_result = strex("Dump_{}", 17);

        CHECK(static_cast<string_view>(strex(" Hello   ").trim()) == "Hello");
        CHECK(static_cast<string>(strex(" \tHello   ").trim()) == "Hello");
        CHECK(direct_ascii_result == "Dump_17");
        CHECK(direct_utf8_result == u8"Dump_17");
        CHECK(utf8_to_string(u8strex("{}", "Dump_17")) == "Dump_17");
        CHECK_THROWS_AS(utf8_to_string(u8strex(u8"Дамп_17")), TextValidationException);
        CHECK(strex("Hello   ").trim().str() == "Hello");
        CHECK(string_view {strex("  Hello").trim().c_str()} == "Hello");
        CHECK(strex("\t\nHel lo\t \r\t").trim().str() == "Hel lo");
    }

    SECTION("Compare")
    {
        CHECK(strex(" Hello   ").str() == strex(" Hello   ").str());
        CHECK_FALSE(strex(" Hello1   ").str() == strex(" Hello2   ").str());
        CHECK(strex(" Hello   ").compare_ignore_case(" heLLo   "));
        CHECK_FALSE(strex(" Hello   ").compare_ignore_case(" hhhhh   "));
        CHECK_FALSE(strex(" Hello   ").compare_ignore_case("xxx"));
    }

    SECTION("Utf8")
    {
        const u8string strict_hello {u8" Привет   "};
        const u8string hello_lower {u8" привет   "};
        const u8string hello_mixed {u8" Привет   "};
        const u8string hello_upper {u8" ПРИВЕТ   "};
        const u8string other_word {u8" тттввв   "};
        const u8string three_e {u8"еее"};
        const u8string upper_source {u8" ПриВЕТ   "};
        const u8string upper_mixed {u8" ПриВет   "};
        const u8string replacement_character {u8"�"};
        const vector<byte> encoded_surrogate_bytes = RawBytes({0xED, 0xA0, 0x80});
        const string_view encoded_surrogate = span_to_string(encoded_surrogate_bytes);
        const vector<byte> invalid_lead_bytes = RawBytes({200});

        CHECK_FALSE(validate_utf8_text(string_view {}));
        CHECK(validate_utf8_text(span_to_string(invalid_lead_bytes)));
        CHECK_FALSE(validate_utf8_text(hello_lower.view().native_view()));
        CHECK_FALSE(validate_utf8_text(replacement_character.view().native_view()));
        CHECK(validate_utf8_text(encoded_surrogate));
        CHECK(u8strvex(hello_mixed).compare_ignore_case(hello_lower));
        CHECK(u8strvex(replacement_character).compare_ignore_case(replacement_character));
        CHECK_FALSE(u8strvex(hello_mixed).compare_ignore_case(other_word));
        CHECK_FALSE(u8strvex(hello_mixed).compare_ignore_case(three_e));
        CHECK(u8strvex(strict_hello).length_utf8() == 10);

        const u8string strict_hello_copy = u8strex(strict_hello);
        CHECK(strict_hello_copy == strict_hello);

        CHECK_THROWS_AS(u8string(span_to_string(invalid_lead_bytes)), TextValidationException);

        CHECK(u8strvex(hello_mixed).length_utf8() == 10);
        CHECK(u8strvex(three_e).length_utf8() == 3);
        CHECK(u8strvex(hello_mixed).compare_ignore_case(hello_upper));
        CHECK_FALSE(u8strvex(hello_mixed).compare_ignore_case(u8" ПРЕТТТ   "));
        CHECK(u8strex(upper_source).lower() == hello_lower);
        CHECK(u8strex(upper_mixed).upper() == hello_upper);

        const u8string controls_removed = u8strex(u8"\x03Привет\x7F\n").erase_ascii_control_chars();
        CHECK(controls_removed == u8"Привет");
        CHECK(u8strvex(u8"\t Привет 🌍 \r\n").trim() == u8"Привет 🌍");
        CHECK(u8strvex(u8" \n\r\t").trim().empty());
        CHECK(u8strvex(u8"жПриветж").trim(u8"ж") == u8"Привет");
        CHECK(u8strvex(u8"жтж").rtrim(u8"ж") == u8"жт");

        const u8string metadata_line {u8"Proto Modifier Сomfortable.Сarrying ComfortableCarrying"};
        const auto metadata_tokens = u8strvex(metadata_line).tokenize();
        REQUIRE(metadata_tokens.size() == 6);
        CHECK(metadata_tokens[0] == u8"Proto");
        CHECK(metadata_tokens[1] == u8"Modifier");
        CHECK(metadata_tokens[2] == u8"Сomfortable");
        CHECK(metadata_tokens[3] == u8".");
        CHECK(metadata_tokens[4] == u8"Сarrying");
        CHECK(metadata_tokens[5] == u8"ComfortableCarrying");
    }

    SECTION("Utf8ScalarValidity")
    {
        CHECK(utf8::IsValid(0x000000u));
        CHECK(utf8::IsValid(0x00D7FFu));
        CHECK(utf8::IsValid(0x00E000u));
        CHECK(utf8::IsValid(0x00FFFDu));
        CHECK(utf8::IsValid(0x10FFFFu));

        CHECK_FALSE(utf8::IsValid(0x00D800u));
        CHECK_FALSE(utf8::IsValid(0x00DFFFu));
        CHECK_FALSE(utf8::IsValid(0x110000u));
        CHECK_FALSE(utf8::IsValid(0xFFFFFFFFu));
    }

    SECTION("Utf8CodecRoundTrip")
    {
        struct ValidCase
        {
            vector<byte> Encoded;
            uint32_t CodePoint;
        };

        const array<ValidCase, 11> valid_cases = {
            ValidCase {RawBytes({0x00}), 0x000000u},
            ValidCase {RawBytes({0x7F}), 0x00007Fu},
            ValidCase {RawBytes({0xC2, 0x80}), 0x000080u},
            ValidCase {RawBytes({0xDF, 0xBF}), 0x0007FFu},
            ValidCase {RawBytes({0xE0, 0xA0, 0x80}), 0x000800u},
            ValidCase {RawBytes({0xED, 0x9F, 0xBF}), 0x00D7FFu},
            ValidCase {RawBytes({0xEE, 0x80, 0x80}), 0x00E000u},
            ValidCase {RawBytes({0xEF, 0xBF, 0xBD}), 0x00FFFDu},
            ValidCase {RawBytes({0xEF, 0xBF, 0xBF}), 0x00FFFFu},
            ValidCase {RawBytes({0xF0, 0x90, 0x80, 0x80}), 0x010000u},
            ValidCase {RawBytes({0xF4, 0x8F, 0xBF, 0xBF}), 0x10FFFFu},
        };

        for (const auto& test : valid_cases) {
            const string_view encoded = span_to_string(test.Encoded);
            size_t decode_length = encoded.size();
            const auto decoded = utf8::Decode(make_ptr(encoded.data()), decode_length);
            REQUIRE(decoded.has_value());
            CHECK(*decoded == test.CodePoint);
            CHECK(decode_length == encoded.size());

            char encoded_buf[4] {};
            const auto encoded_length = utf8::Encode(test.CodePoint, encoded_buf);
            REQUIRE(encoded_length.has_value());
            CHECK(*encoded_length == encoded.size());
            CHECK(std::ranges::equal(make_byte_span(encoded_buf, *encoded_length), test.Encoded));

            size_t round_trip_length = *encoded_length;
            const auto round_trip = utf8::Decode(make_ptr(encoded_buf), round_trip_length);
            REQUIRE(round_trip.has_value());
            CHECK(*round_trip == test.CodePoint);
            CHECK(round_trip_length == *encoded_length);
        }
    }

    SECTION("Utf8MalformedSequences")
    {
        struct InvalidCase
        {
            vector<byte> Encoded;
            size_t ConsumedLength;
        };

        const array<InvalidCase, 22> invalid_cases = {
            InvalidCase {RawBytes({0x80}), 1},
            InvalidCase {RawBytes({0xC0, 0x80}), 1},
            InvalidCase {RawBytes({0xC1, 0xBF}), 1},
            InvalidCase {RawBytes({0xC2}), 1},
            InvalidCase {RawBytes({0xC2, 0x20}), 1},
            InvalidCase {RawBytes({0xE0, 0x80, 0x80}), 1},
            InvalidCase {RawBytes({0xE0, 0x9F, 0xBF}), 1},
            InvalidCase {RawBytes({0xE1, 0x80}), 1},
            InvalidCase {RawBytes({0xE1, 0x41, 0x80}), 1},
            InvalidCase {RawBytes({0xE1, 0x80, 0x41}), 1},
            InvalidCase {RawBytes({0xED, 0xA0, 0x80}), 3},
            InvalidCase {RawBytes({0xED, 0xBF, 0xBF}), 3},
            InvalidCase {RawBytes({0xF0, 0x80, 0x80, 0x80}), 1},
            InvalidCase {RawBytes({0xF0, 0x8F, 0xBF, 0xBF}), 1},
            InvalidCase {RawBytes({0xF0, 0x90, 0x41, 0x80}), 1},
            InvalidCase {RawBytes({0xF0, 0x90, 0x80, 0x41}), 1},
            InvalidCase {RawBytes({0xF1, 0x80, 0x80}), 1},
            InvalidCase {RawBytes({0xF1, 0x80, 0x41, 0x80}), 1},
            InvalidCase {RawBytes({0xF1, 0x80, 0x80, 0x41}), 1},
            InvalidCase {RawBytes({0xF4, 0x90, 0x80, 0x80}), 1},
            InvalidCase {RawBytes({0xF5, 0x80, 0x80, 0x80}), 1},
            InvalidCase {RawBytes({0xFF}), 1},
        };

        for (const auto& test : invalid_cases) {
            const string_view encoded = span_to_string(test.Encoded);
            size_t decode_length = encoded.size();
            const auto decoded = utf8::Decode(make_ptr(encoded.data()), decode_length);
            CHECK_FALSE(decoded.has_value());
            CHECK(decode_length == test.ConsumedLength);
        }

        size_t empty_length = 0;
        CHECK_FALSE(utf8::Decode(make_ptr(""), empty_length).has_value());
        CHECK(empty_length == 0);
    }

    SECTION("Utf8EncodeRejectsInvalidScalars")
    {
        constexpr array<uint32_t, 4> invalid_scalars = {0x00D800u, 0x00DFFFu, 0x110000u, 0xFFFFFFFFu};

        for (const uint32_t code_point : invalid_scalars) {
            char encoded_buf[4] = {'k', 'e', 'e', 'p'};
            const auto encoded_length = utf8::Encode(code_point, encoded_buf);
            CHECK_FALSE(encoded_length.has_value());
            CHECK(string_view(encoded_buf, sizeof(encoded_buf)) == "keep");
        }
    }

    SECTION("StartsWith")
    {
        CHECK(strex(" Hello   ").starts_with(" Hell"));
        CHECK(strex("xHello   ").starts_with('x'));
        CHECK(strex(" Hello1   ").ends_with("1   "));
        CHECK(strex(" Hello1  x").ends_with('x'));
    }

    SECTION("Numbers")
    {
        CHECK_FALSE(strex("").is_number());
        CHECK_FALSE(strex("   ").is_number());
        CHECK_FALSE(strex("\t\n\r").is_number());
        CHECK_FALSE(strex("0x").is_number());
        CHECK_FALSE(strex("-0x").is_number());
        CHECK(strex("-0x5").is_number());
        CHECK(strex("-0xa").is_number());
        CHECK_FALSE(strex("--1").is_number());
        CHECK_FALSE(strex("-0xy").is_number());
        CHECK_FALSE(strex("0XZ").is_number());
        CHECK(strex("0x1").is_number());
        CHECK(strex("123").is_number());
        CHECK(strex("0x123").is_number());
        CHECK(strex(" 123").is_number());
        CHECK(strex("123 \r\n\t").is_number());
        CHECK_FALSE(strex("123llll").is_number());
        CHECK_FALSE(strex("123+=").is_number());
        CHECK(strex("1.0").is_number());
        CHECK_FALSE(strex("x123").is_number());
        CHECK(strex("\t0123").is_number());
        CHECK(strex(" 0x123").is_number());
        CHECK(strex("1.0f").is_number());
        CHECK(strex("12.0").is_number());
        CHECK(strex("12").is_number());
        CHECK(strex("12.0f").is_number());
        CHECK(strex("1.0f").is_number());
        CHECK_FALSE(strex("nan").is_number());
        CHECK_FALSE(strex("inf").is_number());
        CHECK_FALSE(strex("-inf").is_number());
        CHECK(strex("nan").is_non_finite_number());
        CHECK(strex(" inf ").is_non_finite_number());
        CHECK(strex("-INFINITY").is_non_finite_number());
        CHECK(strex("nan(payload)").is_non_finite_number());
        CHECK_FALSE(strex("NaNish").is_non_finite_number());
        CHECK(strex(string(strex::MAX_NUMBER_STRING_LENGTH, '5')).is_number());
        CHECK_FALSE(strex(string(strex::MAX_NUMBER_STRING_LENGTH + 1, '5')).is_number());

        CHECK(strex("true").is_explicit_bool());
        CHECK(strex("TRUE ").is_explicit_bool());
        CHECK(strex("False  ").is_explicit_bool());
        CHECK(strex(" \t  False\t").is_explicit_bool());
        CHECK_FALSE(strex("1").is_explicit_bool());
        CHECK_FALSE(strex("").is_explicit_bool());

        CHECK(strex("").to_int32() == 0);
        CHECK(strex("00000000012").to_int32() == 12);
        CHECK(strex("1").to_int32() == 1);
        CHECK(strex("-156").to_int32() == -156);
        CHECK(strex("-429496729500").to_int32() == std::numeric_limits<int32_t>::min());
        CHECK(strex("4294967295").to_int32() == std::numeric_limits<int32_t>::max());
        CHECK(strex("0xFFFF").to_int32() == 0xFFFF);
        CHECK(strex("-0xFFFF").to_int32() == -0xFFFF);
        CHECK(strex("012345").to_int32() == 12345); // Do not recognize octal numbers
        CHECK(strex("-012345").to_int32() == -12345);
        CHECK(strex("0xFFFF.88").to_int32() == 0);
        CHECK(strex("12.9").to_int32() == 12);
        CHECK(strex("12.1111").to_int32() == 12);

        CHECK(strex("111").to_uint32() == 111);
        CHECK(strex("-100").to_uint32() == std::numeric_limits<uint32_t>::min());
        CHECK(strex("1000000000000").to_uint32() == std::numeric_limits<uint32_t>::max());
        CHECK(strex("4294967295").to_uint32() == 4294967295);

        CHECK(strex("66666666666677").to_int64() == 66666666666677ll);
        CHECK(strex("66666666666677.087468726").to_int64() == 66666666666677ll);
        CHECK(strex("66666666666677.33f").to_int64() == 66666666666677ll);
        CHECK(strex("-666666666666.").to_int64() == -666666666666ll);
        CHECK(strex("-666666666666.f").to_int64() == -666666666666ll);
        CHECK(strex("-666666666666.111111111111111").to_int64() == -666666666666ll);
        CHECK(strex("-9223372036854775808").to_int64() == std::numeric_limits<int64_t>::min()); // Min 64bit signed -9223372036854775808
        CHECK(strex("-9223372036854775807").to_int64() == (std::numeric_limits<int64_t>::min() + 1));
        CHECK(strex("-9223372036854775809").to_int64() == std::numeric_limits<int64_t>::min());
        CHECK(strex("-9223372036854779999").to_int64() == std::numeric_limits<int64_t>::min());
        CHECK(strex("-9223372036854775809").to_int64() == std::numeric_limits<int64_t>::min());
        CHECK(strex("-18446744073709551614").to_int64() == std::numeric_limits<int64_t>::min());
        CHECK(strex("-18446744073709551616").to_int64() == std::numeric_limits<int64_t>::min());
        CHECK(strex("-18446744073456345654709551616").to_int64() == std::numeric_limits<int64_t>::min());
        CHECK(strex("9223372036854775807").to_int64() == std::numeric_limits<int64_t>::max()); // Max 64bit signed 9223372036854775807
        CHECK(strex("9223372036854775806").to_int64() == (std::numeric_limits<int64_t>::max() - 1));
        CHECK_FALSE(strex("9223372036854775808").to_int64() == std::numeric_limits<int64_t>::max());
        CHECK_FALSE(strex("18446744073709551615").to_int64() == std::numeric_limits<int64_t>::max());
        CHECK(strex("18446744073709551615").to_int64() == std::bit_cast<int64_t>(std::numeric_limits<uint64_t>::max())); // Max 64bit 18446744073709551615
        CHECK(strex("18446744073709551614").to_int64() == std::bit_cast<int64_t>(std::numeric_limits<uint64_t>::max() - 1));
        CHECK(strex("18446744073709551616").to_int64() == std::bit_cast<int64_t>(std::numeric_limits<uint64_t>::max()));
        CHECK(strex("184467440737095516546734734716").to_int64() == std::bit_cast<int64_t>(std::numeric_limits<uint64_t>::max()));
        CHECK(strex(string(strex::MAX_NUMBER_STRING_LENGTH, '5')).to_int64() == std::bit_cast<int64_t>(std::numeric_limits<uint64_t>::max()));
        CHECK(strex(string(strex::MAX_NUMBER_STRING_LENGTH + 1, '5')).to_int64() == 0);

        CHECK(is_float_equal(strex("455.6573").to_float32(), 455.6573f));
        CHECK(is_float_equal(strex("0xFFFF").to_float32(), numeric_cast<float32_t>(0xFFFF)));
        CHECK(is_float_equal(strex("-0xFFFF").to_float32(), numeric_cast<float32_t>(-0xFFFF)));
        CHECK(is_float_equal(strex("0xFFFF.44").to_float32(), numeric_cast<float32_t>(0)));
        CHECK(is_float_equal(strex("f").to_float32(), numeric_cast<float32_t>(0)));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<float32_t>::min()).to_float32(), std::numeric_limits<float32_t>::min()));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<float32_t>::max()).to_float32(), std::numeric_limits<float32_t>::max()));
        CHECK(is_float_equal(strex("34567774455.65745678555").to_float64(), 34567774455.65745678555));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<float64_t>::min()).to_float64(), std::numeric_limits<float64_t>::min()));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<float64_t>::max()).to_float64(), std::numeric_limits<float64_t>::max()));
        CHECK(is_float_equal(strex("nan").to_float32(), 0.0f));
        CHECK(is_float_equal(strex("inf").to_float64(), 0.0));

        CHECK(strex(" true ").to_bool() == true);
        CHECK(strex(" 1 ").to_bool() == true);
        CHECK(strex(" 211 ").to_bool() == true);
        CHECK(strex(" false ").to_bool() == false);
        CHECK(strex(" 0").to_bool() == false);
        CHECK(strex(" abc").to_bool() == false);
    }

    SECTION("Split")
    {
        CHECK(strex(" One Two    \tThree   ").split(' ') == vector<string>({"One", "Two", "Three"}));
        CHECK(strex(" One Two    \tThree   ").split('\t') == vector<string>({"One Two", "Three"}));
        CHECK(strex(" One Two    \tThree   ").split('X') == vector<string>({"One Two    \tThree"}));
        CHECK(strex(" One Two  X \tThree   ").split('X') == vector<string>({"One Two", "Three"}));
        CHECK(strex(",One,Two").split(',') == vector<string>({"One", "Two"}));
        CHECK(strex("One,Two,").split(',') == vector<string>({"One", "Two"}));

        const auto utf8_parts = u8strex(u8" Привет мир  Земля ").split(u8' ');
        constexpr u8string_view hello {u8"Привет"};
        constexpr u8string_view world {u8"мир"};
        constexpr u8string_view earth {u8"Земля"};
        REQUIRE(utf8_parts.size() == 3);
        CHECK(utf8_parts[0] == hello);
        CHECK(utf8_parts[1] == world);
        CHECK(utf8_parts[2] == earth);

        CHECK(strex(" 111 222  33Three g66 7").split_to_int32(' ') == vector<int32_t>({111, 222, 0, 0, 7}));
        CHECK(strex("").split_to_int32(' ') == vector<int32_t>({}));
        CHECK(strex("             ").split_to_int32(' ') == vector<int32_t>({}));
        CHECK(strex("1").split_to_int32(' ') == vector<int32_t>({1}));
        CHECK(strex("1 -2").split_to_int32(' ') == vector<int32_t>({1, -2}));
        CHECK(strex("\t1   X -2 X 3").split_to_int32('X') == vector<int32_t>({1, -2, 3}));
        CHECK(strex("\t1 X\t\t  X X-2   X X 3\n").split_to_int32('X') == vector<int32_t>({1, -2, 3}));
    }

    SECTION("Substring")
    {
        CHECK(strex("abcdpZ ppplZ ls").substring_until('Z').str() == "abcdp");
        CHECK(strex("abcdpZ ppplZ ls").substring_until('X').str() == "abcdpZ ppplZ ls");
        CHECK(strex("abcdpZ ppplZ ls").substring_until("lZ").str() == "abcdpZ ppp");
        CHECK(strex("abcdpZ ppplZ ls").substring_until("XXX").str() == "abcdpZ ppplZ ls");
        CHECK(strex("abcdpZ ppplZ ls").substring_after('Z').str() == " ppplZ ls");
        CHECK(strex("abcdpZ ppplZ ls").substring_after('X').str().empty());
        CHECK(strex("abcdpZ ppplZ ls").substring_after("lZ").str() == " ls");
        CHECK(strex("abcdpZ ppplZ ls").substring_after("XXX").str().empty());
    }

    SECTION("Modify")
    {
        CHECK(strex("aaaBBBcccDBEFGh Hello !").erase('B').str() == "aaacccDEFGh Hello !");
        CHECK(strex("aaaBBBcccDBEFaGh HelBlo Ba!").erase('a', 'B').str() == "BBcccDBEFlo Ba!");
        CHECK(strex("aaaBBBcccDBEFaGh HelBlo Ba!").erase('X', 'Y').str() == "aaaBBBcccDBEFaGh HelBlo Ba!");
        CHECK(strex("aaaBBBcccDBEFaGh HelBlo Ba!").replace('a', 'X').str() == "XXXBBBcccDBEFXGh HelBlo BX!");
        CHECK(strex("aaBDdDBaBBBDBcccDBEFaGh HelDBBlo Ba!").replace('D', 'B', 'X').str() == "aaBDdXaBBBXcccXEFaGh HelXBlo Ba!");
        CHECK(strex("aaBDdDBaHelDBBlocccDBEFaGh HelDBBlo Ba!").replace("HelDBBlo", "X").str() == "aaBDdDBaXcccDBEFaGh X Ba!");
        const u8string replaced_utf8 = u8strex(u8"Привет, мир! Привет!").replace(u8"Привет", u8"Здравствуй");
        CHECK(replaced_utf8 == u8"Здравствуй, мир! Здравствуй!");
        CHECK(strex("aaaBBBcccDBEFGh Hello !").lower().str() == "aaabbbcccdbefgh hello !");
        CHECK(strex("aaaBBBcccDBEFGh Hello !").upper().str() == "AAABBBCCCDBEFGH HELLO !");
    }

    SECTION("Path")
    {
        CHECK(strex("./cur").format_path().str() == "cur");
        CHECK(strex("./cur/next").format_path().str() == "cur/next");
        CHECK(strex("./cur/next/../last").format_path().str() == "cur/last");
        CHECK(strex("./cur/next/../../last/").format_path().str() == "last/");
        CHECK(strex("D:\\MyDir\\X\\..\\Y\\.\\.\\Z\\").format_path().str() == "D:/MyDir/Y/Z/");
        CHECK(strex("./cur/next/../../last/a/FILE1.ZIP").format_path().extract_dir().str() == "last/a");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").format_path().extract_file_name().str() == "FILE1.ZIP");
        CHECK(strex("./cur/next/../../last/a/FILE1.ZIP").format_path().extract_dir().str() == "last/a");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").format_path().change_file_name("NEWfile").str() == "last/NEWfile.zip");
        CHECK(strex("./cur/next/../../last").format_path().change_file_name("NEWfile").str() == "NEWfile");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").format_path().get_file_extension().str() == "zip");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").format_path().erase_file_extension().str() == "last/FILE1");
        CHECK(strex("cur/next/../../last/a/").combine_path("x/y/z").str() == "last/a/x/y/z");
        CHECK(strex("../cur/next/../../last/a/").combine_path("x/y/z").str() == "../last/a/x/y/z");
        CHECK(strex("../../cur/next/../../last/a/").combine_path("x/y/z").str() == "../../last/a/x/y/z");
        const u8string combined_utf8_path = u8strex(u8"каталог/").combine_path(u8"файл.txt");
        CHECK(combined_utf8_path == u8"каталог/файл.txt");
        CHECK(strex("D:\\MyDir\\X\\..\\Y\\.\\.\\Z\\").normalize_path_slashes().str() == "D:/MyDir/X/../Y/././Z/");
    }

    SECTION("LineEndings")
    {
        CHECK(strex("Hello\r\nWorld\r\n").normalize_line_endings().str() == "Hello\nWorld\n");
    }

#if FO_WINDOWS
    SECTION("WinParseWideChar")
    {
        const u8string utf8_world {u8"Мир"};
        const string char_utf8_world = utf8_to_char_string(utf8_world);

        CHECK(strex().parse_wide_char(L"Hello").str() == "Hello");
        CHECK(strex("Hello").parse_wide_char(L"World").str() == "HelloWorld");
        CHECK(strex().parse_wide_char(L"HelloМир").to_wide_char() == L"HelloМир");
        CHECK(strex(char_utf8_world).parse_wide_char(L"Мир").to_wide_char() == L"МирМир");
    }
#endif
}

FO_END_NAMESPACE
