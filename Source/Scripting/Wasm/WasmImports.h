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

#pragma once

#include "Common.h"

#if FO_WASM_SCRIPTING

FO_BEGIN_NAMESPACE

class BaseEngine;
class ScriptSystem;

enum class WasmScalarKind : int32_t
{
    None = 0,
    I32 = 1,
    I64 = 2,
    F32 = 3,
    F64 = 4,
};

struct WasmImportDesc
{
    string_view Module {};
    string_view Name {};
    string_view NativeSignature {};
    array<WasmScalarKind, MAX_CALL_ARGS> Args {};
    size_t ArgsCount {};
    WasmScalarKind Ret {};
};

struct WasmRuntimeContext
{
    int32_t Side {};
    int64_t FrameTimeMs {};
    int64_t FrameDeltaTimeMs {};
    int32_t TimeSynchronized {};
    int64_t SynchronizedTimeMs {};
    nptr<ScriptSystem> ScriptSys {};
};

[[nodiscard]] auto GetWasmImportDescs() noexcept -> const_span<WasmImportDesc>;
[[nodiscard]] auto FindWasmImportDesc(string_view module_name, string_view import_name) noexcept -> const WasmImportDesc*;
[[nodiscard]] auto ValidateWasmImportSignature(const WasmImportDesc& desc, const_span<WasmScalarKind> args, const_span<WasmScalarKind> results) noexcept -> bool;
[[nodiscard]] auto ResolveWasmScalarKind(string_view type_name) noexcept -> WasmScalarKind;
[[nodiscard]] auto WasmScalarKindToTypeName(WasmScalarKind kind) noexcept -> string_view;
[[nodiscard]] auto WasmScalarKindToEngineTypeName(WasmScalarKind kind) noexcept -> string_view;
[[nodiscard]] auto GetDefaultWasmRuntimeContext() noexcept -> const WasmRuntimeContext&;
[[nodiscard]] auto MakeWasmRuntimeContext(const BaseEngine& engine, ScriptSystem* script_sys = nullptr) -> WasmRuntimeContext;

void WasmLogI32(int32_t value);
void WasmLogI64(int64_t value);
void WasmLogF32(float32_t value);
void WasmLogF64(float64_t value);
void WasmLogUtf8(const char* text, int32_t text_len);
[[nodiscard]] auto WasmGetSide(const WasmRuntimeContext& context) noexcept -> int32_t;
[[nodiscard]] auto WasmGetFrameTimeMs(const WasmRuntimeContext& context) noexcept -> int64_t;
[[nodiscard]] auto WasmGetFrameDeltaTimeMs(const WasmRuntimeContext& context) noexcept -> int64_t;
[[nodiscard]] auto WasmIsTimeSynchronized(const WasmRuntimeContext& context) noexcept -> int32_t;
[[nodiscard]] auto WasmGetSynchronizedTimeMs(const WasmRuntimeContext& context) noexcept -> int64_t;
[[nodiscard]] auto WasmRetainCallback(const WasmRuntimeContext& context, const char* token, int32_t token_len) noexcept -> int32_t;
[[nodiscard]] auto WasmReleaseCallback(const WasmRuntimeContext& context, const char* token, int32_t token_len) noexcept -> int32_t;

FO_END_NAMESPACE

#endif
