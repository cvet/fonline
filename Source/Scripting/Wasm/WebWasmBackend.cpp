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

#include "WebWasmBackend.h"

#if FO_WASM_SCRIPTING && FO_WEB

#include "Application.h"
#include "EngineBase.h"
#include "EntityProtos.h"
#include "FileSystem.h"
#include "Settings.h"
#include "WasmImports.h"
#include "WasmRefHandles.h"

#include <cstdlib>
#include <json.hpp>

FO_BEGIN_NAMESPACE

static constexpr WasmScalarKind WEB_WASM_EXPORT_REF_TYPE_HANDLE_KIND = WasmScalarKind::I64;
static constexpr size_t WEB_WASM_EXPORT_OPAQUE_VALUE_STORAGE_SIZE = 16;

struct alignas(std::max_align_t) WebWasmExportOpaqueValueStorage
{
    array<uint8_t, WEB_WASM_EXPORT_OPAQUE_VALUE_STORAGE_SIZE> Data {};
};

struct WebWasmBackend::WasmFunction
{
    string ExportName {};
    string ScriptName {};
    vector<WasmScalarKind> Args {};
    vector<WasmApiParamAbiKind> ArgAbi {};
    vector<ComplexTypeDesc> ArgTypes {};
    WasmScalarKind Ret {};
    ComplexTypeDesc RetType {};
    unique_nptr<ScriptFuncDesc> FuncDesc {};
};

struct WebWasmBackend::WasmModule
{
    string Name {};
    string Path {};
    vector<unique_ptr<WasmFunction>> Functions {};
};

// clang-format off
EM_JS(char*, WebWasmGetManifestJsonImpl, (), {
    const host = globalThis.FOnlineWasmHost;
    const manifest = host && host.getManifest ? host.getManifest() : { version: 1, modules: [] };
    const text = JSON.stringify(manifest);
    const bytes = intArrayFromString(text);
    const ptr = _malloc(bytes.length);
    HEAPU8.set(bytes, ptr);
    return ptr;
});

EM_JS(char*, WebWasmTakeLastErrorImpl, (), {
    const host = globalThis.FOnlineWasmHost;
    const text = host && host.takeLastError ? host.takeLastError() : 'Web WASM host is not initialized';
    const bytes = intArrayFromString(text);
    const ptr = _malloc(bytes.length);
    HEAPU8.set(bytes, ptr);
    return ptr;
});

EM_JS(int32_t, WebWasmCallFunctionImpl, (const char* module_name_ptr, const char* export_name_ptr, int32_t argc, const int32_t* arg_kinds_ptr, const int32_t* arg_abi_ptr, const uint64_t* arg_values_ptr, int32_t result_kind, int32_t result_buffer, uint64_t* result_value_ptr, const int64_t* context_values_ptr), {
    const host = globalThis.FOnlineWasmHost;
    if (!host || !host.call) {
        return 0;
    }
    return host.call(module_name_ptr, export_name_ptr, argc, arg_kinds_ptr, arg_abi_ptr, arg_values_ptr, result_kind, result_value_ptr, context_values_ptr, result_buffer) ? 1 : 0;
});

EM_JS(int32_t, WebWasmRegisterApiImportImpl, (const char* import_name_ptr, int32_t argc, const int32_t* arg_kinds_ptr, const int32_t* param_abi_ptr, int32_t result_kind, uintptr_t backend_ptr, int32_t method_index, uintptr_t callback_ptr), {
    const host = globalThis.FOnlineWasmHost;
    if (!host || !host.registerApiImport) {
        return 0;
    }
    host.registerApiImport(UTF8ToString(import_name_ptr), argc, arg_kinds_ptr, param_abi_ptr, result_kind, backend_ptr, method_index, callback_ptr);
    return 1;
});
// clang-format on

static auto WebWasmRetainCallbackImpl(uintptr_t script_sys_ptr, const char* token, int32_t token_len) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    WasmRuntimeContext context {};
    context.ScriptSys = reinterpret_cast<ScriptSystem*>(script_sys_ptr);
    return WasmRetainCallback(context, token, token_len);
}

static auto WebWasmReleaseCallbackImpl(uintptr_t script_sys_ptr, const char* token, int32_t token_len) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    WasmRuntimeContext context {};
    context.ScriptSys = reinterpret_cast<ScriptSystem*>(script_sys_ptr);
    return WasmReleaseCallback(context, token, token_len);
}

static auto TakeJsString(char* text) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (text == nullptr) {
        return {};
    }

    string result {text};
    std::free(text);
    return result;
}

static auto ResolveEngineType(EngineMetadata* meta, WasmScalarKind kind) -> optional<ComplexTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    string_view type_name = WasmScalarKindToEngineTypeName(kind);

    if (type_name.empty()) {
        return std::nullopt;
    }

    return ComplexTypeDesc {.Kind = ComplexTypeKind::Simple, .BaseType = meta->GetBaseType(type_name)};
}

static auto MakeArgDesc(EngineMetadata* meta, WasmScalarKind kind, size_t arg_index) -> optional<ArgDesc>
{
    FO_STACK_TRACE_ENTRY();

    auto type = ResolveEngineType(meta, kind);

    if (!type.has_value()) {
        return std::nullopt;
    }

    return ArgDesc {.Name = strex("arg{}", arg_index), .Type = std::move(type.value())};
}

static auto TryResolveWebWasmExportScalarKind(const BaseTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsGlobalEntity || type.IsString) {
        return WasmScalarKind::None;
    }
    if (type.IsRefType) {
        return WEB_WASM_EXPORT_REF_TYPE_HANDLE_KIND;
    }
    if (type.IsEntity || type.IsEntityProto || type.IsFixedType) {
        return WasmScalarKind::I64;
    }
    if (type.IsEnum) {
        return type.EnumUnderlyingType != nullptr ? TryResolveWebWasmExportScalarKind(*type.EnumUnderlyingType) : WasmScalarKind::None;
    }
    if (type.IsSimpleStruct) {
        if (type.StructLayout == nullptr || type.StructLayout->Fields.size() != 1) {
            return WasmScalarKind::None;
        }

        return TryResolveWebWasmExportScalarKind(type.StructLayout->Fields.front().Type);
    }
    if (type.IsComplexStruct) {
        if (type.StructLayout == nullptr || type.Size == 0 || type.Size > sizeof(uint64_t)) {
            return WasmScalarKind::None;
        }

        for (const FieldDesc& field : type.StructLayout->Fields) {
            const WasmScalarKind field_kind = TryResolveWebWasmExportScalarKind(field.Type);

            if (field_kind == WasmScalarKind::None || field.Type.Size == 0 || field.Type.Size > sizeof(uint64_t)) {
                return WasmScalarKind::None;
            }
        }

        return type.Size <= sizeof(uint32_t) ? WasmScalarKind::I32 : WasmScalarKind::I64;
    }
    if (type.IsHashedString) {
        return WasmScalarKind::I64;
    }
    if (type.IsBool || type.IsInt) {
        return TryResolveWasmApiScalarKind(type);
    }
    if (type.IsSingleFloat) {
        return WasmScalarKind::F32;
    }
    if (type.IsDoubleFloat) {
        return WasmScalarKind::F64;
    }

    return WasmScalarKind::None;
}

static auto TryResolveWebWasmExportScalarKind(const ComplexTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Kind != ComplexTypeKind::Simple || type.IsMutable) {
        return WasmScalarKind::None;
    }

    return TryResolveWebWasmExportScalarKind(type.BaseType);
}

static auto IsWebWasmExportBufferStructType(const BaseTypeDesc& type) noexcept -> bool;

static auto IsWebWasmExportBufferStructFieldType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsBool || type.IsInt || type.IsFloat || type.IsEnum || type.IsHashedString) {
        return true;
    }
    if (type.IsSimpleStruct || type.IsComplexStruct) {
        return IsWebWasmExportBufferStructType(type);
    }

    return false;
}

static auto IsWebWasmExportBufferStructType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if ((!type.IsSimpleStruct && !type.IsComplexStruct) || type.StructLayout == nullptr || type.Size == 0 || type.Size > WEB_WASM_EXPORT_OPAQUE_VALUE_STORAGE_SIZE) {
        return false;
    }

    for (const FieldDesc& field : type.StructLayout->Fields) {
        if (!IsWebWasmExportBufferStructFieldType(field.Type)) {
            return false;
        }
    }

    return true;
}

static auto IsWebWasmExportBufferElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWebWasmExportBufferStructType(type);
}

static auto IsWebWasmExportResolvedBufferElementType(const BaseTypeDesc& type, WasmScalarKind kind) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return kind != WasmScalarKind::None || IsWebWasmExportBufferElementType(type);
}

static auto IsWebWasmExportTextType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && !type.IsMutable && type.BaseType.IsString && (type.BaseType.Name == "string" || type.BaseType.Name == "any");
}

static auto IsWebWasmExportMutableTextType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && type.IsMutable && type.BaseType.IsString && (type.BaseType.Name == "string" || type.BaseType.Name == "any");
}

static auto IsWebWasmExportValueBufferType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && !type.IsMutable && !type.BaseType.IsString && !type.BaseType.IsRefType && TryResolveWebWasmExportScalarKind(type.BaseType) == WasmScalarKind::None && IsWebWasmExportBufferElementType(type.BaseType);
}

static auto IsWebWasmExportMutableValueType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Kind != ComplexTypeKind::Simple || !type.IsMutable || type.BaseType.IsString) {
        return false;
    }
    if (type.BaseType.IsRefType) {
        return WasmExportRefHandleScope::HasLifecycle(type.BaseType);
    }

    return IsWebWasmExportResolvedBufferElementType(type.BaseType, TryResolveWebWasmExportScalarKind(type.BaseType));
}

static auto IsWebWasmExportCallbackType(const ComplexTypeDesc& type) noexcept -> bool
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
        if (callback_args[i].Kind == ComplexTypeKind::Callback && !IsWebWasmExportCallbackType(callback_args[i])) {
            return false;
        }
    }

    return true;
}

static auto IsWebWasmExportTextArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Array && !type.IsMutable && type.BaseType.IsString && (type.BaseType.Name == "string" || type.BaseType.Name == "any");
}

static auto IsWebWasmExportMutableTextArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Array && type.IsMutable && type.BaseType.IsString && (type.BaseType.Name == "string" || type.BaseType.Name == "any");
}

static auto IsWebWasmExportArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Array && !type.IsMutable && (IsWebWasmExportTextArrayType(type) || IsWebWasmExportResolvedBufferElementType(type.BaseType, TryResolveWebWasmExportScalarKind(type.BaseType)));
}

static auto IsWebWasmExportMutableArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Kind != ComplexTypeKind::Array || !type.IsMutable) {
        return false;
    }
    if (type.BaseType.IsRefType) {
        return WasmExportRefHandleScope::HasLifecycle(type.BaseType);
    }

    return IsWebWasmExportMutableTextArrayType(type) || IsWebWasmExportResolvedBufferElementType(type.BaseType, TryResolveWebWasmExportScalarKind(type.BaseType));
}

static auto IsWebWasmExportDictElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return (type.IsString && (type.Name == "string" || type.Name == "any")) || IsWebWasmExportResolvedBufferElementType(type, TryResolveWebWasmExportScalarKind(type));
}

static auto IsWebWasmExportDictArrayElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return (type.IsString && (type.Name == "string" || type.Name == "any")) || IsWebWasmExportResolvedBufferElementType(type, TryResolveWebWasmExportScalarKind(type));
}

static auto IsWebWasmExportMutableDictElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWebWasmExportDictElementType(type) && (!type.IsRefType || WasmExportRefHandleScope::HasLifecycle(type));
}

static auto IsWebWasmExportMutableDictArrayElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWebWasmExportDictArrayElementType(type) && (!type.IsRefType || WasmExportRefHandleScope::HasLifecycle(type));
}

static auto IsWebWasmExportDictType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Dict && !type.IsMutable && type.KeyType.has_value() && IsWebWasmExportDictElementType(*type.KeyType) && IsWebWasmExportDictElementType(type.BaseType);
}

static auto IsWebWasmExportDictOfArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::DictOfArray && !type.IsMutable && type.KeyType.has_value() && IsWebWasmExportDictElementType(*type.KeyType) && IsWebWasmExportDictArrayElementType(type.BaseType);
}

static auto IsWebWasmExportMutableDictType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Dict && type.IsMutable && type.KeyType.has_value() && IsWebWasmExportMutableDictElementType(*type.KeyType) && IsWebWasmExportMutableDictElementType(type.BaseType);
}

static auto IsWebWasmExportMutableDictOfArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::DictOfArray && type.IsMutable && type.KeyType.has_value() && IsWebWasmExportMutableDictElementType(*type.KeyType) && IsWebWasmExportMutableDictArrayElementType(type.BaseType);
}

static auto IsWebWasmExportEntityHandleType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsEntity && !type.IsGlobalEntity && !type.IsEntityProto;
}

static auto IsWebWasmExportProtoOrFixedHandleType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsEntityProto || type.IsFixedType;
}

static auto IsWebWasmExportEntityTypeCompatible(string_view expected_type_name, string_view actual_type_name) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (expected_type_name == "Entity" || expected_type_name == actual_type_name) {
        return true;
    }

    for (const string_view prefix : {string_view {"Proto"}, string_view {"Static"}, string_view {"Abstract"}}) {
        if (expected_type_name.starts_with(prefix) && expected_type_name.substr(prefix.size()) == actual_type_name) {
            return true;
        }
    }

    return false;
}

static auto MakeWebWasmExportIdent(uint64_t raw_id) noexcept -> ident_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return ident_t {std::bit_cast<ident_t::underlying_type>(raw_id)};
}

static auto PackWebWasmExportIdent(ident_t id) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::bit_cast<uint64_t>(id.underlying_value());
}

static auto ResolveWebWasmExportEntityHandle(BaseEngine& engine, const BaseTypeDesc& type, uint64_t raw_id, string_view role) -> nptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    if (raw_id == 0) {
        return nullptr;
    }

    ident_t entity_id = MakeWebWasmExportIdent(raw_id);
    nptr<Entity> entity = engine.ResolveScriptEntityHandle(type.Name, entity_id);

    if (!entity) {
        throw ScriptCallException(strex("Web WASM export {} entity not found", role), type.Name, entity_id.underlying_value());
    }
    if (entity->IsDestroyed()) {
        throw ScriptCallException(strex("Web WASM export {} entity is destroyed", role), type.Name, entity_id.underlying_value());
    }

    ptr<const PropertyRegistrator> actual_registrator = entity->GetProperties()->GetRegistrator();

    if (!IsWebWasmExportEntityTypeCompatible(type.Name, actual_registrator->GetTypeName().as_str())) {
        throw ScriptCallException(strex("Web WASM export {} entity type mismatch", role), type.Name, actual_registrator->GetTypeName());
    }

    return entity;
}

static auto ResolveWebWasmExportProtoHandle(BaseEngine& engine, const BaseTypeDesc& type, uint64_t raw_hash, string_view role) -> nptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    if (raw_hash == 0) {
        return nullptr;
    }

    hstring proto_id = engine.Hashes.ResolveHash(raw_hash);
    nptr<const ProtoEntity> proto = engine.GetProtoEntity(type.HashedName, proto_id);

    if (!proto) {
        throw ScriptCallException(strex("Web WASM export {} proto not found", role), type.Name, proto_id);
    }

    ptr<const PropertyRegistrator> actual_registrator = proto->GetProperties()->GetRegistrator();

    if (!IsWebWasmExportEntityTypeCompatible(type.Name, actual_registrator->GetTypeName().as_str())) {
        throw ScriptCallException(strex("Web WASM export {} proto type mismatch", role), type.Name, actual_registrator->GetTypeName());
    }

    return make_ptr(const_cast<ProtoEntity*>(std::addressof(*proto)));
}

static auto ResolveWebWasmExportDictTypeName(EngineMetadata* meta, string_view type_name) -> optional<ComplexTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    constexpr string_view dict_suffix = "_dict";

    if (!type_name.ends_with(dict_suffix)) {
        return std::nullopt;
    }

    string_view body = type_name.substr(0, type_name.size() - dict_suffix.size());
    size_t separator = body.find('_');

    while (separator != string_view::npos) {
        string_view key_type_name = body.substr(0, separator);
        string_view value_type_name = body.substr(separator + 1);

        if (meta->IsValidBaseType(key_type_name) && meta->IsValidBaseType(value_type_name)) {
            return meta->ResolveComplexType(strex("{}=>{}", key_type_name, value_type_name));
        }

        separator = body.find('_', separator + 1);
    }

    return std::nullopt;
}

static auto ResolveWebWasmExportDictOfArrayTypeName(EngineMetadata* meta, string_view type_name) -> optional<ComplexTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    constexpr string_view dict_suffix = "_array_dict";

    if (!type_name.ends_with(dict_suffix)) {
        return std::nullopt;
    }

    string_view body = type_name.substr(0, type_name.size() - dict_suffix.size());
    size_t separator = body.find('_');

    while (separator != string_view::npos) {
        string_view key_type_name = body.substr(0, separator);
        string_view value_type_name = body.substr(separator + 1);

        if (meta->IsValidBaseType(key_type_name) && meta->IsValidBaseType(value_type_name)) {
            return meta->ResolveComplexType(strex("{}=>{}[]", key_type_name, value_type_name));
        }

        separator = body.find('_', separator + 1);
    }

    return std::nullopt;
}

static auto ResolveWebWasmExportMutableTypeName(EngineMetadata* meta, string_view type_name) -> optional<ComplexTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    constexpr string_view mut_suffix = "_mut";

    if (!type_name.ends_with(mut_suffix)) {
        return std::nullopt;
    }

    string_view base_type_name = type_name.substr(0, type_name.size() - mut_suffix.size());

    if (base_type_name.ends_with("_array_dict")) {
        string_view body = base_type_name.substr(0, base_type_name.size() - string_view {"_array_dict"}.size());
        size_t separator = body.find('_');

        while (separator != string_view::npos) {
            string_view key_type_name = body.substr(0, separator);
            string_view value_type_name = body.substr(separator + 1);

            if (meta->IsValidBaseType(key_type_name) && meta->IsValidBaseType(value_type_name)) {
                return meta->ResolveComplexType(strex("{}=>{}[]&", key_type_name, value_type_name));
            }

            separator = body.find('_', separator + 1);
        }
    }

    if (base_type_name.ends_with("_dict")) {
        string_view body = base_type_name.substr(0, base_type_name.size() - string_view {"_dict"}.size());
        size_t separator = body.find('_');

        while (separator != string_view::npos) {
            string_view key_type_name = body.substr(0, separator);
            string_view value_type_name = body.substr(separator + 1);

            if (meta->IsValidBaseType(key_type_name) && meta->IsValidBaseType(value_type_name)) {
                return meta->ResolveComplexType(strex("{}=>{}&", key_type_name, value_type_name));
            }

            separator = body.find('_', separator + 1);
        }
    }

    if (base_type_name.ends_with("_array")) {
        string_view array_base_type_name = base_type_name.substr(0, base_type_name.size() - string_view {"_array"}.size());

        if (meta->IsValidBaseType(array_base_type_name)) {
            return meta->ResolveComplexType(strex("{}[]&", array_base_type_name));
        }
    }

    if (!meta->IsValidBaseType(base_type_name)) {
        return std::nullopt;
    }

    return meta->ResolveComplexType(strex("{}&", base_type_name));
}

static auto ResolveWebWasmExportTypeName(EngineMetadata* meta, string_view type_name, bool allow_void) -> optional<ComplexTypeDesc>;

static auto SplitWebWasmExportCallbackTypeList(EngineMetadata* meta, string_view type_names) -> optional<vector<ComplexTypeDesc>>
{
    FO_STACK_TRACE_ENTRY();

    vector<ComplexTypeDesc> result;
    function<bool(string_view, bool)> split_types;
    split_types = [&](string_view remaining, bool is_return_type) -> bool {
        if (remaining.empty()) {
            return true;
        }

        size_t candidate_size = remaining.size();

        while (candidate_size != string_view::npos) {
            string_view type_name = remaining.substr(0, candidate_size);
            optional<ComplexTypeDesc> type = is_return_type && type_name == "void" ? optional<ComplexTypeDesc> {ComplexTypeDesc {}} : ResolveWebWasmExportTypeName(meta, type_name, false);

            if (type.has_value() && !type->IsMutable && !(is_return_type && type->Kind == ComplexTypeKind::Callback)) {
                if (candidate_size == remaining.size()) {
                    result.emplace_back(std::move(type.value()));
                    return true;
                }
                if (remaining[candidate_size] == '_') {
                    result.emplace_back(std::move(type.value()));

                    if (split_types(remaining.substr(candidate_size + 1), false)) {
                        return true;
                    }

                    result.pop_back();
                }
            }

            if (candidate_size == 0) {
                break;
            }

            size_t separator = remaining.rfind('_', candidate_size - 1);

            if (separator == string_view::npos) {
                break;
            }

            candidate_size = separator;
        }

        return false;
    };

    if (!split_types(type_names, true) || result.empty()) {
        return std::nullopt;
    }

    return result;
}

static auto ResolveWebWasmExportCallbackTypeName(EngineMetadata* meta, string_view type_name) -> optional<ComplexTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    constexpr string_view callback_prefix = "callback_";
    constexpr string_view callback_suffix = "_callback";

    if (!type_name.starts_with(callback_prefix) || !type_name.ends_with(callback_suffix)) {
        return std::nullopt;
    }

    string_view body = type_name.substr(callback_prefix.size(), type_name.size() - callback_prefix.size() - callback_suffix.size());
    auto callback_types = SplitWebWasmExportCallbackTypeList(meta, body);

    if (!callback_types.has_value()) {
        return std::nullopt;
    }

    return ComplexTypeDesc {
        .Kind = ComplexTypeKind::Callback,
        .CallbackArgs = SafeAlloc::MakeShared<vector<ComplexTypeDesc>>(std::move(callback_types.value())),
    };
}

static auto ResolveWebWasmExportTypeName(EngineMetadata* meta, string_view type_name, bool allow_void) -> optional<ComplexTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    if (allow_void && type_name == "void") {
        return ComplexTypeDesc {};
    }
    if (!meta->IsValidBaseType(type_name)) {
        if (type_name.ends_with("_array")) {
            string_view base_type_name = type_name.substr(0, type_name.size() - string_view {"_array"}.size());

            if (meta->IsValidBaseType(base_type_name)) {
                return meta->ResolveComplexType(strex("{}[]", base_type_name));
            }
        }
        if (auto dict_type = ResolveWebWasmExportDictTypeName(meta, type_name); dict_type.has_value()) {
            return dict_type;
        }
        if (auto dict_of_array_type = ResolveWebWasmExportDictOfArrayTypeName(meta, type_name); dict_of_array_type.has_value()) {
            return dict_of_array_type;
        }
        if (auto mutable_type = ResolveWebWasmExportMutableTypeName(meta, type_name); mutable_type.has_value()) {
            return mutable_type;
        }
        if (auto callback_type = ResolveWebWasmExportCallbackTypeName(meta, type_name); callback_type.has_value()) {
            return callback_type;
        }

        return std::nullopt;
    }

    return meta->ResolveComplexType(type_name);
}

static auto SplitWebWasmExportTypeList(EngineMetadata* meta, string_view type_names) -> optional<vector<string_view>>
{
    FO_STACK_TRACE_ENTRY();

    if (type_names == "void") {
        return vector<string_view> {};
    }

    vector<string_view> result;
    function<bool(string_view)> split_types;
    split_types = [&](string_view remaining) -> bool {
        if (remaining.empty()) {
            return true;
        }

        size_t candidate_size = remaining.size();

        while (candidate_size != string_view::npos) {
            string_view type_name = remaining.substr(0, candidate_size);

            if (ResolveWebWasmExportTypeName(meta, type_name, false).has_value()) {
                if (candidate_size == remaining.size()) {
                    result.emplace_back(type_name);
                    return true;
                }
                if (remaining[candidate_size] == '_') {
                    result.emplace_back(type_name);

                    if (split_types(remaining.substr(candidate_size + 1))) {
                        return true;
                    }

                    result.pop_back();
                }
            }

            if (candidate_size == 0) {
                break;
            }

            size_t separator = remaining.rfind('_', candidate_size - 1);

            if (separator == string_view::npos) {
                break;
            }

            candidate_size = separator;
        }

        return false;
    };

    if (!split_types(type_names)) {
        return std::nullopt;
    }

    return result;
}

struct WebWasmExportMetadataSignature
{
    string ScriptExportName {};
    vector<ComplexTypeDesc> Args {};
    ComplexTypeDesc Ret {};
    bool HasSignature {};
};

static auto ParseWebWasmExportMetadataSignature(EngineMetadata* meta, string_view export_name) -> optional<WebWasmExportMetadataSignature>
{
    FO_STACK_TRACE_ENTRY();

    size_t ret_separator = export_name.rfind("__");

    if (ret_separator == string_view::npos) {
        return WebWasmExportMetadataSignature {.ScriptExportName = string {export_name}};
    }
    if (ret_separator == 0) {
        return std::nullopt;
    }

    size_t args_separator = export_name.rfind("__", ret_separator - 1);

    if (args_separator == string_view::npos) {
        return WebWasmExportMetadataSignature {.ScriptExportName = string {export_name}};
    }
    if (args_separator == 0 || args_separator + 2 >= ret_separator || ret_separator + 2 >= export_name.size()) {
        return std::nullopt;
    }

    string_view script_export_name = export_name.substr(0, args_separator);
    string_view args_part = export_name.substr(args_separator + 2, ret_separator - args_separator - 2);
    string_view ret_part = export_name.substr(ret_separator + 2);
    auto arg_names = SplitWebWasmExportTypeList(meta, args_part);
    auto ret = ResolveWebWasmExportTypeName(meta, ret_part, true);

    if (!arg_names.has_value() || !ret.has_value()) {
        return std::nullopt;
    }

    WebWasmExportMetadataSignature signature {
        .ScriptExportName = string {script_export_name},
        .Ret = std::move(ret.value()),
        .HasSignature = true,
    };
    signature.Args.reserve(arg_names->size());

    for (const string_view arg_name : arg_names.value()) {
        auto arg = ResolveWebWasmExportTypeName(meta, arg_name, false);

        if (!arg.has_value()) {
            return std::nullopt;
        }

        signature.Args.emplace_back(std::move(arg.value()));
    }

    return signature;
}

static auto ReadTypeList(const nlohmann::json& types_json) -> optional<vector<WasmScalarKind>>
{
    FO_STACK_TRACE_ENTRY();

    if (!types_json.is_array()) {
        return std::nullopt;
    }

    vector<WasmScalarKind> types;

    for (const auto& type_json : types_json) {
        if (!type_json.is_string()) {
            return std::nullopt;
        }

        const WasmScalarKind kind = ResolveWasmScalarKind(type_json.get<string>());

        if (kind == WasmScalarKind::None) {
            return std::nullopt;
        }

        types.emplace_back(kind);
    }

    return types;
}

static void ValidateModuleImports(const nlohmann::json& module_json, string_view module_path, const vector<WasmApiMethodDesc>& api_methods, const vector<WasmApiPropertyDesc>& api_properties)
{
    FO_STACK_TRACE_ENTRY();

    if (!module_json.contains("imports")) {
        return;
    }
    if (!module_json["imports"].is_array()) {
        throw ScriptSystemException("Invalid Web WASM import manifest", module_path);
    }

    for (const auto& import_json : module_json["imports"]) {
        if (!import_json.is_object() || import_json.value("kind", string {}) != "func") {
            continue;
        }
        if (!import_json.contains("module") || !import_json["module"].is_string() || !import_json.contains("name") || !import_json["name"].is_string()) {
            throw ScriptSystemException("Invalid Web WASM import manifest entry", module_path);
        }

        const string module_name = import_json["module"].get<string>();
        const string import_name = import_json["name"].get<string>();
        const string import_full_name = strex("{}.{}", module_name, import_name);
        auto args = ReadTypeList(import_json.value("params", nlohmann::json::array()));
        auto results = ReadTypeList(import_json.value("results", nlohmann::json::array()));

        if (!args.has_value() || !results.has_value() || args->size() > MAX_CALL_ARGS || results->size() > 1) {
            throw ScriptSystemException("Unsupported Web WASM import signature", module_path, import_full_name);
        }

        const WasmImportDesc* host_desc = FindWasmImportDesc(module_name, import_name);

        if (host_desc != nullptr) {
            if (!ValidateWasmImportSignature(*host_desc, args.value(), results.value())) {
                throw ScriptSystemException("Web WASM import signature mismatch", module_path, import_full_name);
            }
            continue;
        }

        nptr<const WasmApiMethodDesc> api_desc = FindWasmApiMethodDesc(api_methods, module_name, import_name);

        if (api_desc) {
            if (!api_desc->Supported) {
                throw ScriptSystemException("Unsupported Web WASM engine API import ABI", module_path, import_full_name, api_desc->UnsupportedReason);
            }
            if (!ValidateWasmApiMethodSignature(*api_desc, args.value(), results.value())) {
                throw ScriptSystemException("Web WASM engine API import signature mismatch", module_path, import_full_name);
            }
            continue;
        }

        nptr<const WasmApiPropertyDesc> api_property_desc = FindWasmApiPropertyDesc(api_properties, module_name, import_name);

        if (api_property_desc) {
            if (!api_property_desc->Supported) {
                throw ScriptSystemException("Unsupported Web WASM engine API import ABI", module_path, import_full_name, api_property_desc->UnsupportedReason);
            }
            if (!ValidateWasmApiPropertySignature(*api_property_desc, args.value(), results.value())) {
                throw ScriptSystemException("Web WASM engine API import signature mismatch", module_path, import_full_name);
            }
            continue;
        }

        throw ScriptSystemException("Unsupported Web WASM import", module_path, import_full_name);
    }
}

static auto PackWebWasmExportBaseValue(const BaseEngine& engine, const BaseTypeDesc& type, ptr<const void> data, nptr<WasmExportRefHandleScope> ref_handles) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsEnum) {
        FO_STRONG_ASSERT(type.EnumUnderlyingType != nullptr, "Web WASM backend invariant failed");
        return PackWebWasmExportBaseValue(engine, *type.EnumUnderlyingType, data, ref_handles);
    }
    if (type.IsSimpleStruct) {
        FO_STRONG_ASSERT(type.StructLayout != nullptr, "Web WASM backend invariant failed");
        FO_STRONG_ASSERT(type.StructLayout->Fields.size() == 1, "Web WASM backend invariant failed");

        const FieldDesc& field = type.StructLayout->Fields.front();
        return PackWebWasmExportBaseValue(engine, field.Type, cast_from_void<const uint8_t*>(data).offset(field.Offset), ref_handles);
    }
    if (type.IsComplexStruct) {
        uint64_t value = 0;
        FO_STRONG_ASSERT(type.Size <= sizeof(value), "Web WASM backend invariant failed");
        MemCopy(&value, data, type.Size);
        return value;
    }
    if (IsWebWasmExportEntityHandleType(type)) {
        nptr<const Entity> entity = NativeDataProvider::ReadConstTypedHandleSlot<Entity>(data);

        if (!entity) {
            return 0;
        }
        if (entity->IsDestroyed()) {
            throw ScriptCallException("Web WASM export entity argument is destroyed", type.Name, entity->GetId().underlying_value());
        }

        ptr<const PropertyRegistrator> actual_registrator = entity->GetProperties()->GetRegistrator();

        if (!IsWebWasmExportEntityTypeCompatible(type.Name, actual_registrator->GetTypeName().as_str())) {
            throw ScriptCallException("Web WASM export entity argument type mismatch", type.Name, actual_registrator->GetTypeName());
        }

        return PackWebWasmExportIdent(entity->GetId());
    }
    if (IsWebWasmExportProtoOrFixedHandleType(type)) {
        nptr<const Entity> entity = NativeDataProvider::ReadConstTypedHandleSlot<Entity>(data);

        if (!entity) {
            return 0;
        }

        nptr<const ProtoEntity> proto = entity.dyn_cast<ProtoEntity>();

        if (!proto) {
            throw ScriptCallException("Web WASM export proto argument type mismatch", type.Name);
        }

        ptr<const PropertyRegistrator> actual_registrator = proto->GetProperties()->GetRegistrator();

        if (!IsWebWasmExportEntityTypeCompatible(type.Name, actual_registrator->GetTypeName().as_str())) {
            throw ScriptCallException("Web WASM export proto argument type mismatch", type.Name, actual_registrator->GetTypeName());
        }

        return proto->GetProtoId().as_hash();
    }
    if (type.IsRefType) {
        nptr<const void> ref = NativeDataProvider::ReadConstTypedHandleSlot<void>(data);

        if (ref_handles != nullptr) {
            ref_handles->TrackBorrowed(type, ref.get());
        }

        return numeric_cast<uint64_t>(ref.as_uintptr());
    }
    if (type.IsHashedString) {
        return cast_from_void<const hstring*>(data)->as_hash();
    }
    if (type.IsBool) {
        return *cast_from_void<const bool*>(data) ? 1 : 0;
    }
    if (type.IsInt8) {
        return std::bit_cast<uint32_t>(numeric_cast<int32_t>(*cast_from_void<const int8_t*>(data)));
    }
    if (type.IsInt16) {
        return std::bit_cast<uint32_t>(numeric_cast<int32_t>(*cast_from_void<const int16_t*>(data)));
    }
    if (type.IsInt32) {
        return std::bit_cast<uint32_t>(*cast_from_void<const int32_t*>(data));
    }
    if (type.IsInt64) {
        return std::bit_cast<uint64_t>(*cast_from_void<const int64_t*>(data));
    }
    if (type.IsUInt8) {
        return *cast_from_void<const uint8_t*>(data);
    }
    if (type.IsUInt16) {
        return *cast_from_void<const uint16_t*>(data);
    }
    if (type.IsUInt32) {
        return *cast_from_void<const uint32_t*>(data);
    }
    if (type.IsUInt64) {
        return *cast_from_void<const uint64_t*>(data);
    }
    if (type.IsSingleFloat) {
        return std::bit_cast<uint32_t>(*cast_from_void<const float32_t*>(data));
    }
    if (type.IsDoubleFloat) {
        return std::bit_cast<uint64_t>(*cast_from_void<const float64_t*>(data));
    }

    throw ScriptCallException("Unsupported Web WASM export argument type", type.Name);
}

static void UnpackWebWasmExportBaseValue(BaseEngine& engine, const BaseTypeDesc& type, uint64_t value, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsEnum) {
        FO_STRONG_ASSERT(type.EnumUnderlyingType != nullptr, "Web WASM backend invariant failed");
        UnpackWebWasmExportBaseValue(engine, *type.EnumUnderlyingType, value, data);
        return;
    }
    if (type.IsSimpleStruct) {
        FO_STRONG_ASSERT(type.StructLayout != nullptr, "Web WASM backend invariant failed");
        FO_STRONG_ASSERT(type.StructLayout->Fields.size() == 1, "Web WASM backend invariant failed");

        const FieldDesc& field = type.StructLayout->Fields.front();
        UnpackWebWasmExportBaseValue(engine, field.Type, value, cast_from_void<uint8_t*>(data).offset(field.Offset));
        return;
    }
    if (type.IsComplexStruct) {
        FO_STRONG_ASSERT(type.Size <= sizeof(value), "Web WASM backend invariant failed");
        MemCopy(data, &value, type.Size);
        return;
    }
    if (IsWebWasmExportEntityHandleType(type)) {
        NativeDataProvider::WriteTypedHandleSlot<Entity>(data, ResolveWebWasmExportEntityHandle(engine, type, value, "return"));
        return;
    }
    if (IsWebWasmExportProtoOrFixedHandleType(type)) {
        NativeDataProvider::WriteTypedHandleSlot<Entity>(data, ResolveWebWasmExportProtoHandle(engine, type, value, "return"));
        return;
    }
    if (type.IsRefType) {
        NativeDataProvider::WriteTypedHandleSlot<void>(data, reinterpret_cast<void*>(numeric_cast<uintptr_t>(value)));
        return;
    }
    if (type.IsHashedString) {
        *cast_from_void<hstring*>(data) = engine.Hashes.ResolveHash(value);
        return;
    }
    if (type.IsBool) {
        *cast_from_void<bool*>(data) = value != 0;
        return;
    }
    if (type.IsInt8) {
        *cast_from_void<int8_t*>(data) = numeric_cast<int8_t>(std::bit_cast<int32_t>(numeric_cast<uint32_t>(value)));
        return;
    }
    if (type.IsInt16) {
        *cast_from_void<int16_t*>(data) = numeric_cast<int16_t>(std::bit_cast<int32_t>(numeric_cast<uint32_t>(value)));
        return;
    }
    if (type.IsInt32) {
        *cast_from_void<int32_t*>(data) = std::bit_cast<int32_t>(numeric_cast<uint32_t>(value));
        return;
    }
    if (type.IsInt64) {
        *cast_from_void<int64_t*>(data) = std::bit_cast<int64_t>(value);
        return;
    }
    if (type.IsUInt8) {
        *cast_from_void<uint8_t*>(data) = numeric_cast<uint8_t>(value);
        return;
    }
    if (type.IsUInt16) {
        *cast_from_void<uint16_t*>(data) = numeric_cast<uint16_t>(value);
        return;
    }
    if (type.IsUInt32) {
        *cast_from_void<uint32_t*>(data) = numeric_cast<uint32_t>(value);
        return;
    }
    if (type.IsUInt64) {
        *cast_from_void<uint64_t*>(data) = value;
        return;
    }
    if (type.IsSingleFloat) {
        *cast_from_void<float32_t*>(data) = std::bit_cast<float32_t>(numeric_cast<uint32_t>(value));
        return;
    }
    if (type.IsDoubleFloat) {
        *cast_from_void<float64_t*>(data) = std::bit_cast<float64_t>(value);
        return;
    }

    throw ScriptCallException("Unsupported Web WASM export return type", type.Name);
}

static auto GetWebWasmExportTextArg(const BaseTypeDesc& type, ptr<const void> data) -> string_view;
static void StoreWebWasmExportTextResult(const BaseTypeDesc& type, string_view text, ptr<void> data);
static void WriteWebWasmExportRawValue(uint64_t value, span<uint8_t> raw_data);
static auto ReadWebWasmExportRawValue(span<const uint8_t> raw_data) noexcept -> uint64_t;

static void CopyWebWasmExportWireValueToNative(BaseEngine& engine, const BaseTypeDesc& type, span<const uint8_t> wire_data, span<uint8_t> native_data)
{
    FO_STACK_TRACE_ENTRY();

    if (wire_data.size() != type.Size || native_data.size() != type.Size) {
        throw ScriptCallException("Invalid Web WASM export array element size", type.Name, wire_data.size(), native_data.size(), type.Size);
    }

    if (type.IsHashedString) {
        hstring value = engine.Hashes.ResolveHash(ReadWebWasmExportRawValue(wire_data));
        MemCopy(native_data.data(), &value, sizeof(value));
        return;
    }
    if (type.StructLayout != nullptr) {
        for (const FieldDesc& field : type.StructLayout->Fields) {
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= wire_data.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= native_data.size(), "Web WASM backend invariant failed");
            CopyWebWasmExportWireValueToNative(engine, field.Type, wire_data.subspan(field.Offset, field.Type.Size), native_data.subspan(field.Offset, field.Type.Size));
        }
        return;
    }

    MemCopy(native_data.data(), wire_data.data(), type.Size);
}

static void CopyWebWasmExportNativeValueToWire(const BaseTypeDesc& type, span<const uint8_t> native_data, span<uint8_t> wire_data)
{
    FO_STACK_TRACE_ENTRY();

    if (wire_data.size() != type.Size || native_data.size() != type.Size) {
        throw ScriptCallException("Invalid Web WASM export array element output size", type.Name, native_data.size(), wire_data.size(), type.Size);
    }

    if (type.IsHashedString) {
        hstring value;
        MemCopy(&value, native_data.data(), sizeof(value));
        WriteWebWasmExportRawValue(value.as_hash(), wire_data);
        return;
    }
    if (type.StructLayout != nullptr) {
        for (const FieldDesc& field : type.StructLayout->Fields) {
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= native_data.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= wire_data.size(), "Web WASM backend invariant failed");
            CopyWebWasmExportNativeValueToWire(field.Type, native_data.subspan(field.Offset, field.Type.Size), wire_data.subspan(field.Offset, field.Type.Size));
        }
        return;
    }

    MemCopy(wire_data.data(), native_data.data(), type.Size);
}

using WebWasmExportArrayElementStorage = variant<WebWasmExportOpaqueValueStorage, hstring, string, any_t, Entity*, void*>;

static void AppendWebWasmExportU32(vector<uint8_t>& data, uint32_t value)
{
    FO_STACK_TRACE_ENTRY();

    size_t old_size = data.size();
    data.resize(old_size + sizeof(value));
    MemCopy(data.data() + old_size, &value, sizeof(value));
}

static auto ReadWebWasmExportU32(span<const uint8_t> data, size_t& offset, string_view role) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    if (offset + sizeof(uint32_t) > data.size()) {
        throw ScriptCallException(strex("Web WASM export {} buffer header is out of bounds", role), offset, data.size());
    }

    uint32_t value = 0;
    MemCopy(&value, data.data() + offset, sizeof(value));
    offset += sizeof(value);
    return value;
}

static auto GetWebWasmExportArrayElementWireSize(const BaseTypeDesc& type) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (IsWebWasmExportEntityHandleType(type) || IsWebWasmExportProtoOrFixedHandleType(type) || type.IsRefType || type.IsHashedString) {
        return sizeof(uint64_t);
    }
    if (IsWebWasmExportBufferElementType(type)) {
        return type.Size;
    }

    if (type.Size == 0 || type.Size > sizeof(uint64_t)) {
        throw ScriptCallException("Web WASM export array element has unsupported size", type.Name, type.Size);
    }

    return type.Size;
}

static void WriteWebWasmExportRawValue(uint64_t value, span<uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(raw_data.size() <= sizeof(value), "Web WASM backend invariant failed");
    MemCopy(raw_data.data(), &value, raw_data.size());
}

static auto ReadWebWasmExportRawValue(span<const uint8_t> raw_data) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t value = 0;
    MemCopy(&value, raw_data.data(), raw_data.size());
    return value;
}

static auto PrepareWebWasmExportArrayElementStorage(const BaseTypeDesc& type, WebWasmExportArrayElementStorage& storage) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsString) {
        if (type.Name == "any") {
            storage.emplace<any_t>();
            return make_nptr(&std::get<any_t>(storage)).void_cast();
        }

        FO_STRONG_ASSERT(type.Name == "string", "Web WASM backend invariant failed");
        storage.emplace<string>();
        return make_nptr(&std::get<string>(storage)).void_cast();
    }
    if (IsWebWasmExportEntityHandleType(type) || IsWebWasmExportProtoOrFixedHandleType(type)) {
        storage.emplace<Entity*>(nullptr);
        return make_nptr(&std::get<Entity*>(storage)).void_cast();
    }
    if (type.IsRefType) {
        storage.emplace<void*>(nullptr);
        return make_nptr(&std::get<void*>(storage)).void_cast();
    }
    if (type.IsHashedString) {
        storage.emplace<hstring>();
        return make_nptr(&std::get<hstring>(storage)).void_cast();
    }

    WebWasmExportOpaqueValueStorage& opaque = storage.emplace<WebWasmExportOpaqueValueStorage>();
    opaque.Data.fill(0);
    return make_nptr(opaque.Data.data()).void_cast();
}

static void WriteWebWasmExportFixedElement(BaseEngine& engine, const BaseTypeDesc& type, ptr<const void> data, span<uint8_t> raw_data, nptr<WasmExportRefHandleScope> ref_handles)
{
    FO_STACK_TRACE_ENTRY();

    WasmScalarKind element_kind = TryResolveWebWasmExportScalarKind(type);

    if (element_kind != WasmScalarKind::None) {
        uint64_t value = PackWebWasmExportBaseValue(engine, type, data, ref_handles);
        WriteWebWasmExportRawValue(value, raw_data);
        return;
    }
    if (IsWebWasmExportBufferElementType(type)) {
        FO_STRONG_ASSERT(raw_data.size() == type.Size, "Web WASM backend invariant failed");
        CopyWebWasmExportNativeValueToWire(type, span<const uint8_t> {cast_from_void<const uint8_t*>(data).get(), type.Size}, raw_data);
        return;
    }

    throw ScriptCallException("Unsupported Web WASM export collection element type", type.Name);
}

static void ReadWebWasmExportFixedElement(BaseEngine& engine, const BaseTypeDesc& type, span<const uint8_t> raw_data, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    WasmScalarKind element_kind = TryResolveWebWasmExportScalarKind(type);

    if (element_kind != WasmScalarKind::None) {
        uint64_t value = ReadWebWasmExportRawValue(raw_data);
        UnpackWebWasmExportBaseValue(engine, type, value, data);
        return;
    }
    if (IsWebWasmExportBufferElementType(type)) {
        FO_STRONG_ASSERT(raw_data.size() == type.Size, "Web WASM backend invariant failed");
        CopyWebWasmExportWireValueToNative(engine, type, raw_data, span<uint8_t> {cast_from_void<uint8_t*>(data).get(), type.Size});
        return;
    }

    throw ScriptCallException("Unsupported Web WASM export collection element type", type.Name);
}

static void WriteWebWasmExportDictElement(BaseEngine& engine, const BaseTypeDesc& type, ptr<const void> data, vector<uint8_t>& raw_data, nptr<WasmExportRefHandleScope> ref_handles)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsString) {
        string_view text = GetWebWasmExportTextArg(type, data);
        AppendWebWasmExportU32(raw_data, numeric_cast<uint32_t>(numeric_cast<int32_t>(text.size())));
        raw_data.insert(raw_data.end(), text.begin(), text.end());
        return;
    }

    size_t element_size = GetWebWasmExportArrayElementWireSize(type);
    size_t old_size = raw_data.size();
    raw_data.resize(old_size + element_size);

    WriteWebWasmExportFixedElement(engine, type, data, span<uint8_t> {raw_data.data(), raw_data.size()}.subspan(old_size, element_size), ref_handles);
}

static auto ReadWebWasmExportDictElement(BaseEngine& engine, const BaseTypeDesc& type, span<const uint8_t> raw_data, size_t& offset, WebWasmExportArrayElementStorage& storage, string_view role) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    ptr<void> element_data = PrepareWebWasmExportArrayElementStorage(type, storage);

    if (type.IsString) {
        uint32_t text_size = ReadWebWasmExportU32(raw_data, offset, role);

        if (offset + text_size > raw_data.size()) {
            throw ScriptCallException(strex("Web WASM export {} dict text element is out of bounds", role), type.Name, text_size, offset, raw_data.size());
        }

        StoreWebWasmExportTextResult(type, string_view {reinterpret_cast<const char*>(raw_data.data() + offset), numeric_cast<size_t>(text_size)}, element_data);
        offset += text_size;
        return element_data;
    }

    size_t element_size = GetWebWasmExportArrayElementWireSize(type);

    if (offset + element_size > raw_data.size()) {
        throw ScriptCallException(strex("Web WASM export {} dict element is out of bounds", role), type.Name, element_size, offset, raw_data.size());
    }

    ReadWebWasmExportFixedElement(engine, type, raw_data.subspan(offset, element_size), element_data);
    offset += element_size;
    return element_data;
}

static auto SerializeWebWasmExportArrayArg(BaseEngine& engine, const ComplexTypeDesc& type, ptr<const DataAccessor> accessor, ptr<void> data, nptr<WasmExportRefHandleScope> ref_handles) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWebWasmExportArrayType(type) || IsWebWasmExportMutableArrayType(type), "Web WASM backend invariant failed");
    size_t array_size = accessor->GetArraySize(data);
    vector<uint8_t> raw_data;

    if (IsWebWasmExportTextArrayType(type) || IsWebWasmExportMutableTextArrayType(type)) {
        AppendWebWasmExportU32(raw_data, numeric_cast<uint32_t>(array_size));

        for (size_t index = 0; index < array_size; index++) {
            string_view text = GetWebWasmExportTextArg(type.BaseType, accessor->GetArrayElement(data, index));
            AppendWebWasmExportU32(raw_data, numeric_cast<uint32_t>(numeric_cast<int32_t>(text.size())));
            raw_data.insert(raw_data.end(), text.begin(), text.end());
        }

        return raw_data;
    }

    size_t element_size = GetWebWasmExportArrayElementWireSize(type.BaseType);
    raw_data.resize(array_size * element_size);

    for (size_t index = 0; index < array_size; index++) {
        WriteWebWasmExportFixedElement(engine, type.BaseType, accessor->GetArrayElement(data, index), span<uint8_t> {raw_data.data(), raw_data.size()}.subspan(index * element_size, element_size), ref_handles);
    }

    return raw_data;
}

static auto SerializeWebWasmExportDictArg(BaseEngine& engine, const ComplexTypeDesc& type, ptr<const DataAccessor> accessor, ptr<void> data, nptr<WasmExportRefHandleScope> ref_handles) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWebWasmExportDictType(type) || IsWebWasmExportMutableDictType(type) || IsWebWasmExportDictOfArrayType(type) || IsWebWasmExportMutableDictOfArrayType(type), "Web WASM backend invariant failed");
    FO_STRONG_ASSERT(type.KeyType.has_value(), "Web WASM backend invariant failed");
    size_t dict_size = accessor->GetDictSize(data);
    vector<uint8_t> raw_data;
    AppendWebWasmExportU32(raw_data, numeric_cast<uint32_t>(dict_size));

    for (size_t index = 0; index < dict_size; index++) {
        const auto [key_data, value_data] = accessor->GetDictElement(data, index);
        WriteWebWasmExportDictElement(engine, *type.KeyType, key_data, raw_data, ref_handles);

        if (type.Kind == ComplexTypeKind::DictOfArray) {
            size_t array_size = accessor->GetNestedArraySize(type.BaseType, value_data);
            AppendWebWasmExportU32(raw_data, numeric_cast<uint32_t>(array_size));

            for (size_t value_index = 0; value_index < array_size; value_index++) {
                if (type.BaseType.IsBool) {
                    bool value = accessor->GetNestedArrayBoolElement(type.BaseType, value_data, value_index);
                    WriteWebWasmExportDictElement(engine, type.BaseType, make_nptr(&value).void_cast(), raw_data, ref_handles);
                }
                else {
                    WriteWebWasmExportDictElement(engine, type.BaseType, accessor->GetNestedArrayElement(type.BaseType, value_data, value_index), raw_data, ref_handles);
                }
            }
        }
        else {
            WriteWebWasmExportDictElement(engine, type.BaseType, value_data, raw_data, ref_handles);
        }
    }

    return raw_data;
}

struct WebWasmExportDictArrayValueStorage
{
    vector<WebWasmExportArrayElementStorage> ElementStorages {};
    vector<ptr<void>> ElementData {};
};

static auto ReadWebWasmExportDictArrayValue(BaseEngine& engine, const BaseTypeDesc& type, span<const uint8_t> raw_data, size_t& offset, string_view role) -> WebWasmExportDictArrayValueStorage
{
    FO_STACK_TRACE_ENTRY();

    WebWasmExportDictArrayValueStorage result;
    uint32_t array_size = ReadWebWasmExportU32(raw_data, offset, role);
    result.ElementStorages.reserve(array_size);
    result.ElementData.reserve(array_size);

    for (uint32_t index = 0; index < array_size; index++) {
        WebWasmExportArrayElementStorage& storage = result.ElementStorages.emplace_back();
        result.ElementData.emplace_back(ReadWebWasmExportDictElement(engine, type, raw_data, offset, storage, role));
    }

    return result;
}

static auto SerializeWebWasmExportValueArg(BaseEngine& engine, const ComplexTypeDesc& type, ptr<void> data, nptr<WasmExportRefHandleScope> ref_handles) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWebWasmExportValueBufferType(type) || IsWebWasmExportMutableValueType(type), "Web WASM backend invariant failed");

    size_t element_size = GetWebWasmExportArrayElementWireSize(type.BaseType);
    vector<uint8_t> raw_data(element_size);

    WriteWebWasmExportFixedElement(engine, type.BaseType, data, raw_data, ref_handles);
    return raw_data;
}

static void StoreWebWasmExportArrayResult(BaseEngine& engine, const ComplexTypeDesc& type, span<const uint8_t> raw_data, ptr<const DataAccessor> accessor, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWebWasmExportArrayType(type) || IsWebWasmExportMutableArrayType(type), "Web WASM backend invariant failed");
    accessor->ClearArray(data);

    if (IsWebWasmExportTextArrayType(type) || IsWebWasmExportMutableTextArrayType(type)) {
        size_t offset = 0;
        uint32_t array_size = ReadWebWasmExportU32(raw_data, offset, "text return");

        for (uint32_t index = 0; index < array_size; index++) {
            uint32_t text_size = ReadWebWasmExportU32(raw_data, offset, "text return");

            if (offset + text_size > raw_data.size()) {
                throw ScriptCallException("Web WASM export text array return element is out of bounds", type.BaseType.Name, index, text_size, offset, raw_data.size());
            }

            WebWasmExportArrayElementStorage storage;
            ptr<void> element_data = PrepareWebWasmExportArrayElementStorage(type.BaseType, storage);
            StoreWebWasmExportTextResult(type.BaseType, string_view {reinterpret_cast<const char*>(raw_data.data() + offset), numeric_cast<size_t>(text_size)}, element_data);
            accessor->AddArrayElement(data, element_data);
            offset += text_size;
        }

        if (offset != raw_data.size()) {
            throw ScriptCallException("Web WASM export text array return has trailing bytes", type.BaseType.Name, offset, raw_data.size());
        }

        return;
    }

    size_t element_size = GetWebWasmExportArrayElementWireSize(type.BaseType);

    if (raw_data.size() % element_size != 0) {
        throw ScriptCallException("Web WASM export array return buffer length is not aligned to element size", type.BaseType.Name, raw_data.size(), element_size);
    }

    size_t array_size = raw_data.size() / element_size;

    for (size_t index = 0; index < array_size; index++) {
        WebWasmExportArrayElementStorage storage;
        ptr<void> element_data = PrepareWebWasmExportArrayElementStorage(type.BaseType, storage);
        ReadWebWasmExportFixedElement(engine, type.BaseType, raw_data.subspan(index * element_size, element_size), element_data);
        accessor->AddArrayElement(data, element_data);
    }
}

static void StoreWebWasmExportValueResult(BaseEngine& engine, const ComplexTypeDesc& type, span<const uint8_t> raw_data, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWebWasmExportValueBufferType(type) || IsWebWasmExportMutableValueType(type), "Web WASM backend invariant failed");

    size_t element_size = GetWebWasmExportArrayElementWireSize(type.BaseType);

    if (raw_data.size() != element_size) {
        throw ScriptCallException("Web WASM export value buffer length does not match element size", type.BaseType.Name, raw_data.size(), element_size);
    }

    ReadWebWasmExportFixedElement(engine, type.BaseType, raw_data, data);
}

static void StoreWebWasmExportDictResult(BaseEngine& engine, const ComplexTypeDesc& type, span<const uint8_t> raw_data, ptr<const DataAccessor> accessor, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWebWasmExportDictType(type) || IsWebWasmExportMutableDictType(type) || IsWebWasmExportDictOfArrayType(type) || IsWebWasmExportMutableDictOfArrayType(type), "Web WASM backend invariant failed");
    FO_STRONG_ASSERT(type.KeyType.has_value(), "Web WASM backend invariant failed");
    accessor->ClearDict(data);

    if (raw_data.empty()) {
        return;
    }

    size_t offset = 0;
    uint32_t dict_size = ReadWebWasmExportU32(raw_data, offset, "dict return");

    for (uint32_t index = 0; index < dict_size; index++) {
        WebWasmExportArrayElementStorage key_storage;
        WebWasmExportArrayElementStorage value_storage;
        ptr<void> key_data = ReadWebWasmExportDictElement(engine, *type.KeyType, raw_data, offset, key_storage, "dict return key");

        if (type.Kind == ComplexTypeKind::DictOfArray) {
            WebWasmExportDictArrayValueStorage value_array = ReadWebWasmExportDictArrayValue(engine, type.BaseType, raw_data, offset, "dict return value array");
            accessor->AddDictArrayElement(data, key_data, type.BaseType, value_array.ElementData);
        }
        else {
            ptr<void> value_data = ReadWebWasmExportDictElement(engine, type.BaseType, raw_data, offset, value_storage, "dict return value");
            accessor->AddDictElement(data, key_data, value_data);
        }
    }

    if (offset != raw_data.size()) {
        throw ScriptCallException("Web WASM export dict return has trailing bytes", type.BaseType.Name, offset, raw_data.size());
    }
}

static auto GetWebWasmExportTextArg(const BaseTypeDesc& type, ptr<const void> data) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.IsString, "Web WASM backend invariant failed");

    if (type.Name == "any") {
        return *cast_from_void<const any_t*>(data);
    }

    FO_STRONG_ASSERT(type.Name == "string", "Web WASM backend invariant failed");
    return *cast_from_void<const string*>(data);
}

static auto GetWebWasmExportCallbackArg(nptr<ScriptSystem> script_sys, ptr<const DataAccessor> accessor, ptr<void> data) -> string
{
    FO_STACK_TRACE_ENTRY();

    unique_del_nptr<ScriptFuncDesc> callback = accessor->GetCallback(data);

    if (!callback) {
        return {};
    }
    if (callback->DelegateObj != 0) {
        if (!script_sys) {
            throw ScriptCallException("Web WASM export callback delegate has no owning script system", callback->Name);
        }

        return script_sys->RegisterTemporaryScriptCallback(take_not_null(callback));
    }

    return string {callback->Name.as_str()};
}

static auto UnpackWebWasmExportTextPointerLength(uint64_t raw_value) noexcept -> pair<uint32_t, uint32_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    uint32_t ptr = numeric_cast<uint32_t>(raw_value & 0xFFFFFFFF);
    uint32_t len = numeric_cast<uint32_t>(raw_value >> 32);
    return {ptr, len};
}

static void StoreWebWasmExportTextResult(const BaseTypeDesc& type, string_view text, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.IsString, "Web WASM backend invariant failed");

    if (type.Name == "any") {
        *cast_from_void<any_t*>(data) = any_t {string {text}};
        return;
    }

    FO_STRONG_ASSERT(type.Name == "string", "Web WASM backend invariant failed");
    *cast_from_void<string*>(data) = string {text};
}

static void FreeWebWasmExportTextResult(uint32_t text_ptr) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (text_ptr != 0) {
        std::free(reinterpret_cast<void*>(static_cast<uintptr_t>(text_ptr)));
    }
}

static void ReadWebWasmExportTextResult(const BaseTypeDesc& type, uint64_t raw_value, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    const auto [text_ptr, text_len] = UnpackWebWasmExportTextPointerLength(raw_value);
    (void)numeric_cast<int32_t>(text_len);
    scope_exit free_text {[text_ptr]() noexcept { FreeWebWasmExportTextResult(text_ptr); }};

    if (text_len == 0) {
        StoreWebWasmExportTextResult(type, {}, data);
        return;
    }

    const char* text_data = reinterpret_cast<const char*>(static_cast<uintptr_t>(text_ptr));

    if (text_data == nullptr) {
        throw ScriptCallException("Web WASM export text return pointer is null", type.Name, text_len);
    }

    StoreWebWasmExportTextResult(type, string_view {text_data, numeric_cast<size_t>(text_len)}, data);
}

static void ReadWebWasmExportArrayResult(BaseEngine& engine, const ComplexTypeDesc& type, uint64_t raw_value, ptr<const DataAccessor> accessor, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    const auto [array_ptr, array_len] = UnpackWebWasmExportTextPointerLength(raw_value);
    (void)numeric_cast<int32_t>(array_len);
    scope_exit free_array {[array_ptr]() noexcept { FreeWebWasmExportTextResult(array_ptr); }};

    if (array_len == 0) {
        StoreWebWasmExportArrayResult(engine, type, {}, accessor, data);
        return;
    }

    const uint8_t* array_data = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(array_ptr));

    if (array_data == nullptr) {
        throw ScriptCallException("Web WASM export array return pointer is null", type.BaseType.Name, array_len);
    }

    StoreWebWasmExportArrayResult(engine, type, {array_data, numeric_cast<size_t>(array_len)}, accessor, data);
}

static void ReadWebWasmExportValueResult(BaseEngine& engine, const ComplexTypeDesc& type, uint64_t raw_value, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    const auto [value_ptr, value_len] = UnpackWebWasmExportTextPointerLength(raw_value);
    (void)numeric_cast<int32_t>(value_len);
    scope_exit free_value {[value_ptr]() noexcept { FreeWebWasmExportTextResult(value_ptr); }};
    size_t element_size = GetWebWasmExportArrayElementWireSize(type.BaseType);

    if (value_len != element_size) {
        throw ScriptCallException("Web WASM export value return buffer length does not match element size", type.BaseType.Name, value_len, element_size);
    }

    const uint8_t* value_data = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(value_ptr));

    if (value_data == nullptr) {
        throw ScriptCallException("Web WASM export value return pointer is null", type.BaseType.Name, value_len);
    }

    StoreWebWasmExportValueResult(engine, type, {value_data, numeric_cast<size_t>(value_len)}, data);
}

static void ReadWebWasmExportDictResult(BaseEngine& engine, const ComplexTypeDesc& type, uint64_t raw_value, ptr<const DataAccessor> accessor, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    const auto [dict_ptr, dict_len] = UnpackWebWasmExportTextPointerLength(raw_value);
    (void)numeric_cast<int32_t>(dict_len);
    scope_exit free_dict {[dict_ptr]() noexcept { FreeWebWasmExportTextResult(dict_ptr); }};

    if (dict_len == 0) {
        StoreWebWasmExportDictResult(engine, type, {}, accessor, data);
        return;
    }

    const uint8_t* dict_data = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(dict_ptr));

    if (dict_data == nullptr) {
        throw ScriptCallException("Web WASM export dict return pointer is null", type.BaseType.Name, dict_len);
    }

    StoreWebWasmExportDictResult(engine, type, {dict_data, numeric_cast<size_t>(dict_len)}, accessor, data);
}

static auto PackArgValue(WasmScalarKind kind, const ComplexTypeDesc& type, const BaseEngine& engine, ptr<void> data, nptr<WasmExportRefHandleScope> ref_handles) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(kind);
    return PackWebWasmExportBaseValue(engine, type.BaseType, data, ref_handles);
}

static void UnpackReturnValue(WasmScalarKind kind, const ComplexTypeDesc& type, BaseEngine& engine, uint64_t value, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(kind);
    UnpackWebWasmExportBaseValue(engine, type.BaseType, value, data);
}

WebWasmBackend::WebWasmBackend(ptr<const ScriptSettings> settings)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(settings);
}

WebWasmBackend::~WebWasmBackend()
{
    FO_STACK_TRACE_ENTRY();
}

void WebWasmBackend::RegisterMetadata(ptr<EngineMetadata> meta)
{
    FO_STACK_TRACE_ENTRY();

    _meta = meta;
    _engine = meta.dyn_cast<BaseEngine>();
    _scriptSys = meta.dyn_cast<ScriptSystem>();
    _apiImports = BuildWasmApiImportTable(*meta);

    FO_VERIFY_AND_THROW(_engine, "Missing engine instance");

    size_t supported_methods = CountWasmApiSupportedMethods(_apiImports);
    size_t supported_properties = CountWasmApiSupportedProperties(_apiImports);
    size_t pending_imports = (_apiImports.Methods.size() - supported_methods) + (_apiImports.Properties.size() - supported_properties);

    WriteLog("Prepared Web WASM {} metadata API bridge: {} supported method import{}, {} supported property import{}, {} pending ABI import{}", GetWasmApiImportTableSideName(_apiImports.Side), supported_methods, supported_methods != 1 ? "s" : "", supported_properties, supported_properties != 1 ? "s" : "", pending_imports, pending_imports != 1 ? "s" : "");

    RegisterApiImports();
}

void WebWasmBackend::LoadScripts(const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(resources);
    FO_VERIFY_AND_THROW(_meta, "Missing engine metadata");

    string manifest_text = TakeJsString(WebWasmGetManifestJsonImpl());

    if (manifest_text.empty()) {
        return;
    }

    auto manifest_json = nlohmann::json::parse(manifest_text);

    if (!manifest_json.is_object() || !manifest_json.contains("modules") || !manifest_json["modules"].is_array()) {
        throw ScriptSystemException("Invalid Web WASM manifest");
    }

    for (const auto& module_json : manifest_json["modules"]) {
        if (!module_json.is_object() || !module_json.contains("name") || !module_json["name"].is_string()) {
            throw ScriptSystemException("Invalid Web WASM module manifest entry");
        }

        auto module = SafeAlloc::MakeUnique<WasmModule>();
        module->Name = module_json["name"].get<string>();
        module->Path = module_json.value("path", string {});

        ValidateModuleImports(module_json, module->Path, _apiImports.Methods, _apiImports.Properties);
        RegisterModule(module, module_json);
        _modules.emplace_back(std::move(module));
    }
}

void WebWasmBackend::RegisterModule(ptr<WasmModule> module, const nlohmann::json& module_json)
{
    FO_STACK_TRACE_ENTRY();

    if (_scriptSys == nullptr) {
        return;
    }

    if (!module_json.contains("exports") || !module_json["exports"].is_array()) {
        return;
    }

    for (const auto& export_json : module_json["exports"]) {
        if (!export_json.is_object() || export_json.value("kind", string {}) != "func") {
            continue;
        }
        if (!export_json.contains("name") || !export_json["name"].is_string()) {
            continue;
        }

        const string export_name = export_json["name"].get<string>();
        auto args = ReadTypeList(export_json.value("params", nlohmann::json::array()));
        auto results = ReadTypeList(export_json.value("results", nlohmann::json::array()));

        if (!args.has_value() || !results.has_value() || results->size() > 1 || args->size() > MAX_CALL_ARGS) {
            WriteLog(LogType::Warning, "Skip Web WASM export {} in {}: unsupported signature", export_name, module->Path);
            continue;
        }

        auto export_signature = ParseWebWasmExportMetadataSignature(_meta.get(), export_name);

        if (!export_signature.has_value()) {
            WriteLog(LogType::Warning, "Skip Web WASM export {} in {}: unsupported metadata signature", export_name, module->Path);
            continue;
        }

        auto function = SafeAlloc::MakeUnique<WasmFunction>();
        function->ExportName = export_name;
        function->ScriptName = strex("{}::{}", module->Name, export_signature->ScriptExportName);
        function->Args = std::move(args.value());

        auto func_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        func_desc->Name = _meta->Hashes.ToHashedString(function->ScriptName);

        bool supported_signature = true;

        if (export_signature->HasSignature) {
            if (export_signature->Ret ? results->size() != 1 : !results->empty()) {
                supported_signature = false;
            }

            size_t param_index = 0;

            for (size_t arg_index = 0; supported_signature && arg_index < export_signature->Args.size(); arg_index++) {
                const ComplexTypeDesc& arg_type = export_signature->Args[arg_index];

                if (IsWebWasmExportTextType(arg_type)) {
                    if (param_index + 1 >= function->Args.size()) {
                        supported_signature = false;
                        break;
                    }
                    if (function->Args[param_index] != WasmScalarKind::I32 || function->Args[param_index + 1] != WasmScalarKind::I32) {
                        supported_signature = false;
                        break;
                    }

                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::Utf8StringPointer);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::Utf8StringLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }
                if (IsWebWasmExportCallbackType(arg_type)) {
                    if (param_index + 1 >= function->Args.size()) {
                        supported_signature = false;
                        break;
                    }
                    if (function->Args[param_index] != WasmScalarKind::I32 || function->Args[param_index + 1] != WasmScalarKind::I32) {
                        supported_signature = false;
                        break;
                    }

                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::CallbackPointer);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::CallbackLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }
                if (IsWebWasmExportValueBufferType(arg_type)) {
                    if (param_index + 1 >= function->Args.size()) {
                        supported_signature = false;
                        break;
                    }
                    if (function->Args[param_index] != WasmScalarKind::I32 || function->Args[param_index + 1] != WasmScalarKind::I32) {
                        supported_signature = false;
                        break;
                    }

                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::ValuePointer);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::ValueByteLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }
                if (IsWebWasmExportMutableTextType(arg_type)) {
                    if (param_index + 3 >= function->Args.size()) {
                        supported_signature = false;
                        break;
                    }
                    if (function->Args[param_index] != WasmScalarKind::I32 || function->Args[param_index + 1] != WasmScalarKind::I32 || function->Args[param_index + 2] != WasmScalarKind::I32 || function->Args[param_index + 3] != WasmScalarKind::I32) {
                        supported_signature = false;
                        break;
                    }

                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringPointer);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringByteLength);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringCapacityByteLength);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringRequiredByteLengthPointer);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 4;
                    continue;
                }
                if (IsWebWasmExportArrayType(arg_type)) {
                    if (param_index + 1 >= function->Args.size()) {
                        supported_signature = false;
                        break;
                    }
                    if (function->Args[param_index] != WasmScalarKind::I32 || function->Args[param_index + 1] != WasmScalarKind::I32) {
                        supported_signature = false;
                        break;
                    }

                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::ArrayPointer);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::ArrayByteLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }
                if (IsWebWasmExportMutableArrayType(arg_type)) {
                    if (param_index + 3 >= function->Args.size()) {
                        supported_signature = false;
                        break;
                    }
                    if (function->Args[param_index] != WasmScalarKind::I32 || function->Args[param_index + 1] != WasmScalarKind::I32 || function->Args[param_index + 2] != WasmScalarKind::I32 || function->Args[param_index + 3] != WasmScalarKind::I32) {
                        supported_signature = false;
                        break;
                    }

                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableArrayPointer);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableArrayByteLength);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableArrayCapacityByteLength);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 4;
                    continue;
                }
                if (IsWebWasmExportMutableDictType(arg_type) || IsWebWasmExportMutableDictOfArrayType(arg_type)) {
                    if (param_index + 3 >= function->Args.size()) {
                        supported_signature = false;
                        break;
                    }
                    if (function->Args[param_index] != WasmScalarKind::I32 || function->Args[param_index + 1] != WasmScalarKind::I32 || function->Args[param_index + 2] != WasmScalarKind::I32 || function->Args[param_index + 3] != WasmScalarKind::I32) {
                        supported_signature = false;
                        break;
                    }

                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableDictPointer);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableDictByteLength);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableDictCapacityByteLength);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 4;
                    continue;
                }
                if (IsWebWasmExportDictType(arg_type) || IsWebWasmExportDictOfArrayType(arg_type)) {
                    if (param_index + 1 >= function->Args.size()) {
                        supported_signature = false;
                        break;
                    }
                    if (function->Args[param_index] != WasmScalarKind::I32 || function->Args[param_index + 1] != WasmScalarKind::I32) {
                        supported_signature = false;
                        break;
                    }

                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::DictPointer);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::DictByteLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }
                if (IsWebWasmExportMutableValueType(arg_type)) {
                    if (param_index + 1 >= function->Args.size()) {
                        supported_signature = false;
                        break;
                    }
                    if (function->Args[param_index] != WasmScalarKind::I32 || function->Args[param_index + 1] != WasmScalarKind::I32) {
                        supported_signature = false;
                        break;
                    }

                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableValuePointer);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableValueLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }

                if (param_index >= function->Args.size()) {
                    supported_signature = false;
                    break;
                }

                const WasmScalarKind actual_kind = function->Args[param_index];
                const WasmScalarKind expected_kind = TryResolveWebWasmExportScalarKind(arg_type);

                if (actual_kind == WasmScalarKind::None || expected_kind == WasmScalarKind::None || actual_kind != expected_kind) {
                    supported_signature = false;
                    break;
                }

                function->ArgAbi.emplace_back(WasmApiParamAbiKind::Scalar);
                function->ArgTypes.emplace_back(arg_type);
                func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                param_index++;
            }

            if (supported_signature && param_index != function->Args.size()) {
                supported_signature = false;
            }

            if (supported_signature && export_signature->Ret) {
                const WasmScalarKind actual_kind = results->front();

                if (IsWebWasmExportTextType(export_signature->Ret)) {
                    if (actual_kind != WasmScalarKind::I64) {
                        supported_signature = false;
                    }
                    else {
                        function->Ret = actual_kind;
                        function->RetType = export_signature->Ret;
                        func_desc->Ret = export_signature->Ret;
                    }
                }
                else if (IsWebWasmExportValueBufferType(export_signature->Ret)) {
                    if (actual_kind != WasmScalarKind::I64) {
                        supported_signature = false;
                    }
                    else {
                        function->Ret = actual_kind;
                        function->RetType = export_signature->Ret;
                        func_desc->Ret = export_signature->Ret;
                    }
                }
                else if (IsWebWasmExportArrayType(export_signature->Ret)) {
                    if (actual_kind != WasmScalarKind::I64) {
                        supported_signature = false;
                    }
                    else {
                        function->Ret = actual_kind;
                        function->RetType = export_signature->Ret;
                        func_desc->Ret = export_signature->Ret;
                    }
                }
                else if (IsWebWasmExportDictType(export_signature->Ret) || IsWebWasmExportDictOfArrayType(export_signature->Ret)) {
                    if (actual_kind != WasmScalarKind::I64) {
                        supported_signature = false;
                    }
                    else {
                        function->Ret = actual_kind;
                        function->RetType = export_signature->Ret;
                        func_desc->Ret = export_signature->Ret;
                    }
                }
                else {
                    const WasmScalarKind expected_kind = TryResolveWebWasmExportScalarKind(export_signature->Ret);

                    if (actual_kind == WasmScalarKind::None || expected_kind == WasmScalarKind::None || actual_kind != expected_kind) {
                        supported_signature = false;
                    }
                    else {
                        function->Ret = actual_kind;
                        function->RetType = export_signature->Ret;
                        func_desc->Ret = export_signature->Ret;
                    }
                }
            }
        }
        else {
            for (size_t arg_index = 0; arg_index < function->Args.size(); arg_index++) {
                auto arg_desc = MakeArgDesc(_meta.get(), function->Args[arg_index], arg_index);

                if (!arg_desc.has_value()) {
                    supported_signature = false;
                    break;
                }

                ArgDesc arg_desc_value = std::move(arg_desc.value());
                function->ArgAbi.emplace_back(WasmApiParamAbiKind::Scalar);
                function->ArgTypes.emplace_back(arg_desc_value.Type);
                func_desc->Args.emplace_back(std::move(arg_desc_value));
            }

            if (supported_signature && results->size() == 1) {
                function->Ret = results->front();
                auto ret_desc = ResolveEngineType(_meta.get(), function->Ret);

                if (!ret_desc.has_value()) {
                    supported_signature = false;
                }
                else {
                    function->RetType = ret_desc.value();
                    func_desc->Ret = std::move(ret_desc.value());
                }
            }
        }

        if (!supported_signature) {
            WriteLog(LogType::Warning, "Skip Web WASM export {} in {}: unsupported value type", function->ExportName, module->Path);
            continue;
        }

        ptr<const WasmModule> registered_module = module;
        ptr<const WasmFunction> registered_function = function;
        func_desc->Call = [this, registered_module, registered_function](FuncCallData& call) { CallWasmFunction(registered_module, registered_function, call); };
        func_desc->AttributeChecker = [](string_view attribute) noexcept -> bool { return attribute == "Wasm"; };

        function->FuncDesc = std::move(func_desc);
        _scriptSys->AddGlobalScriptFunc(function->FuncDesc.get());
        module->Functions.emplace_back(std::move(function));
    }

    if (!module->Functions.empty()) {
        WriteLog("Loaded Web WASM module {} with {} function{}", module->Path, module->Functions.size(), module->Functions.size() != 1 ? "s" : "");
    }
}

void WebWasmBackend::RegisterApiImports()
{
    FO_STACK_TRACE_ENTRY();

    array<int32_t, MAX_CALL_ARGS> arg_kinds {};
    array<int32_t, MAX_CALL_ARGS> param_abi {};
    uintptr_t backend_ptr = reinterpret_cast<uintptr_t>(this);
    uintptr_t method_callback_ptr = reinterpret_cast<uintptr_t>(&WebWasmBackend::CallApiImportCallback);
    uintptr_t property_callback_ptr = reinterpret_cast<uintptr_t>(&WebWasmBackend::CallPropertyImportCallback);

    for (size_t method_index = 0; method_index < _apiImports.Methods.size(); method_index++) {
        const WasmApiMethodDesc& method = _apiImports.Methods[method_index];

        if (!method.Supported) {
            continue;
        }

        for (size_t arg_index = 0; arg_index < method.Args.size(); arg_index++) {
            arg_kinds[arg_index] = static_cast<int32_t>(method.Args[arg_index]);
            param_abi[arg_index] = static_cast<int32_t>(method.ParamAbi[arg_index]);
        }

        int32_t registered = WebWasmRegisterApiImportImpl(method.Name.c_str(), numeric_cast<int32_t>(method.Args.size()), arg_kinds.data(), param_abi.data(), static_cast<int32_t>(method.Ret), backend_ptr, numeric_cast<int32_t>(method_index), method_callback_ptr);

        if (registered == 0) {
            throw ScriptSystemException("Web WASM API import registration failed", method.Name);
        }
    }

    for (size_t property_index = 0; property_index < _apiImports.Properties.size(); property_index++) {
        const WasmApiPropertyDesc& property = _apiImports.Properties[property_index];

        if (!property.Supported) {
            continue;
        }

        for (size_t arg_index = 0; arg_index < property.ArgsCount; arg_index++) {
            arg_kinds[arg_index] = static_cast<int32_t>(property.Args[arg_index]);
            param_abi[arg_index] = static_cast<int32_t>(property.ParamAbi[arg_index]);
        }

        int32_t registered = WebWasmRegisterApiImportImpl(property.Name.c_str(), numeric_cast<int32_t>(property.ArgsCount), arg_kinds.data(), param_abi.data(), static_cast<int32_t>(property.Ret), backend_ptr, numeric_cast<int32_t>(property_index), property_callback_ptr);

        if (registered == 0) {
            throw ScriptSystemException("Web WASM API property import registration failed", property.Name);
        }
    }
}

void WebWasmBackend::CallWasmFunction(ptr<const WasmModule> module, ptr<const WasmFunction> function, FuncCallData& call) const
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(function->ArgTypes.size() == call.ArgsData.size(), "Web WASM backend invariant failed");
    FO_STRONG_ASSERT(function->ArgAbi.size() == function->Args.size(), "Web WASM backend invariant failed");

    array<int32_t, MAX_CALL_ARGS> arg_kinds {};
    array<int32_t, MAX_CALL_ARGS> arg_abi {};
    array<uint64_t, MAX_CALL_ARGS> arg_values {};
    vector<vector<uint8_t>> array_arg_buffers;
    array_arg_buffers.reserve(function->ArgTypes.size());
    vector<string> callback_arg_buffers;
    callback_arg_buffers.reserve(function->ArgTypes.size());
    WasmExportRefHandleScope ref_handle_scope;
    struct MutableValueCopyback
    {
        size_t BufferIndex {};
        ComplexTypeDesc Type {};
        ptr<void> Data;
    };
    struct MutableTextCopyback
    {
        size_t BufferIndex {};
        size_t RequiredLengthIndex {};
        ComplexTypeDesc Type {};
        ptr<void> Data;
    };
    struct MutableArrayCopyback
    {
        size_t BufferIndex {};
        size_t RequiredLengthIndex {};
        ComplexTypeDesc Type {};
        ptr<void> Data;
    };
    struct MutableDictCopyback
    {
        size_t BufferIndex {};
        size_t RequiredLengthIndex {};
        ComplexTypeDesc Type {};
        ptr<void> Data;
    };
    vector<MutableValueCopyback> mutable_value_copybacks;
    vector<MutableTextCopyback> mutable_text_copybacks;
    vector<MutableArrayCopyback> mutable_array_copybacks;
    vector<MutableDictCopyback> mutable_dict_copybacks;
    vector<uint32_t> mutable_text_required_lengths;
    mutable_text_required_lengths.reserve(function->ArgTypes.size());
    vector<uint32_t> mutable_array_required_lengths;
    mutable_array_required_lengths.reserve(function->ArgTypes.size());
    vector<uint32_t> mutable_dict_required_lengths;
    mutable_dict_required_lengths.reserve(function->ArgTypes.size());
    nptr<ScriptSystem> script_sys = _scriptSys;
    size_t callback_scope = 0;
    if (script_sys) {
        callback_scope = script_sys->PushTemporaryScriptCallbackScope();
    }
    auto pop_callback_scope = scope_exit([script_sys, callback_scope]() mutable noexcept {
        if (script_sys) {
            script_sys->PopTemporaryScriptCallbackScope(callback_scope);
        }
    });

    for (size_t i = 0; i < function->Args.size(); i++) {
        arg_kinds[i] = static_cast<int32_t>(function->Args[i]);
        arg_abi[i] = static_cast<int32_t>(function->ArgAbi[i]);
    }

    size_t raw_arg_index = 0;

    for (size_t arg_index = 0; arg_index < function->ArgTypes.size(); arg_index++) {
        FO_STRONG_ASSERT(raw_arg_index < function->Args.size(), "Web WASM backend invariant failed");

        WasmApiParamAbiKind abi = function->ArgAbi[raw_arg_index];

        if (abi == WasmApiParamAbiKind::Utf8StringPointer) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::Utf8StringLength, "Web WASM backend invariant failed");

            string_view text = GetWebWasmExportTextArg(function->ArgTypes[arg_index].BaseType, call.ArgsData[arg_index]);
            uint32_t text_len = numeric_cast<uint32_t>(numeric_cast<int32_t>(text.size()));
            arg_values[raw_arg_index] = text.empty() ? 0 : numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(text.data()));
            arg_values[raw_arg_index + 1] = text_len;
            raw_arg_index += 2;
            continue;
        }
        if (abi == WasmApiParamAbiKind::CallbackPointer) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::CallbackLength, "Web WASM backend invariant failed");

            callback_arg_buffers.emplace_back(GetWebWasmExportCallbackArg(script_sys, call.Accessor, call.ArgsData[arg_index]));
            const string& callback_name = callback_arg_buffers.back();
            arg_values[raw_arg_index] = callback_name.empty() ? 0 : numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(callback_name.data()));
            arg_values[raw_arg_index + 1] = numeric_cast<uint32_t>(numeric_cast<int32_t>(callback_name.size()));
            raw_arg_index += 2;
            continue;
        }
        if (abi == WasmApiParamAbiKind::ValuePointer) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::ValueByteLength, "Web WASM backend invariant failed");

            array_arg_buffers.emplace_back(SerializeWebWasmExportValueArg(*_engine.get_no_const(), function->ArgTypes[arg_index], call.ArgsData[arg_index], &ref_handle_scope));
            const vector<uint8_t>& raw_data = array_arg_buffers.back();
            arg_values[raw_arg_index] = raw_data.empty() ? 0 : numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_data.data()));
            arg_values[raw_arg_index + 1] = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
            raw_arg_index += 2;
            continue;
        }
        if (abi == WasmApiParamAbiKind::MutableUtf8StringPointer) {
            FO_STRONG_ASSERT(raw_arg_index + 3 < function->Args.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableUtf8StringByteLength, "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 2] == WasmApiParamAbiKind::MutableUtf8StringCapacityByteLength, "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 3] == WasmApiParamAbiKind::MutableUtf8StringRequiredByteLengthPointer, "Web WASM backend invariant failed");

            string_view text = GetWebWasmExportTextArg(function->ArgTypes[arg_index].BaseType, call.ArgsData[arg_index]);
            array_arg_buffers.emplace_back();
            vector<uint8_t>& raw_data = array_arg_buffers.back();
            raw_data.insert(raw_data.end(), text.begin(), text.end());
            mutable_text_required_lengths.emplace_back(numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size())));
            mutable_text_copybacks.emplace_back(MutableTextCopyback {
                .BufferIndex = array_arg_buffers.size() - 1,
                .RequiredLengthIndex = mutable_text_required_lengths.size() - 1,
                .Type = function->ArgTypes[arg_index],
                .Data = call.ArgsData[arg_index],
            });
            arg_values[raw_arg_index] = raw_data.empty() ? 0 : numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_data.data()));
            arg_values[raw_arg_index + 1] = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
            arg_values[raw_arg_index + 2] = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
            arg_values[raw_arg_index + 3] = numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&mutable_text_required_lengths.back()));
            raw_arg_index += 4;
            continue;
        }
        if (abi == WasmApiParamAbiKind::ArrayPointer) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::ArrayByteLength, "Web WASM backend invariant failed");

            array_arg_buffers.emplace_back(SerializeWebWasmExportArrayArg(*_engine.get_no_const(), function->ArgTypes[arg_index], call.Accessor, call.ArgsData[arg_index], &ref_handle_scope));
            const vector<uint8_t>& raw_data = array_arg_buffers.back();
            arg_values[raw_arg_index] = raw_data.empty() ? 0 : numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_data.data()));
            arg_values[raw_arg_index + 1] = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
            raw_arg_index += 2;
            continue;
        }
        if (abi == WasmApiParamAbiKind::MutableArrayPointer) {
            FO_STRONG_ASSERT(raw_arg_index + 3 < function->Args.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableArrayByteLength, "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 2] == WasmApiParamAbiKind::MutableArrayCapacityByteLength, "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 3] == WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer, "Web WASM backend invariant failed");

            array_arg_buffers.emplace_back(SerializeWebWasmExportArrayArg(*_engine.get_no_const(), function->ArgTypes[arg_index], call.Accessor, call.ArgsData[arg_index], &ref_handle_scope));
            const vector<uint8_t>& raw_data = array_arg_buffers.back();
            mutable_array_required_lengths.emplace_back(numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size())));
            mutable_array_copybacks.emplace_back(MutableArrayCopyback {
                .BufferIndex = array_arg_buffers.size() - 1,
                .RequiredLengthIndex = mutable_array_required_lengths.size() - 1,
                .Type = function->ArgTypes[arg_index],
                .Data = call.ArgsData[arg_index],
            });
            arg_values[raw_arg_index] = raw_data.empty() ? 0 : numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_data.data()));
            arg_values[raw_arg_index + 1] = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
            arg_values[raw_arg_index + 2] = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
            arg_values[raw_arg_index + 3] = numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&mutable_array_required_lengths.back()));
            raw_arg_index += 4;
            continue;
        }
        if (abi == WasmApiParamAbiKind::MutableDictPointer) {
            FO_STRONG_ASSERT(raw_arg_index + 3 < function->Args.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableDictByteLength, "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 2] == WasmApiParamAbiKind::MutableDictCapacityByteLength, "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 3] == WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer, "Web WASM backend invariant failed");

            array_arg_buffers.emplace_back(SerializeWebWasmExportDictArg(*_engine.get_no_const(), function->ArgTypes[arg_index], call.Accessor, call.ArgsData[arg_index], &ref_handle_scope));
            const vector<uint8_t>& raw_data = array_arg_buffers.back();
            mutable_dict_required_lengths.emplace_back(numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size())));
            mutable_dict_copybacks.emplace_back(MutableDictCopyback {
                .BufferIndex = array_arg_buffers.size() - 1,
                .RequiredLengthIndex = mutable_dict_required_lengths.size() - 1,
                .Type = function->ArgTypes[arg_index],
                .Data = call.ArgsData[arg_index],
            });
            arg_values[raw_arg_index] = raw_data.empty() ? 0 : numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_data.data()));
            arg_values[raw_arg_index + 1] = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
            arg_values[raw_arg_index + 2] = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
            arg_values[raw_arg_index + 3] = numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&mutable_dict_required_lengths.back()));
            raw_arg_index += 4;
            continue;
        }
        if (abi == WasmApiParamAbiKind::DictPointer) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::DictByteLength, "Web WASM backend invariant failed");

            array_arg_buffers.emplace_back(SerializeWebWasmExportDictArg(*_engine.get_no_const(), function->ArgTypes[arg_index], call.Accessor, call.ArgsData[arg_index], &ref_handle_scope));
            const vector<uint8_t>& raw_data = array_arg_buffers.back();
            arg_values[raw_arg_index] = raw_data.empty() ? 0 : numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_data.data()));
            arg_values[raw_arg_index + 1] = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
            raw_arg_index += 2;
            continue;
        }
        if (abi == WasmApiParamAbiKind::MutableValuePointer) {
            FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "Web WASM backend invariant failed");
            FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableValueLength, "Web WASM backend invariant failed");

            array_arg_buffers.emplace_back(SerializeWebWasmExportValueArg(*_engine.get_no_const(), function->ArgTypes[arg_index], call.ArgsData[arg_index], &ref_handle_scope));
            const vector<uint8_t>& raw_data = array_arg_buffers.back();
            mutable_value_copybacks.emplace_back(MutableValueCopyback {
                .BufferIndex = array_arg_buffers.size() - 1,
                .Type = function->ArgTypes[arg_index],
                .Data = call.ArgsData[arg_index],
            });
            arg_values[raw_arg_index] = raw_data.empty() ? 0 : numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_data.data()));
            arg_values[raw_arg_index + 1] = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
            raw_arg_index += 2;
            continue;
        }

        FO_STRONG_ASSERT(abi == WasmApiParamAbiKind::Scalar, "Web WASM backend invariant failed");
        arg_values[raw_arg_index] = PackArgValue(function->Args[raw_arg_index], function->ArgTypes[arg_index], *_engine, call.ArgsData[arg_index], &ref_handle_scope);
        raw_arg_index++;
    }

    FO_STRONG_ASSERT(raw_arg_index == function->Args.size(), "Web WASM backend invariant failed");

    uint64_t result_value = 0;
    int32_t result_kind = static_cast<int32_t>(function->Ret);
    int32_t result_buffer = function->Ret != WasmScalarKind::None && (IsWebWasmExportTextType(function->RetType) || IsWebWasmExportValueBufferType(function->RetType) || IsWebWasmExportArrayType(function->RetType) || IsWebWasmExportDictType(function->RetType) || IsWebWasmExportDictOfArrayType(function->RetType)) ? 1 : 0;
    WasmRuntimeContext runtime_context = MakeWasmRuntimeContext(*_engine, _scriptSys.get_no_const());
    array<int64_t, 8> context_values = {
        numeric_cast<int64_t>(runtime_context.Side),
        runtime_context.FrameTimeMs,
        runtime_context.FrameDeltaTimeMs,
        numeric_cast<int64_t>(runtime_context.TimeSynchronized),
        runtime_context.SynchronizedTimeMs,
        numeric_cast<int64_t>(runtime_context.ScriptSys.as_intptr()),
        numeric_cast<int64_t>(reinterpret_cast<uintptr_t>(&WebWasmRetainCallbackImpl)),
        numeric_cast<int64_t>(reinterpret_cast<uintptr_t>(&WebWasmReleaseCallbackImpl)),
    };
    int32_t called = WebWasmCallFunctionImpl(module->Name.c_str(), function->ExportName.c_str(), numeric_cast<int32_t>(function->Args.size()), arg_kinds.data(), arg_abi.data(), arg_values.data(), result_kind, result_buffer, &result_value, context_values.data());

    if (called == 0) {
        throw ScriptCallException("Web WASM function call failed", function->ScriptName, TakeJsString(WebWasmTakeLastErrorImpl()));
    }

    for (const MutableArrayCopyback& copyback : mutable_array_copybacks) {
        if (!copyback.Type.BaseType.IsRefType) {
            continue;
        }

        FO_STRONG_ASSERT(copyback.BufferIndex < array_arg_buffers.size(), "Web WASM backend invariant failed");
        FO_STRONG_ASSERT(copyback.RequiredLengthIndex < mutable_array_required_lengths.size(), "Web WASM backend invariant failed");
        const vector<uint8_t>& raw_data = array_arg_buffers[copyback.BufferIndex];
        const uint32_t required_byte_len = mutable_array_required_lengths[copyback.RequiredLengthIndex];

        if (required_byte_len > raw_data.size()) {
            throw ScriptCallException("Web WASM export mutable ref array requires a larger buffer", copyback.Type.BaseType.Name, required_byte_len, raw_data.size());
        }

        ref_handle_scope.ValidateAndRetainMutableArray(copyback.Type, {raw_data.data(), numeric_cast<size_t>(required_byte_len)});
    }

    for (const MutableDictCopyback& copyback : mutable_dict_copybacks) {
        if (!WasmExportRefHandleScope::ContainsRefHandles(copyback.Type)) {
            continue;
        }

        FO_STRONG_ASSERT(copyback.BufferIndex < array_arg_buffers.size(), "Web WASM backend invariant failed");
        FO_STRONG_ASSERT(copyback.RequiredLengthIndex < mutable_dict_required_lengths.size(), "Web WASM backend invariant failed");
        const vector<uint8_t>& raw_data = array_arg_buffers[copyback.BufferIndex];
        const uint32_t required_byte_len = mutable_dict_required_lengths[copyback.RequiredLengthIndex];

        if (required_byte_len > raw_data.size()) {
            throw ScriptCallException("Web WASM export mutable ref dict requires a larger buffer", copyback.Type.BaseType.Name, required_byte_len, raw_data.size());
        }

        ref_handle_scope.ValidateAndRetainMutableDict(copyback.Type, {raw_data.data(), numeric_cast<size_t>(required_byte_len)});
    }

    for (const MutableValueCopyback& copyback : mutable_value_copybacks) {
        if (!copyback.Type.BaseType.IsRefType) {
            continue;
        }

        FO_STRONG_ASSERT(copyback.BufferIndex < array_arg_buffers.size(), "Web WASM backend invariant failed");
        const vector<uint8_t>& raw_data = array_arg_buffers[copyback.BufferIndex];
        ref_handle_scope.ValidateAndRetainMutableValue(copyback.Type, raw_data);
    }

    for (const MutableTextCopyback& copyback : mutable_text_copybacks) {
        FO_STRONG_ASSERT(copyback.BufferIndex < array_arg_buffers.size(), "Web WASM backend invariant failed");
        FO_STRONG_ASSERT(copyback.RequiredLengthIndex < mutable_text_required_lengths.size(), "Web WASM backend invariant failed");
        const vector<uint8_t>& raw_data = array_arg_buffers[copyback.BufferIndex];
        const uint32_t required_byte_len = mutable_text_required_lengths[copyback.RequiredLengthIndex];

        if (required_byte_len > raw_data.size()) {
            throw ScriptCallException("Web WASM export mutable text requires a larger buffer", copyback.Type.BaseType.Name, required_byte_len, raw_data.size());
        }

        const char* text_data = required_byte_len == 0 ? "" : reinterpret_cast<const char*>(raw_data.data());
        StoreWebWasmExportTextResult(copyback.Type.BaseType, {text_data, numeric_cast<size_t>(required_byte_len)}, copyback.Data);
    }

    for (const MutableArrayCopyback& copyback : mutable_array_copybacks) {
        FO_STRONG_ASSERT(copyback.BufferIndex < array_arg_buffers.size(), "Web WASM backend invariant failed");
        FO_STRONG_ASSERT(copyback.RequiredLengthIndex < mutable_array_required_lengths.size(), "Web WASM backend invariant failed");
        const vector<uint8_t>& raw_data = array_arg_buffers[copyback.BufferIndex];
        const uint32_t required_byte_len = mutable_array_required_lengths[copyback.RequiredLengthIndex];

        if (required_byte_len > raw_data.size()) {
            throw ScriptCallException("Web WASM export mutable array requires a larger buffer", copyback.Type.BaseType.Name, required_byte_len, raw_data.size());
        }

        StoreWebWasmExportArrayResult(*_engine.get_no_const(), copyback.Type, {raw_data.data(), numeric_cast<size_t>(required_byte_len)}, call.Accessor, copyback.Data);
    }

    for (const MutableDictCopyback& copyback : mutable_dict_copybacks) {
        FO_STRONG_ASSERT(copyback.BufferIndex < array_arg_buffers.size(), "Web WASM backend invariant failed");
        FO_STRONG_ASSERT(copyback.RequiredLengthIndex < mutable_dict_required_lengths.size(), "Web WASM backend invariant failed");
        const vector<uint8_t>& raw_data = array_arg_buffers[copyback.BufferIndex];
        const uint32_t required_byte_len = mutable_dict_required_lengths[copyback.RequiredLengthIndex];

        if (required_byte_len > raw_data.size()) {
            throw ScriptCallException("Web WASM export mutable dict requires a larger buffer", copyback.Type.BaseType.Name, required_byte_len, raw_data.size());
        }

        StoreWebWasmExportDictResult(*_engine.get_no_const(), copyback.Type, {raw_data.data(), numeric_cast<size_t>(required_byte_len)}, call.Accessor, copyback.Data);
    }

    for (const MutableValueCopyback& copyback : mutable_value_copybacks) {
        FO_STRONG_ASSERT(copyback.BufferIndex < array_arg_buffers.size(), "Web WASM backend invariant failed");
        const vector<uint8_t>& raw_data = array_arg_buffers[copyback.BufferIndex];
        StoreWebWasmExportValueResult(*_engine.get_no_const(), copyback.Type, raw_data, copyback.Data);
    }

    ref_handle_scope.ReleaseRetained();

    if (function->Ret != WasmScalarKind::None) {
        FO_STRONG_ASSERT(call.RetData != nullptr, "Web WASM backend invariant failed");

        if (IsWebWasmExportTextType(function->RetType)) {
            ReadWebWasmExportTextResult(function->RetType.BaseType, result_value, call.RetData);
        }
        else if (IsWebWasmExportValueBufferType(function->RetType)) {
            ReadWebWasmExportValueResult(*_engine.get_no_const(), function->RetType, result_value, call.RetData);
        }
        else if (IsWebWasmExportArrayType(function->RetType)) {
            ReadWebWasmExportArrayResult(*_engine.get_no_const(), function->RetType, result_value, call.Accessor, call.RetData);
        }
        else if (IsWebWasmExportDictType(function->RetType) || IsWebWasmExportDictOfArrayType(function->RetType)) {
            ReadWebWasmExportDictResult(*_engine.get_no_const(), function->RetType, result_value, call.Accessor, call.RetData);
        }
        else {
            UnpackReturnValue(function->Ret, function->RetType, *_engine.get_no_const(), result_value, call.RetData);
        }
    }
}

void WebWasmBackend::CallApiImport(size_t method_index, const_span<uint64_t> raw_args, uint64_t* raw_result)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_engine, "Web WASM backend invariant failed");

    if (method_index >= _apiImports.Methods.size()) {
        throw ScriptCallException("Invalid Web WASM API import index", method_index);
    }

    const WasmApiMethodDesc& method = _apiImports.Methods[method_index];

    if (!method.Supported) {
        throw ScriptCallException("Unsupported Web WASM API import ABI", method.Name, method.UnsupportedReason);
    }
    if (raw_args.size() != method.Args.size()) {
        throw ScriptCallException("Web WASM API import argument count mismatch", method.Name, raw_args.size(), method.Args.size());
    }

    CallWasmApiMethod(*_engine, method, raw_args, raw_result);
}

void WebWasmBackend::CallPropertyImport(size_t property_index, const_span<uint64_t> raw_args, uint64_t* raw_result)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_engine, "Web WASM backend invariant failed");

    if (property_index >= _apiImports.Properties.size()) {
        throw ScriptCallException("Invalid Web WASM API property import index", property_index);
    }

    const WasmApiPropertyDesc& property = _apiImports.Properties[property_index];

    if (!property.Supported) {
        throw ScriptCallException("Unsupported Web WASM API property import ABI", property.Name, property.UnsupportedReason);
    }
    if (raw_args.size() != property.ArgsCount) {
        throw ScriptCallException("Web WASM API property import argument count mismatch", property.Name, raw_args.size(), property.ArgsCount);
    }

    CallWasmApiProperty(*_engine, property, raw_args, raw_result);
}

auto WebWasmBackend::CallApiImportCallback(uintptr_t backend_ptr, int32_t method_index, int32_t argc, const uint64_t* raw_args, uint64_t* raw_result) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    try {
        nptr<WebWasmBackend> backend = reinterpret_cast<WebWasmBackend*>(backend_ptr);

        FO_STRONG_ASSERT(backend, "Web WASM backend invariant failed");
        FO_STRONG_ASSERT(argc >= 0, "Web WASM backend invariant failed");

        backend->CallApiImport(numeric_cast<size_t>(method_index), const_span<uint64_t> {raw_args, numeric_cast<size_t>(argc)}, raw_result);
        return 1;
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        return 0;
    }
}

auto WebWasmBackend::CallPropertyImportCallback(uintptr_t backend_ptr, int32_t property_index, int32_t argc, const uint64_t* raw_args, uint64_t* raw_result) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    try {
        nptr<WebWasmBackend> backend = reinterpret_cast<WebWasmBackend*>(backend_ptr);

        FO_STRONG_ASSERT(backend, "Web WASM backend invariant failed");
        FO_STRONG_ASSERT(argc >= 0, "Web WASM backend invariant failed");

        backend->CallPropertyImport(numeric_cast<size_t>(property_index), const_span<uint64_t> {raw_args, numeric_cast<size_t>(argc)}, raw_result);
        return 1;
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        return 0;
    }
}

FO_END_NAMESPACE

#endif
