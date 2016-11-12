#include "Common.h"
#include "Mapper.h"
#include "Exception.h"
#include <locale.h>

int main( int argc, char** argv )
{
    InitialSetup( argc, argv );

    // Threading
    Thread::SetCurrentName( "GUI" );

    // Exceptions
    CatchExceptions( "FOnlineMapper", FONLINE_VERSION );

    // Timer
    Timer::Init();

    // Logging
    LogToFile( "FOMapper.log" );

    // Data files
    FileManager::InitDataFiles( "./" );

    // Options
    GetClientOptions();

    // Start
    WriteLog( "Starting Mapper (version {})...\n", FONLINE_VERSION );

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
    Script::RunMandatorySuspended();
    Script::RaiseInternalEvent( MapperFunctions.Finish );

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
