#include "StdAfx.h"
#include "Mapper.h"
#include "Exception.h"
#include "Version.h"
#include <locale.h>

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
