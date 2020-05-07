//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "Common.h"

#include "Server.h"
#include "Settings.h"
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
    while (Settings.Quit)
        server.MainLoop();
    return server.Run();
}
