// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Application.h"
#include "Client.h"
#include "Server.h"
#include "Settings.h"

FO_USING_NAMESPACE();

static constexpr std::chrono::milliseconds SERVER_STARTUP_POLL_INTERVAL {10};

FO_BEGIN_NAMESPACE
void ClientStartupSettingsHook(GlobalSettings& settings, int32_t client_index, bool embedded);
FO_END_NAMESPACE

static void ServerWithClientsLoop(ptr<ServerEngine> server, vector<unique_ptr<GlobalSettings>>& client_settings, vector<refcount_ptr<ClientEngine>>& clients);

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ServerHeadlessApp(CommandLineArgs args) -> int
#endif
{
#if !FO_TESTING_APP
    ProgramArgs program_args {numeric_cast<int32_t>(argc), argv};
    CommandLineArgs args {program_args.GetArgs()};
#endif

    try {
        InitApp(args, AppInitFlags::PrebakeResources);

        {
            auto server_settings {make_ptr(&GetApp()->Settings)};
            auto server {safe_alloc::make_refcounted<ServerEngine>(server_settings, GetServerResources(*server_settings))};
            vector<unique_ptr<GlobalSettings>> client_settings {};
            vector<refcount_ptr<ClientEngine>> clients {};

            if (GetApp()->Settings.Server.AutoStartClientOnServer != 0) {
                ServerWithClientsLoop(server, client_settings, clients);
            }
            else {
                while (!GetApp()->IsQuitRequested() && !server->IsStartingError()) {
                    coarse_sleep(SERVER_STARTUP_POLL_INTERVAL);
                }
            }

            if (server->IsStartingError()) {
                logging::write(logging::type::error, "Server startup failed, shutting down");
                GetApp()->RequestQuit(false);
            }

            for (auto& client : std::exchange(clients, {})) {
                client->Shutdown();
            }

            client_settings.clear();
            server->Shutdown();
        }

        exit_app(GetApp()->GetRequestedQuitSuccess());
    }
    catch (const std::exception& ex) {
        exceptions::report_and_exit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

static void ServerWithClientsLoop(ptr<ServerEngine> server, vector<unique_ptr<GlobalSettings>>& client_settings, vector<refcount_ptr<ClientEngine>>& clients)
{
    logging::write("Auto start embedded headless client(s): {}", GetApp()->Settings.Server.AutoStartClientOnServer);

    FrameBalancer balancer {false, 0, 100}; // 100 fps

    while (!GetApp()->IsQuitRequested() && !server->IsStartingError()) {
        balancer.StartLoop();
        GetApp()->BeginFrame();

        int32_t target_client_count {std::max(GetApp()->Settings.Server.AutoStartClientOnServer, 0)};

        while (server->IsStarted() && clients.size() < numeric_cast<size_t>(target_client_count)) {
            int32_t client_index {numeric_cast<int32_t>(clients.size()) + 1};
            auto settings {safe_alloc::make_unique<GlobalSettings>(false)};
            settings->CopyFrom(GetApp()->Settings);

            if (settings->Common.Packaged) {
                settings->SetSettingValue(SettingNames::Common_ResourcesPath, strex(settings->Common.ResourcesPath).combine_path(CLIENT_RESOURCE_PACKS_SUBDIR).str());
            }

            ClientStartupSettingsHook(*settings, client_index, true);

            ptr<GlobalSettings> settings_ptr {settings.get()};
            auto client {safe_alloc::make_refcounted<ClientEngine>(settings_ptr, GetClientResources(*settings), &GetApp()->MainWindow)};
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
