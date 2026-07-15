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

#include "PropertiesSerializator.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

static auto RawBytesEqual(span<const uint8_t> lhs, span<const uint8_t> rhs) -> bool;

auto PropertiesSerializator::SaveToDocument(ptr<const Properties> props, nptr<const Properties> base, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Document
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!base || props->GetRegistrator() == base->GetRegistrator(), "Serialized properties use a different base registrator");

    AnyData::Document doc;

    for (size_t i = 1; i < props->GetRegistrator()->GetPropertiesCount(); i++) {
        auto prop = props->GetRegistrator()->GetPropertyByIndex(numeric_cast<int32_t>(i));
        FO_VERIFY_AND_THROW(prop, "Property is null");

        if (prop->IsDisabled()) {
            continue;
        }
        if (!prop->IsPersistent()) {
            continue;
        }

        props->ValidateForRawData(prop);

        // Skip same as in base or zero values
        if (base) {
            const auto base_raw_data = base->GetRawData(prop);
            const auto raw_data = props->GetRawData(prop);

            if (RawBytesEqual(raw_data, base_raw_data)) {
                continue;
            }
        }
        else {
            const auto raw_data = props->GetRawData(prop);

            if (prop->IsPlainData()) {
                if (std::ranges::all_of(raw_data, [](uint8_t value) noexcept { return value == 0; })) {
                    continue;
                }
            }
            else {
                if (raw_data.empty()) {
                    continue;
                }
            }
        }

        auto value = SavePropertyToValue(props, prop, hash_resolver, name_resolver);
        doc.Emplace(string {prop->GetName()}, std::move(value));
    }

    return doc;
}

auto PropertiesSerializator::LoadFromDocument(ptr<Properties> props, const AnyData::Document& doc, HashResolver& hash_resolver, NameResolver& name_resolver) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(props.get(), "Missing required properties to load into");

    bool is_error = false;

    for (auto&& [doc_key, doc_value] : doc) {
        // Skip technical fields
        if (doc_key.empty() || doc_key[0] == '$' || doc_key[0] == '_') {
            continue;
        }

        try {
            // Find property
            auto prop = props->GetRegistrator()->FindProperty(doc_key);

            if (prop && !prop->IsDisabled() && prop->IsPersistent()) {
                LoadPropertyFromValue(props, prop, doc_value, hash_resolver, name_resolver);
            }
        }
        catch (const std::exception& ex) {
            WriteLog(LogType::Warning, "Unable to load property {}: {}", doc_key, ex.what());
            is_error = true;
        }
    }

    return !is_error;
}

auto PropertiesSerializator::SavePropertyToValue(ptr<const Properties> props, ptr<const Property> prop, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(!prop->IsVirtual(), "Property is virtual");

    props->ValidateForRawData(prop);

    const auto raw_data = props->GetRawData(prop);

    return SavePropertyToValue(prop, raw_data, hash_resolver, name_resolver);
}

static auto NormalizeTopLevelCodedString(string str) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
        if (str[1] != ' ' && str[1] != '\t' && str[str.length() - 2] != ' ' && str[str.length() - 2] != '\t') {
            str = StringEscaping::DecodeString(str);
        }
    }

    return str;
}

static auto ReadTextTokenView(ptr<const char> str, string_view& result) -> nptr<const char>
{
    FO_STACK_TRACE_ENTRY();

    if (str[0] == 0) {
        return nullptr;
    }

    const auto decode_char = [str](size_t char_pos, size_t& char_len) {
        char_len = utf8::DecodeStrNtLen(&str[char_pos]);
        utf8::Decode(&str[char_pos], char_len);
    };

    size_t pos = 0;
    size_t length = 0;
    decode_char(pos, length);

    while (length == 1 && (str[pos] == ' ' || str[pos] == '\t')) {
        pos++;

        decode_char(pos, length);
    }

    if (str[pos] == 0) {
        return nullptr;
    }

    size_t begin;

    if (length == 1 && str[pos] == '"') {
        pos++;
        begin = pos;

        while (str[pos] != 0) {
            if (length == 1 && str[pos] == '\\') {
                pos++;

                if (str[pos] != 0) {
                    decode_char(pos, length);

                    pos += length;
                }
            }
            else if (length == 1 && str[pos] == '"') {
                break;
            }
            else {
                pos += length;
            }

            decode_char(pos, length);
        }
    }
    else {
        begin = pos;

        while (str[pos] != 0) {
            if (length == 1 && str[pos] == '\\') {
                pos++;

                decode_char(pos, length);

                pos += length;
            }
            else if (length == 1 && (str[pos] == ' ' || str[pos] == '\t')) {
                break;
            }
            else {
                pos += length;
            }

            decode_char(pos, length);
        }
    }

    auto next_token = make_ptr(&str[pos + (str[pos] != 0 ? 1 : 0)]);
    result = string_view {&str[begin], pos - begin};
    return next_token;
}

static auto RawBytesPtr(span<const uint8_t> data) -> ptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!data.empty(), "Raw byte span is empty");

    return data.data();
}

static auto RawVectorBytesPtr(vector<uint8_t>& data) -> ptr<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!data.empty(), "Raw byte vector is empty");

    return data.data();
}

template<typename T>
    requires(!std::is_const_v<T>)
static auto RawMutableObjectBytes(T& value) noexcept -> ptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(std::is_trivially_copyable_v<T>);

    auto value_ptr = make_ptr(&value);
    return value_ptr.template reinterpret_as<uint8_t>();
}

template<typename T>
static auto RawObjectBytes(const T& value) noexcept -> ptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(std::is_trivially_copyable_v<T>);

    auto value_ptr = make_ptr(&value);
    return value_ptr.template reinterpret_as<const uint8_t>();
}

static auto RawWritePtrAt(ptr<uint8_t> data, size_t pos) noexcept -> ptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    return data.offset(pos);
}

static auto RawBytesEqual(span<const uint8_t> lhs, span<const uint8_t> rhs) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (lhs.size() != rhs.size()) {
        return false;
    }
    if (lhs.empty()) {
        return true;
    }

    auto lhs_data = RawBytesPtr(lhs);
    auto rhs_data = RawBytesPtr(rhs);
    return MemCompare(lhs_data, rhs_data, lhs.size());
}

template<typename T>
static auto ReadRawValue(span<const uint8_t> data) -> T
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(data.size() >= sizeof(T), "Raw byte span is too small to hold the requested value");

    T value {};
    auto source = RawBytesPtr(data);
    auto target = RawMutableObjectBytes(value);
    MemCopy(target, source, sizeof(value));
    return value;
}

struct RawReadCursor final
{
    span<const uint8_t> Data {};
    size_t Pos {};
};

struct RawWriteCursor final
{
    nptr<uint8_t> Data {};
    size_t Pos;
};

static auto TakeRawBytes(span<const uint8_t> raw_data, size_t& data_pos, size_t size) -> span<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(data_pos <= raw_data.size(), "Raw data read position is past the end of the buffer");
    FO_VERIFY_AND_THROW(raw_data.size() - data_pos >= size, "Raw data buffer has fewer remaining bytes than requested");

    auto result = raw_data.subspan(data_pos, size);
    data_pos += size;
    return result;
}

static auto ReadCursorBytes(RawReadCursor& cursor, size_t size) -> span<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    return TakeRawBytes(cursor.Data, cursor.Pos, size);
}

template<typename T>
static auto ReadCursorValue(RawReadCursor& cursor) -> T
{
    FO_STACK_TRACE_ENTRY();

    cursor.Pos = align_up(cursor.Pos, alignment_for_size(sizeof(T)));
    return ReadRawValue<T>(ReadCursorBytes(cursor, sizeof(T)));
}

template<typename T>
static auto ReadFiniteRawFloat(span<const uint8_t> data, string_view type_name) -> T
{
    FO_STACK_TRACE_ENTRY();

    const T value = ReadRawValue<T>(data);

    if (!std::isfinite(value)) {
        throw PropertySerializationException("Numeric value is not finite", type_name, value);
    }

    return value;
}

template<typename T>
static auto ReadFiniteCursorFloat(RawReadCursor& cursor, string_view type_name) -> T
{
    FO_STACK_TRACE_ENTRY();

    const T value = ReadCursorValue<T>(cursor);

    if (!std::isfinite(value)) {
        throw PropertySerializationException("Numeric value is not finite", type_name, value);
    }

    return value;
}

static auto ReadCursorEnumValue(RawReadCursor& cursor, size_t size) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size <= sizeof(int32_t), "Enum value size is larger than its int32 storage");

    cursor.Pos = align_up(cursor.Pos, alignment_for_size(size));

    int32_t enum_value = 0;
    const auto bytes = ReadCursorBytes(cursor, size);

    if (!bytes.empty()) {
        auto bytes_ptr = RawBytesPtr(bytes);
        auto enum_value_ptr = RawMutableObjectBytes(enum_value);
        MemCopy(enum_value_ptr, bytes_ptr, bytes.size());
    }

    return enum_value;
}

static auto TakePropertyRawBytes(ptr<const Property> prop, string_view message, span<const uint8_t> raw_data, size_t& data_pos, size_t size) -> span<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    if (data_pos > raw_data.size() || raw_data.size() - data_pos < size) {
        throw PropertySerializationException(message, prop->GetName());
    }

    return TakeRawBytes(raw_data, data_pos, size);
}

static void AlignPropertyRawPos(ptr<const Property> prop, string_view message, span<const uint8_t> raw_data, size_t& data_pos, size_t alignment)
{
    FO_STACK_TRACE_ENTRY();

    data_pos = align_up(data_pos, alignment);

    if (data_pos > raw_data.size()) {
        throw PropertySerializationException(message, prop->GetName());
    }
}

static auto ReadPropertyRawUInt32(ptr<const Property> prop, string_view message, span<const uint8_t> raw_data, size_t& data_pos) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    AlignPropertyRawPos(prop, message, raw_data, data_pos, sizeof(uint32_t));
    const auto bytes = TakePropertyRawBytes(prop, message, raw_data, data_pos, sizeof(uint32_t));
    return ReadRawValue<uint32_t>(bytes);
}

static void WriteRawBytes(ptr<uint8_t> target, size_t& data_pos, nptr<const void> source, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    if (size != 0) {
        FO_VERIFY_AND_THROW(source, "Raw write source is null for a non-empty copy");
        auto target_pos = RawWritePtrAt(target, data_pos);
        MemCopy(target_pos, source, size);
    }

    data_pos += size;
}

static void WriteRawUInt32(ptr<uint8_t> target, size_t& data_pos, uint32_t value)
{
    FO_STACK_TRACE_ENTRY();

    data_pos = align_up(data_pos, sizeof(uint32_t));

    auto value_bytes = RawObjectBytes(value);
    WriteRawBytes(target, data_pos, value_bytes, sizeof(value));
}

static void WriteRawSpan(ptr<uint8_t> target, size_t& data_pos, span<const uint8_t> source)
{
    FO_STACK_TRACE_ENTRY();

    if (source.empty()) {
        return;
    }

    auto source_ptr = RawBytesPtr(source);
    WriteRawBytes(target, data_pos, source_ptr, source.size());
}

static void WriteCursorBytes(RawWriteCursor& cursor, nptr<const void> source, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(cursor.Data, "Write cursor has no target buffer");
    WriteRawBytes(cursor.Data, cursor.Pos, source, size);
}

template<typename T>
static void WriteCursorValue(RawWriteCursor& cursor, T value)
{
    FO_STACK_TRACE_ENTRY();

    cursor.Pos = align_up(cursor.Pos, alignment_for_size(sizeof(T)));

    auto value_bytes = RawObjectBytes(value);
    WriteCursorBytes(cursor, value_bytes, sizeof(value));
}

static void WriteCursorEnumValue(RawWriteCursor& cursor, int32_t enum_value, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size <= sizeof(int32_t), "Enum value size is larger than its int32 storage");

    cursor.Pos = align_up(cursor.Pos, alignment_for_size(size));

    if (size == sizeof(uint8_t)) {
        WriteCursorValue(cursor, numeric_cast<uint8_t>(enum_value));
    }
    else if (size == sizeof(uint16_t)) {
        WriteCursorValue(cursor, numeric_cast<uint16_t>(enum_value));
    }
    else {
        auto enum_value_bytes = RawObjectBytes(enum_value);
        WriteCursorBytes(cursor, enum_value_bytes, size);
    }
}

static void WriteRawString(ptr<uint8_t> target, size_t& data_pos, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    if (value.empty()) {
        return;
    }

    WriteRawSpan(target, data_pos, make_const_span(value));
}

static void AppendRawBytes(vector<uint8_t>& data, nptr<const void> value, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    if (size == 0) {
        return;
    }

    const auto old_size = data.size();
    data.resize(old_size + size);

    FO_VERIFY_AND_THROW(value, "Appended raw source is null for a non-empty copy");

    auto target = RawVectorBytesPtr(data);
    size_t data_pos = old_size;
    WriteRawBytes(target, data_pos, value, size);
    FO_VERIFY_AND_THROW(data_pos == data.size(), "Appended bytes did not fill the grown buffer exactly");
}

static void AppendRawBytes(vector<uint8_t>& data, span<const uint8_t> value)
{
    FO_STACK_TRACE_ENTRY();

    if (value.empty()) {
        return;
    }

    auto value_ptr = RawBytesPtr(value);
    AppendRawBytes(data, value_ptr, value.size());
}

static void AlignRawBuffer(vector<uint8_t>& data, size_t alignment)
{
    FO_STACK_TRACE_ENTRY();

    data.resize(align_up(data.size(), alignment));
}

static void AppendRawScalarBytes(vector<uint8_t>& data, nptr<const void> value, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    AlignRawBuffer(data, alignment_for_size(size));
    AppendRawBytes(data, value, size);
}

static void AppendRawString(vector<uint8_t>& data, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    const auto str_len = numeric_cast<uint32_t>(str.length());
    AppendRawScalarBytes(data, &str_len, sizeof(str_len));

    AppendRawBytes(data, make_const_span(str));
}

static auto DecodeTextIfNeeded(string_view text, string& decoded_storage) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (text.find_first_of("\\\"") == string_view::npos) {
        return text;
    }

    decoded_storage = StringEscaping::DecodeString(text);
    return decoded_storage;
}

template<typename T>
static auto GetIntegralMaxFloat64() noexcept -> float64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    float64_t max_value = static_cast<float64_t>(std::numeric_limits<T>::max());

    if constexpr (std::same_as<T, int64_t>) {
        max_value = std::nextafter(max_value, 0.0);
    }

    return max_value;
}

static auto ParseStrictFloatText(string_view text) -> float64_t;

static auto ParseStrictIntText(string_view text) -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    auto value = strvex(text);
    value.trim();
    const string_view str = value.strv();

    if (str.empty()) {
        throw PropertySerializationException("Invalid numeric value", text);
    }

    const char* ptr = str.data();
    const char* end_ptr = ptr + str.length();
    bool negative = false;
    int32_t base = 10;

    if (*ptr == '-') {
        ptr++;
        negative = true;
    }

    if (end_ptr - ptr >= 2 && ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
        ptr += 2;
        base = 16;
    }

    if (ptr == end_ptr) {
        throw PropertySerializationException("Invalid numeric value", text);
    }

    uint64_t parsed_value = 0;
    const auto parse_result = std::from_chars(ptr, end_ptr, parsed_value, base);

    if (parse_result.ec != std::errc() || parse_result.ptr != end_ptr) {
        if (parse_result.ec == std::errc::result_out_of_range) {
            throw PropertySerializationException("Numeric value out of range", text);
        }

        if (base == 10) {
            const float64_t float_value = ParseStrictFloatText(str);
            const float64_t min_value = static_cast<float64_t>(std::numeric_limits<int64_t>::min());
            const float64_t max_value = GetIntegralMaxFloat64<int64_t>();
            const float64_t comparable_value = std::trunc(float_value);

            if (comparable_value < min_value || comparable_value > max_value) {
                throw PropertySerializationException("Numeric value out of range", text);
            }

            return static_cast<int64_t>(float_value);
        }

        throw PropertySerializationException("Invalid numeric value", text);
    }

    constexpr auto max_value = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    constexpr auto min_abs_value = max_value + 1U;

    if (negative) {
        if (parsed_value > min_abs_value) {
            throw PropertySerializationException("Numeric value out of range", text);
        }

        return parsed_value == min_abs_value ? std::numeric_limits<int64_t>::min() : -static_cast<int64_t>(parsed_value);
    }

    if (parsed_value > max_value) {
        throw PropertySerializationException("Numeric value out of range", text);
    }

    return static_cast<int64_t>(parsed_value);
}

static auto ParseStrictFloatText(string_view text) -> float64_t
{
    FO_STACK_TRACE_ENTRY();

    auto value = strvex(text);
    value.trim();
    const string_view str = value.strv();

    if (str.empty()) {
        throw PropertySerializationException("Invalid numeric value", text);
    }

    const char* ptr = str.data();
    const char* end_ptr = ptr + str.length();

    if (str.back() == 'f') {
        end_ptr -= 1;
    }

    if (ptr == end_ptr) {
        throw PropertySerializationException("Invalid numeric value", text);
    }

    float64_t parsed_value = 0.0;
    const auto parse_result = std::from_chars(ptr, end_ptr, parsed_value);

    if (parse_result.ec != std::errc() || parse_result.ptr != end_ptr) {
        throw PropertySerializationException("Invalid numeric value", text);
    }
    if (!std::isfinite(parsed_value)) {
        throw PropertySerializationException("Numeric value out of range", text);
    }

    return parsed_value;
}

static auto ParseStrictBoolText(string_view text) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto value = strvex(text);
    value.trim();

    if (value.is_explicit_bool()) {
        return value.to_bool();
    }

    const auto float_value = ParseStrictFloatText(value.strv());
    const auto int_value = std::trunc(float_value);

    if (!is_float_equal(float_value, int_value) || (int_value != 0.0 && int_value != 1.0)) {
        throw PropertySerializationException("Invalid bool numeric value", text);
    }

    return int_value != 0.0;
}

static void AppendBaseTypeFromText(vector<uint8_t>& data, ptr<const Property> prop, const BaseTypeDesc& base_type, string_view text, HashResolver& hash_resolver, NameResolver& name_resolver);

template<typename T>
static auto ConvertFloat64ToNumber(float64_t value) -> T;

static void AppendPrimitiveFromText(vector<uint8_t>& data, const BaseTypeDesc& primitive_type, string_view text)
{
    FO_STACK_TRACE_ENTRY();

    if (primitive_type.IsInt8) {
        const auto value = numeric_cast<int8_t>(ParseStrictIntText(text));
        AppendRawScalarBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsInt16) {
        const auto value = numeric_cast<int16_t>(ParseStrictIntText(text));
        AppendRawScalarBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsInt32) {
        const auto value = numeric_cast<int32_t>(ParseStrictIntText(text));
        AppendRawScalarBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsInt64) {
        const auto value = numeric_cast<int64_t>(ParseStrictIntText(text));
        AppendRawScalarBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsUInt8) {
        const auto value = numeric_cast<uint8_t>(ParseStrictIntText(text));
        AppendRawScalarBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsUInt16) {
        const auto value = numeric_cast<uint16_t>(ParseStrictIntText(text));
        AppendRawScalarBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsUInt32) {
        const auto value = numeric_cast<uint32_t>(ParseStrictIntText(text));
        AppendRawScalarBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsSingleFloat) {
        const auto value = ConvertFloat64ToNumber<float32_t>(ParseStrictFloatText(text));
        AppendRawScalarBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsDoubleFloat) {
        const auto value = numeric_cast<float64_t>(ParseStrictFloatText(text));
        AppendRawScalarBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsBool) {
        const auto value = ParseStrictBoolText(text);
        AppendRawScalarBytes(data, &value, sizeof(value));
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

static void AppendComplexStructFromText(vector<uint8_t>& data, ptr<const Property> prop, const BaseTypeDesc& base_type, string_view text, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    FO_STACK_TRACE_ENTRY();

    const auto decoded = StringEscaping::DecodeString(text);
    auto s = make_nptr(decoded.c_str());
    string_view token;

    for (const auto& field : base_type.StructLayout->Fields) {
        s = ReadTextTokenView(s, token);

        if (!s) {
            throw PropertySerializationException("Wrong struct size (from text)");
        }

        AppendBaseTypeFromText(data, prop, field.Type, token, hash_resolver, name_resolver);
    }

    if (ReadTextTokenView(s, token)) {
        throw PropertySerializationException("Wrong struct size (from text)");
    }
}

static auto ResolveEnumValueWithMigration(const BaseTypeDesc& base_type, HashResolver& hash_resolver, NameResolver& name_resolver, string_view value_name) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    bool failed = false;
    const int32_t enum_value = name_resolver.ResolveEnumValue(base_type.Name, value_name, &failed);

    if (!failed) {
        return enum_value;
    }

    // Removed or renamed enum value in persisted data: migrate it via ///@ MigrationRule Enum <EnumName> <Old> <New>.
    if (const auto migrated = name_resolver.CheckMigrationRule(hash_resolver.ToHashedString("Enum"), hash_resolver.ToHashedString(base_type.Name), hash_resolver.ToHashedString(value_name)); migrated.has_value()) {
        return name_resolver.ResolveEnumValue(base_type.Name, migrated.value().as_str());
    }

    // No migration rule: keep the original throwing behavior for genuinely unknown values.
    return name_resolver.ResolveEnumValue(base_type.Name, value_name);
}

static void AppendBaseTypeFromText(vector<uint8_t>& data, ptr<const Property> prop, const BaseTypeDesc& base_type, string_view text, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    FO_STACK_TRACE_ENTRY();

    if (base_type.IsString) {
        string decoded_storage;
        AppendRawString(data, DecodeTextIfNeeded(text, decoded_storage));
    }
    else if (base_type.IsHashedString) {
        string decoded_storage;
        const auto resolved_value = hash_resolver.ToHashedString(DecodeTextIfNeeded(text, decoded_storage));
        const auto hash = resolved_value.as_hash();
        AppendRawScalarBytes(data, &hash, sizeof(hash));
    }
    else if (base_type.IsFixedType || base_type.IsEntityProto) {
        string decoded_storage;
        auto resolved_value = hash_resolver.ToHashedString(DecodeTextIfNeeded(text, decoded_storage));
        auto proto = name_resolver.GetProtoEntity(base_type.HashedName, resolved_value);

        if (proto) {
            resolved_value = proto->GetProtoId();
        }

        if (!resolved_value) {
            if (!prop->IsNullable()) {
                throw PropertySerializationException("Proto reference property requires non-null proto, add Nullable or explicit Proto MigrationRule to valid target", prop->GetName(), base_type.Name);
            }
        }
        else if (!proto) {
            throw PropertySerializationException("Proto reference does not exist, add explicit Proto MigrationRule for deletion", base_type.Name, resolved_value);
        }

        const auto hash = resolved_value.as_hash();
        AppendRawScalarBytes(data, &hash, sizeof(hash));
    }
    else if (base_type.IsEnum) {
        string decoded_storage;
        const auto decoded = DecodeTextIfNeeded(text, decoded_storage);
        int64_t parsed_enum_value = 0;
        int32_t enum_value = 0;
        bool is_numeric_value = false;

        try {
            parsed_enum_value = ParseStrictIntText(decoded);
            is_numeric_value = true;
        }
        catch (const PropertySerializationException&) {
            is_numeric_value = false;
        }

        if (is_numeric_value) {
            enum_value = numeric_cast<int32_t>(parsed_enum_value);
            (void)name_resolver.ResolveEnumValueName(base_type.Name, enum_value);
        }
        else {
            enum_value = ResolveEnumValueWithMigration(base_type, hash_resolver, name_resolver, decoded);
        }

        if (base_type.Size == sizeof(uint8_t)) {
            const auto value = numeric_cast<uint8_t>(enum_value);
            AppendRawScalarBytes(data, &value, sizeof(value));
        }
        else if (base_type.Size == sizeof(uint16_t)) {
            const auto value = numeric_cast<uint16_t>(enum_value);
            AppendRawScalarBytes(data, &value, sizeof(value));
        }
        else {
            AppendRawScalarBytes(data, &enum_value, base_type.Size);
        }
    }
    else if (base_type.IsPrimitive || base_type.IsSimpleStruct) {
        const auto& primitive_type = base_type.IsSimpleStruct ? base_type.StructLayout->Fields.front().Type : base_type;
        AppendPrimitiveFromText(data, primitive_type, text);
    }
    else if (base_type.IsComplexStruct) {
        AppendComplexStructFromText(data, prop, base_type, text, hash_resolver, name_resolver);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

static auto ParseArrayFromText(ptr<const Property> prop, const BaseTypeDesc& base_type, bool is_array_of_string, string_view text, bool encoded_text, HashResolver& hash_resolver, NameResolver& name_resolver) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    const auto decoded = encoded_text ? StringEscaping::DecodeString(text) : string(text);
    auto s = make_nptr(decoded.c_str());
    string_view token;
    vector<uint8_t> data;
    data.reserve(std::max<size_t>(decoded.length() + sizeof(uint32_t), 32));
    uint32_t arr_size = 0;

    if (is_array_of_string) {
        data.resize(sizeof(uint32_t));
    }

    while ((s = ReadTextTokenView(s, token))) {
        AppendBaseTypeFromText(data, prop, base_type, token, hash_resolver, name_resolver);
        arr_size++;
    }

    if (is_array_of_string) {
        auto data_ptr = RawVectorBytesPtr(data);
        size_t data_pos = 0;
        WriteRawUInt32(data_ptr, data_pos, arr_size);
        FO_VERIFY_AND_THROW(data_pos == sizeof(uint32_t), "String array size prefix did not occupy exactly one uint32");
    }

    return data;
}

static void AppendPrimitiveToCodedString(string& result, const BaseTypeDesc& primitive_type, RawReadCursor& cursor)
{
    FO_STACK_TRACE_ENTRY();

    if (primitive_type.IsInt8) {
        result += strex("{}", ReadCursorValue<int8_t>(cursor));
    }
    else if (primitive_type.IsInt16) {
        result += strex("{}", ReadCursorValue<int16_t>(cursor));
    }
    else if (primitive_type.IsInt32) {
        result += strex("{}", ReadCursorValue<int32_t>(cursor));
    }
    else if (primitive_type.IsInt64) {
        result += strex("{}", ReadCursorValue<int64_t>(cursor));
    }
    else if (primitive_type.IsUInt8) {
        result += strex("{}", ReadCursorValue<uint8_t>(cursor));
    }
    else if (primitive_type.IsUInt16) {
        result += strex("{}", ReadCursorValue<uint16_t>(cursor));
    }
    else if (primitive_type.IsUInt32) {
        result += strex("{}", ReadCursorValue<uint32_t>(cursor));
    }
    else if (primitive_type.IsSingleFloat) {
        result += strex("{:f}", ReadFiniteCursorFloat<float32_t>(cursor, primitive_type.Name)).rtrim("0").rtrim(".");
    }
    else if (primitive_type.IsDoubleFloat) {
        result += strex("{:f}", ReadFiniteCursorFloat<float64_t>(cursor, primitive_type.Name)).rtrim("0").rtrim(".");
    }
    else if (primitive_type.IsBool) {
        result += ReadCursorValue<bool>(cursor) ? "True" : "False";
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

static void AppendBaseTypeToCodedString(string& result, const BaseTypeDesc& base_type, HashResolver& hash_resolver, NameResolver& name_resolver, RawReadCursor& cursor)
{
    FO_STACK_TRACE_ENTRY();

    if (base_type.IsString) {
        const uint32_t str_len = ReadCursorValue<uint32_t>(cursor);
        StringEscaping::AppendCodeString(result, span_to_string(ReadCursorBytes(cursor, str_len)));
    }
    else if (base_type.IsHashedString) {
        const auto hash = ReadCursorValue<hstring::hash_t>(cursor);
        StringEscaping::AppendCodeString(result, hash_resolver.ResolveHash(hash).as_str());
    }
    else if (base_type.IsFixedType || base_type.IsEntityProto) {
        const auto hash = ReadCursorValue<hstring::hash_t>(cursor);
        StringEscaping::AppendCodeString(result, hash_resolver.ResolveHash(hash).as_str());
    }
    else if (base_type.IsEnum) {
        const int32_t enum_value = ReadCursorEnumValue(cursor, base_type.Size);
        StringEscaping::AppendCodeString(result, name_resolver.ResolveEnumValueName(base_type.Name, enum_value));
    }
    else if (base_type.IsPrimitive || base_type.IsSimpleStruct) {
        const auto& primitive_type = base_type.IsSimpleStruct ? base_type.StructLayout->Fields.front().Type : base_type;
        AppendPrimitiveToCodedString(result, primitive_type, cursor);
    }
    else if (base_type.IsComplexStruct) {
        string struct_str;
        struct_str.reserve(128);
        bool next_iteration = false;

        for (const auto& field : base_type.StructLayout->Fields) {
            if (next_iteration) {
                struct_str.append(" ");
            }
            else {
                next_iteration = true;
            }

            AppendBaseTypeToCodedString(struct_str, field.Type, hash_resolver, name_resolver, cursor);
        }

        StringEscaping::AppendCodeString(result, struct_str);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

static void AppendBaseTypeToCodedStringAt(string& result, const BaseTypeDesc& base_type, HashResolver& hash_resolver, NameResolver& name_resolver, span<const uint8_t> raw_data, size_t& data_pos)
{
    FO_STACK_TRACE_ENTRY();

    RawReadCursor cursor {raw_data, data_pos};
    AppendBaseTypeToCodedString(result, base_type, hash_resolver, name_resolver, cursor);
    data_pos = cursor.Pos;
}

static auto RawDataToValue(const BaseTypeDesc& base_type, HashResolver& hash_resolver, NameResolver& name_resolver, RawReadCursor& cursor) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    if (base_type.IsString) {
        const uint32_t str_len = ReadCursorValue<uint32_t>(cursor);
        return string {span_to_string(ReadCursorBytes(cursor, str_len))};
    }
    else if (base_type.IsHashedString || base_type.IsFixedType || base_type.IsEntityProto) {
        const auto hash = ReadCursorValue<hstring::hash_t>(cursor);
        return string(hash_resolver.ResolveHash(hash).as_str());
    }
    else if (base_type.IsEnum) {
        const int32_t enum_value = ReadCursorEnumValue(cursor, base_type.Size);
        return string(name_resolver.ResolveEnumValueName(base_type.Name, enum_value));
    }
    else if (base_type.IsPrimitive || base_type.IsSimpleStruct) {
        const auto& primitive_type = base_type.IsSimpleStruct ? base_type.StructLayout->Fields.front().Type : base_type;

        if (primitive_type.IsInt8) {
            return numeric_cast<int64_t>(ReadCursorValue<int8_t>(cursor));
        }
        else if (primitive_type.IsInt16) {
            return numeric_cast<int64_t>(ReadCursorValue<int16_t>(cursor));
        }
        else if (primitive_type.IsInt32) {
            return numeric_cast<int64_t>(ReadCursorValue<int32_t>(cursor));
        }
        else if (primitive_type.IsInt64) {
            return numeric_cast<int64_t>(ReadCursorValue<int64_t>(cursor));
        }
        else if (primitive_type.IsUInt8) {
            return numeric_cast<int64_t>(ReadCursorValue<uint8_t>(cursor));
        }
        else if (primitive_type.IsUInt16) {
            return numeric_cast<int64_t>(ReadCursorValue<uint16_t>(cursor));
        }
        else if (primitive_type.IsUInt32) {
            return numeric_cast<int64_t>(ReadCursorValue<uint32_t>(cursor));
        }
        else if (primitive_type.IsSingleFloat) {
            return numeric_cast<float64_t>(ReadFiniteCursorFloat<float32_t>(cursor, primitive_type.Name));
        }
        else if (primitive_type.IsDoubleFloat) {
            return numeric_cast<float64_t>(ReadFiniteCursorFloat<float64_t>(cursor, primitive_type.Name));
        }
        else if (primitive_type.IsBool) {
            return ReadCursorValue<bool>(cursor);
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (base_type.IsComplexStruct) {
        auto struct_layout = base_type.StructLayout;
        FO_VERIFY_AND_THROW(struct_layout, "Struct layout is null");
        AnyData::Array struct_value;
        struct_value.Reserve(struct_layout->Fields.size());

        for (const auto& field : struct_layout->Fields) {
            auto field_value = RawDataToValue(field.Type, hash_resolver, name_resolver, cursor);
            struct_value.EmplaceBack(std::move(field_value));
        }

        return std::move(struct_value);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

static auto RawDataToValueAt(const BaseTypeDesc& base_type, HashResolver& hash_resolver, NameResolver& name_resolver, span<const uint8_t> raw_data, size_t& data_pos) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    RawReadCursor cursor {raw_data, data_pos};
    auto value = RawDataToValue(base_type, hash_resolver, name_resolver, cursor);
    data_pos = cursor.Pos;
    return value;
}

static auto IsDefaultPropertyRawData(ptr<const Property> prop, span<const uint8_t> raw_data) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (raw_data.empty()) {
        return true;
    }
    if (!prop->IsPlainData()) {
        return false;
    }

    for (const auto byte : raw_data) {
        if (byte != 0) {
            return false;
        }
    }

    return true;
}

static auto GetRefTypeFieldsRegistrator(const BaseTypeDesc& base_type) -> ptr<const PropertyRegistrator>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsRefType, "Base type is not a reference type");
    FO_VERIFY_AND_THROW(base_type.RefType != nullptr, "Reference type descriptor is null");
    FO_VERIFY_AND_THROW(base_type.RefType->FieldsRegistrator != nullptr, "Reference type has no fields registrator");
    return base_type.RefType->FieldsRegistrator;
}

static void ForEachRefTypeFieldRawData(string_view owner_name, const BaseTypeDesc& base_type, span<const uint8_t> raw_data, const function<void(ptr<const Property>, const_span<uint8_t>)>& callback)
{
    FO_STACK_TRACE_ENTRY();

    auto fields_registrator = GetRefTypeFieldsRegistrator(base_type);
    size_t data_pos = 0;

    for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
        auto field_prop = fields_registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
        span<const uint8_t> field_raw_data {};

        if (data_pos < raw_data.size()) {
            data_pos = align_up(data_pos, sizeof(uint32_t));

            if (data_pos > raw_data.size() || raw_data.size() - data_pos < sizeof(uint32_t)) {
                throw PropertySerializationException("Corrupted ref type property data", owner_name, field_prop->GetName());
            }

            const auto field_size = ReadRawValue<uint32_t>(TakeRawBytes(raw_data, data_pos, sizeof(uint32_t)));

            if (field_prop->IsPlainData() && field_size != 0 && field_size != field_prop->GetBaseSize()) {
                throw PropertySerializationException("Wrong ref field raw size", owner_name, field_prop->GetName());
            }

            if (field_size != 0) {
                data_pos = align_up(data_pos, field_prop->GetDataAlignment());
            }

            if (data_pos > raw_data.size() || raw_data.size() - data_pos < field_size) {
                throw PropertySerializationException("Corrupted ref type property data", owner_name, field_prop->GetName());
            }

            field_raw_data = TakeRawBytes(raw_data, data_pos, field_size);
        }

        callback(field_prop, field_raw_data);
    }

    if (data_pos != raw_data.size()) {
        throw PropertySerializationException("Corrupted ref type property data", owner_name);
    }
}

static auto BuildRefTypePropertyData(const BaseTypeDesc& base_type, const Properties& field_props) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    auto fields_registrator = GetRefTypeFieldsRegistrator(base_type);
    vector<span<const uint8_t>> field_raw_entries(fields_registrator->GetPropertiesCount());
    vector<bool> field_is_default(fields_registrator->GetPropertiesCount(), true);
    size_t last_non_default_field = 0;

    for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
        auto field_prop = fields_registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
        const auto field_raw_data = field_props.GetRawData(field_prop);
        const auto is_default = IsDefaultPropertyRawData(field_prop, field_raw_data);

        field_raw_entries[i] = field_raw_data;
        field_is_default[i] = is_default;

        if (!is_default) {
            last_non_default_field = i;
        }
    }

    if (last_non_default_field == 0) {
        return {};
    }

    size_t data_size = 0;

    for (size_t i = 1; i <= last_non_default_field; i++) {
        data_size = align_up(data_size, sizeof(uint32_t));
        data_size += sizeof(uint32_t);

        if (!field_is_default[i]) {
            data_size = align_up(data_size, fields_registrator->GetPropertyByIndexUnsafe(i)->GetDataAlignment());
            data_size += field_raw_entries[i].size();
        }
    }

    vector<uint8_t> data(data_size);
    size_t data_pos = 0;
    auto data_ptr = RawVectorBytesPtr(data);

    for (size_t i = 1; i <= last_non_default_field; i++) {
        const auto field_size = !field_is_default[i] ? numeric_cast<uint32_t>(field_raw_entries[i].size()) : 0U;
        WriteRawUInt32(data_ptr, data_pos, field_size);

        if (field_size != 0) {
            data_pos = align_up(data_pos, fields_registrator->GetPropertyByIndexUnsafe(i)->GetDataAlignment());
            WriteRawSpan(data_ptr, data_pos, field_raw_entries[i]);
        }
    }

    FO_VERIFY_AND_THROW(data_pos == data.size(), "Serialized ref-type field buffer size does not match bytes written", fields_registrator->GetTypeName(), data_pos, data.size());
    return data;
}

static auto SaveRefTypeToValue(string_view owner_name, const BaseTypeDesc& base_type, span<const uint8_t> raw_data, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    AnyData::Dict dict;

    ForEachRefTypeFieldRawData(owner_name, base_type, raw_data, [&dict, &hash_resolver, &name_resolver](ptr<const Property> field_prop, span<const uint8_t> field_raw_data) {
        if (field_raw_data.empty()) {
            return;
        }

        auto field_value = PropertiesSerializator::SavePropertyToValue(field_prop, field_raw_data, hash_resolver, name_resolver);
        dict.Emplace(string {field_prop->GetNameWithoutComponent()}, std::move(field_value));
    });

    return std::move(dict);
}

static auto SaveRefTypeToText(string_view owner_name, const BaseTypeDesc& base_type, span<const uint8_t> raw_data, HashResolver& hash_resolver, NameResolver& name_resolver) -> string
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(owner_name);

    string ref_str;
    bool next_iteration = false;

    ForEachRefTypeFieldRawData(owner_name, base_type, raw_data, [&ref_str, &next_iteration, &hash_resolver, &name_resolver](ptr<const Property> field_prop, span<const uint8_t> field_raw_data) {
        if (field_raw_data.empty()) {
            return;
        }

        if (next_iteration) {
            ref_str.append(" ");
        }
        else {
            next_iteration = true;
        }

        ref_str.append(field_prop->GetNameWithoutComponent());
        ref_str.append(" ");

        const auto field_text = PropertiesSerializator::SavePropertyToText(field_prop, field_raw_data, hash_resolver, name_resolver);
        StringEscaping::AppendCodeString(ref_str, field_text);
    });

    return ref_str;
}

static auto LoadRefTypeFromValue(string_view owner_name, const BaseTypeDesc& base_type, const AnyData::Value& value, HashResolver& hash_resolver, NameResolver& name_resolver) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    if (value.Type() != AnyData::ValueType::Dict) {
        throw PropertySerializationException("Wrong ref type value type", owner_name, value.Type());
    }

    auto fields_registrator = GetRefTypeFieldsRegistrator(base_type);
    const auto& dict = value.AsDict();
    Properties field_props(fields_registrator);

    for (auto&& [field_name, field_value] : dict) {
        auto field_prop = fields_registrator->FindProperty(field_name);

        if (!field_prop) {
            throw PropertySerializationException("Unknown ref type field", owner_name, field_name);
        }

        PropertiesSerializator::LoadPropertyFromValue(&field_props, field_prop, field_value, hash_resolver, name_resolver);
    }

    return BuildRefTypePropertyData(base_type, field_props);
}

static auto LoadRefTypeFromText(string_view owner_name, const BaseTypeDesc& base_type, string_view text, HashResolver& hash_resolver, NameResolver& name_resolver) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    if (text.empty()) {
        return {};
    }

    auto fields_registrator = GetRefTypeFieldsRegistrator(base_type);
    const auto fields_value = AnyData::ParseValue(string {text}, false, true, AnyData::ValueType::String);
    const auto& fields_arr = fields_value.AsArray();

    if (fields_arr.Size() % 2 != 0) {
        throw PropertySerializationException("Wrong ref type text field count", owner_name, text);
    }

    Properties field_props(fields_registrator);
    unordered_set<string> seen_fields;

    for (size_t i = 0; i < fields_arr.Size(); i += 2) {
        const string_view field_name = fields_arr[i].AsString();
        auto field_prop = fields_registrator->FindProperty(field_name);

        if (!field_prop) {
            throw PropertySerializationException("Unknown ref type field", owner_name, field_name);
        }
        if (!seen_fields.emplace(field_name).second) {
            throw PropertySerializationException("Duplicate ref type field", owner_name, field_name);
        }

        PropertiesSerializator::LoadPropertyFromText(&field_props, field_prop, fields_arr[i + 1].AsString(), hash_resolver, name_resolver);
    }

    return BuildRefTypePropertyData(base_type, field_props);
}

auto PropertiesSerializator::SavePropertyToValue(ptr<const Property> prop, span<const uint8_t> raw_data, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(!prop->IsVirtual(), "Property is virtual");

    auto base_type = make_ptr(&prop->GetBaseType());
    size_t data_pos = 0;
    const auto read_array_size = [&]() -> uint32_t { return ReadPropertyRawUInt32(prop, "Corrupted array property data", raw_data, data_pos); };
    const auto read_dict_size = [&]() -> uint32_t { return ReadPropertyRawUInt32(prop, "Corrupted dict property data", raw_data, data_pos); };
    const auto take_array_data = [&](size_t size) -> span<const uint8_t> { return TakePropertyRawBytes(prop, "Corrupted array property data", raw_data, data_pos, size); };
    const auto take_dict_data = [&](size_t size) -> span<const uint8_t> { return TakePropertyRawBytes(prop, "Corrupted dict property data", raw_data, data_pos, size); };
    const auto align_array_pos = [&](size_t alignment) { AlignPropertyRawPos(prop, "Corrupted array property data", raw_data, data_pos, alignment); };
    const auto align_dict_pos = [&](size_t alignment) { AlignPropertyRawPos(prop, "Corrupted dict property data", raw_data, data_pos, alignment); };

    if (prop->IsPlainData()) {
        auto value = RawDataToValueAt(*base_type, hash_resolver, name_resolver, raw_data, data_pos);

        FO_VERIFY_AND_THROW(data_pos == raw_data.size(), "Plain property deserialization did not consume the full raw payload", prop->GetName(), data_pos, raw_data.size());
        return value;
    }
    else if (base_type->IsRefType && !prop->IsArray() && !prop->IsDict()) {
        return SaveRefTypeToValue(prop->GetName(), *base_type, raw_data, hash_resolver, name_resolver);
    }
    else if (prop->IsString()) {
        auto value = string {span_to_string(raw_data)};
        data_pos = raw_data.size();

        FO_VERIFY_AND_THROW(data_pos == raw_data.size(), "String property deserialization did not consume the full raw payload", prop->GetName(), data_pos, raw_data.size());
        return value;
    }
    else if (prop->IsArray()) {
        AnyData::Array arr;

        if (!raw_data.empty()) {
            uint32_t arr_size;

            if (prop->IsArrayOfString() || base_type->IsRefType) {
                arr_size = read_array_size();
            }
            else {
                arr_size = numeric_cast<uint32_t>(raw_data.size() / base_type->Size);
            }

            arr.Reserve(arr_size);

            for (uint32_t i = 0; i < arr_size; i++) {
                if (base_type->IsRefType) {
                    const auto ref_data_size = read_array_size();

                    if (ref_data_size != 0) {
                        align_array_pos(MAX_SERIALIZED_ALIGNMENT);
                    }

                    const auto ref_data = take_array_data(ref_data_size);
                    auto arr_entry = SaveRefTypeToValue(prop->GetName(), *base_type, ref_data, hash_resolver, name_resolver);
                    arr.EmplaceBack(std::move(arr_entry));
                }
                else {
                    auto arr_entry = RawDataToValueAt(*base_type, hash_resolver, name_resolver, raw_data, data_pos);
                    arr.EmplaceBack(std::move(arr_entry));
                }
            }
        }

        FO_VERIFY_AND_THROW(data_pos == raw_data.size(), "Array property deserialization did not consume the full raw payload", prop->GetName(), data_pos, raw_data.size());
        return std::move(arr);
    }
    else if (prop->IsDict()) {
        AnyData::Dict dict;

        if (!raw_data.empty()) {
            auto dict_key_type = make_ptr(&prop->GetDictKeyType());

            if (dict_key_type->IsSimpleStruct) {
                dict_key_type = &dict_key_type->StructLayout->Fields.front().Type;
            }

            const auto get_key_string = [&dict_key_type, &hash_resolver, &name_resolver](span<const uint8_t> key_data) -> string {
                if (dict_key_type->IsString) {
                    const uint32_t str_len = ReadRawValue<uint32_t>(key_data.first(sizeof(uint32_t)));
                    return string {span_to_string(key_data.subspan(sizeof(uint32_t), str_len))};
                }
                else if (dict_key_type->IsHashedString) {
                    const auto hash = ReadRawValue<hstring::hash_t>(key_data.first(sizeof(hstring::hash_t)));
                    return string(hash_resolver.ResolveHash(hash).as_str());
                }
                else if (dict_key_type->IsEnum) {
                    int32_t enum_value = 0;

                    if (dict_key_type->Size == sizeof(uint8_t)) {
                        enum_value = numeric_cast<int32_t>(ReadRawValue<uint8_t>(key_data.first(sizeof(uint8_t))));
                    }
                    else if (dict_key_type->Size == sizeof(uint16_t)) {
                        enum_value = numeric_cast<int32_t>(ReadRawValue<uint16_t>(key_data.first(sizeof(uint16_t))));
                    }
                    else if (dict_key_type->Size == sizeof(int32_t)) {
                        enum_value = ReadRawValue<int32_t>(key_data.first(sizeof(int32_t)));
                    }
                    else {
                        FO_UNREACHABLE_PLACE();
                    }

                    return string(name_resolver.ResolveEnumValueName(dict_key_type->Name, enum_value));
                }
                else if (dict_key_type->IsInt8) {
                    return strex("{}", ReadRawValue<int8_t>(key_data.first(sizeof(int8_t))));
                }
                else if (dict_key_type->IsInt16) {
                    return strex("{}", ReadRawValue<int16_t>(key_data.first(sizeof(int16_t))));
                }
                else if (dict_key_type->IsInt32) {
                    return strex("{}", ReadRawValue<int32_t>(key_data.first(sizeof(int32_t))));
                }
                else if (dict_key_type->IsInt64) {
                    return strex("{}", ReadRawValue<int64_t>(key_data.first(sizeof(int64_t))));
                }
                else if (dict_key_type->IsUInt8) {
                    return strex("{}", ReadRawValue<uint8_t>(key_data.first(sizeof(uint8_t))));
                }
                else if (dict_key_type->IsUInt16) {
                    return strex("{}", ReadRawValue<uint16_t>(key_data.first(sizeof(uint16_t))));
                }
                else if (dict_key_type->IsUInt32) {
                    return strex("{}", ReadRawValue<uint32_t>(key_data.first(sizeof(uint32_t))));
                }
                else if (dict_key_type->IsSingleFloat) {
                    return strex("{}", ReadFiniteRawFloat<float32_t>(key_data.first(sizeof(float32_t)), dict_key_type->Name));
                }
                else if (dict_key_type->IsDoubleFloat) {
                    return strex("{}", ReadFiniteRawFloat<float64_t>(key_data.first(sizeof(float64_t)), dict_key_type->Name));
                }
                else if (dict_key_type->IsBool) {
                    return ReadRawValue<bool>(key_data.first(sizeof(bool))) ? "True" : "False";
                }
                else {
                    FO_UNREACHABLE_PLACE();
                }
            };

            const auto get_key_len = [&dict_key_type, &prop](span<const uint8_t> key_data) -> size_t {
                if (dict_key_type->IsString) {
                    if (key_data.size() < sizeof(uint32_t)) {
                        throw PropertySerializationException("Corrupted dict property data", prop->GetName());
                    }

                    const uint32_t str_len = ReadRawValue<uint32_t>(key_data.first(sizeof(uint32_t)));
                    return sizeof(uint32_t) + str_len;
                }
                else {
                    return dict_key_type->Size;
                }
            };

            const size_t key_alignment = dict_key_type->IsString ? sizeof(uint32_t) : alignment_for_size(dict_key_type->Size);

            if (prop->IsDictOfArray()) {
                while (data_pos < raw_data.size()) {
                    align_dict_pos(key_alignment);
                    const auto key_len = get_key_len(raw_data.subspan(data_pos));
                    const auto key_data = take_dict_data(key_len);
                    string key_str = get_key_string(key_data);
                    const auto arr_size = read_dict_size();

                    AnyData::Array arr;
                    arr.Reserve(arr_size);

                    if (arr_size != 0 && !base_type->IsRefType && !base_type->IsString) {
                        align_dict_pos(alignment_for_size(base_type->Size));
                    }

                    for (uint32_t i = 0; i < arr_size; i++) {
                        if (base_type->IsRefType) {
                            const auto ref_data_size = read_dict_size();

                            if (ref_data_size != 0) {
                                align_dict_pos(MAX_SERIALIZED_ALIGNMENT);
                            }

                            const auto ref_data = take_dict_data(ref_data_size);
                            auto arr_entry = SaveRefTypeToValue(prop->GetName(), *base_type, ref_data, hash_resolver, name_resolver);
                            arr.EmplaceBack(std::move(arr_entry));
                        }
                        else {
                            auto arr_entry = RawDataToValueAt(*base_type, hash_resolver, name_resolver, raw_data, data_pos);
                            arr.EmplaceBack(std::move(arr_entry));
                        }
                    }

                    dict.Emplace(std::move(key_str), std::move(arr));
                }
            }
            else {
                while (data_pos < raw_data.size()) {
                    align_dict_pos(key_alignment);
                    const auto key_len = get_key_len(raw_data.subspan(data_pos));
                    const auto key_data = take_dict_data(key_len);
                    string key_str = get_key_string(key_data);

                    if (base_type->IsRefType) {
                        const auto ref_data_size = read_dict_size();

                        if (ref_data_size != 0) {
                            align_dict_pos(MAX_SERIALIZED_ALIGNMENT);
                        }

                        const auto ref_data = take_dict_data(ref_data_size);
                        auto dict_value = SaveRefTypeToValue(prop->GetName(), *base_type, ref_data, hash_resolver, name_resolver);
                        dict.Emplace(std::move(key_str), std::move(dict_value));
                    }
                    else {
                        if (!base_type->IsString) {
                            align_dict_pos(alignment_for_size(base_type->Size));
                        }

                        auto dict_value = RawDataToValueAt(*base_type, hash_resolver, name_resolver, raw_data, data_pos);
                        dict.Emplace(std::move(key_str), std::move(dict_value));
                    }
                }
            }
        }

        FO_VERIFY_AND_THROW(data_pos == raw_data.size(), "Dict property deserialization did not consume the full raw payload", prop->GetName(), data_pos, raw_data.size());
        return std::move(dict);
    }

    FO_UNREACHABLE_PLACE();
}

void PropertiesSerializator::LoadPropertyFromValue(ptr<Properties> props, ptr<const Property> prop, const AnyData::Value& value, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(!prop->IsVirtual(), "Property is virtual");

    auto set_data = [props, prop](span<const uint8_t> raw_data) mutable { props->SetRawData(prop, raw_data); };

    return LoadPropertyFromValue(prop, value, set_data, hash_resolver, name_resolver);
}

static auto ConvertToString(const AnyData::Value& value, string& buf) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    switch (value.Type()) {
    case AnyData::ValueType::String:
        return value.AsString();
    case AnyData::ValueType::Int64:
        return buf = strex("{}", value.AsInt64());
    case AnyData::ValueType::Float64:
        if (!std::isfinite(value.AsDouble())) {
            throw PropertySerializationException("Numeric value is not finite", value.AsDouble());
        }
        return buf = strex("{}", value.AsDouble());
    case AnyData::ValueType::Bool:
        return buf = strex("{}", value.AsBool());
    default:
        throw PropertySerializationException("Unable to convert not string, int32, float32 or bool value to string", value.Type());
    }
};

template<typename T>
static auto ConvertFloat64ToNumber(float64_t value) -> T
{
    FO_STACK_TRACE_ENTRY();

    if (!std::isfinite(value)) {
        throw PropertySerializationException("Numeric value is not finite", value, typeid(T).name());
    }

    if constexpr (std::same_as<T, bool>) {
        return !is_float_equal(value, 0.0);
    }
    else if constexpr (std::integral<T>) {
        const float64_t rounded_value = std::round(value);
        const float64_t min_value = static_cast<float64_t>(std::numeric_limits<T>::min());
        const float64_t max_value = GetIntegralMaxFloat64<T>();

        if (rounded_value < min_value || rounded_value > max_value) {
            throw PropertySerializationException("Numeric value out of range", value, typeid(T).name());
        }

        return numeric_cast<T>(std::llround(value));
    }
    else if constexpr (std::same_as<T, float32_t>) {
        constexpr auto min_value = static_cast<float64_t>(std::numeric_limits<float32_t>::lowest());
        constexpr auto max_value = static_cast<float64_t>(std::numeric_limits<float32_t>::max());

        if (value < min_value || value > max_value) {
            throw PropertySerializationException("Numeric value out of range", value, typeid(T).name());
        }

        return numeric_cast<float32_t>(value);
    }
    else {
        return numeric_cast<T>(value);
    }
}

template<typename T>
static void ConvertToNumber(const AnyData::Value& value, T& result_value)
{
    FO_STACK_TRACE_ENTRY();

    if (value.Type() == AnyData::ValueType::Int64) {
        if constexpr (std::same_as<T, bool>) {
            result_value = value.AsInt64() != 0;
        }
        else {
            result_value = numeric_cast<T>(value.AsInt64());
        }
    }
    else if (value.Type() == AnyData::ValueType::Float64) {
        result_value = ConvertFloat64ToNumber<T>(value.AsDouble());
    }
    else if (value.Type() == AnyData::ValueType::Bool) {
        if constexpr (std::same_as<T, bool>) {
            result_value = value.AsBool();
        }
        else if constexpr (std::floating_point<T>) {
            result_value = value.AsBool() ? 1.0f : 0.0f;
        }
        else {
            result_value = value.AsBool() ? 1 : 0;
        }
    }
    else if (value.Type() == AnyData::ValueType::String) {
        const string_view str = value.AsString();
        const auto str_value = strvex(str);

        if (str_value.is_explicit_bool()) {
            if constexpr (std::same_as<T, bool>) {
                result_value = str_value.to_bool();
            }
            else if constexpr (std::floating_point<T>) {
                result_value = str_value.to_bool() ? 1.0f : 0.0f;
            }
            else {
                result_value = str_value.to_bool() ? 1 : 0;
            }
        }
        else {
            if constexpr (std::same_as<T, bool>) {
                result_value = !is_float_equal(ParseStrictFloatText(str), 0.0);
            }
            else if constexpr (std::floating_point<T>) {
                result_value = ConvertFloat64ToNumber<T>(ParseStrictFloatText(str));
            }
            else {
                result_value = numeric_cast<T>(ParseStrictIntText(str));
            }
        }
    }
    else {
        throw PropertySerializationException("Wrong value type (not string, int, float or bool)", value.Type());
    }
}

auto PropertiesSerializator::SavePropertyToText(ptr<const Properties> props, ptr<const Property> prop, HashResolver& hash_resolver, NameResolver& name_resolver) -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(!prop->IsVirtual(), "Property is virtual");

    props->ValidateForRawData(prop);

    return SavePropertyToText(prop, props->GetRawData(prop), hash_resolver, name_resolver);
}

auto PropertiesSerializator::SavePropertyToText(ptr<const Property> prop, span<const uint8_t> raw_data, HashResolver& hash_resolver, NameResolver& name_resolver) -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(!prop->IsVirtual(), "Property is virtual");

    auto base_type = make_ptr(&prop->GetBaseType());
    size_t data_pos = 0;
    const auto read_array_size = [&]() -> uint32_t { return ReadPropertyRawUInt32(prop, "Corrupted array property data", raw_data, data_pos); };
    const auto read_dict_size = [&]() -> uint32_t { return ReadPropertyRawUInt32(prop, "Corrupted dict property data", raw_data, data_pos); };
    const auto take_array_data = [&](size_t size) -> span<const uint8_t> { return TakePropertyRawBytes(prop, "Corrupted array property data", raw_data, data_pos, size); };
    const auto take_dict_data = [&](size_t size) -> span<const uint8_t> { return TakePropertyRawBytes(prop, "Corrupted dict property data", raw_data, data_pos, size); };
    const auto align_array_pos = [&](size_t alignment) { AlignPropertyRawPos(prop, "Corrupted array property data", raw_data, data_pos, alignment); };
    const auto align_dict_pos = [&](size_t alignment) { AlignPropertyRawPos(prop, "Corrupted dict property data", raw_data, data_pos, alignment); };
    string result;
    result.reserve(std::max<size_t>(raw_data.size() * 2, 64));

    if (prop->IsString()) {
        StringEscaping::AppendCodeString(result, span_to_string(raw_data));
        data_pos = raw_data.size();
    }
    else if (prop->IsPlainData()) {
        AppendBaseTypeToCodedStringAt(result, *base_type, hash_resolver, name_resolver, raw_data, data_pos);
    }
    else if (base_type->IsRefType && !prop->IsArray() && !prop->IsDict()) {
        StringEscaping::AppendCodeString(result, SaveRefTypeToText(prop->GetName(), *base_type, raw_data, hash_resolver, name_resolver));
        data_pos = raw_data.size();
    }
    else if (prop->IsArray()) {
        string arr_str;
        arr_str.reserve(std::max<size_t>(raw_data.size() * 2, 64));
        bool next_iteration = false;

        if (!raw_data.empty()) {
            uint32_t arr_size;

            if (prop->IsArrayOfString() || base_type->IsRefType) {
                arr_size = read_array_size();
            }
            else {
                arr_size = numeric_cast<uint32_t>(raw_data.size() / base_type->Size);
            }

            for (uint32_t i = 0; i < arr_size; i++) {
                if (next_iteration) {
                    arr_str.append(" ");
                }
                else {
                    next_iteration = true;
                }

                if (base_type->IsRefType) {
                    const auto ref_data_size = read_array_size();

                    if (ref_data_size != 0) {
                        align_array_pos(MAX_SERIALIZED_ALIGNMENT);
                    }

                    const auto ref_data = take_array_data(ref_data_size);
                    StringEscaping::AppendCodeString(arr_str, SaveRefTypeToText(prop->GetName(), *base_type, ref_data, hash_resolver, name_resolver));
                }
                else {
                    AppendBaseTypeToCodedStringAt(arr_str, *base_type, hash_resolver, name_resolver, raw_data, data_pos);
                }
            }
        }

        StringEscaping::AppendCodeString(result, arr_str);
    }
    else if (prop->IsDict()) {
        string dict_str;
        dict_str.reserve(std::max<size_t>(raw_data.size() * 2, 128));
        bool next_iteration = false;
        auto dict_key_type = make_ptr(&prop->GetDictKeyType());

        if (dict_key_type->IsSimpleStruct) {
            dict_key_type = &dict_key_type->StructLayout->Fields.front().Type;
        }

        const auto get_key_len = [&dict_key_type, &prop](span<const uint8_t> key_data) -> size_t {
            if (dict_key_type->IsString) {
                if (key_data.size() < sizeof(uint32_t)) {
                    throw PropertySerializationException("Corrupted dict property data", prop->GetName());
                }

                const uint32_t str_len = ReadRawValue<uint32_t>(key_data.first(sizeof(uint32_t)));
                return sizeof(uint32_t) + str_len;
            }
            else {
                return dict_key_type->Size;
            }
        };

        const size_t key_alignment = dict_key_type->IsString ? sizeof(uint32_t) : alignment_for_size(dict_key_type->Size);

        while (data_pos < raw_data.size()) {
            if (next_iteration) {
                dict_str.append(" ");
            }
            else {
                next_iteration = true;
            }

            align_dict_pos(key_alignment);
            const auto key_len = get_key_len(raw_data.subspan(data_pos));
            const auto key_data = take_dict_data(key_len);
            size_t key_pos = 0;
            AppendBaseTypeToCodedStringAt(dict_str, *dict_key_type, hash_resolver, name_resolver, key_data, key_pos);
            FO_VERIFY_AND_THROW(key_pos == key_data.size(), "Dict key serialization did not consume the full key payload");
            dict_str.append(" ");

            if (prop->IsDictOfArray()) {
                string arr_str;
                arr_str.reserve(64);

                const auto arr_size = read_dict_size();

                if (arr_size != 0 && !base_type->IsRefType && !base_type->IsString) {
                    align_dict_pos(alignment_for_size(base_type->Size));
                }

                for (uint32_t i = 0; i < arr_size; i++) {
                    if (i != 0) {
                        arr_str.append(" ");
                    }

                    if (base_type->IsRefType) {
                        const auto ref_data_size = read_dict_size();

                        if (ref_data_size != 0) {
                            align_dict_pos(MAX_SERIALIZED_ALIGNMENT);
                        }

                        const auto ref_data = take_dict_data(ref_data_size);
                        StringEscaping::AppendCodeString(arr_str, SaveRefTypeToText(prop->GetName(), *base_type, ref_data, hash_resolver, name_resolver));
                    }
                    else {
                        AppendBaseTypeToCodedStringAt(arr_str, *base_type, hash_resolver, name_resolver, raw_data, data_pos);
                    }
                }

                StringEscaping::AppendCodeString(dict_str, arr_str);
            }
            else {
                if (base_type->IsRefType) {
                    const auto ref_data_size = read_dict_size();

                    if (ref_data_size != 0) {
                        align_dict_pos(MAX_SERIALIZED_ALIGNMENT);
                    }

                    const auto ref_data = take_dict_data(ref_data_size);
                    StringEscaping::AppendCodeString(dict_str, SaveRefTypeToText(prop->GetName(), *base_type, ref_data, hash_resolver, name_resolver));
                }
                else {
                    if (!base_type->IsString) {
                        align_dict_pos(alignment_for_size(base_type->Size));
                    }

                    AppendBaseTypeToCodedStringAt(dict_str, *base_type, hash_resolver, name_resolver, raw_data, data_pos);
                }
            }
        }

        StringEscaping::AppendCodeString(result, dict_str);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    FO_VERIFY_AND_THROW(data_pos == raw_data.size(), "Property coded-string serialization did not consume the full raw payload", prop->GetName(), data_pos, raw_data.size());
    return NormalizeTopLevelCodedString(std::move(result));
}

static void ConvertFixedValue(ptr<const Property> prop, const BaseTypeDesc& base_type, HashResolver& hash_resolver, NameResolver& name_resolver, const AnyData::Value& value, RawWriteCursor& cursor)
{
    FO_STACK_TRACE_ENTRY();

    if (base_type.IsHashedString) {
        hstring::hash_t hash = {};

        if (value.Type() == AnyData::ValueType::String) {
            const auto resolved_value = hash_resolver.ToHashedString(value.AsString());
            hash = resolved_value.as_hash();
        }
        else {
            throw PropertySerializationException("Wrong hash value type");
        }

        WriteCursorValue(cursor, hash);
    }
    else if (base_type.IsFixedType || base_type.IsEntityProto) {
        hstring::hash_t hash = {};

        if (value.Type() == AnyData::ValueType::String) {
            auto resolved_value = hash_resolver.ToHashedString(value.AsString());
            auto proto = name_resolver.GetProtoEntity(base_type.HashedName, resolved_value);

            if (proto) {
                resolved_value = proto->GetProtoId();
            }

            if (!resolved_value) {
                if (!prop->IsNullable()) {
                    throw PropertySerializationException("Proto reference property requires non-null proto, add Nullable or explicit Proto MigrationRule to valid target", prop->GetName(), base_type.Name);
                }
            }
            else if (!proto) {
                throw PropertySerializationException("Proto reference does not exist, add explicit Proto MigrationRule for deletion", base_type.Name, resolved_value);
            }

            hash = resolved_value.as_hash();
        }
        else {
            throw PropertySerializationException("Wrong hash value type");
        }

        WriteCursorValue(cursor, hash);
    }
    else if (base_type.IsEnum) {
        int32_t enum_value = 0;

        if (value.Type() == AnyData::ValueType::String) {
            enum_value = ResolveEnumValueWithMigration(base_type, hash_resolver, name_resolver, value.AsString());
        }
        else if (value.Type() == AnyData::ValueType::Int64) {
            enum_value = numeric_cast<int32_t>(value.AsInt64());
            const string_view enum_value_name = name_resolver.ResolveEnumValueName(base_type.Name, enum_value);
            ignore_unused(enum_value_name);
        }
        else {
            throw PropertySerializationException("Wrong enum value type (not string or int)");
        }

        WriteCursorEnumValue(cursor, enum_value, base_type.Size);
    }
    else if (base_type.IsPrimitive || base_type.IsSimpleStruct) {
        const auto& primitive_type = base_type.IsSimpleStruct ? base_type.StructLayout->Fields.front().Type : base_type;

        if (primitive_type.IsInt8) {
            int8_t result_value = {};
            ConvertToNumber(value, result_value);
            WriteCursorValue(cursor, result_value);
        }
        else if (primitive_type.IsInt16) {
            int16_t result_value = {};
            ConvertToNumber(value, result_value);
            WriteCursorValue(cursor, result_value);
        }
        else if (primitive_type.IsInt32) {
            int32_t result_value = {};
            ConvertToNumber(value, result_value);
            WriteCursorValue(cursor, result_value);
        }
        else if (primitive_type.IsInt64) {
            int64_t result_value = {};
            ConvertToNumber(value, result_value);
            WriteCursorValue(cursor, result_value);
        }
        else if (primitive_type.IsUInt8) {
            uint8_t result_value = {};
            ConvertToNumber(value, result_value);
            WriteCursorValue(cursor, result_value);
        }
        else if (primitive_type.IsUInt16) {
            uint16_t result_value = {};
            ConvertToNumber(value, result_value);
            WriteCursorValue(cursor, result_value);
        }
        else if (primitive_type.IsUInt32) {
            uint32_t result_value = {};
            ConvertToNumber(value, result_value);
            WriteCursorValue(cursor, result_value);
        }
        else if (primitive_type.IsSingleFloat) {
            float32_t result_value = {};
            ConvertToNumber(value, result_value);
            WriteCursorValue(cursor, result_value);
        }
        else if (primitive_type.IsDoubleFloat) {
            float64_t result_value = {};
            ConvertToNumber(value, result_value);
            WriteCursorValue(cursor, result_value);
        }
        else if (primitive_type.IsBool) {
            bool result_value = {};
            ConvertToNumber(value, result_value);
            WriteCursorValue(cursor, result_value);
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (base_type.IsComplexStruct) {
        auto struct_layout = base_type.StructLayout;
        FO_VERIFY_AND_THROW(struct_layout, "Struct layout is null");

        if (value.Type() == AnyData::ValueType::Array) {
            const auto& struct_value = value.AsArray();

            if (struct_value.Size() != struct_layout->Fields.size()) {
                throw PropertySerializationException("Wrong struct size (from array)", struct_value.Size(), struct_layout->Fields.size());
            }

            for (size_t i = 0; i < struct_layout->Fields.size(); i++) {
                const auto& field = struct_layout->Fields[i];
                const auto& field_value = struct_value[i];
                ConvertFixedValue(prop, field.Type, hash_resolver, name_resolver, field_value, cursor);
            }
        }
        else if (value.Type() == AnyData::ValueType::String) {
            const auto arr_value = AnyData::ParseValue(string {value.AsString()}, false, true, AnyData::ValueType::String);
            const auto& struct_value = arr_value.AsArray();

            if (struct_value.Size() != struct_layout->Fields.size()) {
                throw PropertySerializationException("Wrong struct size (from string)", struct_value.Size(), struct_layout->Fields.size());
            }

            for (size_t i = 0; i < struct_layout->Fields.size(); i++) {
                const auto& field = struct_layout->Fields[i];
                const auto& field_value = struct_value[i];
                ConvertFixedValue(prop, field.Type, hash_resolver, name_resolver, field_value, cursor);
            }
        }
        else {
            throw PropertySerializationException("Wrong struct value type", value.Type());
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

static void ConvertFixedValueAt(ptr<const Property> prop, const BaseTypeDesc& base_type, HashResolver& hash_resolver, NameResolver& name_resolver, const AnyData::Value& value, ptr<uint8_t> data, size_t& data_pos)
{
    FO_STACK_TRACE_ENTRY();

    RawWriteCursor cursor {data, data_pos};
    ConvertFixedValue(prop, base_type, hash_resolver, name_resolver, value, cursor);
    data_pos = cursor.Pos;
}

static void SetDataFromBuffer(const function<void(span<const uint8_t>)>& set_data, ptr<const uint8_t> data, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    if (size == 0) {
        set_data({});
        return;
    }

    set_data({data.get(), size});
}

static void SetDataFromString(const function<void(span<const uint8_t>)>& set_data, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    if (str.empty()) {
        set_data({});
        return;
    }

    set_data(make_const_span(str));
}

void PropertiesSerializator::LoadPropertyFromValue(ptr<const Property> prop, const AnyData::Value& value, const function<void(span<const uint8_t>)>& set_data, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(!prop->IsVirtual(), "Property is virtual");

    auto base_type = make_ptr(&prop->GetBaseType());

    // Parse value
    if (prop->IsPlainData()) {
        if (base_type->Size > sizeof(size_t)) {
            PropertyRawData struct_data;
            struct_data.Alloc(base_type->Size);
            auto data = struct_data.GetPtrAs<uint8_t>();
            size_t data_pos = 0;

            ConvertFixedValueAt(prop, *base_type, hash_resolver, name_resolver, value, data, data_pos);

            FO_VERIFY_AND_THROW(data_pos == base_type->Size, "Converted plain value did not fill the full base type size");
            SetDataFromBuffer(set_data, data, base_type->Size);
        }
        else {
            uint8_t primitive_data[sizeof(size_t)];
            auto data = make_ptr(primitive_data);
            size_t data_pos = 0;

            ConvertFixedValueAt(prop, *base_type, hash_resolver, name_resolver, value, data, data_pos);

            FO_VERIFY_AND_THROW(data_pos == base_type->Size, "Converted plain value did not fill the full base type size");
            SetDataFromBuffer(set_data, data, base_type->Size);
        }
    }
    else if (base_type->IsRefType && !prop->IsArray() && !prop->IsDict()) {
        const auto data = LoadRefTypeFromValue(prop->GetName(), *base_type, value, hash_resolver, name_resolver);
        set_data(data);
    }
    else if (prop->IsString()) {
        string str_buf;
        const string_view str = ConvertToString(value, str_buf);

        SetDataFromString(set_data, str);
    }
    else if (prop->IsArray()) {
        if (value.Type() != AnyData::ValueType::Array) {
            throw PropertySerializationException("Wrong array value type", prop->GetName(), value.Type());
        }

        const auto& arr = value.AsArray();

        if (arr.Empty()) {
            set_data({});
            return;
        }

        string str_buf;

        if (prop->IsArrayOfString()) {
            size_t data_size = sizeof(uint32_t);

            for (const auto& arr_entry : arr) {
                string_view str = ConvertToString(arr_entry, str_buf);
                data_size = align_up(data_size, sizeof(uint32_t));
                data_size += sizeof(uint32_t) + str.length();
            }

            auto data = SafeAlloc::MakeUniqueArr<uint8_t>(data_size);
            ptr<uint8_t> data_ptr = data.get();
            size_t data_pos = 0;

            const auto arr_size = numeric_cast<uint32_t>(arr.Size());
            WriteRawUInt32(data_ptr, data_pos, arr_size);

            for (const auto& arr_entry : arr) {
                const string_view str = ConvertToString(arr_entry, str_buf);
                const auto str_len = numeric_cast<uint32_t>(str.length());
                WriteRawUInt32(data_ptr, data_pos, str_len);
                WriteRawString(data_ptr, data_pos, str);
            }

            FO_VERIFY_AND_THROW(data_pos == data_size, "String array property buffer size does not match bytes written", prop->GetName(), data_pos, data_size);
            SetDataFromBuffer(set_data, data_ptr, data_size);
        }
        else if (base_type->IsRefType) {
            vector<vector<uint8_t>> ref_entries;
            ref_entries.reserve(arr.Size());
            size_t data_size = sizeof(uint32_t);

            for (const auto& arr_entry : arr) {
                auto ref_data = LoadRefTypeFromValue(prop->GetName(), *base_type, arr_entry, hash_resolver, name_resolver);
                data_size = align_up(data_size, sizeof(uint32_t));
                data_size += sizeof(uint32_t);

                if (!ref_data.empty()) {
                    data_size = align_up(data_size, MAX_SERIALIZED_ALIGNMENT);
                    data_size += ref_data.size();
                }

                ref_entries.emplace_back(std::move(ref_data));
            }

            auto data = SafeAlloc::MakeUniqueArr<uint8_t>(data_size);
            ptr<uint8_t> data_ptr = data.get();
            size_t data_pos = 0;
            const auto arr_size = numeric_cast<uint32_t>(arr.Size());
            WriteRawUInt32(data_ptr, data_pos, arr_size);

            for (const auto& ref_data : ref_entries) {
                const auto ref_data_size = numeric_cast<uint32_t>(ref_data.size());
                WriteRawUInt32(data_ptr, data_pos, ref_data_size);

                if (ref_data_size != 0) {
                    data_pos = align_up(data_pos, MAX_SERIALIZED_ALIGNMENT);
                    WriteRawSpan(data_ptr, data_pos, ref_data);
                }
            }

            FO_VERIFY_AND_THROW(data_pos == data_size, "Ref-type array property buffer size does not match bytes written", prop->GetName(), data_pos, data_size);
            SetDataFromBuffer(set_data, data_ptr, data_size);
        }
        else {
            const size_t data_size = arr.Size() * base_type->Size;
            auto data = SafeAlloc::MakeUniqueArr<uint8_t>(data_size);
            ptr<uint8_t> data_ptr = data.get();
            size_t data_pos = 0;

            for (const auto& arr_entry : arr) {
                ConvertFixedValueAt(prop, *base_type, hash_resolver, name_resolver, arr_entry, data_ptr, data_pos);
            }

            FO_VERIFY_AND_THROW(data_pos == data_size, "Plain array property buffer size does not match bytes written", prop->GetName(), data_pos, data_size);
            SetDataFromBuffer(set_data, data_ptr, data_size);
        }
    }
    else if (prop->IsDict()) {
        if (value.Type() != AnyData::ValueType::Dict) {
            throw PropertySerializationException("Wrong dict value type", prop->GetName(), value.Type());
        }

        const auto& dict = value.AsDict();

        if (dict.Empty()) {
            set_data({});
            return;
        }

        auto dict_key_type = make_ptr(&prop->GetDictKeyType());

        if (dict_key_type->IsSimpleStruct) {
            dict_key_type = &dict_key_type->StructLayout->Fields.front().Type;
        }
        string str_buf;

        // Measure data length
        size_t data_size = 0;

        for (auto&& [dict_key, dict_value] : dict) {
            if (dict_key_type->IsString) {
                data_size = align_up(data_size, sizeof(uint32_t));
                data_size += sizeof(uint32_t) + dict_key.length();
            }
            else {
                data_size = align_up(data_size, alignment_for_size(dict_key_type->Size));
                data_size += dict_key_type->Size;
            }

            if (prop->IsDictOfArray()) {
                if (dict_value.Type() != AnyData::ValueType::Array) {
                    throw PropertySerializationException("Wrong dict array value type", prop->GetName(), dict_value.Type());
                }

                const auto& arr = dict_value.AsArray();

                data_size = align_up(data_size, sizeof(uint32_t));
                data_size += sizeof(uint32_t);

                if (prop->IsDictOfArrayOfString()) {
                    for (const auto& arr_entry : arr) {
                        data_size = align_up(data_size, sizeof(uint32_t));
                        data_size += sizeof(uint32_t) + ConvertToString(arr_entry, str_buf).length();
                    }
                }
                else if (base_type->IsRefType) {
                    for (const auto& arr_entry : arr) {
                        data_size = align_up(data_size, sizeof(uint32_t));
                        data_size += sizeof(uint32_t);

                        const size_t ref_data_size = LoadRefTypeFromValue(prop->GetName(), *base_type, arr_entry, hash_resolver, name_resolver).size();

                        if (ref_data_size != 0) {
                            data_size = align_up(data_size, MAX_SERIALIZED_ALIGNMENT);
                            data_size += ref_data_size;
                        }
                    }
                }
                else {
                    if (!arr.Empty()) {
                        data_size = align_up(data_size, alignment_for_size(base_type->Size));
                        data_size += arr.Size() * base_type->Size;
                    }
                }
            }
            else if (prop->IsDictOfString()) {
                data_size = align_up(data_size, sizeof(uint32_t));
                data_size += sizeof(uint32_t) + ConvertToString(dict_value, str_buf).length();
            }
            else if (base_type->IsRefType) {
                data_size = align_up(data_size, sizeof(uint32_t));
                data_size += sizeof(uint32_t);

                const size_t ref_data_size = LoadRefTypeFromValue(prop->GetName(), *base_type, dict_value, hash_resolver, name_resolver).size();

                if (ref_data_size != 0) {
                    data_size = align_up(data_size, MAX_SERIALIZED_ALIGNMENT);
                    data_size += ref_data_size;
                }
            }
            else {
                data_size = align_up(data_size, alignment_for_size(base_type->Size));
                data_size += base_type->Size;
            }
        }

        // Write data
        auto data = SafeAlloc::MakeUniqueArr<uint8_t>(data_size);
        ptr<uint8_t> data_ptr = data.get();
        size_t data_pos = 0;
        const auto write_key_data = [&](nptr<const void> source, size_t size) {
            data_pos = align_up(data_pos, alignment_for_size(size));
            WriteRawBytes(data_ptr, data_pos, source, size);
        };
        const auto write_uint32 = [&](uint32_t value) { WriteRawUInt32(data_ptr, data_pos, value); };

        for (auto&& [dict_key, dict_value] : dict) {
            // Key
            if (dict_key_type->IsString) {
                const auto key_len = numeric_cast<uint32_t>(dict_key.length());
                write_uint32(key_len);
                WriteRawString(data_ptr, data_pos, dict_key);
            }
            else if (dict_key_type->IsHashedString) {
                const auto hash = hash_resolver.ToHashedString(dict_key).as_hash();
                write_key_data(&hash, sizeof(hash));
            }
            else if (dict_key_type->IsEnum) {
                const int32_t enum_value = ResolveEnumValueWithMigration(*dict_key_type, hash_resolver, name_resolver, dict_key);

                if (dict_key_type->Size == sizeof(uint8_t)) {
                    const auto converted_value = numeric_cast<uint8_t>(enum_value);
                    write_key_data(&converted_value, sizeof(converted_value));
                }
                else if (dict_key_type->Size == sizeof(uint16_t)) {
                    const auto converted_value = numeric_cast<uint16_t>(enum_value);
                    write_key_data(&converted_value, sizeof(converted_value));
                }
                else {
                    write_key_data(&enum_value, dict_key_type->Size);
                }
            }
            else if (dict_key_type->IsInt8) {
                const auto converted_value = numeric_cast<int8_t>(ParseStrictIntText(dict_key));
                write_key_data(&converted_value, sizeof(converted_value));
            }
            else if (dict_key_type->IsInt16) {
                const auto converted_value = numeric_cast<int16_t>(ParseStrictIntText(dict_key));
                write_key_data(&converted_value, sizeof(converted_value));
            }
            else if (dict_key_type->IsInt32) {
                const auto converted_value = numeric_cast<int32_t>(ParseStrictIntText(dict_key));
                write_key_data(&converted_value, sizeof(converted_value));
            }
            else if (dict_key_type->IsInt64) {
                const auto converted_value = numeric_cast<int64_t>(ParseStrictIntText(dict_key));
                write_key_data(&converted_value, sizeof(converted_value));
            }
            else if (dict_key_type->IsUInt8) {
                const auto converted_value = numeric_cast<uint8_t>(ParseStrictIntText(dict_key));
                write_key_data(&converted_value, sizeof(converted_value));
            }
            else if (dict_key_type->IsUInt16) {
                const auto converted_value = numeric_cast<uint16_t>(ParseStrictIntText(dict_key));
                write_key_data(&converted_value, sizeof(converted_value));
            }
            else if (dict_key_type->IsUInt32) {
                const auto converted_value = numeric_cast<uint32_t>(ParseStrictIntText(dict_key));
                write_key_data(&converted_value, sizeof(converted_value));
            }
            else if (dict_key_type->IsSingleFloat) {
                const auto converted_value = ConvertFloat64ToNumber<float32_t>(ParseStrictFloatText(dict_key));
                write_key_data(&converted_value, sizeof(converted_value));
            }
            else if (dict_key_type->IsDoubleFloat) {
                const auto converted_value = numeric_cast<float64_t>(ParseStrictFloatText(dict_key));
                write_key_data(&converted_value, sizeof(converted_value));
            }
            else if (dict_key_type->IsBool) {
                const auto converted_value = strvex(dict_key).to_bool();
                write_key_data(&converted_value, sizeof(converted_value));
            }
            else {
                FO_UNREACHABLE_PLACE();
            }

            // Value
            if (prop->IsDictOfArray()) {
                const auto& arr = dict_value.AsArray();

                write_uint32(numeric_cast<uint32_t>(arr.Size()));

                if (prop->IsDictOfArrayOfString()) {
                    for (const auto& arr_entry : arr) {
                        const string_view str = ConvertToString(arr_entry, str_buf);
                        write_uint32(numeric_cast<uint32_t>(str.length()));
                        WriteRawString(data_ptr, data_pos, str);
                    }
                }
                else if (base_type->IsRefType) {
                    for (const auto& arr_entry : arr) {
                        const auto ref_data = LoadRefTypeFromValue(prop->GetName(), *base_type, arr_entry, hash_resolver, name_resolver);
                        const auto ref_data_size = numeric_cast<uint32_t>(ref_data.size());
                        write_uint32(ref_data_size);

                        if (ref_data_size != 0) {
                            data_pos = align_up(data_pos, MAX_SERIALIZED_ALIGNMENT);
                            WriteRawSpan(data_ptr, data_pos, ref_data);
                        }
                    }
                }
                else {
                    if (!arr.Empty()) {
                        data_pos = align_up(data_pos, alignment_for_size(base_type->Size));
                    }

                    for (const auto& arr_entry : arr) {
                        ConvertFixedValueAt(prop, *base_type, hash_resolver, name_resolver, arr_entry, data_ptr, data_pos);
                    }
                }
            }
            else if (prop->IsDictOfString()) {
                const string_view str = ConvertToString(dict_value, str_buf);
                write_uint32(numeric_cast<uint32_t>(str.length()));
                WriteRawString(data_ptr, data_pos, str);
            }
            else if (base_type->IsRefType) {
                const auto ref_data = LoadRefTypeFromValue(prop->GetName(), *base_type, dict_value, hash_resolver, name_resolver);
                const auto ref_data_size = numeric_cast<uint32_t>(ref_data.size());
                write_uint32(ref_data_size);

                if (ref_data_size != 0) {
                    data_pos = align_up(data_pos, MAX_SERIALIZED_ALIGNMENT);
                    WriteRawSpan(data_ptr, data_pos, ref_data);
                }
            }
            else {
                data_pos = align_up(data_pos, alignment_for_size(base_type->Size));
                ConvertFixedValueAt(prop, *base_type, hash_resolver, name_resolver, dict_value, data_ptr, data_pos);
            }
        }

        FO_VERIFY_AND_THROW(data_pos == data_size, "Dict property buffer size does not match bytes written", prop->GetName(), data_pos, data_size);
        SetDataFromBuffer(set_data, data_ptr, data_size);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

void PropertiesSerializator::LoadPropertyFromText(ptr<Properties> props, ptr<const Property> prop, string_view text, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(!prop->IsVirtual(), "Property is virtual");

    auto set_data = [props, prop](span<const uint8_t> raw_data) mutable { props->SetRawData(prop, raw_data); };
    vector<uint8_t> data;

    if (prop->IsString()) {
        const auto decoded = StringEscaping::DecodeString(text);
        data.assign(decoded.begin(), decoded.end());
    }
    else if (prop->IsPlainData()) {
        if (prop->IsBaseTypeComplexStruct()) {
            data.reserve(prop->GetBaseSize());
            const string source_text {text};
            auto s = make_nptr(source_text.c_str());
            string_view token;

            for (const auto& field : prop->GetBaseTypeLayout().Fields) {
                s = ReadTextTokenView(s, token);
                if (!s) {
                    throw PropertySerializationException("Wrong struct size (from text)");
                }

                AppendBaseTypeFromText(data, prop, field.Type, token, hash_resolver, name_resolver);
            }

            if (ReadTextTokenView(s, token)) {
                throw PropertySerializationException("Wrong struct size (from text)");
            }
        }
        else {
            data.reserve(prop->GetBaseSize());
            AppendBaseTypeFromText(data, prop, prop->GetBaseType(), text, hash_resolver, name_resolver);
        }
    }
    else if (prop->IsBaseTypeRefType() && !prop->IsArray() && !prop->IsDict()) {
        if (text.empty()) {
            set_data({});
            return;
        }

        data = LoadRefTypeFromText(prop->GetName(), prop->GetBaseType(), text, hash_resolver, name_resolver);
    }
    else if (prop->IsArray()) {
        if (prop->IsBaseTypeRefType()) {
            if (!text.empty()) {
                const auto arr_value = AnyData::ParseValue(string {text}, false, true, AnyData::ValueType::String);
                const auto& arr = arr_value.AsArray();
                data.reserve(sizeof(uint32_t) + text.length() * 2);

                const auto arr_count = numeric_cast<uint32_t>(arr.Size());
                AppendRawScalarBytes(data, &arr_count, sizeof(arr_count));

                for (const auto& arr_entry : arr) {
                    const auto ref_data = LoadRefTypeFromText(prop->GetName(), prop->GetBaseType(), arr_entry.AsString(), hash_resolver, name_resolver);
                    const auto ref_data_size = numeric_cast<uint32_t>(ref_data.size());
                    AppendRawScalarBytes(data, &ref_data_size, sizeof(ref_data_size));

                    if (ref_data_size != 0) {
                        AlignRawBuffer(data, MAX_SERIALIZED_ALIGNMENT);
                        AppendRawBytes(data, span<const uint8_t> {ref_data});
                    }
                }
            }
        }
        else {
            data = ParseArrayFromText(prop, prop->GetBaseType(), prop->IsArrayOfString(), text, false, hash_resolver, name_resolver);
        }
    }
    else if (prop->IsDict()) {
        const string source_text {text};
        auto s = make_nptr(source_text.c_str());
        string_view key_token;
        string_view value_token;
        data.reserve(std::max<size_t>(text.length() * 2, 64));

        while ((s = ReadTextTokenView(s, key_token)) && (s = ReadTextTokenView(s, value_token))) {
            AppendBaseTypeFromText(data, prop, prop->GetDictKeyType(), key_token, hash_resolver, name_resolver);

            if (prop->IsDictOfArray()) {
                if (prop->IsBaseTypeRefType()) {
                    const auto decoded_arr = StringEscaping::DecodeString(value_token);

                    if (decoded_arr.empty()) {
                        constexpr uint32_t arr_count = 0;
                        AppendRawScalarBytes(data, &arr_count, sizeof(arr_count));
                    }
                    else {
                        const auto arr_value = AnyData::ParseValue(decoded_arr, false, true, AnyData::ValueType::String);
                        const auto& arr = arr_value.AsArray();
                        const auto arr_count = numeric_cast<uint32_t>(arr.Size());
                        AppendRawScalarBytes(data, &arr_count, sizeof(arr_count));

                        for (const auto& arr_entry : arr) {
                            const auto ref_data = LoadRefTypeFromText(prop->GetName(), prop->GetBaseType(), arr_entry.AsString(), hash_resolver, name_resolver);
                            const auto ref_data_size = numeric_cast<uint32_t>(ref_data.size());
                            AppendRawScalarBytes(data, &ref_data_size, sizeof(ref_data_size));

                            if (ref_data_size != 0) {
                                AlignRawBuffer(data, MAX_SERIALIZED_ALIGNMENT);
                                AppendRawBytes(data, span<const uint8_t> {ref_data});
                            }
                        }
                    }
                }
                else {
                    const auto arr_data = ParseArrayFromText(prop, prop->GetBaseType(), prop->IsDictOfArrayOfString(), value_token, true, hash_resolver, name_resolver);

                    if (prop->IsDictOfArrayOfString()) {
                        const auto arr_data_span = span<const uint8_t> {arr_data};
                        const auto arr_count = arr_data_span.empty() ? 0U : ReadRawValue<uint32_t>(arr_data_span.first(sizeof(uint32_t)));
                        AppendRawScalarBytes(data, &arr_count, sizeof(arr_count));

                        if (arr_data_span.size() > sizeof(uint32_t)) {
                            AppendRawBytes(data, arr_data_span.subspan(sizeof(uint32_t)));
                        }
                    }
                    else {
                        const auto arr_count = numeric_cast<uint32_t>(prop->GetBaseSize() == 0 ? 0 : arr_data.size() / prop->GetBaseSize());
                        AppendRawScalarBytes(data, &arr_count, sizeof(arr_count));

                        if (!arr_data.empty()) {
                            AlignRawBuffer(data, alignment_for_size(prop->GetBaseSize()));
                            AppendRawBytes(data, span<const uint8_t> {arr_data});
                        }
                    }
                }
            }
            else {
                if (prop->IsBaseTypeRefType()) {
                    const auto decoded_value = StringEscaping::DecodeString(value_token);
                    const auto ref_data = LoadRefTypeFromText(prop->GetName(), prop->GetBaseType(), decoded_value, hash_resolver, name_resolver);
                    const auto ref_data_size = numeric_cast<uint32_t>(ref_data.size());
                    AppendRawScalarBytes(data, &ref_data_size, sizeof(ref_data_size));

                    if (ref_data_size != 0) {
                        AlignRawBuffer(data, MAX_SERIALIZED_ALIGNMENT);
                        AppendRawBytes(data, span<const uint8_t> {ref_data});
                    }
                }
                else {
                    if (!prop->GetBaseType().IsString) {
                        AlignRawBuffer(data, alignment_for_size(prop->GetBaseSize()));
                    }

                    AppendBaseTypeFromText(data, prop, prop->GetBaseType(), value_token, hash_resolver, name_resolver);
                }
            }
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    set_data(data);
}

FO_END_NAMESPACE
