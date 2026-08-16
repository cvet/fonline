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

#include "WasmBackend.h"

#if FO_WASM_SCRIPTING && !FO_WEB

#include "Application.h"
#include "EngineBase.h"
#include "EntityProtos.h"
#include "FileSystem.h"
#include "Settings.h"
#include "WasmImports.h"
#include "WasmRefHandles.h"

#include <cstdlib>

FO_BEGIN_NAMESPACE

static constexpr uint32_t WASM_ERROR_BUF_SIZE = 512;
static constexpr WasmScalarKind WASM_EXPORT_REF_TYPE_HANDLE_KIND = WasmScalarKind::I64;
static constexpr size_t WASM_EXPORT_OPAQUE_VALUE_STORAGE_SIZE = 16;

struct alignas(std::max_align_t) WasmExportOpaqueValueStorage
{
    array<uint8_t, WASM_EXPORT_OPAQUE_VALUE_STORAGE_SIZE> Data {};
};

struct WasmRuntimeData
{
    std::mutex Locker {};
    int32_t RefCount {};
};
FO_GLOBAL_DATA(WasmRuntimeData, WasmRuntime);

struct WasmBackend::WasmFunction
{
    string ExportName {};
    string ScriptName {};
    wasm_function_inst_t Function {};
    vector<wasm_valkind_t> Args {};
    vector<WasmApiParamAbiKind> ArgAbi {};
    vector<ComplexTypeDesc> ArgTypes {};
    optional<wasm_valkind_t> Ret {};
    ComplexTypeDesc RetType {};
    unique_nptr<ScriptFuncDesc> FuncDesc {};
};

struct WasmBackend::WasmModule
{
    string Name {};
    string Path {};
    vector<uint8_t> Binary {};
    wasm_module_t Module {};
    wasm_module_inst_t ModuleInst {};
    wasm_exec_env_t ExecEnv {};
    vector<unique_ptr<WasmFunction>> Functions {};
};

struct WasmApiNativeMethod
{
    nptr<BaseEngine> Engine {};
    nptr<const WasmApiMethodDesc> Method {};
    nptr<const WasmApiPropertyDesc> Property {};
};

class WasmModuleContextScope final
{
public:
    WasmModuleContextScope(wasm_module_inst_t module_inst, const WasmRuntimeContext& context) :
        _moduleInst {module_inst},
        _previousContext {wasm_runtime_get_custom_data(module_inst)}
    {
        FO_STACK_TRACE_ENTRY();

        wasm_runtime_set_custom_data(_moduleInst, const_cast<WasmRuntimeContext*>(&context));
    }
    WasmModuleContextScope(const WasmModuleContextScope&) = delete;
    WasmModuleContextScope(WasmModuleContextScope&&) noexcept = delete;
    auto operator=(const WasmModuleContextScope&) = delete;
    auto operator=(WasmModuleContextScope&&) noexcept = delete;
    ~WasmModuleContextScope()
    {
        FO_STACK_TRACE_ENTRY();

        wasm_runtime_set_custom_data(_moduleInst, _previousContext.get());
    }

private:
    wasm_module_inst_t _moduleInst {};
    nptr<void> _previousContext {};
};

static auto WasmAlloc(size_t size) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::malloc(size);
}

static auto WasmRealloc(void* ptr, size_t size) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::realloc(ptr, size);
}

static void WasmFree(void* ptr)
{
    FO_NO_STACK_TRACE_ENTRY();

    std::free(ptr);
}

static auto GetWasmNativeContext(wasm_exec_env_t exec_env) noexcept -> const WasmRuntimeContext&
{
    FO_NO_STACK_TRACE_ENTRY();

    if (exec_env == nullptr) {
        return GetDefaultWasmRuntimeContext();
    }

    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);

    if (module_inst == nullptr) {
        return GetDefaultWasmRuntimeContext();
    }

    nptr<const WasmRuntimeContext> context = cast_from_void<const WasmRuntimeContext*>(wasm_runtime_get_custom_data(module_inst));
    return context ? *context : GetDefaultWasmRuntimeContext();
}

static void WasmNativeLogI32(wasm_exec_env_t exec_env, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(exec_env);
    WasmLogI32(value);
}

static void WasmNativeLogI64(wasm_exec_env_t exec_env, int64_t value)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(exec_env);
    WasmLogI64(value);
}

static void WasmNativeLogF32(wasm_exec_env_t exec_env, float32_t value)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(exec_env);
    WasmLogF32(value);
}

static void WasmNativeLogF64(wasm_exec_env_t exec_env, float64_t value)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(exec_env);
    WasmLogF64(value);
}

static void WasmNativeLogUtf8(wasm_exec_env_t exec_env, const char* text, int32_t text_len)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(exec_env);
    WasmLogUtf8(text, text_len);
}

static auto WasmNativeRetainCallback(wasm_exec_env_t exec_env, const char* token, int32_t token_len) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return WasmRetainCallback(GetWasmNativeContext(exec_env), token, token_len);
}

static auto WasmNativeReleaseCallback(wasm_exec_env_t exec_env, const char* token, int32_t token_len) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return WasmReleaseCallback(GetWasmNativeContext(exec_env), token, token_len);
}

static auto WasmNativeGetSide(wasm_exec_env_t exec_env) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return WasmGetSide(GetWasmNativeContext(exec_env));
}

static auto WasmNativeGetFrameTimeMs(wasm_exec_env_t exec_env) -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return WasmGetFrameTimeMs(GetWasmNativeContext(exec_env));
}

static auto WasmNativeGetFrameDeltaTimeMs(wasm_exec_env_t exec_env) -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return WasmGetFrameDeltaTimeMs(GetWasmNativeContext(exec_env));
}

static auto WasmNativeIsTimeSynchronized(wasm_exec_env_t exec_env) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return WasmIsTimeSynchronized(GetWasmNativeContext(exec_env));
}

static auto WasmNativeGetSynchronizedTimeMs(wasm_exec_env_t exec_env) -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return WasmGetSynchronizedTimeMs(GetWasmNativeContext(exec_env));
}

static void AcquireWasmRuntime()
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock lock(WasmRuntime->Locker);

    if (WasmRuntime->RefCount == 0) {
        RuntimeInitArgs init_args {};
        init_args.mem_alloc_type = Alloc_With_Allocator;
        init_args.mem_alloc_option.allocator.malloc_func = reinterpret_cast<void*>(&WasmAlloc);
        init_args.mem_alloc_option.allocator.realloc_func = reinterpret_cast<void*>(&WasmRealloc);
        init_args.mem_alloc_option.allocator.free_func = reinterpret_cast<void*>(&WasmFree);
        init_args.running_mode = Mode_Interp;

        const WasmImportDesc* log_i32 = FindWasmImportDesc("fonline", "log_i32");
        const WasmImportDesc* log_i64 = FindWasmImportDesc("fonline", "log_i64");
        const WasmImportDesc* log_f32 = FindWasmImportDesc("fonline", "log_f32");
        const WasmImportDesc* log_f64 = FindWasmImportDesc("fonline", "log_f64");
        const WasmImportDesc* log_utf8 = FindWasmImportDesc("fonline", "log_utf8");
        const WasmImportDesc* callback_retain = FindWasmImportDesc("fonline", "callback_retain");
        const WasmImportDesc* callback_release = FindWasmImportDesc("fonline", "callback_release");
        const WasmImportDesc* get_side = FindWasmImportDesc("fonline", "get_side");
        const WasmImportDesc* get_frame_time_ms = FindWasmImportDesc("fonline", "get_frame_time_ms");
        const WasmImportDesc* get_frame_delta_time_ms = FindWasmImportDesc("fonline", "get_frame_delta_time_ms");
        const WasmImportDesc* is_time_synchronized = FindWasmImportDesc("fonline", "is_time_synchronized");
        const WasmImportDesc* get_synchronized_time_ms = FindWasmImportDesc("fonline", "get_synchronized_time_ms");

        FO_STRONG_ASSERT(log_i32, "WASM backend invariant failed");
        FO_STRONG_ASSERT(log_i64, "WASM backend invariant failed");
        FO_STRONG_ASSERT(log_f32, "WASM backend invariant failed");
        FO_STRONG_ASSERT(log_f64, "WASM backend invariant failed");
        FO_STRONG_ASSERT(log_utf8, "WASM backend invariant failed");
        FO_STRONG_ASSERT(callback_retain, "WASM backend invariant failed");
        FO_STRONG_ASSERT(callback_release, "WASM backend invariant failed");
        FO_STRONG_ASSERT(get_side, "WASM backend invariant failed");
        FO_STRONG_ASSERT(get_frame_time_ms, "WASM backend invariant failed");
        FO_STRONG_ASSERT(get_frame_delta_time_ms, "WASM backend invariant failed");
        FO_STRONG_ASSERT(is_time_synchronized, "WASM backend invariant failed");
        FO_STRONG_ASSERT(get_synchronized_time_ms, "WASM backend invariant failed");

        static NativeSymbol native_symbols[] = {
            {log_i32->Name.data(), reinterpret_cast<void*>(&WasmNativeLogI32), log_i32->NativeSignature.data(), nullptr},
            {log_i64->Name.data(), reinterpret_cast<void*>(&WasmNativeLogI64), log_i64->NativeSignature.data(), nullptr},
            {log_f32->Name.data(), reinterpret_cast<void*>(&WasmNativeLogF32), log_f32->NativeSignature.data(), nullptr},
            {log_f64->Name.data(), reinterpret_cast<void*>(&WasmNativeLogF64), log_f64->NativeSignature.data(), nullptr},
            {log_utf8->Name.data(), reinterpret_cast<void*>(&WasmNativeLogUtf8), log_utf8->NativeSignature.data(), nullptr},
            {callback_retain->Name.data(), reinterpret_cast<void*>(&WasmNativeRetainCallback), callback_retain->NativeSignature.data(), nullptr},
            {callback_release->Name.data(), reinterpret_cast<void*>(&WasmNativeReleaseCallback), callback_release->NativeSignature.data(), nullptr},
            {get_side->Name.data(), reinterpret_cast<void*>(&WasmNativeGetSide), get_side->NativeSignature.data(), nullptr},
            {get_frame_time_ms->Name.data(), reinterpret_cast<void*>(&WasmNativeGetFrameTimeMs), get_frame_time_ms->NativeSignature.data(), nullptr},
            {get_frame_delta_time_ms->Name.data(), reinterpret_cast<void*>(&WasmNativeGetFrameDeltaTimeMs), get_frame_delta_time_ms->NativeSignature.data(), nullptr},
            {is_time_synchronized->Name.data(), reinterpret_cast<void*>(&WasmNativeIsTimeSynchronized), is_time_synchronized->NativeSignature.data(), nullptr},
            {get_synchronized_time_ms->Name.data(), reinterpret_cast<void*>(&WasmNativeGetSynchronizedTimeMs), get_synchronized_time_ms->NativeSignature.data(), nullptr},
        };

        init_args.native_module_name = "fonline";
        init_args.native_symbols = native_symbols;
        init_args.n_native_symbols = numeric_cast<uint32_t>(std::size(native_symbols));

        if (!wasm_runtime_full_init(&init_args)) {
            throw ScriptSystemException("WAMR runtime initialization failed");
        }
    }

    WasmRuntime->RefCount++;
}

static void ReleaseWasmRuntime()
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock lock(WasmRuntime->Locker);

    FO_STRONG_ASSERT(WasmRuntime->RefCount > 0, "WASM backend invariant failed");

    WasmRuntime->RefCount--;

    if (WasmRuntime->RefCount == 0) {
        wasm_runtime_destroy();
    }
}

static auto ShouldLoadForSide(string_view path, EngineSideKind side) -> bool
{
    FO_STACK_TRACE_ENTRY();

    string lower_path = strex(path).lower();
    bool server_only = lower_path.ends_with(".server.wasm");
    bool client_only = lower_path.ends_with(".client.wasm");
    bool mapper_only = lower_path.ends_with(".mapper.wasm");

    if (!server_only && !client_only && !mapper_only) {
        return true;
    }

    switch (side) {
    case EngineSideKind::ServerSide:
        return server_only;
    case EngineSideKind::ClientSide:
        return client_only;
    case EngineSideKind::MapperSide:
        return mapper_only;
    default:
        return false;
    }
}

static auto MakeModuleName(const File& file) -> string
{
    FO_STACK_TRACE_ENTRY();

    string module_name {file.GetNameNoExt()};
    string lower_name = strex(module_name).lower();

    if (lower_name.ends_with(".server") || lower_name.ends_with(".client") || lower_name.ends_with(".mapper")) {
        module_name.resize(module_name.rfind('.'));
    }

    return module_name;
}

static auto MakeWasmError(span<const char> error_buf) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    size_t error_len = 0;

    while (error_len < error_buf.size() && error_buf[error_len] != '\0') {
        error_len++;
    }

    return string_view {error_buf.data(), error_len};
}

static auto ResolveWasmScalarKindFromWamr(wasm_valkind_t kind) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (kind) {
    case WASM_I32:
        return WasmScalarKind::I32;
    case WASM_I64:
        return WasmScalarKind::I64;
    case WASM_F32:
        return WasmScalarKind::F32;
    case WASM_F64:
        return WasmScalarKind::F64;
    default:
        return WasmScalarKind::None;
    }
}

static auto WasmScalarKindToWamrValueKind(WasmScalarKind kind) -> wasm_valkind_t
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (kind) {
    case WasmScalarKind::I32:
        return WASM_I32;
    case WasmScalarKind::I64:
        return WASM_I64;
    case WasmScalarKind::F32:
        return WASM_F32;
    case WasmScalarKind::F64:
        return WASM_F64;
    default:
        throw ScriptCallException("Unsupported WASM scalar kind", static_cast<int32_t>(kind));
    }
}

static auto ResolveWasmType(EngineMetadata* meta, wasm_valkind_t kind) -> optional<ComplexTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    string_view type_name {};

    switch (kind) {
    case WASM_I32:
        type_name = "int32";
        break;
    case WASM_I64:
        type_name = "int64";
        break;
    case WASM_F32:
        type_name = "float32";
        break;
    case WASM_F64:
        type_name = "float64";
        break;
    default:
        return std::nullopt;
    }

    return ComplexTypeDesc {.Kind = ComplexTypeKind::Simple, .BaseType = meta->GetBaseType(type_name)};
}

static void SetWasmNativeException(wasm_exec_env_t exec_env, const std::exception& ex) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (exec_env != nullptr) {
        if (wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env); module_inst != nullptr) {
            wasm_runtime_set_exception(module_inst, ex.what());
        }
    }
}

static void WasmNativeApiCall(wasm_exec_env_t exec_env, uint64_t* raw_args)
{
    FO_STACK_TRACE_ENTRY();

    nptr<WasmApiNativeMethod> native_method = cast_from_void<WasmApiNativeMethod*>(wasm_runtime_get_function_attachment(exec_env));

    try {
        FO_STRONG_ASSERT(native_method, "WASM backend invariant failed");
        FO_STRONG_ASSERT(native_method->Engine != nullptr, "WASM backend invariant failed");
        FO_STRONG_ASSERT(raw_args != nullptr, "WASM backend invariant failed");

        if (native_method->Method != nullptr) {
            const WasmApiMethodDesc& desc = *native_method->Method;
            CallWasmApiMethod(*native_method->Engine, desc, const_span<uint64_t> {raw_args, desc.Args.size()}, raw_args);
        }
        else {
            FO_STRONG_ASSERT(native_method->Property != nullptr, "WASM backend invariant failed");

            const WasmApiPropertyDesc& desc = *native_method->Property;
            CallWasmApiProperty(*native_method->Engine, desc, const_span<uint64_t> {raw_args, desc.ArgsCount}, raw_args);
        }
    }
    catch (const std::exception& ex) {
        SetWasmNativeException(exec_env, ex);
    }
}

static auto MakeWasmImportFullName(const wasm_import_t& import_type) -> string
{
    FO_STACK_TRACE_ENTRY();

    string_view module_name = import_type.module_name != nullptr ? string_view {import_type.module_name} : string_view {};
    string_view import_name = import_type.name != nullptr ? string_view {import_type.name} : string_view {};

    return strex("{}.{}", module_name, import_name);
}

static auto ReadWasmImportArgs(const wasm_import_t& import_type) -> optional<vector<WasmScalarKind>>
{
    FO_STACK_TRACE_ENTRY();

    if (import_type.u.func_type == nullptr) {
        return std::nullopt;
    }

    uint32_t param_count = wasm_func_type_get_param_count(import_type.u.func_type);

    if (param_count > MAX_CALL_ARGS) {
        return std::nullopt;
    }

    vector<WasmScalarKind> args;
    args.reserve(param_count);

    for (uint32_t param_index = 0; param_index < param_count; param_index++) {
        WasmScalarKind kind = ResolveWasmScalarKindFromWamr(wasm_func_type_get_param_valkind(import_type.u.func_type, param_index));

        if (kind == WasmScalarKind::None) {
            return std::nullopt;
        }

        args.emplace_back(kind);
    }

    return args;
}

static auto ReadWasmImportResults(const wasm_import_t& import_type) -> optional<vector<WasmScalarKind>>
{
    FO_STACK_TRACE_ENTRY();

    if (import_type.u.func_type == nullptr) {
        return std::nullopt;
    }

    uint32_t result_count = wasm_func_type_get_result_count(import_type.u.func_type);

    if (result_count > 1) {
        return std::nullopt;
    }

    vector<WasmScalarKind> results;
    results.reserve(result_count);

    for (uint32_t result_index = 0; result_index < result_count; result_index++) {
        WasmScalarKind kind = ResolveWasmScalarKindFromWamr(wasm_func_type_get_result_valkind(import_type.u.func_type, result_index));

        if (kind == WasmScalarKind::None) {
            return std::nullopt;
        }

        results.emplace_back(kind);
    }

    return results;
}

static void ValidateWasmModuleImports(wasm_module_t module, string_view module_path, const vector<WasmApiMethodDesc>& api_methods, const vector<WasmApiPropertyDesc>& api_properties)
{
    FO_STACK_TRACE_ENTRY();

    int32_t import_count = wasm_runtime_get_import_count(module);

    if (import_count < 0) {
        throw ScriptSystemException("WASM import enumeration failed", module_path);
    }

    for (int32_t import_index = 0; import_index < import_count; import_index++) {
        wasm_import_t import_type {};
        wasm_runtime_get_import_type(module, import_index, &import_type);

        if (import_type.kind != WASM_IMPORT_EXPORT_KIND_FUNC) {
            throw ScriptSystemException("Unsupported WASM import kind", module_path, MakeWasmImportFullName(import_type));
        }
        if (import_type.module_name == nullptr || import_type.name == nullptr) {
            throw ScriptSystemException("Invalid WASM import", module_path);
        }

        string import_full_name = MakeWasmImportFullName(import_type);
        auto args = ReadWasmImportArgs(import_type);
        auto results = ReadWasmImportResults(import_type);

        if (!args.has_value() || !results.has_value()) {
            throw ScriptSystemException("Unsupported WASM import signature", module_path, import_full_name);
        }

        const WasmImportDesc* host_desc = FindWasmImportDesc(import_type.module_name, import_type.name);

        if (host_desc != nullptr) {
            if (!ValidateWasmImportSignature(*host_desc, args.value(), results.value())) {
                throw ScriptSystemException("WASM import signature mismatch", module_path, import_full_name);
            }
            continue;
        }

        nptr<const WasmApiMethodDesc> api_desc = FindWasmApiMethodDesc(api_methods, import_type.module_name, import_type.name);

        if (api_desc) {
            if (!api_desc->Supported) {
                throw ScriptSystemException("Unsupported WASM engine API import ABI", module_path, import_full_name, api_desc->UnsupportedReason);
            }
            if (!ValidateWasmApiMethodSignature(*api_desc, args.value(), results.value())) {
                throw ScriptSystemException("WASM engine API import signature mismatch", module_path, import_full_name);
            }
            continue;
        }

        nptr<const WasmApiPropertyDesc> api_property_desc = FindWasmApiPropertyDesc(api_properties, import_type.module_name, import_type.name);

        if (api_property_desc) {
            if (!api_property_desc->Supported) {
                throw ScriptSystemException("Unsupported WASM engine API import ABI", module_path, import_full_name, api_property_desc->UnsupportedReason);
            }
            if (!ValidateWasmApiPropertySignature(*api_property_desc, args.value(), results.value())) {
                throw ScriptSystemException("WASM engine API import signature mismatch", module_path, import_full_name);
            }
            continue;
        }

        {
            throw ScriptSystemException("Unsupported WASM import", module_path, import_full_name);
        }
    }
}

static auto MakeArgDesc(EngineMetadata* meta, wasm_valkind_t kind, size_t arg_index) -> optional<ArgDesc>
{
    FO_STACK_TRACE_ENTRY();

    auto type = ResolveWasmType(meta, kind);

    if (!type.has_value()) {
        return std::nullopt;
    }

    return ArgDesc {.Name = strex("arg{}", arg_index), .Type = std::move(type.value())};
}

static auto TryResolveWasmExportScalarKind(const BaseTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsGlobalEntity || type.IsString) {
        return WasmScalarKind::None;
    }
    if (type.IsRefType) {
        return WASM_EXPORT_REF_TYPE_HANDLE_KIND;
    }
    if (type.IsEntity || type.IsEntityProto || type.IsFixedType) {
        return WasmScalarKind::I64;
    }
    if (type.IsEnum) {
        return type.EnumUnderlyingType != nullptr ? TryResolveWasmExportScalarKind(*type.EnumUnderlyingType) : WasmScalarKind::None;
    }
    if (type.IsSimpleStruct) {
        if (type.StructLayout == nullptr || type.StructLayout->Fields.size() != 1) {
            return WasmScalarKind::None;
        }

        return TryResolveWasmExportScalarKind(type.StructLayout->Fields.front().Type);
    }
    if (type.IsComplexStruct) {
        if (type.StructLayout == nullptr || type.Size == 0 || type.Size > sizeof(uint64_t)) {
            return WasmScalarKind::None;
        }

        for (const FieldDesc& field : type.StructLayout->Fields) {
            WasmScalarKind field_kind = TryResolveWasmExportScalarKind(field.Type);

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

static auto TryResolveWasmExportScalarKind(const ComplexTypeDesc& type) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Kind != ComplexTypeKind::Simple || type.IsMutable) {
        return WasmScalarKind::None;
    }

    return TryResolveWasmExportScalarKind(type.BaseType);
}

static auto IsWasmExportBufferStructType(const BaseTypeDesc& type) noexcept -> bool;

static auto IsWasmExportBufferStructFieldType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsBool || type.IsInt || type.IsFloat || type.IsEnum || type.IsHashedString) {
        return true;
    }
    if (type.IsSimpleStruct || type.IsComplexStruct) {
        return IsWasmExportBufferStructType(type);
    }

    return false;
}

static auto IsWasmExportBufferStructType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if ((!type.IsSimpleStruct && !type.IsComplexStruct) || type.StructLayout == nullptr || type.Size == 0 || type.Size > WASM_EXPORT_OPAQUE_VALUE_STORAGE_SIZE) {
        return false;
    }

    for (const FieldDesc& field : type.StructLayout->Fields) {
        if (!IsWasmExportBufferStructFieldType(field.Type)) {
            return false;
        }
    }

    return true;
}

static auto IsWasmExportBufferElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmExportBufferStructType(type);
}

static auto IsWasmExportResolvedBufferElementType(const BaseTypeDesc& type, WasmScalarKind kind) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return kind != WasmScalarKind::None || IsWasmExportBufferElementType(type);
}

static auto IsWasmExportTextType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && !type.IsMutable && type.BaseType.IsString && (type.BaseType.Name == "string" || type.BaseType.Name == "any");
}

static auto IsWasmExportMutableTextType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && type.IsMutable && type.BaseType.IsString && (type.BaseType.Name == "string" || type.BaseType.Name == "any");
}

static auto IsWasmExportValueBufferType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && !type.IsMutable && !type.BaseType.IsString && !type.BaseType.IsRefType && TryResolveWasmExportScalarKind(type.BaseType) == WasmScalarKind::None && IsWasmExportBufferElementType(type.BaseType);
}

static auto IsWasmExportMutableValueType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Kind != ComplexTypeKind::Simple || !type.IsMutable || type.BaseType.IsString) {
        return false;
    }
    if (type.BaseType.IsRefType) {
        return WasmExportRefHandleScope::HasLifecycle(type.BaseType);
    }

    return IsWasmExportResolvedBufferElementType(type.BaseType, TryResolveWasmExportScalarKind(type.BaseType));
}

static auto IsWasmExportCallbackType(const ComplexTypeDesc& type) noexcept -> bool
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
        if (callback_args[i].Kind == ComplexTypeKind::Callback && !IsWasmExportCallbackType(callback_args[i])) {
            return false;
        }
    }

    return true;
}

static auto IsWasmExportTextArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Array && !type.IsMutable && type.BaseType.IsString && (type.BaseType.Name == "string" || type.BaseType.Name == "any");
}

static auto IsWasmExportMutableTextArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Array && type.IsMutable && type.BaseType.IsString && (type.BaseType.Name == "string" || type.BaseType.Name == "any");
}

static auto IsWasmExportArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Array && !type.IsMutable && (IsWasmExportTextArrayType(type) || IsWasmExportResolvedBufferElementType(type.BaseType, TryResolveWasmExportScalarKind(type.BaseType)));
}

static auto IsWasmExportMutableArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Kind != ComplexTypeKind::Array || !type.IsMutable) {
        return false;
    }
    if (type.BaseType.IsRefType) {
        return WasmExportRefHandleScope::HasLifecycle(type.BaseType);
    }

    return IsWasmExportMutableTextArrayType(type) || IsWasmExportResolvedBufferElementType(type.BaseType, TryResolveWasmExportScalarKind(type.BaseType));
}

static auto IsWasmExportDictElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return (type.IsString && (type.Name == "string" || type.Name == "any")) || IsWasmExportResolvedBufferElementType(type, TryResolveWasmExportScalarKind(type));
}

static auto IsWasmExportDictArrayElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return (type.IsString && (type.Name == "string" || type.Name == "any")) || IsWasmExportResolvedBufferElementType(type, TryResolveWasmExportScalarKind(type));
}

static auto IsWasmExportMutableDictElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmExportDictElementType(type) && (!type.IsRefType || WasmExportRefHandleScope::HasLifecycle(type));
}

static auto IsWasmExportMutableDictArrayElementType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsWasmExportDictArrayElementType(type) && (!type.IsRefType || WasmExportRefHandleScope::HasLifecycle(type));
}

static auto IsWasmExportDictType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Dict && !type.IsMutable && type.KeyType.has_value() && IsWasmExportDictElementType(*type.KeyType) && IsWasmExportDictElementType(type.BaseType);
}

static auto IsWasmExportDictOfArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::DictOfArray && !type.IsMutable && type.KeyType.has_value() && IsWasmExportDictElementType(*type.KeyType) && IsWasmExportDictArrayElementType(type.BaseType);
}

static auto IsWasmExportMutableDictType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Dict && type.IsMutable && type.KeyType.has_value() && IsWasmExportMutableDictElementType(*type.KeyType) && IsWasmExportMutableDictElementType(type.BaseType);
}

static auto IsWasmExportMutableDictOfArrayType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::DictOfArray && type.IsMutable && type.KeyType.has_value() && IsWasmExportMutableDictElementType(*type.KeyType) && IsWasmExportMutableDictArrayElementType(type.BaseType);
}

static auto IsWasmExportEntityHandleType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsEntity && !type.IsGlobalEntity && !type.IsEntityProto;
}

static auto IsWasmExportProtoOrFixedHandleType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsEntityProto || type.IsFixedType;
}

static auto IsWasmExportEntityTypeCompatible(string_view expected_type_name, string_view actual_type_name) noexcept -> bool
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

static auto MakeWasmExportIdent(uint64_t raw_id) noexcept -> ident_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return ident_t {std::bit_cast<ident_t::underlying_type>(raw_id)};
}

static auto PackWasmExportIdent(ident_t id) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::bit_cast<uint64_t>(id.underlying_value());
}

static auto ResolveWasmExportEntityHandle(BaseEngine& engine, const BaseTypeDesc& type, uint64_t raw_id, string_view role) -> nptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    if (raw_id == 0) {
        return nullptr;
    }

    ident_t entity_id = MakeWasmExportIdent(raw_id);
    nptr<Entity> entity = engine.ResolveScriptEntityHandle(type.Name, entity_id);

    if (!entity) {
        throw ScriptCallException(strex("WASM export {} entity not found", role), type.Name, entity_id.underlying_value());
    }
    if (entity->IsDestroyed()) {
        throw ScriptCallException(strex("WASM export {} entity is destroyed", role), type.Name, entity_id.underlying_value());
    }

    ptr<const PropertyRegistrar> actual_registrator = entity->GetProperties()->GetRegistrar();

    if (!IsWasmExportEntityTypeCompatible(type.Name, actual_registrator->GetTypeName().as_str())) {
        throw ScriptCallException(strex("WASM export {} entity type mismatch", role), type.Name, actual_registrator->GetTypeName());
    }

    return entity;
}

static auto ResolveWasmExportProtoHandle(BaseEngine& engine, const BaseTypeDesc& type, uint64_t raw_hash, string_view role) -> nptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    if (raw_hash == 0) {
        return nullptr;
    }

    hstring proto_id = engine.Hashes.ResolveHash(raw_hash);
    nptr<const ProtoEntity> proto = engine.GetProtoEntity(type.HashedName, proto_id);

    if (!proto) {
        throw ScriptCallException(strex("WASM export {} proto not found", role), type.Name, proto_id);
    }

    ptr<const PropertyRegistrar> actual_registrator = proto->GetProperties()->GetRegistrar();

    if (!IsWasmExportEntityTypeCompatible(type.Name, actual_registrator->GetTypeName().as_str())) {
        throw ScriptCallException(strex("WASM export {} proto type mismatch", role), type.Name, actual_registrator->GetTypeName());
    }

    return make_ptr(const_cast<ProtoEntity*>(std::addressof(*proto)));
}

static auto ResolveWasmExportDictTypeName(EngineMetadata* meta, string_view type_name) -> optional<ComplexTypeDesc>
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

static auto ResolveWasmExportDictOfArrayTypeName(EngineMetadata* meta, string_view type_name) -> optional<ComplexTypeDesc>
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

static auto ResolveWasmExportMutableTypeName(EngineMetadata* meta, string_view type_name) -> optional<ComplexTypeDesc>
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

static auto ResolveWasmExportTypeName(EngineMetadata* meta, string_view type_name, bool allow_void) -> optional<ComplexTypeDesc>;

static auto SplitWasmExportCallbackTypeList(EngineMetadata* meta, string_view type_names) -> optional<vector<ComplexTypeDesc>>
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
            optional<ComplexTypeDesc> type = is_return_type && type_name == "void" ? optional<ComplexTypeDesc> {ComplexTypeDesc {}} : ResolveWasmExportTypeName(meta, type_name, false);

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

static auto ResolveWasmExportCallbackTypeName(EngineMetadata* meta, string_view type_name) -> optional<ComplexTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    constexpr string_view callback_prefix = "callback_";
    constexpr string_view callback_suffix = "_callback";

    if (!type_name.starts_with(callback_prefix) || !type_name.ends_with(callback_suffix)) {
        return std::nullopt;
    }

    string_view body = type_name.substr(callback_prefix.size(), type_name.size() - callback_prefix.size() - callback_suffix.size());
    auto callback_types = SplitWasmExportCallbackTypeList(meta, body);

    if (!callback_types.has_value()) {
        return std::nullopt;
    }

    return ComplexTypeDesc {
        .Kind = ComplexTypeKind::Callback,
        .CallbackArgs = SafeAlloc::MakeShared<vector<ComplexTypeDesc>>(std::move(callback_types.value())),
    };
}

static auto ResolveWasmExportTypeName(EngineMetadata* meta, string_view type_name, bool allow_void) -> optional<ComplexTypeDesc>
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
        if (auto dict_type = ResolveWasmExportDictTypeName(meta, type_name); dict_type.has_value()) {
            return dict_type;
        }
        if (auto dict_of_array_type = ResolveWasmExportDictOfArrayTypeName(meta, type_name); dict_of_array_type.has_value()) {
            return dict_of_array_type;
        }
        if (auto mutable_type = ResolveWasmExportMutableTypeName(meta, type_name); mutable_type.has_value()) {
            return mutable_type;
        }
        if (auto callback_type = ResolveWasmExportCallbackTypeName(meta, type_name); callback_type.has_value()) {
            return callback_type;
        }

        return std::nullopt;
    }

    return meta->ResolveComplexType(type_name);
}

static auto SplitWasmExportTypeList(EngineMetadata* meta, string_view type_names) -> optional<vector<string_view>>
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

            if (ResolveWasmExportTypeName(meta, type_name, false).has_value()) {
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

struct WasmExportMetadataSignature
{
    string ScriptExportName {};
    vector<ComplexTypeDesc> Args {};
    ComplexTypeDesc Ret {};
    bool HasSignature {};
};

static auto ParseWasmExportMetadataSignature(EngineMetadata* meta, string_view export_name) -> optional<WasmExportMetadataSignature>
{
    FO_STACK_TRACE_ENTRY();

    size_t ret_separator = export_name.rfind("__");

    if (ret_separator == string_view::npos) {
        return WasmExportMetadataSignature {.ScriptExportName = string {export_name}};
    }

    if (ret_separator == 0) {
        return std::nullopt;
    }

    size_t args_separator = export_name.rfind("__", ret_separator - 1);

    if (args_separator == string_view::npos) {
        return WasmExportMetadataSignature {.ScriptExportName = string {export_name}};
    }
    if (args_separator == 0 || args_separator + 2 >= ret_separator || ret_separator + 2 >= export_name.size()) {
        return std::nullopt;
    }

    string_view script_export_name = export_name.substr(0, args_separator);
    string_view args_part = export_name.substr(args_separator + 2, ret_separator - args_separator - 2);
    string_view ret_part = export_name.substr(ret_separator + 2);
    auto arg_names = SplitWasmExportTypeList(meta, args_part);
    auto ret = ResolveWasmExportTypeName(meta, ret_part, true);

    if (!arg_names.has_value() || !ret.has_value()) {
        return std::nullopt;
    }

    WasmExportMetadataSignature signature {
        .ScriptExportName = string {script_export_name},
        .Ret = std::move(ret.value()),
        .HasSignature = true,
    };
    signature.Args.reserve(arg_names->size());

    for (string_view arg_name : arg_names.value()) {
        auto arg = ResolveWasmExportTypeName(meta, arg_name, false);

        if (!arg.has_value()) {
            return std::nullopt;
        }

        signature.Args.emplace_back(std::move(arg.value()));
    }

    return signature;
}

static auto GetWasmExportTextArg(const BaseTypeDesc& type, ptr<const void> data) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.IsString, "WASM backend invariant failed");

    if (type.Name == "any") {
        return *cast_from_void<const any_t*>(data);
    }

    FO_STRONG_ASSERT(type.Name == "string", "WASM backend invariant failed");
    return *cast_from_void<const string*>(data);
}

static auto GetWasmExportCallbackArg(nptr<ScriptSystem> script_sys, ptr<const DataAccessor> accessor, ptr<void> data) -> string
{
    FO_STACK_TRACE_ENTRY();

    unique_del_nptr<ScriptFuncDesc> callback = accessor->GetCallback(data);

    if (!callback) {
        return {};
    }
    if (callback->DelegateObj != 0) {
        if (!script_sys) {
            throw ScriptCallException("WASM export callback delegate has no owning script system", callback->Name);
        }

        return script_sys->RegisterTemporaryScriptCallback(take_not_null(callback));
    }

    return string {callback->Name.as_str()};
}

static auto UnpackWasmExportTextPointerLength(uint64_t raw_value) noexcept -> pair<uint32_t, uint32_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    uint32_t ptr = numeric_cast<uint32_t>(raw_value & 0xFFFFFFFF);
    uint32_t len = numeric_cast<uint32_t>(raw_value >> 32);
    return {ptr, len};
}

static void StoreWasmExportTextResult(const BaseTypeDesc& type, string_view text, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.IsString, "WASM backend invariant failed");

    if (type.Name == "any") {
        *cast_from_void<any_t*>(data) = any_t {string {text}};
        return;
    }

    FO_STRONG_ASSERT(type.Name == "string", "WASM backend invariant failed");
    *cast_from_void<string*>(data) = string {text};
}

static void ReadWasmExportTextResult(wasm_module_inst_t module_inst, const BaseTypeDesc& type, uint64_t raw_value, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    const auto [text_ptr, text_len] = UnpackWasmExportTextPointerLength(raw_value);
    (void)numeric_cast<int32_t>(text_len);

    if (text_len == 0) {
        StoreWasmExportTextResult(type, {}, data);
        return;
    }

    if (!wasm_runtime_validate_app_addr(module_inst, text_ptr, text_len)) {
        throw ScriptCallException("WASM export text return buffer is out of bounds", type.Name, text_ptr, text_len);
    }

    const void* native_ptr = wasm_runtime_addr_app_to_native(module_inst, text_ptr);

    if (native_ptr == nullptr) {
        throw ScriptCallException("WASM export text return address conversion failed", type.Name, text_ptr, text_len);
    }

    StoreWasmExportTextResult(type, string_view {cast_from_void<const char*>(native_ptr).get(), numeric_cast<size_t>(text_len)}, data);
}

static auto PackWasmExportBaseValue(const BaseEngine& engine, const BaseTypeDesc& type, wasm_valkind_t kind, ptr<const void> data, nptr<WasmExportRefHandleScope> ref_handles) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(kind);

    if (type.IsEnum) {
        FO_STRONG_ASSERT(type.EnumUnderlyingType != nullptr, "WASM backend invariant failed");
        return PackWasmExportBaseValue(engine, *type.EnumUnderlyingType, kind, data, ref_handles);
    }
    if (type.IsSimpleStruct) {
        FO_STRONG_ASSERT(type.StructLayout != nullptr, "WASM backend invariant failed");
        FO_STRONG_ASSERT(type.StructLayout->Fields.size() == 1, "WASM backend invariant failed");

        const FieldDesc& field = type.StructLayout->Fields.front();
        return PackWasmExportBaseValue(engine, field.Type, kind, cast_from_void<const uint8_t*>(data).offset(field.Offset), ref_handles);
    }
    if (type.IsComplexStruct) {
        uint64_t value = 0;
        FO_STRONG_ASSERT(type.Size <= sizeof(value), "WASM backend invariant failed");
        MemCopy(&value, data, type.Size);
        return value;
    }
    if (IsWasmExportEntityHandleType(type)) {
        nptr<const Entity> entity = NativeDataProvider::ReadConstTypedHandleSlot<Entity>(data);

        if (!entity) {
            return 0;
        }
        if (entity->IsDestroyed()) {
            throw ScriptCallException("WASM export entity argument is destroyed", type.Name, entity->GetId().underlying_value());
        }

        ptr<const PropertyRegistrar> actual_registrator = entity->GetProperties()->GetRegistrar();

        if (!IsWasmExportEntityTypeCompatible(type.Name, actual_registrator->GetTypeName().as_str())) {
            throw ScriptCallException("WASM export entity argument type mismatch", type.Name, actual_registrator->GetTypeName());
        }

        return PackWasmExportIdent(entity->GetId());
    }
    if (IsWasmExportProtoOrFixedHandleType(type)) {
        nptr<const Entity> entity = NativeDataProvider::ReadConstTypedHandleSlot<Entity>(data);

        if (!entity) {
            return 0;
        }

        nptr<const ProtoEntity> proto = entity.dyn_cast<ProtoEntity>();

        if (!proto) {
            throw ScriptCallException("WASM export proto argument type mismatch", type.Name);
        }

        ptr<const PropertyRegistrar> actual_registrator = proto->GetProperties()->GetRegistrar();

        if (!IsWasmExportEntityTypeCompatible(type.Name, actual_registrator->GetTypeName().as_str())) {
            throw ScriptCallException("WASM export proto argument type mismatch", type.Name, actual_registrator->GetTypeName());
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

    throw ScriptCallException("Unsupported WASM export argument type", type.Name);
}

static void UnpackWasmExportBaseValue(BaseEngine& engine, const BaseTypeDesc& type, uint64_t value, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsEnum) {
        FO_STRONG_ASSERT(type.EnumUnderlyingType != nullptr, "WASM backend invariant failed");
        UnpackWasmExportBaseValue(engine, *type.EnumUnderlyingType, value, data);
        return;
    }
    if (type.IsSimpleStruct) {
        FO_STRONG_ASSERT(type.StructLayout != nullptr, "WASM backend invariant failed");
        FO_STRONG_ASSERT(type.StructLayout->Fields.size() == 1, "WASM backend invariant failed");

        const FieldDesc& field = type.StructLayout->Fields.front();
        UnpackWasmExportBaseValue(engine, field.Type, value, cast_from_void<uint8_t*>(data).offset(field.Offset));
        return;
    }
    if (type.IsComplexStruct) {
        FO_STRONG_ASSERT(type.Size <= sizeof(value), "WASM backend invariant failed");
        MemCopy(data, &value, type.Size);
        return;
    }
    if (IsWasmExportEntityHandleType(type)) {
        NativeDataProvider::WriteTypedHandleSlot<Entity>(data, ResolveWasmExportEntityHandle(engine, type, value, "return"));
        return;
    }
    if (IsWasmExportProtoOrFixedHandleType(type)) {
        NativeDataProvider::WriteTypedHandleSlot<Entity>(data, ResolveWasmExportProtoHandle(engine, type, value, "return"));
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

    throw ScriptCallException("Unsupported WASM export return type", type.Name);
}

static void WriteWasmExportRawValue(uint64_t value, span<uint8_t> raw_data);
static auto ReadWasmExportRawValue(span<const uint8_t> raw_data) noexcept -> uint64_t;

static void CopyWasmExportWireValueToNative(BaseEngine& engine, const BaseTypeDesc& type, span<const uint8_t> wire_data, span<uint8_t> native_data)
{
    FO_STACK_TRACE_ENTRY();

    if (wire_data.size() != type.Size || native_data.size() != type.Size) {
        throw ScriptCallException("Invalid WASM export array element size", type.Name, wire_data.size(), native_data.size(), type.Size);
    }

    if (type.IsHashedString) {
        hstring value = engine.Hashes.ResolveHash(ReadWasmExportRawValue(wire_data));
        MemCopy(native_data.data(), &value, sizeof(value));
        return;
    }
    if (type.StructLayout != nullptr) {
        for (const FieldDesc& field : type.StructLayout->Fields) {
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= wire_data.size(), "WASM backend invariant failed");
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= native_data.size(), "WASM backend invariant failed");
            CopyWasmExportWireValueToNative(engine, field.Type, wire_data.subspan(field.Offset, field.Type.Size), native_data.subspan(field.Offset, field.Type.Size));
        }
        return;
    }

    MemCopy(native_data.data(), wire_data.data(), type.Size);
}

static void CopyWasmExportNativeValueToWire(const BaseTypeDesc& type, span<const uint8_t> native_data, span<uint8_t> wire_data)
{
    FO_STACK_TRACE_ENTRY();

    if (wire_data.size() != type.Size || native_data.size() != type.Size) {
        throw ScriptCallException("Invalid WASM export array element output size", type.Name, native_data.size(), wire_data.size(), type.Size);
    }

    if (type.IsHashedString) {
        hstring value;
        MemCopy(&value, native_data.data(), sizeof(value));
        WriteWasmExportRawValue(value.as_hash(), wire_data);
        return;
    }
    if (type.StructLayout != nullptr) {
        for (const FieldDesc& field : type.StructLayout->Fields) {
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= native_data.size(), "WASM backend invariant failed");
            FO_STRONG_ASSERT(field.Offset + field.Type.Size <= wire_data.size(), "WASM backend invariant failed");
            CopyWasmExportNativeValueToWire(field.Type, native_data.subspan(field.Offset, field.Type.Size), wire_data.subspan(field.Offset, field.Type.Size));
        }
        return;
    }

    MemCopy(wire_data.data(), native_data.data(), type.Size);
}

using WasmExportArrayElementStorage = variant<WasmExportOpaqueValueStorage, hstring, string, any_t, Entity*, void*>;

static void AppendWasmExportU32(vector<uint8_t>& data, uint32_t value)
{
    FO_STACK_TRACE_ENTRY();

    size_t old_size = data.size();
    data.resize(old_size + sizeof(value));
    MemCopy(data.data() + old_size, &value, sizeof(value));
}

static auto ReadWasmExportU32(span<const uint8_t> data, size_t& offset, string_view role) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    if (offset + sizeof(uint32_t) > data.size()) {
        throw ScriptCallException(strex("WASM export {} buffer header is out of bounds", role), offset, data.size());
    }

    uint32_t value = 0;
    MemCopy(&value, data.data() + offset, sizeof(value));
    offset += sizeof(value);
    return value;
}

static auto GetWasmExportArrayElementWireSize(const BaseTypeDesc& type) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmExportEntityHandleType(type) || IsWasmExportProtoOrFixedHandleType(type) || type.IsRefType || type.IsHashedString) {
        return sizeof(uint64_t);
    }
    if (IsWasmExportBufferElementType(type)) {
        return type.Size;
    }

    if (type.Size == 0 || type.Size > sizeof(uint64_t)) {
        throw ScriptCallException("WASM export array element has unsupported size", type.Name, type.Size);
    }

    return type.Size;
}

static void WriteWasmExportRawValue(uint64_t value, span<uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(raw_data.size() <= sizeof(value), "WASM backend invariant failed");
    MemCopy(raw_data.data(), &value, raw_data.size());
}

static auto ReadWasmExportRawValue(span<const uint8_t> raw_data) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t value = 0;
    MemCopy(&value, raw_data.data(), raw_data.size());
    return value;
}

static auto PrepareWasmExportArrayElementStorage(const BaseTypeDesc& type, WasmExportArrayElementStorage& storage) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsString) {
        if (type.Name == "any") {
            storage.emplace<any_t>();
            return make_nptr(&std::get<any_t>(storage)).void_cast();
        }

        FO_STRONG_ASSERT(type.Name == "string", "WASM backend invariant failed");
        storage.emplace<string>();
        return make_nptr(&std::get<string>(storage)).void_cast();
    }
    if (IsWasmExportEntityHandleType(type) || IsWasmExportProtoOrFixedHandleType(type)) {
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

    WasmExportOpaqueValueStorage& opaque = storage.emplace<WasmExportOpaqueValueStorage>();
    opaque.Data.fill(0);
    return make_nptr(opaque.Data.data()).void_cast();
}

static void WriteWasmExportFixedElement(BaseEngine& engine, const BaseTypeDesc& type, ptr<const void> data, span<uint8_t> raw_data, nptr<WasmExportRefHandleScope> ref_handles)
{
    FO_STACK_TRACE_ENTRY();

    WasmScalarKind element_kind = TryResolveWasmExportScalarKind(type);

    if (element_kind != WasmScalarKind::None) {
        uint64_t value = PackWasmExportBaseValue(engine, type, WasmScalarKindToWamrValueKind(element_kind), data, ref_handles);
        WriteWasmExportRawValue(value, raw_data);
        return;
    }
    if (IsWasmExportBufferElementType(type)) {
        FO_STRONG_ASSERT(raw_data.size() == type.Size, "WASM backend invariant failed");
        CopyWasmExportNativeValueToWire(type, span<const uint8_t> {cast_from_void<const uint8_t*>(data).get(), type.Size}, raw_data);
        return;
    }

    throw ScriptCallException("Unsupported WASM export collection element type", type.Name);
}

static void ReadWasmExportFixedElement(BaseEngine& engine, const BaseTypeDesc& type, span<const uint8_t> raw_data, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    WasmScalarKind element_kind = TryResolveWasmExportScalarKind(type);

    if (element_kind != WasmScalarKind::None) {
        uint64_t value = ReadWasmExportRawValue(raw_data);
        UnpackWasmExportBaseValue(engine, type, value, data);
        return;
    }
    if (IsWasmExportBufferElementType(type)) {
        FO_STRONG_ASSERT(raw_data.size() == type.Size, "WASM backend invariant failed");
        CopyWasmExportWireValueToNative(engine, type, raw_data, span<uint8_t> {cast_from_void<uint8_t*>(data).get(), type.Size});
        return;
    }

    throw ScriptCallException("Unsupported WASM export collection element type", type.Name);
}

static void WriteWasmExportDictElement(BaseEngine& engine, const BaseTypeDesc& type, ptr<const void> data, vector<uint8_t>& raw_data, nptr<WasmExportRefHandleScope> ref_handles)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsString) {
        string_view text = GetWasmExportTextArg(type, data);
        AppendWasmExportU32(raw_data, numeric_cast<uint32_t>(numeric_cast<int32_t>(text.size())));
        raw_data.insert(raw_data.end(), text.begin(), text.end());
        return;
    }

    size_t element_size = GetWasmExportArrayElementWireSize(type);
    size_t old_size = raw_data.size();
    raw_data.resize(old_size + element_size);

    WriteWasmExportFixedElement(engine, type, data, span<uint8_t> {raw_data.data(), raw_data.size()}.subspan(old_size, element_size), ref_handles);
}

static auto ReadWasmExportDictElement(BaseEngine& engine, const BaseTypeDesc& type, span<const uint8_t> raw_data, size_t& offset, WasmExportArrayElementStorage& storage, string_view role) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    ptr<void> element_data = PrepareWasmExportArrayElementStorage(type, storage);

    if (type.IsString) {
        uint32_t text_size = ReadWasmExportU32(raw_data, offset, role);

        if (offset + text_size > raw_data.size()) {
            throw ScriptCallException(strex("WASM export {} dict text element is out of bounds", role), type.Name, text_size, offset, raw_data.size());
        }

        StoreWasmExportTextResult(type, string_view {reinterpret_cast<const char*>(raw_data.data() + offset), numeric_cast<size_t>(text_size)}, element_data);
        offset += text_size;
        return element_data;
    }

    size_t element_size = GetWasmExportArrayElementWireSize(type);

    if (offset + element_size > raw_data.size()) {
        throw ScriptCallException(strex("WASM export {} dict element is out of bounds", role), type.Name, element_size, offset, raw_data.size());
    }

    ReadWasmExportFixedElement(engine, type, raw_data.subspan(offset, element_size), element_data);
    offset += element_size;
    return element_data;
}

static auto SerializeWasmExportArrayArg(BaseEngine& engine, const ComplexTypeDesc& type, ptr<const DataAccessor> accessor, ptr<void> data, nptr<WasmExportRefHandleScope> ref_handles) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportArrayType(type) || IsWasmExportMutableArrayType(type), "WASM backend invariant failed");
    size_t array_size = accessor->GetArraySize(data);
    vector<uint8_t> raw_data;

    if (IsWasmExportTextArrayType(type) || IsWasmExportMutableTextArrayType(type)) {
        AppendWasmExportU32(raw_data, numeric_cast<uint32_t>(array_size));

        for (size_t index = 0; index < array_size; index++) {
            string_view text = GetWasmExportTextArg(type.BaseType, accessor->GetArrayElement(data, index));
            AppendWasmExportU32(raw_data, numeric_cast<uint32_t>(numeric_cast<int32_t>(text.size())));
            raw_data.insert(raw_data.end(), text.begin(), text.end());
        }

        return raw_data;
    }

    size_t element_size = GetWasmExportArrayElementWireSize(type.BaseType);
    raw_data.resize(array_size * element_size);

    for (size_t index = 0; index < array_size; index++) {
        WriteWasmExportFixedElement(engine, type.BaseType, accessor->GetArrayElement(data, index), span<uint8_t> {raw_data.data(), raw_data.size()}.subspan(index * element_size, element_size), ref_handles);
    }

    return raw_data;
}

static auto SerializeWasmExportDictArg(BaseEngine& engine, const ComplexTypeDesc& type, ptr<const DataAccessor> accessor, ptr<void> data, nptr<WasmExportRefHandleScope> ref_handles) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportDictType(type) || IsWasmExportMutableDictType(type) || IsWasmExportDictOfArrayType(type) || IsWasmExportMutableDictOfArrayType(type), "WASM backend invariant failed");
    FO_STRONG_ASSERT(type.KeyType.has_value(), "WASM backend invariant failed");
    size_t dict_size = accessor->GetDictSize(data);
    vector<uint8_t> raw_data;
    AppendWasmExportU32(raw_data, numeric_cast<uint32_t>(dict_size));

    for (size_t index = 0; index < dict_size; index++) {
        const auto [key_data, value_data] = accessor->GetDictElement(data, index);
        WriteWasmExportDictElement(engine, *type.KeyType, key_data, raw_data, ref_handles);

        if (type.Kind == ComplexTypeKind::DictOfArray) {
            size_t array_size = accessor->GetNestedArraySize(type.BaseType, value_data);
            AppendWasmExportU32(raw_data, numeric_cast<uint32_t>(array_size));

            for (size_t value_index = 0; value_index < array_size; value_index++) {
                if (type.BaseType.IsBool) {
                    bool value = accessor->GetNestedArrayBoolElement(type.BaseType, value_data, value_index);
                    WriteWasmExportDictElement(engine, type.BaseType, make_nptr(&value).void_cast(), raw_data, ref_handles);
                }
                else {
                    WriteWasmExportDictElement(engine, type.BaseType, accessor->GetNestedArrayElement(type.BaseType, value_data, value_index), raw_data, ref_handles);
                }
            }
        }
        else {
            WriteWasmExportDictElement(engine, type.BaseType, value_data, raw_data, ref_handles);
        }
    }

    return raw_data;
}

struct WasmExportDictArrayValueStorage
{
    vector<WasmExportArrayElementStorage> ElementStorages {};
    vector<ptr<void>> ElementData {};
};

static auto ReadWasmExportDictArrayValue(BaseEngine& engine, const BaseTypeDesc& type, span<const uint8_t> raw_data, size_t& offset, string_view role) -> WasmExportDictArrayValueStorage
{
    FO_STACK_TRACE_ENTRY();

    WasmExportDictArrayValueStorage result;
    uint32_t array_size = ReadWasmExportU32(raw_data, offset, role);
    result.ElementStorages.reserve(array_size);
    result.ElementData.reserve(array_size);

    for (uint32_t index = 0; index < array_size; index++) {
        WasmExportArrayElementStorage& storage = result.ElementStorages.emplace_back();
        result.ElementData.emplace_back(ReadWasmExportDictElement(engine, type, raw_data, offset, storage, role));
    }

    return result;
}

static auto SerializeWasmExportValueArg(BaseEngine& engine, const ComplexTypeDesc& type, ptr<void> data, nptr<WasmExportRefHandleScope> ref_handles) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportValueBufferType(type) || IsWasmExportMutableValueType(type), "WASM backend invariant failed");

    size_t element_size = GetWasmExportArrayElementWireSize(type.BaseType);
    vector<uint8_t> raw_data(element_size);

    WriteWasmExportFixedElement(engine, type.BaseType, data, raw_data, ref_handles);
    return raw_data;
}

static void StoreWasmExportArrayResult(BaseEngine& engine, const ComplexTypeDesc& type, span<const uint8_t> raw_data, ptr<const DataAccessor> accessor, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportArrayType(type) || IsWasmExportMutableArrayType(type), "WASM backend invariant failed");
    accessor->ClearArray(data);

    if (IsWasmExportTextArrayType(type) || IsWasmExportMutableTextArrayType(type)) {
        size_t offset = 0;
        uint32_t array_size = ReadWasmExportU32(raw_data, offset, "text return");

        for (uint32_t index = 0; index < array_size; index++) {
            uint32_t text_size = ReadWasmExportU32(raw_data, offset, "text return");

            if (offset + text_size > raw_data.size()) {
                throw ScriptCallException("WASM export text array return element is out of bounds", type.BaseType.Name, index, text_size, offset, raw_data.size());
            }

            WasmExportArrayElementStorage storage;
            ptr<void> element_data = PrepareWasmExportArrayElementStorage(type.BaseType, storage);
            StoreWasmExportTextResult(type.BaseType, string_view {reinterpret_cast<const char*>(raw_data.data() + offset), numeric_cast<size_t>(text_size)}, element_data);
            accessor->AddArrayElement(data, element_data);
            offset += text_size;
        }

        if (offset != raw_data.size()) {
            throw ScriptCallException("WASM export text array return has trailing bytes", type.BaseType.Name, offset, raw_data.size());
        }

        return;
    }

    size_t element_size = GetWasmExportArrayElementWireSize(type.BaseType);

    if (raw_data.size() % element_size != 0) {
        throw ScriptCallException("WASM export array return buffer length is not aligned to element size", type.BaseType.Name, raw_data.size(), element_size);
    }

    size_t array_size = raw_data.size() / element_size;

    for (size_t index = 0; index < array_size; index++) {
        WasmExportArrayElementStorage storage;
        ptr<void> element_data = PrepareWasmExportArrayElementStorage(type.BaseType, storage);
        ReadWasmExportFixedElement(engine, type.BaseType, raw_data.subspan(index * element_size, element_size), element_data);
        accessor->AddArrayElement(data, element_data);
    }
}

static void StoreWasmExportDictResult(BaseEngine& engine, const ComplexTypeDesc& type, span<const uint8_t> raw_data, ptr<const DataAccessor> accessor, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportDictType(type) || IsWasmExportMutableDictType(type) || IsWasmExportDictOfArrayType(type) || IsWasmExportMutableDictOfArrayType(type), "WASM backend invariant failed");
    FO_STRONG_ASSERT(type.KeyType.has_value(), "WASM backend invariant failed");
    accessor->ClearDict(data);

    if (raw_data.empty()) {
        return;
    }

    size_t offset = 0;
    uint32_t dict_size = ReadWasmExportU32(raw_data, offset, "dict return");

    for (uint32_t index = 0; index < dict_size; index++) {
        WasmExportArrayElementStorage key_storage;
        WasmExportArrayElementStorage value_storage;
        ptr<void> key_data = ReadWasmExportDictElement(engine, *type.KeyType, raw_data, offset, key_storage, "dict return key");

        if (type.Kind == ComplexTypeKind::DictOfArray) {
            WasmExportDictArrayValueStorage value_array = ReadWasmExportDictArrayValue(engine, type.BaseType, raw_data, offset, "dict return value array");
            accessor->AddDictArrayElement(data, key_data, type.BaseType, value_array.ElementData);
        }
        else {
            ptr<void> value_data = ReadWasmExportDictElement(engine, type.BaseType, raw_data, offset, value_storage, "dict return value");
            accessor->AddDictElement(data, key_data, value_data);
        }
    }

    if (offset != raw_data.size()) {
        throw ScriptCallException("WASM export dict return has trailing bytes", type.BaseType.Name, offset, raw_data.size());
    }
}

static void StoreWasmExportValueResult(BaseEngine& engine, const ComplexTypeDesc& type, span<const uint8_t> raw_data, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportValueBufferType(type) || IsWasmExportMutableValueType(type), "WASM backend invariant failed");

    size_t element_size = GetWasmExportArrayElementWireSize(type.BaseType);

    if (raw_data.size() != element_size) {
        throw ScriptCallException("WASM export value buffer length does not match element size", type.BaseType.Name, raw_data.size(), element_size);
    }

    ReadWasmExportFixedElement(engine, type.BaseType, raw_data, data);
}

struct WasmExportMutableBufferArg
{
    uint32_t Ptr {};
    uint32_t ByteLen {};
    uint32_t CapacityByteLen {};
    uint32_t RequiredByteLengthPtr {};
};

static void ReadWasmExportArrayResult(wasm_module_inst_t module_inst, BaseEngine& engine, const ComplexTypeDesc& type, uint64_t raw_value, ptr<const DataAccessor> accessor, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    const auto [array_ptr, array_len] = UnpackWasmExportTextPointerLength(raw_value);
    (void)numeric_cast<int32_t>(array_len);

    if (array_len == 0) {
        StoreWasmExportArrayResult(engine, type, {}, accessor, data);
        return;
    }

    if (!wasm_runtime_validate_app_addr(module_inst, array_ptr, array_len)) {
        throw ScriptCallException("WASM export array return buffer is out of bounds", type.BaseType.Name, array_ptr, array_len);
    }

    const void* native_ptr = wasm_runtime_addr_app_to_native(module_inst, array_ptr);

    if (native_ptr == nullptr) {
        throw ScriptCallException("WASM export array return address conversion failed", type.BaseType.Name, array_ptr, array_len);
    }

    StoreWasmExportArrayResult(engine, type, {cast_from_void<const uint8_t*>(native_ptr).get(), numeric_cast<size_t>(array_len)}, accessor, data);
}

static void ReadWasmExportValueResult(wasm_module_inst_t module_inst, BaseEngine& engine, const ComplexTypeDesc& type, uint64_t raw_value, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    const auto [value_ptr, value_len] = UnpackWasmExportTextPointerLength(raw_value);
    (void)numeric_cast<int32_t>(value_len);
    size_t element_size = GetWasmExportArrayElementWireSize(type.BaseType);

    if (value_len != element_size) {
        throw ScriptCallException("WASM export value return buffer length does not match element size", type.BaseType.Name, value_len, element_size);
    }
    if (!wasm_runtime_validate_app_addr(module_inst, value_ptr, value_len)) {
        throw ScriptCallException("WASM export value return buffer is out of bounds", type.BaseType.Name, value_ptr, value_len);
    }

    const void* native_ptr = wasm_runtime_addr_app_to_native(module_inst, value_ptr);

    if (native_ptr == nullptr) {
        throw ScriptCallException("WASM export value return address conversion failed", type.BaseType.Name, value_ptr, value_len);
    }

    StoreWasmExportValueResult(engine, type, {cast_from_void<const uint8_t*>(native_ptr).get(), numeric_cast<size_t>(value_len)}, data);
}

static void ReadWasmExportDictResult(wasm_module_inst_t module_inst, BaseEngine& engine, const ComplexTypeDesc& type, uint64_t raw_value, ptr<const DataAccessor> accessor, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    const auto [dict_ptr, dict_len] = UnpackWasmExportTextPointerLength(raw_value);
    (void)numeric_cast<int32_t>(dict_len);

    if (dict_len == 0) {
        StoreWasmExportDictResult(engine, type, {}, accessor, data);
        return;
    }

    if (!wasm_runtime_validate_app_addr(module_inst, dict_ptr, dict_len)) {
        throw ScriptCallException("WASM export dict return buffer is out of bounds", type.BaseType.Name, dict_ptr, dict_len);
    }

    const void* native_ptr = wasm_runtime_addr_app_to_native(module_inst, dict_ptr);

    if (native_ptr == nullptr) {
        throw ScriptCallException("WASM export dict return address conversion failed", type.BaseType.Name, dict_ptr, dict_len);
    }

    StoreWasmExportDictResult(engine, type, {cast_from_void<const uint8_t*>(native_ptr).get(), numeric_cast<size_t>(dict_len)}, accessor, data);
}

static void FillWasmI32Arg(wasm_val_t& wasm_arg, uint32_t value) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    wasm_arg.kind = WASM_I32;
    wasm_arg.of.i32 = std::bit_cast<int32_t>(value);
}

static void FillWasmArg(wasm_val_t& wasm_arg, wasm_valkind_t kind, const ComplexTypeDesc& type, const BaseEngine& engine, ptr<void> data, nptr<WasmExportRefHandleScope> ref_handles)
{
    FO_STACK_TRACE_ENTRY();

    uint64_t value = PackWasmExportBaseValue(engine, type.BaseType, kind, data, ref_handles);
    wasm_arg.kind = kind;

    switch (kind) {
    case WASM_I32:
        wasm_arg.of.i32 = std::bit_cast<int32_t>(numeric_cast<uint32_t>(value));
        break;
    case WASM_I64:
        wasm_arg.of.i64 = std::bit_cast<int64_t>(value);
        break;
    case WASM_F32:
        wasm_arg.of.f32 = std::bit_cast<float32_t>(numeric_cast<uint32_t>(value));
        break;
    case WASM_F64:
        wasm_arg.of.f64 = std::bit_cast<float64_t>(value);
        break;
    default:
        throw ScriptCallException("Unsupported WASM argument type", kind);
    }
}

static void ReadWasmResult(wasm_module_inst_t module_inst, const wasm_val_t& wasm_result, const ComplexTypeDesc& type, BaseEngine& engine, ptr<const DataAccessor> accessor, ptr<void> data)
{
    FO_STACK_TRACE_ENTRY();

    if (IsWasmExportTextType(type)) {
        if (wasm_result.kind != WASM_I64) {
            throw ScriptCallException("Unsupported WASM text return type", wasm_result.kind);
        }

        ReadWasmExportTextResult(module_inst, type.BaseType, std::bit_cast<uint64_t>(wasm_result.of.i64), data);
        return;
    }
    if (IsWasmExportValueBufferType(type)) {
        if (wasm_result.kind != WASM_I64) {
            throw ScriptCallException("Unsupported WASM value return type", wasm_result.kind);
        }

        ReadWasmExportValueResult(module_inst, engine, type, std::bit_cast<uint64_t>(wasm_result.of.i64), data);
        return;
    }
    if (IsWasmExportArrayType(type)) {
        if (wasm_result.kind != WASM_I64) {
            throw ScriptCallException("Unsupported WASM array return type", wasm_result.kind);
        }

        ReadWasmExportArrayResult(module_inst, engine, type, std::bit_cast<uint64_t>(wasm_result.of.i64), accessor, data);
        return;
    }
    if (IsWasmExportDictType(type) || IsWasmExportDictOfArrayType(type)) {
        if (wasm_result.kind != WASM_I64) {
            throw ScriptCallException("Unsupported WASM dict return type", wasm_result.kind);
        }

        ReadWasmExportDictResult(module_inst, engine, type, std::bit_cast<uint64_t>(wasm_result.of.i64), accessor, data);
        return;
    }

    uint64_t value = 0;

    switch (wasm_result.kind) {
    case WASM_I32:
        value = std::bit_cast<uint32_t>(wasm_result.of.i32);
        break;
    case WASM_I64:
        value = std::bit_cast<uint64_t>(wasm_result.of.i64);
        break;
    case WASM_F32:
        value = std::bit_cast<uint32_t>(wasm_result.of.f32);
        break;
    case WASM_F64:
        value = std::bit_cast<uint64_t>(wasm_result.of.f64);
        break;
    default:
        throw ScriptCallException("Unsupported WASM return type", wasm_result.kind);
    }

    UnpackWasmExportBaseValue(engine, type.BaseType, value, data);
}

static auto AllocateWasmExportTextArg(wasm_module_inst_t module_inst, const ComplexTypeDesc& type, ptr<void> data, vector<uint64_t>& allocations) -> pair<uint32_t, uint32_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportTextType(type), "WASM backend invariant failed");

    string_view text = GetWasmExportTextArg(type.BaseType, data);
    uint32_t text_len = numeric_cast<uint32_t>(numeric_cast<int32_t>(text.size()));

    if (text.empty()) {
        return {0, 0};
    }

    void* native_ptr = nullptr;
    uint64_t app_ptr = wasm_runtime_module_malloc(module_inst, text.size(), &native_ptr);

    if (app_ptr == 0 || native_ptr == nullptr) {
        throw ScriptCallException("WASM export text argument allocation failed", type.BaseType.Name, text.size());
    }

    allocations.emplace_back(app_ptr);
    MemCopy(native_ptr, text.data(), text.size());

    return {numeric_cast<uint32_t>(app_ptr), text_len};
}

static auto AllocateWasmExportCallbackArg(wasm_module_inst_t module_inst, nptr<ScriptSystem> script_sys, const ComplexTypeDesc& type, ptr<const DataAccessor> accessor, ptr<void> data, vector<uint64_t>& allocations) -> pair<uint32_t, uint32_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportCallbackType(type), "WASM backend invariant failed");

    string callback_name = GetWasmExportCallbackArg(script_sys, accessor, data);
    uint32_t callback_len = numeric_cast<uint32_t>(numeric_cast<int32_t>(callback_name.size()));

    if (callback_name.empty()) {
        return {0, 0};
    }

    void* native_ptr = nullptr;
    uint64_t app_ptr = wasm_runtime_module_malloc(module_inst, callback_name.size(), &native_ptr);

    if (app_ptr == 0 || native_ptr == nullptr) {
        throw ScriptCallException("WASM export callback argument allocation failed", callback_name.size());
    }

    allocations.emplace_back(app_ptr);
    MemCopy(native_ptr, callback_name.data(), callback_name.size());

    return {numeric_cast<uint32_t>(app_ptr), callback_len};
}

static auto AllocateWasmExportValueArg(wasm_module_inst_t module_inst, BaseEngine& engine, const ComplexTypeDesc& type, ptr<void> data, vector<uint64_t>& allocations, nptr<WasmExportRefHandleScope> ref_handles) -> pair<uint32_t, uint32_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportValueBufferType(type), "WASM backend invariant failed");

    vector<uint8_t> raw_data = SerializeWasmExportValueArg(engine, type, data, ref_handles);
    uint32_t raw_len = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));

    void* native_ptr = nullptr;
    uint64_t app_ptr = wasm_runtime_module_malloc(module_inst, raw_data.size(), &native_ptr);

    if (app_ptr == 0 || native_ptr == nullptr) {
        throw ScriptCallException("WASM export value argument allocation failed", type.BaseType.Name, raw_data.size());
    }

    allocations.emplace_back(app_ptr);
    MemCopy(native_ptr, raw_data.data(), raw_data.size());

    return {numeric_cast<uint32_t>(app_ptr), raw_len};
}

static auto AllocateWasmExportMutableTextArg(wasm_module_inst_t module_inst, const ComplexTypeDesc& type, ptr<void> data, vector<uint64_t>& allocations) -> WasmExportMutableBufferArg
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportMutableTextType(type), "WASM backend invariant failed");

    string_view text = GetWasmExportTextArg(type.BaseType, data);
    uint32_t text_len = numeric_cast<uint32_t>(numeric_cast<int32_t>(text.size()));
    WasmExportMutableBufferArg result {
        .ByteLen = text_len,
        .CapacityByteLen = text_len,
    };

    if (!text.empty()) {
        void* native_ptr = nullptr;
        uint64_t app_ptr = wasm_runtime_module_malloc(module_inst, text.size(), &native_ptr);

        if (app_ptr == 0 || native_ptr == nullptr) {
            throw ScriptCallException("WASM export mutable text argument allocation failed", type.BaseType.Name, text.size());
        }

        allocations.emplace_back(app_ptr);
        MemCopy(native_ptr, text.data(), text.size());
        result.Ptr = numeric_cast<uint32_t>(app_ptr);
    }

    void* required_native_ptr = nullptr;
    uint64_t required_app_ptr = wasm_runtime_module_malloc(module_inst, sizeof(uint32_t), &required_native_ptr);

    if (required_app_ptr == 0 || required_native_ptr == nullptr) {
        throw ScriptCallException("WASM export mutable text required length allocation failed", type.BaseType.Name);
    }

    allocations.emplace_back(required_app_ptr);
    MemCopy(required_native_ptr, &text_len, sizeof(text_len));
    result.RequiredByteLengthPtr = numeric_cast<uint32_t>(required_app_ptr);
    return result;
}

static auto AllocateWasmExportArrayArg(wasm_module_inst_t module_inst, BaseEngine& engine, const ComplexTypeDesc& type, ptr<const DataAccessor> accessor, ptr<void> data, vector<uint64_t>& allocations, nptr<WasmExportRefHandleScope> ref_handles) -> pair<uint32_t, uint32_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportArrayType(type), "WASM backend invariant failed");

    vector<uint8_t> raw_data = SerializeWasmExportArrayArg(engine, type, accessor, data, ref_handles);
    uint32_t raw_len = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));

    if (raw_data.empty()) {
        return {0, 0};
    }

    void* native_ptr = nullptr;
    uint64_t app_ptr = wasm_runtime_module_malloc(module_inst, raw_data.size(), &native_ptr);

    if (app_ptr == 0 || native_ptr == nullptr) {
        throw ScriptCallException("WASM export array argument allocation failed", type.BaseType.Name, raw_data.size());
    }

    allocations.emplace_back(app_ptr);
    MemCopy(native_ptr, raw_data.data(), raw_data.size());

    return {numeric_cast<uint32_t>(app_ptr), raw_len};
}

static auto AllocateWasmExportMutableArrayArg(wasm_module_inst_t module_inst, BaseEngine& engine, const ComplexTypeDesc& type, ptr<const DataAccessor> accessor, ptr<void> data, vector<uint64_t>& allocations, nptr<WasmExportRefHandleScope> ref_handles) -> WasmExportMutableBufferArg
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportMutableArrayType(type), "WASM backend invariant failed");

    vector<uint8_t> raw_data = SerializeWasmExportArrayArg(engine, type, accessor, data, ref_handles);
    uint32_t raw_len = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
    WasmExportMutableBufferArg result {
        .ByteLen = raw_len,
        .CapacityByteLen = raw_len,
    };

    if (!raw_data.empty()) {
        void* native_ptr = nullptr;
        uint64_t app_ptr = wasm_runtime_module_malloc(module_inst, raw_data.size(), &native_ptr);

        if (app_ptr == 0 || native_ptr == nullptr) {
            throw ScriptCallException("WASM export mutable array argument allocation failed", type.BaseType.Name, raw_data.size());
        }

        allocations.emplace_back(app_ptr);
        MemCopy(native_ptr, raw_data.data(), raw_data.size());
        result.Ptr = numeric_cast<uint32_t>(app_ptr);
    }

    void* required_native_ptr = nullptr;
    uint64_t required_app_ptr = wasm_runtime_module_malloc(module_inst, sizeof(uint32_t), &required_native_ptr);

    if (required_app_ptr == 0 || required_native_ptr == nullptr) {
        throw ScriptCallException("WASM export mutable array required length allocation failed", type.BaseType.Name);
    }

    allocations.emplace_back(required_app_ptr);
    MemCopy(required_native_ptr, &raw_len, sizeof(raw_len));
    result.RequiredByteLengthPtr = numeric_cast<uint32_t>(required_app_ptr);
    return result;
}

static auto AllocateWasmExportMutableDictArg(wasm_module_inst_t module_inst, BaseEngine& engine, const ComplexTypeDesc& type, ptr<const DataAccessor> accessor, ptr<void> data, vector<uint64_t>& allocations, nptr<WasmExportRefHandleScope> ref_handles) -> WasmExportMutableBufferArg
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportMutableDictType(type) || IsWasmExportMutableDictOfArrayType(type), "WASM backend invariant failed");

    vector<uint8_t> raw_data = SerializeWasmExportDictArg(engine, type, accessor, data, ref_handles);
    uint32_t raw_len = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));
    WasmExportMutableBufferArg result {
        .ByteLen = raw_len,
        .CapacityByteLen = raw_len,
    };

    if (!raw_data.empty()) {
        void* native_ptr = nullptr;
        uint64_t app_ptr = wasm_runtime_module_malloc(module_inst, raw_data.size(), &native_ptr);

        if (app_ptr == 0 || native_ptr == nullptr) {
            throw ScriptCallException("WASM export mutable dict argument allocation failed", type.BaseType.Name, raw_data.size());
        }

        allocations.emplace_back(app_ptr);
        MemCopy(native_ptr, raw_data.data(), raw_data.size());
        result.Ptr = numeric_cast<uint32_t>(app_ptr);
    }

    void* required_native_ptr = nullptr;
    uint64_t required_app_ptr = wasm_runtime_module_malloc(module_inst, sizeof(uint32_t), &required_native_ptr);

    if (required_app_ptr == 0 || required_native_ptr == nullptr) {
        throw ScriptCallException("WASM export mutable dict required length allocation failed", type.BaseType.Name);
    }

    allocations.emplace_back(required_app_ptr);
    MemCopy(required_native_ptr, &raw_len, sizeof(raw_len));
    result.RequiredByteLengthPtr = numeric_cast<uint32_t>(required_app_ptr);
    return result;
}

static auto AllocateWasmExportMutableValueArg(wasm_module_inst_t module_inst, BaseEngine& engine, const ComplexTypeDesc& type, ptr<void> data, vector<uint64_t>& allocations, nptr<WasmExportRefHandleScope> ref_handles) -> pair<uint32_t, uint32_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportMutableValueType(type), "WASM backend invariant failed");

    vector<uint8_t> raw_data = SerializeWasmExportValueArg(engine, type, data, ref_handles);
    uint32_t raw_len = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));

    void* native_ptr = nullptr;
    uint64_t app_ptr = wasm_runtime_module_malloc(module_inst, raw_data.size(), &native_ptr);

    if (app_ptr == 0 || native_ptr == nullptr) {
        throw ScriptCallException("WASM export mutable value argument allocation failed", type.BaseType.Name, raw_data.size());
    }

    allocations.emplace_back(app_ptr);
    MemCopy(native_ptr, raw_data.data(), raw_data.size());

    return {numeric_cast<uint32_t>(app_ptr), raw_len};
}

static auto AllocateWasmExportDictArg(wasm_module_inst_t module_inst, BaseEngine& engine, const ComplexTypeDesc& type, ptr<const DataAccessor> accessor, ptr<void> data, vector<uint64_t>& allocations, nptr<WasmExportRefHandleScope> ref_handles) -> pair<uint32_t, uint32_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsWasmExportDictType(type) || IsWasmExportDictOfArrayType(type), "WASM backend invariant failed");

    vector<uint8_t> raw_data = SerializeWasmExportDictArg(engine, type, accessor, data, ref_handles);
    uint32_t raw_len = numeric_cast<uint32_t>(numeric_cast<int32_t>(raw_data.size()));

    if (raw_data.empty()) {
        return {0, 0};
    }

    void* native_ptr = nullptr;
    uint64_t app_ptr = wasm_runtime_module_malloc(module_inst, raw_data.size(), &native_ptr);

    if (app_ptr == 0 || native_ptr == nullptr) {
        throw ScriptCallException("WASM export dict argument allocation failed", type.BaseType.Name, raw_data.size());
    }

    allocations.emplace_back(app_ptr);
    MemCopy(native_ptr, raw_data.data(), raw_data.size());

    return {numeric_cast<uint32_t>(app_ptr), raw_len};
}

WasmBackend::WasmBackend(ptr<const ScriptSettings> settings) :
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();

    AcquireWasmRuntime();
}

WasmBackend::~WasmBackend()
{
    FO_STACK_TRACE_ENTRY();

    UnloadModules();
    UnregisterApiNativeSymbols();
    ReleaseWasmRuntime();
}

void WasmBackend::RegisterMetadata(ptr<EngineMetadata> meta)
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

    WriteLog("Prepared WASM {} metadata API bridge: {} supported method import{}, {} supported property import{}, {} pending ABI import{}", GetWasmApiImportTableSideName(_apiImports.Side), supported_methods, supported_methods != 1 ? "s" : "", supported_properties, supported_properties != 1 ? "s" : "", pending_imports, pending_imports != 1 ? "s" : "");

    RegisterApiNativeSymbols();
}

void WasmBackend::LoadScripts(const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_meta, "Missing engine metadata");

    auto wasm_files = resources.FilterFiles("wasm");

    for (const auto& file_header : wasm_files) {
        if (!ShouldLoadForSide(file_header.GetPath(), _meta->GetSide())) {
            continue;
        }

        LoadModule(File::Load(file_header));
    }
}

void WasmBackend::LoadModule(const File& file)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_meta, "WASM backend invariant failed");

    auto module = SafeAlloc::MakeUnique<WasmModule>();
    module->Name = MakeModuleName(file);
    module->Path = file.GetPath();
    module->Binary = file.GetData();

    array<char, WASM_ERROR_BUF_SIZE> error_buf {};

    module->Module = wasm_runtime_load(module->Binary.data(), numeric_cast<uint32_t>(module->Binary.size()), error_buf.data(), numeric_cast<uint32_t>(error_buf.size()));

    if (module->Module == nullptr) {
        throw ScriptSystemException("WASM module load failed", module->Path, MakeWasmError(error_buf));
    }

    try {
        ValidateWasmModuleImports(module->Module, module->Path, _apiImports.Methods, _apiImports.Properties);
    }
    catch (...) {
        wasm_runtime_unload(module->Module);
        module->Module = nullptr;
        throw;
    }

    module->ModuleInst = wasm_runtime_instantiate(module->Module, numeric_cast<uint32_t>(_settings->WasmStackSize), numeric_cast<uint32_t>(_settings->WasmHeapSize), error_buf.data(), numeric_cast<uint32_t>(error_buf.size()));

    if (module->ModuleInst == nullptr) {
        wasm_runtime_unload(module->Module);
        module->Module = nullptr;
        throw ScriptSystemException("WASM module instantiate failed", module->Path, MakeWasmError(error_buf));
    }

    module->ExecEnv = wasm_runtime_create_exec_env(module->ModuleInst, numeric_cast<uint32_t>(_settings->WasmStackSize));

    if (module->ExecEnv == nullptr) {
        wasm_runtime_deinstantiate(module->ModuleInst);
        wasm_runtime_unload(module->Module);
        module->ModuleInst = nullptr;
        module->Module = nullptr;
        throw ScriptSystemException("WASM execution environment creation failed", module->Path);
    }

    RegisterModuleExports(module);

    if (!module->Functions.empty()) {
        WriteLog("Loaded WASM module {} with {} function{}", module->Path, module->Functions.size(), module->Functions.size() != 1 ? "s" : "");
    }

    _modules.emplace_back(std::move(module));
}

void WasmBackend::RegisterApiNativeSymbols()
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_engine, "WASM backend invariant failed");
    FO_STRONG_ASSERT(!_apiNativeSymbolsRegistered, "WASM backend invariant failed");

    size_t supported_methods = CountWasmApiSupportedMethods(_apiImports);
    size_t supported_properties = CountWasmApiSupportedProperties(_apiImports);
    size_t supported_imports = supported_methods + supported_properties;

    if (supported_imports == 0) {
        return;
    }

    _apiNativeMethods.clear();
    _apiNativeSymbolNames.clear();
    _apiNativeSymbolSignatures.clear();
    _apiNativeSymbols.clear();
    _apiNativeMethods.reserve(supported_imports);
    _apiNativeSymbolNames.reserve(supported_imports);
    _apiNativeSymbolSignatures.reserve(supported_imports);
    _apiNativeSymbols.reserve(supported_imports);

    for (const WasmApiMethodDesc& method : _apiImports.Methods) {
        if (!method.Supported) {
            continue;
        }

        auto& native_method = _apiNativeMethods.emplace_back(WasmApiNativeMethod {
            .Engine = _engine,
            .Method = &method,
        });
        string& native_name = _apiNativeSymbolNames.emplace_back(method.Name);
        string& native_signature = _apiNativeSymbolSignatures.emplace_back(MakeWasmApiNativeSignature(method));

        _apiNativeSymbols.emplace_back(NativeSymbol {
            native_name.c_str(),
            reinterpret_cast<void*>(&WasmNativeApiCall),
            native_signature.c_str(),
            &native_method,
        });
    }

    for (const WasmApiPropertyDesc& property : _apiImports.Properties) {
        if (!property.Supported) {
            continue;
        }

        auto& native_method = _apiNativeMethods.emplace_back(WasmApiNativeMethod {
            .Engine = _engine,
            .Property = &property,
        });
        string& native_name = _apiNativeSymbolNames.emplace_back(property.Name);
        string& native_signature = _apiNativeSymbolSignatures.emplace_back(MakeWasmApiPropertyNativeSignature(property));

        _apiNativeSymbols.emplace_back(NativeSymbol {
            native_name.c_str(),
            reinterpret_cast<void*>(&WasmNativeApiCall),
            native_signature.c_str(),
            &native_method,
        });
    }

    if (!wasm_runtime_register_natives_raw(WASM_ENGINE_API_MODULE.data(), _apiNativeSymbols.data(), numeric_cast<uint32_t>(_apiNativeSymbols.size()))) {
        throw ScriptSystemException("WASM engine API native registration failed");
    }

    _apiNativeSymbolsRegistered = true;
}

void WasmBackend::UnregisterApiNativeSymbols() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_apiNativeSymbolsRegistered) {
        return;
    }

    (void)wasm_runtime_unregister_natives(WASM_ENGINE_API_MODULE.data(), _apiNativeSymbols.data());
    _apiNativeSymbolsRegistered = false;
}

void WasmBackend::RegisterModuleExports(ptr<WasmModule> module)
{
    FO_STACK_TRACE_ENTRY();

    if (_scriptSys == nullptr) {
        return;
    }

    int32_t export_count = wasm_runtime_get_export_count(module->Module);

    if (export_count < 0) {
        throw ScriptSystemException("WASM export enumeration failed", module->Path);
    }

    for (int32_t export_index = 0; export_index < export_count; export_index++) {
        wasm_export_t export_type {};
        wasm_runtime_get_export_type(module->Module, export_index, &export_type);

        if (export_type.kind != WASM_IMPORT_EXPORT_KIND_FUNC || export_type.name == nullptr || export_type.u.func_type == nullptr) {
            continue;
        }

        uint32_t param_count = wasm_func_type_get_param_count(export_type.u.func_type);
        uint32_t result_count = wasm_func_type_get_result_count(export_type.u.func_type);

        if (param_count > MAX_CALL_ARGS || result_count > 1) {
            WriteLog(LogType::Warning, "Skip WASM export {} in {}: unsupported arity", export_type.name, module->Path);
            continue;
        }

        auto export_signature = ParseWasmExportMetadataSignature(_meta.get(), export_type.name);

        if (!export_signature.has_value()) {
            WriteLog(LogType::Warning, "Skip WASM export {} in {}: unsupported metadata signature", export_type.name, module->Path);
            continue;
        }

        auto function = SafeAlloc::MakeUnique<WasmFunction>();
        function->ExportName = export_type.name;
        function->ScriptName = strex("{}::{}", module->Name, export_signature->ScriptExportName);
        function->Function = wasm_runtime_lookup_function(module->ModuleInst, function->ExportName.c_str());

        if (function->Function == nullptr) {
            WriteLog(LogType::Warning, "Skip WASM export {} in {}: lookup failed", function->ExportName, module->Path);
            continue;
        }

        auto func_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        func_desc->Name = _meta->Hashes.ToHashedString(function->ScriptName);

        bool supported_signature = true;

        if (export_signature->HasSignature) {
            if (export_signature->Ret ? result_count != 1 : result_count != 0) {
                supported_signature = false;
            }

            uint32_t param_index = 0;

            for (size_t arg_index = 0; supported_signature && arg_index < export_signature->Args.size(); arg_index++) {
                const ComplexTypeDesc& arg_type = export_signature->Args[arg_index];

                if (IsWasmExportTextType(arg_type)) {
                    if (param_index + 1 >= param_count) {
                        supported_signature = false;
                        break;
                    }

                    wasm_valkind_t ptr_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                    wasm_valkind_t len_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 1);

                    if (ptr_type != WASM_I32 || len_type != WASM_I32) {
                        supported_signature = false;
                        break;
                    }

                    function->Args.emplace_back(ptr_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::Utf8StringPointer);
                    function->Args.emplace_back(len_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::Utf8StringLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }
                if (IsWasmExportCallbackType(arg_type)) {
                    if (param_index + 1 >= param_count) {
                        supported_signature = false;
                        break;
                    }

                    wasm_valkind_t ptr_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                    wasm_valkind_t len_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 1);

                    if (ptr_type != WASM_I32 || len_type != WASM_I32) {
                        supported_signature = false;
                        break;
                    }

                    function->Args.emplace_back(ptr_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::CallbackPointer);
                    function->Args.emplace_back(len_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::CallbackLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }
                if (IsWasmExportValueBufferType(arg_type)) {
                    if (param_index + 1 >= param_count) {
                        supported_signature = false;
                        break;
                    }

                    wasm_valkind_t ptr_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                    wasm_valkind_t len_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 1);

                    if (ptr_type != WASM_I32 || len_type != WASM_I32) {
                        supported_signature = false;
                        break;
                    }

                    function->Args.emplace_back(ptr_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::ValuePointer);
                    function->Args.emplace_back(len_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::ValueByteLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }
                if (IsWasmExportMutableTextType(arg_type)) {
                    if (param_index + 3 >= param_count) {
                        supported_signature = false;
                        break;
                    }

                    wasm_valkind_t ptr_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                    wasm_valkind_t len_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 1);
                    wasm_valkind_t capacity_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 2);
                    wasm_valkind_t required_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 3);

                    if (ptr_type != WASM_I32 || len_type != WASM_I32 || capacity_type != WASM_I32 || required_type != WASM_I32) {
                        supported_signature = false;
                        break;
                    }

                    function->Args.emplace_back(ptr_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringPointer);
                    function->Args.emplace_back(len_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringByteLength);
                    function->Args.emplace_back(capacity_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringCapacityByteLength);
                    function->Args.emplace_back(required_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableUtf8StringRequiredByteLengthPointer);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 4;
                    continue;
                }
                if (IsWasmExportArrayType(arg_type)) {
                    if (param_index + 1 >= param_count) {
                        supported_signature = false;
                        break;
                    }

                    wasm_valkind_t ptr_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                    wasm_valkind_t len_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 1);

                    if (ptr_type != WASM_I32 || len_type != WASM_I32) {
                        supported_signature = false;
                        break;
                    }

                    function->Args.emplace_back(ptr_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::ArrayPointer);
                    function->Args.emplace_back(len_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::ArrayByteLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }
                if (IsWasmExportMutableArrayType(arg_type)) {
                    if (param_index + 3 >= param_count) {
                        supported_signature = false;
                        break;
                    }

                    wasm_valkind_t ptr_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                    wasm_valkind_t len_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 1);
                    wasm_valkind_t capacity_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 2);
                    wasm_valkind_t required_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 3);

                    if (ptr_type != WASM_I32 || len_type != WASM_I32 || capacity_type != WASM_I32 || required_type != WASM_I32) {
                        supported_signature = false;
                        break;
                    }

                    function->Args.emplace_back(ptr_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableArrayPointer);
                    function->Args.emplace_back(len_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableArrayByteLength);
                    function->Args.emplace_back(capacity_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableArrayCapacityByteLength);
                    function->Args.emplace_back(required_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 4;
                    continue;
                }
                if (IsWasmExportMutableDictType(arg_type) || IsWasmExportMutableDictOfArrayType(arg_type)) {
                    if (param_index + 3 >= param_count) {
                        supported_signature = false;
                        break;
                    }

                    wasm_valkind_t ptr_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                    wasm_valkind_t len_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 1);
                    wasm_valkind_t capacity_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 2);
                    wasm_valkind_t required_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 3);

                    if (ptr_type != WASM_I32 || len_type != WASM_I32 || capacity_type != WASM_I32 || required_type != WASM_I32) {
                        supported_signature = false;
                        break;
                    }

                    function->Args.emplace_back(ptr_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableDictPointer);
                    function->Args.emplace_back(len_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableDictByteLength);
                    function->Args.emplace_back(capacity_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableDictCapacityByteLength);
                    function->Args.emplace_back(required_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 4;
                    continue;
                }
                if (IsWasmExportDictType(arg_type) || IsWasmExportDictOfArrayType(arg_type)) {
                    if (param_index + 1 >= param_count) {
                        supported_signature = false;
                        break;
                    }

                    wasm_valkind_t ptr_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                    wasm_valkind_t len_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 1);

                    if (ptr_type != WASM_I32 || len_type != WASM_I32) {
                        supported_signature = false;
                        break;
                    }

                    function->Args.emplace_back(ptr_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::DictPointer);
                    function->Args.emplace_back(len_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::DictByteLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }
                if (IsWasmExportMutableValueType(arg_type)) {
                    if (param_index + 1 >= param_count) {
                        supported_signature = false;
                        break;
                    }

                    wasm_valkind_t ptr_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                    wasm_valkind_t len_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index + 1);

                    if (ptr_type != WASM_I32 || len_type != WASM_I32) {
                        supported_signature = false;
                        break;
                    }

                    function->Args.emplace_back(ptr_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableValuePointer);
                    function->Args.emplace_back(len_type);
                    function->ArgAbi.emplace_back(WasmApiParamAbiKind::MutableValueLength);
                    function->ArgTypes.emplace_back(arg_type);
                    func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                    param_index += 2;
                    continue;
                }

                if (param_index >= param_count) {
                    supported_signature = false;
                    break;
                }

                wasm_valkind_t param_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                WasmScalarKind actual_kind = ResolveWasmScalarKindFromWamr(param_type);
                WasmScalarKind expected_kind = TryResolveWasmExportScalarKind(arg_type);

                if (actual_kind == WasmScalarKind::None || expected_kind == WasmScalarKind::None || actual_kind != expected_kind) {
                    supported_signature = false;
                    break;
                }

                function->Args.emplace_back(param_type);
                function->ArgAbi.emplace_back(WasmApiParamAbiKind::Scalar);
                function->ArgTypes.emplace_back(arg_type);
                func_desc->Args.emplace_back(ArgDesc {.Name = strex("arg{}", arg_index), .Type = arg_type});
                param_index++;
            }

            if (supported_signature && param_index != param_count) {
                supported_signature = false;
            }

            if (supported_signature && export_signature->Ret) {
                wasm_valkind_t result_type = wasm_func_type_get_result_valkind(export_type.u.func_type, 0);

                if (IsWasmExportTextType(export_signature->Ret)) {
                    if (result_type != WASM_I64) {
                        supported_signature = false;
                    }
                    else {
                        function->Ret = result_type;
                        function->RetType = export_signature->Ret;
                        func_desc->Ret = export_signature->Ret;
                    }
                }
                else if (IsWasmExportValueBufferType(export_signature->Ret)) {
                    if (result_type != WASM_I64) {
                        supported_signature = false;
                    }
                    else {
                        function->Ret = result_type;
                        function->RetType = export_signature->Ret;
                        func_desc->Ret = export_signature->Ret;
                    }
                }
                else if (IsWasmExportArrayType(export_signature->Ret)) {
                    if (result_type != WASM_I64) {
                        supported_signature = false;
                    }
                    else {
                        function->Ret = result_type;
                        function->RetType = export_signature->Ret;
                        func_desc->Ret = export_signature->Ret;
                    }
                }
                else if (IsWasmExportDictType(export_signature->Ret) || IsWasmExportDictOfArrayType(export_signature->Ret)) {
                    if (result_type != WASM_I64) {
                        supported_signature = false;
                    }
                    else {
                        function->Ret = result_type;
                        function->RetType = export_signature->Ret;
                        func_desc->Ret = export_signature->Ret;
                    }
                }
                else {
                    WasmScalarKind actual_kind = ResolveWasmScalarKindFromWamr(result_type);
                    WasmScalarKind expected_kind = TryResolveWasmExportScalarKind(export_signature->Ret);

                    if (actual_kind == WasmScalarKind::None || expected_kind == WasmScalarKind::None || actual_kind != expected_kind) {
                        supported_signature = false;
                    }
                    else {
                        function->Ret = result_type;
                        function->RetType = export_signature->Ret;
                        func_desc->Ret = export_signature->Ret;
                    }
                }
            }
        }
        else {
            for (uint32_t param_index = 0; param_index < param_count; param_index++) {
                wasm_valkind_t param_type = wasm_func_type_get_param_valkind(export_type.u.func_type, param_index);
                auto arg_desc = MakeArgDesc(_meta.get(), param_type, param_index);

                if (!arg_desc.has_value()) {
                    supported_signature = false;
                    break;
                }

                ArgDesc arg_desc_value = std::move(arg_desc.value());
                function->Args.emplace_back(param_type);
                function->ArgAbi.emplace_back(WasmApiParamAbiKind::Scalar);
                function->ArgTypes.emplace_back(arg_desc_value.Type);
                func_desc->Args.emplace_back(std::move(arg_desc_value));
            }

            if (supported_signature && result_count == 1) {
                wasm_valkind_t result_type = wasm_func_type_get_result_valkind(export_type.u.func_type, 0);
                auto ret_desc = ResolveWasmType(_meta.get(), result_type);

                if (!ret_desc.has_value()) {
                    supported_signature = false;
                }
                else {
                    function->Ret = result_type;
                    function->RetType = ret_desc.value();
                    func_desc->Ret = std::move(ret_desc.value());
                }
            }
        }

        if (!supported_signature) {
            WriteLog(LogType::Warning, "Skip WASM export {} in {}: unsupported value type", function->ExportName, module->Path);
            continue;
        }

        ptr<const WasmFunction> registered_function = function;
        func_desc->Call = [this, module, registered_function](FuncCallData& call) { CallWasmFunction(module, registered_function, call); };
        func_desc->AttributeChecker = [](string_view attribute) noexcept -> bool { return attribute == "Wasm"; };

        function->FuncDesc = std::move(func_desc);
        _scriptSys->AddGlobalScriptFunc(function->FuncDesc.get());
        module->Functions.emplace_back(std::move(function));
    }
}

void WasmBackend::UnloadModules() noexcept
{
    FO_STACK_TRACE_ENTRY();

    for (auto& module : _modules) {
        if (module->ExecEnv != nullptr) {
            wasm_runtime_destroy_exec_env(module->ExecEnv);
            module->ExecEnv = nullptr;
        }
        if (module->ModuleInst != nullptr) {
            wasm_runtime_deinstantiate(module->ModuleInst);
            module->ModuleInst = nullptr;
        }
        if (module->Module != nullptr) {
            wasm_runtime_unload(module->Module);
            module->Module = nullptr;
        }
    }

    _modules.clear();
}

void WasmBackend::CallWasmFunction(ptr<WasmModule> module, ptr<const WasmFunction> function, FuncCallData& call) const
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(function->ArgTypes.size() == call.ArgsData.size(), "WASM backend invariant failed");
    FO_STRONG_ASSERT(function->ArgAbi.size() == function->Args.size(), "WASM backend invariant failed");

    vector<wasm_val_t> args(function->Args.size());
    vector<uint64_t> module_allocations;
    WasmExportRefHandleScope ref_handle_scope;
    struct MutableValueCopyback
    {
        uint64_t AppPtr {};
        uint32_t ByteLen {};
        ComplexTypeDesc Type {};
        ptr<void> Data;
    };
    struct MutableTextCopyback
    {
        uint64_t AppPtr {};
        uint32_t CapacityByteLen {};
        uint64_t RequiredByteLengthPtr {};
        ComplexTypeDesc Type {};
        ptr<void> Data;
    };
    struct MutableArrayCopyback
    {
        uint64_t AppPtr {};
        uint32_t CapacityByteLen {};
        uint64_t RequiredByteLengthPtr {};
        ComplexTypeDesc Type {};
        ptr<void> Data;
    };
    struct MutableDictCopyback
    {
        uint64_t AppPtr {};
        uint32_t CapacityByteLen {};
        uint64_t RequiredByteLengthPtr {};
        ComplexTypeDesc Type {};
        ptr<void> Data;
    };
    vector<MutableValueCopyback> mutable_value_copybacks;
    vector<MutableTextCopyback> mutable_text_copybacks;
    vector<MutableArrayCopyback> mutable_array_copybacks;
    vector<MutableDictCopyback> mutable_dict_copybacks;
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

    auto free_module_allocations = [&]() noexcept {
        for (uint64_t app_ptr : module_allocations) {
            wasm_runtime_module_free(module->ModuleInst, app_ptr);
        }

        module_allocations.clear();
    };

    try {
        size_t raw_arg_index = 0;

        for (size_t arg_index = 0; arg_index < function->ArgTypes.size(); arg_index++) {
            FO_STRONG_ASSERT(raw_arg_index < function->Args.size(), "WASM backend invariant failed");

            WasmApiParamAbiKind arg_abi = function->ArgAbi[raw_arg_index];

            if (arg_abi == WasmApiParamAbiKind::Utf8StringPointer) {
                FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::Utf8StringLength, "WASM backend invariant failed");

                const auto [text_ptr, text_len] = AllocateWasmExportTextArg(module->ModuleInst, function->ArgTypes[arg_index], call.ArgsData[arg_index], module_allocations);
                FillWasmI32Arg(args[raw_arg_index], text_ptr);
                FillWasmI32Arg(args[raw_arg_index + 1], text_len);
                raw_arg_index += 2;
                continue;
            }
            if (arg_abi == WasmApiParamAbiKind::CallbackPointer) {
                FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::CallbackLength, "WASM backend invariant failed");

                const auto [callback_ptr, callback_len] = AllocateWasmExportCallbackArg(module->ModuleInst, script_sys, function->ArgTypes[arg_index], call.Accessor, call.ArgsData[arg_index], module_allocations);
                FillWasmI32Arg(args[raw_arg_index], callback_ptr);
                FillWasmI32Arg(args[raw_arg_index + 1], callback_len);
                raw_arg_index += 2;
                continue;
            }
            if (arg_abi == WasmApiParamAbiKind::ValuePointer) {
                FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::ValueByteLength, "WASM backend invariant failed");

                const auto [value_ptr, value_len] = AllocateWasmExportValueArg(module->ModuleInst, *_engine.get_no_const(), function->ArgTypes[arg_index], call.ArgsData[arg_index], module_allocations, make_nptr(&ref_handle_scope));
                FillWasmI32Arg(args[raw_arg_index], value_ptr);
                FillWasmI32Arg(args[raw_arg_index + 1], value_len);
                raw_arg_index += 2;
                continue;
            }
            if (arg_abi == WasmApiParamAbiKind::MutableUtf8StringPointer) {
                FO_STRONG_ASSERT(raw_arg_index + 3 < function->Args.size(), "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableUtf8StringByteLength, "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 2] == WasmApiParamAbiKind::MutableUtf8StringCapacityByteLength, "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 3] == WasmApiParamAbiKind::MutableUtf8StringRequiredByteLengthPointer, "WASM backend invariant failed");

                WasmExportMutableBufferArg text_arg = AllocateWasmExportMutableTextArg(module->ModuleInst, function->ArgTypes[arg_index], call.ArgsData[arg_index], module_allocations);
                mutable_text_copybacks.emplace_back(MutableTextCopyback {
                    .AppPtr = text_arg.Ptr,
                    .CapacityByteLen = text_arg.CapacityByteLen,
                    .RequiredByteLengthPtr = text_arg.RequiredByteLengthPtr,
                    .Type = function->ArgTypes[arg_index],
                    .Data = call.ArgsData[arg_index],
                });
                FillWasmI32Arg(args[raw_arg_index], text_arg.Ptr);
                FillWasmI32Arg(args[raw_arg_index + 1], text_arg.ByteLen);
                FillWasmI32Arg(args[raw_arg_index + 2], text_arg.CapacityByteLen);
                FillWasmI32Arg(args[raw_arg_index + 3], text_arg.RequiredByteLengthPtr);
                raw_arg_index += 4;
                continue;
            }
            if (arg_abi == WasmApiParamAbiKind::ArrayPointer) {
                FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::ArrayByteLength, "WASM backend invariant failed");

                const auto [array_ptr, array_len] = AllocateWasmExportArrayArg(module->ModuleInst, *_engine.get_no_const(), function->ArgTypes[arg_index], call.Accessor, call.ArgsData[arg_index], module_allocations, make_nptr(&ref_handle_scope));
                FillWasmI32Arg(args[raw_arg_index], array_ptr);
                FillWasmI32Arg(args[raw_arg_index + 1], array_len);
                raw_arg_index += 2;
                continue;
            }
            if (arg_abi == WasmApiParamAbiKind::MutableArrayPointer) {
                FO_STRONG_ASSERT(raw_arg_index + 3 < function->Args.size(), "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableArrayByteLength, "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 2] == WasmApiParamAbiKind::MutableArrayCapacityByteLength, "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 3] == WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer, "WASM backend invariant failed");

                WasmExportMutableBufferArg array_arg = AllocateWasmExportMutableArrayArg(module->ModuleInst, *_engine.get_no_const(), function->ArgTypes[arg_index], call.Accessor, call.ArgsData[arg_index], module_allocations, make_nptr(&ref_handle_scope));
                mutable_array_copybacks.emplace_back(MutableArrayCopyback {
                    .AppPtr = array_arg.Ptr,
                    .CapacityByteLen = array_arg.CapacityByteLen,
                    .RequiredByteLengthPtr = array_arg.RequiredByteLengthPtr,
                    .Type = function->ArgTypes[arg_index],
                    .Data = call.ArgsData[arg_index],
                });
                FillWasmI32Arg(args[raw_arg_index], array_arg.Ptr);
                FillWasmI32Arg(args[raw_arg_index + 1], array_arg.ByteLen);
                FillWasmI32Arg(args[raw_arg_index + 2], array_arg.CapacityByteLen);
                FillWasmI32Arg(args[raw_arg_index + 3], array_arg.RequiredByteLengthPtr);
                raw_arg_index += 4;
                continue;
            }
            if (arg_abi == WasmApiParamAbiKind::MutableDictPointer) {
                FO_STRONG_ASSERT(raw_arg_index + 3 < function->Args.size(), "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableDictByteLength, "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 2] == WasmApiParamAbiKind::MutableDictCapacityByteLength, "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 3] == WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer, "WASM backend invariant failed");

                WasmExportMutableBufferArg dict_arg = AllocateWasmExportMutableDictArg(module->ModuleInst, *_engine.get_no_const(), function->ArgTypes[arg_index], call.Accessor, call.ArgsData[arg_index], module_allocations, make_nptr(&ref_handle_scope));
                mutable_dict_copybacks.emplace_back(MutableDictCopyback {
                    .AppPtr = dict_arg.Ptr,
                    .CapacityByteLen = dict_arg.CapacityByteLen,
                    .RequiredByteLengthPtr = dict_arg.RequiredByteLengthPtr,
                    .Type = function->ArgTypes[arg_index],
                    .Data = call.ArgsData[arg_index],
                });
                FillWasmI32Arg(args[raw_arg_index], dict_arg.Ptr);
                FillWasmI32Arg(args[raw_arg_index + 1], dict_arg.ByteLen);
                FillWasmI32Arg(args[raw_arg_index + 2], dict_arg.CapacityByteLen);
                FillWasmI32Arg(args[raw_arg_index + 3], dict_arg.RequiredByteLengthPtr);
                raw_arg_index += 4;
                continue;
            }
            if (arg_abi == WasmApiParamAbiKind::DictPointer) {
                FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::DictByteLength, "WASM backend invariant failed");

                const auto [dict_ptr, dict_len] = AllocateWasmExportDictArg(module->ModuleInst, *_engine.get_no_const(), function->ArgTypes[arg_index], call.Accessor, call.ArgsData[arg_index], module_allocations, make_nptr(&ref_handle_scope));
                FillWasmI32Arg(args[raw_arg_index], dict_ptr);
                FillWasmI32Arg(args[raw_arg_index + 1], dict_len);
                raw_arg_index += 2;
                continue;
            }
            if (arg_abi == WasmApiParamAbiKind::MutableValuePointer) {
                FO_STRONG_ASSERT(raw_arg_index + 1 < function->Args.size(), "WASM backend invariant failed");
                FO_STRONG_ASSERT(function->ArgAbi[raw_arg_index + 1] == WasmApiParamAbiKind::MutableValueLength, "WASM backend invariant failed");

                const auto [value_ptr, value_len] = AllocateWasmExportMutableValueArg(module->ModuleInst, *_engine.get_no_const(), function->ArgTypes[arg_index], call.ArgsData[arg_index], module_allocations, make_nptr(&ref_handle_scope));
                mutable_value_copybacks.emplace_back(MutableValueCopyback {
                    .AppPtr = value_ptr,
                    .ByteLen = value_len,
                    .Type = function->ArgTypes[arg_index],
                    .Data = call.ArgsData[arg_index],
                });
                FillWasmI32Arg(args[raw_arg_index], value_ptr);
                FillWasmI32Arg(args[raw_arg_index + 1], value_len);
                raw_arg_index += 2;
                continue;
            }

            FO_STRONG_ASSERT(arg_abi == WasmApiParamAbiKind::Scalar, "WASM backend invariant failed");
            FillWasmArg(args[raw_arg_index], function->Args[raw_arg_index], function->ArgTypes[arg_index], *_engine.get_no_const(), call.ArgsData[arg_index], make_nptr(&ref_handle_scope));
            raw_arg_index++;
        }

        FO_STRONG_ASSERT(raw_arg_index == function->Args.size(), "WASM backend invariant failed");

        wasm_val_t result {};
        wasm_val_t* result_ptr = nullptr;
        uint32_t result_count = 0;

        if (function->Ret.has_value()) {
            result.kind = function->Ret.value();
            result_ptr = &result;
            result_count = 1;
            FO_STRONG_ASSERT(call.RetData != nullptr, "WASM backend invariant failed");
        }

        WasmRuntimeContext runtime_context = MakeWasmRuntimeContext(*_engine, _scriptSys.get_no_const());
        WasmModuleContextScope context_scope(module->ModuleInst, runtime_context);
        bool called = wasm_runtime_call_wasm_a(module->ExecEnv, function->Function, result_count, result_ptr, numeric_cast<uint32_t>(args.size()), args.empty() ? nullptr : args.data());

        if (called) {
            for (const MutableArrayCopyback& copyback : mutable_array_copybacks) {
                if (!copyback.Type.BaseType.IsRefType) {
                    continue;
                }
                if (!wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.RequiredByteLengthPtr), sizeof(uint32_t))) {
                    throw ScriptCallException("WASM export mutable ref array required length pointer is out of bounds", copyback.Type.BaseType.Name, copyback.RequiredByteLengthPtr);
                }

                const void* required_native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.RequiredByteLengthPtr);

                if (required_native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable ref array required length address conversion failed", copyback.Type.BaseType.Name, copyback.RequiredByteLengthPtr);
                }

                uint32_t required_byte_len = 0;
                MemCopy(&required_byte_len, required_native_ptr, sizeof(required_byte_len));

                if (required_byte_len > copyback.CapacityByteLen) {
                    throw ScriptCallException("WASM export mutable ref array requires a larger buffer", copyback.Type.BaseType.Name, required_byte_len, copyback.CapacityByteLen);
                }
                if (required_byte_len == 0) {
                    ref_handle_scope.ValidateAndRetainMutableArray(copyback.Type, {});
                    continue;
                }
                if (copyback.AppPtr == 0 || !wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.AppPtr), required_byte_len)) {
                    throw ScriptCallException("WASM export mutable ref array argument buffer is out of bounds", copyback.Type.BaseType.Name, copyback.AppPtr, required_byte_len);
                }

                const void* native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.AppPtr);

                if (native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable ref array address conversion failed", copyback.Type.BaseType.Name, copyback.AppPtr, required_byte_len);
                }

                ref_handle_scope.ValidateAndRetainMutableArray(copyback.Type, {cast_from_void<const uint8_t*>(native_ptr).get(), numeric_cast<size_t>(required_byte_len)});
            }

            for (const MutableDictCopyback& copyback : mutable_dict_copybacks) {
                if (!WasmExportRefHandleScope::ContainsRefHandles(copyback.Type)) {
                    continue;
                }
                if (!wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.RequiredByteLengthPtr), sizeof(uint32_t))) {
                    throw ScriptCallException("WASM export mutable ref dict required length pointer is out of bounds", copyback.Type.BaseType.Name, copyback.RequiredByteLengthPtr);
                }

                const void* required_native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.RequiredByteLengthPtr);

                if (required_native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable ref dict required length address conversion failed", copyback.Type.BaseType.Name, copyback.RequiredByteLengthPtr);
                }

                uint32_t required_byte_len = 0;
                MemCopy(&required_byte_len, required_native_ptr, sizeof(required_byte_len));

                if (required_byte_len > copyback.CapacityByteLen) {
                    throw ScriptCallException("WASM export mutable ref dict requires a larger buffer", copyback.Type.BaseType.Name, required_byte_len, copyback.CapacityByteLen);
                }
                if (required_byte_len == 0) {
                    ref_handle_scope.ValidateAndRetainMutableDict(copyback.Type, {});
                    continue;
                }
                if (copyback.AppPtr == 0 || !wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.AppPtr), required_byte_len)) {
                    throw ScriptCallException("WASM export mutable ref dict argument buffer is out of bounds", copyback.Type.BaseType.Name, copyback.AppPtr, required_byte_len);
                }

                const void* native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.AppPtr);

                if (native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable ref dict address conversion failed", copyback.Type.BaseType.Name, copyback.AppPtr, required_byte_len);
                }

                ref_handle_scope.ValidateAndRetainMutableDict(copyback.Type, {cast_from_void<const uint8_t*>(native_ptr).get(), numeric_cast<size_t>(required_byte_len)});
            }

            for (const MutableValueCopyback& copyback : mutable_value_copybacks) {
                if (!copyback.Type.BaseType.IsRefType) {
                    continue;
                }
                if (!wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.AppPtr), copyback.ByteLen)) {
                    throw ScriptCallException("WASM export mutable ref value argument buffer is out of bounds", copyback.Type.BaseType.Name, copyback.AppPtr, copyback.ByteLen);
                }

                const void* native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.AppPtr);

                if (native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable ref value address conversion failed", copyback.Type.BaseType.Name, copyback.AppPtr, copyback.ByteLen);
                }

                ref_handle_scope.ValidateAndRetainMutableValue(copyback.Type, {cast_from_void<const uint8_t*>(native_ptr).get(), numeric_cast<size_t>(copyback.ByteLen)});
            }

            for (const MutableTextCopyback& copyback : mutable_text_copybacks) {
                if (!wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.RequiredByteLengthPtr), sizeof(uint32_t))) {
                    throw ScriptCallException("WASM export mutable text required length pointer is out of bounds", copyback.Type.BaseType.Name, copyback.RequiredByteLengthPtr);
                }

                const void* required_native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.RequiredByteLengthPtr);

                if (required_native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable text required length address conversion failed", copyback.Type.BaseType.Name, copyback.RequiredByteLengthPtr);
                }

                uint32_t required_byte_len = 0;
                MemCopy(&required_byte_len, required_native_ptr, sizeof(required_byte_len));

                if (required_byte_len > copyback.CapacityByteLen) {
                    throw ScriptCallException("WASM export mutable text requires a larger buffer", copyback.Type.BaseType.Name, required_byte_len, copyback.CapacityByteLen);
                }
                if (required_byte_len == 0) {
                    StoreWasmExportTextResult(copyback.Type.BaseType, {}, copyback.Data);
                    continue;
                }
                if (copyback.AppPtr == 0 || !wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.AppPtr), required_byte_len)) {
                    throw ScriptCallException("WASM export mutable text argument buffer is out of bounds", copyback.Type.BaseType.Name, copyback.AppPtr, required_byte_len);
                }

                const void* native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.AppPtr);

                if (native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable text address conversion failed", copyback.Type.BaseType.Name, copyback.AppPtr, required_byte_len);
                }

                StoreWasmExportTextResult(copyback.Type.BaseType, string_view {cast_from_void<const char*>(native_ptr).get(), numeric_cast<size_t>(required_byte_len)}, copyback.Data);
            }

            for (const MutableArrayCopyback& copyback : mutable_array_copybacks) {
                if (!wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.RequiredByteLengthPtr), sizeof(uint32_t))) {
                    throw ScriptCallException("WASM export mutable array required length pointer is out of bounds", copyback.Type.BaseType.Name, copyback.RequiredByteLengthPtr);
                }

                const void* required_native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.RequiredByteLengthPtr);

                if (required_native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable array required length address conversion failed", copyback.Type.BaseType.Name, copyback.RequiredByteLengthPtr);
                }

                uint32_t required_byte_len = 0;
                MemCopy(&required_byte_len, required_native_ptr, sizeof(required_byte_len));

                if (required_byte_len > copyback.CapacityByteLen) {
                    throw ScriptCallException("WASM export mutable array requires a larger buffer", copyback.Type.BaseType.Name, required_byte_len, copyback.CapacityByteLen);
                }
                if (required_byte_len == 0) {
                    StoreWasmExportArrayResult(*_engine.get_no_const(), copyback.Type, {}, call.Accessor.get(), copyback.Data);
                    continue;
                }
                if (copyback.AppPtr == 0 || !wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.AppPtr), required_byte_len)) {
                    throw ScriptCallException("WASM export mutable array argument buffer is out of bounds", copyback.Type.BaseType.Name, copyback.AppPtr, required_byte_len);
                }

                const void* native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.AppPtr);

                if (native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable array address conversion failed", copyback.Type.BaseType.Name, copyback.AppPtr, required_byte_len);
                }

                const_span<uint8_t> raw_data {cast_from_void<const uint8_t*>(native_ptr).get(), numeric_cast<size_t>(required_byte_len)};

                StoreWasmExportArrayResult(*_engine.get_no_const(), copyback.Type, raw_data, call.Accessor.get(), copyback.Data);
            }

            for (const MutableDictCopyback& copyback : mutable_dict_copybacks) {
                if (!wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.RequiredByteLengthPtr), sizeof(uint32_t))) {
                    throw ScriptCallException("WASM export mutable dict required length pointer is out of bounds", copyback.Type.BaseType.Name, copyback.RequiredByteLengthPtr);
                }

                const void* required_native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.RequiredByteLengthPtr);

                if (required_native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable dict required length address conversion failed", copyback.Type.BaseType.Name, copyback.RequiredByteLengthPtr);
                }

                uint32_t required_byte_len = 0;
                MemCopy(&required_byte_len, required_native_ptr, sizeof(required_byte_len));

                if (required_byte_len > copyback.CapacityByteLen) {
                    throw ScriptCallException("WASM export mutable dict requires a larger buffer", copyback.Type.BaseType.Name, required_byte_len, copyback.CapacityByteLen);
                }
                if (required_byte_len == 0) {
                    StoreWasmExportDictResult(*_engine.get_no_const(), copyback.Type, {}, call.Accessor.get(), copyback.Data);
                    continue;
                }
                if (copyback.AppPtr == 0 || !wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.AppPtr), required_byte_len)) {
                    throw ScriptCallException("WASM export mutable dict argument buffer is out of bounds", copyback.Type.BaseType.Name, copyback.AppPtr, required_byte_len);
                }

                const void* native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.AppPtr);

                if (native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable dict address conversion failed", copyback.Type.BaseType.Name, copyback.AppPtr, required_byte_len);
                }

                const_span<uint8_t> raw_data {cast_from_void<const uint8_t*>(native_ptr).get(), numeric_cast<size_t>(required_byte_len)};

                StoreWasmExportDictResult(*_engine.get_no_const(), copyback.Type, raw_data, call.Accessor.get(), copyback.Data);
            }

            for (const MutableValueCopyback& copyback : mutable_value_copybacks) {
                if (!wasm_runtime_validate_app_addr(module->ModuleInst, numeric_cast<uint32_t>(copyback.AppPtr), copyback.ByteLen)) {
                    throw ScriptCallException("WASM export mutable value argument buffer is out of bounds", copyback.Type.BaseType.Name, copyback.AppPtr, copyback.ByteLen);
                }

                const void* native_ptr = wasm_runtime_addr_app_to_native(module->ModuleInst, copyback.AppPtr);

                if (native_ptr == nullptr) {
                    throw ScriptCallException("WASM export mutable value address conversion failed", copyback.Type.BaseType.Name, copyback.AppPtr, copyback.ByteLen);
                }

                const_span<uint8_t> raw_data {cast_from_void<const uint8_t*>(native_ptr).get(), numeric_cast<size_t>(copyback.ByteLen)};

                StoreWasmExportValueResult(*_engine.get_no_const(), copyback.Type, raw_data, copyback.Data);
            }

            ref_handle_scope.ReleaseRetained();
        }

        free_module_allocations();

        if (!called) {
            const char* exception = wasm_runtime_get_exception(module->ModuleInst);
            string exception_text = exception != nullptr ? string(exception) : string("unknown WASM exception");
            wasm_runtime_clear_exception(module->ModuleInst);
            throw ScriptCallException("WASM function call failed", function->ScriptName, exception_text);
        }

        if (function->Ret.has_value()) {
            ReadWasmResult(module->ModuleInst, result, function->RetType, *_engine.get_no_const(), call.Accessor.get(), call.RetData);
        }
    }
    catch (...) {
        free_module_allocations();
        throw;
    }
}

FO_END_NAMESPACE

#endif
