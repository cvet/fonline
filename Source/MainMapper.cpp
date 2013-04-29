#include "StdAfx.h"
#include "Mapper.h"
#include "Exception.h"
#include "Version.h"
#include <locale.h>

FOWindow* MainWindow = NULL;
FOMapper* Mapper = NULL;

int main( int argc, char** argv )
{
    setlocale( LC_ALL, "Russian" );
    RestoreMainDirectory();

    // Threading
    #ifdef FO_WINDOWS
    pthread_win32_process_attach_np();
    #endif
    Thread::SetCurrentName( "GUI" );

    // Exceptions
    CatchExceptions( "FOnlineMapper", MAPPER_VERSION );

    // Make command line
    SetCommandLine( argc, argv );

    // Timer
    Timer::Init();

    LogToFile( "FOMapper.log" );

    GetClientOptions();

    WriteLog( "Starting Mapper (%s)...\n", MAPPER_VERSION_STR );

    // Create window
    MainWindow = new FOWindow();

    // Create engine
    Mapper = new FOMapper();
    if( !Mapper || !Mapper->Init() )
    {
        WriteLog( "FOnline engine initialization fail.\n" );
        GameOpt.Quit = true;
        return 0;
    }

    // Loop
    while( !GameOpt.Quit && Fl::check() )
        Mapper->MainLoop();
    GameOpt.Quit = true;

    // Destroy engine
    Mapper->Finish();
    SAFEDEL( Mapper );

    // Finish
    #ifdef FO_WINDOWS
    if( Singleplayer )
        SingleplayerData.Finish();
    #endif
    WriteLog( "FOnline finished.\n" );
    LogFinish();

    return 0;
}
