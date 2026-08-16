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

#include "WasmImports.h"

#if FO_WASM_SCRIPTING

#include "EngineBase.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

static const array<WasmImportDesc, 12> WASM_IMPORTS = {{
    {.Module = "fonline", .Name = "log_i32", .NativeSignature = "(i)", .Args = {WasmScalarKind::I32}, .ArgsCount = 1, .Ret = WasmScalarKind::None},
    {.Module = "fonline", .Name = "log_i64", .NativeSignature = "(I)", .Args = {WasmScalarKind::I64}, .ArgsCount = 1, .Ret = WasmScalarKind::None},
    {.Module = "fonline", .Name = "log_f32", .NativeSignature = "(f)", .Args = {WasmScalarKind::F32}, .ArgsCount = 1, .Ret = WasmScalarKind::None},
    {.Module = "fonline", .Name = "log_f64", .NativeSignature = "(F)", .Args = {WasmScalarKind::F64}, .ArgsCount = 1, .Ret = WasmScalarKind::None},
    {.Module = "fonline", .Name = "log_utf8", .NativeSignature = "(*~)", .Args = {WasmScalarKind::I32, WasmScalarKind::I32}, .ArgsCount = 2, .Ret = WasmScalarKind::None},
    {.Module = "fonline", .Name = "callback_retain", .NativeSignature = "(*~)i", .Args = {WasmScalarKind::I32, WasmScalarKind::I32}, .ArgsCount = 2, .Ret = WasmScalarKind::I32},
    {.Module = "fonline", .Name = "callback_release", .NativeSignature = "(*~)i", .Args = {WasmScalarKind::I32, WasmScalarKind::I32}, .ArgsCount = 2, .Ret = WasmScalarKind::I32},
    {.Module = "fonline", .Name = "get_side", .NativeSignature = "()i", .Ret = WasmScalarKind::I32},
    {.Module = "fonline", .Name = "get_frame_time_ms", .NativeSignature = "()I", .Ret = WasmScalarKind::I64},
    {.Module = "fonline", .Name = "get_frame_delta_time_ms", .NativeSignature = "()I", .Ret = WasmScalarKind::I64},
    {.Module = "fonline", .Name = "is_time_synchronized", .NativeSignature = "()i", .Ret = WasmScalarKind::I32},
    {.Module = "fonline", .Name = "get_synchronized_time_ms", .NativeSignature = "()I", .Ret = WasmScalarKind::I64},
}};

static const WasmRuntimeContext DEFAULT_WASM_RUNTIME_CONTEXT {};

auto GetWasmImportDescs() noexcept -> const_span<WasmImportDesc>
{
    FO_NO_STACK_TRACE_ENTRY();

    return WASM_IMPORTS;
}

auto FindWasmImportDesc(string_view module_name, string_view import_name) noexcept -> const WasmImportDesc*
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const WasmImportDesc& desc : WASM_IMPORTS) {
        if (desc.Module == module_name && desc.Name == import_name) {
            return &desc;
        }
    }

    return nullptr;
}

auto ValidateWasmImportSignature(const WasmImportDesc& desc, const_span<WasmScalarKind> args, const_span<WasmScalarKind> results) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

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

auto ResolveWasmScalarKind(string_view type_name) noexcept -> WasmScalarKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type_name == "i32") {
        return WasmScalarKind::I32;
    }
    if (type_name == "i64") {
        return WasmScalarKind::I64;
    }
    if (type_name == "f32") {
        return WasmScalarKind::F32;
    }
    if (type_name == "f64") {
        return WasmScalarKind::F64;
    }

    return WasmScalarKind::None;
}

auto WasmScalarKindToTypeName(WasmScalarKind kind) noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (kind) {
    case WasmScalarKind::I32:
        return "i32";
    case WasmScalarKind::I64:
        return "i64";
    case WasmScalarKind::F32:
        return "f32";
    case WasmScalarKind::F64:
        return "f64";
    default:
        return {};
    }
}

auto WasmScalarKindToEngineTypeName(WasmScalarKind kind) noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (kind) {
    case WasmScalarKind::I32:
        return "int32";
    case WasmScalarKind::I64:
        return "int64";
    case WasmScalarKind::F32:
        return "float32";
    case WasmScalarKind::F64:
        return "float64";
    default:
        return {};
    }
}

auto GetDefaultWasmRuntimeContext() noexcept -> const WasmRuntimeContext&
{
    FO_NO_STACK_TRACE_ENTRY();

    return DEFAULT_WASM_RUNTIME_CONTEXT;
}

auto MakeWasmRuntimeContext(const BaseEngine& engine, ScriptSystem* script_sys) -> WasmRuntimeContext
{
    FO_STACK_TRACE_ENTRY();

    bool time_synchronized = engine.GameTime.IsTimeSynchronized();

    return WasmRuntimeContext {
        .Side = numeric_cast<int32_t>(static_cast<uint8_t>(engine.GetSide())),
        .FrameTimeMs = engine.GameTime.GetFrameTime().milliseconds(),
        .FrameDeltaTimeMs = engine.GameTime.GetFrameDeltaTime().milliseconds(),
        .TimeSynchronized = time_synchronized ? 1 : 0,
        .SynchronizedTimeMs = time_synchronized ? engine.GameTime.GetSynchronizedTime().milliseconds() : 0,
        .ScriptSys = script_sys,
    };
}

void WasmLogI32(int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("[wasm:i32] {}", value);
}

void WasmLogI64(int64_t value)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("[wasm:i64] {}", value);
}

void WasmLogF32(float32_t value)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("[wasm:f32] {}", value);
}

void WasmLogF64(float64_t value)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("[wasm:f64] {}", value);
}

void WasmLogUtf8(const char* text, int32_t text_len)
{
    FO_STACK_TRACE_ENTRY();

    if (text == nullptr || text_len < 0) {
        WriteLog(LogType::Warning, "Invalid WASM UTF-8 log buffer");
        return;
    }

    WriteLog("[wasm:utf8] {}", string_view {text, numeric_cast<size_t>(text_len)});
}

auto WasmGetSide(const WasmRuntimeContext& context) noexcept -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return context.Side;
}

auto WasmGetFrameTimeMs(const WasmRuntimeContext& context) noexcept -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return context.FrameTimeMs;
}

auto WasmGetFrameDeltaTimeMs(const WasmRuntimeContext& context) noexcept -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return context.FrameDeltaTimeMs;
}

auto WasmIsTimeSynchronized(const WasmRuntimeContext& context) noexcept -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return context.TimeSynchronized;
}

auto WasmGetSynchronizedTimeMs(const WasmRuntimeContext& context) noexcept -> int64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return context.SynchronizedTimeMs;
}

auto WasmRetainCallback(const WasmRuntimeContext& context, const char* token, int32_t token_len) noexcept -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (token == nullptr || token_len < 0) {
        return 0;
    }

    string_view token_view {token, numeric_cast<size_t>(token_len)};

    if (token_view.empty()) {
        return 0;
    }
    if (!ScriptSystem::IsTemporaryScriptCallbackToken(token_view)) {
        return 1;
    }
    if (!context.ScriptSys) {
        return 0;
    }

    return context.ScriptSys.get_no_const()->RetainTemporaryScriptCallback(token_view) ? 1 : 0;
}

auto WasmReleaseCallback(const WasmRuntimeContext& context, const char* token, int32_t token_len) noexcept -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (token == nullptr || token_len < 0) {
        return 0;
    }

    string_view token_view {token, numeric_cast<size_t>(token_len)};

    if (token_view.empty()) {
        return 0;
    }
    if (!ScriptSystem::IsTemporaryScriptCallbackToken(token_view)) {
        return 1;
    }
    if (!context.ScriptSys) {
        return 0;
    }

    return context.ScriptSys.get_no_const()->ReleaseTemporaryScriptCallback(token_view) ? 1 : 0;
}

FO_END_NAMESPACE

#endif
