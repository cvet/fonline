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
static void ValidateInboundPlainData(const BaseTypeDesc& type, const_span<uint8_t> data, const EngineMetadata& meta);
static void ValidateInboundPackedValue(string_view owner_name, const BaseTypeDesc& type, const_span<uint8_t> data, size_t& offset, const EngineMetadata& meta);
static void ValidateInboundArrayPropertyData(ptr<const Property> prop, const_span<uint8_t> data, const EngineMetadata& meta);
static void ValidateInboundDictPropertyData(ptr<const Property> prop, const_span<uint8_t> data, const EngineMetadata& meta);

template<typename T>
static auto ReadTrivialValue(const_span<uint8_t> data) -> T
{
    FO_STACK_TRACE_ENTRY();

    static_assert(std::is_standard_layout_v<T>);
    static_assert(std::is_trivially_copyable_v<T>);
    FO_VERIFY_AND_THROW(data.size() == sizeof(T), "Trivial value payload size does not match the expected type size");

    T value {};

    if (!data.empty()) {
        ptr<uint8_t> target = ptr<T> {&value}.template reinterpret_as<uint8_t>();
        MemCopy(target, data.data(), sizeof(T));
    }

    return value;
}

static auto ReadPaddedInt32(const_span<uint8_t> data) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(data.size() <= sizeof(int32_t), "Padded int32 payload is wider than a 32-bit integer");

    int32_t value = 0;

    if (!data.empty()) {
        ptr<uint8_t> target = ptr<int32_t> {&value}.reinterpret_as<uint8_t>();
        MemCopy(target, data.data(), data.size());
    }

    return value;
}

// Property raw data places every fixed-size item at its natural alignment with zeroed padding bytes;
// the payload is untrusted network input, so skipped padding is verified to actually be zero
static void SkipAlignmentPadding(string_view owner_name, const_span<uint8_t> data, size_t& offset, size_t alignment)
{
    FO_STACK_TRACE_ENTRY();

    const size_t aligned_offset = align_up(offset, alignment);

    if (aligned_offset > data.size()) {
        throw ClientDataValidationException("Alignment padding extends past the end of property data", owner_name);
    }

    for (size_t i = offset; i < aligned_offset; i++) {
        if (data[i] != 0) {
            throw ClientDataValidationException("Non-zero alignment padding byte in property data", owner_name, i);
        }
    }

    offset = aligned_offset;
}

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

void ValidateInboundPropertyData(ptr<const Property> prop, const_span<uint8_t> data, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    if (prop->IsPlainData()) {
        if (data.size() != prop->GetBaseSize()) {
            throw ClientDataValidationException("Property plain data size mismatch", prop->GetName(), data.size(), prop->GetBaseSize());
        }

        ValidateInboundPlainData(prop->GetBaseType(), data, meta);
        return;
    }

    // Empty payload means default value (empty string/array/dict/ref) — always valid
    if (data.empty()) {
        return;
    }

    const auto& base_type = prop->GetBaseType();

    if (prop->IsString()) {
        const string_view str = span_to_string(data);

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
        (void)reader.ReadBytes(type.Size);
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
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Missing required enum underlying type");
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type is not integer");
        // Supported enum underlying types are uint8/uint16/uint32/int32 — none of them are narrow signed,
        // so MemCopy into a zero-initialized int32 gives the correct numeric value for any size.
        FO_VERIFY_AND_THROW(type.Size <= sizeof(int32_t), "Enum payload is wider than the validation scratch integer", type.Name, type.Size, sizeof(int32_t));

        const int32_t value = ReadPaddedInt32(reader.ReadBytes(type.Size));

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
        const_span<uint8_t> str_data = reader.ReadBytes(str_size);
        const string_view str = span_to_string(str_data);

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
        const_span<uint8_t> ref_raw_data = reader.ReadBytes(raw_size);
        ValidateInboundRefTypeRawData(type.Name, type, ref_raw_data, meta);
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

    FO_VERIFY_AND_THROW(ref_type.IsRefType, "Type is not a reference type");
    FO_VERIFY_AND_THROW(ref_type.RefType, "Missing required reference type descriptor");
    FO_VERIFY_AND_THROW(ref_type.RefType->FieldsRegistrator, "Reference type has no fields registrator");

    auto fields_registrator = ref_type.RefType->FieldsRegistrator.as_ptr();
    size_t offset = 0;

    for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
        auto field_prop = fields_registrator->GetPropertyByIndexUnsafe(i);

        if (offset >= raw_data.size()) {
            // Remaining fields take their default values
            continue;
        }

        SkipAlignmentPadding(owner_name, raw_data, offset, sizeof(uint32_t));

        if (raw_data.size() - offset < sizeof(uint32_t)) {
            throw ClientDataValidationException("Corrupted ref type payload", owner_name, field_prop->GetName());
        }

        const uint32_t field_size = ReadTrivialValue<uint32_t>(raw_data.subspan(offset, sizeof(uint32_t)));
        offset += sizeof(field_size);

        if (field_prop->IsPlainData() && field_size != 0 && field_size != field_prop->GetBaseSize()) {
            throw ClientDataValidationException("Wrong ref field raw size", owner_name, field_prop->GetName(), field_size);
        }

        if (field_size != 0) {
            SkipAlignmentPadding(owner_name, raw_data, offset, field_prop->GetDataAlignment());

            if (raw_data.size() - offset < field_size) {
                throw ClientDataValidationException("Corrupted ref type payload", owner_name, field_prop->GetName());
            }

            ValidateInboundPropertyData(field_prop, raw_data.subspan(offset, field_size), meta);
            offset += field_size;
        }
    }

    if (offset != raw_data.size()) {
        throw ClientDataValidationException("Corrupted ref type payload", owner_name);
    }
}

static void ValidateInboundPackedValue(string_view owner_name, const BaseTypeDesc& type, const_span<uint8_t> data, size_t& offset, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(offset <= data.size(), "Packed value read offset is past the end of the data buffer");

    if (type.IsString) {
        SkipAlignmentPadding(owner_name, data, offset, sizeof(uint32_t));

        if (data.size() - offset < sizeof(uint32_t)) {
            throw ClientDataValidationException("Corrupted string in packed property data", owner_name);
        }

        const uint32_t str_len = ReadTrivialValue<uint32_t>(data.subspan(offset, sizeof(uint32_t)));
        offset += sizeof(str_len);

        if (data.size() - offset < str_len) {
            throw ClientDataValidationException("Corrupted string in packed property data", owner_name);
        }

        const_span<uint8_t> str_data = data.subspan(offset, str_len);
        const string_view str = span_to_string(str_data);

        if (!strvex(str).is_valid_utf8()) {
            throw ClientDataValidationException("String in packed property data is not valid UTF-8", owner_name);
        }
        if (str.find('\0') != string_view::npos) {
            throw ClientDataValidationException("String in packed property data contains NUL character", owner_name);
        }

        offset += str_data.size();
    }
    else if (type.IsRefType) {
        SkipAlignmentPadding(owner_name, data, offset, sizeof(uint32_t));

        if (data.size() - offset < sizeof(uint32_t)) {
            throw ClientDataValidationException("Corrupted ref in packed property data", owner_name);
        }

        const uint32_t ref_size = ReadTrivialValue<uint32_t>(data.subspan(offset, sizeof(uint32_t)));
        offset += sizeof(ref_size);

        if (ref_size != 0) {
            SkipAlignmentPadding(owner_name, data, offset, MAX_SERIALIZED_ALIGNMENT);
        }

        if (data.size() - offset < ref_size) {
            throw ClientDataValidationException("Corrupted ref in packed property data", owner_name);
        }

        ValidateInboundRefTypeRawData(owner_name, type, data.subspan(offset, ref_size), meta);
        offset += ref_size;
    }
    else {
        // Plain primitive or struct — fixed bytes equal to type.Size
        if (type.Size == 0) {
            throw ClientDataValidationException("Zero-sized value in packed property data", owner_name, type.Name);
        }

        SkipAlignmentPadding(owner_name, data, offset, alignment_for_size(type.Size));

        if (data.size() - offset < type.Size) {
            throw ClientDataValidationException("Corrupted value in packed property data", owner_name, type.Name);
        }

        ValidateInboundPlainData(type, data.subspan(offset, type.Size), meta);
        offset += type.Size;
    }
}

static void ValidateInboundArrayPropertyData(ptr<const Property> prop, const_span<uint8_t> data, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    const auto& base_type = prop->GetBaseType();
    uint32_t arr_size = 0;
    size_t offset = 0;

    if (prop->IsArrayOfString() || base_type.IsRefType) {
        if (data.size() - offset < sizeof(uint32_t)) {
            throw ClientDataValidationException("Corrupted array property data", prop->GetName());
        }

        arr_size = ReadTrivialValue<uint32_t>(data.subspan(offset, sizeof(uint32_t)));
        offset += sizeof(arr_size);
    }
    else {
        if (base_type.Size == 0 || data.size() % base_type.Size != 0) {
            throw ClientDataValidationException("Array property data size not aligned to element size", prop->GetName(), data.size(), base_type.Size);
        }

        arr_size = numeric_cast<uint32_t>(data.size() / base_type.Size);
    }

    for (uint32_t i = 0; i < arr_size; i++) {
        ValidateInboundPackedValue(prop->GetName(), base_type, data, offset, meta);
    }

    if (offset != data.size()) {
        throw ClientDataValidationException("Corrupted array property data", prop->GetName());
    }
}

static void ValidateInboundDictPropertyData(ptr<const Property> prop, const_span<uint8_t> data, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    const auto& base_type = prop->GetBaseType();
    const auto& dict_key_type_raw = prop->GetDictKeyType();
    const auto& dict_key_type = dict_key_type_raw.IsSimpleStruct ? dict_key_type_raw.StructLayout->Fields.front().Type : dict_key_type_raw;

    size_t offset = 0;

    while (offset < data.size()) {
        ValidateInboundPackedValue(prop->GetName(), dict_key_type, data, offset, meta);

        if (prop->IsDictOfArray()) {
            SkipAlignmentPadding(prop->GetName(), data, offset, sizeof(uint32_t));

            if (data.size() - offset < sizeof(uint32_t)) {
                throw ClientDataValidationException("Corrupted dict-of-array property data", prop->GetName());
            }

            const uint32_t arr_size = ReadTrivialValue<uint32_t>(data.subspan(offset, sizeof(uint32_t)));
            offset += sizeof(arr_size);

            for (uint32_t i = 0; i < arr_size; i++) {
                ValidateInboundPackedValue(prop->GetName(), base_type, data, offset, meta);
            }
        }
        else {
            ValidateInboundPackedValue(prop->GetName(), base_type, data, offset, meta);
        }
    }
}

static void ValidateInboundPlainData(const BaseTypeDesc& type, const_span<uint8_t> data, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    if (data.size() != type.Size) {
        throw ClientDataValidationException("Plain data size mismatch", type.Name, data.size(), type.Size);
    }

    if (type.IsBool) {
        const uint8_t value = data[0];

        if (value > 1) {
            throw ClientDataValidationException("Invalid bool value", type.Name, value);
        }
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            const float32_t value = ReadTrivialValue<float32_t>(data);

            if (!std::isfinite(value)) {
                throw ClientDataValidationException("Invalid float32 value", type.Name, value);
            }
        }
        else if (type.IsDoubleFloat) {
            const float64_t value = ReadTrivialValue<float64_t>(data);

            if (!std::isfinite(value)) {
                throw ClientDataValidationException("Invalid float64 value", type.Name, value);
            }
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (type.IsEnum) {
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Missing required enum underlying type");
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type is not integer");
        FO_VERIFY_AND_THROW(type.Size <= sizeof(int32_t), "Enum payload is wider than the validation scratch integer", type.Name, type.Size, sizeof(int32_t));

        const int32_t value = ReadPaddedInt32(data);

        bool failed = false;
        const auto hstr = meta.ResolveEnumValueName(type.Name, value, &failed);
        ignore_unused(hstr);

        if (failed) {
            throw ClientDataValidationException("Invalid enum value", type.Name, value);
        }
    }
    else if (type.IsHashedString || type.IsFixedType || type.IsEntityProto) {
        const hstring::hash_t hash = ReadTrivialValue<hstring::hash_t>(data);
        bool failed = false;
        const auto hstr = meta.Hashes.ResolveHash(hash, &failed);

        if (failed) {
            throw ClientDataValidationException("Unknown hashed value", type.Name, hash);
        }

        if ((type.IsFixedType || type.IsEntityProto) && hstr) {
            const auto hname = meta.Hashes.ToHashedString(type.Name);
            auto proto = meta.GetProtoEntity(hname, hstr);

            if (!proto) {
                throw ClientDataValidationException("Unknown proto for type", type.Name, hstr);
            }
        }
    }
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            FO_VERIFY_AND_THROW(field.Offset <= data.size(), "Struct field offset is past the end of the data buffer");
            FO_VERIFY_AND_THROW(data.size() - field.Offset >= field.Type.Size, "Struct field extends past the end of the data buffer");

            ValidateInboundPlainData(field.Type, data.subspan(field.Offset, field.Type.Size), meta);
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
