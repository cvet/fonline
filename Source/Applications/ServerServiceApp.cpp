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
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "FileSystem.h"
#include "Server.h"
#include "Settings.h"
#include "Testing.h"
#include "Version_Include.h"
#include "WinApi_Include.h"

static GlobalSettings Settings;
static FOServer* Server;
static std::thread ServerThread;

#ifdef FO_WINDOWS
static SERVICE_STATUS_HANDLE FOServiceStatusHandle;
static VOID WINAPI FOServiceStart(DWORD argc, LPTSTR* argv);
static VOID WINAPI FOServiceCtrlHandler(DWORD opcode);
static void SetFOServiceStatus(uint state);
#endif

static void ServerEntry()
{
    Server = new FOServer(Settings);
    Server->Run();
    FOServer* server = Server;
    Server = nullptr;
    delete server;
}

#ifndef FO_TESTING
int main(int argc, char** argv)
#else
static int main_disabled(int argc, char** argv)
#endif
{
    CatchExceptions("FOnlineServerService", FO_VERSION);
    LogToFile("FOnlineServerService.log");
    Settings.ParseArgs(argc, argv);

#ifdef FO_WINDOWS
    if (Settings.CommandLine.find("--server-service-start") != string::npos)
    {
        // Start
        SERVICE_TABLE_ENTRY dispatch_table[] = {{L"FOnlineServer", FOServiceStart}, {nullptr, nullptr}};
        StartServiceCtrlDispatcher(dispatch_table);
    }
    else if (Settings.CommandLine.find("--server-service-delete") != string::npos)
    {
        // Delete
        SC_HANDLE manager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
        if (!manager)
        {
            MessageBoxW(nullptr, L"Can't open service manager.", L"FOnlineServer", MB_OK | MB_ICONHAND);
            return 1;
        }

        SC_HANDLE service = OpenServiceW(manager, L"FOnlineServer", DELETE);
        if (!service)
            return 0;

        bool error = (DeleteService(service) == FALSE);
        if (!error)
            MessageBoxW(nullptr, L"Service deleted.", L"FOnlineServer", MB_OK | MB_ICONASTERISK);
        else
            MessageBoxW(nullptr, L"Can't delete service.", L"FOnlineServer", MB_OK | MB_ICONHAND);

        CloseServiceHandle(service);
        CloseServiceHandle(manager);

        if (error)
            return 1;
    }
    else
    {
        // Register
        SC_HANDLE manager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
        if (!manager)
        {
            MessageBoxW(nullptr, L"Can't open service manager.", L"FOnlineServer", MB_OK | MB_ICONHAND);
            return 1;
        }

        // Manage service
        SC_HANDLE service = OpenServiceW(manager, L"FOnlineServer",
            SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_START);
        bool error = false;

        // Compile service path
        string path = _str("\"{}\" {} --server-service", DiskFileSystem::GetExePath(), Settings.CommandLine);

        // Change executable path, if changed
        if (service)
        {
            LPQUERY_SERVICE_CONFIG service_cfg = (LPQUERY_SERVICE_CONFIG)calloc(8192, 1);
            DWORD dw;
            if (!QueryServiceConfigW(service, service_cfg, 8192, &dw))
            {
                error = true;
            }
            else if (_str(_str().parseWideChar(service_cfg->lpBinaryPathName)) != path)
            {
                if (!ChangeServiceConfigW(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
                        _str(path).toWideChar().c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr))
                    error = true;
            }
            free(service_cfg);
        }

        // Register service
        if (!service)
        {
            service = CreateServiceW(manager, L"FOnlineServer", L"FOnlineServer", SERVICE_ALL_ACCESS,
                SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, _str(path).toWideChar().c_str(),
                nullptr, nullptr, nullptr, nullptr, nullptr);

            if (!service)
                error = true;

            if (service)
                MessageBoxW(
                    nullptr, L"\'FOnlineServer\' service registered.", L"FOnlineServer", MB_OK | MB_ICONASTERISK);
            else
                MessageBoxW(
                    nullptr, L"Can't register \'FOnlineServer\' service.", L"FOnlineServer", MB_OK | MB_ICONHAND);
        }

        // Close handles
        if (service)
            CloseServiceHandle(service);
        if (manager)
            CloseServiceHandle(manager);

        if (error)
            return 1;
    }

#else
    RUNTIME_ASSERT(!"Invalid OS");
#endif

    return 0;
}

#ifdef FO_WINDOWS
static VOID WINAPI FOServiceStart(DWORD argc, LPTSTR* argv)
{
    LogToFile("FOnlineServerService.log");

    FOServiceStatusHandle = RegisterServiceCtrlHandlerW(L"FOnlineServer", FOServiceCtrlHandler);
    if (!FOServiceStatusHandle)
        return;

    SetFOServiceStatus(SERVICE_START_PENDING);

    ServerThread = std::thread(ServerEntry);
    while (!Server || !Server->Started() || !Server->Stopped())
        std::this_thread::sleep_for(std::chrono::milliseconds(0));

    if (Server->Started())
        SetFOServiceStatus(SERVICE_RUNNING);
    else
        SetFOServiceStatus(SERVICE_STOPPED);
}

static VOID WINAPI FOServiceCtrlHandler(DWORD opcode)
{
    switch (opcode)
    {
    case SERVICE_CONTROL_STOP:
        SetFOServiceStatus(SERVICE_STOP_PENDING);
        Settings.Quit = true;
        if (ServerThread.joinable())
            ServerThread.join();
        SetFOServiceStatus(SERVICE_STOPPED);
        return;
    case SERVICE_CONTROL_INTERROGATE:
        // Fall through to send current status
        break;
    default:
        break;
    }

    // Send current status
    SetFOServiceStatus(0);
}

static void SetFOServiceStatus(uint state)
{
    static uint last_state = 0;
    static uint check_point = 0;

    if (!state)
        state = last_state;
    else
        last_state = state;

    SERVICE_STATUS srv_status;
    srv_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    srv_status.dwCurrentState = state;
    srv_status.dwWin32ExitCode = 0;
    srv_status.dwServiceSpecificExitCode = 0;
    srv_status.dwWaitHint = 0;
    srv_status.dwCheckPoint = 0;
    srv_status.dwControlsAccepted = 0;

    if (state == SERVICE_RUNNING)
        srv_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    if (!(state == SERVICE_RUNNING || state == SERVICE_STOPPED))
        srv_status.dwCheckPoint = ++check_point;

    SetServiceStatus(FOServiceStatusHandle, &srv_status);
}
#endif
