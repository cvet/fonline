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

#include "RemoteCallValidation.h"
#include "DataSerialization.h"
#include "EngineBase.h"
#include "PropertiesSerializator.h"

FO_BEGIN_NAMESPACE

static void ValidateInboundRemoteCallArgData(const ComplexTypeDesc& type, DataReader& reader, const EngineMetadata& meta);
static void ValidateInboundSimpleRemoteCallData(const BaseTypeDesc& type, DataReader& reader, const EngineMetadata& meta);
static void ValidateInboundRefTypeRawData(const BaseTypeDesc& type, span<const uint8_t> raw_data);

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
    catch (const DataReadingException& ex) {
        throw RemoteCallValidationException("Malformed remote call payload", inbound_call.Name, ex.what());
    }
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
            throw RemoteCallValidationException("Negative array size", type.BaseType.Name, arr_size);
        }

        for (int32_t i = 0; i < arr_size; i++) {
            ValidateInboundSimpleRemoteCallData(type.BaseType, reader, meta);
        }
    }
    else if (type.Kind == ComplexTypeKind::Dict) {
        const int32_t dict_size = reader.Read<int32_t>();

        if (dict_size < 0) {
            throw RemoteCallValidationException("Negative dict size", type.BaseType.Name, dict_size);
        }

        for (int32_t i = 0; i < dict_size; i++) {
            ValidateInboundSimpleRemoteCallData(type.KeyType.value(), reader, meta);
            ValidateInboundSimpleRemoteCallData(type.BaseType, reader, meta);
        }
    }
    else if (type.Kind == ComplexTypeKind::DictOfArray) {
        const int32_t dict_size = reader.Read<int32_t>();

        if (dict_size < 0) {
            throw RemoteCallValidationException("Negative dict size", type.BaseType.Name, dict_size);
        }

        for (int32_t i = 0; i < dict_size; i++) {
            ValidateInboundSimpleRemoteCallData(type.KeyType.value(), reader, meta);

            const int32_t arr_size = reader.Read<int32_t>();

            if (arr_size < 0) {
                throw RemoteCallValidationException("Negative array size", type.BaseType.Name, arr_size);
            }

            for (int32_t j = 0; j < arr_size; j++) {
                ValidateInboundSimpleRemoteCallData(type.BaseType, reader, meta);
            }
        }
    }
    else {
        const int32_t type_kind = static_cast<int32_t>(type.Kind);
        throw RemoteCallValidationException("Unsupported remote call container type", type_kind);
    }
}

static void ValidateInboundSimpleRemoteCallData(const BaseTypeDesc& type, DataReader& reader, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsBool) {
        const uint8_t value = reader.Read<uint8_t>();

        if (value > 1) {
            throw RemoteCallValidationException("Invalid bool value", type.Name, value);
        }
    }
    else if (type.IsInt) {
        reader.ReadPtr<uint8_t>(type.Size);
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            const float32_t value = reader.Read<float32_t>();

            if (!std::isfinite(value)) {
                throw RemoteCallValidationException("Invalid float32 value", type.Name, value);
            }
        }
        else if (type.IsDoubleFloat) {
            const float64_t value = reader.Read<float64_t>();

            if (!std::isfinite(value)) {
                throw RemoteCallValidationException("Invalid float64 value", type.Name, value);
            }
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (type.IsEnum) {
        const int32_t value = reader.Read<int32_t>();
        bool failed = false;
        const auto hstr = meta.ResolveEnumValueName(type.Name, value, &failed);
        ignore_unused(hstr);

        if (failed) {
            throw RemoteCallValidationException("Invalid enum value", type.Name, value);
        }
    }
    else if (type.IsString) {
        const int32_t str_len = reader.Read<int32_t>();

        if (str_len < 0) {
            throw RemoteCallValidationException("Negative string length", type.Name, str_len);
        }

        const size_t str_size = numeric_cast<size_t>(str_len);
        const char* str = reader.ReadPtr<char>(str_size);

        if (!strvex(string_view {str, str_size}).is_valid_utf8()) {
            throw RemoteCallValidationException("String is not valid UTF-8", type.Name);
        }
    }
    else if (type.IsHashedString) {
        const hstring::hash_t hash = reader.Read<hstring::hash_t>();
        bool failed = false;
        const auto hstr = meta.Hashes.ResolveHash(hash, &failed);
        ignore_unused(hstr);

        if (failed) {
            throw RemoteCallValidationException("Unknown hashed string value", type.Name, hash);
        }
    }
    else if (type.IsRefType) {
        const uint32_t raw_size = reader.Read<uint32_t>();
        const auto* raw_data = reader.ReadPtr<uint8_t>(raw_size);
        ValidateInboundRefTypeRawData(type, {raw_data, raw_size});
    }
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            ValidateInboundSimpleRemoteCallData(field.Type, reader, meta);
        }
    }
    else {
        throw RemoteCallValidationException("Unsupported remote call argument type", type.Name);
    }
}

static void ValidateInboundRefTypeRawData(const BaseTypeDesc& type, span<const uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(type.IsRefType);
    FO_RUNTIME_ASSERT(type.RefType);
    FO_RUNTIME_ASSERT(type.RefType->FieldsRegistrator);

    const auto* fields_registrator = type.RefType->FieldsRegistrator.get();
    const auto* pdata = raw_data.data();
    const auto* pdata_end = raw_data.data() + raw_data.size();

    for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
        const auto* field_prop = fields_registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
        span<const uint8_t> field_raw_data {};

        if (pdata < pdata_end) {
            if (static_cast<size_t>(pdata_end - pdata) < sizeof(uint32_t)) {
                throw RemoteCallValidationException("Corrupted ref type payload", type.Name, field_prop->GetName());
            }

            uint32_t field_size;
            MemCopy(&field_size, pdata, sizeof(field_size));
            pdata += sizeof(field_size);

            if (field_prop->IsPlainData() && field_size != 0 && field_size != field_prop->GetBaseSize()) {
                throw RemoteCallValidationException("Wrong ref field raw size", type.Name, field_prop->GetName(), field_size);
            }
            if (static_cast<size_t>(pdata_end - pdata) < field_size) {
                throw RemoteCallValidationException("Corrupted ref type payload", type.Name, field_prop->GetName());
            }

            field_raw_data = {pdata, field_size};
            pdata += field_size;
        }

        if (!field_raw_data.empty()) {
            try {
                ignore_unused(PropertiesSerializator::SavePropertyToValue(field_prop, field_raw_data, *field_prop->GetRegistrator()->GetHashResolver(), *field_prop->GetRegistrator()->GetNameResolver()));
            }
            catch (const std::exception& ex) {
                throw RemoteCallValidationException("Invalid ref type field payload", type.Name, field_prop->GetName(), ex.what());
            }
        }
    }

    if (pdata != pdata_end) {
        throw RemoteCallValidationException("Corrupted ref type payload", type.Name);
    }
}

FO_END_NAMESPACE
