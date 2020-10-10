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

#include "AppGui.h"
#include "NetBuffer.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"
#include "Version-Include.h"

#include "SDL_main.h"

struct ServerAppData
{
    GlobalSettings* Settings {};
    FOServer* Server {};
    std::thread ServerThread {};
    std::atomic_bool StartServer {};
};
GLOBAL_DATA(ServerAppData, Data);

static void ServerEntry()
{
    // Todo: fix data racing
    try {
        try {
            while (!Data->StartServer) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            Data->Server = new FOServer(*Data->Settings);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndExit(ex);
        }

        while (!Data->Settings->Quit) {
            try {
                Data->Server->MainLoop();
            }
            catch (const GenericException& ex) {
                ReportExceptionAndContinue(ex);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }

        try {
            auto* server = Data->Server;
            Data->Server = nullptr;
            delete server;
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
[[maybe_unused]] static auto ServerApp(int argc, char** argv) -> int
#endif
{
    try {
        CreateGlobalData();
        CatchExceptions("FOnlineServer", FO_VERSION);
        LogToFile("FOnlineServer.log");

        Data->Settings = new GlobalSettings(argc, argv);

#ifdef FO_HAVE_DIRECT_3D
        const auto use_dx = !Data->Settings->ForceOpenGL;
#else
        bool use_dx = false;
#endif
        if (!AppGui::Init("FOnline Server", use_dx, false, false)) {
            return -1;
        }

        // Autostart
        if (!Data->Settings->NoStart) {
            Data->StartServer = true;
        }

        // Server loop in separate thread
        Data->ServerThread = std::thread(ServerEntry);
        while (Data->StartServer && Data->Server == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(0));
        }

        // Gui loop
        while (!Data->StartServer || Data->Server != nullptr) {
            if (!AppGui::BeginFrame()) {
                // Immediate finish
                if (!Data->StartServer || Data->Server == nullptr || !Data->Server->Started) {
                    return 0;
                }

                // Graceful finish
                Data->Settings->Quit = true;
            }

            if (!Data->StartServer) {
                auto& io = ImGui::GetIO();
                ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                if (ImGui::Begin("---", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
                    if (ImGui::Button("Start server", ImVec2(200, 30))) {
                        Data->StartServer = true;
                        while (Data->Server == nullptr) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(0));
                        }
                    }
                }
                ImGui::End();
            }
            else {
                Data->Server->DrawGui();
            }

            AppGui::EndFrame();
        }

        // Wait server finish
        if (Data->ServerThread.joinable()) {
            Data->ServerThread.join();
        }

        return 0;
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
