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

struct ManagedAbiRuntimeState;
struct ManagedBackendCaches;

class ManagedScriptBackend final : public ScriptSystemBackend
{
public:
    ManagedScriptBackend();
    ~ManagedScriptBackend() override;

    [[nodiscard]] auto GetDomain() const -> void* { return _domain.get_no_const(); }
    [[nodiscard]] auto GetMetadata() const noexcept -> nptr<EngineMetadata> { return _meta; }
    [[nodiscard]] auto GetGlobalEntity() const noexcept -> nptr<Entity>;
    [[nodiscard]] auto GetImages() const noexcept -> const vector<nptr<void>>& { return _images; }
    [[nodiscard]] auto GetAbi() const -> nptr<const ManagedAbiRuntimeState>;
    [[nodiscard]] auto GetAbi() -> nptr<ManagedAbiRuntimeState>;
    [[nodiscard]] auto GetCaches() const -> nptr<ManagedBackendCaches>;

    void RegisterMetadata(ptr<EngineMetadata> meta);
    void LoadAssemblies(const FileSystem& resources, string_view assembly_cache_dir, string_view bake_output_dir = {});
    auto LoadDynamicAssembly(ptr<void> image, nptr<void> symbols) -> ptr<void>;
    auto ReadClientScriptsImage() -> vector<uint8_t>;
    void BindRequiredStuff();
    void Process() override;
    void AddManagedGlobalFunc(unique_ptr<ScriptFuncDesc> desc);
    void AdoptPersistentGcHandle(uint32_t gc_handle);
    void BuildAbiTables();
    void AddInnerEntityVisits(uint64_t count);

private:
    auto CreateLoadScope(const std::filesystem::path& host_assembly_path, const vector<std::filesystem::path>& assembly_paths, const vector<std::filesystem::path>& entry_assembly_paths) -> vector<nptr<void>>;
    void ReleaseLoadScope() noexcept;
    void InvokeInitializator(void* assembly, const char* method_name);
    void UnbindBackend();
    void EnableDeepEntityWrapperTracking();
    void ClearScriptStatics() noexcept;
    void FinalizeManagedObjects() noexcept;

    nptr<EngineMetadata> _meta {};
    nptr<ScriptSystem> _scriptSys {};
    nptr<void> _domain {};
    nptr<void> _managedHostImage {};
    vector<nptr<void>> _images {};
    vector<nptr<void>> _continuationPumps {};
    vector<nptr<void>> _continuationShutdowns {};
    vector<nptr<void>> _backendUnbinds {};
    vector<unique_ptr<ScriptFuncDesc>> _globalFuncs {};
    vector<uint32_t> _persistentGcHandles {};
    uint32_t _loadScopeGcHandle {};
    unique_nptr<ManagedAbiRuntimeState> _abi {};
    unique_nptr<ManagedBackendCaches> _caches {};
};

FO_END_NAMESPACE

#endif
