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

#include "ClientDataValidation.h"
#include "DataSerialization.h"
#include "EngineBase.h"
#include "Properties.h"

FO_BEGIN_NAMESPACE

static void ValidateInboundRemoteCallArgData(const ComplexTypeDesc& type, DataReader& reader, const EngineMetadata& meta);
static void ValidateInboundSimpleRemoteCallData(const BaseTypeDesc& type, DataReader& reader, const EngineMetadata& meta);
static void ValidateInboundRefTypeRawData(string_view owner_name, const BaseTypeDesc& ref_type, span<const uint8_t> raw_data, const EngineMetadata& meta);
static void ValidateInboundPlainData(const BaseTypeDesc& type, const uint8_t* data, size_t data_size, const EngineMetadata& meta);
static void ValidateInboundPackedValue(string_view owner_name, const BaseTypeDesc& type, const uint8_t*& pdata, const uint8_t* pdata_end, const EngineMetadata& meta);
static void ValidateInboundArrayPropertyData(const Property* prop, const_span<uint8_t> data, const EngineMetadata& meta);
static void ValidateInboundDictPropertyData(const Property* prop, const_span<uint8_t> data, const EngineMetadata& meta);

void ValidateInboundRemoteCallData(const RemoteCallDesc& inbound_call, const_span<uint8_t> data, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    try {
        DataReader reader(data);

        for (const auto& arg : inbound_call.Args) {
            ValidateInboundRemoteCallArgData(arg.Type, reader, meta);
        }

        reader.VerifyEnd();
    }
    catch (const ClientDataValidationException&) {
        throw;
    }
    catch (const DataReadingException& ex) {
        throw ClientDataValidationException("Malformed remote call payload", inbound_call.Name, ex.what());
    }
    catch (const std::exception& ex) {
        throw ClientDataValidationException("Invalid remote call data", inbound_call.Name, ex.what());
    }
}

void ValidateInboundPropertyData(const Property* prop, const_span<uint8_t> data, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    if (prop->IsPlainData()) {
        if (data.size() != prop->GetBaseSize()) {
            throw ClientDataValidationException("Property plain data size mismatch", prop->GetName(), data.size(), prop->GetBaseSize());
        }

        ValidateInboundPlainData(prop->GetBaseType(), data.data(), data.size(), meta);
        return;
    }

    // Empty payload means default value (empty string/array/dict/ref) — always valid
    if (data.empty()) {
        return;
    }

    const auto& base_type = prop->GetBaseType();

    if (prop->IsString()) {
        const string_view str {reinterpret_cast<const char*>(data.data()), data.size()};

        if (!strvex(str).is_valid_utf8()) {
            throw ClientDataValidationException("Property string is not valid UTF-8", prop->GetName());
        }
        if (str.find('\0') != string_view::npos) {
            throw ClientDataValidationException("Property string contains NUL character", prop->GetName());
        }

        return;
    }

    if (base_type.IsRefType && !prop->IsArray() && !prop->IsDict()) {
        ValidateInboundRefTypeRawData(prop->GetName(), base_type, data, meta);
        return;
    }

    if (prop->IsArray()) {
        ValidateInboundArrayPropertyData(prop, data, meta);
        return;
    }

    if (prop->IsDict()) {
        ValidateInboundDictPropertyData(prop, data, meta);
        return;
    }

    throw ClientDataValidationException("Unsupported property layout", prop->GetName());
}

static void ValidateInboundRemoteCallArgData(const ComplexTypeDesc& type, DataReader& reader, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    if (type.Kind == ComplexTypeKind::Simple) {
        ValidateInboundSimpleRemoteCallData(type.BaseType, reader, meta);
    }
    else if (type.Kind == ComplexTypeKind::Array) {
        const int32_t arr_size = reader.Read<int32_t>();

        if (arr_size < 0) {
            throw ClientDataValidationException("Negative array size", type.BaseType.Name, arr_size);
        }

        for (int32_t i = 0; i < arr_size; i++) {
            ValidateInboundSimpleRemoteCallData(type.BaseType, reader, meta);
        }
    }
    else if (type.Kind == ComplexTypeKind::Dict) {
        const int32_t dict_size = reader.Read<int32_t>();

        if (dict_size < 0) {
            throw ClientDataValidationException("Negative dict size", type.BaseType.Name, dict_size);
        }

        for (int32_t i = 0; i < dict_size; i++) {
            ValidateInboundSimpleRemoteCallData(type.KeyType.value(), reader, meta);
            ValidateInboundSimpleRemoteCallData(type.BaseType, reader, meta);
        }
    }
    else if (type.Kind == ComplexTypeKind::DictOfArray) {
        const int32_t dict_size = reader.Read<int32_t>();

        if (dict_size < 0) {
            throw ClientDataValidationException("Negative dict size", type.BaseType.Name, dict_size);
        }

        for (int32_t i = 0; i < dict_size; i++) {
            ValidateInboundSimpleRemoteCallData(type.KeyType.value(), reader, meta);

            const int32_t arr_size = reader.Read<int32_t>();

            if (arr_size < 0) {
                throw ClientDataValidationException("Negative array size", type.BaseType.Name, arr_size);
            }

            for (int32_t j = 0; j < arr_size; j++) {
                ValidateInboundSimpleRemoteCallData(type.BaseType, reader, meta);
            }
        }
    }
    else {
        const int32_t type_kind = static_cast<int32_t>(type.Kind);
        throw ClientDataValidationException("Unsupported remote call container type", type_kind);
    }
}

static void ValidateInboundSimpleRemoteCallData(const BaseTypeDesc& type, DataReader& reader, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsBool) {
        const uint8_t value = reader.Read<uint8_t>();

        if (value > 1) {
            throw ClientDataValidationException("Invalid bool value", type.Name, value);
        }
    }
    else if (type.IsInt) {
        reader.ReadPtr<uint8_t>(type.Size);
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            const float32_t value = reader.Read<float32_t>();

            if (!std::isfinite(value)) {
                throw ClientDataValidationException("Invalid float32 value", type.Name, value);
            }
        }
        else if (type.IsDoubleFloat) {
            const float64_t value = reader.Read<float64_t>();

            if (!std::isfinite(value)) {
                throw ClientDataValidationException("Invalid float64 value", type.Name, value);
            }
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (type.IsEnum) {
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Missing required type enum underlying type");
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type is not integer");
        // Supported enum underlying types are uint8/uint16/uint32/int32 — none of them are narrow signed,
        // so MemCopy into a zero-initialized int32 gives the correct numeric value for any size.
        FO_VERIFY_AND_THROW(type.Size <= sizeof(int32_t), "Enum payload is wider than the validation scratch integer", type.Name, type.Size, sizeof(int32_t));

        int32_t value = 0;
        MemCopy(&value, reader.ReadPtr<uint8_t>(type.Size), type.Size);

        bool failed = false;
        const auto hstr = meta.ResolveEnumValueName(type.Name, value, &failed);
        ignore_unused(hstr);

        if (failed) {
            throw ClientDataValidationException("Invalid enum value", type.Name, value);
        }
    }
    else if (type.IsString) {
        const int32_t str_len = reader.Read<int32_t>();

        if (str_len < 0) {
            throw ClientDataValidationException("Negative string length", type.Name, str_len);
        }

        const size_t str_size = numeric_cast<size_t>(str_len);
        const string_view str {reader.ReadPtr<char>(str_size), str_size};

        if (!strvex(str).is_valid_utf8()) {
            throw ClientDataValidationException("String is not valid UTF-8", type.Name);
        }
        if (str.find('\0') != string_view::npos) {
            throw ClientDataValidationException("String contains NUL character", type.Name);
        }
    }
    else if (type.IsHashedString) {
        const hstring::hash_t hash = reader.Read<hstring::hash_t>();
        bool failed = false;
        const auto hstr = meta.Hashes.ResolveHash(hash, &failed);
        ignore_unused(hstr);

        if (failed) {
            throw ClientDataValidationException("Unknown hashed string value", type.Name, hash);
        }
    }
    else if (type.IsRefType) {
        const uint32_t raw_size = reader.Read<uint32_t>();
        const auto* raw_data = reader.ReadPtr<uint8_t>(raw_size);
        ValidateInboundRefTypeRawData(type.Name, type, {raw_data, raw_size}, meta);
    }
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            ValidateInboundSimpleRemoteCallData(field.Type, reader, meta);
        }
    }
    else {
        throw ClientDataValidationException("Unsupported remote call argument type", type.Name);
    }
}

static void ValidateInboundRefTypeRawData(string_view owner_name, const BaseTypeDesc& ref_type, span<const uint8_t> raw_data, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ref_type.IsRefType, "Missing required reference type is ref type");
    FO_VERIFY_AND_THROW(ref_type.RefType, "Missing required reference type ref type");
    FO_VERIFY_AND_THROW(ref_type.RefType->FieldsRegistrator, "Reference type has no fields registrator");

    const auto* fields_registrator = ref_type.RefType->FieldsRegistrator.get();
    const auto* pdata = raw_data.data();
    const auto* pdata_end = raw_data.data() + raw_data.size();

    for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
        const auto* field_prop = fields_registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));

        if (pdata >= pdata_end) {
            // Remaining fields take their default values
            continue;
        }

        if (numeric_cast<size_t>(pdata_end - pdata) < sizeof(uint32_t)) {
            throw ClientDataValidationException("Corrupted ref type payload", owner_name, field_prop->GetName());
        }

        uint32_t field_size;
        MemCopy(&field_size, pdata, sizeof(field_size));
        pdata += sizeof(field_size);

        if (field_prop->IsPlainData() && field_size != 0 && field_size != field_prop->GetBaseSize()) {
            throw ClientDataValidationException("Wrong ref field raw size", owner_name, field_prop->GetName(), field_size);
        }
        if (numeric_cast<size_t>(pdata_end - pdata) < field_size) {
            throw ClientDataValidationException("Corrupted ref type payload", owner_name, field_prop->GetName());
        }

        if (field_size > 0) {
            ValidateInboundPropertyData(field_prop, {pdata, field_size}, meta);
        }
        pdata += field_size;
    }

    if (pdata != pdata_end) {
        throw ClientDataValidationException("Corrupted ref type payload", owner_name);
    }
}

static void ValidateInboundPackedValue(string_view owner_name, const BaseTypeDesc& type, const uint8_t*& pdata, const uint8_t* pdata_end, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsString) {
        if (numeric_cast<size_t>(pdata_end - pdata) < sizeof(uint32_t)) {
            throw ClientDataValidationException("Corrupted string in packed property data", owner_name);
        }

        uint32_t str_len;
        MemCopy(&str_len, pdata, sizeof(str_len));
        pdata += sizeof(str_len);

        if (numeric_cast<size_t>(pdata_end - pdata) < str_len) {
            throw ClientDataValidationException("Corrupted string in packed property data", owner_name);
        }

        const string_view str {reinterpret_cast<const char*>(pdata), str_len};

        if (!strvex(str).is_valid_utf8()) {
            throw ClientDataValidationException("String in packed property data is not valid UTF-8", owner_name);
        }
        if (str.find('\0') != string_view::npos) {
            throw ClientDataValidationException("String in packed property data contains NUL character", owner_name);
        }

        pdata += str_len;
    }
    else if (type.IsRefType) {
        if (numeric_cast<size_t>(pdata_end - pdata) < sizeof(uint32_t)) {
            throw ClientDataValidationException("Corrupted ref in packed property data", owner_name);
        }

        uint32_t ref_size;
        MemCopy(&ref_size, pdata, sizeof(ref_size));
        pdata += sizeof(ref_size);

        if (numeric_cast<size_t>(pdata_end - pdata) < ref_size) {
            throw ClientDataValidationException("Corrupted ref in packed property data", owner_name);
        }

        ValidateInboundRefTypeRawData(owner_name, type, {pdata, ref_size}, meta);
        pdata += ref_size;
    }
    else {
        // Plain primitive or struct — fixed bytes equal to type.Size
        if (type.Size == 0) {
            throw ClientDataValidationException("Zero-sized value in packed property data", owner_name, type.Name);
        }
        if (numeric_cast<size_t>(pdata_end - pdata) < type.Size) {
            throw ClientDataValidationException("Corrupted value in packed property data", owner_name, type.Name);
        }

        ValidateInboundPlainData(type, pdata, type.Size, meta);
        pdata += type.Size;
    }
}

static void ValidateInboundArrayPropertyData(const Property* prop, const_span<uint8_t> data, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    const auto& base_type = prop->GetBaseType();
    const auto* pdata = data.data();
    const auto* pdata_end = data.data() + data.size();

    uint32_t arr_size;

    if (prop->IsArrayOfString() || base_type.IsRefType) {
        if (numeric_cast<size_t>(pdata_end - pdata) < sizeof(uint32_t)) {
            throw ClientDataValidationException("Corrupted array property data", prop->GetName());
        }

        MemCopy(&arr_size, pdata, sizeof(arr_size));
        pdata += sizeof(arr_size);
    }
    else {
        if (base_type.Size == 0 || data.size() % base_type.Size != 0) {
            throw ClientDataValidationException("Array property data size not aligned to element size", prop->GetName(), data.size(), base_type.Size);
        }

        arr_size = numeric_cast<uint32_t>(data.size() / base_type.Size);
    }

    for (uint32_t i = 0; i < arr_size; i++) {
        ValidateInboundPackedValue(prop->GetName(), base_type, pdata, pdata_end, meta);
    }

    if (pdata != pdata_end) {
        throw ClientDataValidationException("Corrupted array property data", prop->GetName());
    }
}

static void ValidateInboundDictPropertyData(const Property* prop, const_span<uint8_t> data, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    const auto& base_type = prop->GetBaseType();
    const auto& dict_key_type_raw = prop->GetDictKeyType();
    const auto& dict_key_type = dict_key_type_raw.IsSimpleStruct ? dict_key_type_raw.StructLayout->Fields.front().Type : dict_key_type_raw;

    const auto* pdata = data.data();
    const auto* pdata_end = data.data() + data.size();

    while (pdata < pdata_end) {
        ValidateInboundPackedValue(prop->GetName(), dict_key_type, pdata, pdata_end, meta);

        if (prop->IsDictOfArray()) {
            if (numeric_cast<size_t>(pdata_end - pdata) < sizeof(uint32_t)) {
                throw ClientDataValidationException("Corrupted dict-of-array property data", prop->GetName());
            }

            uint32_t arr_size;
            MemCopy(&arr_size, pdata, sizeof(arr_size));
            pdata += sizeof(arr_size);

            for (uint32_t i = 0; i < arr_size; i++) {
                ValidateInboundPackedValue(prop->GetName(), base_type, pdata, pdata_end, meta);
            }
        }
        else {
            ValidateInboundPackedValue(prop->GetName(), base_type, pdata, pdata_end, meta);
        }
    }
}

static void ValidateInboundPlainData(const BaseTypeDesc& type, const uint8_t* data, size_t data_size, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    if (data_size != type.Size) {
        throw ClientDataValidationException("Plain data size mismatch", type.Name, data_size, type.Size);
    }

    if (type.IsBool) {
        const uint8_t value = data[0];

        if (value > 1) {
            throw ClientDataValidationException("Invalid bool value", type.Name, value);
        }
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            float32_t value;
            MemCopy(&value, data, sizeof(value));

            if (!std::isfinite(value)) {
                throw ClientDataValidationException("Invalid float32 value", type.Name, value);
            }
        }
        else if (type.IsDoubleFloat) {
            float64_t value;
            MemCopy(&value, data, sizeof(value));

            if (!std::isfinite(value)) {
                throw ClientDataValidationException("Invalid float64 value", type.Name, value);
            }
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (type.IsEnum) {
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Missing required type enum underlying type");
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type is not integer");
        FO_VERIFY_AND_THROW(type.Size <= sizeof(int32_t), "Enum payload is wider than the validation scratch integer", type.Name, type.Size, sizeof(int32_t));

        int32_t value = 0;
        MemCopy(&value, data, type.Size);

        bool failed = false;
        const auto hstr = meta.ResolveEnumValueName(type.Name, value, &failed);
        ignore_unused(hstr);

        if (failed) {
            throw ClientDataValidationException("Invalid enum value", type.Name, value);
        }
    }
    else if (type.IsHashedString || type.IsFixedType || type.IsEntityProto) {
        hstring::hash_t hash;
        MemCopy(&hash, data, sizeof(hash));
        bool failed = false;
        const auto hstr = meta.Hashes.ResolveHash(hash, &failed);

        if (failed) {
            throw ClientDataValidationException("Unknown hashed value", type.Name, hash);
        }

        if ((type.IsFixedType || type.IsEntityProto) && hstr) {
            const auto hname = meta.Hashes.ToHashedString(type.Name);
            const auto* proto = meta.GetProtoEntity(hname, hstr);

            if (proto == nullptr) {
                throw ClientDataValidationException("Unknown proto for type", type.Name, hstr);
            }
        }
    }
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            ValidateInboundPlainData(field.Type, data + field.Offset, field.Type.Size, meta);
        }
    }
    else if (type.IsInt) {
        // Any int value is valid
    }
    else {
        throw ClientDataValidationException("Unsupported plain data type", type.Name);
    }
}

FO_END_NAMESPACE
