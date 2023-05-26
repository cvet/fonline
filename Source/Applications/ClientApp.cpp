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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Log.h"
#include "Settings.h"
#include "Timer.h"
#include "Updater.h"
#include "Version-Include.h"

#if !FO_TESTING_APP
#include "SDL_main.h"
#endif

struct ClientAppData
{
    FOClient* Client {};
    bool ResourcesSynced {};
    Updater* ResourceUpdater {};
};
GLOBAL_DATA(ClientAppData, Data);

static void MainEntry(void*)
{
    STACK_TRACE_ENTRY();

    try {
#if FO_WEB
        // Wait file system synchronization
        if (EM_ASM_INT(return Module.syncfsDone) != 1) {
            return;
        }
#endif

        App->BeginFrame();

        // Synchronize files and start client
        if (Data->Client == nullptr) {
            try {
                // Synchronize files
                if (!Data->ResourcesSynced) {
                    if (!App->Settings.DataSynchronization) {
                        Data->ResourcesSynced = true;
                        App->EndFrame();
                        return;
                    }

                    if (Data->ResourceUpdater == nullptr) {
                        Data->ResourceUpdater = new Updater(App->Settings, &App->MainWindow);
                    }

                    if (!Data->ResourceUpdater->Process()) {
                        App->EndFrame();
                        return;
                    }

                    delete Data->ResourceUpdater;
                    Data->ResourceUpdater = nullptr;
                    Data->ResourcesSynced = true;
                }

                // Create game module
                Data->Client = new FOClient(App->Settings, &App->MainWindow, false);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }

        // Client loop
        try {
            Data->Client->MainLoop();
        }
        catch (const ResourcesOutdatedException&) {
            try {
                Data->ResourcesSynced = false;
                Data->Client->Shutdown();
                Data->Client->Release();
                Data->Client = nullptr;
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }
        catch (const EngineDataNotFoundException& ex) {
            ReportExceptionAndExit(ex);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);

            // Recreate client on unhandled error
            try {
                Data->Client->Shutdown();
                Data->Client->Release();
                Data->Client = nullptr;
            }
            catch (const std::exception& ex2) {
                ReportExceptionAndExit(ex2);
            }
        }

        App->EndFrame();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}

#if !FO_TESTING_APP
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto ClientApp(int argc, char** argv) -> int
#endif
{
    STACK_TRACE_FIRST_ENTRY();

    try {
        ShowExceptionMessageBox(true);
        InitApp(argc, argv, true);

#if FO_IOS
        MainEntry(nullptr);
        App->SetMainLoopCallback(MainEntry);

#elif FO_WEB
        EM_ASM(FS.mkdir('/PersistentData'); FS.mount(IDBFS, {}, '/PersistentData'); Module.syncfsDone = 0; FS.syncfs(
            true, function(err) {
                assert(!err);
                Module.syncfsDone = 1;
            }););
        emscripten_set_main_loop_arg(MainEntry, nullptr, 0, 1);

#elif FO_ANDROID
        while (!App->Settings.Quit) {
            MainEntry(nullptr);
        }

#else
        auto balancer = FrameBalancer(!App->Settings.VSync && App->Settings.FixedFPS != 0, App->Settings.FixedFPS);

        while (!App->Settings.Quit) {
            balancer.StartLoop();
            MainEntry(nullptr);
            balancer.EndLoop();
        }
#endif

        WriteLog("Exit from game");

        if (Data->Client != nullptr) {
            Data->Client->Shutdown();
            delete Data->Client;
        }

        ExitApp(true);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
