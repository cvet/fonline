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
#include "Keyboard.h"
#include "Log.h"
#include "Mapper.h"
#include "Settings.h"
#include "Testing.h"
#include "Timer.h"
#include "Version_Include.h"

#include "SDL_main.h"

static GlobalSettings Settings;

static void MapperEntry(void*)
{
    static FOMapper* mapper = nullptr;
    if (!mapper)
    {
#ifdef FO_WEB
        // Wait file system synchronization
        if (EM_ASM_INT(return Module.syncfsDone) != 1)
            return;
#endif

        BEGIN_ROOT_EXCEPTION_BLOCK();
        mapper = new FOMapper(Settings);
        CATCH_EXCEPTION(GenericException);
        WriteLog("Something going wrong...");
        exit(1);
        END_ROOT_EXCEPTION_BLOCK();
    }

    BEGIN_ROOT_EXCEPTION_BLOCK();
    App::BeginFrame();
    mapper->MainLoop();
    App::EndFrame();
    CATCH_EXCEPTION(GenericException);
    WriteLog("Something going wrong...");
    exit(1);
    END_ROOT_EXCEPTION_BLOCK();
}

#ifndef FO_TESTING
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
static int main_disabled(int argc, char** argv)
#endif
{
    CatchExceptions("FOnlineMapper", FO_VERSION);
    LogToFile("FOnlineMapper.log");
    Settings.ParseArgs(argc, argv);

    // Start message
    WriteLog("Starting Mapper ({:#x})...\n", FO_VERSION);

    // Init graphic
    App::Init(Settings);

    // Loop
#if defined(FO_IOS)
    MapperEntry(nullptr);
    SDL_iPhoneSetAnimationCallback(SprMngr_MainWindow, 1, MapperEntry, nullptr);
    return 0;

#elif defined(FO_WEB)
    EM_ASM(FS.mkdir('/PersistentData'); FS.mount(IDBFS, {}, '/PersistentData'); Module.syncfsDone = 0; FS.syncfs(
        true, function(err) {
            assert(!err);
            Module.syncfsDone = 1;
        }););
    emscripten_set_main_loop_arg(MapperEntry, nullptr, 0, 1);

#elif defined(FO_ANDROID)
    while (!Settings.Quit)
        MapperEntry(nullptr);

#else
    while (!Settings.Quit)
    {
        double start_loop = Timer::AccurateTick();

        MapperEntry(nullptr);

        if (!Settings.VSync && Settings.FixedFPS)
        {
            if (Settings.FixedFPS > 0)
            {
                static double balance = 0.0;
                double elapsed = Timer::AccurateTick() - start_loop;
                double need_elapsed = 1000.0 / (double)Settings.FixedFPS;
                if (need_elapsed > elapsed)
                {
                    double sleep = need_elapsed - elapsed + balance;
                    balance = fmod(sleep, 1.0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(sleep)));
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(-Settings.FixedFPS));
            }
        }
    }
#endif

    // Finish script
    // Todo: fix script system
    /*if (Script::GetEngine())
    {
        Script::RunMandatorySuspended();
        Script::RaiseInternalEvent(ClientFunctions.Finish);
    }*/

    return 0;
}
