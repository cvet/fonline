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

auto _str::length() const -> size_t
{
    return _s.length();
}

auto _str::empty() const -> bool
{
    return _s.empty();
}

auto _str::compareIgnoreCase(string_view r) const -> bool
{
    if (_s.length() != r.length()) {
        return false;
    }

    for (size_t i = 0; i < _s.length(); i++) {
        if (tolower(_s[i]) != tolower(r[i])) {
            return false;
        }
    }
    return true;
}

auto _str::compareIgnoreCaseUtf8(string_view r) const -> bool
{
    if (_s.length() != r.length()) {
        return false;
    }

    return _str(_s).lowerUtf8() == _str(r).lowerUtf8();
}

auto _str::startsWith(char r) const -> bool
{
    return _s.length() >= 1 && _s.front() == r;
}

auto _str::startsWith(string_view r) const -> bool
{
    return _s.length() >= r.length() && _s.compare(0, r.length(), r) == 0;
}

auto _str::endsWith(char r) const -> bool
{
    return _s.length() >= 1 && _s.back() == r;
}

auto _str::endsWith(string_view r) const -> bool
{
    return _s.length() >= r.length() && _s.compare(_s.length() - r.length(), r.length(), r) == 0;
}

auto _str::isValidUtf8() const -> bool
{
    for (size_t i = 0; i < _s.length();) {
        uint length = 0;
        const auto ucs = utf8::Decode(_s.c_str() + i, &length);
        if (!utf8::IsValid(ucs)) {
            return false;
        }
        i += length;
    }
    return true;
}

auto _str::lengthUtf8() const -> size_t
{
    size_t length = 0;
    const auto* str = _s.c_str();

    while (*str != 0) {
        length += static_cast<uint>((*str++ & 0xC0) != 0x80);
    }
    return length;
}

auto _str::substringUntil(char separator) -> _str&
{
    const auto pos = _s.find(separator);

    if (pos != string::npos) {
        _s = _s.substr(0, pos);
    }
    return *this;
}

auto _str::substringUntil(string_view separator) -> _str&
{
    const auto pos = _s.find(separator);

    if (pos != string::npos) {
        _s = _s.substr(0, pos);
    }
    return *this;
}

auto _str::substringAfter(char separator) -> _str&
{
    const auto pos = _s.find(separator);

    if (pos != string::npos) {
        _s = _s.substr(pos + 1);
    }
    else {
        _s.erase();
    }
    return *this;
}

auto _str::substringAfter(string_view separator) -> _str&
{
    const auto pos = _s.find(separator);

    if (pos != string::npos) {
        _s = _s.substr(pos + separator.length());
    }
    else {
        _s.erase();
    }
    return *this;
}

auto _str::trim() -> _str&
{
    // Left trim
    const auto l = _s.find_first_not_of(" \n\r\t");

    if (l == string::npos) {
        _s.erase();
    }
    else {
        if (l > 0) {
            _s.erase(0, l);
        }

        // Right trim
        const auto r = _s.find_last_not_of(" \n\r\t");

        if (r < _s.length() - 1) {
            _s.erase(r + 1);
        }
    }
    return *this;
}

auto _str::erase(char what) -> _str&
{
    _s.erase(std::remove(_s.begin(), _s.end(), what), _s.end());
    return *this;
}

auto _str::erase(char begin, char end) -> _str&
{
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
    return *this;
}

auto _str::replace(char from, char to) -> _str&
{
    std::replace(_s.begin(), _s.end(), from, to);
    return *this;
}

auto _str::replace(char from1, char from2, char to) -> _str&
{
    replace(string({from1, from2}), string({to}));
    return *this;
}

auto _str::replace(string_view from, string_view to) -> _str&
{
    size_t pos = 0;

    while ((pos = _s.find(from, pos)) != std::string::npos) {
        _s.replace(pos, from.length(), to);
        pos += to.length();
    }
    return *this;
}

auto _str::lower() -> _str&
{
    std::transform(_s.begin(), _s.end(), _s.begin(), tolower);
    return *this;
}

auto _str::upper() -> _str&
{
    std::transform(_s.begin(), _s.end(), _s.begin(), toupper);
    return *this;
}

auto _str::lowerUtf8() -> _str&
{
    for (size_t i = 0; i < _s.length();) {
        uint length = 0;
        auto ucs = utf8::Decode(_s.c_str() + i, &length);

        ucs = utf8::Lower(ucs);

        char buf[4];
        const auto new_length = utf8::Encode(ucs, buf);

        _s.replace(i, length, buf, new_length);

        i += new_length;
    }
    return *this;
}

auto _str::upperUtf8() -> _str&
{
    for (size_t i = 0; i < _s.length();) {
        uint length = 0;
        auto ucs = utf8::Decode(_s.c_str() + i, &length);

        ucs = utf8::Upper(ucs);

        char buf[4];
        const auto new_length = utf8::Encode(ucs, buf);

        _s.replace(i, length, buf, new_length);

        i += new_length;
    }
    return *this;
}

auto _str::split(char divider) const -> vector<string>
{
    vector<string> result;
    std::stringstream ss(_s);
    string entry;

    while (std::getline(ss, entry, divider)) {
        entry = _str(entry).trim();

        if (!entry.empty()) {
            result.push_back(entry);
        }
    }
    return result;
}

auto _str::splitToInt(char divider) const -> vector<int>
{
    vector<int> result;
    std::stringstream ss(_s);
    string entry;

    while (std::getline(ss, entry, divider)) {
        entry = _str(entry).trim();

        if (!entry.empty()) {
            result.push_back(_str(entry).toInt());
        }
    }
    return result;
}

auto _str::isNumber() const -> bool
{
    return isInt() || isFloat();
}

auto _str::isInt() const -> bool
{
    if (_s.empty()) {
        return false;
    }

    char* str_end = nullptr;
    const auto v = std::strtoul(_s.c_str(), &str_end, 10);
    UNUSED_VARIABLE(v);

    return str_end != _s.c_str();
}

auto _str::isFloat() const -> bool
{
    if (_s.empty()) {
        return false;
    }

    char* str_end = nullptr;
    const auto v = std::strtod(_s.c_str(), &str_end);
    UNUSED_VARIABLE(v);

    return str_end != _s.c_str();
}

auto _str::isExplicitBool() const -> bool
{
    if (compareIgnoreCase("true")) {
        return true;
    }
    if (compareIgnoreCase("false")) {
        return true;
    }

    return false;
}

auto _str::toInt() const -> int
{
    return static_cast<int>(std::strtoll(_s.c_str(), nullptr, 0));
}

auto _str::toUInt() const -> uint
{
    return static_cast<uint>(std::strtoull(_s.c_str(), nullptr, 0));
}

auto _str::toInt64() const -> int64
{
    return static_cast<int64>(std::strtoll(_s.c_str(), nullptr, 0));
}

auto _str::toUInt64() const -> uint64
{
    return static_cast<uint64>(std::strtoull(_s.c_str(), nullptr, 0));
}

auto _str::toFloat() const -> float
{
    return static_cast<float>(std::strtod(_s.c_str(), nullptr));
}

auto _str::toDouble() const -> double
{
    return std::strtod(_s.c_str(), nullptr);
}

auto _str::toBool() const -> bool
{
    if (compareIgnoreCase("true")) {
        return true;
    }
    if (compareIgnoreCase("false")) {
        return false;
    }

    return toInt() != 0;
}

auto _str::formatPath() -> _str&
{
    trim();
    normalizePathSlashes();

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
            break;
        }

        _s.erase(pos2 + 1, pos - pos2 - 1 + 3);
    }

    // Apply skipped '../'
    for (uint i = 0; i < back_count; i++) {
        _s.insert(0, "../");
    }

    return *this;
}

auto _str::extractDir() -> _str&
{
    formatPath();

    const auto pos = _s.find_last_of('/');

    if (pos != string::npos) {
        _s = _s.substr(0, pos);
    }
    return *this;
}

auto _str::extractFileName() -> _str&
{
    formatPath();

    const auto pos = _s.find_last_of('/');

    if (pos != string::npos) {
        _s = _s.substr(pos + 1);
    }
    return *this;
}

auto _str::getFileExtension() -> _str&
{
    const auto dot = _s.find_last_of('.');

    _s = dot != string::npos ? _s.substr(dot + 1) : "";

    lower();
    return *this;
}

auto _str::eraseFileExtension() -> _str&
{
    const auto dot = _s.find_last_of('.');

    if (dot != string::npos) {
        _s = _s.substr(0, dot);
    }
    return *this;
}

auto _str::changeFileName(string_view new_name) -> _str&
{
    const auto ext = _str(_s).getFileExtension();
    if (!ext.empty()) {
        const auto new_name_with_ext = _str("{}.{}", new_name, ext);
        _s = _str(_s).extractDir().combinePath(new_name_with_ext);
    }
    else {
        _s = _str(_s).extractDir().combinePath(new_name);
    }
    return *this;
}

auto _str::combinePath(string_view path) -> _str&
{
    if (!path.empty()) {
        if (!_s.empty() && _s.back() != '/' && path.front() != '/') {
            _s += "/";
        }

        _s += path;
        formatPath();
    }

    return *this;
}

auto _str::normalizePathSlashes() -> _str&
{
    std::replace(_s.begin(), _s.end(), '\\', '/');
    return *this;
}

auto _str::normalizeLineEndings() -> _str&
{
    replace('\r', '\n', '\n');
    replace('\r', '\n');
    return *this;
}

#if FO_WINDOWS
auto _str::parseWideChar(const wchar_t* str) -> _str&
{
    const auto len = static_cast<int>(::wcslen(str));

    if (len != 0) {
        auto* buf = static_cast<char*>(_malloca(static_cast<size_t>(len) * 4));
        const auto r = ::WideCharToMultiByte(CP_UTF8, 0, str, len, buf, len * 4, nullptr, nullptr);

        _s += buf != nullptr ? string(buf, r) : string();
        _freea(buf);
    }

    return *this;
}

auto _str::toWideChar() const -> std::wstring
{
    if (_s.empty()) {
        return L"";
    }

    auto* buf = static_cast<wchar_t*>(_malloca(_s.length() * sizeof(wchar_t) * 2));
    const auto len = ::MultiByteToWideChar(CP_UTF8, 0, _s.c_str(), static_cast<int>(_s.length()), buf, static_cast<int>(_s.length()));

    auto result = buf != nullptr ? std::wstring(buf, len) : std::wstring();
    _freea(buf);
    return result;
}
#endif

// ReSharper restore CppInconsistentNaming

auto utf8::IsValid(uint ucs) -> bool
{
    // 0xFFFD - Unicode REPLACEMENT CHARACTER
    return ucs != 0xFFFD && ucs <= 0x10FFFF;
}

auto utf8::Decode(string_view str, uint* length) -> uint
{
#define DECODE_FAIL() \
    do { \
        if (length) { \
            *length = 1; \
        } \
        return 0xFFFD; \
    } while (0)

    const auto c = *reinterpret_cast<const uint8*>(str.data());
    if (c < 0x80) {
        if (length != nullptr) {
            *length = 1;
        }
        return c;
    }

    if (c < 0xc2) {
        DECODE_FAIL();
    }
    if ((str[1] & 0xc0) != 0x80) {
        DECODE_FAIL();
    }

    if (c < 0xe0) {
        if (length != nullptr) {
            *length = 2;
        }
        return ((str[0] & 0x1f) << 6) + (str[1] & 0x3f);
    }

    if (c == 0xe0) {
        if (reinterpret_cast<const uint8*>(str.data())[1] < 0xa0) {
            DECODE_FAIL();
        }

        if ((str[2] & 0xc0) != 0x80) {
            DECODE_FAIL();
        }
        if (length != nullptr) {
            *length = 3;
        }
        return ((str[0] & 0x0f) << 12) + ((str[1] & 0x3f) << 6) + (str[2] & 0x3f);
    }

    if (c < 0xf0) {
        if ((str[2] & 0xc0) != 0x80) {
            DECODE_FAIL();
        }
        if (length != nullptr) {
            *length = 3;
        }
        return ((str[0] & 0x0f) << 12) + ((str[1] & 0x3f) << 6) + (str[2] & 0x3f);
    }

    if (c == 0xf0) {
        if (reinterpret_cast<const uint8*>(str.data())[1] < 0x90) {
            DECODE_FAIL();
        }
        if ((str[2] & 0xc0) != 0x80 || (str[3] & 0xc0) != 0x80) {
            DECODE_FAIL();
        }
        if (length != nullptr) {
            *length = 4;
        }
        return ((str[0] & 0x07) << 18) + ((str[1] & 0x3f) << 12) + ((str[2] & 0x3f) << 6) + (str[3] & 0x3f);
    }

    if (c < 0xf4) {
        if ((str[2] & 0xc0) != 0x80 || (str[3] & 0xc0) != 0x80) {
            DECODE_FAIL();
        }
        if (length != nullptr) {
            *length = 4;
        }
        return ((str[0] & 0x07) << 18) + ((str[1] & 0x3f) << 12) + ((str[2] & 0x3f) << 6) + (str[3] & 0x3f);
    }

    if (c == 0xf4) {
        if (reinterpret_cast<const uint8*>(str.data())[1] > 0x8f) {
            DECODE_FAIL();
        }
        if ((str[2] & 0xc0) != 0x80 || (str[3] & 0xc0) != 0x80) {
            DECODE_FAIL();
        }
        if (length != nullptr) {
            *length = 4;
        }
        return ((str[0] & 0x07) << 18) + ((str[1] & 0x3f) << 12) + ((str[2] & 0x3f) << 6) + (str[3] & 0x3f);
    }

    DECODE_FAIL();

#undef DECODE_FAIL
}

auto utf8::Encode(uint ucs, char (&buf)[4]) -> uint
{
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

auto utf8::Lower(uint ucs) -> uint
{
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

auto utf8::Upper(uint ucs) -> uint
{
    if (ucs >= 0x10000) {
        return ucs;
    }
    return Data->UpperTable[ucs];
}
