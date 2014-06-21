#include "StdAfx.h"
#include "Client.h"
#include "Exception.h"
#include "Version.h"
#include "Keyboard.h"
#include <locale.h>
#ifndef FO_WINDOWS
# include <signal.h>
#endif

extern "C" int main( int argc, char** argv ) // Handled by SDL
{
    RestoreMainDirectory();

    // Threading
    #ifdef FO_WINDOWS
    pthread_win32_process_attach_np();
    #endif
    Thread::SetCurrentName( "Main" );

    // Disable SIGPIPE signal
    #ifndef FO_WINDOWS
    signal( SIGPIPE, SIG_IGN );
    #endif

    // Exception
    CatchExceptions( "FOnline", FONLINE_VERSION );

    // Make command line
    SetCommandLine( argc, argv );

    // Timer
    Timer::Init();

    // Logging
    LogToFile( "FOnline.log" );
    LogToDebugOutput( true );

    // Singleplayer mode initialization
    #ifdef FO_WINDOWS
    char full_path[ MAX_FOPATH ] = { 0 };
    char path[ MAX_FOPATH ] = { 0 };
    char name[ MAX_FOPATH ] = { 0 };
    GetModuleFileName( NULL, full_path, MAX_FOPATH );
    FileManager::ExtractPath( full_path, path );
    FileManager::ExtractFileName( full_path, name );
    if( Str::Substring( name, "Singleplayer" ) || Str::Substring( CommandLine, "Singleplayer" ) )
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
        IniParser cfg;
        char      server_exe[ MAX_FOPATH ] = { 0 };
        char      server_path[ MAX_FOPATH ] = { 0 };
        char      server_cmdline[ MAX_FOPATH ] = { 0 };
        cfg.LoadFile( GetConfigFileName(), PT_ROOT );
        cfg.GetStr( CLIENT_CONFIG_APP, "ServerAppName", "FOnlineServer.exe", server_exe );
        cfg.GetStr( CLIENT_CONFIG_APP, "ServerPath", "..\\Server\\", server_path );
        cfg.GetStr( CLIENT_CONFIG_APP, "ServerCommandLine", "", server_cmdline );

        // Process attributes
        PROCESS_INFORMATION server;
        memzero( &server, sizeof( server ) );
        STARTUPINFOA        sui;
        memzero( &sui, sizeof( sui ) );
        sui.cb = sizeof( sui );
        HANDLE client_process = OpenProcess( SYNCHRONIZE, TRUE, GetCurrentProcessId() );
        char   command_line[ 2048 ];

        // Start server
        Str::Format( command_line, "\"%s%s\" -singleplayer %p %p %s -logpath %s", server_path, server_exe, map_file, client_process, server_cmdline, path );
        if( !CreateProcess( NULL, command_line, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, server_path, &sui, &server ) )
        {
            WriteLog( "Can't start server process, error<%u>.\n", GetLastError() );
            return 0;
        }
        CloseHandle( server.hProcess );
        CloseHandle( server.hThread );
    }
    #endif

    // Check for already runned window
    #ifndef DEV_VERSION
    # ifdef FO_WINDOWS
    HANDLE h = CreateEvent( NULL, FALSE, FALSE, "fonline_instance" );
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
    WriteLog( "Starting FOnline (version %d)...\n", FONLINE_VERSION );

    // Create engine
    FOClient* engine = new FOClient();
    if( !engine->Init() )
    {
        WriteLog( "FOnline engine initialization fail.\n" );
        ExitProcess( -1 );
    }

    // iOS loop
    #ifdef FO_OSX_IOS
    struct App
    {
        static void ShowFrame( void* engine )
        {
            ( (FOClient*) engine )->MainLoop();
        }
    };
    SDL_iPhoneSetAnimationCallback( MainWindow, 1, App::ShowFrame, engine );
    return 0;
    #endif

    // Loop
    while( !GameOpt.Quit )
    {
        if( !engine->MainLoop() )
            Thread::Sleep( 100 );
    }

    // Finish script
    if( Script::PrepareContext( ClientFunctions.Finish, _FUNC_, "Game" ) )
        Script::RunPrepared();

    // Just kill process
    // System automatically clean up all resources
    WriteLog( "Exit from game.\n" );
    ExitProcess( 0 );

    // Destroy engine
    engine->Finish();
    SAFEDEL( engine );

    // Finish
    #ifdef FO_WINDOWS
    if( Singleplayer )
        SingleplayerData.Finish();
    #endif
    WriteLog( "FOnline finished.\n" );
    LogFinish();
    Timer::Finish();

    return 0;
}
