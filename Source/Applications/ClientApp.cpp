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
#include "Client.h"
#include "Log.h"
#include "Settings.h"
#include "Testing.h"
#include "Timer.h"
#include "Version-Include.h"
#ifdef FO_SINGLEPLAYER
#include "Server.h"
#endif

#include "SDL_main.h"

struct ClientAppData
{
    GlobalSettings* Settings {};
    FOClient* Client {};
#ifdef FO_SINGLEPLAYER
    FOServer* Server {};
#endif
};
GLOBAL_DATA(ClientAppData, Data);

#ifdef FO_SINGLEPLAYER
NetServerBase* NetServerBase::StartTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback)
{
    throw UnreachablePlaceException(LINE_STR);
}
NetServerBase* NetServerBase::StartWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback)
{
    throw UnreachablePlaceException(LINE_STR);
}
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

static void ClientEntry(void*)
{
    try {
        if (Data->Client == nullptr) {
#ifdef FO_WEB
            // Wait file system synchronization
            if (EM_ASM_INT(return Module.syncfsDone) != 1)
                return;
#endif

            try {
#ifdef FO_SINGLEPLAYER
                Data->Server = new FOServer(*Data->Settings);
#endif
                Data->Client = new FOClient(*Data->Settings);
#ifdef FO_SINGLEPLAYER
                Data->Server->ConnectClient(Data->Client);
#endif
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }

        try {
            App->BeginFrame();
#ifdef FO_SINGLEPLAYER
            Data->Server->MainLoop();
#endif
            Data->Client->MainLoop();
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

#ifndef FO_TESTING
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto ClientApp(int argc, char** argv) -> int
#endif
{
    try {
        SetAppName("FOnline");
        CatchSystemExceptions();
        CreateGlobalData();
        LogToFile();

        WriteLog("Starting FOnline ({:#x})...\n", FO_VERSION);

        Data->Settings = new GlobalSettings(argc, argv);
        InitApplication(*Data->Settings);

        // Hard restart, need wait before lock event dissapeared
#ifdef FO_WINDOWS
        if (::wcsstr(GetCommandLineW(), L"--restart") != nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
#endif

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
        while (!Settings->Quit) {
            ClientEntry(nullptr);
        }

#else
        while (!Data->Settings->Quit) {
            const auto start_loop = Timer::RealtimeTick();

            ClientEntry(nullptr);

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

        WriteLog("Exit from game.\n");

#ifdef FO_SINGLEPLAYER
        delete Data->Server;
#endif
        delete Data->Client;
        return 0;
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
