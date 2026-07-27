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

struct InvalidFormattedByte
{
    uint8_t Value {};
};

struct NoncopyableFormattedValue
{
    NoncopyableFormattedValue() = default;
    NoncopyableFormattedValue(const NoncopyableFormattedValue&) = delete;
    NoncopyableFormattedValue(NoncopyableFormattedValue&&) = delete;
    auto operator=(const NoncopyableFormattedValue&) -> NoncopyableFormattedValue& = delete;
    auto operator=(NoncopyableFormattedValue&&) -> NoncopyableFormattedValue& = delete;

    uint32_t Value {};
};

FO_END_NAMESPACE

template<>
struct std::formatter<FO_NAMESPACE InvalidFormattedByte> : formatter<char>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE InvalidFormattedByte& value, FormatContext& ctx) const
    {
        FO_NO_STACK_TRACE_ENTRY();

        return formatter<char>::format(std::bit_cast<char>(value.Value), ctx);
    }
};

template<>
struct std::formatter<FO_NAMESPACE NoncopyableFormattedValue> : formatter<uint32_t>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE NoncopyableFormattedValue& value, FormatContext& ctx) const
    {
        FO_NO_STACK_TRACE_ENTRY();

        return formatter<uint32_t>::format(value.Value, ctx);
    }
};

FO_BEGIN_NAMESPACE

template<typename T>
concept CanFormatWithStrexAsciiLiteral = requires(T&& value) { strex("{}", std::forward<T>(value)); };

template<typename T>
concept CanFormatWithStrexUtf8Literal = requires(T&& value) { strex(u8"{}", std::forward<T>(value)); };

template<typename T>
concept CanFormatWithU8StrexUtf8Literal = requires(T&& value) { u8strex(u8"{}", std::forward<T>(value)); };

template<typename T>
concept CanFormatUtf8WithAsciiFormatArgument = requires(T&& value) {
    { FormatUtf8("{}", std::forward<T>(value)) } -> std::same_as<u8string>;
};

template<typename T>
concept CanFormatUtf8WithUtf8FormatArgument = requires(T&& value) {
    { FormatUtf8(u8"{}", std::forward<T>(value)) } -> std::same_as<u8string>;
};

template<typename T>
concept CanUseDynamicStrexFormat = requires(T&& format) { strex(std::forward<T>(format), uint32_t {17}); };

template<typename T>
concept CanUseDynamicUtf8Format = requires(T&& format) {
    { FormatUtf8(std::forward<T>(format), uint32_t {17}) } -> std::same_as<u8string>;
};

static_assert(std::same_as<decltype(FormatUtf8("{}", uint32_t {17})), u8string>);
static_assert(std::same_as<decltype(FormatUtf8(u8"{}", uint32_t {17})), u8string>);

static_assert(CanFormatWithStrexAsciiLiteral<string&>);
static_assert(CanFormatWithStrexAsciiLiteral<string_view>);
static_assert(!CanFormatWithStrexAsciiLiteral<u8string&>);
static_assert(!CanFormatWithStrexAsciiLiteral<u8string_view>);
static_assert(CanFormatWithStrexAsciiLiteral<const char*>);
static_assert(CanFormatWithStrexAsciiLiteral<InvalidFormattedByte>);
static_assert(CanFormatWithStrexAsciiLiteral<NoncopyableFormattedValue&>);
static_assert(CanFormatWithStrexAsciiLiteral<any_t&>);
static_assert(!CanFormatWithStrexAsciiLiteral<u8string>);
static_assert(CanFormatWithStrexAsciiLiteral<strex&>);
static_assert(CanFormatWithStrexAsciiLiteral<strvex&>);
static_assert(!CanFormatWithStrexAsciiLiteral<const char8_t*>);
static_assert(!CanFormatWithStrexAsciiLiteral<const wchar_t*>);

static_assert(!CanFormatWithStrexUtf8Literal<string&>);
static_assert(!CanFormatWithStrexUtf8Literal<u8string&>);
static_assert(CanFormatWithU8StrexUtf8Literal<string&>);
static_assert(CanFormatWithU8StrexUtf8Literal<string_view>);
static_assert(CanFormatWithU8StrexUtf8Literal<u8string&>);
static_assert(CanFormatWithU8StrexUtf8Literal<u8string_view>);
static_assert(CanFormatWithU8StrexUtf8Literal<const char*>);
static_assert(CanFormatWithU8StrexUtf8Literal<InvalidFormattedByte>);
static_assert(CanFormatWithU8StrexUtf8Literal<NoncopyableFormattedValue&>);
static_assert(CanFormatWithU8StrexUtf8Literal<any_t&>);
static_assert(!CanFormatWithStrexUtf8Literal<u8string>);
static_assert(!CanFormatWithStrexUtf8Literal<strex&>);
static_assert(!CanFormatWithStrexUtf8Literal<strvex&>);
static_assert(CanFormatWithU8StrexUtf8Literal<strex&>);
static_assert(CanFormatWithU8StrexUtf8Literal<u8strex&>);
static_assert(!CanFormatWithStrexUtf8Literal<const char8_t*>);
static_assert(!CanFormatWithStrexUtf8Literal<const wchar_t*>);

static_assert(CanFormatUtf8WithAsciiFormatArgument<string>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<const string>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<string&>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<const string&>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<u8string&>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<const u8string&>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<string_view>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<string_view_nt>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<u8string_view>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<u8string_view_nt>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<InvalidFormattedByte>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<NoncopyableFormattedValue&>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<strex&>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<strvex&>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<std::string&>);
static_assert(!CanFormatUtf8WithAsciiFormatArgument<std::u8string&>);
static_assert(!CanFormatUtf8WithAsciiFormatArgument<std::u8string_view>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<u8string>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<const u8string>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<const char*>);
static_assert(!CanFormatUtf8WithAsciiFormatArgument<const char8_t*>);
static_assert(!CanFormatUtf8WithAsciiFormatArgument<const wchar_t*>);
static_assert(!CanFormatUtf8WithAsciiFormatArgument<const char16_t*>);
static_assert(!CanFormatUtf8WithAsciiFormatArgument<const char32_t*>);
static_assert(CanFormatUtf8WithAsciiFormatArgument<const char (&)[4]>);
static_assert(!CanFormatUtf8WithAsciiFormatArgument<const char8_t (&)[4]>);
static_assert(!CanFormatUtf8WithAsciiFormatArgument<const wchar_t (&)[4]>);
static_assert(!CanFormatUtf8WithAsciiFormatArgument<const char16_t (&)[4]>);
static_assert(!CanFormatUtf8WithAsciiFormatArgument<const char32_t (&)[4]>);

static_assert(CanFormatUtf8WithUtf8FormatArgument<string>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<const string>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<string&>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<const string&>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<u8string&>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<const u8string&>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<string_view>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<string_view_nt>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<u8string_view>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<u8string_view_nt>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<InvalidFormattedByte>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<NoncopyableFormattedValue&>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<strex&>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<strvex&>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<std::string&>);
static_assert(!CanFormatUtf8WithUtf8FormatArgument<std::u8string&>);
static_assert(!CanFormatUtf8WithUtf8FormatArgument<std::u8string_view>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<u8string>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<const u8string>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<const char*>);
static_assert(!CanFormatUtf8WithUtf8FormatArgument<const char8_t*>);
static_assert(!CanFormatUtf8WithUtf8FormatArgument<const wchar_t*>);
static_assert(!CanFormatUtf8WithUtf8FormatArgument<const char16_t*>);
static_assert(!CanFormatUtf8WithUtf8FormatArgument<const char32_t*>);
static_assert(CanFormatUtf8WithUtf8FormatArgument<const char (&)[4]>);
static_assert(!CanFormatUtf8WithUtf8FormatArgument<const char8_t (&)[4]>);
static_assert(!CanFormatUtf8WithUtf8FormatArgument<const wchar_t (&)[4]>);
static_assert(!CanFormatUtf8WithUtf8FormatArgument<const char16_t (&)[4]>);
static_assert(!CanFormatUtf8WithUtf8FormatArgument<const char32_t (&)[4]>);

static_assert(!CanUseDynamicStrexFormat<string_view>);
static_assert(!CanUseDynamicStrexFormat<string&>);
static_assert(!CanUseDynamicStrexFormat<const char*>);
static_assert(!CanUseDynamicUtf8Format<string_view>);
static_assert(!CanUseDynamicUtf8Format<std::u8string_view>);
static_assert(!CanUseDynamicUtf8Format<string_view>);
static_assert(!CanUseDynamicUtf8Format<u8string_view>);
static_assert(!CanUseDynamicUtf8Format<string&>);
static_assert(!CanUseDynamicUtf8Format<u8string&>);
static_assert(!CanUseDynamicUtf8Format<const char*>);
static_assert(!CanUseDynamicUtf8Format<const char8_t*>);

TEST_CASE("TextFormatting")
{
    SECTION("EmptyFormatsAndStrictStringsStayEmpty")
    {
        const string empty_ascii;
        const u8string empty_utf8;

        CHECK(strex("").empty());
        CHECK(FormatUtf8("").empty());
        CHECK(FormatUtf8(u8"").empty());

        const string bracketed_ascii = strex("[{}]", empty_ascii);
        const u8string bracketed_utf8 = FormatUtf8(u8"[{}|{}]", empty_ascii, empty_utf8);

        CHECK(bracketed_ascii == "[]");
        CHECK(bracketed_utf8 == u8"[|]");
    }

    SECTION("StrexFormatsAsciiAndUtf8Destinations")
    {
        const u8string utf8_text {u8"мир"};
        const string ascii_result = strex("id={:04}; hex={:#x}; fixed={:.2f}; text={:>5}", uint32_t {7}, uint32_t {255}, 3.14159, "ok");
        const u8string utf8_result = u8strex(u8"значение={:04}; текст={}", uint32_t {7}, utf8_text);

        CHECK(ascii_result == "id=0007; hex=0xff; fixed=3.14; text=   ok");
        CHECK(utf8_result == u8"значение=0007; текст=мир");
        CHECK_THROWS_AS(
            [] {
                const string invalid_ascii_result = strex("{}", InvalidFormattedByte {0x80});
                ignore_unused(invalid_ascii_result);
            }(),
            TextValidationException);
    }

    SECTION("Utf8LiteralAndStrictArgumentsPreserveEveryTextShape")
    {
        const string ascii {"ASCII"};
        const u8string localized {u8"Привет"};
        const u8string result = FormatUtf8(u8"literal=текст|{}|{}|e\u0301|🌍", ascii, localized);

        CHECK(result == u8"literal=текст|ASCII|Привет|e\u0301|🌍");
    }

    SECTION("AsciiFormatPromotesStrictAsciiToUtf8AndAcceptsUtf8Arguments")
    {
        const string label {"message"};
        const u8string value {u8"мир 🌍"};
        const u8string result = FormatUtf8("{}: {}", label, value);
        const u8string temporary_result = FormatUtf8("{}: {}", label, u8string {u8"временный 🌍"});

        CHECK(result == u8"message: мир 🌍");
        CHECK(temporary_result == u8"message: временный 🌍");
    }

    SECTION("NarrowCharacterArgumentsPromoteDirectlyToUtf8")
    {
        const u8string strict_value {u8"ошибка 🌍"};
        const string narrow_value = utf8_to_char_string(strict_value);
        const char* narrow_ptr = narrow_value.c_str();

        const u8string ascii_format_result = FormatUtf8("{} | {}", narrow_value, narrow_ptr);
        const u8string utf8_format_result = FormatUtf8(u8"значение: {}", narrow_ptr);

        CHECK(ascii_format_result == u8"ошибка 🌍 | ошибка 🌍");
        CHECK(utf8_format_result == u8"значение: ошибка 🌍");
    }

    SECTION("MalformedNarrowCharacterArgumentsAreRejectedAfterFormatting")
    {
        string malformed_value {"bad "};
        malformed_value.push_back(std::bit_cast<char>(uint8_t {0xFF}));

        CHECK_THROWS_AS(FormatUtf8("{}", malformed_value), TextValidationException);
        CHECK_THROWS_AS(FormatUtf8(u8"{}", malformed_value.c_str()), TextValidationException);
    }

    SECTION("GenericFormatterReceivesNoncopyableLvalueWithoutCopying")
    {
        NoncopyableFormattedValue value;
        value.Value = 42;

        const string ascii_result = strex("value={:04}", value);
        const u8string utf8_result = FormatUtf8(u8"значение={}", value);

        CHECK(ascii_result == "value=0042");
        CHECK(utf8_result == u8"значение=42");
    }

    SECTION("EmbeddedNullsArePreservedInFormatsAndArguments")
    {
        const string ascii_argument = string(string_view {"x\0y", 3});
        const string ascii_result = strex("A\0{}Z", ascii_argument);
        const string_view expected_ascii {"A\0x\0yZ", 6};

        CHECK(ascii_result.size() == expected_ascii.size());
        CHECK(ascii_result == expected_ascii);

        const u8string utf8_argument = u8string::FromChecked(std::u8string_view {u8"я\0🌍", 7});
        const u8string utf8_result = FormatUtf8(u8"Ю\0{}!", utf8_argument);
        const std::u8string_view expected_utf8 {u8"Ю\0я\0🌍!", 11};

        CHECK(utf8_result.size() == expected_utf8.size());
        CHECK(utf8_result.view().native_view() == expected_utf8);
    }

    SECTION("WholeAsciiOutputIsValidatedAfterCustomFormatting")
    {
        try {
            const string result = strex("ok:{}!", InvalidFormattedByte {0x80});
            ignore_unused(result);
            FAIL("Invalid ASCII strex result was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Ascii);
            CHECK(ex.error() == TextValidationError::NonAsciiByte);
            CHECK(ex.offset() == 3);
        }
    }

    SECTION("WholeUtf8OutputIsValidatedAfterCustomFormatting")
    {
        try {
            (void)FormatUtf8("ok:{}!", InvalidFormattedByte {0x80});
            FAIL("Invalid UTF-8 formatter output from an ASCII format was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::InvalidLeadByte);
            CHECK(ex.offset() == 3);
        }

        try {
            (void)FormatUtf8(u8"ok:{}!", InvalidFormattedByte {0xFF});
            FAIL("Invalid UTF-8 formatter output was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::ScalarOutOfRange);
            CHECK(ex.offset() == 3);
        }
    }
}

FO_END_NAMESPACE
