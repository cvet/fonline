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

static auto ParseValidatedScalarValue(u8string_view raw_value, AnyData::ValueType value_type) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    if (value_type == AnyData::ValueType::String) {
        return StringEscaping::DecodeString(raw_value);
    }

    if (validate_ascii_text(raw_value.native_view())) {
        throw AnyDataException("Invalid non-ASCII scalar value", raw_value);
    }

    string ascii_value = utf8_to_string(raw_value);
    auto value = strvex(ascii_value);
    value.trim();

    switch (value_type) {
    case AnyData::ValueType::Int64:
        if (!value.is_number()) {
            throw AnyDataException("Invalid int64 value", ascii_value);
        }

        return value.to_int64();
    case AnyData::ValueType::Float64: {
        if (!value.is_number()) {
            throw AnyDataException("Invalid float64 value", ascii_value);
        }

        float64_t parsed_value = value.to_float64();

        if (!std::isfinite(parsed_value)) {
            throw AnyDataException("Invalid float64 value", ascii_value);
        }

        return parsed_value;
    }
    case AnyData::ValueType::Bool:
        if (!value.is_explicit_bool() && !value.is_number()) {
            throw AnyDataException("Invalid bool value", ascii_value);
        }

        if (value.is_number()) {
            int64_t int_value = value.to_int64();
            float64_t float_value = value.to_float64();

            if (!is_float_equal(float_value, static_cast<float64_t>(int_value)) || (int_value != 0 && int_value != 1)) {
                throw AnyDataException("Invalid bool numeric value", ascii_value);
            }
        }

        return value.to_bool();
    case AnyData::ValueType::String:
        FO_UNREACHABLE_PLACE();
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
        return u8string {AsString()};
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

auto AnyData::ValueToCodedString(const Value& value) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    constexpr int32_t default_buf_size = 1024;

    switch (value.Type()) {
    case ValueType::Int64:
        return FormatUtf8("{}", value.AsInt64());
    case ValueType::Float64: {
        float64_t float_value = value.AsDouble();

        if (!std::isfinite(float_value)) {
            throw AnyDataException("Cannot serialize non-finite float64 value", float_value);
        }

        string formatted_value = strex("{:f}", float_value).rtrim("0").rtrim(".");
        return formatted_value;
    }
    case ValueType::Bool:
        return value.AsBool() ? u8string {u8"True"} : u8string {u8"False"};
    case ValueType::String:
        return StringEscaping::CodeString(value.AsString());
    case ValueType::Array: {
        u8string arr_str;
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

            auto coded_value = ValueToCodedString(arr_entry);
            arr_str.append(coded_value.view());
        }

        return StringEscaping::CodeString(arr_str.view());
    }
    case ValueType::Dict: {
        u8string dict_str;
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

            auto coded_key = StringEscaping::CodeString(dict_key.view());
            dict_str.append(coded_key.view());
            dict_str.append(" ");
            auto coded_value = ValueToCodedString(dict_value);
            dict_str.append(coded_value.view());
        }

        return StringEscaping::CodeString(dict_str.view());
    }
    }

    FO_UNREACHABLE_PLACE();
}

auto AnyData::ValueToString(const Value& value) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    u8string str = ValueToCodedString(value);

    auto str_view = str.view().native_view();

    if (str_view.length() >= 2 && str_view.front() == u8'\"' && str_view.back() == u8'\"') {
        if (str_view[1] != u8' ' && str_view[1] != u8'\t' && str_view[str_view.length() - 2] != u8' ' && str_view[str_view.length() - 2] != u8'\t') {
            str = StringEscaping::DecodeString(str.view());
        }
    }

    return str;
}

auto AnyData::ParseValue(u8string_view str, bool as_dict, bool as_array, ValueType value_type) -> Value
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(value_type == ValueType::Int64 || value_type == ValueType::Float64 || value_type == ValueType::Bool || value_type == ValueType::String, "AnyData value type cannot be converted to raw payload");

    if (as_dict) {
        Dict dict;

        size_t pos = 0;
        u8string dict_key_entry;
        u8string dict_value_entry;

        while (ReadToken(str, pos, dict_key_entry)) {
            if (!ReadToken(str, pos, dict_value_entry)) {
                throw AnyDataException("Invalid dict value, missing entry for key", dict_key_entry.view());
            }

            auto dict_key = StringEscaping::DecodeString(dict_key_entry.view());

            if (as_array) {
                Array dict_arr;

                auto decoded_dict_value_entry = StringEscaping::DecodeString(dict_value_entry.view());
                size_t nested_pos = 0;
                u8string arr_entry;

                while (ReadToken(decoded_dict_value_entry.view(), nested_pos, arr_entry)) {
                    dict_arr.EmplaceBack(ParseValidatedScalarValue(arr_entry.view(), value_type));
                }

                if (!dict.Contains(dict_key.view())) {
                    dict.Emplace(std::move(dict_key), std::move(dict_arr));
                }
                else {
                    throw AnyDataException("Invalid dict value, duplicate key", dict_key_entry.view());
                }
            }
            else {
                if (!dict.Contains(dict_key.view())) {
                    dict.Emplace(std::move(dict_key), ParseValidatedScalarValue(dict_value_entry.view(), value_type));
                }
                else {
                    throw AnyDataException("Invalid dict value, duplicate key", dict_key_entry.view());
                }
            }
        }

        return dict;
    }
    else if (as_array) {
        Array arr;

        size_t pos = 0;
        u8string arr_entry;

        while (ReadToken(str, pos, arr_entry)) {
            arr.EmplaceBack(ParseValidatedScalarValue(arr_entry.view(), value_type));
        }

        return arr;
    }
    else {
        return ParseValidatedScalarValue(str, value_type);
    }
}

auto AnyData::ParseValue(const u8string& str, bool as_dict, bool as_array, ValueType value_type) -> Value
{
    FO_STACK_TRACE_ENTRY();

    return ParseValue(str.view(), as_dict, as_array, value_type);
}

auto AnyData::ReadToken(u8string_view str, size_t& pos, u8string& result) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto source = str.native_view();
    size_t cursor = pos;

    while (cursor < source.size() && (source[cursor] == u8' ' || source[cursor] == u8'\t')) {
        cursor++;
    }

    if (cursor == source.size()) {
        pos = cursor;
        return false;
    }

    size_t begin;

    if (source[cursor] == u8'"') {
        bool quote_closed = false;
        cursor++;
        begin = cursor;

        while (cursor < source.size()) {
            if (source[cursor] == u8'\\') {
                cursor++;

                if (cursor == source.size()) {
                    throw AnyDataException("Invalid escape sequence in quoted token", str);
                }

                cursor++;
            }
            else if (source[cursor] == u8'"') {
                quote_closed = true;
                break;
            }
            else {
                cursor++;
            }
        }

        if (!quote_closed) {
            throw AnyDataException("Unterminated quoted token", str);
        }

        if (cursor + 1 < source.size() && source[cursor + 1] != u8' ' && source[cursor + 1] != u8'\t') {
            throw AnyDataException("Invalid quoted token delimiter", str);
        }

        u8string parsed_result {u8string_view::FromChecked(source.substr(begin, cursor - begin))};
        result = std::move(parsed_result);
        cursor++;
    }
    else {
        begin = cursor;

        while (cursor < source.size()) {
            if (source[cursor] == u8'\\') {
                cursor++;

                if (cursor == source.size()) {
                    throw AnyDataException("Invalid escape sequence in token", str);
                }

                cursor++;
            }
            else if (source[cursor] == u8' ' || source[cursor] == u8'\t') {
                break;
            }
            else {
                cursor++;
            }
        }

        u8string parsed_result {u8string_view::FromChecked(source.substr(begin, cursor - begin))};
        result = std::move(parsed_result);
    }

    pos = cursor;
    return true;
}

void StringEscaping::AppendCodeString(u8string& result, u8string_view str)
{
    FO_STACK_TRACE_ENTRY();

    auto source = str.native_view();

    if (auto issue = validate_utf8_text(source)) {
        throw TextValidationException(TextEncoding::Utf8, issue->Error, issue->Offset);
    }

    bool protect = source.empty() || source.find_first_of(u8" \t\r\n\\\"") != std::u8string_view::npos;

    if (protect) {
        result.append("\"");
    }

    size_t segment_begin = 0;

    for (size_t i = 0; i < source.size(); i++) {
        string_view replacement;

        switch (source[i]) {
        case u8'\r':
            replacement = "";
            break;
        case u8'\n':
            replacement = "\\n";
            break;
        case u8'\"':
            replacement = "\\\"";
            break;
        case u8'\\':
            replacement = "\\\\";
            break;
        default:
            continue;
        }

        result.append(u8string_view::FromChecked(source.substr(segment_begin, i - segment_begin)));
        result.append(replacement);
        segment_begin = i + 1;
    }

    result.append(u8string_view::FromChecked(source.substr(segment_begin)));

    if (protect) {
        result.append("\"");
    }
}

auto StringEscaping::CodeString(u8string_view str) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    u8string result;
    result.reserve(str.size() * 2);
    AppendCodeString(result, str);

    return result;
}

auto StringEscaping::DecodeString(u8string_view str) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    if (str.empty()) {
        return {};
    }

    auto source = str.native_view();
    bool is_protected = source.front() == u8'\"';
    bool closing_quote_found = false;
    size_t segment_begin = is_protected ? 1 : 0;
    u8string result;
    result.reserve(str.size());

    for (size_t i = segment_begin; i < source.size(); i++) {
        if (is_protected && source[i] == u8'\"') {
            if (i != source.size() - 1) {
                throw AnyDataException("Unexpected closing quote inside protected string", str);
            }

            result.append(u8string_view::FromChecked(source.substr(segment_begin, i - segment_begin)));
            closing_quote_found = true;
            segment_begin = i + 1;
            break;
        }

        if (source[i] != u8'\\') {
            continue;
        }

        result.append(u8string_view::FromChecked(source.substr(segment_begin, i - segment_begin)));
        i++;

        if (i >= source.size()) {
            throw AnyDataException("Invalid escape sequence in string", str);
        }

        switch (source[i]) {
        case u8'r':
            segment_begin = i + 1;
            break;
        case u8'n':
            result.append("\n");
            segment_begin = i + 1;
            break;
        case u8'\"':
            result.append("\"");
            segment_begin = i + 1;
            break;
        case u8'\\':
            result.append("\\");
            segment_begin = i + 1;
            break;
        default:
            result.append("\\");
            segment_begin = i;
            break;
        }
    }

    if (is_protected && !closing_quote_found) {
        throw AnyDataException("Unterminated protected string", str);
    }

    if (!closing_quote_found) {
        result.append(u8string_view::FromChecked(source.substr(segment_begin)));
    }

    return result;
}

void StringEscaping::AppendCodeString(string& result, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    bool protect = str.empty() || str.find_first_of(" \t\r\n\\\"") != string::npos;

    if (protect) {
        result.append(1, '\"');
    }

    for (size_t i = 0; i < str.length();) {
        auto s = make_ptr(str.data()).offset(i);
        size_t length = str.length() - i;
        (void)utf8::Decode(s, length);

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
                result.append(s.get(), 1);
                break;
            }
        }
        else {
            result.append(s.get(), length);
        }

        i += length;
    }

    if (protect) {
        result.append(1, '\"');
    }
}

auto StringEscaping::CodeString(string_view str) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;
    result.reserve(str.length() * 2);
    AppendCodeString(result, str);

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

    auto s = make_ptr(str.data());
    size_t length = str.length();
    (void)utf8::Decode(s, length);

    bool is_protected = length == 1 && *s == '\"';
    bool closing_quote_found = false;

    for (size_t i = is_protected ? 1 : 0; i < str.length();) {
        s = make_ptr(str.data()).offset(i);
        length = str.length() - i;
        (void)utf8::Decode(s, length);

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

            s = make_ptr(str.data()).offset(i);
            length = str.length() - i;
            (void)utf8::Decode(s, length);

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
                result.append(s.get(), length);
                break;
            }
        }
        else {
            result.append(s.get(), length);
        }

        i += length;
    }

    if (is_protected && !closing_quote_found) {
        throw AnyDataException("Unterminated protected string", string(str));
    }

    return result;
}

FO_END_NAMESPACE
