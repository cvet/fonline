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

#include "Application.h"
#include "ClientScripting.h"
#include "Log.h"
#include "Mapper.h"
#include "Settings.h"
#include "Testing.h"
#include "Timer.h"
#include "Version-Include.h"

#include "SDL_main.h"

struct MapperAppData
{
    GlobalSettings* Settings {};
    FOMapper* Mapper {};
};
GLOBAL_DATA(MapperAppData, Data);

#if !FO_TESTING
void ClientScriptSystem::InitNativeScripting()
{
}
void ClientScriptSystem::InitAngelScriptScripting()
{
}
void ClientScriptSystem::InitMonoScripting()
{
}
#endif

static void MapperEntry(void*)
{
    try {
        if (Data->Mapper == nullptr) {
#if FO_WEB
            // Wait file system synchronization
            if (EM_ASM_INT(return Module.syncfsDone) != 1)
                return;
#endif

            try {
                Data->Mapper = new FOMapper(*Data->Settings);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }

        try {
            App->BeginFrame();
            Data->Mapper->MapperMainLoop();
            App->EndFrame();
        }
        catch (const GenericException& ex) {
            ReportExceptionAndContinue(ex);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndExit(ex);
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}

#if !FO_TESTING
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto MapperApp(int argc, char** argv) -> int
#endif
{
    try {
        SetAppName("FOnlineMapper");
        CatchSystemExceptions();
        CreateGlobalData();
        LogToFile();

        WriteLog("Starting Mapper {}...\n", FO_GAME_VERSION);

        Data->Settings = new GlobalSettings(argc, argv);
        InitApplication(*Data->Settings);

#if FO_IOS
        MapperEntry(nullptr);
        App->SetMainLoopCallback(MapperEntry);

#elif FO_WEB
        EM_ASM(FS.mkdir('/PersistentData'); FS.mount(IDBFS, {}, '/PersistentData'); Module.syncfsDone = 0; FS.syncfs(
            true, function(err) {
                assert(!err);
                Module.syncfsDone = 1;
            }););
        emscripten_set_main_loop_arg(MapperEntry, nullptr, 0, 1);

#elif FO_ANDROID
        while (!Data->Settings->Quit) {
            MapperEntry(nullptr);
        }

#else
        while (!Data->Settings->Quit) {
            const auto start_loop = Timer::RealtimeTick();

            MapperEntry(nullptr);

            if (!Data->Settings->VSync && Data->Settings->FixedFPS != 0) {
                if (Data->Settings->FixedFPS > 0) {
                    static auto balance = 0.0;
                    const auto elapsed = Timer::RealtimeTick() - start_loop;
                    const auto need_elapsed = 1000.0 / static_cast<double>(Data->Settings->FixedFPS);
                    if (need_elapsed > elapsed) {
                        const auto sleep = need_elapsed - elapsed + balance;
                        balance = fmod(sleep, 1.0);
                        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(sleep)));
                    }
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(-Data->Settings->FixedFPS));
                }
            }
        }
#endif
        return 0;
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
