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

FO_BEGIN_NAMESPACE
extern void ApplicationShutdownHook();
extern void ClientStartupSettingsHook(GlobalSettings& settings, int32_t client_index, bool embedded);
FO_END_NAMESPACE

struct ClientAppData
{
    refcount_nptr<ClientEngine> Client {};
    bool ResourcesSynced {};
    bool ReloadRequested {};
    u8string StagedRuntimePath;
    optional<Updater> ResourceUpdater {};
};
FO_GLOBAL_DATA(ClientAppData, Data);

struct RequestedClientRuntime
{
    u8string Path {};
    string CompatibilityVersion {};
    bool CheckCompatibilityVersion {};
    bool ExplicitPath {};
    bool ForceEmbedded {}; // ForceEmbeddedRuntime setting (command-line form): skip the implicit bundled-DLL load
};

static auto RunEmbeddedOrLoadedClient(CommandLineArgs args) -> bool;
static auto RunClientFromLibrary(CommandLineArgs args, const RequestedClientRuntime& requested_runtime) -> optional<ClientRuntimeHostResult>;
static auto PromoteStagedReloadForRestart(u8string_view runtime_path) -> bool;
static auto RunEmbeddedClient(CommandLineArgs args) -> ClientRuntimeHostResult;
static auto RunClientRuntime(CommandLineArgs args) noexcept -> ClientRuntimeResult;
static void MainEntry(void* data);
static void CleanupClientApp() noexcept;
static auto TryLoadRuntime(const RequestedClientRuntime& requested_runtime, ClientRuntimeExports& exports) -> nptr<void>;
static auto ApplyStagedBinaryUpdate(u8string_view runtime_live_path) -> bool;
static auto ResolveRequestedClientRuntime(CommandLineArgs args) -> RequestedClientRuntime;
static auto ResolveBundledRuntimePath() -> u8string;
static auto IsInstalledClientLayout() -> bool;
static auto GetInstalledClientRuntimeBootstrapPath() -> optional<u8string>;
static auto GetCurrentClientRuntimeFileName() -> u8string;
static void CaptureRuntimeResultStrings(ClientRuntimeHostResult& runtime_result);

#if !FO_TESTING_APP
int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto ClientApp(CommandLineArgs args) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

    CreateGlobalData();
    LogToFile(GetExeLogFileName(), false);

#if !FO_TESTING_APP
    CommandLineArgs args {numeric_cast<int32_t>(argc), argv};
#endif
    bool run_result = RunEmbeddedOrLoadedClient(args);

    ExitApp(run_result);
}

static auto RunEmbeddedOrLoadedClient(CommandLineArgs args) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto requested_runtime = ResolveRequestedClientRuntime(args);
    bool can_self_update = CanSelfUpdateNativeModules(GetCurrentUpdatePlatform());

    const string_view compatibility_check = requested_runtime.CheckCompatibilityVersion ? "enabled" : "disabled";
    const string_view explicit_path = requested_runtime.ExplicitPath ? "yes" : "no";
    const string_view force_embedded = requested_runtime.ForceEmbedded ? "yes" : "no";
    const string_view self_update = can_self_update ? "enabled" : "disabled";
    WriteLog("Client runtime host: bundled DLL {}, compatibility check {}, explicit path {}, force embedded {}, native self-update {} for {}, embedded build {}, embedded compatibility {}", requested_runtime.Path, compatibility_check, explicit_path, force_embedded, self_update, GetCurrentBinaryUpdateTargetName(), FO_BUILD_HASH, FO_COMPATIBILITY_VERSION);

    // Try the bundled runtime DLL first on self-update platforms (the DLL is the authoritative
    // runtime); if it is absent or fails to load, RunClientFromLibrary falls back to the embedded
    // engine. ForceEmbeddedRuntime skips the implicit DLL load; an explicit --ClientLibPath still loads.
    bool can_load_bundled_runtime = requested_runtime.ExplicitPath || (!requested_runtime.ForceEmbedded && can_self_update);

    if (can_load_bundled_runtime) {
        auto loaded_runtime_result = RunClientFromLibrary(args, requested_runtime);
        auto loaded_result = RunClientRuntimeHostPass(loaded_runtime_result, PromoteStagedReloadForRestart);

        if (loaded_result.has_value()) {
            return loaded_result.value();
        }

        WriteLog("Client runtime host: bundled DLL did not start, trying embedded fallback");
    }
    else {
        WriteLog("Client runtime host: bundled DLL load skipped");
    }

    if (requested_runtime.CheckCompatibilityVersion && requested_runtime.CompatibilityVersion != FO_COMPATIBILITY_VERSION) {
        WriteLog("Client runtime host: embedded fallback rejected, requested compatibility {}, embedded compatibility {}", requested_runtime.CompatibilityVersion, FO_COMPATIBILITY_VERSION);
        return false;
    }

    auto embedded_runtime_result = RunEmbeddedClient(args);
    auto embedded_result = RunClientRuntimeHostPass(embedded_runtime_result, PromoteStagedReloadForRestart);

    FO_VERIFY_AND_THROW(embedded_result.has_value(), "Embedded client runtime pass did not return a result");
    return embedded_result.value();
}

static auto RunClientFromLibrary(CommandLineArgs args, const RequestedClientRuntime& requested_runtime) -> optional<ClientRuntimeHostResult>
{
    FO_STACK_TRACE_ENTRY();

    const string_view compatibility_check = requested_runtime.CheckCompatibilityVersion ? "enabled" : "disabled";
    WriteLog("Client runtime host: preparing DLL {}, compatibility check {}", requested_runtime.Path, compatibility_check);

    if (!ApplyStagedBinaryUpdate(requested_runtime.Path)) {
        WriteLog("Client runtime host: failed to apply staged binary update before loading {}", requested_runtime.Path);
        return std::nullopt;
    }

    if (!fs_exists(requested_runtime.Path)) {
        WriteLog("Client runtime host: no bundled runtime DLL at {}, using embedded runtime", requested_runtime.Path);
        return std::nullopt;
    }

    ClientRuntimeExports exports {};
    auto runtime_module = TryLoadRuntime(requested_runtime, exports);

    if (!runtime_module) {
        WriteLog("Client runtime host: failed to load DLL {}", requested_runtime.Path);
        return std::nullopt;
    }

    const string loaded_runtime_name = exports.Metadata.RuntimeName != nullptr ? string(string_view {exports.Metadata.RuntimeName}) : string {};
    const string loaded_build_hash = exports.Metadata.BuildHash != nullptr ? string(string_view {exports.Metadata.BuildHash}) : string {};
    const string loaded_compat = exports.Metadata.CompatibilityVersion != nullptr ? string(string_view {exports.Metadata.CompatibilityVersion}) : string {};

    WriteLog("Client runtime host: loaded DLL {}, runtime {}, build {}, compatibility {}, ABI {}", requested_runtime.Path, loaded_runtime_name, loaded_build_hash, loaded_compat, exports.Metadata.HostAbiVersion);

    auto unload_runtime = scope_exit([&]() noexcept {
        WriteLog("Client runtime host: unloading DLL {}, runtime {}, build {}, compatibility {}", requested_runtime.Path, loaded_runtime_name, loaded_build_hash, loaded_compat);
        Platform::UnloadModule(runtime_module);
        WriteLog("Client runtime host: unloaded DLL {}", requested_runtime.Path);
    });

    ClientRuntimeHostResult runtime_result {};
    runtime_result.Result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
    runtime_result.LoadedBuildHash = loaded_build_hash;
    vector<string> runtime_arg_storage(args.size());
    vector<char*> runtime_args(args.size());

    for (size_t index = 0; index < args.size(); ++index) {
        if (!args[index].empty()) {
            runtime_arg_storage[index] = utf8_to_char_string(args[index]);
        }
    }

    for (size_t index = 0; index < args.size(); ++index) {
        runtime_args[index] = runtime_arg_storage[index].data();
    }

    WriteLog("Client runtime host: entering DLL {}, runtime {}, build {}", requested_runtime.Path, loaded_runtime_name, loaded_build_hash);
    exports.Run(numeric_cast<int32_t>(args.size()), runtime_args.data(), &runtime_result.Result);
    CaptureRuntimeResultStrings(runtime_result);
    const string_view runtime_status = runtime_result.Result.Success ? "yes" : "no";
    WriteLog("Client runtime host: DLL {} returned {}, success {}, requested path {}, requested compatibility {}", requested_runtime.Path, ClientRuntimeResultKindToString(runtime_result.Result.ResultKind), runtime_status, runtime_result.RequestedRuntimePath, runtime_result.RequestedCompatibilityVersion);

    if (!IsValidClientRuntimeResult(runtime_result.Result)) {
        WriteLog("Client runtime host: DLL {} returned invalid result {}, success {}, requested path {}", requested_runtime.Path, ClientRuntimeResultKindToString(runtime_result.Result.ResultKind), runtime_status, runtime_result.RequestedRuntimePath);
        return std::nullopt;
    }

    return runtime_result;
}

static auto PromoteStagedReloadForRestart(u8string_view runtime_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!runtime_path.empty(), "Client runtime host received a reload result without a requested runtime path");

    if (!ApplyStagedBinaryUpdate(runtime_path)) {
        WriteLog("Client runtime host: failed to promote staged runtime at {}", runtime_path);
        return false;
    }

    if (IsInstalledClientLayout()) {
        auto bootstrap_path = GetInstalledClientRuntimeBootstrapPath();

        if (!bootstrap_path.has_value()) {
            WriteLog("Client runtime host: failed to resolve installed runtime bootstrap path for {}", runtime_path);
            return false;
        }

        const u8string runtime_file_name = GetCurrentClientRuntimeFileName();

        if (!WriteClientRuntimeBootstrapTarget(*bootstrap_path, runtime_path, runtime_file_name)) {
            WriteLog("Client runtime host: failed to persist installed runtime bootstrap {} -> {}", *bootstrap_path, runtime_path);
            return false;
        }

        WriteLog("Client runtime host: persisted installed runtime bootstrap {} -> {}", *bootstrap_path, runtime_path);
    }

    WriteLog("Client runtime host: staged self-update promoted at {}, exiting for user restart", runtime_path);
    return true;
}

static auto RunEmbeddedClient(CommandLineArgs args) -> ClientRuntimeHostResult
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Client runtime host: entering embedded client build {}, compatibility {}", FO_BUILD_HASH, FO_COMPATIBILITY_VERSION);

    ClientRuntimeHostResult runtime_result {};
    runtime_result.LoadedBuildHash = string {FO_BUILD_HASH};
    runtime_result.Result = RunClientRuntime(args);
    CaptureRuntimeResultStrings(runtime_result);
    const string_view runtime_status = runtime_result.Result.Success ? "yes" : "no";
    WriteLog("Client runtime host: embedded client returned {}, success {}, requested path {}, requested compatibility {}", ClientRuntimeResultKindToString(runtime_result.Result.ResultKind), runtime_status, runtime_result.RequestedRuntimePath, runtime_result.RequestedCompatibilityVersion);
    return runtime_result;
}

static auto RunClientRuntime(CommandLineArgs args) noexcept -> ClientRuntimeResult
{
    FO_STACK_TRACE_ENTRY();

    ClientRuntimeResult runtime_result {};
    runtime_result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
    runtime_result.ResultKind = ClientRuntimeResultKind::Shutdown;

    try {
        WriteLog("Client runtime embedded: starting InitApp, build {}, compatibility {}", FO_BUILD_HASH, FO_COMPATIBILITY_VERSION);
        InitApp(args, CombineEnum(AppInitFlags::ClientMode, AppInitFlags::ShowMessageOnException, AppInitFlags::PrebakeResources, AppInitFlags::AppendLogFile));
        WriteLog("Compatibility version: {}", GetApp()->Settings.CompatibilityVersion);
        WriteLog("Client runtime: embedded client build {}, compatibility {}", FO_BUILD_HASH, FO_COMPATIBILITY_VERSION);

#if FO_IOS
        MainEntry(nullptr);
        GetApp()->SetMainLoopCallback(MainEntry);

#elif FO_WEB
        WebRelated::InitializePersistentData();
        WebRelated::StartMainLoop(MainEntry, nullptr);

#elif FO_ANDROID
        while (!GetApp()->IsQuitRequested()) {
            MainEntry(nullptr);
        }

#else
        auto balancer = FrameBalancer(!GetApp()->Settings.VSync, GetApp()->Settings.Sleep, GetApp()->Settings.FixedFPS);

        while (!GetApp()->IsQuitRequested()) {
            balancer.StartLoop();
            MainEntry(nullptr);
            balancer.EndLoop();
        }
#endif

        WriteLog("Exit from game");
        WriteLog("Client runtime embedded: main loop exited");

        bool quit_success = GetApp()->GetRequestedQuitSuccess();
        CleanupClientApp();

        if (Data->ReloadRequested) {
            WriteLog("Client runtime embedded: requesting reload from {}", Data->StagedRuntimePath);
            runtime_result.ResultKind = ClientRuntimeResultKind::ReloadRequested;
            runtime_result.RequestedRuntimePath = utf8_to_c_str(Data->StagedRuntimePath.view_nt()).get();
            runtime_result.Success = true;
        }
        else {
            const string_view quit_status = quit_success ? "yes" : "no";
            WriteLog("Client runtime embedded: returning shutdown, success {}", quit_status);
            runtime_result.Success = quit_success;
        }

        if (Data->ReloadRequested) {
            WriteLog("Client runtime embedded: resetting application before reload");
            ResetApp();
        }
    }
    catch (const std::exception& ex) {
        WriteLog("Client runtime embedded: exception {}", ex.what());
        CleanupClientApp();
        ReportExceptionAndContinue(ex);

        runtime_result.ResultKind = ClientRuntimeResultKind::FatalError;
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }

    WriteLog("Client runtime embedded: calling application shutdown hook");
    safe_call([] { ApplicationShutdownHook(); });
    const string_view runtime_status = runtime_result.Success ? "yes" : "no";
    WriteLog("Client runtime embedded: finished with {}, success {}", ClientRuntimeResultKindToString(runtime_result.ResultKind), runtime_status);

    return runtime_result;
}

static auto GetClient() -> ptr<ClientEngine>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(Data->Client, "Client engine is not created");
    return Data->Client;
}

static void MainEntry([[maybe_unused]] void* data)
{
    FO_STACK_TRACE_ENTRY();

    if (GetApp()->IsQuitRequested()) {
        return;
    }

    try {
        if (!WebRelated::IsPersistentDataReady()) {
            return;
        }

        try {
            GetApp()->BeginFrame();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndExit(ex);
        }

        auto end_frame = scope_success([&]() {
            try {
                GetApp()->EndFrame();
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
                        WriteLog("Client runtime embedded: creating updater");
                        Data->ResourceUpdater.emplace(&GetApp()->Settings, &GetApp()->MainWindow);
                    }

                    if (!Data->ResourceUpdater->Process()) {
                        return;
                    }

                    auto result = Data->ResourceUpdater->GetResult();
                    // The updater stages the new runtime under its own binary dir (the writable root
                    // for an installed client, the exe dir for a portable one); reload that exact path.
                    u8string staged_runtime_path = Data->ResourceUpdater->GetRuntimeLivePath();
                    Data->ResourceUpdater.reset();

                    switch (result) {
                    case UpdaterResult::ResourcesReady:
                        WriteLog("Client runtime embedded: updater finished, resources ready");
                        Data->ResourcesSynced = true;
                        break;
                    case UpdaterResult::BinariesStaged:
                        Data->StagedRuntimePath = staged_runtime_path;
                        Data->ReloadRequested = true;
                        WriteLog("Client runtime embedded: updater staged binaries at {}", Data->StagedRuntimePath);
                        GetApp()->RequestQuit();
                        return;
                    default:
                        WriteLog("Client runtime embedded: updater failed");
                        ShowUpdaterFailure(result);
                        GetApp()->RequestQuit();
                        return;
                    }
                }

                ClientStartupSettingsHook(GetApp()->Settings, 1, false);
                auto settings = make_ptr(&GetApp()->Settings);
                Data->Client = SafeAlloc::MakeRefCounted<ClientEngine>(settings, GetClientResources(*settings), &GetApp()->MainWindow);
#if FO_HEADLESS_APP
                auto client = GetClient();
                client->Connect();
#endif
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }

        try {
            auto client = GetClient();
            client->MainLoop();
        }
        catch (const ResourcesOutdatedException&) {
            Data->ResourcesSynced = false;
            auto client = GetClient();
            client->Shutdown();
            Data->Client.reset();
        }
        catch (const MetadataNotFoundException& ex) {
            ReportExceptionAndExit(ex);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);

            if (GetApp()->Settings.RecreateClientOnError) {
                auto client = GetClient();
                client->Shutdown();
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
        safe_call([]() {
            auto client = GetClient();
            client->Shutdown();
        });
        Data->Client.reset();
    }
}

static auto TryLoadRuntime(const RequestedClientRuntime& requested_runtime, ClientRuntimeExports& exports) -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Client runtime host: loading DLL {}", requested_runtime.Path);
    auto runtime_module = Platform::LoadModule(requested_runtime.Path.view_nt());

    if (!runtime_module) {
        WriteLog("Client runtime host: LoadModule failed for {}", requested_runtime.Path);
        return nullptr;
    }

    const auto query_exports = Platform::GetFuncAddr<QueryClientRuntimeExportsFunc>(runtime_module, string_view_nt {"FO_QueryClientRuntimeExports"});

    if (query_exports == nullptr) {
        WriteLog("Client runtime host: DLL {} does not export FO_QueryClientRuntimeExports", requested_runtime.Path);
        Platform::UnloadModule(runtime_module);
        return nullptr;
    }

    exports = {};
    exports.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeExports));

    bool query_ok = query_exports(FO_CLIENT_RUNTIME_HOST_ABI_VERSION, &exports);
    bool exports_valid = query_ok && IsValidClientRuntimeExports(exports);
    bool abi_supported = exports_valid && IsSupportedClientRuntimeAbi(exports.Metadata.HostAbiVersion);

    if (!query_ok || !exports_valid || !abi_supported) {
        const string_view query_status = query_ok ? "ok" : "failed";
        const string_view metadata_status = exports_valid ? "valid" : "invalid";
        const string_view abi_status = abi_supported ? "supported" : "unsupported";
        WriteLog("Client runtime host: DLL {} rejected, export query {}, metadata {}, ABI {}, runtime ABI {}, host ABI {}", requested_runtime.Path, query_status, metadata_status, abi_status, exports.Metadata.HostAbiVersion, FO_CLIENT_RUNTIME_HOST_ABI_VERSION);
        Platform::UnloadModule(runtime_module);
        return nullptr;
    }

    if (requested_runtime.CheckCompatibilityVersion && !IsClientRuntimeCompatibilityMatch(exports.Metadata, requested_runtime.CompatibilityVersion)) {
        const string metadata_compat = exports.Metadata.CompatibilityVersion != nullptr ? string(string_view {exports.Metadata.CompatibilityVersion}) : string {};
        const string metadata_build = exports.Metadata.BuildHash != nullptr ? string(string_view {exports.Metadata.BuildHash}) : string {};
        WriteLog("Client runtime host: DLL {} rejected by compatibility check, requested {}, DLL compatibility {}, DLL build {}", requested_runtime.Path, requested_runtime.CompatibilityVersion, metadata_compat, metadata_build);
        Platform::UnloadModule(runtime_module);
        return nullptr;
    }

    WriteLog("Client runtime host: accepted DLL {}, runtime {}, build {}, compatibility {}, ABI {}", requested_runtime.Path, exports.Metadata.RuntimeName, exports.Metadata.BuildHash, exports.Metadata.CompatibilityVersion, exports.Metadata.HostAbiVersion);
    return runtime_module;
}

static auto ApplyStagedBinaryUpdate(u8string_view runtime_live_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    // The runtime being loaded decides where staging lives: the install-dir base DLL on the initial
    // load, or the writable-root DLL on an installed client's reload. Portable clients use the exe dir
    // in both passes, so this is identical to the previous exe-dir-only behavior for them.
    const u8string staged_path = MakeClientRuntimeStagingPath(runtime_live_path);
    const u8string binary_dir = fs_path_to_u8string(std::filesystem::path {fs_make_path(runtime_live_path)}.parent_path());

    if (!fs_exists(staged_path)) {
        WriteLog("Client runtime host: no staged DLL at {}", staged_path);
        PromoteStagedRuntimeCompanions(binary_dir);
        return true;
    }

    const u8string final_path {runtime_live_path};
    const u8string backup_path = FormatUtf8("{}.bak", final_path);
    const auto final_exists = fs_exists(final_path);

    const string_view live_dll_status = final_exists ? "yes" : "no";
    WriteLog("Client runtime host: promoting staged DLL {} to {}, backup {}, live DLL exists {}", staged_path, final_path, backup_path, live_dll_status);
    (void)fs_remove_file(backup_path);

    if (final_exists && !fs_rename(final_path, backup_path)) {
        WriteLog("Client runtime host: failed to move live DLL {} to backup {}", final_path, backup_path);
        return false;
    }

    if (!fs_rename(staged_path, final_path)) {
        WriteLog("Client runtime host: failed to promote staged DLL {} to {}", staged_path, final_path);

        if (final_exists) {
            (void)fs_rename(backup_path, final_path);
        }

        return false;
    }

    if (final_exists) {
        (void)fs_remove_file(backup_path);
    }

    PromoteStagedRuntimeCompanions(binary_dir);
    WriteLog("Client runtime host: promoted staged DLL to {}", final_path);

    return true;
}

static auto ResolveRequestedClientRuntime(CommandLineArgs args) -> RequestedClientRuntime
{
    FO_STACK_TRACE_ENTRY();

    RequestedClientRuntime requested_runtime {};
    requested_runtime.Path = ResolveBundledRuntimePath();

    for (size_t index = 1; index < args.size(); index++) {
        const u8string_view arg = args.Get(index);

        if (arg.empty()) {
            continue;
        }

        const bool has_next_arg = index + 1 < args.size();
        const u8string_view next_arg = args.Get(index + 1);

        if (arg == u8"--ClientLibPath" && has_next_arg) {
            requested_runtime.ExplicitPath = true;
            requested_runtime.Path = u8string {next_arg};
        }

        if (arg == u8"--ClientLibCompatibilityVersion" && has_next_arg) {
            requested_runtime.CompatibilityVersion = utf8_to_string(next_arg);
            requested_runtime.CheckCompatibilityVersion = true;
        }

        if (arg == u8"--ForceEmbeddedRuntime" || arg == u8"-ForceEmbeddedRuntime") {
            const u8string_view value = has_next_arg && !CommandLineArgs::IsOption(next_arg) ? next_arg : u8"1";
            requested_runtime.ForceEmbedded = value != u8"0" && value != u8"false" && value != u8"False";
        }
    }

    return requested_runtime;
}

static auto ResolveBundledRuntimePath() -> u8string
{
    FO_STACK_TRACE_ENTRY();

    const u8string install_runtime_path = GetClientRuntimeLivePath();
    const auto bootstrap_path = GetInstalledClientRuntimeBootstrapPath();

    if (!bootstrap_path.has_value()) {
        return install_runtime_path;
    }

    const u8string runtime_file_name = GetCurrentClientRuntimeFileName();
    const u8string bootstrap_target = ResolveClientRuntimeBootstrapTarget(*bootstrap_path, runtime_file_name, install_runtime_path);

    if (bootstrap_target == install_runtime_path) {
        WriteLog("Client runtime host: installed runtime bootstrap {} selected no alternate runtime, using base runtime {}", *bootstrap_path, install_runtime_path);
        return install_runtime_path;
    }

    WriteLog("Client runtime host: selected installed runtime {} from bootstrap {}", bootstrap_target, *bootstrap_path);
    return bootstrap_target;
}

static auto GetInstalledClientRuntimeBootstrapPath() -> optional<u8string>
{
    FO_STACK_TRACE_ENTRY();

    if (!IsInstalledClientLayout()) {
        return std::nullopt;
    }

    const u8string user_data_base = Platform::GetUserDataBase();

    if (user_data_base.empty()) {
        WriteLog(LogType::Warning, "Client runtime host: installed layout detected but no user data dir is available");
        return std::nullopt;
    }

    const u8string runtime_file_name = GetCurrentClientRuntimeFileName();
    const u8string selector_file_name = FormatUtf8("{}.path", runtime_file_name);
    const u8string nice_name {FO_NICE_NAME};
    const u8string runtime_host_dir {"ClientRuntimeHost"};
    const u8string selector_path = fs_path_to_u8string(std::filesystem::path {fs_make_path(user_data_base)} / std::filesystem::path {fs_make_path(nice_name)} / std::filesystem::path {fs_make_path(runtime_host_dir)} / std::filesystem::path {fs_make_path(selector_file_name)});
    return fs_resolve_path(selector_path);
}

static auto IsInstalledClientLayout() -> bool
{
    FO_STACK_TRACE_ENTRY();

    const optional<u8string> exe_path = Platform::GetExePath();

    if (!exe_path.has_value()) {
        return false;
    }

    const u8string installed_marker {"INSTALLED"};
    const u8string installed_path = fs_path_to_u8string(std::filesystem::path {fs_make_path(*exe_path)}.parent_path() / std::filesystem::path {fs_make_path(installed_marker)});
    return fs_exists(installed_path);
}

static auto GetCurrentClientRuntimeFileName() -> u8string
{
    FO_STACK_TRACE_ENTRY();

    u8string runtime_file_name = GetCurrentClientRuntimeLibraryName();
    runtime_file_name.append(GetClientRuntimeLibraryExtension());
    return runtime_file_name;
}

static void CaptureRuntimeResultStrings(ClientRuntimeHostResult& runtime_result)
{
    FO_STACK_TRACE_ENTRY();

    if (runtime_result.Result.RequestedRuntimePath != nullptr) {
        const char* const requested_runtime_path = runtime_result.Result.RequestedRuntimePath;
        runtime_result.RequestedRuntimePath = utf8_from_char_span(const_span<char> {requested_runtime_path, std::strlen(requested_runtime_path)});
        runtime_result.Result.RequestedRuntimePath = utf8_as_char_view(runtime_result.RequestedRuntimePath).data();
    }
    else {
        runtime_result.RequestedRuntimePath.clear();
    }

    if (runtime_result.Result.RequestedCompatibilityVersion != nullptr) {
        runtime_result.RequestedCompatibilityVersion = string(string_view {runtime_result.Result.RequestedCompatibilityVersion});
        runtime_result.Result.RequestedCompatibilityVersion = runtime_result.RequestedCompatibilityVersion.c_str();
    }
    else {
        runtime_result.RequestedCompatibilityVersion.clear();
    }
}
