//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

FO_USING_NAMESPACE();

#if !FO_WINDOWS && !FO_LINUX && !FO_MAC
static_assert(false, "Client runtime library is supported only on Windows, Linux and macOS");
#endif

struct ClientAppData
{
    refcount_ptr<ClientEngine> Client {};
    bool ResourcesSynced {};
    bool ReloadRequested {};
    string StagedRuntimePath;
    unique_ptr<Updater> ResourceUpdater {};
};
FO_GLOBAL_DATA(ClientAppData, Data);

static void RunClientRuntime(int32_t argc, char** argv, ClientRuntimeResult* runtime_result) noexcept;
static void MainEntry(void* data);
static void CleanupClientApp() noexcept;

FO_EXPORT_FUNC auto FO_QueryClientRuntimeExports(uint32_t host_abi_version, ClientRuntimeExports* exports) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!IsSupportedClientRuntimeAbi(host_abi_version) || exports == nullptr) {
        return false;
    }

    // Pin the runtime name string for the lifetime of this DLL — host reads it as
    // const char* through the ABI, and it must outlive every consumer call.
    static const string runtime_name = GetCurrentClientRuntimeLibraryName();

    exports->StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeExports));
    exports->Metadata.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeMetadata));
    exports->Metadata.HostAbiVersion = FO_CLIENT_RUNTIME_HOST_ABI_VERSION;
    exports->Metadata.RuntimeName = runtime_name.c_str();
    exports->Metadata.BuildHash = FO_BUILD_HASH.c_str();
    exports->Metadata.CompatibilityVersion = FO_COMPATIBILITY_VERSION.c_str();
    exports->Run = &RunClientRuntime;
    return true;
}

static void RunClientRuntime(int32_t argc, char** argv, ClientRuntimeResult* runtime_result) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (runtime_result != nullptr) {
        runtime_result->StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
        runtime_result->ResultKind = ClientRuntimeResultKind::Shutdown;
        runtime_result->Success = false;
        runtime_result->RequestedRuntimePath = nullptr;
        runtime_result->RequestedCompatibilityVersion = nullptr;
    }

    try {
        InitApp(argc, argv, CombineEnum(AppInitFlags::ClientMode, AppInitFlags::ShowMessageOnException, AppInitFlags::PrebakeResources));
        WriteLog("Compatibility version: {}", App->Settings.CompatibilityVersion);

        auto balancer = FrameBalancer(!App->Settings.VSync, App->Settings.Sleep, App->Settings.FixedFPS);

        while (!App->IsQuitRequested()) {
            balancer.StartLoop();
            MainEntry(nullptr);
            balancer.EndLoop();
        }

        WriteLog("Exit from game");

        const bool quit_success = App->GetRequestedQuitSuccess();
        CleanupClientApp();

        if (runtime_result != nullptr) {
            if (Data->ReloadRequested) {
                FO_RUNTIME_ASSERT(!Data->StagedRuntimePath.empty());
                runtime_result->ResultKind = ClientRuntimeResultKind::ReloadRequested;
                runtime_result->Success = true;
                runtime_result->RequestedRuntimePath = Data->StagedRuntimePath.c_str();
            }
            else {
                runtime_result->Success = quit_success;
            }
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        CleanupClientApp();

        if (runtime_result != nullptr) {
            runtime_result->ResultKind = ClientRuntimeResultKind::FatalError;
        }
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

static void MainEntry([[maybe_unused]] void* data)
{
    FO_STACK_TRACE_ENTRY();

    if (App->IsQuitRequested()) {
        return;
    }

    try {
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
        Data->Client->Shutdown();
        Data->Client.reset();
    }
}
