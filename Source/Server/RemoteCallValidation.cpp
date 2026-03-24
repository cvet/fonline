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

FO_BEGIN_NAMESPACE

static void ValidateInboundRemoteCallArgData(const ComplexTypeDesc& type, DataReader& reader, const EngineMetadata& meta);
static void ValidateInboundSimpleRemoteCallData(const BaseTypeDesc& type, DataReader& reader, const EngineMetadata& meta);

void ValidateInboundRemoteCallData(const RemoteCallDesc& inbound_call, const_span<uint8> data, const EngineMetadata& meta)
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
        const int32 arr_size = reader.Read<int32>();

        if (arr_size < 0) {
            throw RemoteCallValidationException("Negative array size", type.BaseType.Name, arr_size);
        }

        for (int32 i = 0; i < arr_size; i++) {
            ValidateInboundSimpleRemoteCallData(type.BaseType, reader, meta);
        }
    }
    else if (type.Kind == ComplexTypeKind::Dict) {
        const int32 dict_size = reader.Read<int32>();

        if (dict_size < 0) {
            throw RemoteCallValidationException("Negative dict size", type.BaseType.Name, dict_size);
        }

        for (int32 i = 0; i < dict_size; i++) {
            ValidateInboundSimpleRemoteCallData(type.KeyType.value(), reader, meta);
            ValidateInboundSimpleRemoteCallData(type.BaseType, reader, meta);
        }
    }
    else if (type.Kind == ComplexTypeKind::DictOfArray) {
        const int32 dict_size = reader.Read<int32>();

        if (dict_size < 0) {
            throw RemoteCallValidationException("Negative dict size", type.BaseType.Name, dict_size);
        }

        for (int32 i = 0; i < dict_size; i++) {
            ValidateInboundSimpleRemoteCallData(type.KeyType.value(), reader, meta);

            const int32 arr_size = reader.Read<int32>();

            if (arr_size < 0) {
                throw RemoteCallValidationException("Negative array size", type.BaseType.Name, arr_size);
            }

            for (int32 j = 0; j < arr_size; j++) {
                ValidateInboundSimpleRemoteCallData(type.BaseType, reader, meta);
            }
        }
    }
    else {
        const int32 type_kind = static_cast<int32>(type.Kind);
        throw RemoteCallValidationException("Unsupported remote call container type", type_kind);
    }
}

static void ValidateInboundSimpleRemoteCallData(const BaseTypeDesc& type, DataReader& reader, const EngineMetadata& meta)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsBool) {
        const uint8 value = reader.Read<uint8>();

        if (value > 1) {
            throw RemoteCallValidationException("Invalid bool value", type.Name, value);
        }
    }
    else if (type.IsInt) {
        reader.ReadPtr<uint8>(type.Size);
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            const float32 value = reader.Read<float32>();

            if (!std::isfinite(value)) {
                throw RemoteCallValidationException("Invalid float32 value", type.Name, value);
            }
        }
        else if (type.IsDoubleFloat) {
            const float64 value = reader.Read<float64>();

            if (!std::isfinite(value)) {
                throw RemoteCallValidationException("Invalid float64 value", type.Name, value);
            }
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (type.IsEnum) {
        const int32 value = reader.Read<int32>();
        bool failed = false;
        const auto hstr = meta.ResolveEnumValueName(type.Name, value, &failed);
        ignore_unused(hstr);

        if (failed) {
            throw RemoteCallValidationException("Invalid enum value", type.Name, value);
        }
    }
    else if (type.IsString) {
        const int32 str_len = reader.Read<int32>();

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
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            ValidateInboundSimpleRemoteCallData(field.Type, reader, meta);
        }
    }
    else {
        throw RemoteCallValidationException("Unsupported remote call argument type", type.Name);
    }
}

FO_END_NAMESPACE
