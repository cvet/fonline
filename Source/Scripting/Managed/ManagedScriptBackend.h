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

#if FO_MANAGED_SCRIPTING

#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

class ManagedScriptBackend final : public ScriptSystemBackend
{
public:
    ~ManagedScriptBackend() override;

    void RegisterMetadata(EngineMetadata* meta);
    void LoadAssemblies(const FileSystem& resources, string_view bake_output_dir = {});
    void BindRequiredStuff();
    // Take ownership of a managed global script func and register it into the cross-backend func map; the GC
    // handle keeps its handler delegate alive (freed at backend shutdown).
    void AddManagedGlobalFunc(unique_ptr<ScriptFuncDesc> desc, uint32_t gc_handle);
    // Keep a managed inbound remote-call handler delegate alive (its GC handle is freed at backend shutdown).
    // The handler itself lives in the engine's inbound-remote-call map; only its lifetime is tracked here.
    void AddRemoteCallHandlerGcHandle(uint32_t gc_handle) { _globalFuncGcHandles.emplace_back(gc_handle); }
    [[nodiscard]] auto GetDomain() const -> void* { return _domain; }
    [[nodiscard]] auto GetMetadata() const noexcept -> EngineMetadata* { return _meta.get_no_const(); }
    [[nodiscard]] auto GetGlobalEntity() const noexcept -> Entity*;
    [[nodiscard]] auto GetImages() const noexcept -> const vector<nptr<void>>& { return _images; }

private:
    static auto GetTargetName(EngineSideKind side) -> string_view;
    static auto SplitEnvList(const char* raw) -> vector<string>;
    static auto BuildAssemblyPathCandidates(const std::filesystem::path& base_dir, string_view target_name, string_view assembly_name) -> vector<std::filesystem::path>;
    static auto FindAssemblyPath(string_view target_name, string_view assembly_name) -> optional<std::filesystem::path>;

    void InvokeInitializator(void* assembly, const char* method_name);

    nptr<EngineMetadata> _meta {};
    nptr<ScriptSystem> _scriptSys {};
    void* _domain {};
    vector<nptr<void>> _images {};
    // Managed global script functions (registered under a marker attribute) so the cross-backend
    // `ScriptSystem::FindFunc` can find them on behalf of an out-of-engine consumer. The descs are owned here (the
    // global func map stores non-owning `raw_ptr`s); the GC handles keep each managed handler delegate alive for
    // the program lifetime.
    vector<unique_ptr<ScriptFuncDesc>> _globalFuncs {};
    // GC handles keeping managed handler delegates alive for the program lifetime: global script funcs (above)
    // and inbound remote-call handlers (whose handlers live in the engine's inbound-remote-call map).
    vector<uint32_t> _globalFuncGcHandles {};
};

FO_END_NAMESPACE

#endif
