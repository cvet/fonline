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

#include "StringUtils.h"
#include "GlobalData.h"
#include "StackTrace.h"
#include "TextConversions.h"
#include "UcsTables.inc"

#if FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

struct StrGlobalData
{
    unordered_set<char> TokSym = {'`', '~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', //
        '+', '-', '=', '|', '\\', '/', '.', ',', '\'', ';', '[', ']', ']', '{', '}', ':', '>', '<', '"'};
};
FO_GLOBAL_DATA(StrGlobalData, StrData);

auto is_uri_scheme_letter(char value) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

auto is_uri_scheme_tail_character(char value) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return is_uri_scheme_letter(value) || (value >= '0' && value <= '9') || value == '+' || value == '-' || value == '.';
}

auto parse_uri_scheme(string_view value) noexcept -> optional<string_view>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (value.empty() || !is_uri_scheme_letter(value.front())) {
        return std::nullopt;
    }

    for (const char code_unit : value.substr(1)) {
        if (!is_uri_scheme_tail_character(code_unit)) {
            return std::nullopt;
        }
    }

    return value;
}

strex::operator string&&()
{
    FO_STACK_TRACE_ENTRY();

    if (const auto issue = validate_ascii_text(_sv)) {
        throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
    }

    own_storage();

    _sv = {};

    return std::move(_s);
}

strex::operator string() const
{
    FO_STACK_TRACE_ENTRY();

    if (const auto issue = validate_ascii_text(_sv)) {
        throw TextValidationException(TextEncoding::Ascii, issue->Error, issue->Offset);
    }

    return string(_sv);
}

strex::operator u8string() const
{
    FO_STACK_TRACE_ENTRY();

    return u8string {_sv};
}

auto strex::str() noexcept -> string&&
{
    FO_NO_STACK_TRACE_ENTRY();

    own_storage();

    _sv = {};

    return std::move(_s);
}

// ReSharper disable once CppInconsistentNaming
auto strex::c_str() noexcept -> const char*
{
    FO_NO_STACK_TRACE_ENTRY();

    own_storage();

    return _s.c_str();
}

void strex::own_storage() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto view_begin = make_nptr(_sv.data());
    auto storage_begin = make_nptr(_s.data());
    nptr<const char> storage_end = storage_begin.offset(_s.size());

    if (view_begin < storage_begin || !(view_begin < storage_end)) {
        _s = _sv;
    }
    else {
        if (!(storage_begin == view_begin)) {
            _s.erase(0, std::bit_cast<size_t>(view_begin.get() - storage_begin.get()));
        }

        if (_s.length() != _sv.length()) {
            _s.resize(_sv.length());
        }
    }

    _sv = _s;
}

auto strvex::length() const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _sv.length();
}

auto strvex::empty() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _sv.empty();
}

auto strvex::compare_ignore_case(string_view other) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_sv.length() != other.length()) {
        return false;
    }

    for (size_t i = 0; i < _sv.length(); i++) {
        if (std::tolower(_sv[i]) != std::tolower(other[i])) {
            return false;
        }
    }

    return true;
}

auto strvex::starts_with(char r) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_sv.empty() && _sv.front() == r;
}

auto strvex::starts_with(string_view r) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _sv.length() >= r.length() && _sv.starts_with(r);
}

auto strvex::ends_with(char r) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_sv.empty() && _sv.back() == r;
}

auto strvex::ends_with(string_view r) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _sv.length() >= r.length() && _sv.ends_with(r);
}

auto strvex::substring_until(char separator) noexcept -> strvex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto pos = _sv.find(separator);

    if (pos != string::npos) {
        _sv = _sv.substr(0, pos);
    }

    return *this;
}

auto strex::substring_until(char separator) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    strvex::substring_until(separator);

    return *this;
}

auto strvex::substring_until(string_view separator) noexcept -> strvex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto pos = _sv.find(separator);

    if (pos != string::npos) {
        _sv = _sv.substr(0, pos);
    }

    return *this;
}

auto strex::substring_until(string_view separator) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    strvex::substring_until(separator);

    return *this;
}

auto strvex::substring_after(char separator) noexcept -> strvex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto pos = _sv.find(separator);

    if (pos != string::npos) {
        _sv = _sv.substr(pos + 1);
    }
    else {
        _sv = {};
    }

    return *this;
}

auto strex::substring_after(char separator) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    strvex::substring_after(separator);

    return *this;
}

auto strvex::substring_after(string_view separator) noexcept -> strvex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto pos = _sv.find(separator);

    if (pos != string::npos) {
        _sv = _sv.substr(pos + separator.length());
    }
    else {
        _sv = {};
    }

    return *this;
}

auto strex::substring_after(string_view separator) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    strvex::substring_after(separator);

    return *this;
}

auto strvex::trim() noexcept -> strvex&
{
    FO_NO_STACK_TRACE_ENTRY();

    trim(" \n\r\t");

    return *this;
}

auto strex::trim() noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    strvex::trim();

    return *this;
}

auto strvex::trim(string_view chars) noexcept -> strvex&
{
    FO_NO_STACK_TRACE_ENTRY();

    ltrim(chars);
    rtrim(chars);

    return *this;
}

auto strex::trim(string_view chars) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    strvex::trim(chars);

    return *this;
}

auto strvex::ltrim(string_view chars) noexcept -> strvex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto l = _sv.find_first_not_of(chars);

    if (l == string::npos) {
        _sv = {};
    }
    else if (l > 0) {
        _sv = _sv.substr(l);
    }

    return *this;
}

auto strex::ltrim(string_view chars) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    strvex::ltrim(chars);

    return *this;
}

auto strvex::rtrim(string_view chars) noexcept -> strvex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto r = _sv.find_last_not_of(chars);

    if (r < _sv.length() - 1) {
        _sv = _sv.substr(0, r + 1);
    }

    return *this;
}

auto strex::rtrim(string_view chars) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    strvex::rtrim(chars);

    return *this;
}

auto strex::erase(char what) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    own_storage();

    std::erase(_s, what);

    _sv = _s;

    return *this;
}

auto strex::erase(char begin, char end) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    own_storage();

    while (true) {
        auto begin_pos = _s.find(begin);

        if (begin_pos == string::npos) {
            break;
        }

        auto end_pos = _s.find(end, begin_pos + 1);

        if (end_pos == string::npos) {
            break;
        }

        _s.erase(begin_pos, end_pos - begin_pos + 1);
    }

    _sv = _s;

    return *this;
}

auto strex::replace(char from, char to) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto pos = _sv.find(from);

    if (pos != string::npos) {
        own_storage();

        auto text_begin = make_nptr(_s.data());
        ptr<char> range_begin = text_begin.offset(pos);
        ptr<char> range_end = text_begin.offset(_s.length());
        auto range = std::ranges::subrange(range_begin.get(), range_end.get());
        std::ranges::replace(range, from, to);
    }

    return *this;
}

auto strex::replace(char from1, char from2, char to) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    const char from_buf[3] = {from1, from2, 0};
    const char to_buf[2] = {to, 0};

    replace(from_buf, to_buf);

    return *this;
}

auto strex::replace(string_view from, string_view to) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto pos = _sv.find(from);

    if (pos != std::string::npos) {
        own_storage();

        while (pos != std::string::npos) {
            _s.replace(pos, from.length(), to);
            pos += to.length();
            pos = _s.find(from, pos);
        }

        _sv = _s;
    }

    return *this;
}

auto strex::lower() noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    own_storage();

    std::ranges::transform(_s, _s.begin(), tolower);

    return *this;
}

auto strex::upper() noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    own_storage();

    std::ranges::transform(_s, _s.begin(), toupper);

    return *this;
}

auto strex::assignVolatile(const volatile char* str, size_t len) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    _s.resize(len);

    for (size_t i = 0; i < len; i++) {
        _s[i] = str[i];
    }

    _sv = _s;

    return *this;
}

auto strex::join(const_span<string_view> parts) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    size_t total_length = _sv.length() * parts.size();

    for (const auto& part : parts) {
        total_length += part.length();
    }

    string result_str;
    result_str.reserve(total_length);

    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) {
            result_str.append(_sv);
        }

        result_str.append(parts[i]);
    }

    _s = std::move(result_str);
    _sv = _s;

    return *this;
}

auto strex::join(const_span<string> parts) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    size_t total_length = _sv.length() * parts.size();

    for (const auto& part : parts) {
        total_length += part.length();
    }

    string result_str;
    result_str.reserve(total_length);

    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) {
            result_str.append(_sv);
        }

        result_str.append(parts[i]);
    }

    _s = std::move(result_str);
    _sv = _s;

    return *this;
}

auto strvex::split(char delimiter) const noexcept -> vector<string_view>
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<string_view> result;

    for (size_t pos = 0;;) {
        size_t end_pos = _sv.find(delimiter, pos);
        string_view entry = _sv.substr(pos, end_pos != string::npos ? end_pos - pos : string::npos);

        if (!entry.empty()) {
            entry = strvex(entry).trim().strv();

            if (!entry.empty()) {
                result.emplace_back(entry);
            }
        }

        if (end_pos != string::npos) {
            pos = end_pos + 1;
        }
        else {
            break;
        }
    }

    return result;
}

auto strex::split(char delimiter) const noexcept -> vector<string>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto vec = strvex::split(delimiter);

    vector<string> result;
    result.reserve(result.size());
    std::ranges::transform(vec, std::back_inserter(result), [](auto&& sv) -> string { return string(sv); });

    return result;
}

auto strvex::split_to_int32(char delimiter) const noexcept -> vector<int32_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<int32_t> result;

    for (size_t pos = 0;;) {
        size_t end_pos = _sv.find(delimiter, pos);
        string_view entry = _sv.substr(pos, end_pos != string::npos ? end_pos - pos : string::npos);

        if (!entry.empty()) {
            entry = strvex(entry).trim().strv();

            if (!entry.empty()) {
                result.emplace_back(strvex(entry).to_int32());
            }
        }

        if (end_pos != string::npos) {
            pos = end_pos + 1;
        }
        else {
            break;
        }
    }

    return result;
}

auto strvex::tokenize() const noexcept -> vector<string_view>
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<string_view> result;

    string_view trimmed_text = strvex(_sv).trim();
    size_t cur_tok_pos = 0;
    size_t cur_tok_len = 0;

    auto flush_tok_if_exists = [&]() noexcept {
        if (cur_tok_len != 0) {
            string_view tok = trimmed_text.substr(cur_tok_pos, cur_tok_len);
            result.emplace_back(tok);
            cur_tok_pos += cur_tok_len;
            cur_tok_len = 0;
        }
    };

    for (char ch : trimmed_text) {
        if (StrData->TokSym.contains(ch)) {
            flush_tok_if_exists();
            cur_tok_len++;
            flush_tok_if_exists();
        }
        else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\\') {
            flush_tok_if_exists();
            cur_tok_pos++;
        }
        else {
            cur_tok_len++;
        }
    }

    flush_tok_if_exists();
    return result;
}

auto strex::tokenize() const noexcept -> vector<string>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto vec = strvex::tokenize();

    vector<string> result;
    result.reserve(result.size());
    std::ranges::transform(vec, std::back_inserter(result), [](auto&& sv) -> string { return string(sv); });

    return result;
}

template<typename T>
static auto ConvertToNumber(string_view sv, T& value) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    size_t len = sv.length();

    if (len == 0) {
        return false;
    }
    if (len > strex::MAX_NUMBER_STRING_LENGTH) {
        return false;
    }

    if constexpr (std::integral<T>) {
        static_assert(std::signed_integral<T>);

        string_view parse_sv = sv;
        int32_t base = 10;
        bool negative = false;

        if (sv[0] == '-') {
            if (len >= 2 && sv[1] == '-') {
                return false;
            }

            parse_sv.remove_prefix(1);
            negative = true;

            if (len >= 3 && sv[1] == '0' && (sv[2] == 'x' || sv[2] == 'X')) {
                parse_sv.remove_prefix(2);
                base = 16;
            }
        }
        else {
            if (len >= 2 && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X')) {
                parse_sv.remove_prefix(2);
                base = 16;
            }
        }

        if (parse_sv.empty()) {
            return false;
        }

        std::make_unsigned_t<T> uvalue;
        auto parse_begin = make_nptr(parse_sv.data());
        ptr<const char> parse_end = parse_begin.offset(parse_sv.size());
        auto result = std::from_chars(parse_begin.get(), parse_end.get(), uvalue, base);
        bool success = result.ec == std::errc() && result.ptr == parse_end.get();
        bool out_of_range = result.ec == std::errc::result_out_of_range;

        if (success) {
            if (negative) {
                if (uvalue >= static_cast<std::make_unsigned_t<T>>(std::numeric_limits<T>::min())) {
                    value = std::numeric_limits<T>::min();
                }
                else {
                    value = -static_cast<T>(uvalue);
                }
            }
            else {
                value = static_cast<T>(uvalue);
            }

            return true;
        }
        else {
            // Out of range
            if (out_of_range) {
                if (negative) {
                    value = std::numeric_limits<T>::min();
                }
                else {
                    value = static_cast<T>(std::numeric_limits<std::make_unsigned_t<T>>::max());
                }

                return true;
            }

            // Try read as float
            if (base == 10) {
                if (float64_t fvalue; ConvertToNumber(sv, fvalue)) {
                    value = static_cast<T>(std::clamp(fvalue, static_cast<float64_t>(std::numeric_limits<T>::min()), static_cast<float64_t>(std::numeric_limits<T>::max())));

                    return true;
                }
            }

            return false;
        }
    }
    else {
        if (((len >= 2 && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X')) || (len >= 3 && sv[0] == '-' && sv[1] == '0' && (sv[2] == 'x' || sv[2] == 'X')))) {
            // Try read as hex integer
            if (int64_t ivalue; ConvertToNumber(sv, ivalue)) {
                value = static_cast<T>(ivalue);

                return true;
            }
            else {
                return false;
            }
        }
        else {
            string_view parse_sv = sv;

            if (parse_sv.back() == 'f') {
                parse_sv.remove_suffix(1);
            }

            if (parse_sv.empty()) {
                return false;
            }

            auto parse_begin = make_nptr(parse_sv.data());
            ptr<const char> parse_end = parse_begin.offset(parse_sv.size());
            auto result = std::from_chars(parse_begin.get(), parse_end.get(), value);
            return result.ec == std::errc() && parse_end == result.ptr && std::isfinite(value);
        }
    }
}

auto strvex::is_number() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_sv.empty()) {
        return false;
    }

    float64_t value;
    bool success = ConvertToNumber(strvex(_sv).trim(), value);
    ignore_unused(value);

    return success;
}

auto strvex::is_non_finite_number() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    string_view parse_sv = strvex(_sv).trim();

    if (parse_sv.empty()) {
        return false;
    }
    if (parse_sv.front() == '+' || parse_sv.front() == '-') {
        parse_sv.remove_prefix(1);
    }
    if (parse_sv.empty()) {
        return false;
    }
    if (parse_sv.size() > 3 && parse_sv.back() == 'f') {
        parse_sv.remove_suffix(1);
    }
    if (strvex(parse_sv).compare_ignore_case("inf") || strvex(parse_sv).compare_ignore_case("infinity") || strvex(parse_sv).compare_ignore_case("nan")) {
        return true;
    }

    return parse_sv.size() > 4 && strvex(parse_sv.substr(0, 4)).compare_ignore_case("nan(") && parse_sv.back() == ')';
}

auto strvex::is_explicit_bool() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (strvex(_sv).trim().compare_ignore_case("true")) {
        return true;
    }
    if (strvex(_sv).trim().compare_ignore_case("false")) {
        return true;
    }

    return false;
}

auto strvex::to_int32() const noexcept -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    int64_t value;
    bool success = ConvertToNumber(strvex(_sv).trim(), value);

    if (success) {
        constexpr int64_t min = static_cast<int64_t>(std::numeric_limits<int32_t>::min());
        constexpr int64_t max = static_cast<int64_t>(std::numeric_limits<int32_t>::max());
        return static_cast<int32_t>(std::clamp(value, min, max));
    }
    else {
        return 0;
    }
}

auto strvex::to_uint32() const noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    int64_t value;
    bool success = ConvertToNumber(strvex(_sv).trim(), value);

    if (success) {
        constexpr int64_t min = static_cast<int64_t>(std::numeric_limits<uint32_t>::min());
        constexpr int64_t max = static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
        return static_cast<uint32_t>(std::clamp(value, min, max));
    }
    else {
        return 0;
    }
}

auto strvex::to_int64() const noexcept -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    int64_t value;
    bool success = ConvertToNumber(strvex(_sv).trim(), value);

    return success ? value : 0;
}

auto strvex::to_float32() const noexcept -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    float64_t value;
    bool success = ConvertToNumber(strvex(_sv).trim(), value);

    return success ? static_cast<float32_t>(value) : 0.0f;
}

auto strvex::to_float64() const noexcept -> float64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    float64_t value;
    bool success = ConvertToNumber(strvex(_sv).trim(), value);

    return success ? value : 0.0;
}

auto strvex::to_bool() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (strvex(_sv).trim().compare_ignore_case("true")) {
        return true;
    }
    if (strvex(_sv).trim().compare_ignore_case("false")) {
        return false;
    }

    return to_int64() != 0;
}

auto strex::format_path() noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    trim();
    normalize_path_slashes();

    own_storage();

    // Erase first './'
    while (_s[0] == '.' && _s[1] == '/') {
        _s.erase(0, 2);
    }

    // Skip first '../'
    uint32_t back_count = 0;

    while (_s.length() >= 3 && _s[0] == '.' && _s[1] == '.' && _s[2] == '/') {
        back_count++;
        _s.erase(0, 3);
    }

    // Replace '/./' to '/'
    while (true) {
        auto pos = _s.find("/./");

        if (pos == string::npos) {
            break;
        }

        _s.replace(pos, 3, "/");
    }

    // Replace 'folder/../' to '/'
    while (true) {
        auto pos = _s.find("/../");

        if (pos == string::npos || pos == 0) {
            break;
        }

        auto pos2 = _s.rfind('/', pos - 1);

        if (pos2 == string::npos) {
            _s.erase(0, pos + 4);
        }
        else {
            _s.erase(pos2 + 1, pos - pos2 + 3);
        }
    }

    // Apply skipped '../'
    for (uint32_t i = 0; i < back_count; i++) {
        _s.insert(0, "../");
    }

    _sv = _s;

    return *this;
}

auto strex::extract_dir() noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    format_path();

    auto pos = _sv.find_last_of('/');

    if (pos != string::npos) {
        _sv = _sv.substr(0, pos);
    }
    else {
        _sv = "";
    }

    return *this;
}

auto strvex::extract_file_name() noexcept -> strvex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto pos = _sv.find_last_of("/\\");

    if (pos != string::npos) {
        _sv = _sv.substr(pos + 1);
    }

    return *this;
}

auto strex::extract_file_name() noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    strvex::extract_file_name();

    return *this;
}

auto strex::get_file_extension() noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto dot = _sv.find_last_of('.');
    _sv = dot != string::npos ? _sv.substr(dot + 1) : "";
    lower();

    return *this;
}

auto strvex::erase_file_extension() noexcept -> strvex&
{
    FO_NO_STACK_TRACE_ENTRY();

    auto dot = _sv.find_last_of('.');

    if (dot != string::npos) {
        _sv = _sv.substr(0, dot);
    }

    return *this;
}

auto strex::erase_file_extension() noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    strvex::erase_file_extension();

    return *this;
}

auto strex::change_file_name(string_view new_name) -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    own_storage();

    string ext = strex(_s).get_file_extension().str();

    if (!ext.empty()) {
        strex new_name_with_ext = strex("{}.{}", new_name, ext);
        _s = strex(_s).extract_dir().combine_path(new_name_with_ext);
    }
    else {
        _s = strex(_s).extract_dir().combine_path(new_name);
    }

    _sv = _s;

    return *this;
}

auto strex::change_file_extension(string_view new_ext) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    erase_file_extension();
    own_storage();

    _s.reserve(_s.size() + new_ext.size() + 1);
    _s += ".";
    _s += new_ext;
    _sv = _s;

    return *this;
}

auto strex::combine_path(string_view path) noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!path.empty()) {
        own_storage();

        if (!_s.empty() && _s.back() != '/' && path.front() != '/') {
            _s += "/";
        }

        _s += path;

        _sv = _s;

        format_path();
    }

    return *this;
}

auto strex::normalize_path_slashes() noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    own_storage();

    std::ranges::replace(_s, '\\', '/');

    return *this;
}

auto strex::normalize_line_endings() noexcept -> strex&
{
    FO_NO_STACK_TRACE_ENTRY();

    replace('\r', '\n', '\n');
    replace('\r', '\n');

    return *this;
}

u8strex::operator u8string&&()
{
    FO_STACK_TRACE_ENTRY();

    own_storage();
    _sv = {};
    return std::move(_s);
}

auto u8strex::str() -> u8string&&
{
    FO_STACK_TRACE_ENTRY();

    own_storage();
    _sv = {};
    return std::move(_s);
}

void u8strex::own_storage()
{
    FO_STACK_TRACE_ENTRY();

    const u8string_view storage_view = _s;

    if (_sv.data() == storage_view.data() && _sv.size() == storage_view.size()) {
        return;
    }

    u8string owned {_sv};
    _s = std::move(owned);
    _sv = _s;
}

auto u8strvex::length() const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _sv.size();
}

auto u8strvex::empty() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _sv.empty();
}

auto u8strvex::compare_ignore_case(u8string_view other) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const std::u8string_view left = _sv.native_view();
    const std::u8string_view right = other.native_view();

    if (left.size() != right.size()) {
        return false;
    }

    for (size_t i = 0; i < left.size();) {
        size_t left_length = left.size() - i;
        const auto left_ucs = utf8::Decode(make_ptr(left.data()).offset(i), left_length);
        size_t right_length = right.size() - i;
        const auto right_ucs = utf8::Decode(make_ptr(right.data()).offset(i), right_length);
        FO_BASIC_STRONG_ASSERT(left_ucs.has_value());
        FO_BASIC_STRONG_ASSERT(right_ucs.has_value());

        if (left_length != right_length || utf8::Lower(*left_ucs) != utf8::Lower(*right_ucs)) {
            return false;
        }

        i += left_length;
    }

    return true;
}

auto u8strvex::starts_with(char8_t value) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_sv.empty() && _sv.native_view().front() == value;
}

auto u8strvex::starts_with(u8string_view value) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _sv.native_view().starts_with(value.native_view());
}

auto u8strvex::ends_with(char8_t value) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_sv.empty() && _sv.native_view().back() == value;
}

auto u8strvex::ends_with(u8string_view value) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _sv.native_view().ends_with(value.native_view());
}

auto u8strvex::length_utf8() const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    size_t length = 0;

    for (const char8_t code_unit : _sv.native_view()) {
        length += (static_cast<uint8_t>(code_unit) & 0xC0) != 0x80 ? 1u : 0u;
    }

    return length;
}

auto u8strvex::is_number() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return strex(utf8_to_string(_sv)).is_number();
}

auto u8strvex::is_non_finite_number() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return strex(utf8_to_string(_sv)).is_non_finite_number();
}

auto u8strvex::is_explicit_bool() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return strex(utf8_to_string(_sv)).is_explicit_bool();
}

auto u8strvex::to_int32() const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return strex(utf8_to_string(_sv)).to_int32();
}

auto u8strvex::to_uint32() const -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    return strex(utf8_to_string(_sv)).to_uint32();
}

auto u8strvex::to_int64() const -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    return strex(utf8_to_string(_sv)).to_int64();
}

auto u8strvex::to_float32() const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    return strex(utf8_to_string(_sv)).to_float32();
}

auto u8strvex::to_float64() const -> float64_t
{
    FO_STACK_TRACE_ENTRY();

    return strex(utf8_to_string(_sv)).to_float64();
}

auto u8strvex::to_bool() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return strex(utf8_to_string(_sv)).to_bool();
}

auto u8strvex::split(char8_t delimiter) const -> vector<u8string_view>
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view source = _sv.native_view();
    vector<u8string_view> result;

    for (size_t pos = 0;;) {
        const size_t end_pos = source.find(delimiter, pos);
        u8string_view entry = u8string_view::FromChecked(source.substr(pos, end_pos != std::u8string_view::npos ? end_pos - pos : std::u8string_view::npos));

        if (!entry.empty()) {
            entry = u8strvex(entry).trim().strv();

            if (!entry.empty()) {
                result.emplace_back(entry);
            }
        }

        if (end_pos == std::u8string_view::npos) {
            break;
        }

        pos = end_pos + 1;
    }

    return result;
}

auto u8strvex::tokenize() const noexcept -> vector<u8string_view>
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<u8string_view> result;

    const u8string_view trimmed_text = u8strvex(_sv).trim();
    const std::u8string_view text = trimmed_text.native_view();
    size_t cur_tok_pos = 0;
    size_t cur_tok_len = 0;

    const auto flush_tok_if_exists = [&]() noexcept {
        if (cur_tok_len != 0) {
            result.emplace_back(u8string_view::FromChecked(text.substr(cur_tok_pos, cur_tok_len)));
            cur_tok_pos += cur_tok_len;
            cur_tok_len = 0;
        }
    };

    for (const char8_t value : text) {
        const bool is_ascii = value <= char8_t {0x7F};
        const char ascii_value = static_cast<char>(value);

        if (is_ascii && StrData->TokSym.contains(ascii_value)) {
            flush_tok_if_exists();
            cur_tok_len++;
            flush_tok_if_exists();
        }
        else if (is_ascii && (ascii_value == ' ' || ascii_value == '\t' || ascii_value == '\r' || ascii_value == '\n' || ascii_value == '\\')) {
            flush_tok_if_exists();
            cur_tok_pos++;
        }
        else {
            cur_tok_len++;
        }
    }

    flush_tok_if_exists();
    return result;
}

auto u8strex::split(char8_t delimiter) const -> vector<u8string>
{
    FO_STACK_TRACE_ENTRY();

    const vector<u8string_view> views = u8strvex::split(delimiter);
    vector<u8string> result;
    result.reserve(views.size());

    for (const u8string_view view : views) {
        result.emplace_back(view);
    }

    return result;
}

auto u8strex::tokenize() const -> vector<u8string>
{
    FO_STACK_TRACE_ENTRY();

    const vector<u8string_view> views = u8strvex::tokenize();
    vector<u8string> result;
    result.reserve(views.size());

    for (const u8string_view view : views) {
        result.emplace_back(view);
    }

    return result;
}

auto u8strvex::substring_until(char8_t separator) -> u8strvex&
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view source = _sv.native_view();
    const size_t pos = source.find(separator);

    if (pos != std::u8string_view::npos) {
        _sv = u8string_view::FromChecked(source.substr(0, pos));
    }

    return *this;
}

auto u8strex::substring_until(char8_t separator) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    u8strvex::substring_until(separator);
    return *this;
}

auto u8strvex::substring_until(u8string_view separator) -> u8strvex&
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view source = _sv.native_view();
    const size_t pos = source.find(separator.native_view());

    if (pos != std::u8string_view::npos) {
        _sv = u8string_view::FromChecked(source.substr(0, pos));
    }

    return *this;
}

auto u8strex::substring_until(u8string_view separator) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    u8strvex::substring_until(separator);
    return *this;
}

auto u8strvex::substring_after(char8_t separator) -> u8strvex&
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view source = _sv.native_view();
    const size_t pos = source.find(separator);
    _sv = pos != std::u8string_view::npos ? u8string_view::FromChecked(source.substr(pos + 1)) : u8string_view {};
    return *this;
}

auto u8strex::substring_after(char8_t separator) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    u8strvex::substring_after(separator);
    return *this;
}

auto u8strvex::substring_after(u8string_view separator) -> u8strvex&
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view source = _sv.native_view();
    const size_t pos = source.find(separator.native_view());
    _sv = pos != std::u8string_view::npos ? u8string_view::FromChecked(source.substr(pos + separator.size())) : u8string_view {};
    return *this;
}

auto u8strex::substring_after(u8string_view separator) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    u8strvex::substring_after(separator);
    return *this;
}

auto u8strvex::trim() -> u8strvex&
{
    FO_STACK_TRACE_ENTRY();

    return trim(u8" \n\r\t");
}

auto u8strex::trim() -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    u8strvex::trim();
    return *this;
}

auto u8strvex::trim(u8string_view chars) -> u8strvex&
{
    FO_STACK_TRACE_ENTRY();

    ltrim(chars);
    rtrim(chars);
    return *this;
}

auto u8strex::trim(u8string_view chars) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    u8strvex::trim(chars);
    return *this;
}

auto u8strvex::ltrim(u8string_view chars) -> u8strvex&
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view source = _sv.native_view();
    const std::u8string_view trim_chars = chars.native_view();
    const auto contains_trim_char = [trim_chars](uint32_t value) noexcept {
        for (size_t i = 0; i < trim_chars.size();) {
            size_t length = trim_chars.size() - i;
            const auto code_point = utf8::Decode(make_ptr(trim_chars.data()).offset(i), length);
            FO_BASIC_STRONG_ASSERT(code_point.has_value());

            if (*code_point == value) {
                return true;
            }

            i += length;
        }

        return false;
    };

    size_t pos = 0;
    while (pos < source.size()) {
        size_t length = source.size() - pos;
        const auto code_point = utf8::Decode(make_ptr(source.data()).offset(pos), length);
        FO_BASIC_STRONG_ASSERT(code_point.has_value());

        if (!contains_trim_char(*code_point)) {
            break;
        }

        pos += length;
    }

    _sv = u8string_view::FromChecked(source.substr(pos));
    return *this;
}

auto u8strex::ltrim(u8string_view chars) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    u8strvex::ltrim(chars);
    return *this;
}

auto u8strvex::rtrim(u8string_view chars) -> u8strvex&
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view source = _sv.native_view();
    const std::u8string_view trim_chars = chars.native_view();
    const auto contains_trim_char = [trim_chars](uint32_t value) noexcept {
        for (size_t i = 0; i < trim_chars.size();) {
            size_t length = trim_chars.size() - i;
            const auto code_point = utf8::Decode(make_ptr(trim_chars.data()).offset(i), length);
            FO_BASIC_STRONG_ASSERT(code_point.has_value());

            if (*code_point == value) {
                return true;
            }

            i += length;
        }

        return false;
    };

    size_t end_pos = 0;
    for (size_t pos = 0; pos < source.size();) {
        size_t length = source.size() - pos;
        const auto code_point = utf8::Decode(make_ptr(source.data()).offset(pos), length);
        FO_BASIC_STRONG_ASSERT(code_point.has_value());

        if (!contains_trim_char(*code_point)) {
            end_pos = pos + length;
        }

        pos += length;
    }

    _sv = u8string_view::FromChecked(source.substr(0, end_pos));
    return *this;
}

auto u8strex::rtrim(u8string_view chars) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    u8strvex::rtrim(chars);
    return *this;
}

auto u8strex::erase(char8_t what) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    mutate_storage([what](auto& storage) { std::erase(storage, what); });
    return *this;
}

auto u8strex::erase(char8_t begin, char8_t end) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    mutate_storage([begin, end](auto& storage) {
        while (true) {
            const size_t begin_pos = storage.find(begin);

            if (begin_pos == storage.npos) {
                break;
            }

            const size_t end_pos = storage.find(end, begin_pos + 1);

            if (end_pos == storage.npos) {
                break;
            }

            storage.erase(begin_pos, end_pos - begin_pos + 1);
        }
    });
    return *this;
}

auto u8strex::replace(char8_t from, char8_t to) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    mutate_storage([from, to](auto& storage) { std::ranges::replace(storage, from, to); });
    return *this;
}

auto u8strex::replace(char8_t from1, char8_t from2, char8_t to) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    const char8_t from[] = {from1, from2, u8'\0'};
    const char8_t replacement[] = {to, u8'\0'};
    return replace(u8string_view::FromChecked(std::u8string_view {from, 2}), u8string_view::FromChecked(std::u8string_view {replacement, 1}));
}

auto u8strex::replace(u8string_view from, u8string_view to) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view from_view = from.native_view();
    const std::u8string_view to_view = to.native_view();

    if (from_view.empty()) {
        return *this;
    }

    mutate_storage([from_view, to_view](auto& storage) {
        size_t pos = storage.find(from_view);

        while (pos != storage.npos) {
            storage.replace(pos, from_view.size(), to_view);
            pos = storage.find(from_view, pos + to_view.size());
        }
    });
    return *this;
}

auto u8strex::lower() -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    mutate_storage([](auto& storage) {
        for (size_t i = 0; i < storage.size();) {
            size_t length = storage.size() - i;
            const auto decoded = utf8::Decode(make_ptr(storage.data()).offset(i), length);
            FO_BASIC_STRONG_ASSERT(decoded.has_value());

            char8_t encoded[4];
            const auto encoded_length = utf8::Encode(utf8::Lower(*decoded), encoded);
            FO_BASIC_STRONG_ASSERT(encoded_length.has_value());
            storage.replace(i, length, encoded, *encoded_length);
            i += *encoded_length;
        }
    });
    return *this;
}

auto u8strex::upper() -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    mutate_storage([](auto& storage) {
        for (size_t i = 0; i < storage.size();) {
            size_t length = storage.size() - i;
            const auto decoded = utf8::Decode(make_ptr(storage.data()).offset(i), length);
            FO_BASIC_STRONG_ASSERT(decoded.has_value());

            char8_t encoded[4];
            const auto encoded_length = utf8::Encode(utf8::Upper(*decoded), encoded);
            FO_BASIC_STRONG_ASSERT(encoded_length.has_value());
            storage.replace(i, length, encoded, *encoded_length);
            i += *encoded_length;
        }
    });
    return *this;
}

auto u8strex::format_path() -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    trim();
    normalize_path_slashes();

    mutate_storage([](auto& storage) {
        while (storage.size() >= 2 && storage[0] == u8'.' && storage[1] == u8'/') {
            storage.erase(0, 2);
        }

        uint32_t back_count = 0;

        while (storage.size() >= 3 && storage[0] == u8'.' && storage[1] == u8'.' && storage[2] == u8'/') {
            back_count++;
            storage.erase(0, 3);
        }

        while (true) {
            const size_t pos = storage.find(u8"/./");

            if (pos == storage.npos) {
                break;
            }

            storage.replace(pos, 3, u8"/");
        }

        while (true) {
            const size_t pos = storage.find(u8"/../");

            if (pos == storage.npos || pos == 0) {
                break;
            }

            const size_t previous = storage.rfind(u8'/', pos - 1);

            if (previous == storage.npos) {
                storage.erase(0, pos + 4);
            }
            else {
                storage.erase(previous + 1, pos - previous + 3);
            }
        }

        for (uint32_t i = 0; i < back_count; i++) {
            storage.insert(0, u8"../");
        }
    });
    return *this;
}

auto u8strex::extract_dir() -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    format_path();
    const std::u8string_view source = _sv.native_view();
    const size_t pos = source.find_last_of(u8'/');
    _sv = pos != std::u8string_view::npos ? u8string_view::FromChecked(source.substr(0, pos)) : u8string_view {};
    return *this;
}

auto u8strvex::extract_file_name() -> u8strvex&
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view source = _sv.native_view();
    const size_t pos = source.find_last_of(u8"/\\");

    if (pos != std::u8string_view::npos) {
        _sv = u8string_view::FromChecked(source.substr(pos + 1));
    }

    return *this;
}

auto u8strex::extract_file_name() -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    u8strvex::extract_file_name();
    return *this;
}

auto u8strex::get_file_extension() -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view source = _sv.native_view();
    const size_t dot = source.find_last_of(u8'.');
    _sv = dot != std::u8string_view::npos ? u8string_view::FromChecked(source.substr(dot + 1)) : u8string_view {};
    return lower();
}

auto u8strvex::erase_file_extension() -> u8strvex&
{
    FO_STACK_TRACE_ENTRY();

    const std::u8string_view source = _sv.native_view();
    const size_t dot = source.find_last_of(u8'.');

    if (dot != std::u8string_view::npos) {
        _sv = u8string_view::FromChecked(source.substr(0, dot));
    }

    return *this;
}

auto u8strex::erase_file_extension() -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    u8strvex::erase_file_extension();
    return *this;
}

auto u8strex::change_file_name(u8string_view new_name) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    const u8string extension = u8strex(_sv).get_file_extension();

    if (!extension.empty()) {
        const u8string name_with_extension = u8strex(u8"{}.{}", new_name, extension);
        _s = u8strex(_sv).extract_dir().combine_path(name_with_extension);
    }
    else {
        _s = u8strex(_sv).extract_dir().combine_path(new_name);
    }

    _sv = _s;
    return *this;
}

auto u8strex::change_file_name(string_view new_name) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_name = new_name;
    return change_file_name(utf8_name);
}

auto u8strex::change_file_extension(u8string_view new_ext) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    erase_file_extension();
    own_storage();
    _s.append(".");
    _s.append(new_ext);
    _sv = _s;
    return *this;
}

auto u8strex::change_file_extension(string_view new_ext) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_ext = new_ext;
    return change_file_extension(utf8_ext);
}

auto u8strex::combine_path(u8string_view path) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    if (path.empty()) {
        return *this;
    }

    own_storage();
    const std::u8string_view path_view = path.native_view();

    if (!_s.empty() && _s.view().native_view().back() != u8'/' && path_view.front() != u8'/') {
        _s.append("/");
    }

    _s.append(path);
    _sv = _s;
    return format_path();
}

auto u8strex::combine_path(string_view path) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    const u8string utf8_path = path;
    return combine_path(utf8_path);
}

auto u8strex::normalize_path_slashes() -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    return replace(u8'\\', u8'/');
}

auto u8strex::normalize_line_endings() -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    replace(u8'\r', u8'\n', u8'\n');
    replace(u8'\r', u8'\n');
    return *this;
}

#if FO_WINDOWS
auto u8strex::parse_wide_char(ptr<const wchar_t> str) -> u8strex&
{
    FO_STACK_TRACE_ENTRY();

    own_storage();
    _s.append(utf16_to_utf8(wide_to_utf16(std::wstring_view {str.get()})));
    _sv = _s;
    return *this;
}

auto strex::to_wide_char() const noexcept -> wstring
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_sv.empty()) {
        return L"";
    }
    if (_sv.length() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        return {};
    }

    auto source = make_nptr(_sv.data());
    int32_t input_len = static_cast<int32_t>(_sv.length());
    int32_t wide_len = ::MultiByteToWideChar(CP_UTF8, 0, source.get(), input_len, nullptr, 0);

    if (wide_len <= 0) {
        return {};
    }

    wstring result;
    result.resize(static_cast<size_t>(wide_len));

    int32_t written_len = ::MultiByteToWideChar(CP_UTF8, 0, source.get(), input_len, result.data(), wide_len);

    if (written_len <= 0) {
        return {};
    }
    if (written_len != wide_len) {
        result.resize(static_cast<size_t>(written_len));
    }

    return result;
}

auto u8strex::to_wide_char() const -> wstring
{
    FO_STACK_TRACE_ENTRY();

    return utf16_to_wide(utf8_to_utf16(_sv));
}
#endif

// ReSharper restore CppInconsistentNaming

auto utf8::IsValid(uint32_t ucs) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return ucs <= 0x10FFFF && (ucs < 0xD800 || ucs > 0xDFFF);
}

auto utf8::DecodeStrNtLen(ptr<const char> str) noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    size_t length = 0;

    if (str[0] != 0) {
        length++;

        if (str[1] != 0) {
            length++;

            if (str[2] != 0) {
                length++;

                if (str[3] != 0) {
                    length++;
                }
            }
        }
    }

    return length;
}

auto utf8::Decode(ptr<const char> str, size_t& length) noexcept -> optional<uint32_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (length == 0) {
        return std::nullopt;
    }

    const auto make_result = [&length](uint32_t ch, size_t ch_length) noexcept -> optional<uint32_t> {
        length = ch_length;

        if (!utf8::IsValid(ch)) {
            return std::nullopt;
        }

        return ch;
    };

    const auto make_error = [&length]() noexcept -> optional<uint32_t> {
        length = 1;
        return std::nullopt;
    };

    auto bytes = str.reinterpret_as<const uint8_t>();
    uint8_t c = bytes[0];

    if (c < 0x80) {
        return make_result(c, 1);
    }

    if (c < 0xc2) {
        return make_error();
    }

    if (length < 2) {
        return make_error();
    }

    if ((bytes[1] & 0xc0) != 0x80) {
        return make_error();
    }

    if (c < 0xe0) {
        return make_result(((bytes[0] & 0x1f) << 6) + (bytes[1] & 0x3f), 2);
    }

    if (length < 3) {
        return make_error();
    }

    if (c == 0xe0) {
        if (bytes[1] < 0xa0) {
            return make_error();
        }

        if ((bytes[2] & 0xc0) != 0x80) {
            return make_error();
        }

        return make_result(((bytes[0] & 0x0f) << 12) + ((bytes[1] & 0x3f) << 6) + (bytes[2] & 0x3f), 3);
    }

    if (c < 0xf0) {
        if ((bytes[2] & 0xc0) != 0x80) {
            return make_error();
        }

        return make_result(((bytes[0] & 0x0f) << 12) + ((bytes[1] & 0x3f) << 6) + (bytes[2] & 0x3f), 3);
    }

    if (length < 4) {
        return make_error();
    }

    if (c == 0xf0) {
        if (bytes[1] < 0x90) {
            return make_error();
        }

        if ((bytes[2] & 0xc0) != 0x80 || (bytes[3] & 0xc0) != 0x80) {
            return make_error();
        }

        return make_result(((bytes[0] & 0x07) << 18) + ((bytes[1] & 0x3f) << 12) + ((bytes[2] & 0x3f) << 6) + (bytes[3] & 0x3f), 4);
    }

    if (c < 0xf4) {
        if ((bytes[2] & 0xc0) != 0x80 || (bytes[3] & 0xc0) != 0x80) {
            return make_error();
        }

        return make_result(((bytes[0] & 0x07) << 18) + ((bytes[1] & 0x3f) << 12) + ((bytes[2] & 0x3f) << 6) + (bytes[3] & 0x3f), 4);
    }

    if (c == 0xf4) {
        if (bytes[1] > 0x8f) {
            return make_error();
        }

        if ((bytes[2] & 0xc0) != 0x80 || (bytes[3] & 0xc0) != 0x80) {
            return make_error();
        }

        return make_result(((bytes[0] & 0x07) << 18) + ((bytes[1] & 0x3f) << 12) + ((bytes[2] & 0x3f) << 6) + (bytes[3] & 0x3f), 4);
    }

    return make_error();
}

auto utf8::Decode(ptr<const char8_t> str, size_t& length) noexcept -> optional<uint32_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    return Decode(str.reinterpret_as<const char>(), length);
}

auto utf8::Encode(uint32_t ucs, char (&buf)[4]) noexcept -> optional<size_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsValid(ucs)) {
        return std::nullopt;
    }

    if (ucs < 0x000080u) {
        buf[0] = static_cast<char>(ucs);
        return 1;
    }

    if (ucs < 0x000800u) {
        buf[0] = static_cast<char>(0xc0 | (ucs >> 6));
        buf[1] = static_cast<char>(0x80 | (ucs & 0x3F));
        return 2;
    }

    if (ucs < 0x010000u) {
        buf[0] = static_cast<char>(0xe0 | (ucs >> 12));
        buf[1] = static_cast<char>(0x80 | ((ucs >> 6) & 0x3F));
        buf[2] = static_cast<char>(0x80 | (ucs & 0x3F));
        return 3;
    }

    buf[0] = static_cast<char>(0xf0 | (ucs >> 18));
    buf[1] = static_cast<char>(0x80 | ((ucs >> 12) & 0x3F));
    buf[2] = static_cast<char>(0x80 | ((ucs >> 6) & 0x3F));
    buf[3] = static_cast<char>(0x80 | (ucs & 0x3F));

    return 4;
}

auto utf8::Encode(uint32_t ucs, char8_t (&buf)[4]) noexcept -> optional<size_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    char char_buf[4];
    const auto length = Encode(ucs, char_buf);

    if (length) {
        for (size_t i = 0; i < *length; i++) {
            buf[i] = static_cast<char8_t>(static_cast<unsigned char>(char_buf[i]));
        }
    }

    return length;
}

auto utf8::Lower(uint32_t ucs) noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint32_t ret;

    if (ucs <= 0x02B6) {
        if (ucs >= 0x0041) {
            ret = UCS_TABLE_0041[ucs - 0x0041];
            if (ret > 0) {
                return ret;
            }
        }

        return ucs;
    }

    if (ucs <= 0x0556) {
        if (ucs >= 0x0386) {
            ret = UCS_TABLE_0386[ucs - 0x0386];
            if (ret > 0) {
                return ret;
            }
        }

        return ucs;
    }

    if (ucs <= 0x10C5) {
        if (ucs >= 0x10A0) {
            ret = UCS_TABLE_10_A0[ucs - 0x10A0];
            if (ret > 0) {
                return ret;
            }
        }

        return ucs;
    }

    if (ucs <= 0x1FFC) {
        if (ucs >= 0x1E00) {
            ret = UCS_TABLE_1_E00[ucs - 0x1E00];
            if (ret > 0) {
                return ret;
            }
        }

        return ucs;
    }

    if (ucs <= 0x2133) {
        if (ucs >= 0x2102) {
            ret = UCS_TABLE_2102[ucs - 0x2102];
            if (ret > 0) {
                return ret;
            }
        }

        return ucs;
    }

    if (ucs <= 0x24CF) {
        if (ucs >= 0x24B6) {
            ret = UCS_TABLE_24_B6[ucs - 0x24B6];
            if (ret > 0) {
                return ret;
            }
        }

        return ucs;
    }

    if (ucs <= 0x33CE) {
        if (ucs >= 0x33CE) {
            ret = UCS_TABLE_33_CE[ucs - 0x33CE];
            if (ret > 0) {
                return ret;
            }
        }

        return ucs;
    }

    if (ucs <= 0xFF3A) {
        if (ucs >= 0xFF21) {
            ret = UCS_TABLE_FF21[ucs - 0xFF21];
            if (ret > 0) {
                return ret;
            }
        }

        return ucs;
    }

    return ucs;
}

struct Utf8Data
{
    Utf8Data() noexcept
    {
        FO_STACK_TRACE_ENTRY();

        UpperTable.resize(0x10000);

        for (uint32_t i = 0; i < 0x10000; i++) {
            UpperTable[i] = static_cast<uint16_t>(i);
        }

        for (uint32_t i = 0; i < 0x10000; i++) {
            uint32_t l = utf8::Lower(i);

            if (l != i) {
                UpperTable[l] = static_cast<uint16_t>(i);
            }
        }
    }

    vector<uint16_t> UpperTable {};
};
FO_GLOBAL_DATA(Utf8Data, Utf8);

auto utf8::Upper(uint32_t ucs) noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (ucs >= 0x10000) {
        return ucs;
    }

    return Utf8->UpperTable[ucs];
}

FO_END_NAMESPACE
