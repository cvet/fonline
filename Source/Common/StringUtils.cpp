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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GenericUtils.h"
#include "UcsTables-Include.h"
#include "WinApi-Include.h"

// ReSharper disable CppInconsistentNaming

StringHelper::operator string&&()
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    _sv = {};

    return std::move(_s);
}

auto StringHelper::str() -> string&&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    _sv = {};

    return std::move(_s);
}

auto StringHelper::c_str() -> const char*
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    return _s.c_str();
}

void StringHelper::OwnStorage()
{
    NO_STACK_TRACE_ENTRY();

    if (_sv.data() < _s.data() || _sv.data() >= _s.data() + _s.size()) {
        _s = _sv;
    }
    else {
        if (_s.data() != _sv.data()) {
            _s.erase(0, static_cast<size_t>(_sv.data() - _s.data()));
        }

        if (_s.length() != _sv.length()) {
            _s.resize(_sv.length());
        }
    }

    _sv = _s;
}

auto StringHelper::length() const noexcept -> size_t
{
    NO_STACK_TRACE_ENTRY();

    return _sv.length();
}

auto StringHelper::empty() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _sv.empty();
}

auto StringHelper::compareIgnoreCase(string_view other) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

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

auto StringHelper::compareIgnoreCaseUtf8(string_view other) const -> bool
{
    NO_STACK_TRACE_ENTRY();

    if (_sv.length() != other.length()) {
        return false;
    }

    return StringHelper(_sv).lowerUtf8() == StringHelper(other).lowerUtf8();
}

auto StringHelper::startsWith(char r) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _sv.length() >= 1 && _sv.front() == r;
}

auto StringHelper::startsWith(string_view r) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _sv.length() >= r.length() && std::memcmp(_sv.data(), r.data(), r.length()) == 0;
}

auto StringHelper::endsWith(char r) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _sv.length() >= 1 && _sv.back() == r;
}

auto StringHelper::endsWith(string_view r) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _sv.length() >= r.length() && _sv.compare(_sv.length() - r.length(), r.length(), r) == 0;
}

auto StringHelper::isValidUtf8() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    if (_sv.empty()) {
        return true;
    }

    for (size_t i = 0; i < _sv.length();) {
        size_t length = _sv.length() - i;
        const auto ucs = utf8::Decode(_sv.data() + i, length);

        if (!utf8::IsValid(ucs)) {
            return false;
        }

        i += length;
    }

    return true;
}

auto StringHelper::lengthUtf8() const noexcept -> size_t
{
    NO_STACK_TRACE_ENTRY();

    size_t length = 0;

    for (size_t i = 0; i < _sv.length(); i++) {
        length += static_cast<uint>((_sv[i] & 0xC0) != 0x80);
    }

    return length;
}

auto StringHelper::substringUntil(char separator) noexcept -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    const auto pos = _sv.find(separator);

    if (pos != string::npos) {
        _sv = _sv.substr(0, pos);
    }

    return *this;
}

auto StringHelper::substringUntil(string_view separator) noexcept -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    const auto pos = _sv.find(separator);

    if (pos != string::npos) {
        _sv = _sv.substr(0, pos);
    }

    return *this;
}

auto StringHelper::substringAfter(char separator) noexcept -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    const auto pos = _sv.find(separator);

    if (pos != string::npos) {
        _sv = _sv.substr(pos + 1);
    }
    else {
        _sv = {};
    }

    return *this;
}

auto StringHelper::substringAfter(string_view separator) noexcept -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    const auto pos = _sv.find(separator);

    if (pos != string::npos) {
        _sv = _sv.substr(pos + separator.length());
    }
    else {
        _sv = {};
    }

    return *this;
}

auto StringHelper::trim() noexcept -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    // Left trim
    const auto l = _sv.find_first_not_of(" \n\r\t");

    if (l == string::npos) {
        _sv = {};
    }
    else {
        if (l > 0) {
            _sv = _sv.substr(l);
        }

        // Right trim
        const auto r = _sv.find_last_not_of(" \n\r\t");

        if (r < _sv.length() - 1) {
            _sv = _sv.substr(0, r + 1);
        }
    }

    return *this;
}

auto StringHelper::erase(char what) -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    _s.erase(std::remove(_s.begin(), _s.end(), what), _s.end());

    _sv = _s;

    return *this;
}

auto StringHelper::erase(char begin, char end) -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    while (true) {
        const auto begin_pos = _s.find(begin);

        if (begin_pos == string::npos) {
            break;
        }

        const auto end_pos = _s.find(end, begin_pos + 1);

        if (end_pos == string::npos) {
            break;
        }

        _s.erase(begin_pos, end_pos - begin_pos + 1);
    }

    _sv = _s;

    return *this;
}

auto StringHelper::replace(char from, char to) -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    std::replace(_s.begin(), _s.end(), from, to);

    return *this;
}

auto StringHelper::replace(char from1, char from2, char to) -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    const char from_buf[3] = {from1, from2, 0};
    const char to_buf[2] = {to, 0};

    replace(from_buf, to_buf);

    return *this;
}

auto StringHelper::replace(string_view from, string_view to) -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    size_t pos = 0;

    while ((pos = _s.find(from, pos)) != std::string::npos) {
        _s.replace(pos, from.length(), to);
        pos += to.length();
    }

    _sv = _s;

    return *this;
}

auto StringHelper::lower() -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    std::transform(_s.begin(), _s.end(), _s.begin(), tolower);

    return *this;
}

auto StringHelper::upper() -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    std::transform(_s.begin(), _s.end(), _s.begin(), toupper);

    return *this;
}

auto StringHelper::lowerUtf8() -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    for (size_t i = 0; i < _s.length();) {
        size_t length = _s.length() - i;
        auto ucs = utf8::Decode(_s.c_str() + i, length);

        ucs = utf8::Lower(ucs);

        char buf[4];
        const auto new_length = utf8::Encode(ucs, buf);

        _s.replace(i, length, buf, new_length);

        i += new_length;
    }

    _sv = _s;

    return *this;
}

auto StringHelper::upperUtf8() -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    for (size_t i = 0; i < _s.length();) {
        size_t length = _s.length() - i;
        auto ucs = utf8::Decode(_s.c_str() + i, length);

        ucs = utf8::Upper(ucs);

        char buf[4];
        const auto new_length = utf8::Encode(ucs, buf);

        _s.replace(i, length, buf, new_length);

        i += new_length;
    }

    _sv = _s;

    return *this;
}

auto StringHelper::split(char delimiter) const -> vector<string>
{
    NO_STACK_TRACE_ENTRY();

    vector<string> result;

    for (size_t pos = 0;;) {
        const size_t end_pos = _sv.find(delimiter, pos);
        string_view entry = _sv.substr(pos, end_pos != string::npos ? end_pos - pos : string::npos);

        if (!entry.empty()) {
            entry = StringHelper(entry).trim().strv();

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

auto StringHelper::splitToInt(char delimiter) const -> vector<int>
{
    NO_STACK_TRACE_ENTRY();

    vector<int> result;

    for (size_t pos = 0;;) {
        const size_t end_pos = _sv.find(delimiter, pos);
        string_view entry = _sv.substr(pos, end_pos != string::npos ? end_pos - pos : string::npos);

        if (!entry.empty()) {
            entry = StringHelper(entry).trim().strv();

            if (!entry.empty()) {
                result.emplace_back(StringHelper(entry).toInt());
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

#if defined(_MSC_VER) && _MSC_VER >= 1924
#define USE_FROM_CHARS 1
#else
#define USE_FROM_CHARS 0
#endif

template<typename T>
static auto ConvertToNumber(string_view sv, T& value) noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const size_t len = sv.length();

    if (len == 0) {
        return false;
    }
    if (len > StringHelper::MAX_NUMBER_STRING_LENGTH) {
        return false;
    }

    if constexpr (std::is_integral_v<T>) {
        static_assert(std::is_signed_v<T>);

        const char* ptr = sv.data();
        const char* end_ptr = ptr + len;
        int base = 10;
        bool negative = false;

        if (sv[0] == '-') {
            if (len >= 2 && sv[1] == '-') {
                return false;
            }

            ptr += 1;
            negative = true;

            if (len >= 3 && sv[1] == '0' && (sv[2] == 'x' || sv[2] == 'X')) {
                ptr += 2;
                base = 16;
            }
        }
        else {
            if (len >= 2 && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X')) {
                ptr += 2;
                base = 16;
            }
        }

        if (ptr == end_ptr) {
            return false;
        }

        std::make_unsigned_t<T> uvalue;
        bool success;
        bool out_of_range;

        if constexpr (USE_FROM_CHARS) {
            const auto result = std::from_chars(ptr, end_ptr, uvalue, base);
            success = result.ec == std::errc() && result.ptr == end_ptr;
            out_of_range = result.ec == std::errc::result_out_of_range;
        }
        else {
            // Assume all our strings are null terminated
            if (*end_ptr != 0) {
                const auto count = static_cast<size_t>(end_ptr - ptr);
                array<char, StringHelper::MAX_NUMBER_STRING_LENGTH + 1> str_nt;
                std::memcpy(str_nt.data(), ptr, count);
                str_nt[count] = 0;

                char* result_end_ptr;
                uvalue = std::strtoull(str_nt.data(), &result_end_ptr, base);
                success = result_end_ptr == str_nt.data() + count;
                out_of_range = uvalue == ULLONG_MAX && errno == ERANGE;
            }
            else {
                const auto count = static_cast<size_t>(end_ptr - ptr);
                char* result_end_ptr;
                uvalue = std::strtoull(ptr, &result_end_ptr, base);
                success = result_end_ptr == ptr + count;
                out_of_range = uvalue == ULLONG_MAX && errno == ERANGE;
            }
        }

        if (success) {
            if (negative) {
                if (uvalue > static_cast<std::make_unsigned_t<T>>(std::numeric_limits<T>::min())) {
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
                if (double fvalue; ConvertToNumber(sv, fvalue)) {
                    value = static_cast<T>(std::clamp(fvalue, static_cast<double>(std::numeric_limits<T>::min()), static_cast<double>(std::numeric_limits<T>::max())));

                    return true;
                }
            }

            return false;
        }
    }
    else {
        if (((len >= 2 && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X')) || (len >= 3 && sv[0] == '-' && sv[1] == '0' && (sv[2] == 'x' || sv[2] == 'X')))) {
            // Try read as hex integer
            if (int64 ivalue; ConvertToNumber(sv, ivalue)) {
                value = static_cast<T>(ivalue);

                return true;
            }
            else {
                return false;
            }
        }
        else {
            const char* ptr = sv.data();
            const char* end_ptr = ptr + len;

            if (sv.back() == 'f') {
                end_ptr -= 1;
            }

            if (ptr == end_ptr) {
                return false;
            }

            if constexpr (USE_FROM_CHARS) {
                const auto result = std::from_chars(ptr, end_ptr, value);

                return result.ec == std::errc() && result.ptr == end_ptr;
            }
            else {
                // Assume all our strings are null terminated
                if (*end_ptr != 0) {
                    const auto count = static_cast<size_t>(end_ptr - ptr);
                    array<char, StringHelper::MAX_NUMBER_STRING_LENGTH + 1> str_nt;
                    std::memcpy(str_nt.data(), ptr, count);
                    str_nt[count] = 0;

                    char* result_end_ptr;
                    value = std::strtod(str_nt.data(), &result_end_ptr);

                    return result_end_ptr == str_nt.data() + count;
                }
                else {
                    const auto count = static_cast<size_t>(end_ptr - ptr);
                    char* result_end_ptr;
                    value = std::strtod(ptr, &result_end_ptr);

                    return result_end_ptr == ptr + count;
                }
            }
        }
    }
}

#undef USE_FROM_CHARS

auto StringHelper::isNumber() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    if (_sv.empty()) {
        return false;
    }

    double value;
    const auto success = ConvertToNumber(StringHelper(_sv).trim(), value);
    UNUSED_VARIABLE(value);

    return success;
}

auto StringHelper::isExplicitBool() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    if (StringHelper(_sv).trim().compareIgnoreCase("true")) {
        return true;
    }
    if (StringHelper(_sv).trim().compareIgnoreCase("false")) {
        return true;
    }

    return false;
}

auto StringHelper::toInt() const noexcept -> int
{
    NO_STACK_TRACE_ENTRY();

    int64 value;
    const auto success = ConvertToNumber(StringHelper(_sv).trim(), value);

    return success ? clamp_to<int>(value) : 0;
}

auto StringHelper::toUInt() const noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    int64 value;
    const auto success = ConvertToNumber(StringHelper(_sv).trim(), value);

    return success ? clamp_to<uint>(value) : 0;
}

auto StringHelper::toInt64() const noexcept -> int64
{
    NO_STACK_TRACE_ENTRY();

    int64 value;
    const auto success = ConvertToNumber(StringHelper(_sv).trim(), value);

    return success ? value : 0;
}

auto StringHelper::toFloat() const noexcept -> float
{
    NO_STACK_TRACE_ENTRY();

    double value;
    const auto success = ConvertToNumber(StringHelper(_sv).trim(), value);

    return success ? static_cast<float>(value) : 0.0f;
}

auto StringHelper::toDouble() const noexcept -> double
{
    NO_STACK_TRACE_ENTRY();

    double value;
    const auto success = ConvertToNumber(StringHelper(_sv).trim(), value);

    return success ? value : 0.0;
}

auto StringHelper::toBool() const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    if (StringHelper(_sv).trim().compareIgnoreCase("true")) {
        return true;
    }
    if (StringHelper(_sv).trim().compareIgnoreCase("false")) {
        return false;
    }

    return toInt() != 0;
}

auto StringHelper::formatPath() -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    trim();
    normalizePathSlashes();

    OwnStorage();

    // Erase first './'
    while (_s[0] == '.' && _s[1] == '/') {
        _s.erase(0, 2);
    }

    // Skip first '../'
    uint back_count = 0;

    while (_s.length() >= 3 && _s[0] == '.' && _s[1] == '.' && _s[2] == '/') {
        back_count++;
        _s.erase(0, 3);
    }

    // Replace '/./' to '/'
    while (true) {
        const auto pos = _s.find("/./");

        if (pos == string::npos) {
            break;
        }

        _s.replace(pos, 3, "/");
    }

    // Replace 'folder/../' to '/'
    while (true) {
        const auto pos = _s.find("/../");

        if (pos == string::npos || pos == 0) {
            break;
        }

        const auto pos2 = _s.rfind('/', pos - 1);

        if (pos2 == string::npos) {
            _s.erase(0, pos + 4);
        }
        else {
            _s.erase(pos2 + 1, pos - pos2 + 3);
        }
    }

    // Apply skipped '../'
    for (uint i = 0; i < back_count; i++) {
        _s.insert(0, "../");
    }

    _sv = _s;

    return *this;
}

auto StringHelper::extractDir() -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    formatPath();

    const auto pos = _sv.find_last_of('/');

    if (pos != string::npos) {
        _sv = _sv.substr(0, pos);
    }

    return *this;
}

auto StringHelper::extractFileName() -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    formatPath();

    const auto pos = _sv.find_last_of('/');

    if (pos != string::npos) {
        _sv = _sv.substr(pos + 1);
    }

    return *this;
}

auto StringHelper::getFileExtension() -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    const auto dot = _sv.find_last_of('.');

    _sv = dot != string::npos ? _sv.substr(dot + 1) : "";

    lower();

    return *this;
}

auto StringHelper::eraseFileExtension() noexcept -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    const auto dot = _sv.find_last_of('.');

    if (dot != string::npos) {
        _sv = _sv.substr(0, dot);
    }

    return *this;
}

auto StringHelper::changeFileName(string_view new_name) -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    const auto ext = StringHelper(_s).getFileExtension().str();

    if (!ext.empty()) {
        const auto new_name_with_ext = StringHelper("{}.{}", new_name, ext);
        _s = StringHelper(_s).extractDir().combinePath(new_name_with_ext);
    }
    else {
        _s = StringHelper(_s).extractDir().combinePath(new_name);
    }

    _sv = _s;

    return *this;
}

auto StringHelper::combinePath(string_view path) -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    if (!path.empty()) {
        OwnStorage();

        if (!_s.empty() && _s.back() != '/' && path.front() != '/') {
            _s += "/";
        }

        _s += path;

        _sv = _s;

        formatPath();
    }

    return *this;
}

auto StringHelper::normalizePathSlashes() -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    std::replace(_s.begin(), _s.end(), '\\', '/');

    return *this;
}

auto StringHelper::normalizeLineEndings() -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    replace('\r', '\n', '\n');
    replace('\r', '\n');

    return *this;
}

#if FO_WINDOWS
auto StringHelper::parseWideChar(const wchar_t* str) -> StringHelper&
{
    NO_STACK_TRACE_ENTRY();

    OwnStorage();

    const auto len = static_cast<int>(::wcslen(str));

    if (len != 0) {
        auto* buf = static_cast<char*>(_malloca(static_cast<size_t>(len) * 4));
        const auto r = ::WideCharToMultiByte(CP_UTF8, 0, str, len, buf, len * 4, nullptr, nullptr);

        _s += buf != nullptr ? string(buf, r) : string();

        _freea(buf);
    }

    _sv = _s;

    return *this;
}

auto StringHelper::toWideChar() const -> std::wstring
{
    NO_STACK_TRACE_ENTRY();

    if (_sv.empty()) {
        return L"";
    }

    auto* buf = static_cast<wchar_t*>(_malloca(_sv.length() * sizeof(wchar_t) * 2));

    const auto len = ::MultiByteToWideChar(CP_UTF8, 0, _sv.data(), static_cast<int>(_sv.length()), buf, static_cast<int>(_sv.length()));
    auto result = buf != nullptr ? std::wstring(buf, len) : std::wstring();

    _freea(buf);

    return result;
}
#endif

// ReSharper restore CppInconsistentNaming

// 0xFFFD - Unicode REPLACEMENT CHARACTER
static constexpr uint UNICODE_BAD_CHAR = 0xFFFD;

auto utf8::IsValid(uint ucs) noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return ucs != UNICODE_BAD_CHAR && ucs <= 0x10FFFF;
}

auto utf8::DecodeStrNtLen(const char* str) noexcept -> size_t
{
    NO_STACK_TRACE_ENTRY();

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

auto utf8::Decode(const char* str, size_t& length) noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    if (length == 0) {
        return UNICODE_BAD_CHAR;
    }

    const auto make_result = [&length](uint ch, size_t ch_lenght) -> uint {
        STRONG_ASSERT(ch_lenght <= length);
        length = ch_lenght;
        return ch;
    };

    const auto make_error = [&length]() -> uint {
        length = 1;
        return UNICODE_BAD_CHAR;
    };

    const auto c = *reinterpret_cast<const uint8*>(str);

    if (c < 0x80) {
        return make_result(c, 1);
    }

    if (c < 0xc2) {
        return make_error();
    }

    if (length < 2) {
        return make_error();
    }

    if ((str[1] & 0xc0) != 0x80) {
        return make_error();
    }

    if (c < 0xe0) {
        return make_result(((str[0] & 0x1f) << 6) + (str[1] & 0x3f), 2);
    }

    if (length < 3) {
        return make_error();
    }

    if (c == 0xe0) {
        if (reinterpret_cast<const uint8*>(str)[1] < 0xa0) {
            return make_error();
        }

        if ((str[2] & 0xc0) != 0x80) {
            return make_error();
        }

        return make_result(((str[0] & 0x0f) << 12) + ((str[1] & 0x3f) << 6) + (str[2] & 0x3f), 3);
    }

    if (c < 0xf0) {
        if ((str[2] & 0xc0) != 0x80) {
            return make_error();
        }

        return make_result(((str[0] & 0x0f) << 12) + ((str[1] & 0x3f) << 6) + (str[2] & 0x3f), 3);
    }

    if (length < 4) {
        return make_error();
    }

    if (c == 0xf0) {
        if (reinterpret_cast<const uint8*>(str)[1] < 0x90) {
            return make_error();
        }

        if ((str[2] & 0xc0) != 0x80 || (str[3] & 0xc0) != 0x80) {
            return make_error();
        }

        return make_result(((str[0] & 0x07) << 18) + ((str[1] & 0x3f) << 12) + ((str[2] & 0x3f) << 6) + (str[3] & 0x3f), 4);
    }

    if (c < 0xf4) {
        if ((str[2] & 0xc0) != 0x80 || (str[3] & 0xc0) != 0x80) {
            return make_error();
        }

        return make_result(((str[0] & 0x07) << 18) + ((str[1] & 0x3f) << 12) + ((str[2] & 0x3f) << 6) + (str[3] & 0x3f), 4);
    }

    if (c == 0xf4) {
        if (reinterpret_cast<const uint8*>(str)[1] > 0x8f) {
            return make_error();
        }

        if ((str[2] & 0xc0) != 0x80 || (str[3] & 0xc0) != 0x80) {
            return make_error();
        }

        return make_result(((str[0] & 0x07) << 18) + ((str[1] & 0x3f) << 12) + ((str[2] & 0x3f) << 6) + (str[3] & 0x3f), 4);
    }

    return make_error();
}

auto utf8::Encode(uint ucs, char (&buf)[4]) noexcept -> size_t
{
    NO_STACK_TRACE_ENTRY();

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

    if (ucs <= 0x0010ffffu) {
        buf[0] = static_cast<char>(0xf0 | (ucs >> 18));
        buf[1] = static_cast<char>(0x80 | ((ucs >> 12) & 0x3F));
        buf[2] = static_cast<char>(0x80 | ((ucs >> 6) & 0x3F));
        buf[3] = static_cast<char>(0x80 | (ucs & 0x3F));
        return 4;
    }

    // Encode 0xFFFD
    buf[0] = static_cast<char>(0xef);
    buf[1] = static_cast<char>(0xbf);
    buf[2] = static_cast<char>(0xbd);
    return 3;
}

auto utf8::Lower(uint ucs) noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    uint ret;

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
    Utf8Data()
    {
        STACK_TRACE_ENTRY();

        UpperTable.resize(0x10000);

        for (uint i = 0; i < 0x10000; i++) {
            UpperTable[i] = static_cast<uint16>(i);
        }

        for (uint i = 0; i < 0x10000; i++) {
            const auto l = utf8::Lower(i);

            if (l != i) {
                UpperTable[l] = static_cast<uint16>(i);
            }
        }
    }

    vector<uint16> UpperTable {};
};
GLOBAL_DATA(Utf8Data, Data);

auto utf8::Upper(uint ucs) noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    if (ucs >= 0x10000) {
        return ucs;
    }

    return Data->UpperTable[ucs];
}
