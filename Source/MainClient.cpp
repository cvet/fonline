#include "Common.h"
#include "Client.h"
#include "Exception.h"
#include "Keyboard.h"
#include <locale.h>
#ifndef FO_WINDOWS
# include <signal.h>
#endif

static void ClientEntry( void* )
{
    static FOClient* client = nullptr;
    if( !client )
    {
        #ifdef FO_WEB
        // Wait file system synchronization
        if( emscripten_run_script_int( "Module.syncfsDone" ) != 1 )
            return;
        #endif

        client = new FOClient();
    }
    client->MainLoop();
}

extern "C" int main( int argc, char** argv ) // Handled by SDL
{
    InitialSetup( argc, argv );

    // Disable SIGPIPE signal
    #ifndef FO_WINDOWS
    signal( SIGPIPE, SIG_IGN );
    #endif

    // Exception
    CatchExceptions( "FOnline", FONLINE_VERSION );

    // Timer
    Timer::Init();

    // Logging
    LogToFile( "FOnline.log" );

    // Singleplayer mode initialization
    #ifdef FO_WINDOWS
    wchar_t full_path[ MAX_FOPATH ] = { 0 };
    char    path[ MAX_FOPATH ] = { 0 };
    char    name[ MAX_FOPATH ] = { 0 };
    GetModuleFileNameW( nullptr, full_path, MAX_FOPATH );
    FileManager::ExtractDir( WideCharToChar( full_path ).c_str(), path );
    FileManager::ExtractFileName( WideCharToChar( full_path ).c_str(), name );
    if( Str::Substring( name, "Singleplayer" ) || MainConfig->IsKey( "", "Singleplayer" ) )
    {
        WriteLog( "Singleplayer mode.\n" );
        Singleplayer = true;
        GameOpt.Singleplayer = true;
        Timer::SetGamePause( true );

        // Create interprocess shared data
        HANDLE map_file = SingleplayerData.Init();
        if( !map_file )
        {
            WriteLog( "Can't map shared data to memory.\n" );
            return 0;
        }

        // Fill interprocess initial data
        if( SingleplayerData.Lock() )
        {
            // Initialize other data
            SingleplayerData.NetPort = 0;
            SingleplayerData.Pause = true;

            SingleplayerData.Unlock();
        }
        else
        {
            WriteLog( "Can't lock mapped file.\n" );
            return 0;
        }

        // Config parsing
        const char* server_exe = "FOnlineServer.exe";
        const char* server_dir = MainConfig->GetStr( "", "ServerDir", "" );
        const char* server_cmdline = MainConfig->GetStr( "", "ServerCommandLine", "" );

        // Process attributes
        PROCESS_INFORMATION server;
        memzero( &server, sizeof( server ) );
        STARTUPINFOW        sui;
        memzero( &sui, sizeof( sui ) );
        sui.cb = sizeof( sui );
        HANDLE  client_process = OpenProcess( SYNCHRONIZE, TRUE, GetCurrentProcessId() );

        wchar_t command_line[ 2048 ];
        wcscpy( command_line, CharToWideChar( fmt::format( "\"{}{}\" -singleplayer {} {} {} -logpath {}", server_dir, server_exe,
                                                           (void*) map_file, (void*) client_process, server_cmdline, path ) ).c_str() );

        // Start server
        if( !CreateProcessW( nullptr, command_line, nullptr, nullptr, TRUE,
                             NORMAL_PRIORITY_CLASS, nullptr, CharToWideChar( server_dir ).c_str(), &sui, &server ) )
        {
            WriteLog( "Can't start server process, error {}.\n", GetLastError() );
            return 0;
        }
        CloseHandle( server.hProcess );
        CloseHandle( server.hThread );
    }
    #endif

    // Hard restart, need wait before event dissapeared
    #ifdef FO_WINDOWS
    if( wcsstr( GetCommandLineW(), L" --restart" ) )
        Thread_Sleep( 500 );
    #endif

    // Check for already runned window
    #ifndef DISABLE_UIDS
    # ifdef FO_WINDOWS
    HANDLE h = CreateEventW( nullptr, FALSE, FALSE, L"fonline_instance" );
    if( !h || h == INVALID_HANDLE_VALUE || GetLastError() == ERROR_ALREADY_EXISTS )
    {
        ShowMessage( "FOnline engine instance is already running." );
        return 0;
    }
    # else
    // Todo: Linux
    # endif
    #endif

    // Options
    GetClientOptions();

    // Start message
    WriteLog( "Starting FOnline (version {})...\n", FONLINE_VERSION );

    // Loop
    #if defined ( FO_IOS )
    SDL_iPhoneSetAnimationCallback( MainWindow, 1, ClientEntry, nullptr );

    #elif defined ( FO_WEB )
    EM_ASM(
        FS.mkdir( '/PersistentData' );
        FS.mount( IDBFS, {}, '/PersistentData' );
        Module.syncfsDone = 0;
        FS.syncfs( true, function( err )
                   {
                       assert( !err );
                       Module.syncfsDone = 1;
                   } );
        );
    emscripten_set_main_loop_arg( ClientEntry, nullptr, 0, 1 );

    #elif defined ( FO_ANDROID )
    while( !GameOpt.Quit )
        ClientEntry( nullptr );

    #else
    while( !GameOpt.Quit )
    {
        double start_loop = Timer::AccurateTick();

        ClientEntry( nullptr );

        if( !GameOpt.VSync && GameOpt.FixedFPS )
        {
            if( GameOpt.FixedFPS > 0 )
            {
                static double balance = 0.0;
                double        elapsed = Timer::AccurateTick() - start_loop;
                double        need_elapsed = 1000.0 / (double) GameOpt.FixedFPS;
                if( need_elapsed > elapsed )
                {
                    double sleep = need_elapsed - elapsed + balance;
                    balance = fmod ( sleep, 1.0 );
                    Thread_Sleep( (uint) floor( sleep ) );
                }
            }
            else
            {
                Thread_Sleep( -GameOpt.FixedFPS );
            }
        }
    }
    #endif

    // Finish script
    if( Script::GetEngine() )
    {
        Script::RunMandatorySuspended();
        Script::RaiseInternalEvent( ClientFunctions.Finish, _FUNC_, "Game" );
    }

    // Memory stats
    #ifdef MEMORY_DEBUG
    WriteLog( "{}", Debugger::GetMemoryStatistics() );
    #endif

    // Just kill process
    // System automatically clean up all resources
    WriteLog( "Exit from game.\n" );

    return 0;
}
