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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Application.h"
#include "Client.h"
#include "Server.h"
#include "Settings.h"

FO_USING_NAMESPACE();

static void ServerWithClientLoop(ServerEngine* server, refcount_ptr<ClientEngine>& client);

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ServerHeadlessApp(int argc, char** argv) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

    try {
        InitApp(numeric_cast<int32>(argc), argv, AppInitFlags::PrebakeResources);

        {
            auto server = SafeAlloc::MakeRefCounted<ServerEngine>(App->Settings, GetServerResources(App->Settings));
            refcount_ptr<ClientEngine> client;

            if (App->Settings.AutoStartClientOnServer) {
                ServerWithClientLoop(server.get(), client);
            }
            else {
                App->WaitForRequestedQuit();
            }

            if (client) {
                client->Shutdown();
                client.reset();
            }

            server->Shutdown();
        }

        ExitApp(App->GetRequestedQuitSuccess());
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

static void ServerWithClientLoop(ServerEngine* server, refcount_ptr<ClientEngine>& client)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(server);

    WriteLog("Auto start embedded headless client");

    FrameBalancer balancer {false, 0, 100}; // 100 fps

    while (!App->IsQuitRequested()) {
        balancer.StartLoop();
        App->BeginFrame();

        if (server->IsStarted() && !client) {
            client = SafeAlloc::MakeRefCounted<ClientEngine>(App->Settings, GetClientResources(App->Settings), App->MainWindow);
        }

        if (client) {
            client->MainLoop();
        }

        App->EndFrame();
        balancer.EndLoop();
    }
}
