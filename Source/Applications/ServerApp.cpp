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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "DiskFileSystem.h"
#include "Server.h"
#include "Settings.h"

#include "imgui.h"

#if !FO_TESTING_APP
#include "SDL_main.h"
#endif

enum class ServerStateType
{
    Stopped,
    StartRequest,
    Started,
    StopRequest,
};

struct ServerAppData
{
    FOServer* Server {};
    std::thread ServerThread {};
    std::atomic<ServerStateType> ServerState {};
    vector<FOClient*> Clients {};
    std::atomic_bool ClientSpawning {};
    std::atomic<FOClient*> SpawnedClient {};
    std::atomic<size_t> ThreadTasks {};
};
GLOBAL_DATA(ServerAppData, Data);

#if !FO_TESTING_APP
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto ServerApp(int argc, char** argv) -> int
#endif
{
    try {
        InitApp(argc, argv, "Server");

        string log_buffer;
        SetLogCallback("ServerApp", [&log_buffer](auto str) {
            log_buffer += str;
            if (log_buffer.size() > 10000000) {
                log_buffer = log_buffer.substr(log_buffer.size() - 50000000);
            }
        });

        // Async server start/stop
        const auto start_server = [] {
            RUNTIME_ASSERT(Data->ServerState == ServerStateType::Stopped);
            Data->ServerState = ServerStateType::StartRequest;
            Data->ThreadTasks.fetch_add(1);
            std::thread([] {
                try {
                    RUNTIME_ASSERT(Data->ServerState == ServerStateType::StartRequest);
                    Data->Server = new FOServer(App->Settings);
                    RUNTIME_ASSERT(Data->ServerState == ServerStateType::StartRequest);
                    Data->ServerState = ServerStateType::Started;
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                    Data->Server = nullptr;
                    Data->ServerState = ServerStateType::Stopped;
                }
                Data->ThreadTasks.fetch_sub(1);
            }).detach();
        };

        const auto stop_server = [] {
            RUNTIME_ASSERT(Data->ServerState == ServerStateType::Started);
            Data->ServerState = ServerStateType::StopRequest;
            Data->ThreadTasks.fetch_add(1);
            std::thread([] {
                try {
                    RUNTIME_ASSERT(Data->ServerState == ServerStateType::StopRequest);
                    Data->Server->Shutdown();
                    Data->Server->Release();
                    RUNTIME_ASSERT(Data->ServerState == ServerStateType::StopRequest);
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
                Data->Server = nullptr;
                Data->ServerState = ServerStateType::Stopped;
                Data->ThreadTasks.fetch_sub(1);
            }).detach();
        };

        const auto spawn_client = [](bool in_separate_window) {
            Data->ClientSpawning = true;
            Data->ThreadTasks.fetch_add(1);
            // Todo: allow instantiate client in separate thread (rendering issues)
            // std::thread([] {
            try {
                auto* window = in_separate_window ? App->CreateChildWindow(800, 600) : &App->MainWindow;
                Data->SpawnedClient = new FOClient(App->Settings, window, Data->Server->RestoreInfoBin);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
            Data->ClientSpawning = false;
            Data->ThreadTasks.fetch_sub(1);
            //}).detach();
        };

        // Autostart
        if (!App->Settings.NoStart) {
            start_server();
        }

        // Gui loop
        while (true) {
            App->BeginFrame();

            const auto& io = ImGui::GetIO();

            // Control panel
            constexpr auto control_btn_size = ImVec2(250, 30);
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2.0f, 50.0f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.0f));
            if (ImGui::Begin("Control panel", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
                const auto imgui_progress_btn = [&control_btn_size](const char* title) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                    ImGui::Button(title, control_btn_size);
                    ImGui::PopStyleColor();
                };

                const auto some_task_running = Data->ThreadTasks > 0;
                if (some_task_running) {
                    ImGui::BeginDisabled();
                }

                if (Data->ServerState == ServerStateType::Stopped) {
                    if (ImGui::Button("Start server", control_btn_size)) {
                        start_server();
                    }
                }
                else if (Data->ServerState == ServerStateType::StartRequest) {
                    imgui_progress_btn("Starting server...");
                }
                else if (Data->ServerState == ServerStateType::Started) {
                    if (ImGui::Button("Stop server", control_btn_size)) {
                        stop_server();
                    }
                }
                else if (Data->ServerState == ServerStateType::StopRequest) {
                    imgui_progress_btn("Stopping server...");
                }

                if (!Data->ClientSpawning) {
                    if (Data->SpawnedClient != nullptr) {
                        Data->Clients.emplace_back(Data->SpawnedClient);
                        Data->SpawnedClient = nullptr;
                    }

                    if (ImGui::Button("Spawn client (in background)", control_btn_size)) {
                        spawn_client(false);
                    }
                    if (ImGui::Button("Spawn client (in new window)", control_btn_size)) {
                        spawn_client(true);
                    }
                }
                else {
                    imgui_progress_btn("Client spawning...");
                    imgui_progress_btn("..................");
                }

                if (ImGui::Button("Create dump", control_btn_size)) {
                    CreateDumpMessage("ManualDump", "Manual");
                }
                if (ImGui::Button("Save log", control_btn_size)) {
                    const auto dt = Timer::GetCurrentDateTime();
                    const string log_name = _str("Logs/FOnlineServer_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.log", "Log", dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);
                    DiskFileSystem::OpenFile(log_name, true).Write(log_buffer);
                }

                if (!App->Settings.Quit) {
                    if (ImGui::Button("Quit", control_btn_size)) {
                        App->Settings.Quit = true;
                    }
                }
                else {
                    imgui_progress_btn("Quitting...");
                }

                if (some_task_running) {
                    ImGui::EndDisabled();
                }
            }
            ImGui::End();

            // Main loop
            if (Data->ServerState == ServerStateType::Started) {
                try {
                    Data->Server->MainLoop();
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }

                try {
                    ImGui::SetNextWindowPos(ImVec2(10, 0), ImGuiCond_FirstUseEver);
                    Data->Server->DrawGui("Server");
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
            }

            // Log
            ImGui::SetNextWindowPos(ImVec2(10, 300), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Log", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
                if (!log_buffer.empty()) {
                    ImGui::TextUnformatted(log_buffer.c_str(), log_buffer.c_str() + log_buffer.size());
                }
            }
            ImGui::End();

            // Clients loop
            for (auto* client : Data->Clients) {
                try {
                    client->MainLoop();
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
            }

            App->EndFrame();

            // Process quit
            if (App->Settings.Quit) {
                for (auto* client : Data->Clients) {
                    client->Shutdown();
                    client->Release();
                }
                Data->Clients.clear();

                if (Data->ServerState == ServerStateType::Started) {
                    stop_server();
                }
                else if (Data->ServerState == ServerStateType::Stopped) {
                    break;
                }
            }
        }

        ExitApp(true);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
