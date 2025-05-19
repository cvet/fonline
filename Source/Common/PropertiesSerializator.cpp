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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Log.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE();

auto PropertiesSerializator::SaveToDocument(const Properties* props, const Properties* base, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Document
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!base || props->GetRegistrator() == base->GetRegistrator());

    AnyData::Document doc;

    for (const auto* prop : props->GetRegistrator()->GetProperties()) {
        if (prop->IsDisabled()) {
            continue;
        }
        if (prop->IsVirtual()) {
            continue;
        }
        if (prop->IsTemporary()) {
            continue;
        }

        props->ValidateForRawData(prop);

        // Skip same as in base or zero values
        if (base != nullptr) {
            const auto base_raw_data = base->GetRawData(prop);
            const auto raw_data = props->GetRawData(prop);

            if (raw_data.size() == base_raw_data.size() && MemCompare(raw_data.data(), base_raw_data.data(), raw_data.size()) == 0) {
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
            bool is_component;
            const auto* prop = props->GetRegistrator()->FindProperty(doc_key, &is_component);

            if (prop != nullptr && !prop->IsDisabled() && !prop->IsVirtual() && !prop->IsTemporary()) {
                LoadPropertyFromValue(props, prop, doc_value, hash_resolver, name_resolver);
            }
            else {
                // Todo: maybe need some optional warning for unknown/wrong properties
                // WriteLog("Skip unknown property {}", key);
                ignore_unused(is_component);
            }
        }
        catch (const std::exception& ex) {
            WriteLog("Unable to load property {}", doc_key);
            ReportExceptionAndContinue(ex);
            is_error = true;
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    return !is_error;
}

auto PropertiesSerializator::SavePropertyToValue(const Properties* props, const Property* prop, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());
    FO_RUNTIME_ASSERT(!prop->IsTemporary());

    props->ValidateForRawData(prop);

    const auto raw_data = props->GetRawData(prop);

    return SavePropertyToValue(prop, raw_data, hash_resolver, name_resolver);
}

static auto RawDataToValue(const BaseTypeInfo& base_type_info, HashResolver& hash_resolver, NameResolver& name_resolver, const uint8*& pdata) -> AnyData::Value
{
    if (base_type_info.IsString) {
        const uint str_len = *reinterpret_cast<const uint*>(pdata);
        pdata += sizeof(str_len);
        const auto* pstr = reinterpret_cast<const char*>(pdata);
        pdata += str_len;
        return string(pstr, str_len);
    }
    else if (base_type_info.IsHash) {
        const auto hash = *reinterpret_cast<const hstring::hash_t*>(pdata);
        pdata += sizeof(hstring::hash_t);
        return hash_resolver.ResolveHash(hash).as_str();
    }
    else if (base_type_info.IsEnum) {
        int enum_value = 0;
        MemCopy(&enum_value, pdata, base_type_info.Size);
        pdata += base_type_info.Size;
        return name_resolver.ResolveEnumValueName(base_type_info.TypeName, enum_value);
    }
    else if (base_type_info.IsPrimitive) {
        pdata += base_type_info.Size;

        if (base_type_info.IsInt8) {
            return static_cast<int64>(*reinterpret_cast<const int8*>(pdata - base_type_info.Size));
        }
        else if (base_type_info.IsInt16) {
            return static_cast<int64>(*reinterpret_cast<const int16*>(pdata - base_type_info.Size));
        }
        else if (base_type_info.IsInt32) {
            return static_cast<int64>(*reinterpret_cast<const int*>(pdata - base_type_info.Size));
        }
        else if (base_type_info.IsInt64) {
            return static_cast<int64>(*reinterpret_cast<const int64*>(pdata - base_type_info.Size));
        }
        else if (base_type_info.IsUInt8) {
            return static_cast<int64>(*reinterpret_cast<const uint8*>(pdata - base_type_info.Size));
        }
        else if (base_type_info.IsUInt16) {
            return static_cast<int64>(*reinterpret_cast<const uint16*>(pdata - base_type_info.Size));
        }
        else if (base_type_info.IsUInt32) {
            return static_cast<int64>(*reinterpret_cast<const uint*>(pdata - base_type_info.Size));
        }
        else if (base_type_info.IsFloat) {
            return static_cast<double>(*reinterpret_cast<const float*>(pdata - base_type_info.Size));
        }
        else if (base_type_info.IsDoubleFloat) {
            return static_cast<double>(*reinterpret_cast<const double*>(pdata - base_type_info.Size));
        }
        else if (base_type_info.IsBool) {
            return static_cast<bool>(*reinterpret_cast<const bool*>(pdata - base_type_info.Size));
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (base_type_info.IsStruct) {
        const auto& struct_layout = *base_type_info.StructLayout;
        AnyData::Array struct_value;
        struct_value.Reserve(struct_layout.size());

        for (const auto& [field_name, field_type_info] : struct_layout) {
            auto field_value = RawDataToValue(field_type_info, hash_resolver, name_resolver, pdata);
            struct_value.EmplaceBack(std::move(field_value));
        }

        return std::move(struct_value);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

auto PropertiesSerializator::SavePropertyToValue(const Property* prop, const_span<uint8> raw_data, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());
    FO_RUNTIME_ASSERT(!prop->IsTemporary());

    const BaseTypeInfo& base_type_info = prop->GetBaseTypeInfo();
    const auto* pdata = raw_data.data();
    const auto* pdata_end = raw_data.data() + raw_data.size();

    if (prop->IsPlainData()) {
        auto value = RawDataToValue(base_type_info, hash_resolver, name_resolver, pdata);

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
            uint arr_size;

            if (prop->IsArrayOfString()) {
                MemCopy(&arr_size, pdata, sizeof(arr_size));
                pdata += sizeof(uint);
            }
            else {
                arr_size = static_cast<uint>(raw_data.size() / base_type_info.Size);
            }

            arr.Reserve(arr_size);

            for (uint i = 0; i < arr_size; i++) {
                auto arr_entry = RawDataToValue(base_type_info, hash_resolver, name_resolver, pdata);
                arr.EmplaceBack(std::move(arr_entry));
            }
        }

        FO_RUNTIME_ASSERT(pdata == pdata_end);
        return std::move(arr);
    }
    else if (prop->IsDict()) {
        AnyData::Dict dict;

        if (!raw_data.empty()) {
            const auto& dict_key_type_info = prop->GetDictKeyTypeInfo();

            const auto get_key_string = [&dict_key_type_info, &hash_resolver, &name_resolver](const uint8* p) -> string {
                if (dict_key_type_info.IsString) {
                    const uint str_len = *reinterpret_cast<const uint*>(p);
                    return string {reinterpret_cast<const char*>(p + sizeof(uint)), str_len};
                }
                else if (dict_key_type_info.IsHash) {
                    const auto hash = *reinterpret_cast<const hstring::hash_t*>(p);
                    return hash_resolver.ResolveHash(hash).as_str();
                }
                else if (dict_key_type_info.IsEnum) {
                    int enum_value = 0;
                    MemCopy(&enum_value, p, dict_key_type_info.Size);
                    return name_resolver.ResolveEnumValueName(dict_key_type_info.TypeName, enum_value);
                }
                else if (dict_key_type_info.IsInt8) {
                    return strex("{}", *reinterpret_cast<const int8*>(p));
                }
                else if (dict_key_type_info.IsInt16) {
                    return strex("{}", *reinterpret_cast<const int16*>(p));
                }
                else if (dict_key_type_info.IsInt32) {
                    return strex("{}", *reinterpret_cast<const int*>(p));
                }
                else if (dict_key_type_info.IsInt64) {
                    return strex("{}", *reinterpret_cast<const int64*>(p));
                }
                else if (dict_key_type_info.IsUInt8) {
                    return strex("{}", *reinterpret_cast<const uint8*>(p));
                }
                else if (dict_key_type_info.IsUInt16) {
                    return strex("{}", *reinterpret_cast<const uint16*>(p));
                }
                else if (dict_key_type_info.IsUInt32) {
                    return strex("{}", *reinterpret_cast<const uint*>(p));
                }
                else if (dict_key_type_info.IsSingleFloat) {
                    return strex("{}", *reinterpret_cast<const float*>(p));
                }
                else if (dict_key_type_info.IsDoubleFloat) {
                    return strex("{}", *reinterpret_cast<const double*>(p));
                }
                else if (dict_key_type_info.IsBool) {
                    return strex("{}", *reinterpret_cast<const bool*>(p));
                }
                else {
                    FO_UNREACHABLE_PLACE();
                }
            };

            const auto get_key_len = [&dict_key_type_info](const uint8* p) -> size_t {
                if (dict_key_type_info.IsString) {
                    const uint str_len = *reinterpret_cast<const uint*>(p);
                    return sizeof(uint) + str_len;
                }
                else {
                    return dict_key_type_info.Size;
                }
            };

            if (prop->IsDictOfArray()) {
                while (pdata < pdata_end) {
                    const auto* key_data = pdata;
                    pdata += get_key_len(key_data);
                    string key_str = get_key_string(key_data);

                    uint arr_size;
                    MemCopy(&arr_size, pdata, sizeof(arr_size));
                    pdata += sizeof(uint);

                    AnyData::Array arr;
                    arr.Reserve(arr_size);

                    for (uint i = 0; i < arr_size; i++) {
                        auto arr_entry = RawDataToValue(base_type_info, hash_resolver, name_resolver, pdata);
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

                    auto dict_value = RawDataToValue(base_type_info, hash_resolver, name_resolver, pdata);
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
    FO_RUNTIME_ASSERT(!prop->IsDisabled());

    const auto set_data = [props, prop](const_span<uint8> raw_data) { props->SetRawData(prop, raw_data); };

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
    case AnyData::ValueType::Double:
        return buf = strex("{}", value.AsDouble());
    case AnyData::ValueType::Bool:
        return buf = strex("{}", value.AsBool());
    default:
        throw PropertySerializationException("Unable to convert not string, int, float or bool value to string", value.Type());
    }
};

template<typename T>
static void ConvertToNumber(const AnyData::Value& value, T& result_value)
{
    FO_STACK_TRACE_ENTRY();

    if (value.Type() == AnyData::ValueType::Int64) {
        result_value = numeric_cast<T>(value.AsInt64());
    }
    else if (value.Type() == AnyData::ValueType::Double) {
        result_value = numeric_cast<T>(value.AsDouble());
    }
    else if (value.Type() == AnyData::ValueType::Bool) {
        result_value = numeric_cast<T>(value.AsBool());
    }
    else if (value.Type() == AnyData::ValueType::String) {
        const auto& str = value.AsString();

        if (strex(str).isNumber()) {
            if constexpr (std::is_integral_v<T>) {
                result_value = numeric_cast<T>(strex(str).toInt64());
            }
            else if constexpr (std::is_floating_point_v<T>) {
                result_value = numeric_cast<T>(strex(str).toDouble());
            }
            else if constexpr (std::is_same_v<T, bool>) {
                result_value = numeric_cast<T>(strex(str).toBool());
            }
            else {
                FO_UNREACHABLE_PLACE();
            }
        }
        else if (strex(str).isExplicitBool()) {
            result_value = numeric_cast<T>(strex(str).toBool());
        }
        else {
            throw PropertySerializationException("Uncable to convert value to number", str);
        }
    }
    else {
        throw PropertySerializationException("Wrong value type (not string, int, float or bool)", value.Type());
    }
}

static void ConvertFixedValue(const BaseTypeInfo& base_type_info, HashResolver& hash_resolver, NameResolver& name_resolver, const AnyData::Value& value, uint8*& pdata)
{
    FO_STACK_TRACE_ENTRY();

    if (base_type_info.IsHash) {
        auto& hash = *reinterpret_cast<hstring::hash_t*>(pdata);

        if (value.Type() == AnyData::ValueType::String) {
            hash = hash_resolver.ToHashedString(value.AsString()).as_hash();
        }
        else {
            throw PropertySerializationException("Wrong hash value type");
        }
    }
    else if (base_type_info.IsEnum) {
        int enum_value = 0;

        if (value.Type() == AnyData::ValueType::String) {
            enum_value = name_resolver.ResolveEnumValue(base_type_info.TypeName, value.AsString());
        }
        else if (value.Type() == AnyData::ValueType::Int64) {
            enum_value = numeric_cast<int>(value.AsInt64());
            const auto& enum_value_name = name_resolver.ResolveEnumValueName(base_type_info.TypeName, enum_value);
            ignore_unused(enum_value_name);
        }
        else {
            throw PropertySerializationException("Wrong enum value type (not string or int)");
        }

        if (base_type_info.Size == sizeof(uint8)) {
            *reinterpret_cast<uint8*>(pdata) = numeric_cast<uint8>(enum_value);
        }
        else if (base_type_info.Size == sizeof(uint16)) {
            *reinterpret_cast<uint16*>(pdata) = numeric_cast<uint16>(enum_value);
        }
        else {
            MemCopy(pdata, &enum_value, base_type_info.Size);
        }
    }
    else if (base_type_info.IsPrimitive) {
        if (base_type_info.IsInt8) {
            ConvertToNumber(value, *reinterpret_cast<int8*>(pdata));
        }
        else if (base_type_info.IsInt16) {
            ConvertToNumber(value, *reinterpret_cast<int16*>(pdata));
        }
        else if (base_type_info.IsInt32) {
            ConvertToNumber(value, *reinterpret_cast<int*>(pdata));
        }
        else if (base_type_info.IsInt64) {
            ConvertToNumber(value, *reinterpret_cast<int64*>(pdata));
        }
        else if (base_type_info.IsUInt8) {
            ConvertToNumber(value, *reinterpret_cast<uint8*>(pdata));
        }
        else if (base_type_info.IsUInt16) {
            ConvertToNumber(value, *reinterpret_cast<uint16*>(pdata));
        }
        else if (base_type_info.IsUInt32) {
            ConvertToNumber(value, *reinterpret_cast<uint*>(pdata));
        }
        else if (base_type_info.IsSingleFloat) {
            ConvertToNumber(value, *reinterpret_cast<float*>(pdata));
        }
        else if (base_type_info.IsDoubleFloat) {
            ConvertToNumber(value, *reinterpret_cast<double*>(pdata));
        }
        else if (base_type_info.IsBool) {
            ConvertToNumber(value, *reinterpret_cast<bool*>(pdata));
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (base_type_info.IsStruct) {
        if (value.Type() == AnyData::ValueType::Array) {
            const auto& struct_value = value.AsArray();
            const auto& struct_layout = *base_type_info.StructLayout;

            if (struct_value.Size() != struct_layout.size()) {
                throw PropertySerializationException("Wrong struct size", struct_value.Size(), struct_layout.size());
            }

            for (size_t i = 0; i < struct_layout.size(); i++) {
                const auto& [field_name, field_type_info] = struct_layout[i];
                const auto& field_value = struct_value[i];
                ConvertFixedValue(field_type_info, hash_resolver, name_resolver, field_value, pdata);
            }
        }
        else {
            throw PropertySerializationException("Wrong struct value type", value.Type());
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    pdata += base_type_info.Size;
}

void PropertiesSerializator::LoadPropertyFromValue(const Property* prop, const AnyData::Value& value, const std::function<void(const_span<uint8>)>& set_data, HashResolver& hash_resolver, NameResolver& name_resolver)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(!prop->IsVirtual());
    FO_RUNTIME_ASSERT(!prop->IsDisabled());

    const auto& base_type_info = prop->GetBaseTypeInfo();

    // Parse value
    if (prop->IsPlainData()) {
        if (base_type_info.IsStruct) {
            PropertyRawData struct_data;
            struct_data.Alloc(base_type_info.Size);
            auto* pdata = struct_data.GetPtrAs<uint8>();

            ConvertFixedValue(base_type_info, hash_resolver, name_resolver, value, pdata);

            set_data({struct_data.GetPtrAs<uint8>(), base_type_info.Size});
        }
        else {
            FO_RUNTIME_ASSERT(base_type_info.Size <= sizeof(size_t));
            uint8 primitive_data[sizeof(size_t)];
            auto* pdata = primitive_data;

            ConvertFixedValue(base_type_info, hash_resolver, name_resolver, value, pdata);

            set_data({primitive_data, base_type_info.Size});
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
            size_t data_size = sizeof(uint);

            for (const auto& arr_entry : arr) {
                string_view str = ConvertToString(arr_entry, str_buf);
                data_size += sizeof(uint) + str.length();
            }

            auto data = SafeAlloc::MakeUniqueArr<uint8>(data_size);
            auto* pdata = data.get();

            *reinterpret_cast<uint*>(pdata) = static_cast<uint>(arr.Size());
            pdata += sizeof(uint);

            for (const auto& arr_entry : arr) {
                const auto& str = ConvertToString(arr_entry, str_buf);
                *reinterpret_cast<uint*>(pdata) = static_cast<uint>(str.length());
                pdata += sizeof(uint);
                MemCopy(pdata, str.c_str(), str.length());
                pdata += str.length();
            }

            FO_RUNTIME_ASSERT(pdata == data.get() + data_size);
            set_data({data.get(), data_size});
        }
        else {
            const size_t data_size = arr.Size() * base_type_info.Size;
            auto data = SafeAlloc::MakeUniqueArr<uint8>(data_size);
            auto* pdata = data.get();

            for (const auto& arr_entry : arr) {
                ConvertFixedValue(base_type_info, hash_resolver, name_resolver, arr_entry, pdata);
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

        const auto& dickt_key_type_info = prop->GetDictKeyTypeInfo();
        string str_buf;

        // Measure data length
        size_t data_size = 0;

        for (auto&& [dict_key, dict_value] : dict) {
            if (dickt_key_type_info.IsString) {
                data_size += sizeof(uint) + dict_key.length();
            }
            else {
                data_size += dickt_key_type_info.Size;
            }

            if (prop->IsDictOfArray()) {
                if (dict_value.Type() != AnyData::ValueType::Array) {
                    throw PropertySerializationException("Wrong dict array value type", prop->GetName(), dict_value.Type());
                }

                const auto& arr = dict_value.AsArray();

                data_size += sizeof(uint);

                if (prop->IsDictOfArrayOfString()) {
                    for (const auto& arr_entry : arr) {
                        data_size += sizeof(uint) + ConvertToString(arr_entry, str_buf).length();
                    }
                }
                else {
                    data_size += arr.Size() * base_type_info.Size;
                }
            }
            else if (prop->IsDictOfString()) {
                data_size += ConvertToString(dict_value, str_buf).length();
            }
            else {
                data_size += base_type_info.Size;
            }
        }

        // Write data
        auto data = SafeAlloc::MakeUniqueArr<uint8>(data_size);
        auto* pdata = data.get();

        for (auto&& [dict_key, dict_value] : dict) {
            // Key
            if (dickt_key_type_info.IsString) {
                const uint key_len = static_cast<uint>(dict_key.length());
                MemCopy(pdata, &key_len, sizeof(key_len));
                pdata += sizeof(key_len);
                MemCopy(pdata, dict_key.c_str(), dict_key.length());
                pdata += dict_key.length();
            }
            else if (dickt_key_type_info.IsHash) {
                *reinterpret_cast<hstring::hash_t*>(pdata) = hash_resolver.ToHashedString(dict_key).as_hash();
            }
            else if (dickt_key_type_info.IsEnum) {
                const int enum_value = name_resolver.ResolveEnumValue(dickt_key_type_info.TypeName, dict_key);

                if (dickt_key_type_info.Size == sizeof(uint8)) {
                    *reinterpret_cast<uint8*>(pdata) = numeric_cast<uint8>(enum_value);
                }
                else if (dickt_key_type_info.Size == sizeof(uint16)) {
                    *reinterpret_cast<uint16*>(pdata) = numeric_cast<uint16>(enum_value);
                }
                else {
                    MemCopy(pdata, &enum_value, base_type_info.Size);
                }
            }
            else if (dickt_key_type_info.IsInt8) {
                *reinterpret_cast<int8*>(pdata) = numeric_cast<int8>(strex(dict_key).toInt64());
            }
            else if (dickt_key_type_info.IsInt16) {
                *reinterpret_cast<int16*>(pdata) = numeric_cast<int16>(strex(dict_key).toInt64());
            }
            else if (dickt_key_type_info.IsInt32) {
                *reinterpret_cast<int*>(pdata) = numeric_cast<int>(strex(dict_key).toInt64());
            }
            else if (dickt_key_type_info.IsInt64) {
                *reinterpret_cast<int64*>(pdata) = numeric_cast<int64>(strex(dict_key).toInt64());
            }
            else if (dickt_key_type_info.IsUInt8) {
                *reinterpret_cast<uint8*>(pdata) = numeric_cast<uint8>(strex(dict_key).toInt64());
            }
            else if (dickt_key_type_info.IsUInt16) {
                *reinterpret_cast<uint16*>(pdata) = numeric_cast<uint16>(strex(dict_key).toInt64());
            }
            else if (dickt_key_type_info.IsUInt32) {
                *reinterpret_cast<uint*>(pdata) = numeric_cast<uint>(strex(dict_key).toInt64());
            }
            else if (dickt_key_type_info.IsSingleFloat) {
                *reinterpret_cast<float*>(pdata) = numeric_cast<float>(strex(dict_key).toFloat());
            }
            else if (dickt_key_type_info.IsDoubleFloat) {
                *reinterpret_cast<double*>(pdata) = numeric_cast<double>(strex(dict_key).toDouble());
            }
            else if (dickt_key_type_info.IsBool) {
                *reinterpret_cast<bool*>(pdata) = numeric_cast<bool>(strex(dict_key).toBool());
            }
            else {
                FO_UNREACHABLE_PLACE();
            }

            if (!dickt_key_type_info.IsString) {
                pdata += dickt_key_type_info.Size;
            }

            // Value
            if (prop->IsDictOfArray()) {
                const auto& arr = dict_value.AsArray();

                *reinterpret_cast<uint*>(pdata) = static_cast<uint>(arr.Size());
                pdata += sizeof(uint);

                if (prop->IsDictOfArrayOfString()) {
                    for (const auto& arr_entry : arr) {
                        const auto& str = ConvertToString(arr_entry, str_buf);
                        *reinterpret_cast<uint*>(pdata) = static_cast<uint>(str.length());
                        pdata += sizeof(uint);
                        MemCopy(pdata, str.c_str(), str.length());
                        pdata += str.length();
                    }
                }
                else {
                    for (const auto& arr_entry : arr) {
                        ConvertFixedValue(base_type_info, hash_resolver, name_resolver, arr_entry, pdata);
                    }
                }
            }
            else if (prop->IsDictOfString()) {
                const auto& str = ConvertToString(dict_value, str_buf);
                *reinterpret_cast<uint*>(pdata) = static_cast<uint>(str.length());
                pdata += sizeof(uint);
                MemCopy(pdata, str.c_str(), str.length());
                pdata += str.length();
            }
            else {
                ConvertFixedValue(base_type_info, hash_resolver, name_resolver, dict_value, pdata);
            }
        }

        FO_RUNTIME_ASSERT(pdata == data.get() + data_size);
        set_data({data.get(), data_size});
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

FO_END_NAMESPACE();
