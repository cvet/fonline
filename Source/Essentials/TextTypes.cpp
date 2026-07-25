//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
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

#include "TextTypes.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

TextValidationException::TextValidationException(TextEncoding encoding, TextValidationError error, size_t offset) noexcept :
    _encoding {encoding},
    _error {error},
    _offset {offset}
{
    FO_NO_STACK_TRACE_ENTRY();
}

auto TextValidationException::what() const noexcept -> const char*
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (_error) {
    case TextValidationError::NonAsciiByte:
        return "Non-ASCII byte in ASCII text";
    case TextValidationError::InvalidLeadByte:
        return "Invalid UTF-8 lead byte";
    case TextValidationError::InvalidContinuationByte:
        return "Invalid UTF-8 continuation byte";
    case TextValidationError::TruncatedSequence:
        return "Truncated UTF-8 sequence";
    case TextValidationError::OverlongSequence:
        return "Overlong UTF-8 sequence";
    case TextValidationError::SurrogateScalar:
        return "UTF-8 sequence encodes a surrogate scalar";
    case TextValidationError::ScalarOutOfRange:
        return "UTF-8 scalar is out of range";
    case TextValidationError::UnpairedHighSurrogate:
        return "UTF-16 contains an unpaired high surrogate";
    case TextValidationError::UnpairedLowSurrogate:
        return "UTF-16 contains an unpaired low surrogate";
    case TextValidationError::EmbeddedNull:
        return "Terminated text contains an embedded null";
    case TextValidationError::MissingTerminator:
        return "Terminated text is missing its final null";
    }

    return "Unknown text validation error";
}

auto u8string_view::FromChecked(native_view_type value) -> u8string_view
{
    FO_STACK_TRACE_ENTRY();

    if (const auto issue = validate_utf8_text(value)) {
        throw TextValidationException(TextEncoding::Utf8, issue->Error, issue->Offset);
    }

    return u8string_view {value, ValidatedTag {}};
}

auto try_string_view_nt_from_span(const_span<char> storage) noexcept -> optional<string_view_nt>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (storage.empty() || storage.back() != char {}) {
        return std::nullopt;
    }

    const string_view content {storage.data(), storage.size() - 1};

    if (content.find(char {}) != string_view::npos) {
        return std::nullopt;
    }

    return string_view_nt {storage.data(), content.size(), string_view_nt::ValidatedTag {}};
}

auto string_view_nt_from_span(const_span<char> storage) -> string_view_nt
{
    FO_STACK_TRACE_ENTRY();

    if (storage.empty() || storage.back() != char {}) {
        const size_t offset = storage.empty() ? 0 : storage.size() - 1;
        throw TextValidationException(TextEncoding::Ascii, TextValidationError::MissingTerminator, offset);
    }

    const string_view content {storage.data(), storage.size() - 1};

    if (const size_t null_pos = content.find(char {}); null_pos != string_view::npos) {
        throw TextValidationException(TextEncoding::Ascii, TextValidationError::EmbeddedNull, null_pos);
    }

    return string_view_nt {storage.data(), content.size(), string_view_nt::ValidatedTag {}};
}

auto u8string_view_nt::TryFrom(const const_span<char8_t>& storage) noexcept -> optional<u8string_view_nt>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (storage.empty() || storage.back() != char8_t {}) {
        return std::nullopt;
    }

    const const_span<char8_t> content = storage.first(storage.size() - 1);

    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == char8_t {}) {
            return std::nullopt;
        }
    }

    const auto checked = u8string_view::TryFrom(std::u8string_view {content.data(), content.size()});

    if (!checked) {
        return std::nullopt;
    }

    return u8string_view_nt {checked.value(), ValidatedTag {}};
}

auto u8string_view_nt::FromChecked(const const_span<char8_t>& storage) -> u8string_view_nt
{
    FO_STACK_TRACE_ENTRY();

    if (storage.empty() || storage.back() != char8_t {}) {
        const size_t offset = storage.empty() ? 0 : storage.size() - 1;
        throw TextValidationException(TextEncoding::Utf8, TextValidationError::MissingTerminator, offset);
    }

    const const_span<char8_t> content = storage.first(storage.size() - 1);

    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == char8_t {}) {
            throw TextValidationException(TextEncoding::Utf8, TextValidationError::EmbeddedNull, i);
        }
    }

    return u8string_view_nt {u8string_view::FromChecked(std::u8string_view {content.data(), content.size()}), ValidatedTag {}};
}

u8string::u8string(u8string_view value)
{
    FO_STACK_TRACE_ENTRY();

    if (const auto issue = validate_utf8_text(value.native_view())) {
        throw TextValidationException(TextEncoding::Utf8, issue->Error, issue->Offset);
    }

    if (!value.empty()) {
        _storage.assign(value.data(), value.size());
    }
}

u8string::u8string(string_view value)
{
    FO_STACK_TRACE_ENTRY();

    if (const auto issue = validate_ascii_text(value)) {
        throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
    }

    _storage.reserve(value.size());

    for (const char code_unit : value) {
        _storage.push_back(static_cast<char8_t>(static_cast<unsigned char>(code_unit)));
    }
}

u8string::u8string(u8string&& other) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _storage.swap(other._storage);
}

auto u8string::operator=(u8string&& other) noexcept -> u8string&
{
    FO_NO_STACK_TRACE_ENTRY();

    if (this != &other) {
        _storage.clear();
        _storage.swap(other._storage);
    }

    return *this;
}

auto u8string::FromChecked(std::u8string_view value) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    return u8string {u8string_view::FromChecked(value)};
}

auto u8string::size() const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _storage.size();
}

auto u8string::empty() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _storage.empty();
}

auto u8string::view() const& noexcept -> u8string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    return u8string_view {std::u8string_view {_storage.data(), _storage.size()}, u8string_view::ValidatedTag {}};
}

auto u8string::try_view_nt() const& noexcept -> optional<u8string_view_nt>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_storage.find(char8_t {}) != storage_type::npos) {
        return std::nullopt;
    }

    return u8string_view_nt {view(), u8string_view_nt::ValidatedTag {}};
}

auto u8string::view_nt() const& -> u8string_view_nt
{
    FO_STACK_TRACE_ENTRY();

    if (const size_t null_pos = _storage.find(char8_t {}); null_pos != storage_type::npos) {
        throw TextValidationException(TextEncoding::Utf8, TextValidationError::EmbeddedNull, null_pos);
    }

    return u8string_view_nt {view(), u8string_view_nt::ValidatedTag {}};
}

void u8string::clear() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _storage.clear();
}

void u8string::reserve(size_t capacity)
{
    FO_STACK_TRACE_ENTRY();

    _storage.reserve(capacity);
}

auto u8string::assign(u8string_view value) -> u8string&
{
    FO_STACK_TRACE_ENTRY();

    if (const auto issue = validate_utf8_text(value.native_view())) {
        throw TextValidationException(TextEncoding::Utf8, issue->Error, issue->Offset);
    }

    storage_type replacement;

    if (!value.empty()) {
        replacement.assign(value.data(), value.size());
    }

    _storage.swap(replacement);
    return *this;
}

auto u8string::assign(string_view value) -> u8string&
{
    FO_STACK_TRACE_ENTRY();

    if (const auto issue = validate_ascii_text(value)) {
        throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
    }

    storage_type replacement;
    replacement.reserve(value.size());

    for (const char code_unit : value) {
        replacement.push_back(static_cast<char8_t>(static_cast<unsigned char>(code_unit)));
    }

    _storage.swap(replacement);
    return *this;
}

auto u8string::append(u8string_view value) -> u8string&
{
    FO_STACK_TRACE_ENTRY();

    if (const auto issue = validate_utf8_text(value.native_view())) {
        throw TextValidationException(TextEncoding::Utf8, issue->Error, issue->Offset);
    }

    storage_type suffix;

    if (!value.empty()) {
        suffix.assign(value.data(), value.size());
    }

    _storage.append(suffix);
    return *this;
}

auto u8string::append(string_view value) -> u8string&
{
    FO_STACK_TRACE_ENTRY();

    if (const auto issue = validate_ascii_text(value)) {
        throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
    }

    storage_type suffix;
    suffix.reserve(value.size());

    for (const char code_unit : value) {
        suffix.push_back(static_cast<char8_t>(static_cast<unsigned char>(code_unit)));
    }

    _storage.append(suffix);
    return *this;
}

void u8string::swap(u8string& other) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _storage.swap(other._storage);
}

FO_END_NAMESPACE
