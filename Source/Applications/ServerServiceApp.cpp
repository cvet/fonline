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
#include "Server.h"
#include "Settings.h"

#if FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include "WinApiUndef.inc"

FO_USING_NAMESPACE();

#if FO_WINDOWS
static wchar_t ServiceName[32] = L"FOnlineServer";
#endif

struct ServerServiceAppData
{
    refcount_nptr<ServerEngine> Server {};
    thread ServerThread {};
    uint32_t LastState {};
    uint32_t CheckPoint {};
#if FO_WINDOWS
    SERVICE_STATUS_HANDLE FOServiceStatusHandle {};
#endif
};
FO_GLOBAL_DATA(ServerServiceAppData, Data);

#if FO_WINDOWS
static VOID WINAPI FOServiceCtrlHandler(DWORD opcode);
static void SetFOServiceStatus(uint32_t state);
#endif

static auto GetServiceServer() -> ptr<ServerEngine>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(Data->Server, "Server engine is not created");
    return Data->Server.as_ptr();
}

static void ServerEntry()
{
    FO_STACK_TRACE_ENTRY();

    try {
        ptr<GlobalSettings> settings = &GetApp()->Settings;
        Data->Server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, GetServerResources(*settings));
        auto server = GetServiceServer();
        GetApp()->WaitForRequestedQuit();
        server->Shutdown();
        Data->Server.reset();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

#if FO_WINDOWS
static VOID WINAPI FOServiceStart(DWORD argc, LPTSTR* argv)
{
    FO_STACK_TRACE_ENTRY();

    try {
        const size_t arg_count = numeric_cast<size_t>(argc);
        const nptr<LPTSTR> service_argv = argv;
        FO_VERIFY_AND_THROW(arg_count == 0 || service_argv, "Service argument vector is null with a non-zero argument count");

        static std::vector<std::string> args_holder;
        static vector<CommandLineArg> args;
        args_holder.resize(arg_count);
        args.resize(arg_count);

        for (size_t i = 0; i < arg_count; ++i) {
            nptr<const wchar_t> service_arg = service_argv[i];
            FO_VERIFY_AND_THROW(service_arg, "Service argument string is null");
            args_holder[i] = strex().parse_wide_char(service_arg.get());

            args[i] = args_holder[i].data();
        }

        const CommandLineArgs service_args {args};
        InitApp(service_args, AppInitFlags::PrebakeResources);

        Data->FOServiceStatusHandle = ::RegisterServiceCtrlHandlerW(ServiceName, FOServiceCtrlHandler);

        if (Data->FOServiceStatusHandle == nullptr) {
            return;
        }

        SetFOServiceStatus(SERVICE_START_PENDING);

        Data->ServerThread = run_thread("ServerService", ServerEntry);

        while (true) {
            if (Data->Server) {
                auto server = GetServiceServer();

                if (server->IsStarted() || server->IsStartingError()) {
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto server = GetServiceServer();
        if (server->IsStarted()) {
            SetFOServiceStatus(SERVICE_RUNNING);
        }
        else {
            SetFOServiceStatus(SERVICE_STOPPED);
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}
#endif

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ServerServiceApp(CommandLineArgs args) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

#if !FO_TESTING_APP
    const CommandLineArgs args {numeric_cast<int32_t>(argc), argv};
#endif

    try {
        InitApp(args, AppInitFlags::PrebakeResources);

#if FO_WINDOWS
        if (std::wstring(::GetCommandLineW()).find(L"--server-service-start") != std::wstring::npos) {
            // Start
            constexpr SERVICE_TABLE_ENTRY dispatch_table[] = {{ServiceName, FOServiceStart}, {nullptr, nullptr}};
            ::StartServiceCtrlDispatcherW(dispatch_table);
        }
        else if (std::wstring(::GetCommandLineW()).find(L"--server-service-delete") != std::wstring::npos) {
            // Delete
            const SC_HANDLE manager = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
            if (manager == nullptr) {
                ::MessageBoxW(nullptr, L"Can't open service manager", ServiceName, MB_OK | MB_ICONHAND);
                return 1;
            }

            const SC_HANDLE service = ::OpenServiceW(manager, ServiceName, DELETE);
            if (service == nullptr) {
                ::CloseServiceHandle(manager);
                return 0;
            }

            const auto error = (::DeleteService(service) == FALSE);
            if (!error) {
                ::MessageBoxW(nullptr, L"Service deleted", ServiceName, MB_OK | MB_ICONASTERISK);
            }
            else {
                ::MessageBoxW(nullptr, L"Can't delete service", ServiceName, MB_OK | MB_ICONHAND);
            }

            ::CloseServiceHandle(service);
            ::CloseServiceHandle(manager);

            if (error) {
                return 1;
            }
        }
        else {
            // Register or manage service
            const SC_HANDLE manager = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
            if (manager == nullptr) {
                ::MessageBoxW(nullptr, L"Can't open service manager", ServiceName, MB_OK | MB_ICONHAND);
                return 1;
            }

            // Manage service
            SC_HANDLE service = ::OpenServiceW(manager, ServiceName, SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_START);
            auto error = false;

            // Evaluate service path
            constexpr DWORD buf_len = 4096 * 2;
            wchar_t buf[buf_len];
            ::GetModuleFileNameW(nullptr, buf, buf_len);
            const auto path = std::wstring(L"\"").append(buf).append(L"\" ").append(::GetCommandLineW()).append(L" --server-service");
            ptr<const wchar_t> path_cstr = path.c_str();

            // Change executable path, if changed
            if (service != nullptr) {
                // ReSharper disable once CppLocalVariableMayBeConst
                alignas(QUERY_SERVICE_CONFIGW) uint8_t service_cfg_buf[8192] = {};
                ptr<uint8_t> service_cfg_storage = service_cfg_buf;
                ptr<QUERY_SERVICE_CONFIGW> service_cfg = service_cfg_storage.reinterpret_as<QUERY_SERVICE_CONFIGW>();

                DWORD dw = 0;
                if (::QueryServiceConfigW(service, service_cfg.get(), 8192, &dw) == 0) {
                    error = true;
                }
                else if (path != service_cfg->lpBinaryPathName) {
                    if (::ChangeServiceConfigW(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, path_cstr.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == FALSE) {
                        error = true;
                    }
                }
            }

            // Register service
            if (service == nullptr) {
                service = ::CreateServiceW(manager, ServiceName, ServiceName, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, path_cstr.get(), nullptr, nullptr, nullptr, nullptr, nullptr);

                if (service == nullptr) {
                    error = true;
                }

                if (service != nullptr) {
                    ::MessageBoxW(nullptr, L"Service registered", ServiceName, MB_OK | MB_ICONASTERISK);
                }
                else {
                    ::MessageBoxW(nullptr, L"Unable to register service", ServiceName, MB_OK | MB_ICONHAND);
                }
            }

            // Close handles
            if (service != nullptr) {
                ::CloseServiceHandle(service);
            }
            ::CloseServiceHandle(manager);

            if (error) {
                return 1;
            }
        }
#else
        FO_VERIFY_AND_THROW(false, "Invalid OS");
#endif
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }

    return 0;
}

#if FO_WINDOWS
static VOID WINAPI FOServiceCtrlHandler(DWORD opcode)
{
    try {
        if (opcode == SERVICE_CONTROL_STOP) {
            SetFOServiceStatus(SERVICE_STOP_PENDING);

            GetApp()->RequestQuit();

            if (Data->ServerThread.joinable()) {
                Data->ServerThread.join();
            }

            SetFOServiceStatus(SERVICE_STOPPED);
        }
        else {
            SetFOServiceStatus(0);
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

static void SetFOServiceStatus(uint32_t state)
{
    if (state == 0) {
        state = Data->LastState;
    }
    else {
        Data->LastState = state;
    }

    SERVICE_STATUS status;
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwWin32ExitCode = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwWaitHint = 0;
    status.dwCheckPoint = 0;
    status.dwControlsAccepted = 0;

    if (state == SERVICE_RUNNING) {
        status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    }
    if (state != SERVICE_RUNNING && state != SERVICE_STOPPED) {
        status.dwCheckPoint = ++Data->CheckPoint;
    }

    ::SetServiceStatus(Data->FOServiceStatusHandle, &status);
}
#endif
