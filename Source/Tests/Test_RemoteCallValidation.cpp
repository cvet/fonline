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
#include "PropertiesSerializator.h"
#include "RemoteCallValidation.h"

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

static auto MakeRefTypeRawData(EngineMetadata& meta, string_view ref_type_name, const AnyData::Value& value) -> vector<uint8_t>
{
    PropertyRegistrator registrator("RemoteCallValidationEntity", EngineSideKind::ServerSide, meta.Hashes, meta);
    const auto* prop = registrator.RegisterProperty({"Common", ref_type_name, "Value"});
    Properties props(&registrator);
    PropertiesSerializator::LoadPropertyFromValue(&props, prop, value, meta.Hashes, meta);

    const auto raw_data = props.GetRawData(prop);
    return {raw_data.begin(), raw_data.end()};
}

static void WriteRefTypePayload(DataWriter& writer, const vector<uint8_t>& raw_data)
{
    writer.Write<uint32_t>(numeric_cast<uint32_t>(raw_data.size()));

    if (!raw_data.empty()) {
        writer.WritePtr(raw_data.data(), raw_data.size());
    }
}

TEST_CASE("RemoteCallValidation")
{
    EngineMetadata meta {[] { }};
    meta.RegisterSide(EngineSideKind::ServerSide);
    meta.RegisterEnumGroup("TestEnum", "int32", {{"None", 0}, {"Value", 1}});
    meta.RegisterValueType("TestStruct");
    meta.RegisterValueTypeLayout("TestStruct", {{"Number", "float32"}, {"Flag", "bool"}});
    meta.RegisterRefType("TestRefType");
    meta.RegisterRefTypeLayout("TestRefType", {{"Numbers", "int32[]"}, {"Label", "string"}, {"Mode", "TestEnum"}});
    meta.RegisterRefType("EnumRefType");
    meta.RegisterRefTypeLayout("EnumRefType", {{"State", "TestEnum"}});
    meta.RegisterRefType("NestedRefType");
    meta.RegisterRefTypeLayout("NestedRefType", {{"Inner", "TestRefType"}, {"Status", "EnumRefType"}, {"Label", "string"}});

    const BaseTypeDesc& string_type = meta.GetBaseType("string");
    const BaseTypeDesc& enum_type = meta.GetBaseType("TestEnum");
    const BaseTypeDesc& float_type = meta.GetBaseType("float32");
    const BaseTypeDesc& hstring_type = meta.GetBaseType("hstring");
    const BaseTypeDesc& int_type = meta.GetBaseType("int32");
    const BaseTypeDesc& uint16_type = meta.GetBaseType("uint16");
    const BaseTypeDesc& bool_type = meta.GetBaseType("bool");
    const BaseTypeDesc& struct_type = meta.GetBaseType("TestStruct");
    const BaseTypeDesc& ref_type = meta.GetBaseType("TestRefType");
    const BaseTypeDesc& enum_ref_type = meta.GetBaseType("EnumRefType");
    const BaseTypeDesc& nested_ref_type = meta.GetBaseType("NestedRefType");
    const hstring known_hash = meta.Hashes.ToHashedString("KnownHash");

    SECTION("Accepts valid nested payload")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(string_type, "text"), MakeSimpleArg(enum_type, "enum_value"), MakeSimpleArg(float_type, "float_value"), MakeSimpleArg(hstring_type, "hash_value"), MakeArrayArg(int_type, "numbers"), MakeDictOfArrayArg(hstring_type, uint16_type, "mapped_values"), MakeSimpleArg(struct_type, "packed_struct")});

        vector<uint8_t> data;
        DataWriter writer(data);
        const string text = "text";
        const uint16_t first_uint16 = 7;
        const uint16_t second_uint16 = 8;
        const uint8_t true_bool = 1;

        writer.Write<int32_t>(numeric_cast<int32_t>(text.length()));
        writer.WritePtr(text.data(), text.length());
        writer.Write<int32_t>(1);
        writer.Write<float32_t>(42.0f);
        writer.Write<hstring::hash_t>(known_hash.as_hash());
        writer.Write<int32_t>(2);
        writer.Write<int32_t>(10);
        writer.Write<int32_t>(20);
        writer.Write<int32_t>(1);
        writer.Write<hstring::hash_t>(known_hash.as_hash());
        writer.Write<int32_t>(2);
        writer.Write<uint16_t>(first_uint16);
        writer.Write<uint16_t>(second_uint16);
        writer.Write<float32_t>(3.5f);
        writer.Write<uint8_t>(true_bool);

        CHECK_NOTHROW(ValidateInboundRemoteCallData(call, data, meta));
    }

    SECTION("Rejects invalid UTF-8 strings")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(string_type)});
        vector<uint8_t> data;
        DataWriter writer(data);
        const uint8_t bytes[] = {0xC3, 0x28};

        writer.Write<int32_t>(2);
        writer.WritePtr(bytes, std::size(bytes));

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects invalid enum values")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(enum_type)});
        vector<uint8_t> data;
        DataWriter writer(data);

        writer.Write<int32_t>(77);

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects non finite floats inside arrays")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeArrayArg(float_type)});
        vector<uint8_t> data;
        DataWriter writer(data);

        writer.Write<int32_t>(2);
        writer.Write<float32_t>(1.0f);
        writer.Write<float32_t>(std::numeric_limits<float32_t>::infinity());

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects unknown hashed strings")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(hstring_type)});
        vector<uint8_t> data;
        DataWriter writer(data);
        const hstring::hash_t unknown_hash = hashing_ex::hash("MissingHash", 11);

        writer.Write<hstring::hash_t>(unknown_hash);

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects invalid bool values")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(bool_type)});
        vector<uint8_t> data;
        DataWriter writer(data);
        const uint8_t invalid_bool = 2;

        writer.Write<uint8_t>(invalid_bool);

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects truncated nested collections")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeDictOfArrayArg(hstring_type, uint16_type)});
        vector<uint8_t> data;
        DataWriter writer(data);
        const uint16_t first_uint16 = 7;

        writer.Write<int32_t>(1);
        writer.Write<hstring::hash_t>(known_hash.as_hash());
        writer.Write<int32_t>(2);
        writer.Write<uint16_t>(first_uint16);

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Accepts valid ref type payloads and collections")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(ref_type, "snapshot"), MakeArrayArg(ref_type, "history"), MakeDictOfArrayArg(int_type, ref_type, "groups")});

        const auto make_full_snapshot = [] {
            AnyData::Array numbers;
            numbers.EmplaceBack(int64_t {10});
            numbers.EmplaceBack(int64_t {20});

            AnyData::Dict snapshot;
            snapshot.Emplace("Numbers", AnyData::Value {std::move(numbers)});
            snapshot.Emplace("Label", AnyData::Value {string {"alpha"}});
            snapshot.Emplace("Mode", AnyData::Value {string {"Value"}});
            return AnyData::Value {std::move(snapshot)};
        };

        const auto make_sparse_snapshot = [] {
            AnyData::Dict snapshot;
            snapshot.Emplace("Label", AnyData::Value {string {"tail"}});
            return AnyData::Value {std::move(snapshot)};
        };

        const auto full_raw = MakeRefTypeRawData(meta, "TestRefType", make_full_snapshot());
        const auto sparse_raw = MakeRefTypeRawData(meta, "TestRefType", make_sparse_snapshot());

        vector<uint8_t> data;
        DataWriter writer(data);

        WriteRefTypePayload(writer, full_raw);

        writer.Write<int32_t>(2);
        WriteRefTypePayload(writer, full_raw);
        WriteRefTypePayload(writer, sparse_raw);

        writer.Write<int32_t>(1);
        writer.Write<int32_t>(7);
        writer.Write<int32_t>(2);
        WriteRefTypePayload(writer, sparse_raw);
        WriteRefTypePayload(writer, full_raw);

        CHECK_NOTHROW(ValidateInboundRemoteCallData(call, data, meta));
    }

    SECTION("Accepts nested ref type payloads and collections")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(nested_ref_type, "snapshot"), MakeArrayArg(nested_ref_type, "history"), MakeDictOfArrayArg(int_type, nested_ref_type, "groups")});

        const auto make_inner_snapshot = [] {
            AnyData::Array numbers;
            numbers.EmplaceBack(int64_t {10});
            numbers.EmplaceBack(int64_t {20});

            AnyData::Dict snapshot;
            snapshot.Emplace("Numbers", AnyData::Value {std::move(numbers)});
            snapshot.Emplace("Label", AnyData::Value {string {"alpha"}});
            snapshot.Emplace("Mode", AnyData::Value {string {"Value"}});
            return AnyData::Value {std::move(snapshot)};
        };

        const auto make_status = [] {
            AnyData::Dict status;
            status.Emplace("State", AnyData::Value {string {"Value"}});
            return AnyData::Value {std::move(status)};
        };

        const auto make_full_snapshot = [&] {
            AnyData::Dict snapshot;
            snapshot.Emplace("Inner", make_inner_snapshot());
            snapshot.Emplace("Status", make_status());
            snapshot.Emplace("Label", AnyData::Value {string {"outer"}});
            return AnyData::Value {std::move(snapshot)};
        };

        const auto make_sparse_snapshot = [] {
            AnyData::Dict snapshot;
            snapshot.Emplace("Label", AnyData::Value {string {"tail"}});
            return AnyData::Value {std::move(snapshot)};
        };

        const auto full_raw = MakeRefTypeRawData(meta, "NestedRefType", make_full_snapshot());
        const auto sparse_raw = MakeRefTypeRawData(meta, "NestedRefType", make_sparse_snapshot());

        vector<uint8_t> data;
        DataWriter writer(data);

        WriteRefTypePayload(writer, full_raw);

        writer.Write<int32_t>(2);
        WriteRefTypePayload(writer, full_raw);
        WriteRefTypePayload(writer, sparse_raw);

        writer.Write<int32_t>(1);
        writer.Write<int32_t>(7);
        writer.Write<int32_t>(2);
        WriteRefTypePayload(writer, sparse_raw);
        WriteRefTypePayload(writer, full_raw);

        CHECK_NOTHROW(ValidateInboundRemoteCallData(call, data, meta));
    }

    SECTION("Accepts empty ref type collections")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeArrayArg(ref_type, "history"), MakeDictOfArrayArg(int_type, ref_type, "groups")});
        vector<uint8_t> data;
        DataWriter writer(data);

        writer.Write<int32_t>(0);
        writer.Write<int32_t>(0);

        CHECK_NOTHROW(ValidateInboundRemoteCallData(call, data, meta));
    }

    SECTION("Rejects invalid ref type field payloads")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeSimpleArg(enum_ref_type)});
        vector<uint8_t> data;
        DataWriter writer(data);
        const uint32_t raw_size = sizeof(uint32_t) + sizeof(int32_t);
        const uint32_t field_size = sizeof(int32_t);
        const int32_t invalid_enum = 77;

        writer.Write<uint32_t>(raw_size);
        writer.Write<uint32_t>(field_size);
        writer.Write<int32_t>(invalid_enum);

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }

    SECTION("Rejects truncated ref type payloads inside arrays")
    {
        const RemoteCallDesc call = MakeRemoteCall(meta, {MakeArrayArg(ref_type)});
        vector<uint8_t> data;
        DataWriter writer(data);
        AnyData::Dict snapshot;
        snapshot.Emplace("Label", AnyData::Value {string {"one"}});
        const auto full_raw = MakeRefTypeRawData(meta, "TestRefType", AnyData::Value {std::move(snapshot)});

        writer.Write<int32_t>(2);
        WriteRefTypePayload(writer, full_raw);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(full_raw.size()));
        writer.WritePtr(full_raw.data(), full_raw.size() - 1);

        CHECK_THROWS_AS(ValidateInboundRemoteCallData(call, data, meta), RemoteCallValidationException);
    }
}

FO_END_NAMESPACE
