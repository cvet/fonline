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

#include "Common.h"

#include "Application.h"
#include "Client.h"
#include "ClientRuntimeApi.h"
#include "MetadataRegistration.h"
#include "Settings.h"
#include "Updater.h"
#include "WebRelated.h"

#if !FO_TESTING_APP && !FO_HEADLESS_APP
#include "SDL3/SDL_main.h"
#endif

FO_USING_NAMESPACE();

struct ClientAppData
{
    refcount_ptr<ClientEngine> Client {};
    bool ResourcesSynced {};
    bool ReloadRequested {};
    string StagedRuntimePath;
    unique_ptr<Updater> ResourceUpdater {};
};
FO_GLOBAL_DATA(ClientAppData, Data);

struct RequestedClientRuntime
{
    string Path {};
    string CompatibilityVersion {};
    bool ExplicitPath {};
    bool ForceEmbedded {}; // ForceEmbeddedRuntime setting (command-line form): skip the implicit bundled-DLL load
    string PreviousBuildHash {}; // Set only on a reload pass: build hash of the module that requested it
};

struct HostClientRuntimeResult
{
    ClientRuntimeResult Result {};
    string RequestedRuntimePath {};
    string RequestedCompatibilityVersion {};
    string LoadedBuildHash {}; // Build hash of the runtime module that actually ran
};

static auto RunEmbeddedOrLoadedClient(int32_t argc, char** argv) -> bool;
static auto RunClientFromLibrary(int32_t argc, char** argv, const RequestedClientRuntime& requested_runtime) -> optional<HostClientRuntimeResult>;
static auto RunReloadedRuntime(int32_t argc, char** argv, const HostClientRuntimeResult& previous) -> optional<HostClientRuntimeResult>;
static auto RunEmbeddedClient(int32_t argc, char** argv) -> HostClientRuntimeResult;
static auto RunClientRuntime(int32_t argc, char** argv) noexcept -> ClientRuntimeResult;
static void MainEntry(void* data);
static void CleanupClientApp() noexcept;
static auto TryLoadRuntime(const RequestedClientRuntime& requested_runtime, ClientRuntimeExports& exports) -> void*;
static auto ApplyStagedBinaryUpdate() -> bool;
static auto ResolveRequestedClientRuntime(int32_t argc, char** argv) -> RequestedClientRuntime;
static auto ResolveBundledRuntimePath() -> string;
static void CaptureRuntimeResultStrings(HostClientRuntimeResult& runtime_result);

#if !FO_TESTING_APP
int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto ClientApp(int argc, char** argv) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

    const bool run_result = RunEmbeddedOrLoadedClient(numeric_cast<int32_t>(argc), argv);
    ExitApp(run_result);
}

static auto RunEmbeddedOrLoadedClient(int32_t argc, char** argv) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto requested_runtime = ResolveRequestedClientRuntime(argc, argv);

    // Try the bundled runtime DLL first on self-update platforms (the DLL is the authoritative
    // runtime); if it is absent or fails to load, RunClientFromLibrary falls back to the embedded
    // engine. ForceEmbeddedRuntime skips the implicit DLL load; an explicit --ClientLibPath still loads.
    const bool can_load_bundled_runtime = requested_runtime.ExplicitPath || (!requested_runtime.ForceEmbedded && CanSelfUpdateNativeModules(GetCurrentUpdatePlatform()));

    if (can_load_bundled_runtime) {
        const auto loaded = RunClientFromLibrary(argc, argv, requested_runtime);

        if (loaded.has_value()) {
            if (loaded->Result.ResultKind == ClientRuntimeResultKind::ReloadRequested) {
                const auto reloaded = RunReloadedRuntime(argc, argv, *loaded);
                return reloaded.has_value() && reloaded->Result.Success;
            }

            return loaded->Result.Success;
        }
    }

    if (requested_runtime.CompatibilityVersion != FO_COMPATIBILITY_VERSION) {
        return false;
    }

    const auto embedded = RunEmbeddedClient(argc, argv);

    if (embedded.Result.ResultKind == ClientRuntimeResultKind::ReloadRequested) {
        const auto reloaded = RunReloadedRuntime(argc, argv, embedded);
        return reloaded.has_value() && reloaded->Result.Success;
    }

    return embedded.Result.Success;
}

static auto RunClientFromLibrary(int32_t argc, char** argv, const RequestedClientRuntime& requested_runtime) -> optional<HostClientRuntimeResult>
{
    FO_STACK_TRACE_ENTRY();

    if (!ApplyStagedBinaryUpdate()) {
        return std::nullopt;
    }

    ClientRuntimeExports exports {};
    void* runtime_module = TryLoadRuntime(requested_runtime, exports);

    if (runtime_module == nullptr) {
        return std::nullopt;
    }

    const auto unload_runtime = scope_exit([&]() noexcept { Platform::UnloadModule(runtime_module); });

    string loaded_build_hash = exports.Metadata.BuildHash != nullptr ? string(exports.Metadata.BuildHash) : string();

    // A reload must map a different module than the one that asked for it. If LoadModule returned a
    // still-resident copy of the previous runtime (same live path, not fully unloaded by the OS),
    // running it would re-enter InitApp in an already-initialized module and trip its once-guard with
    // an opaque StrongAssertionException. Abort the reload cleanly; the next launch maps the swapped
    // module from a fresh process.
    if (!requested_runtime.PreviousBuildHash.empty() && requested_runtime.PreviousBuildHash == loaded_build_hash) {
        WriteLog("Runtime reload loaded an unchanged module (build hash '{}'); aborting reload to avoid double initialization", loaded_build_hash);
        return std::nullopt;
    }

    HostClientRuntimeResult runtime_result {};
    runtime_result.Result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
    runtime_result.LoadedBuildHash = loaded_build_hash;

    exports.Run(argc, argv, &runtime_result.Result);
    CaptureRuntimeResultStrings(runtime_result);

    if (!IsValidClientRuntimeResult(runtime_result.Result)) {
        return std::nullopt;
    }

    return runtime_result;
}

static auto RunReloadedRuntime(int32_t argc, char** argv, const HostClientRuntimeResult& previous) -> optional<HostClientRuntimeResult>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!previous.RequestedRuntimePath.empty());

    RequestedClientRuntime reloaded_runtime {};
    reloaded_runtime.Path = previous.RequestedRuntimePath;
    reloaded_runtime.CompatibilityVersion = previous.RequestedCompatibilityVersion;
    reloaded_runtime.ExplicitPath = true;
    reloaded_runtime.PreviousBuildHash = previous.LoadedBuildHash;

    return RunClientFromLibrary(argc, argv, reloaded_runtime);
}

static auto RunEmbeddedClient(int32_t argc, char** argv) -> HostClientRuntimeResult
{
    FO_STACK_TRACE_ENTRY();

    HostClientRuntimeResult runtime_result {};
    runtime_result.LoadedBuildHash = FO_BUILD_HASH;
    runtime_result.Result = RunClientRuntime(argc, argv);
    CaptureRuntimeResultStrings(runtime_result);
    return runtime_result;
}

static auto RunClientRuntime(int32_t argc, char** argv) noexcept -> ClientRuntimeResult
{
    FO_STACK_TRACE_ENTRY();

    ClientRuntimeResult runtime_result {};
    runtime_result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
    runtime_result.ResultKind = ClientRuntimeResultKind::Shutdown;

    try {
        InitApp(argc, argv, CombineEnum(AppInitFlags::ClientMode, AppInitFlags::ShowMessageOnException, AppInitFlags::PrebakeResources));
        WriteLog("Compatibility version: {}", App->Settings.CompatibilityVersion);

#if FO_IOS
        MainEntry(nullptr);
        App->SetMainLoopCallback(MainEntry);

#elif FO_WEB
        WebRelated::InitializePersistentData();
        WebRelated::StartMainLoop(MainEntry, nullptr);

#elif FO_ANDROID
        while (!App->IsQuitRequested()) {
            MainEntry(nullptr);
        }

#else
        auto balancer = FrameBalancer(!App->Settings.VSync, App->Settings.Sleep, App->Settings.FixedFPS);

        while (!App->IsQuitRequested()) {
            balancer.StartLoop();
            MainEntry(nullptr);
            balancer.EndLoop();
        }
#endif

        WriteLog("Exit from game");

        const bool quit_success = App->GetRequestedQuitSuccess();
        CleanupClientApp();

        if (Data->ReloadRequested) {
            runtime_result.ResultKind = ClientRuntimeResultKind::ReloadRequested;
            runtime_result.RequestedRuntimePath = Data->StagedRuntimePath.c_str();
            runtime_result.Success = true;
        }
        else {
            runtime_result.Success = quit_success;
        }

        if (Data->ReloadRequested) {
            App.reset();
        }
    }
    catch (const std::exception& ex) {
        CleanupClientApp();
        ReportExceptionAndContinue(ex);

        runtime_result.ResultKind = ClientRuntimeResultKind::FatalError;
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }

    return runtime_result;
}

static void MainEntry([[maybe_unused]] void* data)
{
    FO_STACK_TRACE_ENTRY();

    if (App->IsQuitRequested()) {
        return;
    }

    try {
        if (!WebRelated::IsPersistentDataReady()) {
            return;
        }

        try {
            App->BeginFrame();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndExit(ex);
        }

        auto end_frame = scope_success([&]() {
            try {
                App->EndFrame();
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        });

        if (!Data->Client) {
            try {
                if (!Data->ResourcesSynced) {
                    if (!IsPackaged()) {
                        Data->ResourcesSynced = true;
                        return;
                    }

                    if (!Data->ResourceUpdater) {
                        Data->ResourceUpdater = SafeAlloc::MakeUnique<Updater>(App->Settings, App->MainWindow);
                    }

                    if (!Data->ResourceUpdater->Process()) {
                        return;
                    }

                    const auto result = Data->ResourceUpdater->GetResult();
                    Data->ResourceUpdater.reset();

                    switch (result) {
                    case UpdaterResult::ResourcesReady:
                        Data->ResourcesSynced = true;
                        break;
                    case UpdaterResult::BinariesStaged:
                        Data->StagedRuntimePath = GetClientRuntimeLivePath();
                        Data->ReloadRequested = true;
                        App->RequestQuit();
                        return;
                    default:
                        ShowUpdaterFailure(result);
                        App->RequestQuit();
                        return;
                    }
                }

                Data->Client = SafeAlloc::MakeRefCounted<ClientEngine>(App->Settings, GetClientResources(App->Settings), App->MainWindow);
#if FO_HEADLESS_APP
                Data->Client->Connect();
#endif
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }

        try {
            Data->Client->MainLoop();
        }
        catch (const ResourcesOutdatedException&) {
            Data->ResourcesSynced = false;
            Data->Client->Shutdown();
            Data->Client.reset();
        }
        catch (const MetadataNotFoundException& ex) {
            ReportExceptionAndExit(ex);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);

            if (App->Settings.RecreateClientOnError) {
                Data->Client->Shutdown();
                Data->Client.reset();
            }
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}

static void CleanupClientApp() noexcept
{
    FO_STACK_TRACE_ENTRY();

    Data->ResourceUpdater.reset();

    if (Data->Client) {
        safe_call([]() { Data->Client->Shutdown(); });
        Data->Client.reset();
    }
}

static auto TryLoadRuntime(const RequestedClientRuntime& requested_runtime, ClientRuntimeExports& exports) -> void*
{
    FO_STACK_TRACE_ENTRY();

    void* runtime_module = Platform::LoadModule(requested_runtime.Path);

    if (runtime_module == nullptr) {
        return nullptr;
    }

    const auto query_exports = Platform::GetFuncAddr<QueryClientRuntimeExportsFunc>(runtime_module, "FO_QueryClientRuntimeExports");

    if (query_exports == nullptr) {
        Platform::UnloadModule(runtime_module);
        runtime_module = nullptr;
        return nullptr;
    }

    exports = {};
    exports.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeExports));

    if (!query_exports(FO_CLIENT_RUNTIME_HOST_ABI_VERSION, &exports) || !IsValidClientRuntimeExports(exports) || !IsSupportedClientRuntimeAbi(exports.Metadata.HostAbiVersion)) {
        Platform::UnloadModule(runtime_module);
        runtime_module = nullptr;
        return nullptr;
    }

    if (!requested_runtime.CompatibilityVersion.empty() && !IsClientRuntimeCompatibilityMatch(exports.Metadata, requested_runtime.CompatibilityVersion)) {
        Platform::UnloadModule(runtime_module);
        runtime_module = nullptr;
        return nullptr;
    }

    return runtime_module;
}

static auto ApplyStagedBinaryUpdate() -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto staged_path = GetClientRuntimeStagingPath();

    if (!fs_exists(staged_path)) {
        PromoteStagedRuntimeCompanions();
        return true;
    }

    const auto final_path = GetClientRuntimeLivePath();
    const auto backup_path = strex("{}.bak", final_path).str();
    const auto final_exists = fs_exists(final_path);

    fs_remove_file(backup_path);

    if (final_exists && !fs_rename(final_path, backup_path)) {
        return false;
    }

    if (!fs_rename(staged_path, final_path)) {
        if (final_exists) {
            fs_rename(backup_path, final_path);
        }

        return false;
    }

    if (final_exists) {
        fs_remove_file(backup_path);
    }

    PromoteStagedRuntimeCompanions();

    return true;
}

static auto ResolveRequestedClientRuntime(int32_t argc, char** argv) -> RequestedClientRuntime
{
    FO_STACK_TRACE_ENTRY();

    RequestedClientRuntime requested_runtime {};
    requested_runtime.Path = ResolveBundledRuntimePath();
    requested_runtime.CompatibilityVersion = FO_COMPATIBILITY_VERSION;

    for (int32_t index = 1; index < argc; index++) {
        const string_view arg = argv[index];
        const bool has_value = index + 1 < argc;

        if (arg == "--ClientLibPath" && has_value) {
            requested_runtime.ExplicitPath = true;
            requested_runtime.Path = argv[index + 1];
        }

        if (arg == "--ClientLibCompatibilityVersion" && has_value) {
            requested_runtime.CompatibilityVersion = argv[index + 1];
        }

        if (arg == "--ForceEmbeddedRuntime" || arg == "-ForceEmbeddedRuntime") {
            const string_view value = has_value && argv[index + 1][0] != '-' ? string_view(argv[index + 1]) : string_view("1");
            requested_runtime.ForceEmbedded = value != "0" && value != "false" && value != "False";
        }
    }

    return requested_runtime;
}

static auto ResolveBundledRuntimePath() -> string
{
    FO_STACK_TRACE_ENTRY();

    return GetClientRuntimeLivePath();
}

static void CaptureRuntimeResultStrings(HostClientRuntimeResult& runtime_result)
{
    FO_STACK_TRACE_ENTRY();

    if (runtime_result.Result.RequestedRuntimePath != nullptr) {
        runtime_result.RequestedRuntimePath = runtime_result.Result.RequestedRuntimePath;
        runtime_result.Result.RequestedRuntimePath = runtime_result.RequestedRuntimePath.c_str();
    }
    else {
        runtime_result.RequestedRuntimePath.clear();
    }

    if (runtime_result.Result.RequestedCompatibilityVersion != nullptr) {
        runtime_result.RequestedCompatibilityVersion = runtime_result.Result.RequestedCompatibilityVersion;
        runtime_result.Result.RequestedCompatibilityVersion = runtime_result.RequestedCompatibilityVersion.c_str();
    }
    else {
        runtime_result.RequestedCompatibilityVersion.clear();
    }
}
