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

#include "Containers.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

static constexpr auto IsUtf8StreamSpace(char8_t ch) noexcept -> bool
{
    return ch == u8' ' || ch == u8'\t' || ch == u8'\n' || ch == u8'\r' || ch == u8'\f' || ch == u8'\v';
}

u8istringstream::u8istringstream(string_view input) :
    _input {input}
{
    FO_STACK_TRACE_ENTRY();
}

u8istringstream::u8istringstream(u8string_view input) :
    _input {input}
{
    FO_STACK_TRACE_ENTRY();
}

auto u8istringstream::operator>>(string& value) -> u8istringstream&
{
    FO_STACK_TRACE_ENTRY();

    const auto token = ReadToken();

    if (token) {
        value = utf8_to_string(*token);
    }

    return *this;
}

auto u8istringstream::operator>>(u8string& value) -> u8istringstream&
{
    FO_STACK_TRACE_ENTRY();

    const auto token = ReadToken();

    if (token) {
        value.assign(*token);
    }

    return *this;
}

auto u8istringstream::eof() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _eof;
}

auto u8istringstream::fail() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _fail;
}

u8istringstream::operator bool() const noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_fail;
}

void u8istringstream::clear() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _eof = false;
    _fail = false;
}

auto u8istringstream::ReadToken() -> optional<u8string_view>
{
    FO_STACK_TRACE_ENTRY();

    if (_fail) {
        return std::nullopt;
    }

    const std::u8string_view input = _input.view().native_view();

    while (_position < input.size() && IsUtf8StreamSpace(input[_position])) {
        _position++;
    }

    if (_position == input.size()) {
        _eof = true;
        _fail = true;
        return std::nullopt;
    }

    const size_t begin = _position;

    while (_position < input.size() && !IsUtf8StreamSpace(input[_position])) {
        _position++;
    }

    _eof = _position == input.size();
    return u8string_view::FromChecked(input.substr(begin, _position - begin));
}

auto u8istringstream::ReadLine(u8string& line, char8_t delimiter) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_fail) {
        return false;
    }

    const std::u8string_view input = _input.view().native_view();

    if (_position == input.size()) {
        _eof = true;
        _fail = true;
        return false;
    }

    const size_t delimiter_pos = input.find(delimiter, _position);

    if (delimiter_pos == std::u8string_view::npos) {
        line.assign(u8string_view::FromChecked(input.substr(_position)));
        _position = input.size();
        _eof = true;
    }
    else {
        line.assign(u8string_view::FromChecked(input.substr(_position, delimiter_pos - _position)));
        _position = delimiter_pos + 1;
        _eof = _position == input.size();
    }

    return true;
}

auto getline(u8istringstream& input, u8string& line, char8_t delimiter) -> u8istringstream&
{
    FO_STACK_TRACE_ENTRY();

    input.ReadLine(line, delimiter);
    return input;
}

auto utf8_map_as_char_views(const map<string_view, u8string_view>& values) -> map<string_view, string_view>
{
    FO_STACK_TRACE_ENTRY();

    map<string_view, string_view> result;

    for (const auto& [key, value] : values) {
        result.emplace(key, utf8_as_char_view(value));
    }

    return result;
}

FO_END_NAMESPACE
