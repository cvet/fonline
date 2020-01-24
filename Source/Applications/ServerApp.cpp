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
#include "SDL_main.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"
#include "Version_Include.h"

static GlobalSettings Settings {};
static FOServer* Server;
static std::thread ServerThread;
static std::atomic_bool StartServer;

static void ServerEntry()
{
    while (!StartServer)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    Server = new FOServer(Settings);
    Server->Run();
    FOServer* server = Server;
    Server = nullptr;
    delete server;
}

#ifndef FO_TESTING
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
static int main_disabled(int argc, char** argv)
#endif
{
    CatchExceptions("FOnlineServer", FO_VERSION);
    LogToFile("FOnlineServer.log");
    Settings.ParseArgs(argc, argv);

    // Gui
    bool use_dx = !Settings.OpenGLRendering;
    if (!AppGui::Init("FOnline Server", use_dx, false, false))
        return -1;

    // Autostart
    if (!Settings.NoStart)
        StartServer = true;

    // Server loop in separate thread
    ServerThread = std::thread(ServerEntry);
    while (StartServer && !Server)
        std::this_thread::sleep_for(std::chrono::milliseconds(0));

    // Gui loop
    while (!StartServer || Server)
    {
        if (!AppGui::BeginFrame())
        {
            // Immediate finish
            if (!StartServer || !Server || (!Server->Started() && !Server->Stopping()))
                exit(0);

            // Graceful finish
            Settings.Quit = true;
        }

        if (!StartServer)
        {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            if (ImGui::Begin("---", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
            {
                if (ImGui::Button("Start server", ImVec2(200, 30)))
                {
                    StartServer = true;
                    while (!Server)
                        std::this_thread::sleep_for(std::chrono::milliseconds(0));
                }
            }
            ImGui::End();
        }
        else
        {
            Server->DrawGui();
        }

        AppGui::EndFrame();
    }

    // Wait server finish
    if (ServerThread.joinable())
        ServerThread.join();

    return 0;
}
