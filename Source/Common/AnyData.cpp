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

#include "AnyData.h"

FO_BEGIN_NAMESPACE

static auto ParseValidatedScalarValue(string_view raw_value, AnyData::ValueType value_type) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    const auto value_str = string(raw_value);
    auto value = strvex(raw_value);
    value.trim();

    switch (value_type) {
    case AnyData::ValueType::Int64:
        if (!value.is_number()) {
            throw AnyDataException("Invalid int64 value", value_str);
        }
        return value.to_int64();
    case AnyData::ValueType::Float64:
        if (!value.is_number()) {
            throw AnyDataException("Invalid float64 value", value_str);
        }
        return value.to_float64();
    case AnyData::ValueType::Bool:
        if (!value.is_explicit_bool() && !value.is_number()) {
            throw AnyDataException("Invalid bool value", value_str);
        }

        if (value.is_number()) {
            const auto int_value = value.to_int64();
            const auto float_value = value.to_float64();

            if (!is_float_equal(float_value, static_cast<float64>(int_value)) || (int_value != 0 && int_value != 1)) {
                throw AnyDataException("Invalid bool numeric value", value_str);
            }
        }

        return value.to_bool();
    case AnyData::ValueType::String:
        return StringEscaping::DecodeString(raw_value);
    default:
        FO_UNREACHABLE_PLACE();
    }
}

auto AnyData::Value::operator==(const Value& other) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (Type() != other.Type()) {
        return false;
    }

    switch (Type()) {
    case ValueType::Int64:
        return AsInt64() == other.AsInt64();
    case ValueType::Float64:
        return is_float_equal(AsDouble(), other.AsDouble());
    case ValueType::Bool:
        return AsBool() == other.AsBool();
    case ValueType::String:
        return AsString() == other.AsString();
    case ValueType::Array:
        return AsArray() == other.AsArray();
    case ValueType::Dict:
        return AsDict() == other.AsDict();
    }

    FO_UNREACHABLE_PLACE();
}

auto AnyData::Value::Copy() const -> Value
{
    FO_STACK_TRACE_ENTRY();

    switch (Type()) {
    case ValueType::Int64:
        return AsInt64();
    case ValueType::Float64:
        return AsDouble();
    case ValueType::Bool:
        return AsBool();
    case ValueType::String:
        return AsString();
    case ValueType::Array:
        return AsArray().Copy();
    case ValueType::Dict:
        return AsDict().Copy();
    }

    FO_UNREACHABLE_PLACE();
}

auto AnyData::Array::Copy() const -> Array
{
    FO_STACK_TRACE_ENTRY();

    Array arr;

    for (const auto& value : _value) {
        arr.EmplaceBack(value.Copy());
    }

    return arr;
}

auto AnyData::Dict::Copy() const -> Dict
{
    FO_STACK_TRACE_ENTRY();

    Dict dict;

    for (const auto& [key, value] : *this) {
        dict.Emplace(key, value.Copy());
    }

    return dict;
}

auto AnyData::Document::Copy() const -> Document
{
    FO_STACK_TRACE_ENTRY();

    Document doc;

    for (const auto& [key, value] : *this) {
        doc.Emplace(key, value.Copy());
    }

    return doc;
}

auto AnyData::ValueToCodedString(const Value& value) -> string
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto default_buf_size = 1024;

    switch (value.Type()) {
    case ValueType::Int64:
        return strex("{}", value.AsInt64());
    case ValueType::Float64:
        return strex("{:f}", value.AsDouble()).rtrim("0").rtrim(".");
    case ValueType::Bool:
        return value.AsBool() ? "True" : "False";
    case ValueType::String:
        return StringEscaping::CodeString(value.AsString());
    case ValueType::Array: {
        string arr_str;
        arr_str.reserve(default_buf_size);
        const auto& arr = value.AsArray();
        bool next_iteration = false;

        for (const auto& arr_entry : arr) {
            if (next_iteration) {
                arr_str.append(" ");
            }
            else {
                next_iteration = true;
            }

            arr_str.append(ValueToCodedString(arr_entry));
        }

        return StringEscaping::CodeString(arr_str);
    }
    case ValueType::Dict: {
        string dict_str;
        dict_str.reserve(default_buf_size);
        const auto& dict = value.AsDict();
        bool next_iteration = false;

        for (auto&& [dict_key, dict_value] : dict) {
            if (next_iteration) {
                dict_str.append(" ");
            }
            else {
                next_iteration = true;
            }

            dict_str.append(StringEscaping::CodeString(dict_key));
            dict_str.append(" ");
            dict_str.append(ValueToCodedString(dict_value));
        }

        return StringEscaping::CodeString(dict_str);
    }
    }

    FO_UNREACHABLE_PLACE();
}

auto AnyData::ValueToString(const Value& value) -> string
{
    FO_STACK_TRACE_ENTRY();

    auto str = ValueToCodedString(value);

    if (str.length() >= 2 && str.front() == '\"' && str.back() == '\"') {
        if (str[1] != ' ' && str[1] != '\t' && str[str.length() - 2] != ' ' && str[str.length() - 2] != '\t') {
            str = StringEscaping::DecodeString(str);
        }
    }

    return str;
}

auto AnyData::ParseValue(const string& str, bool as_dict, bool as_array, ValueType value_type) -> Value
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(value_type == ValueType::Int64 || value_type == ValueType::Float64 || value_type == ValueType::Bool || value_type == ValueType::String);

    if (as_dict) {
        Dict dict;

        const char* s = str.c_str();
        string dict_key_entry;
        string dict_value_entry;

        while ((s = ReadToken(s, dict_key_entry)) != nullptr) {
            s = ReadToken(s, dict_value_entry);

            if (s == nullptr) {
                throw AnyDataException("Invalid dict value, missing entry for key", dict_key_entry);
            }

            auto dict_key = StringEscaping::DecodeString(dict_key_entry);

            if (as_array) {
                Array dict_arr;

                const auto decoded_dict_value_entry = StringEscaping::DecodeString(dict_value_entry);
                const char* s2 = decoded_dict_value_entry.c_str();
                string arr_entry;

                while ((s2 = ReadToken(s2, arr_entry)) != nullptr) {
                    dict_arr.EmplaceBack(ParseValidatedScalarValue(arr_entry, value_type));
                }

                if (!dict.Contains(dict_key)) {
                    dict.Emplace(std::move(dict_key), std::move(dict_arr));
                }
                else {
                    throw AnyDataException("Invalid dict value, duplicate key", dict_key_entry);
                }
            }
            else {
                if (!dict.Contains(dict_key)) {
                    dict.Emplace(std::move(dict_key), ParseValidatedScalarValue(dict_value_entry, value_type));
                }
                else {
                    throw AnyDataException("Invalid dict value, duplicate key", dict_key_entry);
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
            arr.EmplaceBack(ParseValidatedScalarValue(arr_entry, value_type));
        }

        return arr;
    }
    else {
        return ParseValidatedScalarValue(str, value_type);
    }

    FO_UNREACHABLE_PLACE();
}

auto AnyData::ReadToken(const char* str, string& result) -> const char*
{
    FO_STACK_TRACE_ENTRY();

    if (str[0] == 0) {
        return nullptr;
    }

    size_t pos = 0;
    size_t length = utf8::DecodeStrNtLen(&str[pos]);
    utf8::Decode(&str[pos], length);

    while (length == 1 && (str[pos] == ' ' || str[pos] == '\t')) {
        pos++;

        length = utf8::DecodeStrNtLen(&str[pos]);
        utf8::Decode(&str[pos], length);
    }

    if (str[pos] == 0) {
        return nullptr;
    }

    size_t begin;

    if (length == 1 && str[pos] == '"') {
        bool quote_closed = false;
        pos++;
        begin = pos;

        while (str[pos] != 0) {
            if (length == 1 && str[pos] == '\\') {
                pos++;

                if (str[pos] == 0) {
                    throw AnyDataException("Invalid escape sequence in quoted token", string(str));
                }

                length = utf8::DecodeStrNtLen(&str[pos]);
                utf8::Decode(&str[pos], length);

                pos += length;
            }
            else if (length == 1 && str[pos] == '"') {
                quote_closed = true;
                break;
            }
            else {
                pos += length;
            }

            length = utf8::DecodeStrNtLen(&str[pos]);
            utf8::Decode(&str[pos], length);
        }

        if (!quote_closed) {
            throw AnyDataException("Unterminated quoted token", string(str));
        }

        if (str[pos + 1] != 0 && str[pos + 1] != ' ' && str[pos + 1] != '\t') {
            throw AnyDataException("Invalid quoted token delimiter", string(str));
        }
    }
    else {
        begin = pos;

        while (str[pos] != 0) {
            if (length == 1 && str[pos] == '\\') {
                pos++;

                if (str[pos] == 0) {
                    throw AnyDataException("Invalid escape sequence in token", string(str));
                }

                length = utf8::DecodeStrNtLen(&str[pos]);
                utf8::Decode(&str[pos], length);

                pos += length;
            }
            else if (length == 1 && (str[pos] == ' ' || str[pos] == '\t')) {
                break;
            }
            else {
                pos += length;
            }

            length = utf8::DecodeStrNtLen(&str[pos]);
            utf8::Decode(&str[pos], length);
        }
    }

    result.assign(&str[begin], pos - begin);
    return str[pos] != 0 ? &str[pos + 1] : &str[pos];
}

auto StringEscaping::CodeString(string_view str) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;
    result.reserve(str.length() * 2);

    const bool protect = str.empty() || str.find_first_of(" \t\r\n\\\"") != string::npos;

    if (protect) {
        result.append(1, '\"');
    }

    for (size_t i = 0; i < str.length();) {
        const auto* s = str.data() + i;
        size_t length = str.length() - i;
        utf8::Decode(s, length);

        if (length == 1) {
            switch (*s) {
            case '\r':
                break;
            case '\n':
                result.append("\\n");
                break;
            case '\"':
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

        i += length;
    }

    if (protect) {
        result.append(1, '\"');
    }

    return result;
}

auto StringEscaping::DecodeString(string_view str) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (str.empty()) {
        return {};
    }

    string result;
    result.reserve(str.length());

    const auto* s = str.data();
    size_t length = str.length();
    utf8::Decode(s, length);

    const auto is_protected = length == 1 && *s == '\"';
    bool closing_quote_found = false;

    for (size_t i = is_protected ? 1 : 0; i < str.length();) {
        s = str.data() + i;
        length = str.length() - i;
        utf8::Decode(s, length);

        if (is_protected && length == 1 && *s == '"') {
            if (i != str.length() - 1) {
                throw AnyDataException("Unexpected closing quote inside protected string", string(str));
            }

            closing_quote_found = true;
            break;
        }

        if (length == 1 && *s == '\\') {
            i++;

            if (i >= str.length()) {
                throw AnyDataException("Invalid escape sequence in string", string(str));
            }

            s = str.data() + i;
            length = str.length() - i;
            utf8::Decode(s, length);

            switch (*s) {
            case 'r':
                break;
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

        i += length;
    }

    if (is_protected && !closing_quote_found) {
        throw AnyDataException("Unterminated protected string", string(str));
    }

    return result;
}

FO_END_NAMESPACE
