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

#pragma once

#include "Common.h"

class Str final
{
public:
    Str() = delete;

    template<int Size>
    static void Copy(char (&to)[Size], string_view from)
    {
        return Copy(to, Size, from);
    }
    static void Copy(char* to, size_t size, string_view from);
};

// ReSharper disable once CppInconsistentNaming
class _str final
{
    // ReSharper disable CppInconsistentNaming

public:
    _str() = default;
    _str(const _str&) = default;
    explicit _str(string s) : _s(std::move(s)) { }
    explicit _str(const char* s) : _s(s) { }
    explicit _str(string_view s) : _s(s) { }
    template<typename... Args>
    explicit _str(string_view format, Args... args) : _s(fmt::format(format, args...))
    {
    }
    _str(_str&&) noexcept = default;
    auto operator=(const _str&) -> _str& = default;
    auto operator=(_str&&) noexcept -> _str& = default;
    ~_str() = default;

    // ReSharper disable once CppNonExplicitConversionOperator
    operator string&&() { return std::move(_s); }
    // ReSharper disable once CppNonExplicitConversionOperator
    operator string_view() const { return _s; }
    auto operator+(const char* r) const -> _str { return _str(_s + string(r)); }
    auto operator+(string_view r) const -> _str { return _str(_s + string(r)); }
    friend auto operator+(const _str& l, string_view r) -> _str { return _str(l._s + string(r)); }
    auto operator==(string_view r) const -> bool { return _s == r; }
    auto operator!=(string_view r) const -> bool { return _s != r; }
    friend auto operator!=(const _str& l, string_view r) -> bool { return l._s != r; }

    [[nodiscard]] auto c_str() const -> const char* { return _s.c_str(); }
    [[nodiscard]] auto str() const -> string { return _s; }
    [[nodiscard]] auto strv() const -> string_view { return _s; }

    [[nodiscard]] auto length() const -> uint;
    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto compareIgnoreCase(string_view r) const -> bool;
    [[nodiscard]] auto compareIgnoreCaseUtf8(string_view r) const -> bool;
    [[nodiscard]] auto startsWith(char r) const -> bool;
    [[nodiscard]] auto startsWith(string_view r) const -> bool;
    [[nodiscard]] auto endsWith(char r) const -> bool;
    [[nodiscard]] auto endsWith(string_view r) const -> bool;
    [[nodiscard]] auto isValidUtf8() const -> bool;
    [[nodiscard]] auto lengthUtf8() const -> uint;

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

    [[nodiscard]] auto split(char divider) const -> vector<string>;
    [[nodiscard]] auto splitToInt(char divider) const -> vector<int>;

    [[nodiscard]] auto isNumber() -> bool; // Todo: make isNumber const
    [[nodiscard]] auto toInt() -> int;
    [[nodiscard]] auto toUInt() -> uint;
    [[nodiscard]] auto toInt64() -> int64;
    [[nodiscard]] auto toUInt64() -> uint64;
    [[nodiscard]] auto toFloat() const -> float;
    [[nodiscard]] auto toDouble() const -> double;
    [[nodiscard]] auto toBool() -> bool;

    auto formatPath() -> _str&;
    auto extractDir() -> _str&;
    auto extractLastDir() -> _str&;
    auto extractFileName() -> _str&;
    auto getFileExtension() -> _str&; // Extension without dot
    auto eraseFileExtension() -> _str&; // Erase extension with dot
    auto combinePath(string_view path) -> _str&;
    auto forwardPath(string_view relative_dir) -> _str&;
    auto normalizePathSlashes() -> _str&;
    auto normalizeLineEndings() -> _str&;

#if FO_WINDOWS
    auto parseWideChar(const wchar_t* str) -> _str&;
    [[nodiscard]] auto toWideChar() const -> std::wstring;
#endif

    [[nodiscard]] auto toHash() -> hstring;

private:
    string _s {};

    // ReSharper restore CppInconsistentNaming
};

namespace utf8
{
    auto IsValid(uint ucs) -> bool;
    auto Decode(string_view str, uint* length) -> uint;
    auto Encode(uint ucs, char (&buf)[4]) -> uint;
    auto Lower(uint ucs) -> uint;
    auto Upper(uint ucs) -> uint;
} // namespace utf8

namespace fmt
{
    // ReSharper disable CppInconsistentNaming

    template<>
    struct formatter<_str>
    {
        template<typename ParseContext>
        constexpr auto parse(ParseContext& ctx)
        {
            return ctx.begin();
        }

        template<typename FormatContext>
        auto format(const _str& s, FormatContext& ctx)
        {
            return format_to(ctx.out(), "{}", s.strv());
        }
    };

    template<>
    struct formatter<hstring>
    {
        template<typename ParseContext>
        constexpr auto parse(ParseContext& ctx)
        {
            return ctx.begin();
        }

        template<typename FormatContext>
        auto format(const hstring& s, FormatContext& ctx)
        {
            return format_to(ctx.out(), "{}", s.Text ? s.Text : "");
        }
    };

    // ReSharper restore CppInconsistentNaming
} // namespace fmt
