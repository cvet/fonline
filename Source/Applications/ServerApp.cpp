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
#include "Server.h"
#include "Settings.h"

#include "SDL_main.h"

enum class ServerStateType
{
    Stopped,
    Start,
    Started,
    Shutdown,
};

struct ServerAppData
{
    GlobalSettings* Settings {};
    FOServer* Server {};
    std::thread ServerThread {};
    std::atomic<ServerStateType> ServerState {};
};
GLOBAL_DATA(ServerAppData, Data);

static void ServerEntry()
{
    while (true) {
        try {
            try {
                while (Data->ServerState == ServerStateType::Stopped) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                RUNTIME_ASSERT(Data->ServerState == ServerStateType::Start);
                Data->Server = new FOServer(*Data->Settings);
                RUNTIME_ASSERT(Data->ServerState == ServerStateType::Start);
                Data->ServerState = ServerStateType::Started;
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }

            while (Data->ServerState != ServerStateType::Shutdown) {
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
                RUNTIME_ASSERT(Data->ServerState == ServerStateType::Shutdown);
                Data->Server->Shutdown();
                delete Data->Server;
                Data->Server = nullptr;
                RUNTIME_ASSERT(Data->ServerState == ServerStateType::Shutdown);
                Data->ServerState = ServerStateType::Stopped;
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndExit(ex);
        }
    }
}

#if !FO_TESTING
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto ServerApp(int argc, char** argv) -> int
#endif
{
    try {
        InitApp("Server");
        LogToBuffer(true);

        Data->Settings = new GlobalSettings(argc, argv);

#if FO_HAVE_DIRECT_3D
        const auto use_dx = !Data->Settings->ForceOpenGL;
#else
        const auto use_dx = false;
#endif
        if (!AppGui::Init(GetAppName(), use_dx, false, false)) {
            std::quick_exit(EXIT_FAILURE);
        }

        // Server loop in separate thread
        Data->ServerThread = std::thread(ServerEntry);

        // Autostart
        if (!Data->Settings->NoStart) {
            Data->ServerState = ServerStateType::Start;
        }

        // Gui loop
        while (true) {
            if (!AppGui::BeginFrame()) {
                Data->Settings->Quit = true;
            }

            if (Data->ServerState == ServerStateType::Stopped) {
                const auto& io = ImGui::GetIO();
                ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                if (ImGui::Begin("---", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
                    if (ImGui::Button("Start server", ImVec2(200, 30))) {
                        Data->ServerState = ServerStateType::Start;
                    }
                }
                ImGui::End();
            }
            else if (Data->ServerState == ServerStateType::Started) {
                Data->Server->DrawGui();
            }

            AppGui::EndFrame();

            if (Data->Settings->Quit) {
                if (Data->ServerState == ServerStateType::Started) {
                    Data->ServerState = ServerStateType::Shutdown;
                }
                else if (Data->ServerState == ServerStateType::Stopped) {
                    break;
                }
            }
        }

        RUNTIME_ASSERT(Data->ServerState == ServerStateType::Stopped);

        std::quick_exit(EXIT_SUCCESS);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
