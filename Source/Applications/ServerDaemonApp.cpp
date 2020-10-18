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

#if defined(FO_LINUX) || defined(FO_MAC)
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef FO_TESTING
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ServerDaemonApp(int argc, char** argv) -> int
#endif
{
    try {
        SetAppName("FOnlineServerDaemon");
        CatchSystemExceptions();
        CreateGlobalData();
        LogToFile();

#if defined(FO_LINUX) || defined(FO_MAC)
        // Start daemon
        pid_t parpid = ::fork();
        if (parpid < 0) {
            throw GenericException("Create child process (fork) failed", strerror(errno));
        }
        else if (parpid != 0) {
            // Close parent process
            return 0;
        }

        ::close(STDIN_FILENO);
        ::close(STDOUT_FILENO);
        ::close(STDERR_FILENO);

        if (::setsid() < 0) {
            throw GenericException("Create child process (fork) failed", strerror(errno));
        }

        ::umask(0);
#endif

        auto settings = GlobalSettings(argc, argv);
        auto* server = new FOServer(settings);

        while (settings.Quit) {
            try {
                server->MainLoop();
            }
            catch (const GenericException& ex) {
                ReportExceptionAndContinue(ex);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }

        delete server;
        return 0;
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
