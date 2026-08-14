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

#if FO_NATIVE_SCRIPTING

#include "EngineBase.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(NativeScriptSynthException);

// Native scripting code-emit helpers. Decoupled from BakingContext /
// FileCollection so they can be driven from the standalone
// build-time tool `LF_NativeScriptSynth`
// (Engine/Source/Applications/NativeScriptSynthApp.cpp), which runs
// at engine codegen time — before `NativeScripting` /
// `NativeScripts_<Role>` libs compile.
//
// These functions are the sole generator of the native scripting
// surface: codegen.py emits nothing for native scripting anymore.
// `Synthesize{NativeApiSurface,NativeApiContextRpcMethods,
// NativeApiModule}` are driven purely by `EngineMetadata`;
// `SynthesizeNativeBindings` takes the user-module list the tool
// builds by scanning the `.cppm` tree.
//
// Inputs are pure reads (EngineMetadata + a pre-scanned module
// list). Outputs are textual file bodies the caller writes to disk.

// Synthesize the union declaration surface embedded into each
// NativeApi.<Role> named-module purview.
[[nodiscard]] auto SynthesizeNativeApiSurface(const EngineMetadata& meta) -> string;

// Multi-bin variant. The per-target wrapper class bodies (methods,
// properties, events) get their metadata from the target-specific
// `EngineMetadata*` returned by `target_meta_lookup(target_name)`
// — `target_name` is one of "Common", "Server", "Client", "Mapper".
// When `target_meta_lookup` returns nullptr for a target, that
// section falls back to `meta`. The non-wrapper sections
// (forward decls, value type aliases, enum aliases, settings,
// fallback Detail) still come from `meta` (treated as the union /
// server bin).
//
// Use this to avoid cross-target type leakage — e.g. Client target's
// `Game` wrapper accidentally referencing `fo::StaticItem` from the
// server bin's metadata.
[[nodiscard]] auto SynthesizeNativeApiSurface(const EngineMetadata& meta, const function<const EngineMetadata*(string_view target_name)>& target_meta_lookup) -> string;

// Three-arg variant adding a per-(target,entity,member-name)
// allowlist callback. When `is_member_visible` returns false for a
// method, event, or property setter, the synth skips emitting the
// matching decl + body. Used to compute the Common target's
// wrapper surface from the INTERSECTION of Server+Client+Mapper
// member names (so server-only methods like `Game::GetStaticItem`
// don't leak into the Common wrapper which user code addresses
// from Common-target .cppm files).
//
// `is_member_visible` is consulted for every method, event, and
// property's setter-name during decl emission and the matching
// out-of-class body emission. Non-Common targets typically pass
// a callback that always returns true.
[[nodiscard]] auto SynthesizeNativeApiSurface(const EngineMetadata& meta, const function<const EngineMetadata*(string_view target_name)>& target_meta_lookup, const function<bool(string_view target_name, string_view entity, string_view member)>& is_member_visible) -> string;

// Single user-`.cppm`-derived init entry — one per
// `export void <Function>(const ModuleInitContext&)` the scanner
// found, paired with the module name from the file's
// `export module NativeScripts.User.<Role>.<Name>;` declaration.
// `SourceFileName` is recorded for the diagnostic comment in the
// emitted dispatcher; full paths are stripped to just the basename
// to keep output stable across build tree relocations.
struct NativeScriptModuleInit
{
    string Module;
    string Function;
    string SourceFileName;
};

// Synthesize `NativeBindings-<Target>.cpp` — the per-role
// dispatcher body that imports each user module and calls its
// init function. Emitted by `LF_NativeScriptSynth` (which scans the
// user `.cppm` tree to build the `modules` list). `target` is one
// of "Common", "Server", "Client", "Mapper", "Baker". An empty
// `modules` vector is legal — the dispatcher emits an empty body
// (`(void) ctx;`) so the `RegisterNativeScriptModules_<Target>`
// symbol still resolves at link time.
[[nodiscard]] auto SynthesizeNativeBindings(string_view target, const vector<NativeScriptModuleInit>& modules) -> string;

// Synthesize `NativeApi_ContextRpcMethods.h` — typed RemoteCall
// wrappers (`<Name>(caller, args...)` outbound + `Bind_<Name>(handler)`
// inbound) on `ModuleInitContext` for every native-origin RemoteCall
// (those tagged `SubsystemHint == "native"`).
[[nodiscard]] auto SynthesizeNativeApiContextRpcMethods(const EngineMetadata& meta) -> string;

// Synthesize `NativeApi.<Target>.cppm` — the complete per-target C++20
// module interface unit. `native_api_surface` is the in-memory union
// surface produced by SynthesizeNativeApiSurface.
//
// `target` is one of "Common", "Server", "Client", "Mapper", "Baker".
[[nodiscard]] auto SynthesizeNativeApiModule(string_view target, const EngineMetadata& meta, string_view native_api_surface) -> string;

FO_END_NAMESPACE

#endif // FO_NATIVE_SCRIPTING
