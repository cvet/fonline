//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "AdminPanelServer.h"
#include "Application.h"
#include "Server.h"
#include "Settings.h"

FO_USING_NAMESPACE();

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ServerDaemonApp(CommandLineArgs args) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

#if !FO_TESTING_APP
    CommandLineArgs args {numeric_cast<int32_t>(argc), argv};
#endif

    try {
        Platform::ForkProcess();

        InitApp(args, AppInitFlags::PrebakeResources);

        {
            auto settings = make_ptr(&GetApp()->Settings);
            refcount_nptr<ServerEngine> server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, GetServerResources(*settings));
            AdminServerHost admin_host("ServerDaemonAdminHost",
                AdminServerHostCallbacks {
                    .GetServer = [&server]() -> ServerEngine* { return server.get(); },
                    .StartServer =
                        [&server]() {
                            if (!server) {
                                auto settings = make_ptr(&GetApp()->Settings);
                                server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, GetServerResources(*settings));
                            }
                        },
                    .StopServer =
                        [&server]() {
                            if (server) {
                                server->Shutdown();
                                server.reset();
                            }
                        },
                    .RestartServer =
                        [&server]() {
                            if (server) {
                                server->Shutdown();
                                server.reset();
                            }

                            auto settings = make_ptr(&GetApp()->Settings);
                            server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, GetServerResources(*settings));
                        },
                });

            while (!GetApp()->IsQuitRequested() && (!server || !server->IsStartingError())) {
                admin_host.Tick();
                std::this_thread::sleep_for(std::chrono::milliseconds {10});
            }

            if (server && server->IsStartingError()) {
                WriteLog(LogType::Error, "Server startup failed, shutting down");
                GetApp()->RequestQuit(false);
            }

            if (server) {
                server->Shutdown();
            }
        }

        ExitApp(GetApp()->GetRequestedQuitSuccess());
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}
