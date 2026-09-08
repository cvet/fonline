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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
#include "ClientSessionMarker.h"
#include "ClientRuntimeApi.h"
#include "MetadataRegistration.h"
#include "Settings.h"
#include "Updater.h"

FO_USING_NAMESPACE();

#if !FO_WINDOWS && !FO_LINUX && !FO_MAC
static_assert(false, "Client runtime library is supported only on Windows, Linux and macOS");
#endif

FO_BEGIN_NAMESPACE
extern void ApplicationShutdownHook();
FO_END_NAMESPACE

struct ClientAppData
{
    refcount_nptr<ClientEngine> Client {};
    bool ResourcesSynced {};
    bool ReloadRequested {};
    string StagedRuntimePath;
    optional<Updater> ResourceUpdater {};
};
FO_GLOBAL_DATA(ClientAppData, Data);

static void RunClientRuntime(CommandLineArgs args, nptr<ClientRuntimeResult> runtime_result) noexcept;
static void ReportPreviousUncleanSession(string_view marker_path) noexcept;
static void MainEntry(void* data);
static void CleanupClientApp() noexcept;

static void RunClientRuntimeAbi(int32_t argc, char** argv, ClientRuntimeResult* runtime_result) noexcept
{
    FO_STACK_TRACE_ENTRY();

    CommandLineArgs args {argc, argv};
    RunClientRuntime(args, runtime_result);
}

FO_EXPORT_FUNC auto FO_QueryClientRuntimeExports(uint32_t host_abi_version, ClientRuntimeExports* raw_exports) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Client runtime DLL: export query from host ABI {}, runtime ABI {}, exports pointer {}, build {}, compatibility {}", host_abi_version, FO_CLIENT_RUNTIME_HOST_ABI_VERSION, raw_exports ? "set" : "null", FO_BUILD_HASH, FO_COMPATIBILITY_VERSION);

    if (!IsSupportedClientRuntimeAbi(host_abi_version) || raw_exports == nullptr) {
        WriteLog("Client runtime DLL: export query rejected, host ABI {}, runtime ABI {}, exports pointer {}", host_abi_version, FO_CLIENT_RUNTIME_HOST_ABI_VERSION, raw_exports ? "set" : "null");
        return false;
    }

    auto exports = make_ptr(raw_exports);

    // Pin the runtime name string for the lifetime of this DLL — host reads it as
    // const char* through the ABI, and it must outlive every consumer call
    static const string runtime_name = GetCurrentClientRuntimeLibraryName();

    exports->StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeExports));
    exports->Metadata.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeMetadata));
    exports->Metadata.HostAbiVersion = FO_CLIENT_RUNTIME_HOST_ABI_VERSION;
    exports->Metadata.RuntimeName = runtime_name.c_str();
    exports->Metadata.BuildHash = FO_BUILD_HASH;
    exports->Metadata.CompatibilityVersion = FO_COMPATIBILITY_VERSION;
    exports->Run = &RunClientRuntimeAbi;

    WriteLog("Client runtime DLL: exports ready, runtime {}, build {}, compatibility {}, ABI {}", runtime_name, FO_BUILD_HASH, FO_COMPATIBILITY_VERSION, FO_CLIENT_RUNTIME_HOST_ABI_VERSION);
    return true;
}

// A marker left behind means the previous run never reached its clean exit. Reported, not thrown: this
// run is healthy, and the exception object is what carries the stage into the crash reporter
static void ReportPreviousUncleanSession(string_view marker_path) noexcept
{
    FO_STACK_TRACE_ENTRY();

    auto previous = TakePreviousClientSession(marker_path);

    if (!previous.has_value()) {
        return;
    }

    WriteLog("Client runtime DLL: previous session did not exit cleanly, stage {}, build {}, started {}", previous->StageName, previous->BuildHash, previous->StartedAt);

    safe_call([&] {
        ClientSessionException ex("Previous client session did not exit cleanly", previous->StageName, previous->BuildHash, previous->StartedAt, FO_BUILD_HASH);
        ReportExceptionAndContinue(ex);
    });
}

static void RunClientRuntime(CommandLineArgs args, nptr<ClientRuntimeResult> runtime_result) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (runtime_result) {
        runtime_result->StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
        runtime_result->ResultKind = ClientRuntimeResultKind::Shutdown;
        runtime_result->Success = false;
        runtime_result->RequestedRuntimePath = nullptr;
        runtime_result->RequestedCompatibilityVersion = nullptr;
    }

    // Outside the try, because the stages recorded below it run after the catch as well. Resolved once
    // settings are loaded, then handed to the host: only the runtime can answer where the client writes
    string session_marker;

    try {
        WriteLog("Client runtime DLL: starting, build {}, compatibility {}", FO_BUILD_HASH, FO_COMPATIBILITY_VERSION);

        InitApp(args, CombineEnum(AppInitFlags::ClientMode, AppInitFlags::ShowMessageOnException, AppInitFlags::PrebakeResources, AppInitFlags::AppendLogFile));
        WriteLog("Client runtime DLL: compatibility version: {}", GetApp()->Settings.CompatibilityVersion);

        // The crash reporter is alive only from here, and a run that hung on the way out could report
        // nothing at the time. Whatever the previous run left behind is delivered now
        session_marker = MakeClientSessionMarkerPath(GetApp()->Settings.UserWritablePath);
        ReportPreviousUncleanSession(session_marker);
        BeginClientSession(session_marker);

        auto balancer = FrameBalancer(!GetApp()->Settings.VSync, GetApp()->Settings.Sleep, GetApp()->Settings.FixedFPS);

        while (!GetApp()->IsQuitRequested()) {
            balancer.StartLoop();
            MainEntry(nullptr);
            balancer.EndLoop();
        }

        WriteLog("Client runtime DLL: main loop exited");
        SetClientShutdownStage(session_marker, ClientShutdownStage::MainLoopExited);

        bool quit_success = GetApp()->GetRequestedQuitSuccess();
        CleanupClientApp();
        SetClientShutdownStage(session_marker, ClientShutdownStage::ClientStopped);

        if (runtime_result) {
            if (Data->ReloadRequested) {
                FO_VERIFY_AND_THROW(!Data->StagedRuntimePath.empty(), "Client runtime requested reload but did not provide a staged runtime path", quit_success, Data->ReloadRequested);
                WriteLog("Client runtime DLL: requesting reload from {}", Data->StagedRuntimePath);
                runtime_result->ResultKind = ClientRuntimeResultKind::ReloadRequested;
                runtime_result->Success = true;
                runtime_result->RequestedRuntimePath = Data->StagedRuntimePath.c_str();
            }
            else {
                WriteLog("Client runtime DLL: returning shutdown, success {}", quit_success ? "yes" : "no");
                runtime_result->Success = quit_success;
            }
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        CleanupClientApp();

        if (runtime_result) {
            runtime_result->ResultKind = ClientRuntimeResultKind::FatalError;
        }
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }

    if (IsAppInitialized()) {
        WriteLog("Client runtime DLL: resetting application before return");
        ResetApp();
    }

    SetClientShutdownStage(session_marker, ClientShutdownStage::ApplicationReset);

    WriteLog("Client runtime DLL: calling application shutdown hook");
    safe_call([] { ApplicationShutdownHook(); });
    SetClientShutdownStage(session_marker, ClientShutdownStage::ShutdownHookDone);

    string_view result_kind = "none";
    bool result_success = false;

    if (runtime_result) {
        result_kind = ClientRuntimeResultKindToString(runtime_result->ResultKind);
        result_success = runtime_result->Success;
    }

    WriteLog("Client runtime DLL: finished with {}, result pointer {}, success {}", result_kind, runtime_result ? "set" : "null", result_success ? "yes" : "no");
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
                    if (!GetApp()->Settings.Packaged) {
                        Data->ResourcesSynced = true;
                        return;
                    }

                    if (!Data->ResourceUpdater) {
                        WriteLog("Client runtime DLL: creating updater");
                        Data->ResourceUpdater.emplace(&GetApp()->Settings, &GetApp()->MainWindow);
                    }

                    if (!Data->ResourceUpdater->Process()) {
                        return;
                    }

                    auto result = Data->ResourceUpdater->GetResult();
                    // The updater stages the new runtime under its own binary dir (the writable root
                    // for an installed client, the exe dir for a portable one); request that exact path
                    string staged_runtime_path = Data->ResourceUpdater->GetRuntimeLivePath();
                    Data->ResourceUpdater.reset();

                    switch (result) {
                    case UpdaterResult::ResourcesReady:
                        WriteLog("Client runtime DLL: updater finished, resources ready");
                        Data->ResourcesSynced = true;
                        break;
                    case UpdaterResult::BinariesStaged:
                        Data->StagedRuntimePath = staged_runtime_path;
                        Data->ReloadRequested = true;
                        WriteLog("Client runtime DLL: updater staged binaries at {}", Data->StagedRuntimePath);
                        GetApp()->RequestQuit();
                        return;
                    default:
                        WriteLog("Client runtime DLL: updater failed");
                        ShowUpdaterFailure(result);
                        GetApp()->RequestQuit();
                        return;
                    }
                }

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
        auto client = GetClient();
        client->Shutdown();
        Data->Client.reset();
    }
}
