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

#include "catch_amalgamated.hpp"

#include "DataSerialization.h"
#include "EngineBase.h"
#include "RemoteCallValidation.h"

#include <limits>

FO_BEGIN_NAMESPACE

static auto MakeSimpleArg(const BaseTypeDesc& type, string_view name = "value") -> ArgDesc
{
    return ArgDesc {.Name = string(name), .Type = ComplexTypeDesc {.Kind = ComplexTypeKind::Simple, .BaseType = type}};
}

static auto MakeArrayArg(const BaseTypeDesc& type, string_view name = "values") -> ArgDesc
{
    return ArgDesc {.Name = string(name), .Type = ComplexTypeDesc {.Kind = ComplexTypeKind::Array, .BaseType = type}};
}

static auto MakeDictOfArrayArg(const BaseTypeDesc& key_type, const BaseTypeDesc& value_type, string_view name = "values") -> ArgDesc
{
    return ArgDesc {.Name = string(name), .Type = ComplexTypeDesc {.Kind = ComplexTypeKind::DictOfArray, .BaseType = value_type, .KeyType = key_type}};
}

static auto MakeRemoteCall(EngineMetadata& meta, std::initializer_list<ArgDesc> args) -> RemoteCallDesc
{
    RemoteCallDesc call {};
    call.Name = meta.Hashes.ToHashedString("TestCall");
    call.Args.assign(args.begin(), args.end());
    return call;
}

TEST_CASE("RemoteCallValidation")
{
    EngineMetadata meta {[] { }};
    meta.RegisterSide(EngineSideKind::ServerSide);
    meta.RegisterEnumGroup("TestEnum", "int32", {{"None", 0}, {"Value", 1}});
    meta.RegisterValueType("TestStruct");
    meta.RegisterValueTypeLayout("TestStruct", {{"Number", "float32"}, {"Flag", "bool"}});

    const BaseTypeDesc& string_type = meta.GetBaseType("string");
    const BaseTypeDesc& enum_type = meta.GetBaseType("TestEnum");
    const BaseTypeDesc& float_type = meta.GetBaseType("float32");
    const BaseTypeDesc& hstring_type = meta.GetBaseType("hstring");
    const BaseTypeDesc& int_type = meta.GetBaseType("int32");
    const BaseTypeDesc& uint16_type = meta.GetBaseType("uint16");
    const BaseTypeDesc& bool_type = meta.GetBaseType("bool");
    const BaseTypeDesc& struct_type = meta.GetBaseType("TestStruct");
    const hstring known_hash = meta.Hashes.ToHashedString("KnownHash");

    SECTION("Accepts valid nested payload")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(string_type, "text"), MakeSimpleArg(enum_type, "enum_value"), MakeSimpleArg(float_type, "float_value"), MakeSimpleArg(hstring_type, "hash_value"), MakeArrayArg(int_type, "numbers"), MakeDictOfArrayArg(hstring_type, uint16_type, "mapped_values"), MakeSimpleArg(struct_type, "packed_struct")});

        vector<uint8> data;
        DataWriter writer(data);
        const string text = "text";
        const uint16 first_uint16 = 7;
        const uint16 second_uint16 = 8;
        const uint8 true_bool = 1;

        writer.Write<int32>(numeric_cast<int32>(text.length()));
        writer.WritePtr(text.data(), text.length());
        writer.Write<int32>(1);
        writer.Write<float32>(42.0f);
        writer.Write<hstring::hash_t>(known_hash.as_hash());
        writer.Write<int32>(2);
        writer.Write<int32>(10);
        writer.Write<int32>(20);
        writer.Write<int32>(1);
        writer.Write<hstring::hash_t>(known_hash.as_hash());
        writer.Write<int32>(2);
        writer.Write<uint16>(first_uint16);
        writer.Write<uint16>(second_uint16);
        writer.Write<float32>(3.5f);
        writer.Write<uint8>(true_bool);

        CHECK_NOTHROW(ValidateInboundRemoteCallData(call, data, meta));
    }

    SECTION("Rejects invalid UTF-8 strings")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(string_type)});
        vector<uint8> data;
        DataWriter writer(data);
        const uint8 bytes[] = {0xC3, 0x28};

        writer.Write<int32>(2);
        writer.WritePtr(bytes, std::size(bytes));

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects invalid enum values")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(enum_type)});
        vector<uint8> data;
        DataWriter writer(data);

        writer.Write<int32>(77);

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects non finite floats inside arrays")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeArrayArg(float_type)});
        vector<uint8> data;
        DataWriter writer(data);

        writer.Write<int32>(2);
        writer.Write<float32>(1.0f);
        writer.Write<float32>(std::numeric_limits<float32>::infinity());

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects unknown hashed strings")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(hstring_type)});
        vector<uint8> data;
        DataWriter writer(data);
        const hstring::hash_t unknown_hash = Hashing::MurmurHash2("MissingHash", 11);

        writer.Write<hstring::hash_t>(unknown_hash);

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects invalid bool values")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(bool_type)});
        vector<uint8> data;
        DataWriter writer(data);
        const uint8 invalid_bool = 2;

        writer.Write<uint8>(invalid_bool);

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects truncated nested collections")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeDictOfArrayArg(hstring_type, uint16_type)});
        vector<uint8> data;
        DataWriter writer(data);
        const uint16 first_uint16 = 7;

        writer.Write<int32>(1);
        writer.Write<hstring::hash_t>(known_hash.as_hash());
        writer.Write<int32>(2);
        writer.Write<uint16>(first_uint16);

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }
}

FO_END_NAMESPACE
