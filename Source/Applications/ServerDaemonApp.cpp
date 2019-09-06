#include "Common.h"
#include "Testing.h"
#include "Server.h"

#if defined ( FO_LINUX ) || defined ( FO_MAC )
# include <sys/stat.h>
#endif

#ifndef FO_TESTING
int main( int argc, char** argv )
#else
static int main_disabled( int argc, char** argv )
#endif
{
    Thread::SetCurrentName( "ServerYoungDaemon" );
    LogToFile( "FOnlineServerDaemon.log" );
    InitialSetup( "FOnlineServerDaemon", argc, argv );

    #if defined ( FO_LINUX ) || defined ( FO_MAC )
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
        WriteLog( "Generate process index (setsid) failed, error '{}'.\n", ERRORSTR );
        return 1;
    }

    umask( 0 );

    #else
    RUNTIME_ASSERT( !"Invalid OS" );
    #endif

    Thread::SetCurrentName( "ServerDaemon" );

    FOServer server;
    server.Run();
    return server.Run();
}
