// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

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

            while (!GetApp()->IsQuitRequested() && !server->IsStartingError()) {
                coarse_sleep(SERVER_STARTUP_POLL_INTERVAL);
            }

            if (server->IsStartingError()) {
                logging::write(logging::type::error, "Server startup failed, shutting down");
                GetApp()->RequestQuit(false);
            }

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
