#include "Common.h"
#include "Mapper.h"
#include "Exception.h"
#include <locale.h>

int main( int argc, char** argv )
{
    RestoreMainDirectory();

    // Threading
    Thread::SetCurrentName( "GUI" );

    // Exceptions
    CatchExceptions( "FOnlineMapper", FONLINE_VERSION );

    // Make command line
    SetCommandLine( argc, argv );

    // Timer
    Timer::Init();

    // Logging
    LogToFile( "FOMapper.log" );

    // Data files
    FileManager::InitDataFiles( DIR_SLASH_SD "data" DIR_SLASH_S );

    // Options
    GetClientOptions();

    // Start
    WriteLog( "Starting Mapper (version %d)...\n", FONLINE_VERSION );

    // Create engine
    FOMapper* mapper = new FOMapper();
    if( !mapper || !mapper->Init() )
    {
        WriteLog( "FOnline engine initialization fail.\n" );
        GameOpt.Quit = true;
        return 0;
    }

    // Loop
    while( !GameOpt.Quit )
        mapper->MainLoop();

    // Finish script
    if( Script::PrepareContext( MapperFunctions.Finish, _FUNC_, "Game" ) )
        Script::RunPrepared();

    // Just kill process
    // System automatically clean up all resources
    WriteLog( "Exit from mapper.\n" );
    ExitProcess( 0 );

    // Destroy engine
    mapper->Finish();
    SAFEDEL( mapper );

    WriteLog( "FOnline finished.\n" );
    LogFinish();

    return 0;
}

#ifdef FO_WINDOWS
int CALLBACK WinMain( HINSTANCE instance, HINSTANCE prev_instance, char* cmd_line, int cmd_show )
{
    char** argv = new char*[ 1 ];
    argv[ 0 ] = cmd_line;
    return main( 1, argv );
}
#endif
