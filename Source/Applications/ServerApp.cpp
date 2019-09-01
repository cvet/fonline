#ifndef FO_SERVER_DAEMON
# include "SDL_main.h"
#endif
#include "Common.h"
#include "Testing.h"
#include "Server.h"
#include "Timer.h"
#include "NetBuffer.h"
#include "IniFile.h"
#include "StringUtils.h"

#ifndef FO_SERVER_DAEMON
# include "AppGui.h"
# include "SDL.h"
#endif

static FOServer* Server;
static Thread    ServerThread;
static bool      StartServer;

static void InitAdminManager();

static void ServerEntry( void* )
{
    while( !StartServer )
        Thread::Sleep( 10 );

    GetServerOptions();

    // Memory debugging
    MemoryDebugLevel = MainConfig->GetInt( "", "MemoryDebugLevel", 0 );
    if( MemoryDebugLevel > 2 )
        Debugger::StartTraceMemory();

    // Admin manager
    InitAdminManager();

    // Init server
    Server = new FOServer();
    if( !Server->Init() )
    {
        WriteLog( "Initialization failed!\n" );
        return;
    }

    // Server loop
    WriteLog( "***   Starting game loop   ***\n" );

    while( !GameOpt.Quit )
        Server->LogicTick();

    WriteLog( "***   Finishing game loop  ***\n" );

    // Finish server
    Server->Finish();
    FOServer* server = Server;
    Server = nullptr;
    delete server;
}

/************************************************************************/
/* GUI & Windows service version                                        */
/************************************************************************/

#ifndef FO_SERVER_DAEMON

// Windows service
# ifdef FO_WINDOWS
static void ServiceMain( bool as_service );
# endif

# ifndef FO_TESTING
extern "C" int main( int argc, char** argv ) // Handled by SDL
# else
static int main_disabled( int argc, char** argv )
# endif
{
    InitialSetup( "FOnlineServer", argc, argv );

    Thread::SetCurrentName( "GUI" );

    // Service
    if( MainConfig->IsKey( "", "Service" ) )
    {
        # ifdef FO_WINDOWS
        ServiceMain( MainConfig->IsKey( "", "StartService" ) );
        # endif
        return 0;
    }

    // Logging
    LogToFile( "FOnlineServer.log" );
    LogToBuffer( true );
    WriteLog( "FOnline Server, version {}.\n", FONLINE_VERSION );

    // Initialize Gui
    bool use_dx = ( MainConfig->GetInt( "", "UseDirectX" ) != 0 );
    if( !AppGui::Init( "FOnline Server", use_dx, false, false ) )
        return -1;

    // Autostart
    if( !MainConfig->IsKey( "", "NoStart" ) )
        StartServer = true;

    // Server loop in separate thread
    ServerThread.Start( ServerEntry, "Main" );
    while( StartServer && !Server )
        Thread::Sleep( 0 );

    // Gui loop
    while( !StartServer || Server )
    {
        if( !AppGui::BeginFrame() )
        {
            // Immediate finish
            if( !StartServer || !Server || Server->Starting() )
                ExitProcess( 0 );

            // Graceful finish
            GameOpt.Quit = true;
        }

        if( !StartServer )
        {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos( ImVec2( io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f ), ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
            if( ImGui::Begin( "---", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse ) )
            {
                if( ImGui::Button( "Start server", ImVec2( 200, 30 ) ) )
                {
                    StartServer = true;
                    while( !Server )
                        Thread::Sleep( 0 );
                }
            }
            ImGui::End();
        }
        else
        {
            Server->DrawGui();
        }

        AppGui::EndFrame();
    }

    // Wait server finish
    if( StartServer )
        ServerThread.Wait();

    return 0;
}

#endif // !FO_SERVER_DAEMON

/************************************************************************/
/* Windows service                                                      */
/************************************************************************/
#ifdef FO_WINDOWS

static SERVICE_STATUS_HANDLE FOServiceStatusHandle;
static VOID WINAPI FOServiceStart( DWORD argc, LPTSTR* argv );
static VOID WINAPI FOServiceCtrlHandler( DWORD opcode );
static void        SetFOServiceStatus( uint state );

static void ServiceMain( bool as_service )
{
    // Binary started as service
    if( as_service )
    {
        // Start
        SERVICE_TABLE_ENTRY dispatch_table[] = { { L"FOnlineServer", FOServiceStart }, { nullptr, nullptr } };
        StartServiceCtrlDispatcher( dispatch_table );
        return;
    }

    // Open service manager
    SC_HANDLE manager = OpenSCManager( nullptr, nullptr, SC_MANAGER_ALL_ACCESS );
    if( !manager )
    {
        MessageBoxW( nullptr, L"Can't open service manager.", L"FOnlineServer", MB_OK | MB_ICONHAND );
        return;
    }

    // Delete service
    if( MainConfig->IsKey( "", "Delete" ) )
    {
        SC_HANDLE service = OpenServiceW( manager, L"FOnlineServer", DELETE );

        if( service && DeleteService( service ) )
            MessageBoxW( nullptr, L"Service deleted.", L"FOnlineServer", MB_OK | MB_ICONASTERISK );
        else
            MessageBoxW( nullptr, L"Can't delete service.", L"FOnlineServer", MB_OK | MB_ICONHAND );

        CloseServiceHandle( service );
        CloseServiceHandle( manager );
        return;
    }

    // Manage service
    SC_HANDLE service = OpenServiceW( manager, L"FOnlineServer", SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_START );

    // Compile service path
    string path = _str( "\"{}\" --service {}", File::GetExePath(), GameOpt.CommandLine );

    // Change executable path, if changed
    if( service )
    {
        LPQUERY_SERVICE_CONFIG service_cfg = (LPQUERY_SERVICE_CONFIG) calloc( 8192, 1 );
        DWORD                  dw;
        if( QueryServiceConfigW( service, service_cfg, 8192, &dw ) && !_str( _str().parseWideChar( service_cfg->lpBinaryPathName ) ).compareIgnoreCase( path ) )
            ChangeServiceConfigW( service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, _str( path ).toWideChar().c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
        free( service_cfg );
    }

    // Register service
    if( !service )
    {
        service = CreateServiceW( manager, L"FOnlineServer", L"FOnlineServer", SERVICE_ALL_ACCESS,
                                  SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, _str( path ).toWideChar().c_str(), nullptr, nullptr, nullptr, nullptr, nullptr );

        if( service )
            MessageBoxW( nullptr, L"\'FOnlineServer\' service registered.", L"FOnlineServer", MB_OK | MB_ICONASTERISK );
        else
            MessageBoxW( nullptr, L"Can't register \'FOnlineServer\' service.", L"FOnlineServer", MB_OK | MB_ICONHAND );
    }

    // Close handles
    if( service )
        CloseServiceHandle( service );
    if( manager )
        CloseServiceHandle( manager );
}

static VOID WINAPI FOServiceStart( DWORD argc, LPTSTR* argv )
{
    Thread::SetCurrentName( "Service" );
    LogToFile( "FOnlineServer.log" );
    WriteLog( "FOnline server service, version {}.\n", FONLINE_VERSION );

    FOServiceStatusHandle = RegisterServiceCtrlHandlerW( L"FOnlineServer", FOServiceCtrlHandler );
    if( !FOServiceStatusHandle )
        return;

    SetFOServiceStatus( SERVICE_START_PENDING );

    StartServer = true;
    ServerThread.Start( ServerEntry, "Main" );
    while( !Server || !Server->Started() || !Server->Stopped() )
        Thread::Sleep( 10 );

    if( Server->Started() )
        SetFOServiceStatus( SERVICE_RUNNING );
    else
        SetFOServiceStatus( SERVICE_STOPPED );
}

static VOID WINAPI FOServiceCtrlHandler( DWORD opcode )
{
    switch( opcode )
    {
    case SERVICE_CONTROL_STOP:
        SetFOServiceStatus( SERVICE_STOP_PENDING );
        GameOpt.Quit = true;
        ServerThread.Wait();
        SetFOServiceStatus( SERVICE_STOPPED );
        return;
    case SERVICE_CONTROL_INTERROGATE:
        // Fall through to send current status
        break;
    default:
        break;
    }

    // Send current status
    SetFOServiceStatus( 0 );
}

static void SetFOServiceStatus( uint state )
{
    static uint last_state = 0;
    static uint check_point = 0;

    if( !state )
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

    if( state == SERVICE_RUNNING )
        srv_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    if( !( state == SERVICE_RUNNING || state == SERVICE_STOPPED ) )
        srv_status.dwCheckPoint = ++check_point;

    SetServiceStatus( FOServiceStatusHandle, &srv_status );
}

#endif // FO_WINDOWS

/************************************************************************/
/* Linux daemon                                                         */
/************************************************************************/
#ifdef FO_SERVER_DAEMON

# include <sys/stat.h>

static void DaemonLoop();

int main( int argc, char** argv )
{
    InitialSetup( "FOnlineServer", argc, argv );

    if( GameOpt.CommandLine.find( "-nodetach" ) == string::npos )
    {
        // Start daemon
        pid_t parpid = fork();
        if( parpid < 0 )
        {
            WriteLog( "Create child process (fork) fail, error '{}'.", ERRORSTR );
            return 1;
        }
        else if( parpid != 0 )
        {
            // Close parent process
            return 0;
        }

        close( STDIN_FILENO );
        close( STDOUT_FILENO );
        close( STDERR_FILENO );

        if( setsid() < 0 )
        {
            WriteLog( "Generate process index (setsid) fail, error '{}'.\n", ERRORSTR );
            return 1;
        }
    }

    umask( 0 );

    // Stuff
    setlocale( LC_ALL, "Russian" );

    // Threading
    Thread::SetCurrentName( "Daemon" );

    // Memory debugging
    MemoryDebugLevel = MainConfig->GetInt( "", "MemoryDebugLevel", 0 );
    if( MemoryDebugLevel > 2 )
        Debugger::StartTraceMemory();

    // Logging
    LogToFile( "./FOnlineServer.log" );

    // Log version
    WriteLog( "FOnline server daemon, version {}.\n", FONLINE_VERSION );
    if( !GameOpt.CommandLine.empty() )
        WriteLog( "Command line '{}'.\n", GameOpt.CommandLine );

    DaemonLoop(); // Never out from here
    return 0;
}

static void DaemonLoop()
{
    // Autostart server
    StartServer = true;
    ServerThread.Start( ServerEntry, "Main" );

    // Daemon loop
    while( !GameOpt.Quit )
        Thread::Sleep( 100 );
    ServerThread.Wait();
}

#endif // FO_SERVER_DAEMON

/************************************************************************/
/* Admin panel                                                          */
/************************************************************************/

#define MAX_SESSIONS    ( 10 )

struct Session
{
    int           RefCount;
    SOCKET        Sock;
    sockaddr_in   From;
    Thread        WorkThread;
    DateTimeStamp StartWork;
    bool          Authorized;
};
typedef vector< Session* > SessionVec;

static void AdminWork( void* );
static void AdminManager( void* );
static Thread AdminManagerThread;

static void InitAdminManager()
{
    ushort port = MainConfig->GetInt( "", "AdminPanelPort", 0 );
    if( port )
        AdminManagerThread.Start( AdminManager, "AdminPanelManager", new ushort( port ) );
}

static void AdminManager( void* port_ )
{
    ushort port = *(ushort*) port_;
    delete (ushort*) port_;

    // Listen socket
    #ifdef FO_WINDOWS
    WSADATA wsa;
    if( WSAStartup( MAKEWORD( 2, 2 ), &wsa ) )
    {
        WriteLog( "WSAStartup fail on creation listen socket for admin manager.\n" );
        return;
    }
    #endif
    SOCKET listen_sock = socket( AF_INET, SOCK_STREAM, 0 );
    if( listen_sock == INVALID_SOCKET )
    {
        WriteLog( "Can't create listen socket for admin manager.\n" );
        return;
    }
    const int   opt = 1;
    setsockopt( listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*) &opt, sizeof( opt ) );
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons( port );
    sin.sin_addr.s_addr = INADDR_ANY;
    if( ::bind( listen_sock, (sockaddr*) &sin, sizeof( sin ) ) == SOCKET_ERROR )
    {
        WriteLog( "Can't bind listen socket for admin manager.\n" );
        closesocket( listen_sock );
        return;
    }
    if( listen( listen_sock, SOMAXCONN ) == SOCKET_ERROR )
    {
        WriteLog( "Can't listen listen socket for admin manager.\n" );
        closesocket( listen_sock );
        return;
    }

    // Accept clients
    SessionVec sessions;
    while( true )
    {
        // Wait connection
        timeval tv = { 1, 0 };
        fd_set  sock_set;
        FD_ZERO( &sock_set );
        FD_SET( listen_sock, &sock_set );
        if( select( (int) listen_sock + 1, &sock_set, nullptr, nullptr, &tv ) > 0 )
        {
            sockaddr_in from;
            #ifdef FO_WINDOWS
            int         len = sizeof( from );
            #else
            socklen_t   len = sizeof( from );
            #endif
            SOCKET      sock = accept( listen_sock, (sockaddr*) &from, &len );
            if( sock != INVALID_SOCKET )
            {
                // Found already connected from this IP
                bool refuse = false;
                for( auto it = sessions.begin(); it != sessions.end(); ++it )
                {
                    Session* s = *it;
                    if( s->From.sin_addr.s_addr == from.sin_addr.s_addr )
                    {
                        refuse = true;
                        break;
                    }
                }
                if( refuse || sessions.size() > MAX_SESSIONS )
                {
                    shutdown( sock, SD_BOTH );
                    closesocket( sock );
                }

                // Add new session
                if( !refuse )
                {
                    Session* s = new Session();
                    s->RefCount = 2;
                    s->Sock = sock;
                    s->From = from;
                    Timer::GetCurrentDateTime( s->StartWork );
                    s->Authorized = false;
                    s->WorkThread.Start( AdminWork, "AdminPanel", (void*) s );
                    sessions.push_back( s );
                }
            }
        }

        // Manage sessions
        if( !sessions.empty() )
        {
            DateTimeStamp cur_dt;
            Timer::GetCurrentDateTime( cur_dt );
            for( auto it = sessions.begin(); it != sessions.end();)
            {
                Session* s = *it;
                bool     erase = false;

                // Erase closed sessions
                if( s->Sock == INVALID_SOCKET )
                    erase = true;

                // Drop long not authorized connections
                if( !s->Authorized && Timer::GetTimeDifference( cur_dt, s->StartWork ) > 60 ) // 1 minute
                    erase = true;

                // Erase
                if( erase )
                {
                    if( s->Sock != INVALID_SOCKET )
                        shutdown( s->Sock, SD_BOTH );
                    if( --s->RefCount == 0 )
                        delete s;
                    it = sessions.erase( it );
                }
                else
                {
                    ++it;
                }
            }
        }

        // Sleep to prevent panel DDOS or keys brute force
        Thread::Sleep( 1000 );
    }
}

#define ADMIN_PREFIX    "Admin panel ({}): "
#define ADMIN_LOG( format, ... )                                     \
    do {                                                             \
        WriteLog( ADMIN_PREFIX format, admin_name, ## __VA_ARGS__ ); \
    } while( 0 )

/*
   string buf = _str(format, ## __VA_ARGS__);                       \
        uint   buf_len = (uint) buf.length() + 1;                                 \
        if( send( s->Sock, buf.c_str(), buf_len, 0 ) != (int) buf_len )           \
        {                                                                         \
            WriteLog( ADMIN_PREFIX "Send data fail, disconnect.\n", admin_name ); \
            goto label_Finish;                                                    \
        }                                                                         \
 */

static void AdminWork( void* session_ )
{
    // Data
    Session* s = (Session*) session_;
    string   admin_name = "Not authorized";

    // Welcome string
    string welcome = "Welcome to FOnline admin panel.\nEnter access key: ";
    int    welcome_len = (int) welcome.length() + 1;
    if( send( s->Sock, welcome.c_str(), welcome_len, 0 ) != welcome_len )
    {
        WriteLog( "Admin connection first send fail, disconnect.\n" );
        goto label_Finish;
    }

    // Commands loop
    while( true )
    {
        // Get command
        char cmd_raw[ MAX_FOTEXT ];
        memzero( cmd_raw, sizeof( cmd_raw ) );
        int  len = recv( s->Sock, cmd_raw, sizeof( cmd_raw ), 0 );
        if( len <= 0 || len == MAX_FOTEXT )
        {
            if( !len )
                WriteLog( ADMIN_PREFIX "Socket closed, disconnect.\n", admin_name );
            else
                WriteLog( ADMIN_PREFIX "Socket error, disconnect.\n", admin_name );
            goto label_Finish;
        }
        if( len > 200 )
            len = 200;
        cmd_raw[ len ] = 0;
        string cmd = _str( cmd_raw ).trim();

        // Authorization
        if( !s->Authorized )
        {
            StrVec client, tester, moder, admin, admin_names;
            FOServer::GetAccesses( client, tester, moder, admin, admin_names );
            int    pos = -1;
            for( size_t i = 0, j = admin.size(); i < j; i++ )
            {
                if( admin[ i ] == cmd )
                {
                    pos = (int) i;
                    break;
                }
            }
            if( pos != -1 )
            {
                if( pos < (int) admin_names.size() )
                    admin_name = admin_names[ pos ];
                else
                    admin_name = _str( "{}", pos );

                s->Authorized = true;
                ADMIN_LOG( "Authorized for admin '{}', IP '{}'.\n", admin_name, inet_ntoa( s->From.sin_addr ) );
                continue;
            }
            else
            {
                WriteLog( "Wrong access key entered in admin panel from IP '{}', disconnect.\n", inet_ntoa( s->From.sin_addr ) );
                string failstr = "Wrong access key!\n";
                send( s->Sock, failstr.c_str(), (int) failstr.length() + 1, 0 );
                goto label_Finish;
            }
        }

        // Process commands
        if( cmd == "exit" )
        {
            ADMIN_LOG( "Disconnect from admin panel.\n" );
            goto label_Finish;
        }
        else if( cmd == "kill" )
        {
            ADMIN_LOG( "Kill whole process.\n" );
            ExitProcess( 0 );
        }
        else if( _str( cmd ).startsWith( "log " ) )
        {
            if( cmd.substr( 4 ) == "disable" )
            {
                LogToFile( cmd.substr( 4 ) );
                ADMIN_LOG( "Logging to file '{}'.\n", cmd.substr( 4 ) );
            }
            else
            {
                LogToFile( "" );
                ADMIN_LOG( "Logging disabled.\n" );
            }
        }
        else if( cmd == "start" )
        {
            if( Server && Server->Starting() )
                ADMIN_LOG( "Server already starting.\n" );
            else if( Server && Server->Started() )
                ADMIN_LOG( "Server already started.\n" );
            else if( Server && Server->Stopping() )
                ADMIN_LOG( "Server stopping, wait.\n" );
            else
            {
                if( !Server && !StartServer )
                {
                    ADMIN_LOG( "Starting server.\n" );
                    StartServer = true;
                    ServerThread.Start( ServerEntry, "Main" );
                }
                else
                {
                    ADMIN_LOG( "Can't start server more than one time. Restart server process.\n" );
                    #pragma MESSAGE( "Allow multiple server starting in one process session." )
                }
            }
        }
        else if( cmd == "stop" )
        {
            if( Server && Server->Starting() )
                ADMIN_LOG( "Server starting, wait.\n" );
            else if( Server && Server->Stopped() )
                ADMIN_LOG( "Server already stopped.\n" );
            else if( Server && Server->Stopping() )
                ADMIN_LOG( "Server already stopping.\n" );
            else
            {
                if( !GameOpt.Quit )
                {
                    ADMIN_LOG( "Stopping server.\n" );
                    GameOpt.Quit = true;
                }
            }
        }
        else if( cmd == "state" )
        {
            if( Server &&  Server->Starting() )
                ADMIN_LOG( "Server starting.\n" );
            else if( Server && Server->Started() )
                ADMIN_LOG( "Server started.\n" );
            else if( Server &&  Server->Stopping() )
                ADMIN_LOG( "Server stopping.\n" );
            else if( Server && Server->Stopped() )
                ADMIN_LOG( "Server stopped.\n" );
            else
                ADMIN_LOG( "Unknown state.\n" );
        }
        else if( !cmd.empty() && cmd[ 0 ] == '~' )
        {
            if( Server && Server->Started() )
            {
                bool    send_fail = false;
                LogFunc func = [ &admin_name, &s, &send_fail ] ( const string &str )
                {
                    string buf = str;
                    if( buf.empty() || buf.back() != '\n' )
                        buf += "\n";

                    if( !send_fail && send( s->Sock, buf.c_str(), (int) buf.length() + 1, 0 ) != (int) buf.length() + 1 )
                    {
                        WriteLog( ADMIN_PREFIX "Send data fail, disconnect.\n", admin_name );
                        send_fail = true;
                    }
                };

                NetBuffer buf;
                PackNetCommand( cmd.substr( 1 ), &buf, func, "" );
                if( !buf.IsEmpty() )
                {
                    uint msg;
                    buf >> msg;
                    WriteLog( ADMIN_PREFIX "Execute command '{}'.\n", admin_name, cmd );
                    Server->Process_Command( buf, func, nullptr, admin_name );
                }

                if( send_fail )
                    goto label_Finish;
            }
            else
            {
                ADMIN_LOG( "Can't run command for not started server.\n" );
            }
        }
        else if( !cmd.empty() )
        {
            ADMIN_LOG( "Unknown command '{}'.\n", cmd );
        }
    }

label_Finish:
    shutdown( s->Sock, SD_BOTH );
    closesocket( s->Sock );
    s->Sock = INVALID_SOCKET;
    if( --s->RefCount == 0 )
        delete s;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
