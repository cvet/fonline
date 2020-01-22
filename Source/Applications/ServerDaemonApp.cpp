#include "Common.h"

#include "Server.h"
#include "Testing.h"
#include "Version_Include.h"

#if defined(FO_LINUX) || defined(FO_MAC)
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static GlobalSettings Settings {};

#ifndef FO_TESTING
int main(int argc, char** argv)
#else
static int main_disabled(int argc, char** argv)
#endif
{
    CatchExceptions("FOnlineServerDaemon", FO_VERSION);
    LogToFile("FOnlineServerDaemon.log");
    Settings.ParseArgs(argc, argv);

#if defined(FO_LINUX) || defined(FO_MAC)
    // Start daemon
    pid_t parpid = fork();
    if (parpid < 0)
    {
        WriteLog("Create child process (fork) fail, error '{}'.", strerror(errno));
        return 1;
    }
    else if (parpid != 0)
    {
        // Close parent process
        return 0;
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    if (setsid() < 0)
    {
        WriteLog("Generate process index (setsid) failed, error '{}'.\n", strerror(errno));
        return 1;
    }

    umask(0);

#else
    RUNTIME_ASSERT(!"Invalid OS");
#endif

    FOServer server(Settings);
    server.Run();
    return server.Run();
}
