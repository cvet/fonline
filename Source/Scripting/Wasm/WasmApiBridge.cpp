//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/ /_/\__, /_/_/ /_/\___/
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

#include "WasmApiBridge.h"

#if FO_WASM_SCRIPTING

#include "EngineBase.h"
#include "ImGuiStuff.h"

FO_BEGIN_NAMESPACE

static constexpr string_view WASM_ENTITY_HANDLE_TYPE_NAME = "ident";
static constexpr string_view WASM_FIXED_TYPE_HANDLE_TYPE_NAME = "hstring";
static constexpr WasmScalarKind WASM_ENTITY_HANDLE_KIND = WasmScalarKind::I64;
static constexpr WasmScalarKind WASM_FIXED_TYPE_HANDLE_KIND = WasmScalarKind::I64;
static constexpr WasmScalarKind WASM_REF_TYPE_HANDLE_KIND = WasmScalarKind::I64;
static constexpr size_t WASM_API_OPAQUE_SCALAR_STORAGE_SIZE = 16;

struct alignas(std::max_align_t) WasmApiOpaqueScalarStorage
{
    array<uint8_t, WASM_API_OPAQUE_SCALAR_STORAGE_SIZE> Data {};
};

struct WasmApiMutableArgCopyback
{
    nptr<const ArgDesc> Arg {};
    size_t StorageIndex {};
    WasmScalarKind Kind {};
    uint64_t RawPtr {};
    uint64_t RawSize {};
};

struct WasmApiMutableArrayCopyback
{
    nptr<const ArgDesc> Arg {};
    size_t StorageIndex {};
    WasmScalarKind Kind {};
    uint64_t RawPtr {};
    uint64_t RawCapacity {};
    uint64_t RawRequiredSizePtr {};
    bool StringArray {};
};

struct WasmApiMutableStringCopyback
{
    nptr<const ArgDesc> Arg {};
    size_t StorageIndex {};
    uint64_t RawPtr {};
    uint64_t RawCapacity {};
    uint64_t RawRequiredSizePtr {};
};

struct WasmApiMutableDictCopyback
{
    nptr<const ArgDesc> Arg {};
    size_t StorageIndex {};
    uint64_t RawPtr {};
    uint64_t RawCapacity {};
    uint64_t RawRequiredSizePtr {};
};

static auto IsApiNameChar(char ch) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

static auto SanitizeApiNamePart(string_view text) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;
    result.reserve(text.size());

    for (char ch : text) {
        result += IsApiNameChar(ch) ? ch : '_';
    }

    return !result.empty() ? result : string {"_"};
}

static auto MakeWasmApiTypeName(const ComplexTypeDesc& type) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;

    switch (type.Kind) {
    case ComplexTypeKind::None:
        result = "void";
        break;
    case ComplexTypeKind::Simple:
        result = type.BaseType.Name;
        break;
    case ComplexTypeKind::Array:
        result = strex("{}_array", type.BaseType.Name);
        break;
    case ComplexTypeKind::Dict:
        result = strex("{}_{}_dict", type.KeyType.has_value() ? type.KeyType->Name : string {"unknown"}, type.BaseType.Name);
        break;
    case ComplexTypeKind::DictOfArray:
        result = strex("{}_{}_array_dict", type.KeyType.has_value() ? type.KeyType->Name : string {"unknown"}, type.BaseType.Name);
        break;
    case ComplexTypeKind::Callback:
        result = "callback";
        break;
    default:
        result = "unknown";
        break;
    }

    if (type.IsMutable) {
        result += "_mut";
    }

    return result;
}

static auto MakeWasmApiPropertyTypeName(const Property& prop) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (prop.IsDictOfArray()) {
        return SanitizeApiNamePart(strex("{}_{}_array_dict", prop.GetDictKeyType().Name, prop.GetBaseType().Name));
    }
    if (prop.IsDict()) {
        return SanitizeApiNamePart(strex("{}_{}_dict", prop.GetDictKeyType().Name, prop.GetBaseType().Name));
    }
    if (prop.IsArray()) {
        return SanitizeApiNamePart(strex("{}_array", prop.GetBaseType().Name));
    }

    return SanitizeApiNamePart(prop.GetViewTypeName());
}

static auto TryResolveWasmIntegerScalarKind(const BaseTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsInt8 || type.IsInt16 || type.IsInt32 || type.IsUInt8 || type.IsUInt16 || type.IsUInt32 || type.IsBool) {
        return WasmScalarKind::I32;
    }
    if (type.IsInt64 || type.IsUInt64) {
        return WasmScalarKind::I64;
    }

    return WasmScalarKind::None;
}

static auto IsWasmApiIdentType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsSimpleStruct && type.Name == "ident";
}

static auto IsWasmApiEntityHandleType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsEntity && !type.IsGlobalEntity && !type.IsEntityProto;
}

static auto IsWasmApiProtoHandleType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsEntityProto;
}

static auto IsWasmApiFixedTypeHandleType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsFixedType;
}

static auto IsWasmApiRefHandleType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsRefType;
}

static auto IsWasmApiProtoOrFixedHandleType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmApiProtoHandleType(type) || IsWasmApiFixedTypeHandleType(type);
}

static auto IsWasmApiStringBaseType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsString && type.Name == "string";
}

static auto IsWasmApiAnyBaseType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsString && type.Name == "any";
}

static auto IsWasmApiTextBaseType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmApiStringBaseType(type) || IsWasmApiAnyBaseType(type);
}

static auto IsWasmApiStringType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && !type.IsMutable && IsWasmApiTextBaseType(type.BaseType);
}

static auto IsWasmApiMutableStringType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && type.IsMutable && IsWasmApiTextBaseType(type.BaseType);
}

static auto IsWasmApiStringProperty(const Property& prop) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return prop.IsString() && IsWasmApiTextBaseType(prop.GetBaseType());
}

static auto TryResolveWasmApiMutableScalarKind(const ComplexTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!type.IsMutable || type.Kind != ComplexTypeKind::Simple || type.BaseType.IsString || IsWasmApiEntityHandleType(type.BaseType) || IsWasmApiProtoOrFixedHandleType(type.BaseType) || IsWasmApiRefHandleType(type.BaseType)) {
        return WasmScalarKind::None;
    }

    return TryResolveWasmApiScalarKind(type.BaseType);
}

static auto IsWasmApiArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Array && !type.IsMutable && !type.BaseType.IsString;
}

static auto IsWasmApiMutableArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Array && type.IsMutable && !type.BaseType.IsString;
}

static auto IsWasmApiStringArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Array && !type.IsMutable && IsWasmApiTextBaseType(type.BaseType);
}

static auto IsWasmApiMutableStringArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Array && type.IsMutable && IsWasmApiTextBaseType(type.BaseType);
}

static auto IsWasmApiStringArrayProperty(const Property& prop) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return prop.IsArrayOfString() && IsWasmApiTextBaseType(prop.GetBaseType());
}

static auto TryResolveWasmApiArrayElementKind(const ComplexTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsWasmApiArrayType(type) && !IsWasmApiMutableArrayType(type)) {
        return WasmScalarKind::None;
    }

    return TryResolveWasmApiScalarKind(type.BaseType);
}

static auto TryResolveWasmApiPropertyArrayElementKind(const Property& prop) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!prop.IsArray() || prop.IsArrayOfString() || IsWasmApiRefHandleType(prop.GetBaseType())) {
        return WasmScalarKind::None;
    }

    return TryResolveWasmApiScalarKind(prop.GetBaseType());
}

static auto IsWasmApiBufferElementType(const BaseTypeDesc& type) noexcept -> bool;
static auto IsWasmApiResolvedBufferElementType(const BaseTypeDesc& type, WasmScalarKind kind) noexcept -> bool;

static auto IsWasmApiDictElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmApiTextBaseType(type) || TryResolveWasmApiScalarKind(type) != WasmScalarKind::None || IsWasmApiBufferElementType(type);
}

static auto IsWasmApiDictKeyType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmApiDictElementType(type);
}

static auto IsWasmApiMutableDictKeyType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmApiDictElementType(type);
}

static auto IsWasmApiPropertyDictKeyType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !IsWasmApiRefHandleType(type) && IsWasmApiDictElementType(type);
}

static auto IsWasmApiMutableDictElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmApiDictElementType(type);
}

static auto IsWasmApiDictType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Dict && !type.IsMutable && type.KeyType.has_value() && IsWasmApiDictKeyType(*type.KeyType) && IsWasmApiDictElementType(type.BaseType);
}

static auto IsWasmApiMutableDictType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Dict && type.IsMutable && type.KeyType.has_value() && IsWasmApiMutableDictKeyType(*type.KeyType) && IsWasmApiMutableDictElementType(type.BaseType);
}

static auto IsWasmApiDictArrayElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmApiTextBaseType(type) || TryResolveWasmApiScalarKind(type) != WasmScalarKind::None || IsWasmApiBufferElementType(type);
}

static auto IsWasmApiMutableDictArrayElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmApiDictArrayElementType(type);
}

static auto IsWasmApiDictOfArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::DictOfArray && !type.IsMutable && type.KeyType.has_value() && IsWasmApiDictKeyType(*type.KeyType) && IsWasmApiDictArrayElementType(type.BaseType);
}

static auto IsWasmApiMutableDictOfArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::DictOfArray && type.IsMutable && type.KeyType.has_value() && IsWasmApiMutableDictKeyType(*type.KeyType) && IsWasmApiMutableDictArrayElementType(type.BaseType);
}

static auto IsWasmApiPropertyDictType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Dict && !type.IsMutable && type.KeyType.has_value() && IsWasmApiPropertyDictKeyType(*type.KeyType) && IsWasmApiDictElementType(type.BaseType);
}

static auto IsWasmApiPropertyDictOfArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::DictOfArray && !type.IsMutable && type.KeyType.has_value() && IsWasmApiPropertyDictKeyType(*type.KeyType) && IsWasmApiDictArrayElementType(type.BaseType);
}

static auto TryResolveWasmApiDictElementKind(const BaseTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmApiTextBaseType(type) ? WasmScalarKind::None : TryResolveWasmApiScalarKind(type);
}

static auto IsWasmApiCallbackType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Kind != ComplexTypeKind::Callback || type.IsMutable || !type.CallbackArgs || type.CallbackArgs->empty()) {
        return false;
    }

    const vector<ComplexTypeDesc>& callback_args = *type.CallbackArgs;

    if (callback_args.front().Kind == ComplexTypeKind::Callback || callback_args.front().IsMutable) {
        return false;
    }

    for (size_t i = 1; i < callback_args.size(); i++) {
        if (callback_args[i].Kind == ComplexTypeKind::None) {
            return false;
        }
        if (callback_args[i].Kind == ComplexTypeKind::Callback && !IsWasmApiCallbackType(callback_args[i])) {
            return false;
        }
    }

    return true;
}

static auto MakeWasmApiReceiverHandleTypeName(string_view entity_name, WasmApiReceiverKind receiver_kind) -> string
{
    FO_STACK_TRACE_ENTRY();

    switch (receiver_kind) {
    case WasmApiReceiverKind::None:
        return {};
    case WasmApiReceiverKind::Entity:
        return string {WASM_ENTITY_HANDLE_TYPE_NAME};
    case WasmApiReceiverKind::FixedType:
        return string {WASM_FIXED_TYPE_HANDLE_TYPE_NAME};
    case WasmApiReceiverKind::RefType:
        return string {entity_name};
    default:
        return {};
    }
}

static auto WasmApiReceiverKindToScalarKind(WasmApiReceiverKind receiver_kind) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (receiver_kind) {
    case WasmApiReceiverKind::None:
        return WasmScalarKind::None;
    case WasmApiReceiverKind::Entity:
        return WASM_ENTITY_HANDLE_KIND;
    case WasmApiReceiverKind::FixedType:
        return WASM_FIXED_TYPE_HANDLE_KIND;
    case WasmApiReceiverKind::RefType:
        return WASM_REF_TYPE_HANDLE_KIND;
    default:
        return WasmScalarKind::None;
    }
}

static auto IsWasmApiPackedStructFieldType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsBool || type.IsInt || type.IsFloat || type.IsEnum) {
        return true;
    }
    if (type.IsSimpleStruct) {
        return type.StructLayout != nullptr && type.StructLayout->Fields.size() == 1 && IsWasmApiPackedStructFieldType(type.StructLayout->Fields.front().Type);
    }

    return false;
}

static auto IsWasmApiPackedStructType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!type.IsComplexStruct || type.StructLayout == nullptr || type.Size == 0 || type.Size > sizeof(uint64_t)) {
        return false;
    }

    for (const FieldDesc& field : type.StructLayout->Fields) {
        if (!IsWasmApiPackedStructFieldType(field.Type)) {
            return false;
        }
    }

    return true;
}

static auto IsWasmApiBufferStructType(const BaseTypeDesc& type) noexcept -> bool;

static auto IsWasmApiBufferStructFieldType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsBool || type.IsInt || type.IsFloat || type.IsEnum || type.IsHashedString) {
        return true;
    }
    if (type.IsSimpleStruct || type.IsComplexStruct) {
        return IsWasmApiBufferStructType(type);
    }

    return false;
}

static auto IsWasmApiBufferStructType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if ((!type.IsSimpleStruct && !type.IsComplexStruct) || type.StructLayout == nullptr || type.Size == 0 || type.Size > WASM_API_OPAQUE_SCALAR_STORAGE_SIZE) {
        return false;
    }

    for (const FieldDesc& field : type.StructLayout->Fields) {
        if (!IsWasmApiBufferStructFieldType(field.Type)) {
            return false;
        }
    }

    return true;
}

static auto IsWasmApiOpaqueScalarType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsEnum || type.IsSimpleStruct || IsWasmApiPackedStructType(type);
}

static auto IsWasmApiBufferElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmApiBufferStructType(type);
}

static auto IsWasmApiResolvedBufferElementType(const BaseTypeDesc& type, WasmScalarKind kind) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return kind != WasmScalarKind::None || IsWasmApiBufferElementType(type);
}

static auto IsWasmApiValueBufferType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && !type.IsMutable && TryResolveWasmApiScalarKind(type) == WasmScalarKind::None && IsWasmApiBufferElementType(type.BaseType);
}

static auto IsWasmApiPropertyValueBufferType(const Property& prop) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return prop.IsPlainData() && !prop.IsArray() && !prop.IsDict() && !IsWasmApiStringProperty(prop) && TryResolveWasmApiScalarKind(prop.GetBaseType()) == WasmScalarKind::None && IsWasmApiBufferElementType(prop.GetBaseType());
}

static auto TryResolveWasmApiRawScalarKind(const BaseTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsEnum) {
        return type.EnumUnderlyingType != nullptr ? TryResolveWasmApiRawScalarKind(*type.EnumUnderlyingType) : WasmScalarKind::None;
    }
    if (type.IsSimpleStruct) {
        if (type.StructLayout == nullptr || type.StructLayout->Fields.size() != 1) {
            return WasmScalarKind::None;
        }

        return TryResolveWasmApiRawScalarKind(type.StructLayout->Fields.front().Type);
    }
    if (IsWasmApiPackedStructType(type)) {
        return type.Size <= sizeof(uint32_t) ? WasmScalarKind::I32 : WasmScalarKind::I64;
    }
    if (type.IsBool || type.IsInt) {
        return TryResolveWasmIntegerScalarKind(type);
    }
    if (type.IsHashedString) {
        return WasmScalarKind::I64;
    }
    if (type.IsSingleFloat) {
        return WasmScalarKind::F32;
    }
    if (type.IsDoubleFloat) {
        return WasmScalarKind::F64;
    }

    return WasmScalarKind::None;
}

static auto BuildUnsupportedReason(const MethodDesc& method, size_t arg_index, const ComplexTypeDesc& type) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("argument {} '{}' has unsupported WASM ABI type '{}'", arg_index, method.Args[arg_index].Name, MakeWasmApiTypeName(type));
}

static auto BuildUnsupportedPropertyReason(const Property& prop) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("property '{}' has unsupported WASM ABI type '{}'", prop.GetName(), prop.GetViewTypeName());
}

static auto BuildUnsupportedPropertyDictReason(const Property& prop, const ComplexTypeDesc& type) -> string
{
    FO_STACK_TRACE_ENTRY();

    if ((type.Kind == ComplexTypeKind::Dict || type.Kind == ComplexTypeKind::DictOfArray) && type.KeyType.has_value() && IsWasmApiRefHandleType(*type.KeyType)) {
        return strex("property '{}' has unsupported WASM ABI type '{}': ref-type dictionary keys require an explicit property raw-storage and serialization policy", prop.GetName(), prop.GetViewTypeName());
    }

    return BuildUnsupportedPropertyReason(prop);
}

static auto ResolveWasmApiGlobalEntity(BaseEngine& engine, string_view entity_name) -> ptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    if (entity_name == "Game") {
        return make_ptr(&engine);
    }
    if (entity_name == "ImGui") {
        return engine.GetImGui();
    }

    throw ScriptCallException("Unsupported WASM global entity receiver", entity_name);
}

static auto MakeWasmApiIdent(uint64_t raw_id) noexcept -> ident_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return ident_t {std::bit_cast<ident_t::underlying_type>(raw_id)};
}

static auto PackWasmApiIdent(ident_t id) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::bit_cast<uint64_t>(id.underlying_value());
}

static auto IsWasmApiEntityTypeCompatible(string_view expected_type_name, string_view actual_type_name) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (expected_type_name == "Entity" || expected_type_name == actual_type_name) {
        return true;
    }

    for (string_view prefix : {string_view {"Proto"}, string_view {"Static"}, string_view {"Abstract"}}) {
        if (expected_type_name.starts_with(prefix) && expected_type_name.substr(prefix.size()) == actual_type_name) {
            return true;
        }
    }

    return false;
}

static auto ResolveWasmApiEntityHandle(BaseEngine& engine, string_view entity_name, uint64_t raw_id, bool nullable, string_view role) -> nptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    if (raw_id == 0 && nullable) {
        return nullptr;
    }

    ident_t entity_id = MakeWasmApiIdent(raw_id);
    nptr<Entity> entity = engine.ResolveScriptEntityHandle(entity_name, entity_id);

    if (!entity) {
        throw ScriptCallException(strex("WASM engine API {} entity not found", role), entity_name, entity_id.underlying_value());
    }
    if (entity->IsDestroyed()) {
        throw ScriptCallException(strex("WASM engine API {} entity is destroyed", role), entity_name, entity_id.underlying_value());
    }

    ptr<const PropertyRegistrator> actual_registrator = entity->GetProperties()->GetRegistrator();

    if (!IsWasmApiEntityTypeCompatible(entity_name, actual_registrator->GetTypeName().as_str())) {
        throw ScriptCallException(strex("WASM engine API {} entity type mismatch", role), entity_name, actual_registrator->GetTypeName());
    }

    return entity;
}

static auto ResolveWasmApiEntityReceiver(BaseEngine& engine, string_view entity_name, uint64_t raw_id) -> ptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    return ResolveWasmApiEntityHandle(engine, entity_name, raw_id, false, "receiver");
}

static auto ResolveWasmApiProtoHandle(BaseEngine& engine, string_view proto_type_name, uint64_t raw_hash, bool nullable) -> nptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    if (raw_hash == 0 && nullable) {
        return nullptr;
    }

    hstring proto_id = engine.Hashes.ResolveHash(raw_hash);
    nptr<const ProtoEntity> proto = engine.GetProtoEntity(engine.Hashes.ToHashedString(proto_type_name), proto_id);

    if (!proto) {
        throw ScriptCallException("WASM engine API proto argument not found", proto_type_name, proto_id);
    }

    ptr<const PropertyRegistrator> actual_registrator = proto->GetProperties()->GetRegistrator();

    if (!IsWasmApiEntityTypeCompatible(proto_type_name, actual_registrator->GetTypeName().as_str())) {
        throw ScriptCallException("WASM engine API proto argument type mismatch", proto_type_name, actual_registrator->GetTypeName());
    }

    return make_ptr(const_cast<ProtoEntity*>(std::addressof(*proto)));
}

static auto ResolveWasmApiFixedTypeReceiver(BaseEngine& engine, string_view fixed_type_name, uint64_t raw_hash) -> ptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    hstring proto_id = engine.Hashes.ResolveHash(raw_hash);
    nptr<const ProtoEntity> proto = engine.GetProtoEntity(engine.Hashes.ToHashedString(fixed_type_name), proto_id);

    if (!proto) {
        throw ScriptCallException("WASM engine API fixed-type receiver not found", fixed_type_name, proto_id);
    }

    ptr<const PropertyRegistrator> actual_registrator = proto->GetProperties()->GetRegistrator();

    if (!IsWasmApiEntityTypeCompatible(fixed_type_name, actual_registrator->GetTypeName().as_str())) {
        throw ScriptCallException("WASM engine API fixed-type receiver type mismatch", fixed_type_name, actual_registrator->GetTypeName());
    }

    return make_ptr(const_cast<ProtoEntity*>(std::addressof(*proto)));
}

static auto ResolveWasmApiRefHandle(string_view ref_type_name, uint64_t raw_handle, bool nullable, string_view role) -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    if (raw_handle == 0) {
        if (nullable) {
            return nullptr;
        }

        throw ScriptCallException(strex("WASM engine API {} ref handle is null", role), ref_type_name);
    }

    return reinterpret_cast<void*>(numeric_cast<uintptr_t>(raw_handle));
}

static auto ResolveWasmApiRefReceiver(string_view ref_type_name, uint64_t raw_handle) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    return ResolveWasmApiRefHandle(ref_type_name, raw_handle, false, "receiver");
}

static auto ResolveWasmApiDynamicRefReceiver(const WasmApiPropertyDesc& desc, const_span<uint64_t> raw_args) -> ptr<DynamicRefTypeInstance>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(desc.Prop, "Missing WASM API property descriptor");

    if (raw_args.empty()) {
        throw ScriptCallException("Missing WASM API ref receiver argument", desc.EntityName);
    }

    ptr<DynamicRefTypeInstance> instance = ResolveWasmApiRefReceiver(desc.EntityName, raw_args.front()).reinterpret_as<DynamicRefTypeInstance>();

    if (instance->GetRegistrator() != desc.Prop->GetRegistrator()) {
        throw ScriptCallException("WASM engine API ref receiver type mismatch", desc.EntityName, desc.Prop->GetName());
    }

    return instance;
}

static auto ResolveWasmApiReceiver(BaseEngine& engine, const WasmApiMethodDesc& desc, const_span<uint64_t> raw_args) -> nptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    switch (desc.ReceiverKind) {
    case WasmApiReceiverKind::None: {
        if (!engine.IsValidEntityType(desc.EntityName)) {
            const auto& ref_types = engine.GetRefTypes();

            if (desc.Method != nullptr && desc.Method->Name == "__Factory" && ref_types.count(desc.EntityName) != 0) {
                return nullptr;
            }
        }

        return ResolveWasmApiGlobalEntity(engine, desc.EntityName);
    }
    case WasmApiReceiverKind::Entity:
        if (raw_args.empty()) {
            throw ScriptCallException("Missing WASM API entity receiver argument", desc.EntityName);
        }
        return ResolveWasmApiEntityReceiver(engine, desc.EntityName, raw_args.front());
    case WasmApiReceiverKind::FixedType:
        if (raw_args.empty()) {
            throw ScriptCallException("Missing WASM API fixed-type receiver argument", desc.EntityName);
        }
        return ResolveWasmApiFixedTypeReceiver(engine, desc.EntityName, raw_args.front());
    case WasmApiReceiverKind::RefType:
        throw ScriptCallException("WASM API ref receiver requires a ref call path", desc.EntityName);
    default:
        throw ScriptCallException("Unsupported WASM API receiver kind", desc.EntityName, static_cast<int32_t>(desc.ReceiverKind));
    }
}

static auto ResolveWasmApiReceiver(BaseEngine& engine, const WasmApiPropertyDesc& desc, const_span<uint64_t> raw_args) -> ptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    switch (desc.ReceiverKind) {
    case WasmApiReceiverKind::None:
        return ResolveWasmApiGlobalEntity(engine, desc.EntityName);
    case WasmApiReceiverKind::Entity:
        if (raw_args.empty()) {
            throw ScriptCallException("Missing WASM API entity receiver argument", desc.EntityName);
        }
        return ResolveWasmApiEntityReceiver(engine, desc.EntityName, raw_args.front());
    case WasmApiReceiverKind::FixedType:
        if (raw_args.empty()) {
            throw ScriptCallException("Missing WASM API fixed-type receiver argument", desc.EntityName);
        }
        return ResolveWasmApiFixedTypeReceiver(engine, desc.EntityName, raw_args.front());
    case WasmApiReceiverKind::RefType:
        throw ScriptCallException("WASM API ref property receiver requires a ref call path", desc.EntityName);
    default:
        throw ScriptCallException("Unsupported WASM API property receiver kind", desc.EntityName, static_cast<int32_t>(desc.ReceiverKind));
    }
}

struct WasmApiCallbackStorage
{
    nptr<ScriptSystem> ScriptSys {};
    string FuncName {};
    hstring HashedFuncName {};
    nptr<const ComplexTypeDesc> Type {};
    bool TemporaryToken {};
    bool Null {};
};

using WasmApiScalarStorage = variant<bool, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, float32_t, float64_t, hstring, ident_t, string, any_t, Entity*, void*, WasmApiOpaqueScalarStorage, WasmApiCallbackStorage>;

struct WasmApiArrayElement
{
    WasmApiScalarStorage Storage {};
    void* Data {};
};

using WasmApiDictArrayStorage = variant<vector<WasmApiArrayElement>, vector<bool>, vector<int8_t>, vector<int16_t>, vector<int32_t>, vector<int64_t>, vector<uint8_t>, vector<uint16_t>, vector<uint32_t>, vector<uint64_t>, vector<float32_t>, vector<float64_t>, vector<hstring>, vector<ident_t>, vector<string>, vector<any_t>, vector<Entity*>, vector<void*>>;

struct WasmApiArrayStorage
{
    BaseTypeDesc ElementType {};
    WasmScalarKind Kind {};
    vector<WasmApiArrayElement> Elements {};
};

struct WasmApiDictEntry
{
    WasmApiScalarStorage KeyStorage {};
    void* KeyData {};
    WasmApiScalarStorage ValueStorage {};
    WasmApiDictArrayStorage ValueArrayStorage {};
    void* ValueData {};
};

struct WasmApiDictStorage
{
    BaseTypeDesc KeyType {};
    WasmScalarKind KeyKind {};
    BaseTypeDesc ValueType {};
    WasmScalarKind ValueKind {};
    bool ValueIsArray {};
    vector<WasmApiDictEntry> Entries {};
};

static void AddWasmApiArrayElementFromNative(WasmApiArrayStorage& storage, ptr<void> value);
static void AddWasmApiDictElementFromNative(WasmApiDictStorage& storage, ptr<void> key, ptr<void> value);
static void AddWasmApiDictArrayElementFromNative(WasmApiDictStorage& storage, ptr<void> key, const_span<ptr<void>> values);
static auto StoreWasmApiText(const BaseTypeDesc& type, string_view text, WasmApiScalarStorage& storage) -> ptr<void>;
static auto GetWasmApiText(const BaseTypeDesc& type, const WasmApiScalarStorage& storage) -> string_view;

class WasmApiDataAccessor final : public DataAccessor
{
public:
    [[nodiscard]] auto GetBackendIndex() const noexcept -> int32_t override { return -1; }
    [[nodiscard]] auto HasUntypedNestedArrayAccess() const noexcept -> bool override { return true; }
    [[nodiscard]] auto GetArraySize(ptr<void> data) const -> size_t override { return cast_from_void<WasmApiArrayStorage*>(data.get())->Elements.size(); }
    [[nodiscard]] auto GetArrayElement(ptr<void> data, size_t index) const -> ptr<void> override { return cast_from_void<WasmApiArrayStorage*>(data.get())->Elements[index].Data; }
    [[nodiscard]] auto GetDictSize(ptr<void> data) const -> size_t override { return cast_from_void<WasmApiDictStorage*>(data.get())->Entries.size(); }
    [[nodiscard]] auto GetDictElement(ptr<void> data, size_t index) const -> pair<ptr<void>, ptr<void>> override
    {
        const WasmApiDictEntry& entry = cast_from_void<WasmApiDictStorage*>(data.get())->Entries[index];
        return {entry.KeyData, entry.ValueData};
    }
    [[nodiscard]] auto GetNestedArraySize(ptr<void> data) const -> size_t override { return cast_from_void<vector<WasmApiArrayElement>*>(data.get())->size(); }
    [[nodiscard]] auto GetNestedArrayElement(ptr<void> data, size_t index) const -> ptr<void> override { return (*cast_from_void<vector<WasmApiArrayElement>*>(data.get()))[index].Data; }
    [[nodiscard]] auto GetNestedArrayBoolElement(ptr<void> data, size_t index) const -> bool override { return *cast_from_void<bool*>((*cast_from_void<vector<WasmApiArrayElement>*>(data.get()))[index].Data); }
    [[nodiscard]] auto GetCallback(ptr<void> data) const -> unique_del_nptr<ScriptFuncDesc> override
    {
        FO_STACK_TRACE_ENTRY();

        auto callback = data.reinterpret_as<WasmApiCallbackStorage>();

        if (callback->Null) {
            return nullptr;
        }

        FO_VERIFY_AND_THROW(callback->ScriptSys, "Missing callback script system", callback->FuncName);
        FO_VERIFY_AND_THROW(callback->Type, "Missing callback type", callback->FuncName);

        nptr<ScriptFuncDesc> func = callback->TemporaryToken ? callback->ScriptSys->FindTemporaryScriptCallback(callback->FuncName, *callback->Type) : callback->ScriptSys->FindFuncDesc(callback->HashedFuncName, *callback->Type);

        if (!func) {
            throw ScriptCallException("WASM API callback not found", callback->FuncName);
        }

        return MakeBorrowedScriptFuncDesc(func.as_ptr());
    }
    void ClearArray(ptr<void> data) const override { cast_from_void<WasmApiArrayStorage*>(data.get())->Elements.clear(); }
    void AddArrayElement(ptr<void> data, ptr<void> value) const override { AddWasmApiArrayElementFromNative(*cast_from_void<WasmApiArrayStorage*>(data.get()), value); }
    void ClearDict(ptr<void> data) const override { cast_from_void<WasmApiDictStorage*>(data.get())->Entries.clear(); }
    void AddDictElement(ptr<void> data, ptr<void> key, ptr<void> value) const override { AddWasmApiDictElementFromNative(*cast_from_void<WasmApiDictStorage*>(data.get()), key, value); }
    void AddDictArrayElement(ptr<void> data, ptr<void> key, const_span<ptr<void>> values) const override { AddWasmApiDictArrayElementFromNative(*cast_from_void<WasmApiDictStorage*>(data.get()), key, values); }
};

static const WasmApiDataAccessor WASM_API_DATA_ACCESSOR;

static auto WasmScalarKindToNativeSignatureChar(WasmScalarKind kind) -> char
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (kind) {
    case WasmScalarKind::I32:
        return 'i';
    case WasmScalarKind::I64:
        return 'I';
    case WasmScalarKind::F32:
        return 'f';
    case WasmScalarKind::F64:
        return 'F';
    default:
        throw ScriptSystemException("Unsupported WASM scalar kind for native signature", static_cast<int32_t>(kind));
    }
}

static void AppendWasmApiNativeSignatureParam(string& signature, WasmScalarKind kind, WasmApiParamAbiKind param_abi)
{
    FO_STACK_TRACE_ENTRY();

    switch (param_abi) {
    case WasmApiParamAbiKind::Scalar:
        signature += WasmScalarKindToNativeSignatureChar(kind);
        break;
    case WasmApiParamAbiKind::Utf8StringPointer:
    case WasmApiParamAbiKind::Utf8StringOutputPointer:
    case WasmApiParamAbiKind::MutableValuePointer:
    case WasmApiParamAbiKind::ValuePointer:
    case WasmApiParamAbiKind::ValueOutputPointer:
    case WasmApiParamAbiKind::ArrayPointer:
    case WasmApiParamAbiKind::ArrayOutputPointer:
    case WasmApiParamAbiKind::MutableArrayPointer:
    case WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer:
    case WasmApiParamAbiKind::DictPointer:
    case WasmApiParamAbiKind::DictOutputPointer:
    case WasmApiParamAbiKind::MutableDictPointer:
    case WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer:
    case WasmApiParamAbiKind::CallbackPointer:
    case WasmApiParamAbiKind::MutableUtf8StringPointer:
    case WasmApiParamAbiKind::MutableUtf8StringRequiredByteLengthPointer:
        signature += '*';
        break;
    case WasmApiParamAbiKind::Utf8StringLength:
    case WasmApiParamAbiKind::Utf8StringOutputLength:
    case WasmApiParamAbiKind::MutableValueLength:
    case WasmApiParamAbiKind::ValueByteLength:
    case WasmApiParamAbiKind::ValueOutputByteLength:
    case WasmApiParamAbiKind::ArrayByteLength:
    case WasmApiParamAbiKind::ArrayOutputByteLength:
    case WasmApiParamAbiKind::MutableArrayByteLength:
    case WasmApiParamAbiKind::MutableArrayCapacityByteLength:
    case WasmApiParamAbiKind::DictByteLength:
    case WasmApiParamAbiKind::DictOutputByteLength:
    case WasmApiParamAbiKind::MutableDictByteLength:
    case WasmApiParamAbiKind::MutableDictCapacityByteLength:
    case WasmApiParamAbiKind::CallbackLength:
    case WasmApiParamAbiKind::MutableUtf8StringByteLength:
    case WasmApiParamAbiKind::MutableUtf8StringCapacityByteLength:
        signature += '~';
        break;
    default:
        throw ScriptSystemException("Unsupported WASM API parameter ABI for native signature", static_cast<int32_t>(param_abi));
    }
}

static void StoreWasmApiRawI32ScalarValue(const BaseTypeDesc& type, uint64_t raw_value, span<uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    int32_t signed_value = std::bit_cast<int32_t>(numeric_cast<uint32_t>(raw_value));

    if (type.IsBool) {
        bool value = signed_value != 0;
        FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
        MemCopy(raw_data.data(), &value, sizeof(value));
        return;
    }
    if (type.IsInt8) {
        int8_t value = numeric_cast<int8_t>(signed_value);
        FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
        MemCopy(raw_data.data(), &value, sizeof(value));
        return;
    }
    if (type.IsInt16) {
        int16_t value = numeric_cast<int16_t>(signed_value);
        FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
        MemCopy(raw_data.data(), &value, sizeof(value));
        return;
    }
    if (type.IsInt32) {
        int32_t value = signed_value;
        FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
        MemCopy(raw_data.data(), &value, sizeof(value));
        return;
    }
    if (type.IsUInt8) {
        uint8_t value = numeric_cast<uint8_t>(signed_value);
        FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
        MemCopy(raw_data.data(), &value, sizeof(value));
        return;
    }
    if (type.IsUInt16) {
        uint16_t value = numeric_cast<uint16_t>(signed_value);
        FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
        MemCopy(raw_data.data(), &value, sizeof(value));
        return;
    }
    if (type.IsUInt32) {
        uint32_t value = std::bit_cast<uint32_t>(signed_value);
        FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
        MemCopy(raw_data.data(), &value, sizeof(value));
        return;
    }

    throw ScriptCallException("Unsupported WASM API raw i32 scalar type", type.Name);
}

static void StoreWasmApiRawScalarValue(BaseEngine* engine, const BaseTypeDesc& type, WasmScalarKind kind, uint64_t raw_value, span<uint8_t> raw_data, bool method_data)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsEnum) {
        FO_STRONG_ASSERT(type.EnumUnderlyingType != nullptr, "WASM API bridge invariant failed");
        StoreWasmApiRawScalarValue(engine, *type.EnumUnderlyingType, kind, raw_value, raw_data, method_data);
        return;
    }
    if (type.IsSimpleStruct) {
        FO_STRONG_ASSERT(type.StructLayout != nullptr, "WASM API bridge invariant failed");
        FO_STRONG_ASSERT(type.StructLayout->Fields.size() == 1, "WASM API bridge invariant failed");

        const FieldDesc& field = type.StructLayout->Fields.front();
        FO_STRONG_ASSERT(field.Offset + field.Type.Size <= raw_data.size(), "WASM API bridge invariant failed");
        StoreWasmApiRawScalarValue(engine, field.Type, kind, raw_value, raw_data.subspan(field.Offset, field.Type.Size), method_data);
        return;
    }
    if (IsWasmApiPackedStructType(type)) {
        switch (kind) {
        case WasmScalarKind::I32: {
            uint32_t value = numeric_cast<uint32_t>(raw_value);
            FO_STRONG_ASSERT(raw_data.size() == type.Size, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(raw_data.size() <= sizeof(value), "WASM API bridge invariant failed");
            MemCopy(raw_data.data(), &value, raw_data.size());
            return;
        }
        case WasmScalarKind::I64: {
            uint64_t value = raw_value;
            FO_STRONG_ASSERT(raw_data.size() == type.Size, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(raw_data.size() <= sizeof(value), "WASM API bridge invariant failed");
            MemCopy(raw_data.data(), &value, raw_data.size());
            return;
        }
        default:
            break;
        }
    }

    switch (kind) {
    case WasmScalarKind::I32:
        StoreWasmApiRawI32ScalarValue(type, raw_value, raw_data);
        return;
    case WasmScalarKind::I64:
        if (type.IsInt64) {
            int64_t value = std::bit_cast<int64_t>(raw_value);
            FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
            MemCopy(raw_data.data(), &value, sizeof(value));
            return;
        }
        if (type.IsUInt64) {
            uint64_t value = raw_value;
            FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
            MemCopy(raw_data.data(), &value, sizeof(value));
            return;
        }
        if (type.IsHashedString) {
            if (method_data) {
                FO_STRONG_ASSERT(engine != nullptr, "WASM API bridge invariant failed");

                hstring value = engine->Hashes.ResolveHash(raw_value);
                FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
                MemCopy(raw_data.data(), &value, sizeof(value));
            }
            else {
                hstring::hash_t value = raw_value;
                FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
                MemCopy(raw_data.data(), &value, sizeof(value));
            }
            return;
        }
        break;
    case WasmScalarKind::F32:
        if (type.IsSingleFloat) {
            float32_t value = std::bit_cast<float32_t>(numeric_cast<uint32_t>(raw_value));
            FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
            MemCopy(raw_data.data(), &value, sizeof(value));
            return;
        }
        break;
    case WasmScalarKind::F64:
        if (type.IsDoubleFloat) {
            float64_t value = std::bit_cast<float64_t>(raw_value);
            FO_STRONG_ASSERT(raw_data.size() == sizeof(value), "WASM API bridge invariant failed");
            MemCopy(raw_data.data(), &value, sizeof(value));
            return;
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API raw scalar type", type.Name, static_cast<int32_t>(kind));
}

static auto PackWasmApiRawI32ScalarValue(const BaseTypeDesc& type, span<const uint8_t> raw_data) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsBool) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(bool), "WASM API bridge invariant failed");
        bool value = false;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return value ? 1 : 0;
    }
    if (type.IsInt8) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(int8_t), "WASM API bridge invariant failed");
        int8_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return std::bit_cast<uint32_t>(numeric_cast<int32_t>(value));
    }
    if (type.IsInt16) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(int16_t), "WASM API bridge invariant failed");
        int16_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return std::bit_cast<uint32_t>(numeric_cast<int32_t>(value));
    }
    if (type.IsInt32) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(int32_t), "WASM API bridge invariant failed");
        int32_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return std::bit_cast<uint32_t>(value);
    }
    if (type.IsUInt8) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(uint8_t), "WASM API bridge invariant failed");
        uint8_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return numeric_cast<uint32_t>(value);
    }
    if (type.IsUInt16) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(uint16_t), "WASM API bridge invariant failed");
        uint16_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return numeric_cast<uint32_t>(value);
    }
    if (type.IsUInt32) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(uint32_t), "WASM API bridge invariant failed");
        uint32_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return value;
    }

    throw ScriptCallException("Unsupported WASM API raw i32 scalar type", type.Name);
}

static auto PackWasmApiRawScalarValue(const BaseTypeDesc& type, span<const uint8_t> raw_data, WasmScalarKind kind, bool method_data) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsEnum) {
        FO_STRONG_ASSERT(type.EnumUnderlyingType != nullptr, "WASM API bridge invariant failed");
        return PackWasmApiRawScalarValue(*type.EnumUnderlyingType, raw_data, kind, method_data);
    }
    if (type.IsSimpleStruct) {
        FO_STRONG_ASSERT(type.StructLayout != nullptr, "WASM API bridge invariant failed");
        FO_STRONG_ASSERT(type.StructLayout->Fields.size() == 1, "WASM API bridge invariant failed");

        const FieldDesc& field = type.StructLayout->Fields.front();
        FO_STRONG_ASSERT(field.Offset + field.Type.Size <= raw_data.size(), "WASM API bridge invariant failed");
        return PackWasmApiRawScalarValue(field.Type, raw_data.subspan(field.Offset, field.Type.Size), kind, method_data);
    }
    if (IsWasmApiPackedStructType(type)) {
        switch (kind) {
        case WasmScalarKind::I32: {
            uint32_t value = 0;
            FO_STRONG_ASSERT(raw_data.size() == type.Size, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(raw_data.size() <= sizeof(value), "WASM API bridge invariant failed");
            MemCopy(&value, raw_data.data(), raw_data.size());
            return value;
        }
        case WasmScalarKind::I64: {
            uint64_t value = 0;
            FO_STRONG_ASSERT(raw_data.size() == type.Size, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(raw_data.size() <= sizeof(value), "WASM API bridge invariant failed");
            MemCopy(&value, raw_data.data(), raw_data.size());
            return value;
        }
        default:
            break;
        }
    }

    switch (kind) {
    case WasmScalarKind::I32:
        return PackWasmApiRawI32ScalarValue(type, raw_data);
    case WasmScalarKind::I64:
        if (type.IsInt64) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(int64_t), "WASM API bridge invariant failed");
            int64_t value = 0;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return std::bit_cast<uint64_t>(value);
        }
        if (type.IsUInt64) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(uint64_t), "WASM API bridge invariant failed");
            uint64_t value = 0;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return value;
        }
        if (type.IsHashedString) {
            if (method_data) {
                FO_STRONG_ASSERT(raw_data.size() == sizeof(hstring), "WASM API bridge invariant failed");
                hstring value {};
                MemCopy(&value, raw_data.data(), sizeof(value));
                return value.as_hash();
            }

            FO_STRONG_ASSERT(raw_data.size() == sizeof(hstring::hash_t), "WASM API bridge invariant failed");
            hstring::hash_t value = 0;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return value;
        }
        break;
    case WasmScalarKind::F32:
        if (type.IsSingleFloat) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(float32_t), "WASM API bridge invariant failed");
            float32_t value = 0.0f;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return std::bit_cast<uint32_t>(value);
        }
        break;
    case WasmScalarKind::F64:
        if (type.IsDoubleFloat) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(float64_t), "WASM API bridge invariant failed");
            float64_t value = 0.0;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return std::bit_cast<uint64_t>(value);
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API raw scalar type", type.Name, static_cast<int32_t>(kind));
}

static auto StoreWasmApiI32Scalar(const BaseTypeDesc& type, uint64_t raw_value, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    int32_t signed_value = std::bit_cast<int32_t>(numeric_cast<uint32_t>(raw_value));

    if (type.IsBool) {
        storage.emplace<bool>(signed_value != 0);
        return make_nptr(&std::get<bool>(storage)).void_cast();
    }
    if (type.IsInt8) {
        storage.emplace<int8_t>(numeric_cast<int8_t>(signed_value));
        return make_nptr(&std::get<int8_t>(storage)).void_cast();
    }
    if (type.IsInt16) {
        storage.emplace<int16_t>(numeric_cast<int16_t>(signed_value));
        return make_nptr(&std::get<int16_t>(storage)).void_cast();
    }
    if (type.IsInt32) {
        storage.emplace<int32_t>(signed_value);
        return make_nptr(&std::get<int32_t>(storage)).void_cast();
    }
    if (type.IsUInt8) {
        storage.emplace<uint8_t>(numeric_cast<uint8_t>(signed_value));
        return make_nptr(&std::get<uint8_t>(storage)).void_cast();
    }
    if (type.IsUInt16) {
        storage.emplace<uint16_t>(numeric_cast<uint16_t>(signed_value));
        return make_nptr(&std::get<uint16_t>(storage)).void_cast();
    }
    if (type.IsUInt32) {
        storage.emplace<uint32_t>(std::bit_cast<uint32_t>(signed_value));
        return make_nptr(&std::get<uint32_t>(storage)).void_cast();
    }

    throw ScriptCallException("Unsupported WASM API i32 scalar type", type.Name);
}

static auto StoreWasmApiOpaqueScalar(BaseEngine& engine, const BaseTypeDesc& type, WasmScalarKind kind, uint64_t raw_value, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    WasmApiOpaqueScalarStorage& opaque = storage.emplace<WasmApiOpaqueScalarStorage>();
    FO_STRONG_ASSERT(type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");

    StoreWasmApiRawScalarValue(&engine, type, kind, raw_value, span<uint8_t> {opaque.Data.data(), type.Size}, true);

    return make_nptr(opaque.Data.data()).void_cast();
}

static auto StoreWasmApiScalar(BaseEngine& engine, const ArgDesc& arg, WasmScalarKind kind, uint64_t raw_value, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    const ComplexTypeDesc& type = arg.Type;
    FO_STRONG_ASSERT(type.Kind == ComplexTypeKind::Simple, "WASM API bridge invariant failed");

    const BaseTypeDesc& base_type = type.BaseType;

    if (kind == WASM_ENTITY_HANDLE_KIND && IsWasmApiEntityHandleType(base_type)) {
        storage.emplace<Entity*>(ResolveWasmApiEntityHandle(engine, base_type.Name, raw_value, arg.Nullable, "argument").get());
        return make_nptr(&std::get<Entity*>(storage)).void_cast();
    }
    if (kind == WasmScalarKind::I64 && IsWasmApiProtoOrFixedHandleType(base_type)) {
        storage.emplace<Entity*>(ResolveWasmApiProtoHandle(engine, base_type.Name, raw_value, arg.Nullable).get());
        return make_nptr(&std::get<Entity*>(storage)).void_cast();
    }
    if (kind == WASM_REF_TYPE_HANDLE_KIND && IsWasmApiRefHandleType(base_type)) {
        storage.emplace<void*>(ResolveWasmApiRefHandle(base_type.Name, raw_value, arg.Nullable, "argument").get());
        return static_cast<void*>(&std::get<void*>(storage));
    }
    if (IsWasmApiOpaqueScalarType(base_type)) {
        return StoreWasmApiOpaqueScalar(engine, base_type, kind, raw_value, storage);
    }

    switch (kind) {
    case WasmScalarKind::I32:
        return StoreWasmApiI32Scalar(base_type, raw_value, storage);
    case WasmScalarKind::I64:
        if (base_type.IsInt64) {
            storage.emplace<int64_t>(std::bit_cast<int64_t>(raw_value));
            return make_nptr(&std::get<int64_t>(storage)).void_cast();
        }
        if (base_type.IsUInt64) {
            storage.emplace<uint64_t>(raw_value);
            return make_nptr(&std::get<uint64_t>(storage)).void_cast();
        }
        if (base_type.IsHashedString) {
            storage.emplace<hstring>(engine.Hashes.ResolveHash(raw_value));
            return make_nptr(&std::get<hstring>(storage)).void_cast();
        }
        if (IsWasmApiIdentType(base_type)) {
            storage.emplace<ident_t>(MakeWasmApiIdent(raw_value));
            return make_nptr(&std::get<ident_t>(storage)).void_cast();
        }
        break;
    case WasmScalarKind::F32:
        if (base_type.IsSingleFloat) {
            storage.emplace<float32_t>(std::bit_cast<float32_t>(numeric_cast<uint32_t>(raw_value)));
            return make_nptr(&std::get<float32_t>(storage)).void_cast();
        }
        break;
    case WasmScalarKind::F64:
        if (base_type.IsDoubleFloat) {
            storage.emplace<float64_t>(std::bit_cast<float64_t>(raw_value));
            return make_nptr(&std::get<float64_t>(storage)).void_cast();
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API scalar type", base_type.Name, static_cast<int32_t>(kind));
}

static auto StoreWasmApiString(const BaseTypeDesc& type, uint64_t raw_address, uint64_t raw_size, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiTextBaseType(type), "WASM API bridge invariant failed");

    const char* str = reinterpret_cast<const char*>(raw_address);
    int32_t size = std::bit_cast<int32_t>(numeric_cast<uint32_t>(raw_size));

    if (size < 0) {
        throw ScriptCallException("Negative WASM API string length", size);
    }
    if (str == nullptr && size != 0) {
        throw ScriptCallException("Null WASM API string pointer", size);
    }

    return StoreWasmApiText(type, str != nullptr ? string_view {str, numeric_cast<size_t>(size)} : string_view {}, storage).get();
}

static auto StoreWasmApiCallback(BaseEngine& engine, const ArgDesc& arg, uint64_t raw_address, uint64_t raw_size, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiCallbackType(arg.Type), "WASM API bridge invariant failed");

    const char* str = reinterpret_cast<const char*>(raw_address);
    int32_t size = std::bit_cast<int32_t>(numeric_cast<uint32_t>(raw_size));

    if (size < 0) {
        throw ScriptCallException("Negative WASM API callback name length", size);
    }
    if (str == nullptr && size != 0) {
        throw ScriptCallException("Null WASM API callback name pointer", size);
    }

    WasmApiCallbackStorage& callback = storage.emplace<WasmApiCallbackStorage>();
    callback.ScriptSys = &engine;
    callback.Type = &arg.Type;
    callback.Null = str == nullptr && size == 0 && arg.Nullable;

    if (!callback.Null) {
        callback.FuncName = string {str != nullptr ? string_view {str, numeric_cast<size_t>(size)} : string_view {}};

        if (callback.FuncName.empty()) {
            throw ScriptCallException("Empty WASM API callback name", arg.Name);
        }

        callback.TemporaryToken = ScriptSystem::IsTemporaryScriptCallbackToken(callback.FuncName);

        if (!callback.TemporaryToken) {
            callback.HashedFuncName = engine.Hashes.ToHashedString(callback.FuncName);
        }
    }

    return make_nptr(&callback).void_cast();
}

static auto CopyWasmApiStringOutput(string_view text, uint64_t raw_address, uint64_t raw_size) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    char* str = reinterpret_cast<char*>(raw_address);
    int32_t size = std::bit_cast<int32_t>(numeric_cast<uint32_t>(raw_size));

    if (size < 0) {
        throw ScriptCallException("Negative WASM API output string buffer length", size);
    }
    if (str == nullptr && size != 0) {
        throw ScriptCallException("Null WASM API output string buffer pointer", size);
    }

    size_t output_size = numeric_cast<size_t>(size);
    size_t copy_size = std::min(text.size(), output_size);
    int32_t text_size = numeric_cast<int32_t>(text.size());

    if (copy_size != 0) {
        MemCopy(str, text.data(), copy_size);
    }

    return numeric_cast<uint32_t>(text_size);
}

static auto GetWasmApiBuffer(uint64_t raw_address, uint64_t raw_size, string_view role) -> span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    uint8_t* data = reinterpret_cast<uint8_t*>(raw_address);
    int32_t size = std::bit_cast<int32_t>(numeric_cast<uint32_t>(raw_size));

    if (size < 0) {
        throw ScriptCallException(strex("Negative WASM API {} buffer length", role), size);
    }
    if (data == nullptr && size != 0) {
        throw ScriptCallException(strex("Null WASM API {} buffer pointer", role), size);
    }

    return {data, numeric_cast<size_t>(size)};
}

static auto StoreWasmApiText(const BaseTypeDesc& type, string_view text, WasmApiScalarStorage& storage) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiTextBaseType(type), "WASM API bridge invariant failed");

    if (IsWasmApiAnyBaseType(type)) {
        storage.emplace<any_t>(any_t {string {text}});
        return make_ptr(&std::get<any_t>(storage)).void_cast();
    }

    storage.emplace<string>(string {text});
    return make_ptr(&std::get<string>(storage)).void_cast();
}

static auto GetWasmApiText(const BaseTypeDesc& type, const WasmApiScalarStorage& storage) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiTextBaseType(type), "WASM API bridge invariant failed");

    if (IsWasmApiAnyBaseType(type)) {
        return std::get<any_t>(storage);
    }

    return std::get<string>(storage);
}

static auto PrepareWasmApiTextReturnStorage(const BaseTypeDesc& type, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiTextBaseType(type), "WASM API bridge invariant failed");

    if (IsWasmApiAnyBaseType(type)) {
        storage.emplace<any_t>();
        return make_nptr(&std::get<any_t>(storage)).void_cast();
    }

    storage.emplace<string>();
    return make_nptr(&std::get<string>(storage)).void_cast();
}

static auto ReadWasmApiU64(span<const uint8_t> raw_data, string_view role) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    if (raw_data.size() != sizeof(uint64_t)) {
        throw ScriptCallException(strex("Invalid WASM API {} handle size", role), raw_data.size());
    }

    uint64_t value = 0;
    MemCopy(&value, raw_data.data(), sizeof(value));
    return value;
}

static void WriteWasmApiU64(uint64_t value, span<uint8_t> raw_data, string_view role)
{
    FO_STACK_TRACE_ENTRY();

    if (raw_data.size() != sizeof(value)) {
        throw ScriptCallException(strex("Invalid WASM API {} handle output size", role), raw_data.size());
    }

    MemCopy(raw_data.data(), &value, sizeof(value));
}

static auto ReadWasmApiU32(span<const uint8_t> raw_data, string_view role) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    if (raw_data.size() != sizeof(uint32_t)) {
        throw ScriptCallException(strex("Invalid WASM API {} u32 size", role), raw_data.size());
    }

    uint32_t value = 0;
    MemCopy(&value, raw_data.data(), sizeof(value));
    return value;
}

static void WriteWasmApiU32(uint32_t value, span<uint8_t> raw_data, string_view role)
{
    FO_STACK_TRACE_ENTRY();

    if (raw_data.size() != sizeof(value)) {
        throw ScriptCallException(strex("Invalid WASM API {} u32 output size", role), raw_data.size());
    }

    MemCopy(raw_data.data(), &value, sizeof(value));
}

static void CopyWasmApiWireToNativeScalar(BaseEngine& engine, const BaseTypeDesc& type, span<const uint8_t> wire_data, span<uint8_t> native_data)
{
    FO_STACK_TRACE_ENTRY();

    if (wire_data.size() != type.Size || native_data.size() != type.Size) {
        throw ScriptCallException("Invalid WASM API array element size", type.Name, wire_data.size(), native_data.size(), type.Size);
    }

    if (type.IsHashedString) {
        hstring value = engine.Hashes.ResolveHash(ReadWasmApiU64(wire_data, "array hstring"));
        MemCopy(native_data.data(), &value, sizeof(value));
        return;
    }
    if (type.StructLayout != nullptr) {
        for (const FieldDesc& field : type.StructLayout->Fields) {
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= wire_data.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= native_data.size(), "WASM API bridge invariant failed");
            CopyWasmApiWireToNativeScalar(engine, field.Type, wire_data.subspan(field.Offset, field.Type.Size), native_data.subspan(field.Offset, field.Type.Size));
        }
        return;
    }

    MemCopy(native_data.data(), wire_data.data(), type.Size);
}

static void CopyWasmApiNativeScalarToWire(const BaseTypeDesc& type, span<const uint8_t> native_data, span<uint8_t> wire_data)
{
    FO_STACK_TRACE_ENTRY();

    if (wire_data.size() != type.Size || native_data.size() != type.Size) {
        throw ScriptCallException("Invalid WASM API array element output size", type.Name, native_data.size(), wire_data.size(), type.Size);
    }

    if (type.IsHashedString) {
        hstring value;
        MemCopy(&value, native_data.data(), sizeof(value));
        WriteWasmApiU64(value.as_hash(), wire_data, "array hstring");
        return;
    }
    if (type.StructLayout != nullptr) {
        for (const FieldDesc& field : type.StructLayout->Fields) {
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= native_data.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= wire_data.size(), "WASM API bridge invariant failed");
            CopyWasmApiNativeScalarToWire(field.Type, native_data.subspan(field.Offset, field.Type.Size), wire_data.subspan(field.Offset, field.Type.Size));
        }
        return;
    }

    MemCopy(wire_data.data(), native_data.data(), type.Size);
}

static auto GetWasmApiArrayElementWireSize(const BaseTypeDesc& type) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmApiEntityHandleType(type) || IsWasmApiProtoOrFixedHandleType(type) || IsWasmApiRefHandleType(type) || type.IsHashedString) {
        return sizeof(uint64_t);
    }

    if (type.Size == 0) {
        throw ScriptCallException("WASM API array element has zero size", type.Name);
    }

    return type.Size;
}

template<typename T>
static auto StoreWasmApiArrayPlainElement(span<const uint8_t> raw_data, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(raw_data.size() == sizeof(T), "WASM API bridge invariant failed");

    T value {};
    MemCopy(&value, raw_data.data(), sizeof(value));
    storage.emplace<T>(value);

    return make_nptr(&std::get<T>(storage)).void_cast();
}

template<typename T>
static auto StoreWasmApiArrayPlainElementFromNative(const void* value, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    storage.emplace<T>(*cast_from_void<const T*>(value));
    return make_nptr(&std::get<T>(storage)).void_cast();
}

template<typename T>
static void CopyWasmApiArrayPlainElementToBytes(const WasmApiScalarStorage& storage, span<uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(raw_data.size() == sizeof(T), "WASM API bridge invariant failed");

    const T& value = std::get<T>(storage);
    MemCopy(raw_data.data(), &value, sizeof(value));
}

static auto StoreWasmApiArrayOpaqueElementFromNative(const BaseTypeDesc& type, const void* value, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    WasmApiOpaqueScalarStorage& opaque = storage.emplace<WasmApiOpaqueScalarStorage>();
    FO_STRONG_ASSERT(type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");

    MemCopy(opaque.Data.data(), value, type.Size);
    return make_nptr(opaque.Data.data()).void_cast();
}

static auto StoreWasmApiArrayElementFromWire(BaseEngine& engine, const BaseTypeDesc& type, WasmScalarKind kind, span<const uint8_t> raw_data, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmApiEntityHandleType(type)) {
        uint64_t raw_id = ReadWasmApiU64(raw_data, "array entity");
        storage.emplace<Entity*>(ResolveWasmApiEntityHandle(engine, type.Name, raw_id, false, "array argument").get());
        return make_nptr(&std::get<Entity*>(storage)).void_cast();
    }
    if (IsWasmApiProtoOrFixedHandleType(type)) {
        uint64_t raw_hash = ReadWasmApiU64(raw_data, "array proto");
        storage.emplace<Entity*>(ResolveWasmApiProtoHandle(engine, type.Name, raw_hash, false).get());
        return make_nptr(&std::get<Entity*>(storage)).void_cast();
    }
    if (IsWasmApiRefHandleType(type)) {
        uint64_t raw_handle = ReadWasmApiU64(raw_data, "array ref");
        storage.emplace<void*>(ResolveWasmApiRefHandle(type.Name, raw_handle, false, "array argument").get());
        return static_cast<void*>(&std::get<void*>(storage));
    }
    if (type.IsHashedString) {
        uint64_t raw_hash = ReadWasmApiU64(raw_data, "array hstring");
        storage.emplace<hstring>(engine.Hashes.ResolveHash(raw_hash));
        return make_nptr(&std::get<hstring>(storage)).void_cast();
    }
    if (IsWasmApiIdentType(type)) {
        ident_t value {};
        MemCopy(&value, raw_data.data(), sizeof(value));
        storage.emplace<ident_t>(value);
        return make_nptr(&std::get<ident_t>(storage)).void_cast();
    }
    if (IsWasmApiOpaqueScalarType(type)) {
        WasmApiOpaqueScalarStorage& opaque = storage.emplace<WasmApiOpaqueScalarStorage>();
        FO_STRONG_ASSERT(type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");
        ignore_unused(kind);

        CopyWasmApiWireToNativeScalar(engine, type, raw_data, span<uint8_t> {opaque.Data.data(), type.Size});
        return make_nptr(opaque.Data.data()).void_cast();
    }
    if (IsWasmApiBufferElementType(type)) {
        WasmApiOpaqueScalarStorage& opaque = storage.emplace<WasmApiOpaqueScalarStorage>();
        FO_STRONG_ASSERT(type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");
        ignore_unused(kind);

        CopyWasmApiWireToNativeScalar(engine, type, raw_data, span<uint8_t> {opaque.Data.data(), type.Size});
        return make_nptr(opaque.Data.data()).void_cast();
    }

    switch (kind) {
    case WasmScalarKind::I32:
        if (type.IsBool) {
            return StoreWasmApiArrayPlainElement<bool>(raw_data, storage);
        }
        if (type.IsInt8) {
            return StoreWasmApiArrayPlainElement<int8_t>(raw_data, storage);
        }
        if (type.IsInt16) {
            return StoreWasmApiArrayPlainElement<int16_t>(raw_data, storage);
        }
        if (type.IsInt32) {
            return StoreWasmApiArrayPlainElement<int32_t>(raw_data, storage);
        }
        if (type.IsUInt8) {
            return StoreWasmApiArrayPlainElement<uint8_t>(raw_data, storage);
        }
        if (type.IsUInt16) {
            return StoreWasmApiArrayPlainElement<uint16_t>(raw_data, storage);
        }
        if (type.IsUInt32) {
            return StoreWasmApiArrayPlainElement<uint32_t>(raw_data, storage);
        }
        break;
    case WasmScalarKind::I64:
        if (type.IsInt64) {
            return StoreWasmApiArrayPlainElement<int64_t>(raw_data, storage);
        }
        if (type.IsUInt64) {
            return StoreWasmApiArrayPlainElement<uint64_t>(raw_data, storage);
        }
        break;
    case WasmScalarKind::F32:
        if (type.IsSingleFloat) {
            return StoreWasmApiArrayPlainElement<float32_t>(raw_data, storage);
        }
        break;
    case WasmScalarKind::F64:
        if (type.IsDoubleFloat) {
            return StoreWasmApiArrayPlainElement<float64_t>(raw_data, storage);
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API array element type", type.Name, static_cast<int32_t>(kind));
}

static auto StoreWasmApiArrayElementFromNative(const BaseTypeDesc& type, WasmScalarKind kind, const void* value, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmApiEntityHandleType(type) || IsWasmApiProtoOrFixedHandleType(type)) {
        storage.emplace<Entity*>(*cast_from_void<Entity* const*>(value));
        return make_nptr(&std::get<Entity*>(storage)).void_cast();
    }
    if (IsWasmApiRefHandleType(type)) {
        const void* const* ref = static_cast<const void* const*>(value);
        storage.emplace<void*>(const_cast<void*>(*ref));
        return static_cast<void*>(&std::get<void*>(storage));
    }
    if (IsWasmApiTextBaseType(type)) {
        if (IsWasmApiAnyBaseType(type)) {
            storage.emplace<any_t>(*cast_from_void<const any_t*>(value));
            return make_nptr(&std::get<any_t>(storage)).void_cast();
        }

        storage.emplace<string>(*cast_from_void<const string*>(value));
        return make_nptr(&std::get<string>(storage)).void_cast();
    }
    if (type.IsHashedString) {
        storage.emplace<hstring>(*cast_from_void<const hstring*>(value));
        return make_nptr(&std::get<hstring>(storage)).void_cast();
    }
    if (IsWasmApiIdentType(type)) {
        storage.emplace<ident_t>(*cast_from_void<const ident_t*>(value));
        return make_nptr(&std::get<ident_t>(storage)).void_cast();
    }
    if (IsWasmApiOpaqueScalarType(type)) {
        return StoreWasmApiArrayOpaqueElementFromNative(type, value, storage);
    }
    if (IsWasmApiBufferElementType(type)) {
        ignore_unused(kind);
        return StoreWasmApiArrayOpaqueElementFromNative(type, value, storage);
    }

    switch (kind) {
    case WasmScalarKind::I32:
        if (type.IsBool) {
            return StoreWasmApiArrayPlainElementFromNative<bool>(value, storage);
        }
        if (type.IsInt8) {
            return StoreWasmApiArrayPlainElementFromNative<int8_t>(value, storage);
        }
        if (type.IsInt16) {
            return StoreWasmApiArrayPlainElementFromNative<int16_t>(value, storage);
        }
        if (type.IsInt32) {
            return StoreWasmApiArrayPlainElementFromNative<int32_t>(value, storage);
        }
        if (type.IsUInt8) {
            return StoreWasmApiArrayPlainElementFromNative<uint8_t>(value, storage);
        }
        if (type.IsUInt16) {
            return StoreWasmApiArrayPlainElementFromNative<uint16_t>(value, storage);
        }
        if (type.IsUInt32) {
            return StoreWasmApiArrayPlainElementFromNative<uint32_t>(value, storage);
        }
        break;
    case WasmScalarKind::I64:
        if (type.IsInt64) {
            return StoreWasmApiArrayPlainElementFromNative<int64_t>(value, storage);
        }
        if (type.IsUInt64) {
            return StoreWasmApiArrayPlainElementFromNative<uint64_t>(value, storage);
        }
        break;
    case WasmScalarKind::F32:
        if (type.IsSingleFloat) {
            return StoreWasmApiArrayPlainElementFromNative<float32_t>(value, storage);
        }
        break;
    case WasmScalarKind::F64:
        if (type.IsDoubleFloat) {
            return StoreWasmApiArrayPlainElementFromNative<float64_t>(value, storage);
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API array element type", type.Name, static_cast<int32_t>(kind));
}

static void AddWasmApiArrayElementFromNative(WasmApiArrayStorage& storage, ptr<void> value)
{
    FO_STACK_TRACE_ENTRY();

    WasmApiArrayElement& element = storage.Elements.emplace_back();
    element.Data = StoreWasmApiArrayElementFromNative(storage.ElementType, storage.Kind, value.get(), element.Storage);
}

template<typename T>
static auto StoreWasmApiDictArrayFromNative(const void* value, WasmApiDictArrayStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    storage.emplace<vector<T>>(*cast_from_void<const vector<T>*>(value));
    return make_nptr(&std::get<vector<T>>(storage)).void_cast();
}

static auto StoreWasmApiDictArrayFromNative(const BaseTypeDesc& type, WasmScalarKind kind, const void* value, WasmApiDictArrayStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmApiTextBaseType(type)) {
        if (IsWasmApiAnyBaseType(type)) {
            return StoreWasmApiDictArrayFromNative<any_t>(value, storage);
        }

        return StoreWasmApiDictArrayFromNative<string>(value, storage);
    }
    if (type.IsHashedString) {
        return StoreWasmApiDictArrayFromNative<hstring>(value, storage);
    }
    if (IsWasmApiIdentType(type)) {
        return StoreWasmApiDictArrayFromNative<ident_t>(value, storage);
    }
    if (IsWasmApiEntityHandleType(type) || IsWasmApiProtoOrFixedHandleType(type)) {
        return StoreWasmApiDictArrayFromNative<Entity*>(value, storage);
    }
    if (IsWasmApiRefHandleType(type)) {
        return StoreWasmApiDictArrayFromNative<void*>(value, storage);
    }

    switch (kind) {
    case WasmScalarKind::I32:
        if (type.IsBool) {
            return StoreWasmApiDictArrayFromNative<bool>(value, storage);
        }
        if (type.IsInt8) {
            return StoreWasmApiDictArrayFromNative<int8_t>(value, storage);
        }
        if (type.IsInt16) {
            return StoreWasmApiDictArrayFromNative<int16_t>(value, storage);
        }
        if (type.IsInt32) {
            return StoreWasmApiDictArrayFromNative<int32_t>(value, storage);
        }
        if (type.IsUInt8) {
            return StoreWasmApiDictArrayFromNative<uint8_t>(value, storage);
        }
        if (type.IsUInt16) {
            return StoreWasmApiDictArrayFromNative<uint16_t>(value, storage);
        }
        if (type.IsUInt32) {
            return StoreWasmApiDictArrayFromNative<uint32_t>(value, storage);
        }
        break;
    case WasmScalarKind::I64:
        if (type.IsInt64) {
            return StoreWasmApiDictArrayFromNative<int64_t>(value, storage);
        }
        if (type.IsUInt64) {
            return StoreWasmApiDictArrayFromNative<uint64_t>(value, storage);
        }
        break;
    case WasmScalarKind::F32:
        if (type.IsSingleFloat) {
            return StoreWasmApiDictArrayFromNative<float32_t>(value, storage);
        }
        break;
    case WasmScalarKind::F64:
        if (type.IsDoubleFloat) {
            return StoreWasmApiDictArrayFromNative<float64_t>(value, storage);
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API dict array element type", type.Name, static_cast<int32_t>(kind));
}

static auto StoreWasmApiDictArrayElementsFromNative(const BaseTypeDesc& type, WasmScalarKind kind, const_span<ptr<void>> values, WasmApiDictArrayStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    vector<WasmApiArrayElement>& elements = storage.emplace<vector<WasmApiArrayElement>>();
    elements.reserve(values.size());

    for (ptr<void> value : values) {
        WasmApiArrayElement& element = elements.emplace_back();
        element.Data = StoreWasmApiArrayElementFromNative(type, kind, value.get(), element.Storage);
    }

    return make_nptr(&elements).void_cast();
}

static void AddWasmApiDictElementFromNative(WasmApiDictStorage& storage, ptr<void> key, ptr<void> value)
{
    FO_STACK_TRACE_ENTRY();

    WasmApiDictEntry& entry = storage.Entries.emplace_back();
    entry.KeyData = StoreWasmApiArrayElementFromNative(storage.KeyType, storage.KeyKind, key.get(), entry.KeyStorage);

    if (storage.ValueIsArray) {
        entry.ValueData = StoreWasmApiDictArrayFromNative(storage.ValueType, storage.ValueKind, value.get(), entry.ValueArrayStorage);
    }
    else {
        entry.ValueData = StoreWasmApiArrayElementFromNative(storage.ValueType, storage.ValueKind, value.get(), entry.ValueStorage);
    }
}

static void AddWasmApiDictArrayElementFromNative(WasmApiDictStorage& storage, ptr<void> key, const_span<ptr<void>> values)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(storage.ValueIsArray, "WASM API bridge invariant failed");

    WasmApiDictEntry& entry = storage.Entries.emplace_back();
    entry.KeyData = StoreWasmApiArrayElementFromNative(storage.KeyType, storage.KeyKind, key.get(), entry.KeyStorage);
    entry.ValueData = StoreWasmApiDictArrayElementsFromNative(storage.ValueType, storage.ValueKind, values, entry.ValueArrayStorage);
}

static void ResetWasmApiArrayStorage(WasmApiArrayStorage& storage, const BaseTypeDesc& element_type, WasmScalarKind kind)
{
    FO_STACK_TRACE_ENTRY();

    storage.ElementType = element_type;
    storage.Kind = kind;
    storage.Elements.clear();
}

static void ResetWasmApiDictStorage(WasmApiDictStorage& storage, const BaseTypeDesc& key_type, WasmScalarKind key_kind, const BaseTypeDesc& value_type, WasmScalarKind value_kind)
{
    FO_STACK_TRACE_ENTRY();

    storage.KeyType = key_type;
    storage.KeyKind = key_kind;
    storage.ValueType = value_type;
    storage.ValueKind = value_kind;
    storage.ValueIsArray = false;
    storage.Entries.clear();
}

static void ResetWasmApiDictOfArrayStorage(WasmApiDictStorage& storage, const BaseTypeDesc& key_type, WasmScalarKind key_kind, const BaseTypeDesc& value_type, WasmScalarKind value_kind)
{
    FO_STACK_TRACE_ENTRY();

    ResetWasmApiDictStorage(storage, key_type, key_kind, value_type, value_kind);
    storage.ValueIsArray = true;
}

static auto StoreWasmApiDictElementFromBytes(BaseEngine& engine, const BaseTypeDesc& type, WasmScalarKind kind, span<const uint8_t> raw_data, size_t& offset, WasmApiScalarStorage& storage, string_view role) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmApiTextBaseType(type)) {
        if (offset + sizeof(uint32_t) > raw_data.size()) {
            throw ScriptCallException(strex("WASM API dict {} string header is out of bounds", role), offset, raw_data.size());
        }

        uint32_t string_size = ReadWasmApiU32(raw_data.subspan(offset, sizeof(uint32_t)), "dict string length");
        offset += sizeof(uint32_t);

        if (offset + string_size > raw_data.size()) {
            throw ScriptCallException(strex("WASM API dict {} string data is out of bounds", role), string_size, offset, raw_data.size());
        }

        ptr<void> data = StoreWasmApiText(type, string_view {reinterpret_cast<const char*>(raw_data.data() + offset), numeric_cast<size_t>(string_size)}, storage);
        offset += string_size;
        return data.get();
    }

    size_t element_size = GetWasmApiArrayElementWireSize(type);

    if (offset + element_size > raw_data.size()) {
        throw ScriptCallException(strex("WASM API dict {} data is out of bounds", role), type.Name, element_size, offset, raw_data.size());
    }

    void* data = StoreWasmApiArrayElementFromWire(engine, type, kind, raw_data.subspan(offset, element_size), storage);
    offset += element_size;
    return data;
}

static auto StoreWasmApiDictGenericArrayFromBytes(BaseEngine& engine, const BaseTypeDesc& type, WasmScalarKind kind, span<const uint8_t> raw_data, size_t& offset, uint32_t count, WasmApiDictArrayStorage& storage, string_view role) -> void*
{
    FO_STACK_TRACE_ENTRY();

    vector<WasmApiArrayElement>& values = storage.emplace<vector<WasmApiArrayElement>>();
    values.reserve(count);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        WasmApiArrayElement& value = values.emplace_back();
        value.Data = StoreWasmApiDictElementFromBytes(engine, type, kind, raw_data, offset, value.Storage, role);
    }

    return make_nptr(&values).void_cast();
}

static auto StoreWasmApiDictArrayFromBytes(BaseEngine& engine, const BaseTypeDesc& type, WasmScalarKind kind, span<const uint8_t> raw_data, size_t& offset, WasmApiDictArrayStorage& storage, string_view role) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (offset + sizeof(uint32_t) > raw_data.size()) {
        throw ScriptCallException(strex("WASM API dict {} array count is out of bounds", role), offset, raw_data.size());
    }

    uint32_t count = ReadWasmApiU32(raw_data.subspan(offset, sizeof(uint32_t)), "dict array count");
    offset += sizeof(uint32_t);

    return StoreWasmApiDictGenericArrayFromBytes(engine, type, kind, raw_data, offset, count, storage, role);
}

static auto StoreWasmApiDictFromBytes(BaseEngine& engine, const ArgDesc& arg, uint64_t raw_address, uint64_t raw_size, WasmApiDictStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    const ComplexTypeDesc& type = arg.Type;
    FO_STRONG_ASSERT(IsWasmApiDictType(type) || IsWasmApiMutableDictType(type) || IsWasmApiDictOfArrayType(type) || IsWasmApiMutableDictOfArrayType(type), "WASM API bridge invariant failed");
    FO_STRONG_ASSERT(type.KeyType.has_value(), "WASM API bridge invariant failed");

    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "dict");
    const BaseTypeDesc& key_type = *type.KeyType;
    const BaseTypeDesc& value_type = type.BaseType;
    WasmScalarKind key_kind = TryResolveWasmApiDictElementKind(key_type);
    WasmScalarKind value_kind = TryResolveWasmApiDictElementKind(value_type);

    if (type.Kind == ComplexTypeKind::DictOfArray) {
        ResetWasmApiDictOfArrayStorage(storage, key_type, key_kind, value_type, value_kind);
    }
    else {
        ResetWasmApiDictStorage(storage, key_type, key_kind, value_type, value_kind);
    }

    if (raw_data.empty()) {
        return make_nptr(&storage).void_cast();
    }
    if (raw_data.size() < sizeof(uint32_t)) {
        throw ScriptCallException("WASM API dict buffer is too small", arg.Name, raw_data.size());
    }

    uint32_t count = ReadWasmApiU32(raw_data.first(sizeof(uint32_t)), "dict count");
    size_t offset = sizeof(uint32_t);
    storage.Entries.reserve(count);

    for (uint32_t entry_index = 0; entry_index < count; entry_index++) {
        WasmApiDictEntry& entry = storage.Entries.emplace_back();
        entry.KeyData = StoreWasmApiDictElementFromBytes(engine, key_type, key_kind, raw_data, offset, entry.KeyStorage, "key");

        if (storage.ValueIsArray) {
            entry.ValueData = StoreWasmApiDictArrayFromBytes(engine, value_type, value_kind, raw_data, offset, entry.ValueArrayStorage, "value");
        }
        else {
            entry.ValueData = StoreWasmApiDictElementFromBytes(engine, value_type, value_kind, raw_data, offset, entry.ValueStorage, "value");
        }
    }

    if (offset != raw_data.size()) {
        throw ScriptCallException("WASM API dict buffer has trailing bytes", arg.Name, raw_data.size() - offset);
    }

    return make_nptr(&storage).void_cast();
}

static auto StoreWasmApiArrayFromBytes(BaseEngine& engine, const ArgDesc& arg, WasmScalarKind kind, uint64_t raw_address, uint64_t raw_size, WasmApiArrayStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    const ComplexTypeDesc& type = arg.Type;
    FO_STRONG_ASSERT(type.Kind == ComplexTypeKind::Array, "WASM API bridge invariant failed");

    const BaseTypeDesc& element_type = type.BaseType;
    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "array");
    size_t element_size = GetWasmApiArrayElementWireSize(element_type);

    if (raw_data.size() % element_size != 0) {
        throw ScriptCallException("WASM API array buffer length is not aligned to element size", arg.Name, element_type.Name, raw_data.size(), element_size);
    }

    ResetWasmApiArrayStorage(storage, element_type, kind);
    storage.Elements.reserve(raw_data.size() / element_size);

    for (size_t offset = 0; offset < raw_data.size(); offset += element_size) {
        WasmApiArrayElement& element = storage.Elements.emplace_back();
        element.Data = StoreWasmApiArrayElementFromWire(engine, element_type, kind, raw_data.subspan(offset, element_size), element.Storage);
    }

    return make_nptr(&storage).void_cast();
}

static auto StoreWasmApiValueFromBytes(BaseEngine& engine, const ArgDesc& arg, WasmScalarKind kind, uint64_t raw_address, uint64_t raw_size, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    const ComplexTypeDesc& type = arg.Type;
    FO_STRONG_ASSERT(IsWasmApiValueBufferType(type), "WASM API bridge invariant failed");

    const BaseTypeDesc& base_type = type.BaseType;
    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "value");
    size_t value_size = GetWasmApiArrayElementWireSize(base_type);

    if (raw_data.size() != value_size) {
        throw ScriptCallException("WASM API value buffer size mismatch", arg.Name, base_type.Name, raw_data.size(), value_size);
    }

    return StoreWasmApiArrayElementFromWire(engine, base_type, kind, raw_data, storage);
}

static auto StoreWasmApiStringArrayFromBytes(const ArgDesc& arg, uint64_t raw_address, uint64_t raw_size, WasmApiArrayStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    const ComplexTypeDesc& type = arg.Type;
    FO_STRONG_ASSERT(IsWasmApiStringArrayType(type) || IsWasmApiMutableStringArrayType(type), "WASM API bridge invariant failed");

    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "string array");

    ResetWasmApiArrayStorage(storage, type.BaseType, WasmScalarKind::None);

    if (raw_data.empty()) {
        return make_nptr(&storage).void_cast();
    }
    if (raw_data.size() < sizeof(uint32_t)) {
        throw ScriptCallException("WASM API string array buffer is too small", arg.Name, raw_data.size());
    }

    uint32_t count = ReadWasmApiU32(raw_data.first(sizeof(uint32_t)), "string array count");
    size_t offset = sizeof(uint32_t);
    storage.Elements.reserve(count);

    for (uint32_t string_index = 0; string_index < count; string_index++) {
        if (offset + sizeof(uint32_t) > raw_data.size()) {
            throw ScriptCallException("WASM API string array entry header is out of bounds", arg.Name, string_index);
        }

        uint32_t string_size = ReadWasmApiU32(raw_data.subspan(offset, sizeof(uint32_t)), "string array entry length");
        offset += sizeof(uint32_t);

        if (offset + string_size > raw_data.size()) {
            throw ScriptCallException("WASM API string array entry is out of bounds", arg.Name, string_index, string_size);
        }

        WasmApiArrayElement& element = storage.Elements.emplace_back();
        element.Data = StoreWasmApiText(type.BaseType, string_view {reinterpret_cast<const char*>(raw_data.data() + offset), numeric_cast<size_t>(string_size)}, element.Storage).get();
        offset += string_size;
    }

    if (offset != raw_data.size()) {
        throw ScriptCallException("WASM API string array buffer has trailing bytes", arg.Name, raw_data.size() - offset);
    }

    return make_nptr(&storage).void_cast();
}

static void CopyWasmApiArrayElementToBytes(const BaseTypeDesc& type, WasmScalarKind kind, const WasmApiScalarStorage& storage, span<uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmApiEntityHandleType(type)) {
        const Entity* entity = std::get<Entity*>(storage);

        if (entity == nullptr || entity->IsDestroyed() || !entity->GetId()) {
            throw ScriptCallException("WASM API array entity output is invalid", type.Name);
        }

        WriteWasmApiU64(PackWasmApiIdent(entity->GetId()), raw_data, "array entity");
        return;
    }
    if (IsWasmApiProtoOrFixedHandleType(type)) {
        nptr<const ProtoEntity> proto = dynamic_cast<const ProtoEntity*>(std::get<Entity*>(storage));

        if (!proto) {
            throw ScriptCallException("WASM API array proto output is invalid", type.Name);
        }

        WriteWasmApiU64(proto->GetProtoId().as_hash(), raw_data, "array proto");
        return;
    }
    if (IsWasmApiRefHandleType(type)) {
        const void* ref = std::get<void*>(storage);

        if (ref == nullptr) {
            throw ScriptCallException("WASM API array ref output is invalid", type.Name);
        }

        WriteWasmApiU64(numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(ref)), raw_data, "array ref");
        return;
    }
    if (type.IsHashedString) {
        WriteWasmApiU64(std::get<hstring>(storage).as_hash(), raw_data, "array hstring");
        return;
    }
    if (IsWasmApiIdentType(type)) {
        WriteWasmApiU64(PackWasmApiIdent(std::get<ident_t>(storage)), raw_data, "array ident");
        return;
    }
    if (IsWasmApiOpaqueScalarType(type)) {
        const WasmApiOpaqueScalarStorage& opaque = std::get<WasmApiOpaqueScalarStorage>(storage);
        FO_STRONG_ASSERT(type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");
        ignore_unused(kind);

        CopyWasmApiNativeScalarToWire(type, span<const uint8_t> {opaque.Data.data(), type.Size}, raw_data);
        return;
    }
    if (IsWasmApiBufferElementType(type)) {
        const WasmApiOpaqueScalarStorage& opaque = std::get<WasmApiOpaqueScalarStorage>(storage);
        FO_STRONG_ASSERT(type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");
        ignore_unused(kind);

        CopyWasmApiNativeScalarToWire(type, span<const uint8_t> {opaque.Data.data(), type.Size}, raw_data);
        return;
    }

    switch (kind) {
    case WasmScalarKind::I32:
        if (type.IsBool) {
            CopyWasmApiArrayPlainElementToBytes<bool>(storage, raw_data);
            return;
        }
        if (type.IsInt8) {
            CopyWasmApiArrayPlainElementToBytes<int8_t>(storage, raw_data);
            return;
        }
        if (type.IsInt16) {
            CopyWasmApiArrayPlainElementToBytes<int16_t>(storage, raw_data);
            return;
        }
        if (type.IsInt32) {
            CopyWasmApiArrayPlainElementToBytes<int32_t>(storage, raw_data);
            return;
        }
        if (type.IsUInt8) {
            CopyWasmApiArrayPlainElementToBytes<uint8_t>(storage, raw_data);
            return;
        }
        if (type.IsUInt16) {
            CopyWasmApiArrayPlainElementToBytes<uint16_t>(storage, raw_data);
            return;
        }
        if (type.IsUInt32) {
            CopyWasmApiArrayPlainElementToBytes<uint32_t>(storage, raw_data);
            return;
        }
        break;
    case WasmScalarKind::I64:
        if (type.IsInt64) {
            CopyWasmApiArrayPlainElementToBytes<int64_t>(storage, raw_data);
            return;
        }
        if (type.IsUInt64) {
            CopyWasmApiArrayPlainElementToBytes<uint64_t>(storage, raw_data);
            return;
        }
        break;
    case WasmScalarKind::F32:
        if (type.IsSingleFloat) {
            CopyWasmApiArrayPlainElementToBytes<float32_t>(storage, raw_data);
            return;
        }
        break;
    case WasmScalarKind::F64:
        if (type.IsDoubleFloat) {
            CopyWasmApiArrayPlainElementToBytes<float64_t>(storage, raw_data);
            return;
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API array element output type", type.Name, static_cast<int32_t>(kind));
}

static auto MeasureWasmApiStringArrayOutput(const WasmApiArrayStorage& storage) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    size_t size = sizeof(uint32_t);

    for (const WasmApiArrayElement& element : storage.Elements) {
        size += sizeof(uint32_t);
        size += GetWasmApiText(storage.ElementType, element.Storage).size();
    }

    return size;
}

static void WriteWasmApiStringArrayOutput(const WasmApiArrayStorage& storage, span<uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    WriteWasmApiU32(numeric_cast<uint32_t>(storage.Elements.size()), raw_data.first(sizeof(uint32_t)), "string array count");

    size_t offset = sizeof(uint32_t);

    for (const WasmApiArrayElement& element : storage.Elements) {
        string_view text = GetWasmApiText(storage.ElementType, element.Storage);

        WriteWasmApiU32(numeric_cast<uint32_t>(text.size()), raw_data.subspan(offset, sizeof(uint32_t)), "string array entry length");
        offset += sizeof(uint32_t);

        if (!text.empty()) {
            MemCopy(raw_data.data() + offset, text.data(), text.size());
            offset += text.size();
        }
    }
}

static auto CopyWasmApiStringArrayOutput(const ComplexTypeDesc& type, const WasmApiArrayStorage& storage, uint64_t raw_address, uint64_t raw_size) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiStringArrayType(type), "WASM API bridge invariant failed");

    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "string array output");
    size_t required_size = MeasureWasmApiStringArrayOutput(storage);

    if (raw_data.size() >= required_size) {
        WriteWasmApiStringArrayOutput(storage, raw_data.first(required_size));
    }

    return numeric_cast<uint32_t>(required_size);
}

static auto CopyWasmApiRawDataOutput(span<const uint8_t> source, uint64_t raw_address, uint64_t raw_size, string_view role) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, role);

    if (raw_data.size() >= source.size() && !source.empty()) {
        MemCopy(raw_data.data(), source.data(), source.size());
    }

    return numeric_cast<uint32_t>(source.size());
}

static void ValidateWasmApiStringArrayRawData(const Property& prop, span<const uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    if (raw_data.empty()) {
        return;
    }
    if (raw_data.size() < sizeof(uint32_t)) {
        throw ScriptCallException("WASM API property string array buffer is too small", prop.GetName(), raw_data.size());
    }

    uint32_t count = ReadWasmApiU32(raw_data.first(sizeof(uint32_t)), "property string array count");
    size_t offset = sizeof(uint32_t);

    for (uint32_t string_index = 0; string_index < count; string_index++) {
        if (offset + sizeof(uint32_t) > raw_data.size()) {
            throw ScriptCallException("WASM API property string array entry header is out of bounds", prop.GetName(), string_index);
        }

        uint32_t string_size = ReadWasmApiU32(raw_data.subspan(offset, sizeof(uint32_t)), "property string array entry length");
        offset += sizeof(uint32_t);

        if (offset + string_size > raw_data.size()) {
            throw ScriptCallException("WASM API property string array entry is out of bounds", prop.GetName(), string_index, string_size);
        }

        offset += string_size;
    }

    if (offset != raw_data.size()) {
        throw ScriptCallException("WASM API property string array buffer has trailing bytes", prop.GetName(), raw_data.size() - offset);
    }
}

static void ValidateWasmApiPropertyArrayRawData(const Property& prop, span<const uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmApiStringArrayProperty(prop)) {
        ValidateWasmApiStringArrayRawData(prop, raw_data);
        return;
    }

    size_t element_size = GetWasmApiArrayElementWireSize(prop.GetBaseType());

    if (raw_data.size() % element_size != 0) {
        throw ScriptCallException("WASM API property array buffer length is not aligned to element size", prop.GetName(), prop.GetBaseType().Name, raw_data.size(), element_size);
    }
}

static void ValidateWasmApiPropertyValueRawData(const Property& prop, span<const uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiPropertyValueBufferType(prop), "WASM API bridge invariant failed");

    size_t value_size = GetWasmApiArrayElementWireSize(prop.GetBaseType());

    if (raw_data.size() != value_size) {
        throw ScriptCallException("WASM API property value buffer size mismatch", prop.GetName(), prop.GetBaseType().Name, raw_data.size(), value_size);
    }
}

static auto MakeWasmApiPropertyDictType(const Property& prop) -> ComplexTypeDesc
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(prop.IsDict(), "WASM API bridge invariant failed");

    return ComplexTypeDesc {
        .Kind = prop.IsDictOfArray() ? ComplexTypeKind::DictOfArray : ComplexTypeKind::Dict,
        .BaseType = prop.GetBaseType(),
        .KeyType = prop.GetDictKeyType(),
    };
}

static void ValidateWasmApiPropertyDictElementRawData(const BaseTypeDesc& type, span<const uint8_t> raw_data, size_t& offset, string_view role)
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmApiTextBaseType(type)) {
        if (offset + sizeof(uint32_t) > raw_data.size()) {
            throw ScriptCallException(strex("WASM API property dict {} string header is out of bounds", role), offset, raw_data.size());
        }

        uint32_t string_size = ReadWasmApiU32(raw_data.subspan(offset, sizeof(uint32_t)), "property dict string length");
        offset += sizeof(uint32_t);

        if (offset + string_size > raw_data.size()) {
            throw ScriptCallException(strex("WASM API property dict {} string data is out of bounds", role), string_size, offset, raw_data.size());
        }

        offset += string_size;
        return;
    }

    size_t element_size = GetWasmApiArrayElementWireSize(type);

    if (offset + element_size > raw_data.size()) {
        throw ScriptCallException(strex("WASM API property dict {} data is out of bounds", role), type.Name, element_size, offset, raw_data.size());
    }

    offset += element_size;
}

static void ValidateWasmApiPropertyDictEntryRawData(const Property& prop, span<const uint8_t> raw_data, size_t& offset)
{
    FO_STACK_TRACE_ENTRY();

    ValidateWasmApiPropertyDictElementRawData(prop.GetDictKeyType(), raw_data, offset, "key");

    if (prop.IsDictOfArray()) {
        if (offset + sizeof(uint32_t) > raw_data.size()) {
            throw ScriptCallException("WASM API property dict value array count is out of bounds", prop.GetName(), offset, raw_data.size());
        }

        uint32_t count = ReadWasmApiU32(raw_data.subspan(offset, sizeof(uint32_t)), "property dict array count");
        offset += sizeof(uint32_t);

        for (uint32_t value_index = 0; value_index < count; value_index++) {
            ValidateWasmApiPropertyDictElementRawData(prop.GetBaseType(), raw_data, offset, "value");
        }
    }
    else {
        ValidateWasmApiPropertyDictElementRawData(prop.GetBaseType(), raw_data, offset, "value");
    }
}

static auto CountWasmApiPropertyDictRawEntries(const Property& prop, span<const uint8_t> raw_data) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    size_t offset = 0;
    size_t count = 0;

    while (offset < raw_data.size()) {
        ValidateWasmApiPropertyDictEntryRawData(prop, raw_data, offset);
        count++;
    }

    return count;
}

static auto CopyWasmApiPropertyDictOutput(const Property& prop, span<const uint8_t> source, uint64_t raw_address, uint64_t raw_size) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    size_t count = CountWasmApiPropertyDictRawEntries(prop, source);
    size_t required_size = sizeof(uint32_t) + source.size();
    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "property dict output");

    if (raw_data.size() >= required_size) {
        WriteWasmApiU32(numeric_cast<uint32_t>(count), raw_data.first(sizeof(uint32_t)), "property dict count");

        if (!source.empty()) {
            MemCopy(raw_data.data() + sizeof(uint32_t), source.data(), source.size());
        }
    }

    return numeric_cast<uint32_t>(required_size);
}

static auto ValidateWasmApiPropertyDictAbiData(const Property& prop, span<const uint8_t> raw_data) -> span<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    if (raw_data.empty()) {
        return {};
    }
    if (raw_data.size() < sizeof(uint32_t)) {
        throw ScriptCallException("WASM API property dict buffer is too small", prop.GetName(), raw_data.size());
    }

    uint32_t count = ReadWasmApiU32(raw_data.first(sizeof(uint32_t)), "property dict count");
    size_t offset = sizeof(uint32_t);

    for (uint32_t entry_index = 0; entry_index < count; entry_index++) {
        ValidateWasmApiPropertyDictEntryRawData(prop, raw_data, offset);
    }

    if (offset != raw_data.size()) {
        throw ScriptCallException("WASM API property dict buffer has trailing bytes", prop.GetName(), raw_data.size() - offset);
    }

    return raw_data.subspan(sizeof(uint32_t));
}

static auto MeasureWasmApiDictElementOutput(const BaseTypeDesc& type, const WasmApiScalarStorage& storage) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmApiTextBaseType(type)) {
        return sizeof(uint32_t) + GetWasmApiText(type, storage).size();
    }

    return GetWasmApiArrayElementWireSize(type);
}

static void WriteWasmApiDictElementOutput(const BaseTypeDesc& type, WasmScalarKind kind, const WasmApiScalarStorage& storage, span<uint8_t> raw_data, size_t& offset)
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmApiTextBaseType(type)) {
        string_view text = GetWasmApiText(type, storage);

        WriteWasmApiU32(numeric_cast<uint32_t>(text.size()), raw_data.subspan(offset, sizeof(uint32_t)), "dict string length");
        offset += sizeof(uint32_t);

        if (!text.empty()) {
            MemCopy(raw_data.data() + offset, text.data(), text.size());
            offset += text.size();
        }

        return;
    }

    size_t element_size = GetWasmApiArrayElementWireSize(type);
    CopyWasmApiArrayElementToBytes(type, kind, storage, raw_data.subspan(offset, element_size));
    offset += element_size;
}

template<typename T>
static auto MeasureWasmApiDictFixedArrayOutput(const BaseTypeDesc& type, const WasmApiDictArrayStorage& storage) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return sizeof(uint32_t) + std::get<vector<T>>(storage).size() * GetWasmApiArrayElementWireSize(type);
}

template<typename T>
static auto MeasureWasmApiDictTextArrayOutput(const WasmApiDictArrayStorage& storage) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    size_t size = sizeof(uint32_t);

    for (const T& value : std::get<vector<T>>(storage)) {
        size += sizeof(uint32_t);
        size += value.size();
    }

    return size;
}

static auto MeasureWasmApiDictGenericArrayOutput(const BaseTypeDesc& type, const WasmApiDictArrayStorage& storage) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    const vector<WasmApiArrayElement>& values = std::get<vector<WasmApiArrayElement>>(storage);
    size_t size = sizeof(uint32_t);

    if (IsWasmApiTextBaseType(type)) {
        for (const WasmApiArrayElement& value : values) {
            size += sizeof(uint32_t);
            size += GetWasmApiText(type, value.Storage).size();
        }
    }
    else {
        size += values.size() * GetWasmApiArrayElementWireSize(type);
    }

    return size;
}

static auto MeasureWasmApiDictArrayOutput(const BaseTypeDesc& type, WasmScalarKind kind, const WasmApiDictArrayStorage& storage) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (std::holds_alternative<vector<WasmApiArrayElement>>(storage)) {
        ignore_unused(kind);
        return MeasureWasmApiDictGenericArrayOutput(type, storage);
    }

    if (IsWasmApiTextBaseType(type)) {
        return IsWasmApiAnyBaseType(type) ? MeasureWasmApiDictTextArrayOutput<any_t>(storage) : MeasureWasmApiDictTextArrayOutput<string>(storage);
    }
    if (type.IsHashedString) {
        return MeasureWasmApiDictFixedArrayOutput<hstring>(type, storage);
    }
    if (IsWasmApiIdentType(type)) {
        return MeasureWasmApiDictFixedArrayOutput<ident_t>(type, storage);
    }
    if (IsWasmApiEntityHandleType(type) || IsWasmApiProtoOrFixedHandleType(type)) {
        return MeasureWasmApiDictFixedArrayOutput<Entity*>(type, storage);
    }
    if (IsWasmApiRefHandleType(type)) {
        return MeasureWasmApiDictFixedArrayOutput<void*>(type, storage);
    }

    switch (kind) {
    case WasmScalarKind::I32:
        if (type.IsBool) {
            return MeasureWasmApiDictFixedArrayOutput<bool>(type, storage);
        }
        if (type.IsInt8) {
            return MeasureWasmApiDictFixedArrayOutput<int8_t>(type, storage);
        }
        if (type.IsInt16) {
            return MeasureWasmApiDictFixedArrayOutput<int16_t>(type, storage);
        }
        if (type.IsInt32) {
            return MeasureWasmApiDictFixedArrayOutput<int32_t>(type, storage);
        }
        if (type.IsUInt8) {
            return MeasureWasmApiDictFixedArrayOutput<uint8_t>(type, storage);
        }
        if (type.IsUInt16) {
            return MeasureWasmApiDictFixedArrayOutput<uint16_t>(type, storage);
        }
        if (type.IsUInt32) {
            return MeasureWasmApiDictFixedArrayOutput<uint32_t>(type, storage);
        }
        break;
    case WasmScalarKind::I64:
        if (type.IsInt64) {
            return MeasureWasmApiDictFixedArrayOutput<int64_t>(type, storage);
        }
        if (type.IsUInt64) {
            return MeasureWasmApiDictFixedArrayOutput<uint64_t>(type, storage);
        }
        break;
    case WasmScalarKind::F32:
        if (type.IsSingleFloat) {
            return MeasureWasmApiDictFixedArrayOutput<float32_t>(type, storage);
        }
        break;
    case WasmScalarKind::F64:
        if (type.IsDoubleFloat) {
            return MeasureWasmApiDictFixedArrayOutput<float64_t>(type, storage);
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API dict array output type", type.Name, static_cast<int32_t>(kind));
}

template<typename T>
static void WriteWasmApiDictFixedArrayOutput(const BaseTypeDesc& type, WasmScalarKind kind, const WasmApiDictArrayStorage& storage, span<uint8_t> raw_data, size_t& offset)
{
    FO_STACK_TRACE_ENTRY();

    const vector<T>& values = std::get<vector<T>>(storage);
    size_t element_size = GetWasmApiArrayElementWireSize(type);

    WriteWasmApiU32(numeric_cast<uint32_t>(values.size()), raw_data.subspan(offset, sizeof(uint32_t)), "dict array count");
    offset += sizeof(uint32_t);

    for (const T& value : values) {
        WasmApiScalarStorage element_storage;
        const void* value_ptr = static_cast<const void*>(&value);
        StoreWasmApiArrayElementFromNative(type, kind, value_ptr, element_storage);
        CopyWasmApiArrayElementToBytes(type, kind, element_storage, raw_data.subspan(offset, element_size));
        offset += element_size;
    }
}

template<typename T>
static void WriteWasmApiDictTextArrayOutput(const WasmApiDictArrayStorage& storage, span<uint8_t> raw_data, size_t& offset)
{
    FO_STACK_TRACE_ENTRY();

    const vector<T>& values = std::get<vector<T>>(storage);

    WriteWasmApiU32(numeric_cast<uint32_t>(values.size()), raw_data.subspan(offset, sizeof(uint32_t)), "dict text array count");
    offset += sizeof(uint32_t);

    for (const T& value : values) {
        string_view text = value;

        WriteWasmApiU32(numeric_cast<uint32_t>(text.size()), raw_data.subspan(offset, sizeof(uint32_t)), "dict text array entry length");
        offset += sizeof(uint32_t);

        if (!text.empty()) {
            MemCopy(raw_data.data() + offset, text.data(), text.size());
            offset += text.size();
        }
    }
}

static void WriteWasmApiDictGenericArrayOutput(const BaseTypeDesc& type, WasmScalarKind kind, const WasmApiDictArrayStorage& storage, span<uint8_t> raw_data, size_t& offset)
{
    FO_STACK_TRACE_ENTRY();

    const vector<WasmApiArrayElement>& values = std::get<vector<WasmApiArrayElement>>(storage);

    WriteWasmApiU32(numeric_cast<uint32_t>(values.size()), raw_data.subspan(offset, sizeof(uint32_t)), "dict array count");
    offset += sizeof(uint32_t);

    if (IsWasmApiTextBaseType(type)) {
        for (const WasmApiArrayElement& value : values) {
            string_view text = GetWasmApiText(type, value.Storage);

            WriteWasmApiU32(numeric_cast<uint32_t>(text.size()), raw_data.subspan(offset, sizeof(uint32_t)), "dict text array entry length");
            offset += sizeof(uint32_t);

            if (!text.empty()) {
                MemCopy(raw_data.data() + offset, text.data(), text.size());
                offset += text.size();
            }
        }
    }
    else {
        size_t element_size = GetWasmApiArrayElementWireSize(type);

        for (const WasmApiArrayElement& value : values) {
            CopyWasmApiArrayElementToBytes(type, kind, value.Storage, raw_data.subspan(offset, element_size));
            offset += element_size;
        }
    }
}

static void WriteWasmApiDictArrayOutput(const BaseTypeDesc& type, WasmScalarKind kind, const WasmApiDictArrayStorage& storage, span<uint8_t> raw_data, size_t& offset)
{
    FO_STACK_TRACE_ENTRY();

    if (std::holds_alternative<vector<WasmApiArrayElement>>(storage)) {
        WriteWasmApiDictGenericArrayOutput(type, kind, storage, raw_data, offset);
        return;
    }

    if (IsWasmApiTextBaseType(type)) {
        if (IsWasmApiAnyBaseType(type)) {
            WriteWasmApiDictTextArrayOutput<any_t>(storage, raw_data, offset);
        }
        else {
            WriteWasmApiDictTextArrayOutput<string>(storage, raw_data, offset);
        }
        return;
    }
    if (type.IsHashedString) {
        WriteWasmApiDictFixedArrayOutput<hstring>(type, kind, storage, raw_data, offset);
        return;
    }
    if (IsWasmApiIdentType(type)) {
        WriteWasmApiDictFixedArrayOutput<ident_t>(type, kind, storage, raw_data, offset);
        return;
    }
    if (IsWasmApiEntityHandleType(type) || IsWasmApiProtoOrFixedHandleType(type)) {
        WriteWasmApiDictFixedArrayOutput<Entity*>(type, kind, storage, raw_data, offset);
        return;
    }
    if (IsWasmApiRefHandleType(type)) {
        WriteWasmApiDictFixedArrayOutput<void*>(type, kind, storage, raw_data, offset);
        return;
    }

    switch (kind) {
    case WasmScalarKind::I32:
        if (type.IsBool) {
            WriteWasmApiDictFixedArrayOutput<bool>(type, kind, storage, raw_data, offset);
            return;
        }
        if (type.IsInt8) {
            WriteWasmApiDictFixedArrayOutput<int8_t>(type, kind, storage, raw_data, offset);
            return;
        }
        if (type.IsInt16) {
            WriteWasmApiDictFixedArrayOutput<int16_t>(type, kind, storage, raw_data, offset);
            return;
        }
        if (type.IsInt32) {
            WriteWasmApiDictFixedArrayOutput<int32_t>(type, kind, storage, raw_data, offset);
            return;
        }
        if (type.IsUInt8) {
            WriteWasmApiDictFixedArrayOutput<uint8_t>(type, kind, storage, raw_data, offset);
            return;
        }
        if (type.IsUInt16) {
            WriteWasmApiDictFixedArrayOutput<uint16_t>(type, kind, storage, raw_data, offset);
            return;
        }
        if (type.IsUInt32) {
            WriteWasmApiDictFixedArrayOutput<uint32_t>(type, kind, storage, raw_data, offset);
            return;
        }
        break;
    case WasmScalarKind::I64:
        if (type.IsInt64) {
            WriteWasmApiDictFixedArrayOutput<int64_t>(type, kind, storage, raw_data, offset);
            return;
        }
        if (type.IsUInt64) {
            WriteWasmApiDictFixedArrayOutput<uint64_t>(type, kind, storage, raw_data, offset);
            return;
        }
        break;
    case WasmScalarKind::F32:
        if (type.IsSingleFloat) {
            WriteWasmApiDictFixedArrayOutput<float32_t>(type, kind, storage, raw_data, offset);
            return;
        }
        break;
    case WasmScalarKind::F64:
        if (type.IsDoubleFloat) {
            WriteWasmApiDictFixedArrayOutput<float64_t>(type, kind, storage, raw_data, offset);
            return;
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API dict array output type", type.Name, static_cast<int32_t>(kind));
}

static auto MeasureWasmApiDictOutput(const WasmApiDictStorage& storage) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    size_t size = sizeof(uint32_t);

    for (const WasmApiDictEntry& entry : storage.Entries) {
        size += MeasureWasmApiDictElementOutput(storage.KeyType, entry.KeyStorage);
        size += storage.ValueIsArray ? MeasureWasmApiDictArrayOutput(storage.ValueType, storage.ValueKind, entry.ValueArrayStorage) : MeasureWasmApiDictElementOutput(storage.ValueType, entry.ValueStorage);
    }

    return size;
}

static void WriteWasmApiDictOutput(const WasmApiDictStorage& storage, span<uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    WriteWasmApiU32(numeric_cast<uint32_t>(storage.Entries.size()), raw_data.first(sizeof(uint32_t)), "dict count");

    size_t offset = sizeof(uint32_t);

    for (const WasmApiDictEntry& entry : storage.Entries) {
        WriteWasmApiDictElementOutput(storage.KeyType, storage.KeyKind, entry.KeyStorage, raw_data, offset);

        if (storage.ValueIsArray) {
            WriteWasmApiDictArrayOutput(storage.ValueType, storage.ValueKind, entry.ValueArrayStorage, raw_data, offset);
        }
        else {
            WriteWasmApiDictElementOutput(storage.ValueType, storage.ValueKind, entry.ValueStorage, raw_data, offset);
        }
    }
}

static auto CopyWasmApiDictOutput(const ComplexTypeDesc& type, const WasmApiDictStorage& storage, uint64_t raw_address, uint64_t raw_size) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiDictType(type) || IsWasmApiDictOfArrayType(type), "WASM API bridge invariant failed");

    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "dict output");
    size_t required_size = MeasureWasmApiDictOutput(storage);

    if (raw_data.size() >= required_size) {
        WriteWasmApiDictOutput(storage, raw_data.first(required_size));
    }

    return numeric_cast<uint32_t>(required_size);
}

static void WriteWasmApiRequiredByteLength(uint64_t raw_address, size_t required_size, string_view role)
{
    FO_STACK_TRACE_ENTRY();

    uint32_t* value = reinterpret_cast<uint32_t*>(raw_address);

    if (value == nullptr) {
        throw ScriptCallException(strex("Null WASM API {} required byte length pointer", role));
    }

    *value = numeric_cast<uint32_t>(required_size);
}

static void CopyWasmApiMutableDictToBytes(const ComplexTypeDesc& type, const WasmApiDictStorage& storage, uint64_t raw_address, uint64_t raw_capacity, uint64_t raw_required_size_ptr)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiMutableDictType(type) || IsWasmApiMutableDictOfArrayType(type), "WASM API bridge invariant failed");

    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_capacity, "mutable dict");
    size_t required_size = MeasureWasmApiDictOutput(storage);

    WriteWasmApiRequiredByteLength(raw_required_size_ptr, required_size, "mutable dict");

    if (raw_data.size() < required_size) {
        return;
    }

    WriteWasmApiDictOutput(storage, raw_data.first(required_size));
}

static void CopyWasmApiMutableArrayToBytes(const ComplexTypeDesc& type, WasmScalarKind kind, const WasmApiArrayStorage& storage, uint64_t raw_address, uint64_t raw_capacity, uint64_t raw_required_size_ptr)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiMutableArrayType(type), "WASM API bridge invariant failed");

    const BaseTypeDesc& element_type = type.BaseType;
    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_capacity, "mutable array");
    size_t element_size = GetWasmApiArrayElementWireSize(element_type);
    size_t required_size = storage.Elements.size() * element_size;

    WriteWasmApiRequiredByteLength(raw_required_size_ptr, required_size, "mutable array");

    if (raw_data.size() < required_size) {
        return;
    }

    for (size_t index = 0; index < storage.Elements.size(); index++) {
        CopyWasmApiArrayElementToBytes(element_type, kind, storage.Elements[index].Storage, raw_data.subspan(index * element_size, element_size));
    }
}

static void CopyWasmApiMutableStringArrayToBytes(const ComplexTypeDesc& type, const WasmApiArrayStorage& storage, uint64_t raw_address, uint64_t raw_capacity, uint64_t raw_required_size_ptr)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiMutableStringArrayType(type), "WASM API bridge invariant failed");

    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_capacity, "mutable string array");
    size_t required_size = MeasureWasmApiStringArrayOutput(storage);

    WriteWasmApiRequiredByteLength(raw_required_size_ptr, required_size, "mutable string array");

    if (raw_data.size() < required_size) {
        return;
    }

    WriteWasmApiStringArrayOutput(storage, raw_data.first(required_size));
}

static void CopyWasmApiMutableStringToBytes(const ComplexTypeDesc& type, const WasmApiScalarStorage& storage, uint64_t raw_address, uint64_t raw_capacity, uint64_t raw_required_size_ptr)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiMutableStringType(type), "WASM API bridge invariant failed");

    string_view text = GetWasmApiText(type.BaseType, storage);
    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_capacity, "mutable UTF-8 string");
    size_t required_size = text.size();

    WriteWasmApiRequiredByteLength(raw_required_size_ptr, required_size, "mutable UTF-8 string");

    if (raw_data.size() < required_size) {
        return;
    }
    if (required_size != 0) {
        MemCopy(raw_data.data(), text.data(), required_size);
    }
}

static auto CopyWasmApiArrayOutput(const ComplexTypeDesc& type, WasmScalarKind kind, const WasmApiArrayStorage& storage, uint64_t raw_address, uint64_t raw_size) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.Kind == ComplexTypeKind::Array, "WASM API bridge invariant failed");

    const BaseTypeDesc& element_type = type.BaseType;
    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "array output");
    size_t element_size = GetWasmApiArrayElementWireSize(element_type);

    if (raw_data.size() % element_size != 0) {
        throw ScriptCallException("WASM API array output buffer length is not aligned to element size", element_type.Name, raw_data.size(), element_size);
    }

    size_t output_capacity = raw_data.size() / element_size;
    size_t copy_count = std::min(storage.Elements.size(), output_capacity);

    for (size_t index = 0; index < copy_count; index++) {
        CopyWasmApiArrayElementToBytes(element_type, kind, storage.Elements[index].Storage, raw_data.subspan(index * element_size, element_size));
    }

    return numeric_cast<uint32_t>(storage.Elements.size() * element_size);
}

static auto CopyWasmApiValueOutput(const ComplexTypeDesc& type, WasmScalarKind kind, const WasmApiScalarStorage& storage, uint64_t raw_address, uint64_t raw_size) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiValueBufferType(type), "WASM API bridge invariant failed");

    const BaseTypeDesc& base_type = type.BaseType;
    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "value output");
    size_t value_size = GetWasmApiArrayElementWireSize(base_type);

    if (raw_data.size() >= value_size) {
        CopyWasmApiArrayElementToBytes(base_type, kind, storage, raw_data.first(value_size));
    }

    return numeric_cast<uint32_t>(value_size);
}

template<typename T>
static auto StoreWasmApiMutablePlainValue(span<const uint8_t> raw_data, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(raw_data.size() >= sizeof(T), "WASM API bridge invariant failed");

    T value {};
    MemCopy(&value, raw_data.data(), sizeof(value));
    storage.emplace<T>(value);

    return make_nptr(&std::get<T>(storage)).void_cast();
}

template<typename T>
static void CopyWasmApiMutablePlainValue(const WasmApiScalarStorage& storage, span<uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(raw_data.size() >= sizeof(T), "WASM API bridge invariant failed");

    const T& value = std::get<T>(storage);
    MemCopy(raw_data.data(), &value, sizeof(value));
}

static auto StoreWasmApiMutableScalarFromBytes(BaseEngine& engine, const ArgDesc& arg, WasmScalarKind kind, uint64_t raw_address, uint64_t raw_size, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    const ComplexTypeDesc& type = arg.Type;
    FO_STRONG_ASSERT(type.Kind == ComplexTypeKind::Simple, "WASM API bridge invariant failed");
    FO_STRONG_ASSERT(type.IsMutable, "WASM API bridge invariant failed");

    const BaseTypeDesc& base_type = type.BaseType;
    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "mutable value");

    if (raw_data.size() < base_type.Size) {
        throw ScriptCallException("WASM API mutable value buffer is too small", arg.Name, base_type.Name, raw_data.size(), base_type.Size);
    }
    if (IsWasmApiOpaqueScalarType(base_type)) {
        WasmApiOpaqueScalarStorage& opaque = storage.emplace<WasmApiOpaqueScalarStorage>();
        FO_STRONG_ASSERT(base_type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");
        ignore_unused(kind);

        MemCopy(opaque.Data.data(), raw_data.data(), base_type.Size);
        return make_nptr(opaque.Data.data()).void_cast();
    }
    if (IsWasmApiBufferElementType(base_type)) {
        WasmApiOpaqueScalarStorage& opaque = storage.emplace<WasmApiOpaqueScalarStorage>();
        FO_STRONG_ASSERT(base_type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");
        ignore_unused(kind);

        CopyWasmApiWireToNativeScalar(engine, base_type, raw_data.first(base_type.Size), span<uint8_t> {opaque.Data.data(), base_type.Size});
        return make_nptr(opaque.Data.data()).void_cast();
    }

    switch (kind) {
    case WasmScalarKind::I32:
        if (base_type.IsBool) {
            return StoreWasmApiMutablePlainValue<bool>(raw_data, storage);
        }
        if (base_type.IsInt8) {
            return StoreWasmApiMutablePlainValue<int8_t>(raw_data, storage);
        }
        if (base_type.IsInt16) {
            return StoreWasmApiMutablePlainValue<int16_t>(raw_data, storage);
        }
        if (base_type.IsInt32) {
            return StoreWasmApiMutablePlainValue<int32_t>(raw_data, storage);
        }
        if (base_type.IsUInt8) {
            return StoreWasmApiMutablePlainValue<uint8_t>(raw_data, storage);
        }
        if (base_type.IsUInt16) {
            return StoreWasmApiMutablePlainValue<uint16_t>(raw_data, storage);
        }
        if (base_type.IsUInt32) {
            return StoreWasmApiMutablePlainValue<uint32_t>(raw_data, storage);
        }
        break;
    case WasmScalarKind::I64:
        if (base_type.IsInt64) {
            return StoreWasmApiMutablePlainValue<int64_t>(raw_data, storage);
        }
        if (base_type.IsUInt64) {
            return StoreWasmApiMutablePlainValue<uint64_t>(raw_data, storage);
        }
        if (base_type.IsHashedString) {
            hstring::hash_t value = 0;
            MemCopy(&value, raw_data.data(), sizeof(value));
            storage.emplace<hstring>(engine.Hashes.ResolveHash(value));
            return make_nptr(&std::get<hstring>(storage)).void_cast();
        }
        if (IsWasmApiIdentType(base_type)) {
            ident_t value {};
            MemCopy(&value, raw_data.data(), sizeof(value));
            storage.emplace<ident_t>(value);
            return make_nptr(&std::get<ident_t>(storage)).void_cast();
        }
        break;
    case WasmScalarKind::F32:
        if (base_type.IsSingleFloat) {
            return StoreWasmApiMutablePlainValue<float32_t>(raw_data, storage);
        }
        break;
    case WasmScalarKind::F64:
        if (base_type.IsDoubleFloat) {
            return StoreWasmApiMutablePlainValue<float64_t>(raw_data, storage);
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API mutable value type", arg.Name, base_type.Name, static_cast<int32_t>(kind));
}

static void CopyWasmApiMutableScalarToBytes(const ArgDesc& arg, WasmScalarKind kind, const WasmApiScalarStorage& storage, uint64_t raw_address, uint64_t raw_size)
{
    FO_STACK_TRACE_ENTRY();

    const ComplexTypeDesc& type = arg.Type;
    FO_STRONG_ASSERT(type.Kind == ComplexTypeKind::Simple, "WASM API bridge invariant failed");
    FO_STRONG_ASSERT(type.IsMutable, "WASM API bridge invariant failed");

    const BaseTypeDesc& base_type = type.BaseType;
    span<uint8_t> raw_data = GetWasmApiBuffer(raw_address, raw_size, "mutable value");

    if (raw_data.size() < base_type.Size) {
        throw ScriptCallException("WASM API mutable value buffer is too small", arg.Name, base_type.Name, raw_data.size(), base_type.Size);
    }
    if (IsWasmApiOpaqueScalarType(base_type)) {
        const WasmApiOpaqueScalarStorage& opaque = std::get<WasmApiOpaqueScalarStorage>(storage);
        FO_STRONG_ASSERT(base_type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");
        ignore_unused(kind);

        MemCopy(raw_data.data(), opaque.Data.data(), base_type.Size);
        return;
    }
    if (IsWasmApiBufferElementType(base_type)) {
        const WasmApiOpaqueScalarStorage& opaque = std::get<WasmApiOpaqueScalarStorage>(storage);
        FO_STRONG_ASSERT(base_type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");
        ignore_unused(kind);

        CopyWasmApiNativeScalarToWire(base_type, span<const uint8_t> {opaque.Data.data(), base_type.Size}, raw_data.first(base_type.Size));
        return;
    }

    switch (kind) {
    case WasmScalarKind::I32:
        if (base_type.IsBool) {
            CopyWasmApiMutablePlainValue<bool>(storage, raw_data);
            return;
        }
        if (base_type.IsInt8) {
            CopyWasmApiMutablePlainValue<int8_t>(storage, raw_data);
            return;
        }
        if (base_type.IsInt16) {
            CopyWasmApiMutablePlainValue<int16_t>(storage, raw_data);
            return;
        }
        if (base_type.IsInt32) {
            CopyWasmApiMutablePlainValue<int32_t>(storage, raw_data);
            return;
        }
        if (base_type.IsUInt8) {
            CopyWasmApiMutablePlainValue<uint8_t>(storage, raw_data);
            return;
        }
        if (base_type.IsUInt16) {
            CopyWasmApiMutablePlainValue<uint16_t>(storage, raw_data);
            return;
        }
        if (base_type.IsUInt32) {
            CopyWasmApiMutablePlainValue<uint32_t>(storage, raw_data);
            return;
        }
        break;
    case WasmScalarKind::I64:
        if (base_type.IsInt64) {
            CopyWasmApiMutablePlainValue<int64_t>(storage, raw_data);
            return;
        }
        if (base_type.IsUInt64) {
            CopyWasmApiMutablePlainValue<uint64_t>(storage, raw_data);
            return;
        }
        if (base_type.IsHashedString) {
            hstring::hash_t value = std::get<hstring>(storage).as_hash();
            MemCopy(raw_data.data(), &value, sizeof(value));
            return;
        }
        if (IsWasmApiIdentType(base_type)) {
            ident_t value = std::get<ident_t>(storage);
            MemCopy(raw_data.data(), &value, sizeof(value));
            return;
        }
        break;
    case WasmScalarKind::F32:
        if (base_type.IsSingleFloat) {
            CopyWasmApiMutablePlainValue<float32_t>(storage, raw_data);
            return;
        }
        break;
    case WasmScalarKind::F64:
        if (base_type.IsDoubleFloat) {
            CopyWasmApiMutablePlainValue<float64_t>(storage, raw_data);
            return;
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API mutable value type", arg.Name, base_type.Name, static_cast<int32_t>(kind));
}

static auto PrepareWasmApiReturnStorage(const ComplexTypeDesc& type, WasmScalarKind kind, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.Kind == ComplexTypeKind::Simple, "WASM API bridge invariant failed");

    const BaseTypeDesc& base_type = type.BaseType;

    if (kind == WASM_ENTITY_HANDLE_KIND && IsWasmApiEntityHandleType(base_type)) {
        storage.emplace<Entity*>(nullptr);
        return make_nptr(&std::get<Entity*>(storage)).void_cast();
    }
    if (kind == WasmScalarKind::I64 && IsWasmApiProtoOrFixedHandleType(base_type)) {
        storage.emplace<Entity*>(nullptr);
        return make_nptr(&std::get<Entity*>(storage)).void_cast();
    }
    if (kind == WASM_REF_TYPE_HANDLE_KIND && IsWasmApiRefHandleType(base_type)) {
        storage.emplace<void*>(nullptr);
        return static_cast<void*>(&std::get<void*>(storage));
    }
    if (IsWasmApiOpaqueScalarType(base_type)) {
        WasmApiOpaqueScalarStorage& opaque = storage.emplace<WasmApiOpaqueScalarStorage>();
        FO_STRONG_ASSERT(base_type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");
        ignore_unused(kind);

        return make_nptr(opaque.Data.data()).void_cast();
    }

    switch (kind) {
    case WasmScalarKind::I32:
        if (base_type.IsBool) {
            storage.emplace<bool>();
            return make_nptr(&std::get<bool>(storage)).void_cast();
        }
        if (base_type.IsInt8) {
            storage.emplace<int8_t>();
            return make_nptr(&std::get<int8_t>(storage)).void_cast();
        }
        if (base_type.IsInt16) {
            storage.emplace<int16_t>();
            return make_nptr(&std::get<int16_t>(storage)).void_cast();
        }
        if (base_type.IsInt32) {
            storage.emplace<int32_t>();
            return make_nptr(&std::get<int32_t>(storage)).void_cast();
        }
        if (base_type.IsUInt8) {
            storage.emplace<uint8_t>();
            return make_nptr(&std::get<uint8_t>(storage)).void_cast();
        }
        if (base_type.IsUInt16) {
            storage.emplace<uint16_t>();
            return make_nptr(&std::get<uint16_t>(storage)).void_cast();
        }
        if (base_type.IsUInt32) {
            storage.emplace<uint32_t>();
            return make_nptr(&std::get<uint32_t>(storage)).void_cast();
        }
        break;
    case WasmScalarKind::I64:
        if (base_type.IsInt64) {
            storage.emplace<int64_t>();
            return make_nptr(&std::get<int64_t>(storage)).void_cast();
        }
        if (base_type.IsUInt64) {
            storage.emplace<uint64_t>();
            return make_nptr(&std::get<uint64_t>(storage)).void_cast();
        }
        if (base_type.IsHashedString) {
            storage.emplace<hstring>();
            return make_nptr(&std::get<hstring>(storage)).void_cast();
        }
        if (IsWasmApiIdentType(base_type)) {
            storage.emplace<ident_t>();
            return make_nptr(&std::get<ident_t>(storage)).void_cast();
        }
        break;
    case WasmScalarKind::F32:
        if (base_type.IsSingleFloat) {
            storage.emplace<float32_t>();
            return make_nptr(&std::get<float32_t>(storage)).void_cast();
        }
        break;
    case WasmScalarKind::F64:
        if (base_type.IsDoubleFloat) {
            storage.emplace<float64_t>();
            return make_nptr(&std::get<float64_t>(storage)).void_cast();
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API return type", base_type.Name, static_cast<int32_t>(kind));
}

static auto PrepareWasmApiValueReturnStorage(const ComplexTypeDesc& type, WasmApiScalarStorage& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmApiValueBufferType(type), "WASM API bridge invariant failed");

    const BaseTypeDesc& base_type = type.BaseType;
    WasmApiOpaqueScalarStorage& opaque = storage.emplace<WasmApiOpaqueScalarStorage>();
    FO_STRONG_ASSERT(base_type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");

    return make_nptr(opaque.Data.data()).void_cast();
}

static auto PackWasmApiI32Return(const BaseTypeDesc& type, const WasmApiScalarStorage& storage) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsBool) {
        return std::get<bool>(storage) ? 1 : 0;
    }
    if (type.IsInt8) {
        return std::bit_cast<uint32_t>(numeric_cast<int32_t>(std::get<int8_t>(storage)));
    }
    if (type.IsInt16) {
        return std::bit_cast<uint32_t>(numeric_cast<int32_t>(std::get<int16_t>(storage)));
    }
    if (type.IsInt32) {
        return std::bit_cast<uint32_t>(std::get<int32_t>(storage));
    }
    if (type.IsUInt8) {
        return numeric_cast<uint32_t>(std::get<uint8_t>(storage));
    }
    if (type.IsUInt16) {
        return numeric_cast<uint32_t>(std::get<uint16_t>(storage));
    }
    if (type.IsUInt32) {
        return std::get<uint32_t>(storage);
    }

    throw ScriptCallException("Unsupported WASM API i32 return type", type.Name);
}

static auto PackWasmApiReturn(const ComplexTypeDesc& type, WasmScalarKind kind, const WasmApiScalarStorage& storage, bool return_nullable) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.Kind == ComplexTypeKind::Simple, "WASM API bridge invariant failed");

    const BaseTypeDesc& base_type = type.BaseType;

    if (kind == WASM_ENTITY_HANDLE_KIND && IsWasmApiEntityHandleType(base_type)) {
        const Entity* entity = std::get<Entity*>(storage);

        if (entity == nullptr) {
            if (return_nullable) {
                return 0;
            }

            throw ScriptCallException("WASM engine API non-nullable entity return is null", base_type.Name);
        }
        if (entity->IsDestroyed()) {
            throw ScriptCallException("WASM engine API entity return is destroyed", base_type.Name);
        }
        if (!entity->GetId()) {
            throw ScriptCallException("WASM engine API entity return has no runtime id", base_type.Name);
        }

        ptr<const PropertyRegistrator> actual_registrator = entity->GetProperties()->GetRegistrator();

        if (!IsWasmApiEntityTypeCompatible(base_type.Name, actual_registrator->GetTypeName().as_str())) {
            throw ScriptCallException("WASM engine API entity return type mismatch", base_type.Name, actual_registrator->GetTypeName());
        }

        return PackWasmApiIdent(entity->GetId());
    }
    if (kind == WasmScalarKind::I64 && IsWasmApiProtoOrFixedHandleType(base_type)) {
        const Entity* entity = std::get<Entity*>(storage);

        if (entity == nullptr) {
            if (return_nullable) {
                return 0;
            }

            throw ScriptCallException("WASM engine API non-nullable proto return is null", base_type.Name);
        }

        nptr<const ProtoEntity> proto = dynamic_cast<const ProtoEntity*>(entity);

        if (!proto) {
            throw ScriptCallException("WASM engine API proto return type mismatch", base_type.Name);
        }
        if (!IsWasmApiEntityTypeCompatible(base_type.Name, proto->GetProperties()->GetRegistrator()->GetTypeName().as_str())) {
            throw ScriptCallException("WASM engine API proto return type mismatch", base_type.Name, proto->GetProperties()->GetRegistrator()->GetTypeName());
        }

        return proto->GetProtoId().as_hash();
    }
    if (kind == WASM_REF_TYPE_HANDLE_KIND && IsWasmApiRefHandleType(base_type)) {
        const void* ref = std::get<void*>(storage);

        if (ref == nullptr) {
            if (return_nullable) {
                return 0;
            }

            throw ScriptCallException("WASM engine API non-nullable ref return is null", base_type.Name);
        }

        return numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(ref));
    }
    if (IsWasmApiOpaqueScalarType(base_type)) {
        const WasmApiOpaqueScalarStorage& opaque = std::get<WasmApiOpaqueScalarStorage>(storage);
        FO_STRONG_ASSERT(base_type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");

        return PackWasmApiRawScalarValue(base_type, span<const uint8_t> {opaque.Data.data(), base_type.Size}, kind, true);
    }

    switch (kind) {
    case WasmScalarKind::I32:
        return PackWasmApiI32Return(base_type, storage);
    case WasmScalarKind::I64:
        if (base_type.IsInt64) {
            return std::bit_cast<uint64_t>(std::get<int64_t>(storage));
        }
        if (base_type.IsUInt64) {
            return std::get<uint64_t>(storage);
        }
        if (base_type.IsHashedString) {
            return std::get<hstring>(storage).as_hash();
        }
        if (IsWasmApiIdentType(base_type)) {
            return PackWasmApiIdent(std::get<ident_t>(storage));
        }
        break;
    case WasmScalarKind::F32:
        if (base_type.IsSingleFloat) {
            return std::bit_cast<uint32_t>(std::get<float32_t>(storage));
        }
        break;
    case WasmScalarKind::F64:
        if (base_type.IsDoubleFloat) {
            return std::bit_cast<uint64_t>(std::get<float64_t>(storage));
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API return type", base_type.Name, static_cast<int32_t>(kind));
}

static auto PackWasmApiPropertyI32Value(const BaseTypeDesc& type, span<const uint8_t> raw_data) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsBool) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(bool), "WASM API bridge invariant failed");
        bool value = false;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return value ? 1 : 0;
    }
    if (type.IsInt8) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(int8_t), "WASM API bridge invariant failed");
        int8_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return std::bit_cast<uint32_t>(numeric_cast<int32_t>(value));
    }
    if (type.IsInt16) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(int16_t), "WASM API bridge invariant failed");
        int16_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return std::bit_cast<uint32_t>(numeric_cast<int32_t>(value));
    }
    if (type.IsInt32) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(int32_t), "WASM API bridge invariant failed");
        int32_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return std::bit_cast<uint32_t>(value);
    }
    if (type.IsUInt8) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(uint8_t), "WASM API bridge invariant failed");
        uint8_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return numeric_cast<uint32_t>(value);
    }
    if (type.IsUInt16) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(uint16_t), "WASM API bridge invariant failed");
        uint16_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return numeric_cast<uint32_t>(value);
    }
    if (type.IsUInt32) {
        FO_STRONG_ASSERT(raw_data.size() == sizeof(uint32_t), "WASM API bridge invariant failed");
        uint32_t value = 0;
        MemCopy(&value, raw_data.data(), sizeof(value));
        return value;
    }

    throw ScriptCallException("Unsupported WASM API property i32 type", type.Name);
}

static auto PackWasmApiPropertyValue(const Property& prop, span<const uint8_t> raw_data, WasmScalarKind kind) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    const BaseTypeDesc& base_type = prop.GetBaseType();

    if (IsWasmApiOpaqueScalarType(base_type)) {
        return PackWasmApiRawScalarValue(base_type, raw_data, kind, false);
    }

    switch (kind) {
    case WasmScalarKind::I32:
        return PackWasmApiPropertyI32Value(base_type, raw_data);
    case WasmScalarKind::I64:
        if (base_type.IsInt64) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(int64_t), "WASM API bridge invariant failed");
            int64_t value = 0;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return std::bit_cast<uint64_t>(value);
        }
        if (base_type.IsUInt64) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(uint64_t), "WASM API bridge invariant failed");
            uint64_t value = 0;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return value;
        }
        if (base_type.IsHashedString) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(hstring::hash_t), "WASM API bridge invariant failed");
            hstring::hash_t value = 0;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return value;
        }
        if (IsWasmApiIdentType(base_type)) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(ident_t), "WASM API bridge invariant failed");
            ident_t value {};
            MemCopy(&value, raw_data.data(), sizeof(value));
            return PackWasmApiIdent(value);
        }
        if (IsWasmApiProtoOrFixedHandleType(base_type)) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(hstring::hash_t), "WASM API bridge invariant failed");
            hstring::hash_t value = 0;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return value;
        }
        break;
    case WasmScalarKind::F32:
        if (base_type.IsSingleFloat) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(float32_t), "WASM API bridge invariant failed");
            float32_t value = 0.0f;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return std::bit_cast<uint32_t>(value);
        }
        break;
    case WasmScalarKind::F64:
        if (base_type.IsDoubleFloat) {
            FO_STRONG_ASSERT(raw_data.size() == sizeof(float64_t), "WASM API bridge invariant failed");
            float64_t value = 0.0;
            MemCopy(&value, raw_data.data(), sizeof(value));
            return std::bit_cast<uint64_t>(value);
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API property type", prop.GetName(), base_type.Name, static_cast<int32_t>(kind));
}

static void StoreWasmApiPropertyI32Value(const BaseTypeDesc& type, uint64_t raw_value, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    int32_t signed_value = std::bit_cast<int32_t>(numeric_cast<uint32_t>(raw_value));

    if (type.IsBool) {
        prop_data.SetAs<bool>(signed_value != 0);
        return;
    }
    if (type.IsInt8) {
        prop_data.SetAs<int8_t>(numeric_cast<int8_t>(signed_value));
        return;
    }
    if (type.IsInt16) {
        prop_data.SetAs<int16_t>(numeric_cast<int16_t>(signed_value));
        return;
    }
    if (type.IsInt32) {
        prop_data.SetAs<int32_t>(signed_value);
        return;
    }
    if (type.IsUInt8) {
        prop_data.SetAs<uint8_t>(numeric_cast<uint8_t>(signed_value));
        return;
    }
    if (type.IsUInt16) {
        prop_data.SetAs<uint16_t>(numeric_cast<uint16_t>(signed_value));
        return;
    }
    if (type.IsUInt32) {
        prop_data.SetAs<uint32_t>(std::bit_cast<uint32_t>(signed_value));
        return;
    }

    throw ScriptCallException("Unsupported WASM API property i32 type", type.Name);
}

static void StoreWasmApiPropertyValue(const Property& prop, WasmScalarKind kind, uint64_t raw_value, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    const BaseTypeDesc& base_type = prop.GetBaseType();

    if (IsWasmApiOpaqueScalarType(base_type)) {
        WasmApiOpaqueScalarStorage opaque;
        FO_STRONG_ASSERT(base_type.Size <= opaque.Data.size(), "WASM API bridge invariant failed");

        StoreWasmApiRawScalarValue(nullptr, base_type, kind, raw_value, span<uint8_t> {opaque.Data.data(), base_type.Size}, false);
        prop_data.Set(opaque.Data.data(), base_type.Size);
        return;
    }

    switch (kind) {
    case WasmScalarKind::I32:
        StoreWasmApiPropertyI32Value(base_type, raw_value, prop_data);
        return;
    case WasmScalarKind::I64:
        if (base_type.IsInt64) {
            prop_data.SetAs<int64_t>(std::bit_cast<int64_t>(raw_value));
            return;
        }
        if (base_type.IsUInt64) {
            prop_data.SetAs<uint64_t>(raw_value);
            return;
        }
        if (base_type.IsHashedString) {
            prop_data.SetAs<hstring::hash_t>(raw_value);
            return;
        }
        if (IsWasmApiIdentType(base_type)) {
            prop_data.SetAs<ident_t>(MakeWasmApiIdent(raw_value));
            return;
        }
        if (IsWasmApiProtoOrFixedHandleType(base_type)) {
            prop_data.SetAs<hstring::hash_t>(raw_value);
            return;
        }
        break;
    case WasmScalarKind::F32:
        if (base_type.IsSingleFloat) {
            prop_data.SetAs<float32_t>(std::bit_cast<float32_t>(numeric_cast<uint32_t>(raw_value)));
            return;
        }
        break;
    case WasmScalarKind::F64:
        if (base_type.IsDoubleFloat) {
            prop_data.SetAs<float64_t>(std::bit_cast<float64_t>(raw_value));
            return;
        }
        break;
    default:
        break;
    }

    throw ScriptCallException("Unsupported WASM API property type", prop.GetName(), base_type.Name, static_cast<int32_t>(kind));
}

static auto BuildWasmApiMethodDesc(string_view entity_name, const MethodDesc& method, WasmApiReceiverKind receiver_kind, string_view unsupported_receiver_reason) -> WasmApiMethodDesc
{
    FO_STACK_TRACE_ENTRY();

    WasmApiMethodDesc desc {
        .Module = string {WASM_ENGINE_API_MODULE},
        .Name = MakeWasmApiImportName(entity_name, method, receiver_kind),
        .EntityName = string {entity_name},
        .Method = &method,
        .Ret = WasmScalarKind::None,
        .ResultAbi = WasmApiResultAbiKind::None,
        .ReceiverKind = receiver_kind,
        .HasReceiverHandle = receiver_kind != WasmApiReceiverKind::None,
        .Supported = true,
    };

    if (!unsupported_receiver_reason.empty()) {
        desc.Supported = false;
        desc.UnsupportedReason = string {unsupported_receiver_reason};
        return desc;
    }
    if (!method.Call) {
        desc.Supported = false;
        desc.UnsupportedReason = "method has no native call adapter";
        return desc;
    }
    if (method.PassOwnership && (method.Ret.Kind != ComplexTypeKind::Simple || !IsWasmApiRefHandleType(method.Ret.BaseType))) {
        desc.Supported = false;
        desc.UnsupportedReason = "ownership-transferring non-ref returns need a dedicated WASM handle ABI";
        return desc;
    }
    size_t api_args_count = desc.HasReceiverHandle ? 1 : 0;

    for (const ArgDesc& arg : method.Args) {
        if (IsWasmApiMutableStringType(arg.Type) || IsWasmApiMutableArrayType(arg.Type) || IsWasmApiMutableStringArrayType(arg.Type) || IsWasmApiMutableDictType(arg.Type) || IsWasmApiMutableDictOfArrayType(arg.Type)) {
            api_args_count += 4;
        }
        else {
            api_args_count += (arg.Type.Kind == ComplexTypeKind::Callback || IsWasmApiStringType(arg.Type) || IsWasmApiValueBufferType(arg.Type) || IsWasmApiArrayType(arg.Type) || IsWasmApiStringArrayType(arg.Type) || IsWasmApiDictType(arg.Type) || IsWasmApiDictOfArrayType(arg.Type) || arg.Type.IsMutable) ? 2 : 1;
        }
    }
    if (IsWasmApiStringType(method.Ret) || IsWasmApiValueBufferType(method.Ret) || IsWasmApiArrayType(method.Ret) || IsWasmApiStringArrayType(method.Ret) || IsWasmApiDictType(method.Ret) || IsWasmApiDictOfArrayType(method.Ret)) {
        api_args_count += 2;
    }

    if (api_args_count > MAX_CALL_ARGS) {
        desc.Supported = false;
        desc.UnsupportedReason = strex("too many arguments: {}", api_args_count);
        return desc;
    }

    desc.Args.reserve(api_args_count);
    desc.ParamAbi.reserve(api_args_count);

    if (desc.HasReceiverHandle) {
        WasmScalarKind receiver_scalar_kind = WasmApiReceiverKindToScalarKind(desc.ReceiverKind);
        FO_STRONG_ASSERT(receiver_scalar_kind != WasmScalarKind::None, "WASM API bridge invariant failed");
        desc.Args.emplace_back(receiver_scalar_kind);
        desc.ParamAbi.emplace_back(WasmApiParamAbiKind::Scalar);
    }

    for (size_t arg_index = 0; arg_index < method.Args.size(); arg_index++) {
        const ComplexTypeDesc& arg_type = method.Args[arg_index].Type;

        if (IsWasmApiMutableStringType(arg_type)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringByteLength);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringCapacityByteLength);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringRequiredByteLengthPointer);
            continue;
        }
        if (IsWasmApiMutableStringArrayType(arg_type)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableArrayPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableArrayByteLength);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableArrayCapacityByteLength);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer);
            continue;
        }
        if (IsWasmApiMutableArrayType(arg_type)) {
            WasmScalarKind arg_kind = TryResolveWasmApiArrayElementKind(arg_type);

            if (!IsWasmApiResolvedBufferElementType(arg_type.BaseType, arg_kind)) {
                desc.Supported = false;
                desc.UnsupportedReason = BuildUnsupportedReason(method, arg_index, arg_type);
                return desc;
            }

            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableArrayPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableArrayByteLength);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableArrayCapacityByteLength);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer);
            continue;
        }
        if (IsWasmApiMutableDictType(arg_type) || IsWasmApiMutableDictOfArrayType(arg_type)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableDictPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableDictByteLength);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableDictCapacityByteLength);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer);
            continue;
        }
        if (IsWasmApiDictType(arg_type) || IsWasmApiDictOfArrayType(arg_type)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::DictPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::DictByteLength);
            continue;
        }
        if (arg_type.IsMutable) {
            WasmScalarKind arg_kind = TryResolveWasmApiMutableScalarKind(arg_type);

            if (!IsWasmApiResolvedBufferElementType(arg_type.BaseType, arg_kind)) {
                desc.Supported = false;
                desc.UnsupportedReason = BuildUnsupportedReason(method, arg_index, arg_type);
                return desc;
            }

            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableValuePointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::MutableValueLength);
            continue;
        }

        if (arg_type.Kind == ComplexTypeKind::Callback) {
            if (!IsWasmApiCallbackType(arg_type)) {
                desc.Supported = false;
                desc.UnsupportedReason = BuildUnsupportedReason(method, arg_index, arg_type);
                return desc;
            }

            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::CallbackPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::CallbackLength);
            continue;
        }

        if (IsWasmApiStringType(arg_type)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::Utf8StringPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::Utf8StringLength);
            continue;
        }
        if (IsWasmApiValueBufferType(arg_type)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ValuePointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ValueByteLength);
            continue;
        }
        if (IsWasmApiStringArrayType(arg_type)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ArrayPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ArrayByteLength);
            continue;
        }
        if (IsWasmApiArrayType(arg_type)) {
            WasmScalarKind arg_kind = TryResolveWasmApiArrayElementKind(arg_type);

            if (!IsWasmApiResolvedBufferElementType(arg_type.BaseType, arg_kind)) {
                desc.Supported = false;
                desc.UnsupportedReason = BuildUnsupportedReason(method, arg_index, arg_type);
                return desc;
            }

            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ArrayPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ArrayByteLength);
            continue;
        }

        WasmScalarKind arg_kind = TryResolveWasmApiScalarKind(arg_type);

        if (arg_kind == WasmScalarKind::None) {
            desc.Supported = false;
            desc.UnsupportedReason = BuildUnsupportedReason(method, arg_index, arg_type);
            return desc;
        }

        desc.Args.emplace_back(arg_kind);
        desc.ParamAbi.emplace_back(WasmApiParamAbiKind::Scalar);
    }

    if (method.Ret.Kind != ComplexTypeKind::None) {
        if (IsWasmApiStringType(method.Ret)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::Utf8StringOutputPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::Utf8StringOutputLength);
            desc.Ret = WasmScalarKind::I32;
            desc.ResultAbi = WasmApiResultAbiKind::Utf8String;
        }
        else if (IsWasmApiValueBufferType(method.Ret)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ValueOutputPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ValueOutputByteLength);
            desc.Ret = WasmScalarKind::I32;
            desc.ResultAbi = WasmApiResultAbiKind::Value;
        }
        else if (IsWasmApiArrayType(method.Ret)) {
            WasmScalarKind ret_kind = TryResolveWasmApiArrayElementKind(method.Ret);

            if (!IsWasmApiResolvedBufferElementType(method.Ret.BaseType, ret_kind)) {
                desc.Supported = false;
                desc.UnsupportedReason = strex("return value has unsupported WASM ABI type '{}'", MakeWasmApiTypeName(method.Ret));
                return desc;
            }

            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ArrayOutputPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ArrayOutputByteLength);
            desc.Ret = WasmScalarKind::I32;
            desc.ResultAbi = WasmApiResultAbiKind::Array;
        }
        else if (IsWasmApiStringArrayType(method.Ret)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ArrayOutputPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::ArrayOutputByteLength);
            desc.Ret = WasmScalarKind::I32;
            desc.ResultAbi = WasmApiResultAbiKind::Array;
        }
        else if (IsWasmApiDictType(method.Ret) || IsWasmApiDictOfArrayType(method.Ret)) {
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.Args.emplace_back(WasmScalarKind::I32);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::DictOutputPointer);
            desc.ParamAbi.emplace_back(WasmApiParamAbiKind::DictOutputByteLength);
            desc.Ret = WasmScalarKind::I32;
            desc.ResultAbi = WasmApiResultAbiKind::Dict;
        }
        else {
            desc.Ret = TryResolveWasmApiScalarKind(method.Ret);

            if (desc.Ret == WasmScalarKind::None) {
                desc.Supported = false;
                desc.UnsupportedReason = strex("return value has unsupported WASM ABI type '{}'", MakeWasmApiTypeName(method.Ret));
                return desc;
            }

            desc.ResultAbi = WasmApiResultAbiKind::Scalar;
        }
    }

    return desc;
}

static auto BuildWasmApiPropertyDesc(string_view entity_name, const Property& prop, WasmApiPropertyAccess access, WasmApiReceiverKind receiver_kind, string_view unsupported_receiver_reason) -> WasmApiPropertyDesc
{
    FO_STACK_TRACE_ENTRY();

    WasmApiPropertyDesc desc {
        .Module = string {WASM_ENGINE_API_MODULE},
        .Name = MakeWasmApiPropertyImportName(entity_name, prop, access, receiver_kind),
        .EntityName = string {entity_name},
        .Prop = &prop,
        .Access = access,
        .Ret = WasmScalarKind::None,
        .ResultAbi = WasmApiResultAbiKind::None,
        .ReceiverKind = receiver_kind,
        .HasReceiverHandle = receiver_kind != WasmApiReceiverKind::None,
        .Supported = true,
    };

    if (!unsupported_receiver_reason.empty()) {
        desc.Supported = false;
        desc.UnsupportedReason = string {unsupported_receiver_reason};
        return desc;
    }
    if (prop.IsDisabled()) {
        desc.Supported = false;
        desc.UnsupportedReason = "property is disabled for this engine side";
        return desc;
    }
    if (prop.IsComponentItself()) {
        desc.Supported = false;
        desc.UnsupportedReason = "component marker properties are not exposed as direct script properties";
        return desc;
    }
    if (access == WasmApiPropertyAccess::Setter && !prop.IsMutable() && desc.ReceiverKind != WasmApiReceiverKind::RefType) {
        desc.Supported = false;
        desc.UnsupportedReason = "property is read-only";
        return desc;
    }
    if (access == WasmApiPropertyAccess::Setter && desc.ReceiverKind == WasmApiReceiverKind::FixedType) {
        desc.Supported = false;
        desc.UnsupportedReason = "fixed-type properties are read-only";
        return desc;
    }
    bool string_property = IsWasmApiStringProperty(prop);
    bool array_property = prop.IsArray();
    bool dict_property = prop.IsDict();
    bool value_property = IsWasmApiPropertyValueBufferType(prop);

    if (!prop.IsPlainData() && !string_property && !array_property && !dict_property) {
        desc.Supported = false;
        desc.UnsupportedReason = BuildUnsupportedPropertyReason(prop);
        return desc;
    }

    if (desc.HasReceiverHandle) {
        WasmScalarKind receiver_scalar_kind = WasmApiReceiverKindToScalarKind(desc.ReceiverKind);
        FO_STRONG_ASSERT(receiver_scalar_kind != WasmScalarKind::None, "WASM API bridge invariant failed");
        desc.Args[desc.ArgsCount++] = receiver_scalar_kind;
        desc.ParamAbi[desc.ArgsCount - 1] = WasmApiParamAbiKind::Scalar;
    }

    if (string_property) {
        if (desc.ArgsCount + 2 > desc.Args.size()) {
            desc.Supported = false;
            desc.UnsupportedReason = strex("too many property arguments: {}", desc.ArgsCount + 2);
            return desc;
        }

        if (access == WasmApiPropertyAccess::Getter) {
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::Utf8StringOutputPointer;
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::Utf8StringOutputLength;
            desc.Ret = WasmScalarKind::I32;
            desc.ResultAbi = WasmApiResultAbiKind::Utf8String;
        }
        else {
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::Utf8StringPointer;
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::Utf8StringLength;
        }

        return desc;
    }

    if (array_property) {
        bool string_array_property = IsWasmApiStringArrayProperty(prop);
        WasmScalarKind value_kind = string_array_property ? WasmScalarKind::None : TryResolveWasmApiPropertyArrayElementKind(prop);

        if (!string_array_property && !IsWasmApiResolvedBufferElementType(prop.GetBaseType(), value_kind)) {
            desc.Supported = false;
            desc.UnsupportedReason = BuildUnsupportedPropertyReason(prop);
            return desc;
        }

        if (desc.ArgsCount + 2 > desc.Args.size()) {
            desc.Supported = false;
            desc.UnsupportedReason = strex("too many property arguments: {}", desc.ArgsCount + 2);
            return desc;
        }

        if (access == WasmApiPropertyAccess::Getter) {
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::ArrayOutputPointer;
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::ArrayOutputByteLength;
            desc.Ret = WasmScalarKind::I32;
            desc.ResultAbi = WasmApiResultAbiKind::Array;
        }
        else {
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::ArrayPointer;
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::ArrayByteLength;
        }

        return desc;
    }

    if (dict_property) {
        ComplexTypeDesc dict_type = MakeWasmApiPropertyDictType(prop);

        if (!IsWasmApiPropertyDictType(dict_type) && !IsWasmApiPropertyDictOfArrayType(dict_type)) {
            desc.Supported = false;
            desc.UnsupportedReason = BuildUnsupportedPropertyDictReason(prop, dict_type);
            return desc;
        }

        if (desc.ArgsCount + 2 > desc.Args.size()) {
            desc.Supported = false;
            desc.UnsupportedReason = strex("too many property arguments: {}", desc.ArgsCount + 2);
            return desc;
        }

        if (access == WasmApiPropertyAccess::Getter) {
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::DictOutputPointer;
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::DictOutputByteLength;
            desc.Ret = WasmScalarKind::I32;
            desc.ResultAbi = WasmApiResultAbiKind::Dict;
        }
        else {
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::DictPointer;
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::DictByteLength;
        }

        return desc;
    }

    if (value_property) {
        if (desc.ArgsCount + 2 > desc.Args.size()) {
            desc.Supported = false;
            desc.UnsupportedReason = strex("too many property arguments: {}", desc.ArgsCount + 2);
            return desc;
        }

        if (access == WasmApiPropertyAccess::Getter) {
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::ValueOutputPointer;
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::ValueOutputByteLength;
            desc.Ret = WasmScalarKind::I32;
            desc.ResultAbi = WasmApiResultAbiKind::Value;
        }
        else {
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::ValuePointer;
            desc.Args[desc.ArgsCount] = WasmScalarKind::I32;
            desc.ParamAbi[desc.ArgsCount++] = WasmApiParamAbiKind::ValueByteLength;
        }

        return desc;
    }

    WasmScalarKind value_kind = TryResolveWasmApiScalarKind(prop.GetBaseType());

    if (value_kind == WasmScalarKind::None) {
        desc.Supported = false;
        desc.UnsupportedReason = BuildUnsupportedPropertyReason(prop);
        return desc;
    }

    if (access == WasmApiPropertyAccess::Getter) {
        desc.Ret = value_kind;
        desc.ResultAbi = WasmApiResultAbiKind::Scalar;
    }
    else {
        desc.Args[desc.ArgsCount++] = value_kind;
        desc.ParamAbi[desc.ArgsCount - 1] = WasmApiParamAbiKind::Scalar;
    }

    return desc;
}

static auto ToWasmApiReceiverKind(ScriptApiReceiverKind receiver_kind) -> WasmApiReceiverKind
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (receiver_kind) {
    case ScriptApiReceiverKind::None:
        return WasmApiReceiverKind::None;
    case ScriptApiReceiverKind::Entity:
        return WasmApiReceiverKind::Entity;
    case ScriptApiReceiverKind::FixedType:
        return WasmApiReceiverKind::FixedType;
    case ScriptApiReceiverKind::RefType:
        return WasmApiReceiverKind::RefType;
    }

    FO_UNREACHABLE_PLACE();
}

static auto ToWasmApiPropertyAccess(ScriptApiPropertyAccess access) -> WasmApiPropertyAccess
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (access) {
    case ScriptApiPropertyAccess::Getter:
        return WasmApiPropertyAccess::Getter;
    case ScriptApiPropertyAccess::Setter:
        return WasmApiPropertyAccess::Setter;
    }

    FO_UNREACHABLE_PLACE();
}

static auto ResolveWasmApiMethodEntry(const EngineMetadata& meta, const ScriptApiMethodEntry& entry) -> const MethodDesc&
{
    FO_STACK_TRACE_ENTRY();

    switch (entry.ReceiverKind) {
    case ScriptApiReceiverKind::None: {
        if (meta.IsValidEntityType(entry.EntityName)) {
            const EntityTypeDesc& desc = meta.GetEntityType(meta.Hashes.ToHashedString(entry.EntityName));
            FO_STRONG_ASSERT(entry.MethodIndex < desc.Methods.size(), "WASM API bridge invariant failed");
            return desc.Methods[entry.MethodIndex];
        }

        const auto& ref_types = meta.GetRefTypes();
        auto it = ref_types.find(entry.EntityName);
        FO_STRONG_ASSERT(it != ref_types.end(), "WASM API bridge invariant failed");
        FO_STRONG_ASSERT(entry.MethodIndex < it->second.Methods.size(), "WASM API bridge invariant failed");
        return it->second.Methods[entry.MethodIndex];
    }
    case ScriptApiReceiverKind::Entity: {
        const EntityTypeDesc& desc = meta.GetEntityType(meta.Hashes.ToHashedString(entry.EntityName));
        FO_STRONG_ASSERT(entry.MethodIndex < desc.Methods.size(), "WASM API bridge invariant failed");
        return desc.Methods[entry.MethodIndex];
    }
    case ScriptApiReceiverKind::FixedType: {
        const EntityTypeDesc& desc = meta.GetFixedType(meta.Hashes.ToHashedString(entry.EntityName));
        FO_STRONG_ASSERT(entry.MethodIndex < desc.Methods.size(), "WASM API bridge invariant failed");
        return desc.Methods[entry.MethodIndex];
    }
    case ScriptApiReceiverKind::RefType: {
        const auto& ref_types = meta.GetRefTypes();
        auto it = ref_types.find(entry.EntityName);
        FO_STRONG_ASSERT(it != ref_types.end(), "WASM API bridge invariant failed");
        FO_STRONG_ASSERT(entry.MethodIndex < it->second.Methods.size(), "WASM API bridge invariant failed");
        return it->second.Methods[entry.MethodIndex];
    }
    }

    FO_UNREACHABLE_PLACE();
}

static auto ResolveWasmApiPropertyEntry(const EngineMetadata& meta, const ScriptApiPropertyEntry& entry) -> const Property&
{
    FO_STACK_TRACE_ENTRY();

    nptr<const Property> prop {};

    switch (entry.ReceiverKind) {
    case ScriptApiReceiverKind::None:
    case ScriptApiReceiverKind::Entity: {
        const EntityTypeDesc& desc = meta.GetEntityType(meta.Hashes.ToHashedString(entry.EntityName));
        prop = desc.PropRegistrator->GetPropertyByIndex(entry.PropertyIndex);
        break;
    }
    case ScriptApiReceiverKind::FixedType: {
        const EntityTypeDesc& desc = meta.GetFixedType(meta.Hashes.ToHashedString(entry.EntityName));
        prop = desc.PropRegistrator->GetPropertyByIndex(entry.PropertyIndex);
        break;
    }
    case ScriptApiReceiverKind::RefType: {
        const auto& ref_types = meta.GetRefTypes();
        auto it = ref_types.find(entry.EntityName);
        FO_STRONG_ASSERT(it != ref_types.end(), "WASM API bridge invariant failed");
        FO_STRONG_ASSERT(it->second.FieldsRegistrator != nullptr, "WASM API bridge invariant failed");
        prop = it->second.FieldsRegistrator->GetPropertyByIndex(entry.PropertyIndex);
        break;
    }
    }

    FO_STRONG_ASSERT(prop, "WASM API bridge invariant failed");
    return *prop;
}

auto BuildWasmApiMethodDescs(const EngineMetadata& meta) -> vector<WasmApiMethodDesc>
{
    FO_STACK_TRACE_ENTRY();

    vector<WasmApiMethodDesc> descs;

    for (const ScriptApiMethodEntry& entry : meta.GetScriptApiMethodEntries()) {
        const MethodDesc& method = ResolveWasmApiMethodEntry(meta, entry);
        descs.emplace_back(BuildWasmApiMethodDesc(entry.EntityName, method, ToWasmApiReceiverKind(entry.ReceiverKind), entry.UnsupportedReceiverReason));
    }

    return descs;
}

auto BuildWasmApiPropertyDescs(const EngineMetadata& meta) -> vector<WasmApiPropertyDesc>
{
    FO_STACK_TRACE_ENTRY();

    vector<WasmApiPropertyDesc> descs;

    for (const ScriptApiPropertyEntry& entry : meta.GetScriptApiPropertyEntries()) {
        const Property& prop = ResolveWasmApiPropertyEntry(meta, entry);
        descs.emplace_back(BuildWasmApiPropertyDesc(entry.EntityName, prop, ToWasmApiPropertyAccess(entry.Access), ToWasmApiReceiverKind(entry.ReceiverKind), entry.UnsupportedReceiverReason));
    }

    return descs;
}

auto BuildWasmApiImportTable(const EngineMetadata& meta) -> WasmApiImportTable
{
    FO_STACK_TRACE_ENTRY();

    return WasmApiImportTable {
        .Side = meta.GetSide(),
        .Methods = BuildWasmApiMethodDescs(meta),
        .Properties = BuildWasmApiPropertyDescs(meta),
    };
}

auto GetWasmApiImportTableSideName(EngineSideKind side) noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (side) {
    case EngineSideKind::ServerSide:
        return "server";
    case EngineSideKind::ClientSide:
        return "client";
    case EngineSideKind::MapperSide:
        return "mapper";
    default:
        return "unknown";
    }
}

auto CountWasmApiSupportedMethods(const WasmApiImportTable& table) noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<size_t>(std::ranges::count_if(table.Methods, [](const WasmApiMethodDesc& method) noexcept { return method.Supported; }));
}

auto CountWasmApiSupportedProperties(const WasmApiImportTable& table) noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<size_t>(std::ranges::count_if(table.Properties, [](const WasmApiPropertyDesc& property) noexcept { return property.Supported; }));
}

auto FindWasmApiMethodDesc(const vector<WasmApiMethodDesc>& methods, string_view module_name, string_view import_name) noexcept -> nptr<const WasmApiMethodDesc>
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const WasmApiMethodDesc& method : methods) {
        if (method.Module == module_name && method.Name == import_name) {
            return make_nptr(&method);
        }
    }

    return nullptr;
}

auto FindWasmApiPropertyDesc(const vector<WasmApiPropertyDesc>& properties, string_view module_name, string_view import_name) noexcept -> nptr<const WasmApiPropertyDesc>
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const WasmApiPropertyDesc& property : properties) {
        if (property.Module == module_name && property.Name == import_name) {
            return make_nptr(&property);
        }
    }

    return nullptr;
}

auto MakeWasmApiImportName(string_view entity_name, const MethodDesc& method, WasmApiReceiverKind receiver_kind) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result = strex("{}_{}__", SanitizeApiNamePart(entity_name), SanitizeApiNamePart(method.Name));
    bool has_arg = false;
    string receiver_handle_type_name = MakeWasmApiReceiverHandleTypeName(entity_name, receiver_kind);

    if (!receiver_handle_type_name.empty()) {
        result += SanitizeApiNamePart(receiver_handle_type_name);
        has_arg = true;
    }

    if (receiver_handle_type_name.empty() && method.Args.empty()) {
        result += "void";
    }
    else {
        for (size_t arg_index = 0; arg_index < method.Args.size(); arg_index++) {
            if (has_arg) {
                result += "_";
            }

            result += SanitizeApiNamePart(MakeWasmApiTypeName(method.Args[arg_index].Type));
            has_arg = true;
        }
    }

    result += "__";
    result += SanitizeApiNamePart(MakeWasmApiTypeName(method.Ret));

    return result;
}

auto MakeWasmApiNativeSignature(const WasmApiMethodDesc& desc) -> string
{
    FO_STACK_TRACE_ENTRY();

    string signature {"("};
    FO_STRONG_ASSERT(desc.ParamAbi.empty() || desc.ParamAbi.size() == desc.Args.size(), "WASM API bridge invariant failed");

    for (size_t arg_index = 0; arg_index < desc.Args.size(); arg_index++) {
        WasmApiParamAbiKind param_abi = !desc.ParamAbi.empty() ? desc.ParamAbi[arg_index] : WasmApiParamAbiKind::Scalar;
        AppendWasmApiNativeSignatureParam(signature, desc.Args[arg_index], param_abi);
    }

    signature += ")";

    if (desc.Ret != WasmScalarKind::None) {
        signature += WasmScalarKindToNativeSignatureChar(desc.Ret);
    }

    return signature;
}

auto MakeWasmApiPropertyImportName(string_view entity_name, const Property& prop, WasmApiPropertyAccess access, WasmApiReceiverKind receiver_kind) -> string
{
    FO_STACK_TRACE_ENTRY();

    string access_name = access == WasmApiPropertyAccess::Getter ? "get" : "set";

    string signature_part;
    string receiver_handle_type_name = MakeWasmApiReceiverHandleTypeName(entity_name, receiver_kind);

    if (!receiver_handle_type_name.empty()) {
        signature_part += SanitizeApiNamePart(receiver_handle_type_name);
        signature_part += "_";
    }

    signature_part += MakeWasmApiPropertyTypeName(prop);

    return strex("{}_{}_{}__{}", SanitizeApiNamePart(entity_name), access_name, SanitizeApiNamePart(prop.GetName()), signature_part);
}

auto MakeWasmApiPropertyNativeSignature(const WasmApiPropertyDesc& desc) -> string
{
    FO_STACK_TRACE_ENTRY();

    string signature {"("};

    for (size_t arg_index = 0; arg_index < desc.ArgsCount; arg_index++) {
        AppendWasmApiNativeSignatureParam(signature, desc.Args[arg_index], desc.ParamAbi[arg_index]);
    }

    signature += ")";

    if (desc.Ret != WasmScalarKind::None) {
        signature += WasmScalarKindToNativeSignatureChar(desc.Ret);
    }

    return signature;
}

auto TryResolveWasmApiScalarKind(const ComplexTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Kind != ComplexTypeKind::Simple || type.IsMutable) {
        return WasmScalarKind::None;
    }

    return TryResolveWasmApiScalarKind(type.BaseType);
}

auto TryResolveWasmApiScalarKind(const BaseTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (IsWasmApiEntityHandleType(type)) {
        return WASM_ENTITY_HANDLE_KIND;
    }
    if (IsWasmApiProtoOrFixedHandleType(type)) {
        return WasmScalarKind::I64;
    }
    if (IsWasmApiRefHandleType(type)) {
        return WASM_REF_TYPE_HANDLE_KIND;
    }

    return TryResolveWasmApiRawScalarKind(type);
}

auto ValidateWasmApiMethodSignature(const WasmApiMethodDesc& desc, const_span<WasmScalarKind> args, const_span<WasmScalarKind> results) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!desc.Supported) {
        return false;
    }
    if (!desc.ParamAbi.empty() && desc.ParamAbi.size() != desc.Args.size()) {
        return false;
    }
    if (args.size() != desc.Args.size()) {
        return false;
    }
    if (results.size() > 1) {
        return false;
    }
    if (desc.Ret == WasmScalarKind::None && !results.empty()) {
        return false;
    }
    if (desc.Ret != WasmScalarKind::None && (results.size() != 1 || results.front() != desc.Ret)) {
        return false;
    }

    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] != desc.Args[i]) {
            return false;
        }
    }

    return true;
}

auto ValidateWasmApiPropertySignature(const WasmApiPropertyDesc& desc, const_span<WasmScalarKind> args, const_span<WasmScalarKind> results) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!desc.Supported) {
        return false;
    }
    if (args.size() != desc.ArgsCount) {
        return false;
    }
    if (results.size() > 1) {
        return false;
    }
    if (desc.Ret == WasmScalarKind::None && !results.empty()) {
        return false;
    }
    if (desc.Ret != WasmScalarKind::None && (results.size() != 1 || results.front() != desc.Ret)) {
        return false;
    }

    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] != desc.Args[i]) {
            return false;
        }
    }

    return true;
}

void CallWasmApiMethod(BaseEngine& engine, const WasmApiMethodDesc& desc, const_span<uint64_t> raw_args, uint64_t* raw_result)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(desc.Method != nullptr, "WASM API bridge invariant failed");
    FO_STRONG_ASSERT(desc.Supported, "WASM API bridge invariant failed");
    FO_STRONG_ASSERT(raw_args.size() == desc.Args.size(), "WASM API bridge invariant failed");
    FO_STRONG_ASSERT(desc.ParamAbi.empty() || desc.ParamAbi.size() == desc.Args.size(), "WASM API bridge invariant failed");
    FO_STRONG_ASSERT((desc.Ret == WasmScalarKind::None) || raw_result != nullptr, "WASM API bridge invariant failed");

    const MethodDesc& method = *desc.Method;
    array<WasmApiScalarStorage, MAX_CALL_ARGS + 1> scalar_storage {};
    array<WasmApiArrayStorage, MAX_CALL_ARGS + 1> array_storage {};
    array<WasmApiDictStorage, MAX_CALL_ARGS + 1> dict_storage {};
    array<nptr<void>, MAX_CALL_ARGS + 1> args_data {};
    array<nptr<void>, MAX_CALL_ARGS + 1> mutable_arg_ptrs {};
    array<WasmApiMutableArgCopyback, MAX_CALL_ARGS> mutable_arg_copybacks {};
    array<WasmApiMutableArrayCopyback, MAX_CALL_ARGS> mutable_array_copybacks {};
    array<WasmApiMutableStringCopyback, MAX_CALL_ARGS> mutable_string_copybacks {};
    array<WasmApiMutableDictCopyback, MAX_CALL_ARGS> mutable_dict_copybacks {};
    size_t mutable_arg_copybacks_count = 0;
    size_t mutable_array_copybacks_count = 0;
    size_t mutable_string_copybacks_count = 0;
    size_t mutable_dict_copybacks_count = 0;
    size_t raw_arg_index = desc.HasReceiverHandle ? 1 : 0;
    uint64_t raw_output_ptr = 0;
    uint64_t raw_output_size = 0;
    Entity* engine_arg = nullptr;
    void* ref_arg = nullptr;

    if (desc.ReceiverKind == WasmApiReceiverKind::RefType) {
        FO_STRONG_ASSERT(!raw_args.empty(), "WASM API bridge invariant failed");
        ref_arg = ResolveWasmApiRefReceiver(desc.EntityName, raw_args.front()).get();
        args_data[0] = make_nptr(&ref_arg).void_cast();
    }
    else {
        engine_arg = ResolveWasmApiReceiver(engine, desc, raw_args).get();
        args_data[0] = make_nptr(&engine_arg).void_cast();
    }

    for (size_t arg_index = 0; arg_index < method.Args.size(); arg_index++) {
        const ArgDesc& arg = method.Args[arg_index];

        if (IsWasmApiMutableStringType(arg.Type)) {
            FO_STRONG_ASSERT(raw_arg_index + 3 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 2] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 3] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::MutableUtf8StringPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableUtf8StringByteLength, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 2] == WasmApiParamAbiKind::MutableUtf8StringCapacityByteLength, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 3] == WasmApiParamAbiKind::MutableUtf8StringRequiredByteLengthPointer, "WASM API bridge invariant failed");

            if (raw_args[raw_arg_index + 2] < raw_args[raw_arg_index + 1]) {
                throw ScriptCallException("WASM API mutable UTF-8 string capacity is smaller than input length", arg.Name, raw_args[raw_arg_index + 2], raw_args[raw_arg_index + 1]);
            }

            mutable_arg_ptrs[arg_index + 1] = StoreWasmApiString(arg.Type.BaseType, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], scalar_storage[arg_index]);
            args_data[arg_index + 1] = static_cast<void*>(&mutable_arg_ptrs[arg_index + 1]);
            mutable_string_copybacks[mutable_string_copybacks_count++] = WasmApiMutableStringCopyback {
                .Arg = &arg,
                .StorageIndex = arg_index,
                .RawPtr = raw_args[raw_arg_index],
                .RawCapacity = raw_args[raw_arg_index + 2],
                .RawRequiredSizePtr = raw_args[raw_arg_index + 3],
            };
            raw_arg_index += 4;
        }
        else if (IsWasmApiMutableStringArrayType(arg.Type)) {
            FO_STRONG_ASSERT(raw_arg_index + 3 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 2] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 3] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::MutableArrayPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableArrayByteLength, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 2] == WasmApiParamAbiKind::MutableArrayCapacityByteLength, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 3] == WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer, "WASM API bridge invariant failed");

            if (raw_args[raw_arg_index + 2] < raw_args[raw_arg_index + 1]) {
                throw ScriptCallException("WASM API mutable string array capacity is smaller than input length", arg.Name, raw_args[raw_arg_index + 2], raw_args[raw_arg_index + 1]);
            }

            args_data[arg_index + 1] = StoreWasmApiStringArrayFromBytes(arg, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], array_storage[arg_index]);
            mutable_array_copybacks[mutable_array_copybacks_count++] = WasmApiMutableArrayCopyback {
                .Arg = &arg,
                .StorageIndex = arg_index,
                .Kind = WasmScalarKind::None,
                .RawPtr = raw_args[raw_arg_index],
                .RawCapacity = raw_args[raw_arg_index + 2],
                .RawRequiredSizePtr = raw_args[raw_arg_index + 3],
                .StringArray = true,
            };
            raw_arg_index += 4;
        }
        else if (IsWasmApiMutableArrayType(arg.Type)) {
            FO_STRONG_ASSERT(raw_arg_index + 3 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 2] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 3] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::MutableArrayPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableArrayByteLength, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 2] == WasmApiParamAbiKind::MutableArrayCapacityByteLength, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 3] == WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer, "WASM API bridge invariant failed");

            if (raw_args[raw_arg_index + 2] < raw_args[raw_arg_index + 1]) {
                throw ScriptCallException("WASM API mutable array capacity is smaller than input length", arg.Name, raw_args[raw_arg_index + 2], raw_args[raw_arg_index + 1]);
            }

            WasmScalarKind arg_kind = TryResolveWasmApiArrayElementKind(arg.Type);
            FO_STRONG_ASSERT(IsWasmApiResolvedBufferElementType(arg.Type.BaseType, arg_kind), "WASM API bridge invariant failed");

            args_data[arg_index + 1] = StoreWasmApiArrayFromBytes(engine, arg, arg_kind, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], array_storage[arg_index]);
            mutable_array_copybacks[mutable_array_copybacks_count++] = WasmApiMutableArrayCopyback {
                .Arg = &arg,
                .StorageIndex = arg_index,
                .Kind = arg_kind,
                .RawPtr = raw_args[raw_arg_index],
                .RawCapacity = raw_args[raw_arg_index + 2],
                .RawRequiredSizePtr = raw_args[raw_arg_index + 3],
                .StringArray = false,
            };
            raw_arg_index += 4;
        }
        else if (IsWasmApiMutableDictType(arg.Type) || IsWasmApiMutableDictOfArrayType(arg.Type)) {
            FO_STRONG_ASSERT(raw_arg_index + 3 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 2] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 3] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::MutableDictPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableDictByteLength, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 2] == WasmApiParamAbiKind::MutableDictCapacityByteLength, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 3] == WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer, "WASM API bridge invariant failed");

            if (raw_args[raw_arg_index + 2] < raw_args[raw_arg_index + 1]) {
                throw ScriptCallException("WASM API mutable dict capacity is smaller than input length", arg.Name, raw_args[raw_arg_index + 2], raw_args[raw_arg_index + 1]);
            }

            args_data[arg_index + 1] = StoreWasmApiDictFromBytes(engine, arg, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], dict_storage[arg_index]);
            mutable_dict_copybacks[mutable_dict_copybacks_count++] = WasmApiMutableDictCopyback {
                .Arg = &arg,
                .StorageIndex = arg_index,
                .RawPtr = raw_args[raw_arg_index],
                .RawCapacity = raw_args[raw_arg_index + 2],
                .RawRequiredSizePtr = raw_args[raw_arg_index + 3],
            };
            raw_arg_index += 4;
        }
        else if (arg.Type.IsMutable) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::MutableValuePointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableValueLength, "WASM API bridge invariant failed");

            WasmScalarKind arg_kind = TryResolveWasmApiMutableScalarKind(arg.Type);
            FO_STRONG_ASSERT(IsWasmApiResolvedBufferElementType(arg.Type.BaseType, arg_kind), "WASM API bridge invariant failed");

            mutable_arg_ptrs[arg_index + 1] = StoreWasmApiMutableScalarFromBytes(engine, arg, arg_kind, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], scalar_storage[arg_index]);
            args_data[arg_index + 1] = static_cast<void*>(&mutable_arg_ptrs[arg_index + 1]);
            mutable_arg_copybacks[mutable_arg_copybacks_count++] = WasmApiMutableArgCopyback {
                .Arg = &arg,
                .StorageIndex = arg_index,
                .Kind = arg_kind,
                .RawPtr = raw_args[raw_arg_index],
                .RawSize = raw_args[raw_arg_index + 1],
            };
            raw_arg_index += 2;
        }
        else if (IsWasmApiStringType(arg.Type)) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi.empty() || desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::Utf8StringPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi.empty() || desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::Utf8StringLength, "WASM API bridge invariant failed");

            args_data[arg_index + 1] = StoreWasmApiString(arg.Type.BaseType, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], scalar_storage[arg_index]);
            raw_arg_index += 2;
        }
        else if (IsWasmApiValueBufferType(arg.Type)) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::ValuePointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::ValueByteLength, "WASM API bridge invariant failed");

            args_data[arg_index + 1] = StoreWasmApiValueFromBytes(engine, arg, WasmScalarKind::None, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], scalar_storage[arg_index]);
            raw_arg_index += 2;
        }
        else if (arg.Type.Kind == ComplexTypeKind::Callback) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::CallbackPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::CallbackLength, "WASM API bridge invariant failed");

            args_data[arg_index + 1] = StoreWasmApiCallback(engine, arg, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], scalar_storage[arg_index]);
            raw_arg_index += 2;
        }
        else if (IsWasmApiStringArrayType(arg.Type)) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::ArrayPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::ArrayByteLength, "WASM API bridge invariant failed");

            args_data[arg_index + 1] = StoreWasmApiStringArrayFromBytes(arg, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], array_storage[arg_index]);
            raw_arg_index += 2;
        }
        else if (IsWasmApiArrayType(arg.Type)) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::ArrayPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::ArrayByteLength, "WASM API bridge invariant failed");

            WasmScalarKind arg_kind = TryResolveWasmApiArrayElementKind(arg.Type);
            FO_STRONG_ASSERT(IsWasmApiResolvedBufferElementType(arg.Type.BaseType, arg_kind), "WASM API bridge invariant failed");

            args_data[arg_index + 1] = StoreWasmApiArrayFromBytes(engine, arg, arg_kind, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], array_storage[arg_index]);
            raw_arg_index += 2;
        }
        else if (IsWasmApiDictType(arg.Type) || IsWasmApiDictOfArrayType(arg.Type)) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::DictPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::DictByteLength, "WASM API bridge invariant failed");

            args_data[arg_index + 1] = StoreWasmApiDictFromBytes(engine, arg, raw_args[raw_arg_index], raw_args[raw_arg_index + 1], dict_storage[arg_index]);
            raw_arg_index += 2;
        }
        else {
            FO_STRONG_ASSERT(raw_arg_index < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi.empty() || desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::Scalar, "WASM API bridge invariant failed");

            args_data[arg_index + 1] = StoreWasmApiScalar(engine, arg, desc.Args[raw_arg_index], raw_args[raw_arg_index], scalar_storage[arg_index]);
            raw_arg_index += 1;
        }
    }

    if (desc.ResultAbi == WasmApiResultAbiKind::Utf8String || desc.ResultAbi == WasmApiResultAbiKind::Value || desc.ResultAbi == WasmApiResultAbiKind::Array || desc.ResultAbi == WasmApiResultAbiKind::Dict) {
        FO_STRONG_ASSERT(raw_arg_index + 1 < raw_args.size(), "WASM API bridge invariant failed");
        FO_STRONG_ASSERT(desc.Args[raw_arg_index] == WasmScalarKind::I32, "WASM API bridge invariant failed");
        FO_STRONG_ASSERT(desc.Args[raw_arg_index + 1] == WasmScalarKind::I32, "WASM API bridge invariant failed");

        if (desc.ResultAbi == WasmApiResultAbiKind::Utf8String) {
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::Utf8StringOutputPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::Utf8StringOutputLength, "WASM API bridge invariant failed");
        }
        else if (desc.ResultAbi == WasmApiResultAbiKind::Value) {
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::ValueOutputPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::ValueOutputByteLength, "WASM API bridge invariant failed");
        }
        else if (desc.ResultAbi == WasmApiResultAbiKind::Array) {
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::ArrayOutputPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::ArrayOutputByteLength, "WASM API bridge invariant failed");
        }
        else {
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index] == WasmApiParamAbiKind::DictOutputPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_arg_index + 1] == WasmApiParamAbiKind::DictOutputByteLength, "WASM API bridge invariant failed");
        }

        raw_output_ptr = raw_args[raw_arg_index];
        raw_output_size = raw_args[raw_arg_index + 1];
        raw_arg_index += 2;
    }

    FO_STRONG_ASSERT(raw_arg_index == raw_args.size(), "WASM API bridge invariant failed");

    vector<ptr<void>> call_args_data;
    call_args_data.reserve(method.Args.size() + 1);

    for (size_t arg_index = 0; arg_index < method.Args.size() + 1; arg_index++) {
        FO_STRONG_ASSERT(args_data[arg_index], "WASM API call argument storage is missing", arg_index, method.Name);
        call_args_data.emplace_back(args_data[arg_index]);
    }

    FuncCallData call {.Accessor = &WASM_API_DATA_ACCESSOR};
    call.ArgsData = call_args_data;

    if (desc.ResultAbi == WasmApiResultAbiKind::Scalar) {
        call.RetData = PrepareWasmApiReturnStorage(method.Ret, desc.Ret, scalar_storage[MAX_CALL_ARGS]);
    }
    else if (desc.ResultAbi == WasmApiResultAbiKind::Utf8String) {
        call.RetData = PrepareWasmApiTextReturnStorage(method.Ret.BaseType, scalar_storage[MAX_CALL_ARGS]);
    }
    else if (desc.ResultAbi == WasmApiResultAbiKind::Value) {
        call.RetData = PrepareWasmApiValueReturnStorage(method.Ret, scalar_storage[MAX_CALL_ARGS]);
    }
    else if (desc.ResultAbi == WasmApiResultAbiKind::Array) {
        WasmScalarKind ret_kind = IsWasmApiStringArrayType(method.Ret) ? WasmScalarKind::None : TryResolveWasmApiArrayElementKind(method.Ret);
        FO_STRONG_ASSERT(IsWasmApiStringArrayType(method.Ret) || IsWasmApiResolvedBufferElementType(method.Ret.BaseType, ret_kind), "WASM API bridge invariant failed");

        ResetWasmApiArrayStorage(array_storage[MAX_CALL_ARGS], method.Ret.BaseType, ret_kind);
        call.RetData = make_nptr(&array_storage[MAX_CALL_ARGS]).void_cast();
    }
    else if (desc.ResultAbi == WasmApiResultAbiKind::Dict) {
        FO_STRONG_ASSERT(method.Ret.KeyType.has_value(), "WASM API bridge invariant failed");

        const BaseTypeDesc& key_type = *method.Ret.KeyType;
        const BaseTypeDesc& value_type = method.Ret.BaseType;
        if (method.Ret.Kind == ComplexTypeKind::DictOfArray) {
            ResetWasmApiDictOfArrayStorage(dict_storage[MAX_CALL_ARGS], key_type, TryResolveWasmApiDictElementKind(key_type), value_type, TryResolveWasmApiDictElementKind(value_type));
        }
        else {
            ResetWasmApiDictStorage(dict_storage[MAX_CALL_ARGS], key_type, TryResolveWasmApiDictElementKind(key_type), value_type, TryResolveWasmApiDictElementKind(value_type));
        }
        call.RetData = make_nptr(&dict_storage[MAX_CALL_ARGS]).void_cast();
    }

    method.Call(call);

    for (size_t copyback_index = 0; copyback_index < mutable_arg_copybacks_count; copyback_index++) {
        const WasmApiMutableArgCopyback& copyback = mutable_arg_copybacks[copyback_index];
        FO_STRONG_ASSERT(copyback.Arg != nullptr, "WASM API bridge invariant failed");

        CopyWasmApiMutableScalarToBytes(*copyback.Arg, copyback.Kind, scalar_storage[copyback.StorageIndex], copyback.RawPtr, copyback.RawSize);
    }

    for (size_t copyback_index = 0; copyback_index < mutable_string_copybacks_count; copyback_index++) {
        const WasmApiMutableStringCopyback& copyback = mutable_string_copybacks[copyback_index];
        FO_STRONG_ASSERT(copyback.Arg != nullptr, "WASM API bridge invariant failed");

        CopyWasmApiMutableStringToBytes(copyback.Arg->Type, scalar_storage[copyback.StorageIndex], copyback.RawPtr, copyback.RawCapacity, copyback.RawRequiredSizePtr);
    }

    for (size_t copyback_index = 0; copyback_index < mutable_array_copybacks_count; copyback_index++) {
        const WasmApiMutableArrayCopyback& copyback = mutable_array_copybacks[copyback_index];
        FO_STRONG_ASSERT(copyback.Arg != nullptr, "WASM API bridge invariant failed");

        if (copyback.StringArray) {
            CopyWasmApiMutableStringArrayToBytes(copyback.Arg->Type, array_storage[copyback.StorageIndex], copyback.RawPtr, copyback.RawCapacity, copyback.RawRequiredSizePtr);
        }
        else {
            CopyWasmApiMutableArrayToBytes(copyback.Arg->Type, copyback.Kind, array_storage[copyback.StorageIndex], copyback.RawPtr, copyback.RawCapacity, copyback.RawRequiredSizePtr);
        }
    }

    for (size_t copyback_index = 0; copyback_index < mutable_dict_copybacks_count; copyback_index++) {
        const WasmApiMutableDictCopyback& copyback = mutable_dict_copybacks[copyback_index];
        FO_STRONG_ASSERT(copyback.Arg != nullptr, "WASM API bridge invariant failed");

        CopyWasmApiMutableDictToBytes(copyback.Arg->Type, dict_storage[copyback.StorageIndex], copyback.RawPtr, copyback.RawCapacity, copyback.RawRequiredSizePtr);
    }

    if (desc.ResultAbi == WasmApiResultAbiKind::Scalar) {
        *raw_result = PackWasmApiReturn(method.Ret, desc.Ret, scalar_storage[MAX_CALL_ARGS], method.ReturnNullable);
    }
    else if (desc.ResultAbi == WasmApiResultAbiKind::Utf8String) {
        *raw_result = CopyWasmApiStringOutput(GetWasmApiText(method.Ret.BaseType, scalar_storage[MAX_CALL_ARGS]), raw_output_ptr, raw_output_size);
    }
    else if (desc.ResultAbi == WasmApiResultAbiKind::Value) {
        *raw_result = CopyWasmApiValueOutput(method.Ret, WasmScalarKind::None, scalar_storage[MAX_CALL_ARGS], raw_output_ptr, raw_output_size);
    }
    else if (desc.ResultAbi == WasmApiResultAbiKind::Array) {
        if (IsWasmApiStringArrayType(method.Ret)) {
            *raw_result = CopyWasmApiStringArrayOutput(method.Ret, array_storage[MAX_CALL_ARGS], raw_output_ptr, raw_output_size);
        }
        else {
            WasmScalarKind ret_kind = TryResolveWasmApiArrayElementKind(method.Ret);
            FO_STRONG_ASSERT(IsWasmApiResolvedBufferElementType(method.Ret.BaseType, ret_kind), "WASM API bridge invariant failed");

            *raw_result = CopyWasmApiArrayOutput(method.Ret, ret_kind, array_storage[MAX_CALL_ARGS], raw_output_ptr, raw_output_size);
        }
    }
    else if (desc.ResultAbi == WasmApiResultAbiKind::Dict) {
        *raw_result = CopyWasmApiDictOutput(method.Ret, dict_storage[MAX_CALL_ARGS], raw_output_ptr, raw_output_size);
    }
}

void CallWasmApiProperty(BaseEngine& engine, const WasmApiPropertyDesc& desc, const_span<uint64_t> raw_args, uint64_t* raw_result)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(desc.Prop != nullptr, "WASM API bridge invariant failed");
    FO_STRONG_ASSERT(desc.Supported, "WASM API bridge invariant failed");

    size_t raw_args_offset = desc.HasReceiverHandle ? 1 : 0;
    bool ref_receiver = desc.ReceiverKind == WasmApiReceiverKind::RefType;
    nptr<Entity> entity {};
    nptr<DynamicRefTypeInstance> ref_instance {};
    const Property& prop = *desc.Prop;

    if (ref_receiver) {
        ref_instance = ResolveWasmApiDynamicRefReceiver(desc, raw_args);
    }
    else {
        entity = ResolveWasmApiReceiver(engine, desc, raw_args);
        FO_STRONG_ASSERT(entity, "WASM API bridge invariant failed");
    }

    if (desc.Access == WasmApiPropertyAccess::Getter) {
        FO_STRONG_ASSERT(raw_args.size() == desc.ArgsCount, "WASM API bridge invariant failed");
        FO_STRONG_ASSERT(raw_result != nullptr, "WASM API bridge invariant failed");

        array<uint8_t, WASM_API_OPAQUE_SCALAR_STORAGE_SIZE> default_raw_data {};
        PropertyRawData virtual_prop_data;
        span<const uint8_t> raw_data {};

        if (ref_receiver) {
            FO_STRONG_ASSERT(ref_instance, "WASM API bridge invariant failed");
            raw_data = ref_instance->GetRawData(&prop);
        }
        else if (prop.IsVirtual()) {
            ptr<const PropertyGetCallback> getter = prop.GetGetter();

            if (*getter) {
                virtual_prop_data = (*getter)(entity.as_ptr(), &prop);
                raw_data = {virtual_prop_data.GetPtrAs<uint8_t>().get(), virtual_prop_data.GetSize()};
            }
            else {
                raw_data = {default_raw_data.data(), prop.GetBaseSize()};
            }
        }
        else {
            raw_data = entity.as_ptr()->GetProperties()->GetRawData(&prop);
        }

        if (desc.ResultAbi == WasmApiResultAbiKind::Utf8String) {
            FO_STRONG_ASSERT(prop.IsString(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Ret == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(raw_args_offset + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset] == WasmApiParamAbiKind::Utf8StringOutputPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset + 1] == WasmApiParamAbiKind::Utf8StringOutputLength, "WASM API bridge invariant failed");

            string_view text {reinterpret_cast<const char*>(raw_data.data()), raw_data.size()};
            *raw_result = CopyWasmApiStringOutput(text, raw_args[raw_args_offset], raw_args[raw_args_offset + 1]);
        }
        else if (desc.ResultAbi == WasmApiResultAbiKind::Value) {
            FO_STRONG_ASSERT(IsWasmApiPropertyValueBufferType(prop), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Ret == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(raw_args_offset + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset] == WasmApiParamAbiKind::ValueOutputPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset + 1] == WasmApiParamAbiKind::ValueOutputByteLength, "WASM API bridge invariant failed");

            ValidateWasmApiPropertyValueRawData(prop, raw_data);
            *raw_result = CopyWasmApiRawDataOutput(raw_data, raw_args[raw_args_offset], raw_args[raw_args_offset + 1], "property value output");
        }
        else if (desc.ResultAbi == WasmApiResultAbiKind::Array) {
            FO_STRONG_ASSERT(prop.IsArray(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Ret == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(raw_args_offset + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset] == WasmApiParamAbiKind::ArrayOutputPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset + 1] == WasmApiParamAbiKind::ArrayOutputByteLength, "WASM API bridge invariant failed");

            *raw_result = CopyWasmApiRawDataOutput(raw_data, raw_args[raw_args_offset], raw_args[raw_args_offset + 1], "property array output");
        }
        else if (desc.ResultAbi == WasmApiResultAbiKind::Dict) {
            FO_STRONG_ASSERT(prop.IsDict(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Ret == WasmScalarKind::I32, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(raw_args_offset + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset] == WasmApiParamAbiKind::DictOutputPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset + 1] == WasmApiParamAbiKind::DictOutputByteLength, "WASM API bridge invariant failed");

            *raw_result = CopyWasmApiPropertyDictOutput(prop, raw_data, raw_args[raw_args_offset], raw_args[raw_args_offset + 1]);
        }
        else {
            FO_STRONG_ASSERT(desc.ResultAbi == WasmApiResultAbiKind::Scalar, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.Ret != WasmScalarKind::None, "WASM API bridge invariant failed");

            *raw_result = PackWasmApiPropertyValue(prop, raw_data, desc.Ret);
        }
    }
    else {
        FO_STRONG_ASSERT(raw_args.size() == desc.ArgsCount, "WASM API bridge invariant failed");
        FO_STRONG_ASSERT(desc.Ret == WasmScalarKind::None, "WASM API bridge invariant failed");
        ignore_unused(raw_result);

        if (IsWasmApiStringProperty(prop)) {
            FO_STRONG_ASSERT(raw_args_offset + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset] == WasmApiParamAbiKind::Utf8StringPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset + 1] == WasmApiParamAbiKind::Utf8StringLength, "WASM API bridge invariant failed");

            WasmApiScalarStorage string_storage;
            StoreWasmApiString(prop.GetBaseType(), raw_args[raw_args_offset], raw_args[raw_args_offset + 1], string_storage);

            if (ref_receiver) {
                FO_STRONG_ASSERT(ref_instance, "WASM API bridge invariant failed");

                string_view text = GetWasmApiText(prop.GetBaseType(), string_storage);
                PropertyRawData prop_data;
                prop_data.Set(text.data(), text.size());
                ref_instance->SetValue(&prop, prop_data);
            }
            else if (IsWasmApiAnyBaseType(prop.GetBaseType())) {
                entity.as_ptr()->GetPropertiesForEdit()->SetValue(&prop, std::get<any_t>(string_storage));
            }
            else {
                entity.as_ptr()->GetPropertiesForEdit()->SetValue(&prop, std::get<string>(string_storage));
            }
        }
        else if (IsWasmApiPropertyValueBufferType(prop)) {
            FO_STRONG_ASSERT(raw_args_offset + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset] == WasmApiParamAbiKind::ValuePointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset + 1] == WasmApiParamAbiKind::ValueByteLength, "WASM API bridge invariant failed");

            span<uint8_t> raw_data = GetWasmApiBuffer(raw_args[raw_args_offset], raw_args[raw_args_offset + 1], "property value");
            ValidateWasmApiPropertyValueRawData(prop, raw_data);

            PropertyRawData prop_data;
            prop_data.Pass(raw_data);

            if (ref_receiver) {
                FO_STRONG_ASSERT(ref_instance, "WASM API bridge invariant failed");
                ref_instance->SetValue(&prop, prop_data);
            }
            else {
                entity.as_ptr()->GetPropertiesForEdit()->SetValue(&prop, prop_data);
            }
        }
        else if (prop.IsArray()) {
            FO_STRONG_ASSERT(raw_args_offset + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset] == WasmApiParamAbiKind::ArrayPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset + 1] == WasmApiParamAbiKind::ArrayByteLength, "WASM API bridge invariant failed");

            span<uint8_t> raw_data = GetWasmApiBuffer(raw_args[raw_args_offset], raw_args[raw_args_offset + 1], "property array");
            ValidateWasmApiPropertyArrayRawData(prop, raw_data);

            PropertyRawData prop_data;
            prop_data.Pass(raw_data);

            if (ref_receiver) {
                FO_STRONG_ASSERT(ref_instance, "WASM API bridge invariant failed");
                ref_instance->SetValue(&prop, prop_data);
            }
            else {
                entity.as_ptr()->GetPropertiesForEdit()->SetValue(&prop, prop_data);
            }
        }
        else if (prop.IsDict()) {
            FO_STRONG_ASSERT(raw_args_offset + 1 < raw_args.size(), "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset] == WasmApiParamAbiKind::DictPointer, "WASM API bridge invariant failed");
            FO_STRONG_ASSERT(desc.ParamAbi[raw_args_offset + 1] == WasmApiParamAbiKind::DictByteLength, "WASM API bridge invariant failed");

            span<uint8_t> raw_data = GetWasmApiBuffer(raw_args[raw_args_offset], raw_args[raw_args_offset + 1], "property dict");
            const_span<uint8_t> property_raw_data = ValidateWasmApiPropertyDictAbiData(prop, raw_data);

            PropertyRawData prop_data;
            prop_data.Pass(property_raw_data);

            if (ref_receiver) {
                FO_STRONG_ASSERT(ref_instance, "WASM API bridge invariant failed");
                ref_instance->SetValue(&prop, prop_data);
            }
            else {
                entity.as_ptr()->GetPropertiesForEdit()->SetValue(&prop, prop_data);
            }
        }
        else {
            PropertyRawData prop_data;
            StoreWasmApiPropertyValue(prop, desc.Args[raw_args_offset], raw_args[raw_args_offset], prop_data);

            if (ref_receiver) {
                FO_STRONG_ASSERT(ref_instance, "WASM API bridge invariant failed");
                ref_instance->SetValue(&prop, prop_data);
            }
            else {
                entity.as_ptr()->GetPropertiesForEdit()->SetValue(&prop, prop_data);
            }
        }
    }
}

FO_END_NAMESPACE

#endif
