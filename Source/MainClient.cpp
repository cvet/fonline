#include "Common.h"
#include "Client.h"
#include "Exception.h"
#include "Keyboard.h"
#include <locale.h>
#ifndef FO_WINDOWS
# include <signal.h>
#endif
#ifdef FO_IOS
extern SDL_Window* SprMngr_MainWindow;
#endif

static void ClientEntry( void* )
{
    static FOClient* client = nullptr;
    if( !client )
    {
        #ifdef FO_WEB
        // Wait file system synchronization
        if( EM_ASM_INT( return Module.syncfsDone ) != 1 )
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

    // Hard restart, need wait before event dissapeared
    #ifdef FO_WINDOWS
    if( wcsstr( GetCommandLineW(), L" --restart" ) )
        Thread_Sleep( 500 );
    #endif

    // Options
    GetClientOptions();

    // Start message
    WriteLog( "Starting FOnline (version {})...\n", FONLINE_VERSION );

    // Loop
    #if defined ( FO_IOS )
    ClientEntry( nullptr );
    SDL_iPhoneSetAnimationCallback( SprMngr_MainWindow, 1, ClientEntry, nullptr );
    return 0;

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
