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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "AnyData.h"
#include "StringUtils.h"

static auto CodeString(string_view str, bool strong_protect, bool just_escape = false) -> string;
static auto DecodeString(string_view str) -> string;
static auto ReadToken(const char* str, string& result) -> const char*;

auto AnyData::ValueToString(const Value& value) -> string
{
    PROFILER_ENTRY();

    constexpr auto default_buf_size = 1024;

    switch (value.index()) {
    case INT_VALUE:
        return _str("{}", std::get<INT_VALUE>(value)).str();
    case INT64_VALUE:
        return _str("{}", std::get<INT64_VALUE>(value)).str();
    case DOUBLE_VALUE:
        return _str("{}", std::get<DOUBLE_VALUE>(value)).str();
    case BOOL_VALUE:
        return std::get<BOOL_VALUE>(value) ? "True" : "False";
    case STRING_VALUE:
        return CodeString(std::get<STRING_VALUE>(value), false);
    case ARRAY_VALUE: {
        string arr_str;
        arr_str.reserve(default_buf_size);
        const auto& arr = std::get<ARRAY_VALUE>(value);

        for (size_t i = 0; i < arr.size(); i++) {
            if (i > 0u) {
                arr_str.append(" ");
            }

            const auto& arr_value = arr[i];
            switch (arr_value.index()) {
            case INT_VALUE:
                arr_str.append(_str("{}", std::get<INT_VALUE>(arr_value)).str());
                break;
            case INT64_VALUE:
                arr_str.append(_str("{}", std::get<INT64_VALUE>(arr_value)).str());
                break;
            case DOUBLE_VALUE:
                arr_str.append(_str("{}", std::get<DOUBLE_VALUE>(arr_value)).str());
                break;
            case BOOL_VALUE:
                arr_str.append(std::get<BOOL_VALUE>(arr_value) ? "True" : "False");
                break;
            case STRING_VALUE:
                arr_str.append(CodeString(std::get<STRING_VALUE>(arr_value), true));
                break;
            default:
                throw UnreachablePlaceException(LINE_STR);
            }
        }

        return arr_str;
    }
    case DICT_VALUE: {
        string dict_str;
        dict_str.reserve(default_buf_size);
        const auto& dict = std::get<DICT_VALUE>(value);

        for (auto it = dict.begin(); it != dict.end(); ++it) {
            const auto& dict_key = it->first;
            const auto& dict_value = it->second;

            if (it != dict.begin()) {
                dict_str.append(" ");
            }

            dict_str.append(CodeString(dict_key, true));

            dict_str.append(" ");

            switch (dict_value.index()) {
            case INT_VALUE:
                dict_str.append(_str("{}", std::get<INT_VALUE>(dict_value)).str());
                break;
            case INT64_VALUE:
                dict_str.append(_str("{}", std::get<INT64_VALUE>(dict_value)).str());
                break;
            case DOUBLE_VALUE:
                dict_str.append(_str("{}", std::get<DOUBLE_VALUE>(dict_value)).str());
                break;
            case BOOL_VALUE:
                dict_str.append(std::get<BOOL_VALUE>(dict_value) ? "True" : "False");
                break;
            case STRING_VALUE:
                dict_str.append(CodeString(std::get<STRING_VALUE>(dict_value), true));
                break;
            case ARRAY_VALUE: {
                string dict_arr_str;
                dict_arr_str.reserve(default_buf_size);
                const auto& dict_arr = std::get<ARRAY_VALUE>(dict_value);

                for (size_t i = 0; i < dict_arr.size(); i++) {
                    if (i > 0u) {
                        dict_arr_str.append(" ");
                    }

                    const auto& dict_arr_value = dict_arr[i];
                    switch (dict_arr_value.index()) {
                    case INT_VALUE:
                        dict_arr_str.append(_str("{}", std::get<INT_VALUE>(dict_arr_value)).str());
                        break;
                    case INT64_VALUE:
                        dict_arr_str.append(_str("{}", std::get<INT64_VALUE>(dict_arr_value)).str());
                        break;
                    case DOUBLE_VALUE:
                        dict_arr_str.append(_str("{}", std::get<DOUBLE_VALUE>(dict_arr_value)).str());
                        break;
                    case BOOL_VALUE:
                        dict_arr_str.append(std::get<BOOL_VALUE>(dict_arr_value) ? "True" : "False");
                        break;
                    case STRING_VALUE:
                        dict_arr_str.append(CodeString(std::get<STRING_VALUE>(dict_arr_value), true));
                        break;
                    default:
                        throw UnreachablePlaceException(LINE_STR);
                    }
                }

                dict_str.append("\"").append(CodeString(dict_arr_str, true, true)).append("\"");
            } break;
            default:
                throw UnreachablePlaceException(LINE_STR);
            }
        }

        return dict_str;
    }
    default:
        break;
    }

    throw UnreachablePlaceException(LINE_STR);
}

auto AnyData::ParseValue(const string& str, bool as_dict, bool as_array, int value_type) -> Value
{
    PROFILER_ENTRY();

    RUNTIME_ASSERT(value_type == INT_VALUE || value_type == INT64_VALUE || value_type == DOUBLE_VALUE || value_type == BOOL_VALUE || value_type == STRING_VALUE);

    if (as_dict) {
        Dict dict;

        const char* s = str.c_str();
        string dict_key_entry;
        string dict_value_entry;
        while ((s = ReadToken(s, dict_key_entry)) != nullptr && (s = ReadToken(s, dict_value_entry)) != nullptr) {
            if (as_array) {
                Array dict_arr;

                const auto decoded_dict_value_entry = DecodeString(dict_value_entry);
                const char* s2 = decoded_dict_value_entry.c_str();
                string arr_entry;
                while ((s2 = ReadToken(s2, arr_entry)) != nullptr) {
                    switch (value_type) {
                    case INT_VALUE:
                        dict_arr.push_back(_str("{}", arr_entry).toInt());
                        break;
                    case INT64_VALUE:
                        dict_arr.push_back(_str("{}", arr_entry).toInt64());
                        break;
                    case DOUBLE_VALUE:
                        dict_arr.push_back(_str("{}", arr_entry).toDouble());
                        break;
                    case BOOL_VALUE:
                        dict_arr.push_back(_str("{}", arr_entry).toBool());
                        break;
                    case STRING_VALUE:
                        dict_arr.push_back(DecodeString(arr_entry));
                        break;
                    default:
                        throw UnreachablePlaceException(LINE_STR);
                    }
                }

                dict.emplace(dict_key_entry, dict_arr);
            }
            else {
                switch (value_type) {
                case INT_VALUE:
                    dict.emplace(dict_key_entry, _str("{}", dict_value_entry).toInt());
                    break;
                case INT64_VALUE:
                    dict.emplace(dict_key_entry, _str("{}", dict_value_entry).toInt64());
                    break;
                case DOUBLE_VALUE:
                    dict.emplace(dict_key_entry, _str("{}", dict_value_entry).toDouble());
                    break;
                case BOOL_VALUE:
                    dict.emplace(dict_key_entry, _str("{}", dict_value_entry).toBool());
                    break;
                case STRING_VALUE:
                    dict.emplace(dict_key_entry, DecodeString(dict_value_entry));
                    break;
                default:
                    throw UnreachablePlaceException(LINE_STR);
                }
            }
        }

        return dict;
    }
    else if (as_array) {
        Array arr;

        const char* s = str.c_str();
        string arr_entry;
        while ((s = ReadToken(s, arr_entry)) != nullptr) {
            switch (value_type) {
            case INT_VALUE:
                arr.push_back(_str("{}", arr_entry).toInt());
                break;
            case INT64_VALUE:
                arr.push_back(_str("{}", arr_entry).toInt64());
                break;
            case DOUBLE_VALUE:
                arr.push_back(_str("{}", arr_entry).toDouble());
                break;
            case BOOL_VALUE:
                arr.push_back(_str("{}", arr_entry).toBool());
                break;
            case STRING_VALUE:
                arr.push_back(DecodeString(arr_entry));
                break;
            default:
                throw UnreachablePlaceException(LINE_STR);
            }
        }

        return arr;
    }
    else {
        switch (value_type) {
        case INT_VALUE:
            return _str("{}", str).toInt();
        case INT64_VALUE:
            return _str("{}", str).toInt64();
        case DOUBLE_VALUE:
            return _str("{}", str).toDouble();
        case BOOL_VALUE:
            return _str("{}", str).toBool();
        case STRING_VALUE:
            return DecodeString(str);
        default:
            break;
        }
    }

    throw UnreachablePlaceException(LINE_STR);
}

static auto CodeString(string_view str, bool strong_protect, bool just_escape) -> string
{
    PROFILER_ENTRY();

    auto protect = false;

    if (!just_escape) {
        if (strong_protect && (str.empty() || str.find_first_of(" \t") != string::npos)) {
            protect = true;
        }
        else if (!strong_protect && !str.empty() && (str.find_first_of(" \t") == 0 || str.find_last_of(" \t") == str.length() - 1)) {
            protect = true;
        }
        if (!protect && str.length() >= 2 && str[str.length() - 1] == '\\' && str[str.length() - 2] == ' ') {
            protect = true;
        }
    }

    string result;
    result.reserve(str.length() * 2);

    if (protect) {
        result.append("\"");
    }

    const auto* s = str.data();
    uint length = 0;
    while (*s != 0) {
        utf8::Decode(s, &length);
        if (length == 1) {
            switch (*s) {
            case '\r':
                break;
            case '\n':
                result.append("\\n");
                break;
            case '"':
                result.append("\\\"");
                break;
            case '\\':
                result.append("\\\\");
                break;
            default:
                result.append(s, 1);
                break;
            }
        }
        else {
            result.append(s, length);
        }

        s += length;
    }

    if (protect) {
        result.append("\"");
    }

    return result;
}

static auto DecodeString(string_view str) -> string
{
    PROFILER_ENTRY();

    if (str.empty()) {
        return string();
    }

    string result;
    result.reserve(str.length());

    const auto* s = str.data();
    uint length = 0;

    utf8::Decode(s, &length);
    const auto is_protected = length == 1 && *s == '\"';
    if (is_protected) {
        s++;
    }

    while (*s != 0) {
        utf8::Decode(s, &length);
        if (length == 1 && *s == '\\') {
            utf8::Decode(++s, &length);

            switch (*s) {
            case 'n':
                result.append("\n");
                break;
            case '\"':
                result.append("\"");
                break;
            case '\\':
                result.append("\\");
                break;
            default:
                result.append(1, '\\');
                result.append(s, length);
                break;
            }
        }
        else {
            result.append(s, length);
        }

        s += length;
    }

    if (is_protected && length == 1 && result.back() == '\"') {
        result.pop_back();
    }

    return result;
}

static auto ReadToken(const char* str, string& result) -> const char*
{
    PROFILER_ENTRY();

    if (*str == 0) {
        return nullptr;
    }

    const auto* s = str;

    uint length = 0;
    utf8::Decode(s, &length);
    while (length == 1 && (*s == ' ' || *s == '\t')) {
        utf8::Decode(++s, &length);
    }

    if (*s == 0) {
        return nullptr;
    }

    const char* begin;

    if (length == 1 && *s == '\"') {
        s++;
        begin = s;

        while (*s != 0) {
            if (length == 1 && *s == '\\') {
                ++s;
                if (*s != 0) {
                    utf8::Decode(s, &length);
                    s += length;
                }
            }
            else if (length == 1 && *s == '\"') {
                break;
            }
            else {
                s += length;
            }
            utf8::Decode(s, &length);
        }
    }
    else {
        begin = s;

        while (*s != 0) {
            if (length == 1 && *s == '\\') {
                utf8::Decode(++s, &length);
                s += length;
            }
            else if (length == 1 && (*s == ' ' || *s == '\t')) {
                break;
            }
            else {
                s += length;
            }
            utf8::Decode(s, &length);
        }
    }

    result.assign(begin, static_cast<uint>(s - begin));
    return *s != 0 ? s + 1 : s;
}
