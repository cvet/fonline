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

#pragma once

#include "Common.h"

// ReSharper disable once CppInconsistentNaming
class _str final
{
    // ReSharper disable CppInconsistentNaming

public:
    _str() = default;
    explicit _str(const char* s) noexcept :
        _sv {s}
    {
    }
    explicit _str(string_view s) noexcept :
        _sv {s}
    {
    }
    explicit _str(string&& s) noexcept :
        _s {std::move(s)},
        _sv {_s}
    {
    }
    template<typename... Args>
    explicit _str(string_view format, Args&&... args) :
        _s {fmt::format(format, std::forward<Args>(args)...)},
        _sv {_s}
    {
    }
    _str(const _str&) = delete;
    _str(_str&&) noexcept = delete;
    auto operator=(const _str&) -> _str& = delete;
    auto operator=(_str&&) noexcept -> _str& = delete;
    ~_str() = default;

    // ReSharper disable once CppNonExplicitConversionOperator
    operator string&&();
    // ReSharper disable once CppNonExplicitConversionOperator
    operator string_view() const noexcept { return _sv; }

    auto operator==(string_view r) const noexcept -> bool { return _sv == r; }
    auto operator!=(string_view r) const noexcept -> bool { return _sv != r; }

    [[nodiscard]] auto c_str() -> const char*;
    [[nodiscard]] auto str() -> string&&;
    [[nodiscard]] auto strv() const -> string_view { return _sv; }

    [[nodiscard]] auto length() const noexcept -> size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;
    [[nodiscard]] auto compareIgnoreCase(string_view r) const noexcept -> bool;
    [[nodiscard]] auto compareIgnoreCaseUtf8(string_view r) const -> bool;
    [[nodiscard]] auto startsWith(char r) const noexcept -> bool;
    [[nodiscard]] auto startsWith(string_view r) const noexcept -> bool;
    [[nodiscard]] auto endsWith(char r) const noexcept -> bool;
    [[nodiscard]] auto endsWith(string_view r) const noexcept -> bool;
    [[nodiscard]] auto isValidUtf8() const noexcept -> bool;
    [[nodiscard]] auto lengthUtf8() const noexcept -> size_t;

    [[nodiscard]] auto isNumber() const noexcept -> bool;
    [[nodiscard]] auto isExplicitBool() const noexcept -> bool;
    [[nodiscard]] auto toInt() const noexcept -> int;
    [[nodiscard]] auto toUInt() const noexcept -> uint;
    [[nodiscard]] auto toInt64() const noexcept -> int64;
    [[nodiscard]] auto toFloat() const noexcept -> float;
    [[nodiscard]] auto toDouble() const noexcept -> double;
    [[nodiscard]] auto toBool() const noexcept -> bool;

    [[nodiscard]] auto split(char divider) const -> vector<string>;
    [[nodiscard]] auto splitToInt(char divider) const -> vector<int>;

    auto substringUntil(char separator) -> _str&;
    auto substringUntil(string_view separator) -> _str&;
    auto substringAfter(char separator) -> _str&;
    auto substringAfter(string_view separator) -> _str&;
    auto trim() -> _str&;
    auto erase(char what) -> _str&;
    auto erase(char begin, char end) -> _str&;
    auto replace(char from, char to) -> _str&;
    auto replace(char from1, char from2, char to) -> _str&;
    auto replace(string_view from, string_view to) -> _str&;
    auto lower() -> _str&;
    auto lowerUtf8() -> _str&;
    auto upper() -> _str&;
    auto upperUtf8() -> _str&;

    auto formatPath() -> _str&;
    auto extractDir() -> _str&;
    auto extractFileName() -> _str&;
    auto getFileExtension() -> _str&; // Extension without dot and lowered
    auto eraseFileExtension() -> _str&; // Erase extension with dot
    auto changeFileName(string_view new_name) -> _str&;
    auto combinePath(string_view path) -> _str&;
    auto normalizePathSlashes() -> _str&;

    auto normalizeLineEndings() -> _str&;

#if FO_WINDOWS
    auto parseWideChar(const wchar_t* str) -> _str&;
    [[nodiscard]] auto toWideChar() const -> std::wstring;
#endif

private:
    void ownStorage();

    string _s {};
    string_view _sv {};

    // ReSharper restore CppInconsistentNaming
};

namespace utf8
{
    auto IsValid(uint ucs) noexcept -> bool;
    auto DecodeStrNtLen(const char* str) noexcept -> size_t;
    auto Decode(const char* str, size_t& length) noexcept -> uint;
    auto Encode(uint ucs, char (&buf)[4]) noexcept -> size_t;
    auto Lower(uint ucs) noexcept -> uint;
    auto Upper(uint ucs) noexcept -> uint;
}

template<>
struct fmt::formatter<_str> : formatter<std::string_view>
{
};
