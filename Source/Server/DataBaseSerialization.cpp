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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "DataBaseSerialization.h"
#include "DataBase.h"

FO_BEGIN_NAMESPACE

namespace DataBaseSerialization
{
    static auto ValueToJson(const AnyData::Value& value, size_t depth) -> nlohmann::json
    {
        FO_VERIFY_AND_THROW(depth <= MAX_DOCUMENT_DEPTH, "Database document nesting limit exceeded", depth);

        switch (value.Type()) {
        case AnyData::ValueType::Int64:
            return value.AsInt64();
        case AnyData::ValueType::Float64: {
            float64_t double_value = value.AsDouble();

            if (!std::isfinite(double_value)) {
                throw DataBaseException("ValueToJson non-finite double", double_value);
            }

            return double_value;
        }
        case AnyData::ValueType::Bool:
            return value.AsBool();
        case AnyData::ValueType::String:
            return string(value.AsString());
        case AnyData::ValueType::Array: {
            auto arr_json = nlohmann::json::array();

            for (const auto& arr_entry : value.AsArray()) {
                arr_json.emplace_back(ValueToJson(arr_entry, depth + 1));
            }

            return arr_json;
        }
        case AnyData::ValueType::Dict: {
            auto dict_json = nlohmann::json::object();

            for (auto&& [dict_key, dict_value] : value.AsDict()) {
                dict_json[nlohmann::json::string_t(dict_key.data(), dict_key.size())] = ValueToJson(dict_value, depth + 1);
            }

            return dict_json;
        }
        }

        throw DataBaseException("Invalid AnyData value type for database JSON", static_cast<int>(value.Type()));
    }

    static auto ValueFromJson(const nlohmann::json& value, size_t depth) -> AnyData::Value
    {
        FO_VERIFY_AND_THROW(depth <= MAX_DOCUMENT_DEPTH, "Database document nesting limit exceeded", depth);

        if (value.is_number_unsigned()) {
            return numeric_cast<int64_t>(value.get<uint64_t>());
        }
        if (value.is_number_integer()) {
            return value.get<int64_t>();
        }
        if (value.is_number_float()) {
            float64_t double_value = value.get<float64_t>();

            if (!std::isfinite(double_value)) {
                throw DataBaseException("ValueFromJson non-finite double", double_value);
            }

            return double_value;
        }
        if (value.is_boolean()) {
            return value.get<bool>();
        }
        if (value.is_string()) {
            return value.get<string>();
        }
        if (value.is_array()) {
            AnyData::Array arr;

            for (const auto& arr_entry : value) {
                arr.EmplaceBack(ValueFromJson(arr_entry, depth + 1));
            }

            return std::move(arr);
        }
        if (value.is_object()) {
            AnyData::Dict dict;

            for (auto&& [dict_key, dict_value] : value.items()) {
                dict.Emplace(string(dict_key), ValueFromJson(dict_value, depth + 1));
            }

            return std::move(dict);
        }

        throw DataBaseException("Invalid database JSON value type");
    }

    auto ToJson(const AnyData::Document& doc) -> nlohmann::json
    {
        auto doc_json = nlohmann::json::object();

        for (auto&& [doc_key, doc_value] : doc) {
            doc_json[nlohmann::json::string_t(doc_key.data(), doc_key.size())] = ValueToJson(doc_value, 1);
        }

        return doc_json;
    }

    auto FromJson(const nlohmann::json& doc_json) -> AnyData::Document
    {
        if (!doc_json.is_object()) {
            throw DataBaseException("Invalid database JSON document");
        }

        AnyData::Document doc;

        for (auto&& [doc_key, doc_value] : doc_json.items()) {
            doc.Emplace(string(doc_key), ValueFromJson(doc_value, 1));
        }

        return doc;
    }

    auto FromJsonText(string_view text) -> AnyData::Document
    {
        FO_TRACE_ZONE(Database);

        FO_VERIFY_AND_THROW(text.size() <= MAX_DOCUMENT_BYTES, "Database document size limit exceeded", text.size());
        auto check_depth = [](int32_t depth, nlohmann::json::parse_event_t, nlohmann::json&) -> bool {
            FO_VERIFY_AND_THROW(depth >= 0 && numeric_cast<size_t>(depth) <= MAX_DOCUMENT_DEPTH, "Database document nesting limit exceeded", depth);
            return true;
        };

        return FromJson(nlohmann::json::parse(text.begin(), text.end(), check_depth));
    }

#if FO_DATABASE_SQLITE
    // SAX callback types belong to the foreign JSON API. Validate before its DOM parser
    // can allocate containers from lengths supplied by an untrusted CBOR record
    class CborValidator final : public nlohmann::json_sax<nlohmann::json>
    {
    public:
        explicit CborValidator(size_t input_size) :
            _inputSize {input_size}
        {
        }

        auto null() -> bool override { return false; }
        auto boolean(bool) -> bool override { return true; }
        auto number_integer(number_integer_t) -> bool override { return true; }
        auto number_unsigned(number_unsigned_t value) -> bool override { return value <= numeric_cast<uint64_t>(std::numeric_limits<int64_t>::max()); }
        auto number_float(number_float_t value, const string_t&) -> bool override { return std::isfinite(value); }
        auto string(string_t&) -> bool override { return true; }
        auto binary(binary_t&) -> bool override { return false; }
        auto start_object(size_t count) -> bool override { return Start(count); }
        auto key(string_t&) -> bool override { return true; }
        auto end_object() -> bool override
        {
            _depth--;
            return true;
        }
        auto start_array(size_t count) -> bool override { return Start(count); }
        auto end_array() -> bool override
        {
            _depth--;
            return true;
        }
        auto parse_error(size_t, const string_t&, const nlohmann::detail::exception&) -> bool override { return false; }

    private:
        auto Start(size_t count) -> bool
        {
            _depth++;
            // The root document is not an AnyData value; count nested values from one
            return _depth <= MAX_DOCUMENT_DEPTH + 1 && (count == std::numeric_limits<size_t>::max() || count <= _inputSize);
        }

        size_t _depth {};
        size_t _inputSize;
    };

    auto ToCbor(const AnyData::Document& doc) -> vector<uint8_t>
    {
        FO_TRACE_ZONE(Database);

        vector<uint8_t> data;
        nlohmann::json::to_cbor(ToJson(doc), data);
        FO_VERIFY_AND_THROW(data.size() <= MAX_DOCUMENT_BYTES, "Database document size limit exceeded", data.size());
        return data;
    }

    auto FromCbor(const_span<uint8_t> data) -> AnyData::Document
    {
        FO_TRACE_ZONE(Database);

        FO_VERIFY_AND_THROW(data.size() <= MAX_DOCUMENT_BYTES, "Database document size limit exceeded", data.size());
        CborValidator validator {data.size()};
        bool valid = nlohmann::json::sax_parse(data.begin(), data.end(), &validator, nlohmann::json::input_format_t::cbor);
        FO_VERIFY_AND_THROW(valid, "Invalid database CBOR document");

        return FromJson(nlohmann::json::from_cbor(data.begin(), data.end()));
    }
#endif
}

FO_END_NAMESPACE
