#include "Common.h"
#include "FileUtils.h"
#include "Server.h"
#include "Settings.h"
#include "Testing.h"
#include "WinApi_Include.h"

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
    Server = new FOServer();
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
    LogToFile("FOnlineServerService.log");
    InitialSetup("FOnlineServerService", argc, argv);

#ifdef FO_WINDOWS
    if (GameOpt.CommandLine.find("--server-service-start") != string::npos)
    {
        // Start
        SERVICE_TABLE_ENTRY dispatch_table[] = {{L"FOnlineServer", FOServiceStart}, {nullptr, nullptr}};
        StartServiceCtrlDispatcher(dispatch_table);
    }
    else if (GameOpt.CommandLine.find("--server-service-delete") != string::npos)
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
        string path = _str("\"{}\" {} --server-service", File::GetExePath(), GameOpt.CommandLine);

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
        GameOpt.Quit = true;
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
