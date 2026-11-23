// FOnline Engine
// SPDX-License-Identifier: MIT

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
    [[nodiscard]] auto GetSnapshotWorkCount() const -> uint64_t;

    void RegisterMetadata(ptr<EngineMetadata> meta);
    void LoadAssemblies(const FileSystem& resources, string_view assembly_cache_dir, string_view bake_output_dir = {});
    auto LoadDynamicAssembly(ptr<void> image, nptr<void> symbols) -> ptr<void>;
    auto ReadClientScriptsImage() -> vector<uint8_t>;
    void BindRequiredStuff();
    void Process() override;
    void SignalContinuationsReady();
    auto RegisterCallCompletion(function<void(bool)>& completion) -> uint64_t;
    void CompleteCall(uint64_t id, bool success);
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
    void BeginManagedTeardown() noexcept;
    void ClearScriptStatics() noexcept;
    void FinalizeManagedObjects() noexcept;

    nptr<EngineMetadata> _meta {};
    nptr<ScriptSystem> _scriptSys {};
    nptr<void> _domain {};
    nptr<void> _managedHostImage {};
    vector<nptr<void>> _images {};
    vector<nptr<void>> _continuationPumps {};
    vector<nptr<void>> _continuationShutdowns {};
    vector<nptr<void>> _snapshotWorkCounters {};
    vector<nptr<void>> _refTypePumps {};
    vector<nptr<void>> _refTypeShutdowns {};
    std::atomic<bool> _continuationsReady {};
    mutex _callCompletionsLocker {};
    unordered_map<uint64_t, function<void(bool)>> _callCompletions FO_TSA_GUARDED_BY(_callCompletionsLocker) {};
    uint64_t _callCompletionCounter FO_TSA_GUARDED_BY(_callCompletionsLocker) {};
    vector<nptr<void>> _backendUnbinds {};
    vector<unique_ptr<ScriptFuncDesc>> _globalFuncs {};
    vector<uint32_t> _persistentGcHandles {};
    uint32_t _loadScopeGcHandle {};
    unique_nptr<ManagedAbiRuntimeState> _abi {};
    unique_nptr<ManagedBackendCaches> _caches {};
};

FO_END_NAMESPACE

#endif
