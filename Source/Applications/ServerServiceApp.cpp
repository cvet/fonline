// FOnline Engine
// SPDX-License-Identifier: MIT

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
static constexpr std::chrono::milliseconds SERVICE_STOP_POLL_INTERVAL {1};
static wchar_t ServiceName[32] {L"FOnlineServer"};
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
    FO_VERIFY_AND_THROW(Data->Server, "Server engine is not created");
    return Data->Server;
}

static void ServerEntry()
{
    try {
        auto settings {make_ptr(&GetApp()->Settings)};
        Data->Server = safe_alloc::make_refcounted<ServerEngine>(settings, GetServerResources(*settings));
        auto server {GetServiceServer()};
        GetApp()->WaitForRequestedQuit();
        server->Shutdown();
        Data->Server.reset();
    }
    catch (const std::exception& ex) {
        exceptions::report_and_exit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

#if FO_WINDOWS
static VOID WINAPI FOServiceStart(DWORD argc, LPTSTR* argv)
{
    FO_TRACE_ZONE(App);

    try {
        size_t arg_count {numeric_cast<size_t>(argc)};
        auto service_argv {make_nptr(argv)};
        FO_VERIFY_AND_THROW(arg_count == 0 || service_argv, "Service argument vector is null with a non-zero argument count");

        vector<string> arg_storage {vector<string>(arg_count)};

        for (size_t i {0}; i < arg_count; ++i) {
            auto service_arg {make_nptr(service_argv[i])};
            FO_VERIFY_AND_THROW(service_arg, "Service argument string is null");
            arg_storage[i] = strex().parse_wide_char(service_arg);
        }

        vector<CommandLineArg> arg_pointers {};
        arg_pointers.reserve(arg_count);

        for (string& arg : arg_storage) {
            arg_pointers.emplace_back(arg.data());
        }

        CommandLineArgs service_args {arg_pointers};
        InitApp(service_args, AppInitFlags::PrebakeResources);

        Data->FOServiceStatusHandle = ::RegisterServiceCtrlHandlerW(ServiceName, FOServiceCtrlHandler);

        if (Data->FOServiceStatusHandle == nullptr) {
            return;
        }

        SetFOServiceStatus(SERVICE_START_PENDING);

        Data->ServerThread = run_thread("ServerService", ServerEntry);

        while (true) {
            if (Data->Server) {
                auto server {GetServiceServer()};

                if (server->IsStarted() || server->IsStartingError()) {
                    break;
                }
            }

            coarse_sleep(SERVICE_STOP_POLL_INTERVAL);
        }

        auto server {GetServiceServer()};
        if (server->IsStarted()) {
            SetFOServiceStatus(SERVICE_RUNNING);
        }
        else {
            SetFOServiceStatus(SERVICE_STOPPED);
        }
    }
    catch (const std::exception& ex) {
        exceptions::report_and_exit(ex);
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
#if !FO_TESTING_APP
    ProgramArgs program_args {numeric_cast<int32_t>(argc), argv};
    CommandLineArgs args {program_args.GetArgs()};
#endif

    try {
        InitApp(args, AppInitFlags::PrebakeResources);

#if FO_WINDOWS
        if (args.GetSettingValue(CommandLineSettings::ServerServiceStart).has_value()) {
            // Start
            constexpr SERVICE_TABLE_ENTRY dispatch_table[] {{ServiceName, FOServiceStart}, {nullptr, nullptr}};
            ::StartServiceCtrlDispatcherW(dispatch_table);
        }
        else if (args.GetSettingValue(CommandLineSettings::ServerServiceDelete).has_value()) {
            // Delete
            SC_HANDLE manager {::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS)};
            if (manager == nullptr) {
                ::MessageBoxW(nullptr, L"Can't open service manager", ServiceName, MB_OK | MB_ICONHAND);
                return 1;
            }

            SC_HANDLE service {::OpenServiceW(manager, ServiceName, DELETE)};
            if (service == nullptr) {
                ::CloseServiceHandle(manager);
                return 0;
            }

            bool error {(::DeleteService(service) == FALSE)};
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
            SC_HANDLE manager {::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS)};
            if (manager == nullptr) {
                ::MessageBoxW(nullptr, L"Can't open service manager", ServiceName, MB_OK | MB_ICONHAND);
                return 1;
            }

            // Manage service
            SC_HANDLE service {::OpenServiceW(manager, ServiceName, SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_START)};
            bool error {false};

            // Evaluate service path
            constexpr DWORD buf_len {4096 * 2};
            wchar_t buf[buf_len] {};
            ::GetModuleFileNameW(nullptr, buf, buf_len);
            wstring service_start_arg {strex(" --{}", CommandLineSettings::ServerServiceStart).to_wide_char()};
            wstring path {wstring(L"\"").append(buf).append(L"\" ").append(::GetCommandLineW()).append(service_start_arg)};
            auto path_cstr {make_ptr(path.c_str())};

            // Change executable path, if changed
            if (service != nullptr) {
                alignas(QUERY_SERVICE_CONFIGW) uint8_t service_cfg_buf[8192] {};
                auto service_cfg_storage {make_ptr(service_cfg_buf)};
                auto service_cfg {service_cfg_storage.reinterpret_as<QUERY_SERVICE_CONFIGW>()};

                if (DWORD dw {0}; ::QueryServiceConfigW(service, service_cfg.get(), 8192, &dw) == 0) {
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
        exceptions::report_and_exit(ex);
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
        exceptions::report_and_exit(ex);
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

    SERVICE_STATUS status {};
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
