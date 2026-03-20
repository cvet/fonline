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

auto PropertiesSerializator::SaveToDocument(const Properties* props, const Properties* base, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Document
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!base || props->GetRegistrator() == base->GetRegistrator());

    AnyData::Document doc;

    for (size_t i = 1; i < props->GetRegistrator()->GetPropertiesCount(); i++) {
        const auto* prop = props->GetRegistrator()->GetPropertyByIndex(numeric_cast<int32>(i));

        if (prop->IsDisabled()) {
            continue;
        }
        if (!prop->IsPersistent()) {
            continue;
        }

        props->ValidateForRawData(prop);

        // Skip same as in base or zero values
        if (base != nullptr) {
            const auto base_raw_data = base->GetRawData(prop);
            const auto raw_data = props->GetRawData(prop);

            if (raw_data.size() == base_raw_data.size() && MemCompare(raw_data.data(), base_raw_data.data(), raw_data.size())) {
                continue;
            }
        }
        else {
            const auto raw_data = props->GetRawData(prop);

            if (prop->IsPlainData()) {
                const auto* pod_data = raw_data.data();
                const auto* pod_data_end = pod_data + raw_data.size();

                while (pod_data != pod_data_end && *pod_data == 0) {
                    ++pod_data;
                }

                if (pod_data == pod_data_end) {
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
        doc.Emplace(prop->GetName(), std::move(value));
    }

    return doc;
}

auto PropertiesSerializator::LoadFromDocument(Properties* props, const AnyData::Document& doc, HashResolver& hash_resolver, NameResolver& name_resolver) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool is_error = false;

    for (auto&& [doc_key, doc_value] : doc) {
        // Skip technical fields
        if (doc_key.empty() || doc_key[0] == '$' || doc_key[0] == '_') {
            continue;
        }

        try {
            // Find property
            const auto* prop = props->GetRegistrator()->FindProperty(doc_key);

            if (prop != nullptr && !prop->IsDisabled() && prop->IsPersistent()) {
                LoadPropertyFromValue(props, prop, doc_value, hash_resolver, name_resolver);
            }
            else {
                // Todo: maybe need some optional warning for unknown/wrong properties
                // WriteLog("Skip unknown property {}", key);
            }
        }
        catch (const std::exception& ex) {
            WriteLog("Unable to load property {}", doc_key);
            ReportExceptionAndContinue(ex);
            is_error = true;
        }
    }

    return !is_error;
}

auto PropertiesSerializator::SavePropertyToValue(const Properties* props, const Property* prop, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());

    props->ValidateForRawData(prop);

    const auto raw_data = props->GetRawData(prop);

    return SavePropertyToValue(prop, raw_data, hash_resolver, name_resolver);
}

static auto NormalizeTopLevelCodedString(string str) -> string
{
    if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
        if (str[1] != ' ' && str[1] != '\t' && str[str.length() - 2] != ' ' && str[str.length() - 2] != '\t') {
            str = StringEscaping::DecodeString(str);
        }
    }

    return str;
}

static auto ReadTextTokenView(const char* str, string_view& result) -> const char*
{
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
        pos++;
        begin = pos;

        while (str[pos] != 0) {
            if (length == 1 && str[pos] == '\\') {
                pos++;

                if (str[pos] != 0) {
                    length = utf8::DecodeStrNtLen(&str[pos]);
                    utf8::Decode(&str[pos], length);

                    pos += length;
                }
            }
            else if (length == 1 && str[pos] == '"') {
                break;
            }
            else {
                pos += length;
            }

            length = utf8::DecodeStrNtLen(&str[pos]);
            utf8::Decode(&str[pos], length);
        }
    }
    else {
        begin = pos;

        while (str[pos] != 0) {
            if (length == 1 && str[pos] == '\\') {
                pos++;

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

    result = string_view {&str[begin], pos - begin};
    return str[pos] != 0 ? &str[pos + 1] : &str[pos];
}

static void AppendRawBytes(vector<uint8>& data, const void* value, size_t size)
{
    const auto old_size = data.size();
    data.resize(old_size + size);
    MemCopy(data.data() + old_size, value, size);
}

static void AppendRawString(vector<uint8>& data, string_view str)
{
    const auto str_len = numeric_cast<uint32>(str.length());
    AppendRawBytes(data, &str_len, sizeof(str_len));

    if (!str.empty()) {
        AppendRawBytes(data, str.data(), str.length());
    }
}

static auto DecodeTextIfNeeded(string_view text, string& decoded_storage) -> string_view
{
    if (text.find_first_of("\\\"") == string_view::npos) {
        return text;
    }

    decoded_storage = StringEscaping::DecodeString(text);
    return decoded_storage;
}

static void AppendBaseTypeFromText(vector<uint8>& data, const Property* prop, const BaseTypeDesc& base_type, string_view text, HashResolver& hash_resolver, NameResolver& name_resolver);

static void AppendPrimitiveFromText(vector<uint8>& data, const BaseTypeDesc& primitive_type, string_view text)
{
    if (primitive_type.IsInt8) {
        const auto value = numeric_cast<int8>(strvex(text).to_int64());
        AppendRawBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsInt16) {
        const auto value = numeric_cast<int16>(strvex(text).to_int64());
        AppendRawBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsInt32) {
        const auto value = numeric_cast<int32>(strvex(text).to_int64());
        AppendRawBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsInt64) {
        const auto value = numeric_cast<int64>(strvex(text).to_int64());
        AppendRawBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsUInt8) {
        const auto value = numeric_cast<uint8>(strvex(text).to_int64());
        AppendRawBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsUInt16) {
        const auto value = numeric_cast<uint16>(strvex(text).to_int64());
        AppendRawBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsUInt32) {
        const auto value = numeric_cast<uint32>(strvex(text).to_int64());
        AppendRawBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsSingleFloat) {
        const auto value = numeric_cast<float32>(strvex(text).to_float64());
        AppendRawBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsDoubleFloat) {
        const auto value = numeric_cast<float64>(strvex(text).to_float64());
        AppendRawBytes(data, &value, sizeof(value));
    }
    else if (primitive_type.IsBool) {
        const auto value = strvex(text).to_bool();
        AppendRawBytes(data, &value, sizeof(value));
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

static void AppendComplexStructFromText(vector<uint8>& data, const Property* prop, const BaseTypeDesc& base_type, string_view text, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    const auto decoded = StringEscaping::DecodeString(text);
    const char* s = decoded.c_str();
    string_view token;

    for (const auto& field : base_type.StructLayout->Fields) {
        s = ReadTextTokenView(s, token);

        if (s == nullptr) {
            throw PropertySerializationException("Wrong struct size (from text)");
        }

        AppendBaseTypeFromText(data, prop, field.Type, token, hash_resolver, name_resolver);
    }

    if (ReadTextTokenView(s, token) != nullptr) {
        throw PropertySerializationException("Wrong struct size (from text)");
    }
}

static void AppendBaseTypeFromText(vector<uint8>& data, const Property* prop, const BaseTypeDesc& base_type, string_view text, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    if (base_type.IsString) {
        string decoded_storage;
        AppendRawString(data, DecodeTextIfNeeded(text, decoded_storage));
    }
    else if (base_type.IsHashedString) {
        string decoded_storage;
        auto resolved_value = hash_resolver.ToHashedString(DecodeTextIfNeeded(text, decoded_storage));
        const auto hash = resolved_value.as_hash();
        AppendRawBytes(data, &hash, sizeof(hash));
    }
    else if (base_type.IsFixedType) {
        string decoded_storage;
        auto resolved_value = hash_resolver.ToHashedString(DecodeTextIfNeeded(text, decoded_storage));
        const auto* proto = name_resolver.GetProtoEntity(base_type.HashedName, resolved_value);

        if (proto != nullptr) {
            resolved_value = proto->GetProtoId();
        }

        if (!resolved_value) {
            if (prop == nullptr || !prop->IsMaybeNull()) {
                throw PropertySerializationException("FixedType property requires non-null proto, add MaybeNull or explicit Proto MigrationRule to valid target", prop != nullptr ? prop->GetName() : base_type.Name, base_type.Name);
            }
        }
        else if (proto == nullptr) {
            throw PropertySerializationException("FixedType proto does not exists, add explicit Proto MigrationRule for deletion", base_type.Name, resolved_value);
        }

        const auto hash = resolved_value.as_hash();
        AppendRawBytes(data, &hash, sizeof(hash));
    }
    else if (base_type.IsEnum) {
        string decoded_storage;
        const auto decoded = DecodeTextIfNeeded(text, decoded_storage);
        int32 enum_value = 0;

        if (strvex(decoded).is_number()) {
            enum_value = numeric_cast<int32>(strvex(decoded).to_int64());
            ignore_unused(name_resolver.ResolveEnumValueName(base_type.Name, enum_value));
        }
        else {
            enum_value = name_resolver.ResolveEnumValue(base_type.Name, decoded);
        }

        if (base_type.Size == sizeof(uint8)) {
            const auto value = numeric_cast<uint8>(enum_value);
            AppendRawBytes(data, &value, sizeof(value));
        }
        else if (base_type.Size == sizeof(uint16)) {
            const auto value = numeric_cast<uint16>(enum_value);
            AppendRawBytes(data, &value, sizeof(value));
        }
        else {
            AppendRawBytes(data, &enum_value, base_type.Size);
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

static auto ParseArrayFromText(const Property* prop, const BaseTypeDesc& base_type, bool is_array_of_string, string_view text, bool encoded_text, HashResolver& hash_resolver, NameResolver& name_resolver) -> vector<uint8>
{
    const auto decoded = encoded_text ? StringEscaping::DecodeString(text) : string(text);
    const char* s = decoded.c_str();
    string_view token;
    vector<uint8> data;
    data.reserve(std::max<size_t>(decoded.length() + sizeof(uint32), 32));
    uint32 arr_size = 0;

    if (is_array_of_string) {
        data.resize(sizeof(uint32));
    }

    while ((s = ReadTextTokenView(s, token)) != nullptr) {
        AppendBaseTypeFromText(data, prop, base_type, token, hash_resolver, name_resolver);
        arr_size++;
    }

    if (is_array_of_string) {
        MemCopy(data.data(), &arr_size, sizeof(arr_size));
    }

    return data;
}

static void AppendPrimitiveToCodedString(string& result, const BaseTypeDesc& primitive_type, const uint8*& pdata)
{
    if (primitive_type.IsInt8) {
        result += strex("{}", *reinterpret_cast<const int8*>(pdata));
    }
    else if (primitive_type.IsInt16) {
        result += strex("{}", *reinterpret_cast<const int16*>(pdata));
    }
    else if (primitive_type.IsInt32) {
        result += strex("{}", *reinterpret_cast<const int32*>(pdata));
    }
    else if (primitive_type.IsInt64) {
        result += strex("{}", *reinterpret_cast<const int64*>(pdata));
    }
    else if (primitive_type.IsUInt8) {
        result += strex("{}", *reinterpret_cast<const uint8*>(pdata));
    }
    else if (primitive_type.IsUInt16) {
        result += strex("{}", *reinterpret_cast<const uint16*>(pdata));
    }
    else if (primitive_type.IsUInt32) {
        result += strex("{}", *reinterpret_cast<const uint32*>(pdata));
    }
    else if (primitive_type.IsSingleFloat) {
        result += strex("{:f}", *reinterpret_cast<const float32*>(pdata)).rtrim("0").rtrim(".");
    }
    else if (primitive_type.IsDoubleFloat) {
        result += strex("{:f}", *reinterpret_cast<const float64*>(pdata)).rtrim("0").rtrim(".");
    }
    else if (primitive_type.IsBool) {
        result += *reinterpret_cast<const bool*>(pdata) ? "True" : "False";
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    pdata += primitive_type.Size;
}

static void AppendBaseTypeToCodedString(string& result, const BaseTypeDesc& base_type, HashResolver& hash_resolver, NameResolver& name_resolver, const uint8*& pdata)
{
    if (base_type.IsString) {
        const uint32 str_len = *reinterpret_cast<const uint32*>(pdata);
        pdata += sizeof(str_len);
        StringEscaping::AppendCodeString(result, {reinterpret_cast<const char*>(pdata), str_len});
        pdata += str_len;
    }
    else if (base_type.IsHashedString) {
        const auto hash = *reinterpret_cast<const hstring::hash_t*>(pdata);
        pdata += sizeof(hstring::hash_t);
        StringEscaping::AppendCodeString(result, hash_resolver.ResolveHash(hash).as_str());
    }
    else if (base_type.IsEnum) {
        int32 enum_value = 0;
        MemCopy(&enum_value, pdata, base_type.Size);
        pdata += base_type.Size;
        StringEscaping::AppendCodeString(result, name_resolver.ResolveEnumValueName(base_type.Name, enum_value));
    }
    else if (base_type.IsPrimitive || base_type.IsSimpleStruct) {
        const auto& primitive_type = base_type.IsSimpleStruct ? base_type.StructLayout->Fields.front().Type : base_type;
        AppendPrimitiveToCodedString(result, primitive_type, pdata);
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

            AppendBaseTypeToCodedString(struct_str, field.Type, hash_resolver, name_resolver, pdata);
        }

        StringEscaping::AppendCodeString(result, struct_str);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

static auto RawDataToValue(const BaseTypeDesc& base_type, HashResolver& hash_resolver, NameResolver& name_resolver, const uint8*& pdata) -> AnyData::Value
{
    if (base_type.IsString) {
        const uint32 str_len = *reinterpret_cast<const uint32*>(pdata);
        pdata += sizeof(str_len);
        const auto* pstr = reinterpret_cast<const char*>(pdata);
        pdata += str_len;
        return string(pstr, str_len);
    }
    else if (base_type.IsHashedString || base_type.IsFixedType) {
        const auto hash = *reinterpret_cast<const hstring::hash_t*>(pdata);
        pdata += sizeof(hstring::hash_t);
        return hash_resolver.ResolveHash(hash).as_str();
    }
    else if (base_type.IsEnum) {
        int32 enum_value = 0;
        MemCopy(&enum_value, pdata, base_type.Size);
        pdata += base_type.Size;
        return name_resolver.ResolveEnumValueName(base_type.Name, enum_value);
    }
    else if (base_type.IsPrimitive || base_type.IsSimpleStruct) {
        const auto& primitive_type = base_type.IsSimpleStruct ? base_type.StructLayout->Fields.front().Type : base_type;
        pdata += primitive_type.Size;

        if (primitive_type.IsInt8) {
            return numeric_cast<int64>(*reinterpret_cast<const int8*>(pdata - primitive_type.Size));
        }
        else if (primitive_type.IsInt16) {
            return numeric_cast<int64>(*reinterpret_cast<const int16*>(pdata - primitive_type.Size));
        }
        else if (primitive_type.IsInt32) {
            return numeric_cast<int64>(*reinterpret_cast<const int32*>(pdata - primitive_type.Size));
        }
        else if (primitive_type.IsInt64) {
            return numeric_cast<int64>(*reinterpret_cast<const int64*>(pdata - primitive_type.Size));
        }
        else if (primitive_type.IsUInt8) {
            return numeric_cast<int64>(*reinterpret_cast<const uint8*>(pdata - primitive_type.Size));
        }
        else if (primitive_type.IsUInt16) {
            return numeric_cast<int64>(*reinterpret_cast<const uint16*>(pdata - primitive_type.Size));
        }
        else if (primitive_type.IsUInt32) {
            return numeric_cast<int64>(*reinterpret_cast<const uint32*>(pdata - primitive_type.Size));
        }
        else if (primitive_type.IsFloat) {
            return numeric_cast<float64>(*reinterpret_cast<const float32*>(pdata - primitive_type.Size));
        }
        else if (primitive_type.IsDoubleFloat) {
            return numeric_cast<float64>(*reinterpret_cast<const float64*>(pdata - primitive_type.Size));
        }
        else if (primitive_type.IsBool) {
            return *reinterpret_cast<const bool*>(pdata - primitive_type.Size);
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (base_type.IsComplexStruct) {
        const auto& struct_layout = *base_type.StructLayout;
        AnyData::Array struct_value;
        struct_value.Reserve(struct_layout.Fields.size());

        for (const auto& field : struct_layout.Fields) {
            auto field_value = RawDataToValue(field.Type, hash_resolver, name_resolver, pdata);
            struct_value.EmplaceBack(std::move(field_value));
        }

        return std::move(struct_value);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

auto PropertiesSerializator::SavePropertyToValue(const Property* prop, span<const uint8> raw_data, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());

    const BaseTypeDesc& base_type = prop->GetBaseType();
    const auto* pdata = raw_data.data();
    const auto* pdata_end = raw_data.data() + raw_data.size();

    if (prop->IsPlainData()) {
        auto value = RawDataToValue(base_type, hash_resolver, name_resolver, pdata);

        FO_RUNTIME_ASSERT(pdata == pdata_end);
        return value;
    }
    else if (prop->IsString()) {
        const auto* pstr = reinterpret_cast<const char*>(pdata);
        pdata += raw_data.size();

        FO_RUNTIME_ASSERT(pdata == pdata_end);
        return string(pstr, raw_data.size());
    }
    else if (prop->IsArray()) {
        AnyData::Array arr;

        if (!raw_data.empty()) {
            uint32 arr_size;

            if (prop->IsArrayOfString()) {
                MemCopy(&arr_size, pdata, sizeof(arr_size));
                pdata += sizeof(uint32);
            }
            else {
                arr_size = numeric_cast<uint32>(raw_data.size() / base_type.Size);
            }

            arr.Reserve(arr_size);

            for (uint32 i = 0; i < arr_size; i++) {
                auto arr_entry = RawDataToValue(base_type, hash_resolver, name_resolver, pdata);
                arr.EmplaceBack(std::move(arr_entry));
            }
        }

        FO_RUNTIME_ASSERT(pdata == pdata_end);
        return std::move(arr);
    }
    else if (prop->IsDict()) {
        AnyData::Dict dict;

        if (!raw_data.empty()) {
            const auto& dict_key_type_ = prop->GetDictKeyType();
            const auto& dict_key_type = dict_key_type_.IsSimpleStruct ? dict_key_type_.StructLayout->Fields.front().Type : dict_key_type_;

            const auto get_key_string = [&dict_key_type, &hash_resolver, &name_resolver](const uint8* p) -> string {
                if (dict_key_type.IsString) {
                    const uint32 str_len = *reinterpret_cast<const uint32*>(p);
                    return string {reinterpret_cast<const char*>(p + sizeof(uint32)), str_len};
                }
                else if (dict_key_type.IsHashedString) {
                    const auto hash = *reinterpret_cast<const hstring::hash_t*>(p);
                    return hash_resolver.ResolveHash(hash).as_str();
                }
                else if (dict_key_type.IsEnum) {
                    int32 enum_value = 0;
                    MemCopy(&enum_value, p, dict_key_type.Size);
                    return name_resolver.ResolveEnumValueName(dict_key_type.Name, enum_value);
                }
                else if (dict_key_type.IsInt8) {
                    return strex("{}", *reinterpret_cast<const int8*>(p));
                }
                else if (dict_key_type.IsInt16) {
                    return strex("{}", *reinterpret_cast<const int16*>(p));
                }
                else if (dict_key_type.IsInt32) {
                    return strex("{}", *reinterpret_cast<const int32*>(p));
                }
                else if (dict_key_type.IsInt64) {
                    return strex("{}", *reinterpret_cast<const int64*>(p));
                }
                else if (dict_key_type.IsUInt8) {
                    return strex("{}", *reinterpret_cast<const uint8*>(p));
                }
                else if (dict_key_type.IsUInt16) {
                    return strex("{}", *reinterpret_cast<const uint16*>(p));
                }
                else if (dict_key_type.IsUInt32) {
                    return strex("{}", *reinterpret_cast<const uint32*>(p));
                }
                else if (dict_key_type.IsSingleFloat) {
                    return strex("{}", *reinterpret_cast<const float32*>(p));
                }
                else if (dict_key_type.IsDoubleFloat) {
                    return strex("{}", *reinterpret_cast<const float64*>(p));
                }
                else if (dict_key_type.IsBool) {
                    return *reinterpret_cast<const bool*>(p) ? "True" : "False";
                }
                else {
                    FO_UNREACHABLE_PLACE();
                }
            };

            const auto get_key_len = [&dict_key_type](const uint8* p) -> size_t {
                if (dict_key_type.IsString) {
                    const uint32 str_len = *reinterpret_cast<const uint32*>(p);
                    return sizeof(uint32) + str_len;
                }
                else {
                    return dict_key_type.Size;
                }
            };

            if (prop->IsDictOfArray()) {
                while (pdata < pdata_end) {
                    const auto* key_data = pdata;
                    pdata += get_key_len(key_data);
                    string key_str = get_key_string(key_data);

                    uint32 arr_size;
                    MemCopy(&arr_size, pdata, sizeof(arr_size));
                    pdata += sizeof(uint32);

                    AnyData::Array arr;
                    arr.Reserve(arr_size);

                    for (uint32 i = 0; i < arr_size; i++) {
                        auto arr_entry = RawDataToValue(base_type, hash_resolver, name_resolver, pdata);
                        arr.EmplaceBack(std::move(arr_entry));
                    }

                    dict.Emplace(std::move(key_str), std::move(arr));
                }
            }
            else {
                while (pdata < pdata_end) {
                    const auto* key_data = pdata;
                    pdata += get_key_len(key_data);
                    string key_str = get_key_string(key_data);

                    auto dict_value = RawDataToValue(base_type, hash_resolver, name_resolver, pdata);
                    dict.Emplace(std::move(key_str), std::move(dict_value));
                }
            }
        }

        FO_RUNTIME_ASSERT(pdata == pdata_end);
        return std::move(dict);
    }

    FO_UNREACHABLE_PLACE();
}

void PropertiesSerializator::LoadPropertyFromValue(Properties* props, const Property* prop, const AnyData::Value& value, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());

    const auto set_data = [props, prop](span<const uint8> raw_data) { props->SetRawData(prop, raw_data); };

    return LoadPropertyFromValue(prop, value, set_data, hash_resolver, name_resolver);
}

static auto ConvertToString(const AnyData::Value& value, string& buf) -> const string&
{
    FO_STACK_TRACE_ENTRY();

    switch (value.Type()) {
    case AnyData::ValueType::String:
        return value.AsString();
    case AnyData::ValueType::Int64:
        return buf = strex("{}", value.AsInt64());
    case AnyData::ValueType::Float64:
        return buf = strex("{}", value.AsDouble());
    case AnyData::ValueType::Bool:
        return buf = strex("{}", value.AsBool());
    default:
        throw PropertySerializationException("Unable to convert not string, int32, float32 or bool value to string", value.Type());
    }
};

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
        if constexpr (std::same_as<T, bool>) {
            result_value = !is_float_equal(value.AsDouble(), 0.0);
        }
        else if constexpr (std::integral<T>) {
            result_value = iround<T>(value.AsDouble());
        }
        else {
            result_value = numeric_cast<T>(value.AsDouble());
        }
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
        const auto& str = value.AsString();

        if (strvex(str).is_number()) {
            if constexpr (std::same_as<T, bool>) {
                result_value = strvex(str).to_bool();
            }
            else if constexpr (std::floating_point<T>) {
                result_value = numeric_cast<T>(strvex(str).to_float64());
            }
            else {
                result_value = numeric_cast<T>(strvex(str).to_int64());
            }
        }
        else if (strvex(str).is_explicit_bool()) {
            if constexpr (std::same_as<T, bool>) {
                result_value = strvex(str).to_bool();
            }
            else if constexpr (std::floating_point<T>) {
                result_value = strvex(str).to_bool() ? 1.0f : 0.0f;
            }
            else {
                result_value = strvex(str).to_bool() ? 1 : 0;
            }
        }
        else {
            throw PropertySerializationException("Unable to convert value to number", str);
        }
    }
    else {
        throw PropertySerializationException("Wrong value type (not string, int, float or bool)", value.Type());
    }
}

auto PropertiesSerializator::SavePropertyToText(const Properties* props, const Property* prop, HashResolver& hash_resolver, NameResolver& name_resolver) -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());

    props->ValidateForRawData(prop);

    return SavePropertyToText(prop, props->GetRawData(prop), hash_resolver, name_resolver);
}

auto PropertiesSerializator::SavePropertyToText(const Property* prop, span<const uint8> raw_data, HashResolver& hash_resolver, NameResolver& name_resolver) -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());

    const BaseTypeDesc& base_type = prop->GetBaseType();
    const auto* pdata = raw_data.data();
    const auto* pdata_end = raw_data.data() + raw_data.size();
    string result;
    result.reserve(std::max<size_t>(raw_data.size() * 2, 64));

    if (prop->IsString()) {
        StringEscaping::AppendCodeString(result, {reinterpret_cast<const char*>(raw_data.data()), raw_data.size()});
        pdata += raw_data.size();
    }
    else if (prop->IsPlainData()) {
        AppendBaseTypeToCodedString(result, base_type, hash_resolver, name_resolver, pdata);
    }
    else if (prop->IsArray()) {
        string arr_str;
        arr_str.reserve(std::max<size_t>(raw_data.size() * 2, 64));
        bool next_iteration = false;

        if (!raw_data.empty()) {
            uint32 arr_size;

            if (prop->IsArrayOfString()) {
                MemCopy(&arr_size, pdata, sizeof(arr_size));
                pdata += sizeof(uint32);
            }
            else {
                arr_size = numeric_cast<uint32>(raw_data.size() / base_type.Size);
            }

            for (uint32 i = 0; i < arr_size; i++) {
                if (next_iteration) {
                    arr_str.append(" ");
                }
                else {
                    next_iteration = true;
                }

                AppendBaseTypeToCodedString(arr_str, base_type, hash_resolver, name_resolver, pdata);
            }
        }

        StringEscaping::AppendCodeString(result, arr_str);
    }
    else if (prop->IsDict()) {
        string dict_str;
        dict_str.reserve(std::max<size_t>(raw_data.size() * 2, 128));
        bool next_iteration = false;
        const auto& dict_key_type_ = prop->GetDictKeyType();
        const auto& dict_key_type = dict_key_type_.IsSimpleStruct ? dict_key_type_.StructLayout->Fields.front().Type : dict_key_type_;

        const auto append_key_to_dict = [&](string& str, const uint8*& key_pdata) {
            if (dict_key_type.IsString) {
                const uint32 str_len = *reinterpret_cast<const uint32*>(key_pdata);
                key_pdata += sizeof(str_len);
                StringEscaping::AppendCodeString(str, {reinterpret_cast<const char*>(key_pdata), str_len});
                key_pdata += str_len;
            }
            else if (dict_key_type.IsHashedString) {
                const auto hash = *reinterpret_cast<const hstring::hash_t*>(key_pdata);
                key_pdata += sizeof(hstring::hash_t);
                StringEscaping::AppendCodeString(str, hash_resolver.ResolveHash(hash).as_str());
            }
            else if (dict_key_type.IsEnum) {
                int32 enum_value = 0;
                MemCopy(&enum_value, key_pdata, dict_key_type.Size);
                key_pdata += dict_key_type.Size;
                StringEscaping::AppendCodeString(str, name_resolver.ResolveEnumValueName(dict_key_type.Name, enum_value));
            }
            else {
                AppendPrimitiveToCodedString(str, dict_key_type, key_pdata);
            }
        };

        while (pdata < pdata_end) {
            if (next_iteration) {
                dict_str.append(" ");
            }
            else {
                next_iteration = true;
            }

            append_key_to_dict(dict_str, pdata);
            dict_str.append(" ");

            if (prop->IsDictOfArray()) {
                string arr_str;
                arr_str.reserve(64);
                uint32 arr_size;
                MemCopy(&arr_size, pdata, sizeof(arr_size));
                pdata += sizeof(arr_size);

                for (uint32 i = 0; i < arr_size; i++) {
                    if (i != 0) {
                        arr_str.append(" ");
                    }

                    AppendBaseTypeToCodedString(arr_str, base_type, hash_resolver, name_resolver, pdata);
                }

                StringEscaping::AppendCodeString(dict_str, arr_str);
            }
            else {
                AppendBaseTypeToCodedString(dict_str, base_type, hash_resolver, name_resolver, pdata);
            }
        }

        StringEscaping::AppendCodeString(result, dict_str);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    FO_RUNTIME_ASSERT(pdata == pdata_end);
    return NormalizeTopLevelCodedString(std::move(result));
}

static void ConvertFixedValue(const Property* prop, const BaseTypeDesc& base_type, HashResolver& hash_resolver, NameResolver& name_resolver, const AnyData::Value& value, uint8*& pdata)
{
    FO_STACK_TRACE_ENTRY();

    if (base_type.IsHashedString || base_type.IsFixedType) {
        auto& hash = *reinterpret_cast<hstring::hash_t*>(pdata);

        if (value.Type() == AnyData::ValueType::String) {
            auto resolved_value = hash_resolver.ToHashedString(value.AsString());

            if (base_type.IsFixedType) {
                const auto* proto = name_resolver.GetProtoEntity(base_type.HashedName, resolved_value);

                if (proto != nullptr) {
                    resolved_value = proto->GetProtoId();
                }

                if (!resolved_value) {
                    if (!prop->IsMaybeNull()) {
                        throw PropertySerializationException("FixedType property requires non-null proto, add MaybeNull or explicit Proto MigrationRule to valid target", prop->GetName(), base_type.Name);
                    }
                }
                else if (proto == nullptr) {
                    throw PropertySerializationException("FixedType proto does not exists, add explicit Proto MigrationRule for deletion", base_type.Name, resolved_value);
                }
            }

            hash = resolved_value.as_hash();
        }
        else {
            throw PropertySerializationException("Wrong hash value type");
        }

        pdata += base_type.Size;
    }
    else if (base_type.IsEnum) {
        int32 enum_value = 0;

        if (value.Type() == AnyData::ValueType::String) {
            enum_value = name_resolver.ResolveEnumValue(base_type.Name, value.AsString());
        }
        else if (value.Type() == AnyData::ValueType::Int64) {
            enum_value = numeric_cast<int32>(value.AsInt64());
            const auto& enum_value_name = name_resolver.ResolveEnumValueName(base_type.Name, enum_value);
            ignore_unused(enum_value_name);
        }
        else {
            throw PropertySerializationException("Wrong enum value type (not string or int)");
        }

        if (base_type.Size == sizeof(uint8)) {
            *reinterpret_cast<uint8*>(pdata) = numeric_cast<uint8>(enum_value);
        }
        else if (base_type.Size == sizeof(uint16)) {
            *reinterpret_cast<uint16*>(pdata) = numeric_cast<uint16>(enum_value);
        }
        else {
            MemCopy(pdata, &enum_value, base_type.Size);
        }

        pdata += base_type.Size;
    }
    else if (base_type.IsPrimitive || base_type.IsSimpleStruct) {
        const auto& primitive_type = base_type.IsSimpleStruct ? base_type.StructLayout->Fields.front().Type : base_type;

        if (primitive_type.IsInt8) {
            ConvertToNumber(value, *reinterpret_cast<int8*>(pdata));
        }
        else if (primitive_type.IsInt16) {
            ConvertToNumber(value, *reinterpret_cast<int16*>(pdata));
        }
        else if (primitive_type.IsInt32) {
            ConvertToNumber(value, *reinterpret_cast<int32*>(pdata));
        }
        else if (primitive_type.IsInt64) {
            ConvertToNumber(value, *reinterpret_cast<int64*>(pdata));
        }
        else if (primitive_type.IsUInt8) {
            ConvertToNumber(value, *reinterpret_cast<uint8*>(pdata));
        }
        else if (primitive_type.IsUInt16) {
            ConvertToNumber(value, *reinterpret_cast<uint16*>(pdata));
        }
        else if (primitive_type.IsUInt32) {
            ConvertToNumber(value, *reinterpret_cast<uint32*>(pdata));
        }
        else if (primitive_type.IsSingleFloat) {
            ConvertToNumber(value, *reinterpret_cast<float32*>(pdata));
        }
        else if (primitive_type.IsDoubleFloat) {
            ConvertToNumber(value, *reinterpret_cast<float64*>(pdata));
        }
        else if (primitive_type.IsBool) {
            ConvertToNumber(value, *reinterpret_cast<bool*>(pdata));
        }
        else {
            FO_UNREACHABLE_PLACE();
        }

        pdata += base_type.Size;
    }
    else if (base_type.IsComplexStruct) {
        const auto& struct_layout = *base_type.StructLayout;

        if (value.Type() == AnyData::ValueType::Array) {
            const auto& struct_value = value.AsArray();

            if (struct_value.Size() != struct_layout.Fields.size()) {
                throw PropertySerializationException("Wrong struct size (from array)", struct_value.Size(), struct_layout.Fields.size());
            }

            for (size_t i = 0; i < struct_layout.Fields.size(); i++) {
                const auto& field = struct_layout.Fields[i];
                const auto& field_value = struct_value[i];
                ConvertFixedValue(prop, field.Type, hash_resolver, name_resolver, field_value, pdata);
            }
        }
        else if (value.Type() == AnyData::ValueType::String) {
            const auto arr_value = AnyData::ParseValue(value.AsString(), false, true, AnyData::ValueType::String);
            const auto& struct_value = arr_value.AsArray();

            if (struct_value.Size() != struct_layout.Fields.size()) {
                throw PropertySerializationException("Wrong struct size (from string)", struct_value.Size(), struct_layout.Fields.size());
            }

            for (size_t i = 0; i < struct_layout.Fields.size(); i++) {
                const auto& field = struct_layout.Fields[i];
                const auto& field_value = struct_value[i];
                ConvertFixedValue(prop, field.Type, hash_resolver, name_resolver, field_value, pdata);
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

void PropertiesSerializator::LoadPropertyFromValue(const Property* prop, const AnyData::Value& value, const function<void(span<const uint8>)>& set_data, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());

    const auto& base_type = prop->GetBaseType();

    // Parse value
    if (prop->IsPlainData()) {
        if (base_type.Size > sizeof(size_t)) {
            PropertyRawData struct_data;
            struct_data.Alloc(base_type.Size);
            auto* pdata = struct_data.GetPtrAs<uint8>();

            ConvertFixedValue(prop, base_type, hash_resolver, name_resolver, value, pdata);

            set_data({struct_data.GetPtrAs<uint8>(), base_type.Size});
        }
        else {
            uint8 primitive_data[sizeof(size_t)];
            auto* pdata = primitive_data;

            ConvertFixedValue(prop, base_type, hash_resolver, name_resolver, value, pdata);

            set_data({primitive_data, base_type.Size});
        }
    }
    else if (prop->IsString()) {
        string str_buf;
        const auto& str = ConvertToString(value, str_buf);

        set_data({reinterpret_cast<const uint8*>(str.c_str()), str.length()});
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
            size_t data_size = sizeof(uint32);

            for (const auto& arr_entry : arr) {
                string_view str = ConvertToString(arr_entry, str_buf);
                data_size += sizeof(uint32) + str.length();
            }

            auto data = SafeAlloc::MakeUniqueArr<uint8>(data_size);
            auto* pdata = data.get();

            *reinterpret_cast<uint32*>(pdata) = numeric_cast<uint32>(arr.Size());
            pdata += sizeof(uint32);

            for (const auto& arr_entry : arr) {
                const auto& str = ConvertToString(arr_entry, str_buf);
                *reinterpret_cast<uint32*>(pdata) = numeric_cast<uint32>(str.length());
                pdata += sizeof(uint32);
                MemCopy(pdata, str.c_str(), str.length());
                pdata += str.length();
            }

            FO_RUNTIME_ASSERT(pdata == data.get() + data_size);
            set_data({data.get(), data_size});
        }
        else {
            const size_t data_size = arr.Size() * base_type.Size;
            auto data = SafeAlloc::MakeUniqueArr<uint8>(data_size);
            auto* pdata = data.get();

            for (const auto& arr_entry : arr) {
                ConvertFixedValue(prop, base_type, hash_resolver, name_resolver, arr_entry, pdata);
            }

            FO_RUNTIME_ASSERT(pdata == data.get() + data_size);
            set_data({data.get(), data_size});
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

        const auto& dict_key_type_ = prop->GetDictKeyType();
        const auto& dict_key_type = dict_key_type_.IsSimpleStruct ? dict_key_type_.StructLayout->Fields.front().Type : dict_key_type_;
        string str_buf;

        // Measure data length
        size_t data_size = 0;

        for (auto&& [dict_key, dict_value] : dict) {
            if (dict_key_type.IsString) {
                data_size += sizeof(uint32) + dict_key.length();
            }
            else {
                data_size += dict_key_type.Size;
            }

            if (prop->IsDictOfArray()) {
                if (dict_value.Type() != AnyData::ValueType::Array) {
                    throw PropertySerializationException("Wrong dict array value type", prop->GetName(), dict_value.Type());
                }

                const auto& arr = dict_value.AsArray();

                data_size += sizeof(uint32);

                if (prop->IsDictOfArrayOfString()) {
                    for (const auto& arr_entry : arr) {
                        data_size += sizeof(uint32) + ConvertToString(arr_entry, str_buf).length();
                    }
                }
                else {
                    data_size += arr.Size() * base_type.Size;
                }
            }
            else if (prop->IsDictOfString()) {
                data_size += sizeof(uint32) + ConvertToString(dict_value, str_buf).length();
            }
            else {
                data_size += base_type.Size;
            }
        }

        // Write data
        auto data = SafeAlloc::MakeUniqueArr<uint8>(data_size);
        auto* pdata = data.get();

        for (auto&& [dict_key, dict_value] : dict) {
            // Key
            if (dict_key_type.IsString) {
                const auto key_len = numeric_cast<uint32>(dict_key.length());
                MemCopy(pdata, &key_len, sizeof(key_len));
                pdata += sizeof(key_len);
                MemCopy(pdata, dict_key.c_str(), dict_key.length());
                pdata += dict_key.length();
            }
            else if (dict_key_type.IsHashedString) {
                *reinterpret_cast<hstring::hash_t*>(pdata) = hash_resolver.ToHashedString(dict_key).as_hash();
            }
            else if (dict_key_type.IsEnum) {
                const int32 enum_value = name_resolver.ResolveEnumValue(dict_key_type.Name, dict_key);

                if (dict_key_type.Size == sizeof(uint8)) {
                    *reinterpret_cast<uint8*>(pdata) = numeric_cast<uint8>(enum_value);
                }
                else if (dict_key_type.Size == sizeof(uint16)) {
                    *reinterpret_cast<uint16*>(pdata) = numeric_cast<uint16>(enum_value);
                }
                else {
                    MemCopy(pdata, &enum_value, dict_key_type.Size);
                }
            }
            else if (dict_key_type.IsInt8) {
                *reinterpret_cast<int8*>(pdata) = numeric_cast<int8>(strvex(dict_key).to_int64());
            }
            else if (dict_key_type.IsInt16) {
                *reinterpret_cast<int16*>(pdata) = numeric_cast<int16>(strvex(dict_key).to_int64());
            }
            else if (dict_key_type.IsInt32) {
                *reinterpret_cast<int32*>(pdata) = numeric_cast<int32>(strvex(dict_key).to_int64());
            }
            else if (dict_key_type.IsInt64) {
                *reinterpret_cast<int64*>(pdata) = numeric_cast<int64>(strvex(dict_key).to_int64());
            }
            else if (dict_key_type.IsUInt8) {
                *reinterpret_cast<uint8*>(pdata) = numeric_cast<uint8>(strvex(dict_key).to_int64());
            }
            else if (dict_key_type.IsUInt16) {
                *reinterpret_cast<uint16*>(pdata) = numeric_cast<uint16>(strvex(dict_key).to_int64());
            }
            else if (dict_key_type.IsUInt32) {
                *reinterpret_cast<uint32*>(pdata) = numeric_cast<uint32>(strvex(dict_key).to_int64());
            }
            else if (dict_key_type.IsSingleFloat) {
                *reinterpret_cast<float32*>(pdata) = numeric_cast<float32>(strvex(dict_key).to_float32());
            }
            else if (dict_key_type.IsDoubleFloat) {
                *reinterpret_cast<float64*>(pdata) = numeric_cast<float64>(strvex(dict_key).to_float64());
            }
            else if (dict_key_type.IsBool) {
                *reinterpret_cast<bool*>(pdata) = strvex(dict_key).to_bool();
            }
            else {
                FO_UNREACHABLE_PLACE();
            }

            if (!dict_key_type.IsString) {
                pdata += dict_key_type.Size;
            }

            // Value
            if (prop->IsDictOfArray()) {
                const auto& arr = dict_value.AsArray();

                *reinterpret_cast<uint32*>(pdata) = numeric_cast<uint32>(arr.Size());
                pdata += sizeof(uint32);

                if (prop->IsDictOfArrayOfString()) {
                    for (const auto& arr_entry : arr) {
                        const auto& str = ConvertToString(arr_entry, str_buf);
                        *reinterpret_cast<uint32*>(pdata) = numeric_cast<uint32>(str.length());
                        pdata += sizeof(uint32);
                        MemCopy(pdata, str.c_str(), str.length());
                        pdata += str.length();
                    }
                }
                else {
                    for (const auto& arr_entry : arr) {
                        ConvertFixedValue(prop, base_type, hash_resolver, name_resolver, arr_entry, pdata);
                    }
                }
            }
            else if (prop->IsDictOfString()) {
                const auto& str = ConvertToString(dict_value, str_buf);
                *reinterpret_cast<uint32*>(pdata) = numeric_cast<uint32>(str.length());
                pdata += sizeof(uint32);
                MemCopy(pdata, str.c_str(), str.length());
                pdata += str.length();
            }
            else {
                ConvertFixedValue(prop, base_type, hash_resolver, name_resolver, dict_value, pdata);
            }
        }

        FO_RUNTIME_ASSERT(pdata == data.get() + data_size);
        set_data({data.get(), data_size});
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

void PropertiesSerializator::LoadPropertyFromText(Properties* props, const Property* prop, string_view text, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());

    const auto set_data = [props, prop](span<const uint8> raw_data) { props->SetRawData(prop, raw_data); };
    vector<uint8> data;

    if (prop->IsString()) {
        const auto decoded = StringEscaping::DecodeString(text);
        data.assign(decoded.begin(), decoded.end());
    }
    else if (prop->IsPlainData()) {
        if (prop->IsBaseTypeComplexStruct()) {
            data.reserve(prop->GetBaseSize());
            const string source_text {text};
            const char* s = source_text.c_str();
            string_view token;

            for (const auto& field : prop->GetBaseTypeLayout().Fields) {
                s = ReadTextTokenView(s, token);
                if (s == nullptr) {
                    throw PropertySerializationException("Wrong struct size (from text)");
                }

                AppendBaseTypeFromText(data, prop, field.Type, token, hash_resolver, name_resolver);
            }

            if (ReadTextTokenView(s, token) != nullptr) {
                throw PropertySerializationException("Wrong struct size (from text)");
            }
        }
        else {
            data.reserve(prop->GetBaseSize());
            AppendBaseTypeFromText(data, prop, prop->GetBaseType(), text, hash_resolver, name_resolver);
        }
    }
    else if (prop->IsArray()) {
        data = ParseArrayFromText(prop, prop->GetBaseType(), prop->IsArrayOfString(), text, false, hash_resolver, name_resolver);
    }
    else if (prop->IsDict()) {
        const string source_text {text};
        const char* s = source_text.c_str();
        string_view key_token;
        string_view value_token;
        data.reserve(std::max<size_t>(text.length() * 2, 64));

        while ((s = ReadTextTokenView(s, key_token)) != nullptr && (s = ReadTextTokenView(s, value_token)) != nullptr) {
            AppendBaseTypeFromText(data, prop, prop->GetDictKeyType(), key_token, hash_resolver, name_resolver);

            if (prop->IsDictOfArray()) {
                const auto arr_data = ParseArrayFromText(prop, prop->GetBaseType(), prop->IsDictOfArrayOfString(), value_token, true, hash_resolver, name_resolver);

                if (prop->IsDictOfArrayOfString()) {
                    const auto arr_count = arr_data.empty() ? 0U : *reinterpret_cast<const uint32*>(arr_data.data());
                    AppendRawBytes(data, &arr_count, sizeof(arr_count));

                    if (arr_data.size() > sizeof(uint32)) {
                        AppendRawBytes(data, arr_data.data() + sizeof(uint32), arr_data.size() - sizeof(uint32));
                    }
                }
                else {
                    const auto arr_count = numeric_cast<uint32>(prop->GetBaseSize() == 0 ? 0 : arr_data.size() / prop->GetBaseSize());
                    AppendRawBytes(data, &arr_count, sizeof(arr_count));

                    if (!arr_data.empty()) {
                        AppendRawBytes(data, arr_data.data(), arr_data.size());
                    }
                }
            }
            else {
                AppendBaseTypeFromText(data, prop, prop->GetBaseType(), value_token, hash_resolver, name_resolver);
            }
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    set_data(data);
}

FO_END_NAMESPACE
