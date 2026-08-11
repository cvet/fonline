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

#include "TextConversions.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

using utf8_storage = std::basic_string<char8_t, std::char_traits<char8_t>, SafeAllocator<char8_t>>;

static auto DecodeUtf8Scalar(std::u8string_view value, size_t& offset) noexcept -> uint32_t;
static void AppendUtf16Scalar(utf16_string& output, uint32_t scalar);
static void AppendUtf8Scalar(utf8_storage& output, uint32_t scalar);

auto utf8_from_char_span(const_span<char> value) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    if (value.empty()) {
        return {};
    }

    string_view source {value.data(), value.size()};

    if (auto issue = validate_utf8_text(source)) {
        throw TextValidationException(TextEncoding::Utf8, issue->Error, issue->Offset);
    }

    utf8_storage copy;
    copy.reserve(value.size());

    for (char code_unit : value) {
        copy.push_back(static_cast<char8_t>(static_cast<unsigned char>(code_unit)));
    }

    return u8string::FromChecked(copy);
}

auto utf8_from_byte_span(const_span<byte> value) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    if (value.empty()) {
        return {};
    }

    if (auto issue = validate_utf8_text(value)) {
        throw TextValidationException(TextEncoding::Utf8, issue->Error, issue->Offset);
    }

    utf8_storage copy;
    copy.reserve(value.size());

    for (byte code_unit : value) {
        copy.push_back(static_cast<char8_t>(std::to_integer<uint8_t>(code_unit)));
    }

    return u8string::FromChecked(copy);
}

auto string_from_byte_span(const_span<byte> value) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (value.empty()) {
        return {};
    }

    if (auto issue = validate_ascii_text(value)) {
        throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
    }

    std::basic_string<char, std::char_traits<char>, SafeAllocator<char>> copy;
    copy.reserve(value.size());

    for (byte code_unit : value) {
        copy.push_back(std::to_integer<char>(code_unit));
    }

    return string(copy);
}

auto utf8_from_terminated_char_span(const_span<char> storage) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    if (storage.empty() || storage.back() != char {}) {
        size_t offset = storage.empty() ? 0 : storage.size() - 1;
        throw TextValidationException(TextEncoding::Utf8, TextValidationError::MissingTerminator, offset);
    }

    const_span<char> content = storage.first(storage.size() - 1);

    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == char {}) {
            throw TextValidationException(TextEncoding::Utf8, TextValidationError::EmbeddedNull, i);
        }
    }

    return utf8_from_char_span(content);
}

auto utf8_as_char_view(u8string_view value) noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    const_span<char> chars = utf8_to_char_span(value);
    return {chars.data(), chars.size()};
}

auto utf8_to_char_string(u8string_view value) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (auto issue = validate_utf8_text(value.native_view())) {
        throw TextValidationException(TextEncoding::Utf8, issue->Error, issue->Offset);
    }

    return string {utf8_as_char_view(value)};
}

auto utf8_to_char_string(const u8string& value) -> string
{
    FO_STACK_TRACE_ENTRY();

    return utf8_to_char_string(value.view());
}

auto utf8_to_string(u8string_view value) -> string
{
    FO_STACK_TRACE_ENTRY();

    return utf8_to_string(value.native_view());
}

auto utf8_to_string(std::u8string_view value) -> string
{
    FO_STACK_TRACE_ENTRY();

    u8string_view utf8_value = u8string_view::FromChecked(value);

    if (auto issue = validate_ascii_text(utf8_value.native_view())) {
        throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
    }

    string result;
    result.reserve(utf8_value.size());

    for (char8_t code_unit : utf8_value.native_view()) {
        result.push_back(static_cast<char>(code_unit));
    }

    return result;
}

auto utf8_to_char_span(u8string_view value) noexcept -> const_span<char>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (value.empty()) {
        return {};
    }

    ptr<const char8_t> data {value.data()};
    return {data.reinterpret_as<char>().get(), value.size()};
}

auto utf8_to_byte_span(u8string_view value) noexcept -> const_span<byte>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (value.empty()) {
        return {};
    }

    return std::as_bytes(const_span<char8_t> {value.data(), value.size()});
}

auto string_to_byte_span(string_view value) noexcept -> const_span<byte>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (value.empty()) {
        return {};
    }

    return std::as_bytes(const_span<char> {value.data(), value.size()});
}

auto utf8_to_c_str(u8string_view_nt value) noexcept -> ptr<const char>
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<const char8_t> data {value.c_str()};
    return data.reinterpret_as<char>();
}

auto return_utf8_c_str(u8string_view_nt value) noexcept -> const char*
{
    FO_NO_STACK_TRACE_ENTRY();

    return utf8_to_c_str(value).get();
}

auto validate_utf16_text(std::u16string_view value) noexcept -> optional<TextValidationIssue>
{
    FO_NO_STACK_TRACE_ENTRY();

    size_t offset = 0;

    while (offset < value.size()) {
        char16_t code_unit = value[offset];

        if (code_unit >= char16_t {0xD800} && code_unit <= char16_t {0xDBFF}) {
            if (offset + 1 >= value.size() || value[offset + 1] < char16_t {0xDC00} || value[offset + 1] > char16_t {0xDFFF}) {
                return TextValidationIssue {TextValidationError::UnpairedHighSurrogate, offset};
            }

            offset += 2;
        }
        else if (code_unit >= char16_t {0xDC00} && code_unit <= char16_t {0xDFFF}) {
            return TextValidationIssue {TextValidationError::UnpairedLowSurrogate, offset};
        }
        else {
            offset++;
        }
    }

    return std::nullopt;
}

auto string_to_utf16(string_view value) -> utf16_string
{
    FO_STACK_TRACE_ENTRY();

    if (auto issue = validate_ascii_text(value)) {
        throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
    }

    utf16_string result;
    result.reserve(value.size());

    for (char code_unit : value) {
        result.push_back(static_cast<char16_t>(code_unit));
    }

    return result;
}

auto utf16_to_string(std::u16string_view value) -> string
{
    FO_STACK_TRACE_ENTRY();

    u8string utf8_value = utf16_to_utf8(value);
    return utf8_to_string(utf8_value);
}

auto utf8_to_utf16(u8string_view value) -> utf16_string
{
    FO_STACK_TRACE_ENTRY();

    if (value.empty()) {
        return {};
    }

    if (auto issue = validate_utf8_text(value.native_view())) {
        throw TextValidationException(TextEncoding::Utf8, issue->Error, issue->Offset);
    }

    utf16_string result;
    result.reserve(value.size());

    size_t offset = 0;

    while (offset < value.size()) {
        uint32_t scalar = DecodeUtf8Scalar(value.native_view(), offset);
        AppendUtf16Scalar(result, scalar);
    }

    return result;
}

auto utf16_to_utf8(std::u16string_view value) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    if (auto issue = validate_utf16_text(value)) {
        throw TextValidationException(TextEncoding::Utf16, issue->Error, issue->Offset);
    }

    utf8_storage result;
    result.reserve(value.size());

    size_t offset = 0;

    while (offset < value.size()) {
        uint32_t first = value[offset];
        uint32_t scalar = first;

        if (first >= 0xD800 && first <= 0xDBFF) {
            uint32_t second = value[offset + 1];
            scalar = 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
            offset += 2;
        }
        else {
            offset++;
        }

        AppendUtf8Scalar(result, scalar);
    }

    return u8string::FromChecked(result);
}

#if FO_WINDOWS
auto utf16_to_wide(std::u16string_view value) -> wide_string
{
    FO_STACK_TRACE_ENTRY();

    static_assert(sizeof(char16_t) == sizeof(wchar_t));

    if (auto issue = validate_utf16_text(value)) {
        throw TextValidationException(TextEncoding::Utf16, issue->Error, issue->Offset);
    }

    wide_string result;
    result.reserve(value.size());

    for (char16_t code_unit : value) {
        result.push_back(std::bit_cast<wchar_t>(code_unit));
    }

    return result;
}

auto wide_to_utf16(std::wstring_view value) -> utf16_string
{
    FO_STACK_TRACE_ENTRY();

    static_assert(sizeof(char16_t) == sizeof(wchar_t));

    utf16_string result;
    result.reserve(value.size());

    for (wchar_t code_unit : value) {
        result.push_back(std::bit_cast<char16_t>(code_unit));
    }

    if (auto issue = validate_utf16_text(std::u16string_view {result.data(), result.size()})) {
        throw TextValidationException(TextEncoding::Utf16, issue->Error, issue->Offset);
    }

    return result;
}

auto string_to_wide_string(string_view_nt value) -> wide_string
{
    FO_STACK_TRACE_ENTRY();

    const_span<char> storage {value.c_str(), value.size() + 1};
    string_view_nt checked = string_view_nt_from_span(storage);
    wide_string result;
    result.reserve(checked.size());

    for (char code_unit : checked) {
        result.push_back(static_cast<wchar_t>(std::bit_cast<uint8_t>(code_unit)));
    }

    return result;
}

auto utf8_to_wide_string(u8string_view value) -> wide_string
{
    FO_STACK_TRACE_ENTRY();

    utf16_string utf16_value = utf8_to_utf16(value);
    return utf16_to_wide(std::u16string_view {utf16_value.data(), utf16_value.size()});
}
#endif

static auto DecodeUtf8Scalar(std::u8string_view value, size_t& offset) noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint32_t lead = value[offset];
    uint32_t scalar = lead;

    if (lead < 0x80) {
        offset++;
    }
    else if (lead < 0xE0) {
        uint32_t continuation1 = value[offset + 1];
        scalar = ((lead & 0x1F) << 6) | (continuation1 & 0x3F);
        offset += 2;
    }
    else if (lead < 0xF0) {
        uint32_t continuation1 = value[offset + 1];
        uint32_t continuation2 = value[offset + 2];
        scalar = ((lead & 0x0F) << 12) | ((continuation1 & 0x3F) << 6) | (continuation2 & 0x3F);
        offset += 3;
    }
    else {
        uint32_t continuation1 = value[offset + 1];
        uint32_t continuation2 = value[offset + 2];
        uint32_t continuation3 = value[offset + 3];
        scalar = ((lead & 0x07) << 18) | ((continuation1 & 0x3F) << 12) | ((continuation2 & 0x3F) << 6) | (continuation3 & 0x3F);
        offset += 4;
    }

    return scalar;
}

static void AppendUtf16Scalar(utf16_string& output, uint32_t scalar)
{
    FO_STACK_TRACE_ENTRY();

    if (scalar <= 0xFFFF) {
        output.push_back(static_cast<char16_t>(scalar));
    }
    else {
        uint32_t adjusted = scalar - 0x10000;
        output.push_back(static_cast<char16_t>(0xD800 + (adjusted >> 10)));
        output.push_back(static_cast<char16_t>(0xDC00 + (adjusted & 0x3FF)));
    }
}

static void AppendUtf8Scalar(utf8_storage& output, uint32_t scalar)
{
    FO_STACK_TRACE_ENTRY();

    if (scalar <= 0x7F) {
        output.push_back(static_cast<char8_t>(scalar));
    }
    else if (scalar <= 0x7FF) {
        output.push_back(static_cast<char8_t>(0xC0 | (scalar >> 6)));
        output.push_back(static_cast<char8_t>(0x80 | (scalar & 0x3F)));
    }
    else if (scalar <= 0xFFFF) {
        output.push_back(static_cast<char8_t>(0xE0 | (scalar >> 12)));
        output.push_back(static_cast<char8_t>(0x80 | ((scalar >> 6) & 0x3F)));
        output.push_back(static_cast<char8_t>(0x80 | (scalar & 0x3F)));
    }
    else {
        output.push_back(static_cast<char8_t>(0xF0 | (scalar >> 18)));
        output.push_back(static_cast<char8_t>(0x80 | ((scalar >> 12) & 0x3F)));
        output.push_back(static_cast<char8_t>(0x80 | ((scalar >> 6) & 0x3F)));
        output.push_back(static_cast<char8_t>(0x80 | (scalar & 0x3F)));
    }
}

FO_END_NAMESPACE
