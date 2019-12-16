#include "Client.h"
#include "Common.h"
#include "Debugger.h"
#include "Keyboard.h"
#include "Log.h"
#include "SDL_main.h"
#include "Settings.h"
#include "Testing.h"
#include "Timer.h"
#include "Version_Include.h"

#ifdef FO_IOS
extern SDL_Window* SprMngr_MainWindow;
#endif

static void ClientEntry(void*)
{
    static FOClient* client = nullptr;
    if (!client)
    {
#ifdef FO_WEB
        // Wait file system synchronization
        if (EM_ASM_INT(return Module.syncfsDone) != 1)
            return;
#endif

        client = new FOClient();
    }
    client->MainLoop();
}

#ifndef FO_TESTING
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
static int main_disabled(int argc, char** argv)
#endif
{
    InitialSetup("FOnline", argc, argv);

    // Logging
    LogToFile("FOnline.log");

// Hard restart, need wait before event dissapeared
#ifdef FO_WINDOWS
    if (wcsstr(GetCommandLineW(), L" --restart"))
        Thread::Sleep(500);
#endif

    // Start message
    WriteLog("Starting FOnline ({:#x})...\n", FO_VERSION);

// Loop
#if defined(FO_IOS)
    ClientEntry(nullptr);
    SDL_iPhoneSetAnimationCallback(SprMngr_MainWindow, 1, ClientEntry, nullptr);
    return 0;

#elif defined(FO_WEB)
    EM_ASM(FS.mkdir('/PersistentData'); FS.mount(IDBFS, {}, '/PersistentData'); Module.syncfsDone = 0; FS.syncfs(
        true, function(err) {
            assert(!err);
            Module.syncfsDone = 1;
        }););
    emscripten_set_main_loop_arg(ClientEntry, nullptr, 0, 1);

#elif defined(FO_ANDROID)
    while (!GameOpt.Quit)
        ClientEntry(nullptr);

#else
    while (!GameOpt.Quit)
    {
        double start_loop = Timer::AccurateTick();

        ClientEntry(nullptr);

        if (!GameOpt.VSync && GameOpt.FixedFPS)
        {
            if (GameOpt.FixedFPS > 0)
            {
                static double balance = 0.0;
                double elapsed = Timer::AccurateTick() - start_loop;
                double need_elapsed = 1000.0 / (double)GameOpt.FixedFPS;
                if (need_elapsed > elapsed)
                {
                    double sleep = need_elapsed - elapsed + balance;
                    balance = fmod(sleep, 1.0);
                    Thread::Sleep((uint)floor(sleep));
                }
            }
            else
            {
                Thread::Sleep(-GameOpt.FixedFPS);
            }
        }
    }
#endif

    // Finish script
    if (Script::GetEngine())
    {
        Script::RunMandatorySuspended();
        Script::RaiseInternalEvent(ClientFunctions.Finish);
    }

    // Memory stats
    if (MemoryDebugLevel > 1)
        WriteLog("{}", Debugger::GetMemoryStatistics());

    // Just kill process
    // System automatically clean up all resources
    WriteLog("Exit from game.\n");

    return 0;
}
