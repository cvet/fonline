//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

auto _str::length() const -> uint
{
    return static_cast<uint>(_s.length());
}

auto _str::empty() const -> bool
{
    return _s.empty();
}

auto _str::compareIgnoreCase(const string& r) const -> bool
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

auto _str::compareIgnoreCaseUtf8(const string& r) const -> bool
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

auto _str::startsWith(const string& r) const -> bool
{
    return _s.length() >= r.length() && _s.compare(0, r.length(), r) == 0;
}

auto _str::endsWith(char r) const -> bool
{
    return _s.length() >= 1 && _s.back() == r;
}

auto _str::endsWith(const string& r) const -> bool
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

auto _str::lengthUtf8() const -> uint
{
    uint length = 0;
    const auto* str = _s.c_str();

    while (*str != 0) {
        length += static_cast<unsigned int>((*str++ & 0xC0) != 0x80);
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

auto _str::substringUntil(const string& separator) -> _str&
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

auto _str::substringAfter(const string& separator) -> _str&
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

auto _str::replace(const string& from, const string& to) -> _str&
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

auto _str::isNumber() -> bool
{
    if (_s.empty()) {
        return false;
    }

    trim();

    if (_s.empty()) {
        return false;
    }

    if (_s.empty() || isdigit(_s[0]) == 0 && _s[0] != '-' && _s[0] != '+') {
        return false;
    }

    char* p = nullptr;
    auto v = strtol(_s.c_str(), &p, 10);
    UNUSED_VARIABLE(v);

    return *p == 0;
}

auto _str::toInt() -> int
{
    return static_cast<int>(toInt64());
}

auto _str::toUInt() -> uint
{
    return static_cast<uint>(toInt64());
}

auto _str::toInt64() -> int64
{
    trim();

    if (_s.length() >= 2 && _s[0] == '0' && (_s[1] == 'x' || _s[1] == 'X')) {
        return strtoll(_s.substr(2).c_str(), nullptr, 16);
    }
    return strtoll(_s.c_str(), nullptr, 10);
}

auto _str::toUInt64() -> uint64
{
    trim();

    if (_s.length() >= 2 && _s[0] == '0' && (_s[1] == 'x' || _s[1] == 'X')) {
        return strtoull(_s.substr(2).c_str(), nullptr, 16);
    }
    return strtoull(_s.c_str(), nullptr, 10);
}

auto _str::toFloat() const -> float
{
    return static_cast<float>(atof(_s.c_str()));
}

auto _str::toDouble() const -> double
{
    return atof(_s.c_str());
}

auto _str::toBool() -> bool
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

    // Replace '//' to '/'
    while (true) {
        const auto pos = _s.find("//");

        if (pos == string::npos) {
            break;
        }

        _s.replace(pos, 2, "/");
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
        _s = _s.substr(0, pos + 1);
    }
    else if (!_s.empty() && _s.back() != '/') {
        _s += "/";
    }
    return *this;
}

auto _str::extractLastDir() -> _str&
{
    formatPath();
    extractDir();

    if (!_s.empty()) {
        _s.pop_back();
    }

    const auto pos = _s.find_last_of('/');

    if (pos != string::npos) {
        _s = _s.substr(pos + 1);
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

auto _str::combinePath(const string& path) -> _str&
{
    extractDir();

    if (!_s.empty() && _s.back() != '/' && (path.empty() || path.front() != '/')) {
        _s += "/";
    }

    _s += path;
    formatPath();

    return *this;
}

auto _str::forwardPath(const string& relative_dir) -> _str&
{
    const string dir = _str(*this).extractDir();
    const string name = _str(*this).extractFileName();

    _s = dir + relative_dir + name;
    formatPath();

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
    const auto len = static_cast<int>(wcslen(str));

    if (len != 0) {
        auto* buf = static_cast<char*>(alloca(UTF8_BUF_SIZE(len)));
        const auto r = WideCharToMultiByte(CP_UTF8, 0, str, len, buf, len * 4, nullptr, nullptr);

        _s += string(buf, r);
    }
    return *this;
}

auto _str::toWideChar() const -> std::wstring
{
    if (_s.empty()) {
        return L"";
    }

    auto* buf = static_cast<wchar_t*>(alloca(_s.length() * sizeof(wchar_t) * 2));
    const auto len = MultiByteToWideChar(CP_UTF8, 0, _s.c_str(), static_cast<int>(_s.length()), buf, static_cast<int>(_s.length()));

    return std::wstring(buf, len);
}
#endif

auto _str::parseHash(hash /*h*/) -> _str&
{
    // Todo: restore hash parsing
    throw UnreachablePlaceException(LINE_STR);
    return *this;
}

auto _str::toHash() -> hash
{
    if (_s.empty()) {
        return 0;
    }

    normalizePathSlashes();
    trim();

    if (_s.empty()) {
        return 0;
    }

    return Hashing::MurmurHash2(reinterpret_cast<const uchar*>(_s.c_str()), static_cast<uint>(_s.length()));
}

// ReSharper restore CppInconsistentNaming

void Str::Copy(char* to, size_t size, const char* from)
{
    RUNTIME_ASSERT(to);
    RUNTIME_ASSERT(from);
    RUNTIME_ASSERT(size > 0);

    const auto from_len = strlen(from);
    if (from_len == 0u) {
        to[0] = 0;
        return;
    }

    if (from_len >= size) {
        std::memcpy(to, from, size - 1);
        to[size - 1] = 0;
    }
    else {
        std::memcpy(to, from, from_len);
        to[from_len] = 0;
    }
}

auto Str::Compare(const char* str1, const char* str2) -> bool
{
    while (*str1 != 0 && *str2 != 0) {
        if (*str1 != *str2) {
            return false;
        }

        str1++;
        str2++;
    }
    return *str1 == 0 && *str2 == 0;
}

auto utf8::IsValid(uint ucs) -> bool
{
    return ucs != 0xFFFD /* Unicode REPLACEMENT CHARACTER */ && ucs <= 0x10FFFF;
}

auto utf8::Decode(const char* str, uint* length) -> uint
{
    // Taked from FLTK
    const auto c = *(unsigned char*)str;
    if (c < 0x80) {
        if (length != nullptr) {
            *length = 1;
        }
        return c;
    }
    if (c < 0xc2) {
        goto FAIL;
    }
    if ((str[1] & 0xc0) != 0x80) {
        goto FAIL;
    }
    if (c < 0xe0) {
        if (length != nullptr) {
            *length = 2;
        }
        return ((str[0] & 0x1f) << 6) + (str[1] & 0x3f);
    }
    else if (c == 0xe0) {
        if (((unsigned char*)str)[1] < 0xa0) {
            goto FAIL;
        }
        goto UTF8_3;
    }
    else if (c < 0xf0) {
    UTF8_3:
        if ((str[2] & 0xc0) != 0x80) {
            goto FAIL;
        }
        if (length) {
            *length = 3;
        }
        return ((str[0] & 0x0f) << 12) + ((str[1] & 0x3f) << 6) + (str[2] & 0x3f);
    }
    else if (c == 0xf0) {
        if (((unsigned char*)str)[1] < 0x90) {
            goto FAIL;
        }
        goto UTF8_4;
    }
    else if (c < 0xf4) {
    UTF8_4:
        if ((str[2] & 0xc0) != 0x80 || (str[3] & 0xc0) != 0x80) {
            goto FAIL;
        }
        if (length) {
            *length = 4;
        }
        return ((str[0] & 0x07) << 18) + ((str[1] & 0x3f) << 12) + ((str[2] & 0x3f) << 6) + (str[3] & 0x3f);
    }
    else if (c == 0xf4) {
        if (((unsigned char*)str)[1] > 0x8f) {
            goto FAIL; /* after 0x10ffff */
        }
        goto UTF8_4;
    }
    else {
    FAIL:
        if (length) {
            *length = 1;
        }
        return 0xfffd; /* Unicode REPLACEMENT CHARACTER */
    }
}

auto utf8::Encode(uint ucs, char (&buf)[4]) -> uint
{
    // Taked from FLTK
    if (ucs < 0x000080U) {
        buf[0] = ucs;
        return 1;
    }
    if (ucs < 0x000800U) {
        buf[0] = 0xc0 | ucs >> 6;
        buf[1] = 0x80 | ucs & 0x3F;
        return 2;
    }
    else if (ucs < 0x010000U) {
        buf[0] = 0xe0 | ucs >> 12;
        buf[1] = 0x80 | ucs >> 6 & 0x3F;
        buf[2] = 0x80 | ucs & 0x3F;
        return 3;
    }
    else if (ucs <= 0x0010ffffU) {
        buf[0] = 0xf0 | ucs >> 18;
        buf[1] = 0x80 | ucs >> 12 & 0x3F;
        buf[2] = 0x80 | ucs >> 6 & 0x3F;
        buf[3] = 0x80 | ucs & 0x3F;
        return 4;
    }
    else {
        /* encode 0xfffd: */
        buf[0] = 0xefU;
        buf[1] = 0xbfU;
        buf[2] = 0xbdU;
        return 3;
    }
}

auto utf8::Lower(uint ucs) -> uint
{
    // Taked from FLTK
    uint ret = 0;
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

auto utf8::Upper(uint ucs) -> uint
{
    // Taken from FLTK
    static unsigned short* table = nullptr;
    if (table == nullptr) {
        table = static_cast<unsigned short*>(malloc(sizeof(unsigned short) * 0x10000));
        for (uint i = 0; i < 0x10000; i++) {
            table[i] = static_cast<unsigned short>(i);
        }
        for (uint i = 0; i < 0x10000; i++) {
            const auto l = Lower(i);
            if (l != i) {
                table[l] = static_cast<unsigned short>(i);
            }
        }
    }
    if (ucs >= 0x10000) {
        return ucs;
    }
    return table[ucs];
}
