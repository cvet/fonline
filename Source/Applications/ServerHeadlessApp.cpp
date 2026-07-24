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

FO_BEGIN_NAMESPACE
extern void ClientStartupSettingsHook(GlobalSettings& settings, int32_t client_index, bool embedded);
FO_END_NAMESPACE

static void ServerWithClientsLoop(ptr<ServerEngine> server, vector<unique_ptr<GlobalSettings>>& client_settings, vector<refcount_ptr<ClientEngine>>& clients);

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ServerHeadlessApp(CommandLineArgs args) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

#if !FO_TESTING_APP
    CommandLineArgs args {numeric_cast<int32_t>(argc), argv};
#endif

    try {
        InitApp(args, AppInitFlags::PrebakeResources);

        {
            auto server_settings = make_ptr(&GetApp()->Settings);
            auto server = SafeAlloc::MakeRefCounted<ServerEngine>(server_settings, GetServerResources(*server_settings));
            vector<unique_ptr<GlobalSettings>> client_settings;
            vector<refcount_ptr<ClientEngine>> clients;

            if (GetApp()->Settings.AutoStartClientOnServer != 0) {
                ServerWithClientsLoop(server, client_settings, clients);
            }
            else {
                while (!GetApp()->IsQuitRequested() && !server->IsStartingError()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds {10});
                }
            }

            if (server->IsStartingError()) {
                WriteLog(LogType::Error, "Server startup failed, shutting down");
                GetApp()->RequestQuit(false);
            }

            for (auto& client : std::exchange(clients, {})) {
                client->Shutdown();
            }

            client_settings.clear();
            server->Shutdown();
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

static void ServerWithClientsLoop(ptr<ServerEngine> server, vector<unique_ptr<GlobalSettings>>& client_settings, vector<refcount_ptr<ClientEngine>>& clients)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Auto start embedded headless client(s): {}", GetApp()->Settings.AutoStartClientOnServer);

    FrameBalancer balancer {false, 0, 100}; // 100 fps

    while (!GetApp()->IsQuitRequested() && !server->IsStartingError()) {
        balancer.StartLoop();
        GetApp()->BeginFrame();

        int32_t target_client_count = std::max(GetApp()->Settings.AutoStartClientOnServer, 0);

        while (server->IsStarted() && clients.size() < numeric_cast<size_t>(target_client_count)) {
            int32_t client_index = numeric_cast<int32_t>(clients.size()) + 1;
            auto settings = SafeAlloc::MakeUnique<GlobalSettings>(false);
            settings->CopyFrom(GetApp()->Settings);
            ClientStartupSettingsHook(*settings, client_index, true);

            ptr<GlobalSettings> settings_ptr = settings.get();
            auto client = SafeAlloc::MakeRefCounted<ClientEngine>(settings_ptr, GetClientResources(*settings), &GetApp()->MainWindow);
            client->Connect();
            client_settings.emplace_back(std::move(settings));
            clients.emplace_back(std::move(client));
        }

        for (auto& client : clients) {
            client->MainLoop();
        }

        GetApp()->EndFrame();
        balancer.EndLoop();
    }
}
