// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "AdminPanelServer.h"
#include "Application.h"
#include "Server.h"
#include "Settings.h"

FO_USING_NAMESPACE();

static constexpr std::chrono::milliseconds SERVER_STARTUP_POLL_INTERVAL {10};

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ServerDaemonApp(CommandLineArgs args) -> int
#endif
{
#if !FO_TESTING_APP
    ProgramArgs program_args {numeric_cast<int32_t>(argc), argv};
    CommandLineArgs args {program_args.GetArgs()};
#endif

    try {
        (void)platform::fork_process();

        InitApp(args, AppInitFlags::PrebakeResources);

        {
            auto settings {make_ptr(&GetApp()->Settings)};
            auto server {safe_alloc::make_refcounted<ServerEngine>(settings, GetServerResources(*settings))};

            auto start_server {[&server] {
                if (!server) {
                    auto new_settings {make_ptr(&GetApp()->Settings)};
                    server = safe_alloc::make_refcounted<ServerEngine>(new_settings, GetServerResources(*new_settings));
                }
            }};

            auto stop_server {[&server] {
                if (server) {
                    server->Shutdown();
                    server.reset();
                }
            }};

            auto restart_server {[&start_server, &stop_server] {
                stop_server();
                start_server();
            }};

            AdminServerHost admin_host {"ServerDaemonAdminHost",
                AdminServerHostCallbacks {
                    .GetServer = [&server]() -> nptr<ServerEngine> { return server; },
                    .StartServer = start_server,
                    .StopServer = stop_server,
                    .RestartServer = restart_server,
                }};

            while (!GetApp()->IsQuitRequested() && (!server || !server->IsStartingError())) {
                admin_host.Tick();
                coarse_sleep(SERVER_STARTUP_POLL_INTERVAL);
            }

            if (server && server->IsStartingError()) {
                logging::write(logging::type::error, "Server startup failed, shutting down");
                GetApp()->RequestQuit(false);
            }

            if (server) {
                server->Shutdown();
            }
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
