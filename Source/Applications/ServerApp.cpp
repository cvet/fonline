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

struct ServerAppData
{
    FOServer* Server {};
    vector<FOClient*> Clients {};
    bool HideControls {};
    size_t ServerStartCycles {100};
};
GLOBAL_DATA(ServerAppData, Data);

#if !FO_TESTING_APP
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto ServerApp(int argc, char** argv) -> int
#endif
{
    STACK_TRACE_FIRST_ENTRY();

    try {
        InitApp(argc, argv);

        list<vector<string>> log_buffer;
        SetLogCallback("ServerApp", [&log_buffer](string_view str) {
            if (auto&& lines = _str(str).split('\n'); !lines.empty()) {
                log_buffer.emplace_back(std::move(lines));
            }
        });

        const auto start_server = [] {
            try {
                Data->Server = new FOServer(App->Settings);
                Data->Server->Start();
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
                Data->Server = nullptr;
            }
        };

        const auto stop_server = [] {
            try {
                Data->Server->Shutdown();
                Data->Server->Release();
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
            Data->Server = nullptr;
        };

        if (!App->Settings.NoStart) {
            WriteLog("Auto start server");
        }

        // Gui loop
        while (true) {
            App->BeginFrame();

            // Autostart
            if (!App->Settings.NoStart && Data->ServerStartCycles > 0 && --Data->ServerStartCycles == 0) {
                start_server();
            }

            if (Data->HideControls) {
                if (ImGui::Begin("Restore controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                    if (ImGui::Button("Restore controls")) {
                        Data->HideControls = false;
                    }
                }
                ImGui::End();
            }

            if (!Data->HideControls) {
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

                    if (ImGui::Button("Start server", control_btn_size)) {
                        start_server();
                    }
                    if (ImGui::Button("Stop server", control_btn_size)) {
                        stop_server();
                    }

                    if (ImGui::Button("Spawn client", control_btn_size)) {
                        ShowExceptionMessageBox(true);
                        try {
                            auto* client = new FOClient(App->Settings, &App->MainWindow, false);
                            Data->Clients.emplace_back(client);
                            Data->HideControls = true;
                        }
                        catch (const std::exception& ex) {
                            ReportExceptionAndContinue(ex);
                        }
                        ShowExceptionMessageBox(false);
                    }

                    if (ImGui::Button("Create dump", control_btn_size)) {
                        CreateDumpMessage("ManualDump", "Manual");
                    }
                    if (ImGui::Button("Save log", control_btn_size)) {
                        string log_lines;
                        for (auto&& lines : log_buffer) {
                            for (auto&& line : lines) {
                                log_lines += line + '\n';
                            }
                        }

                        const auto dt = Timer::GetCurrentDateTime();
                        const string log_name = _str("FOnlineServer_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.log", "Log", dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);
                        DiskFileSystem::OpenFile(log_name, true).Write(log_lines);
                    }

                    if (!App->Settings.Quit) {
                        if (ImGui::Button("Quit", control_btn_size)) {
                            App->Settings.Quit = true;
                        }
                    }
                    else {
                        imgui_progress_btn("Quitting...");
                    }
                }
                ImGui::End();
            }

            // Main loop
            if (Data->Server != nullptr) {
                try {
                    Data->Server->MainLoop();
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }

                if (!Data->HideControls) {
                    try {
                        ImGui::SetNextWindowPos(ImVec2(10, 0), ImGuiCond_FirstUseEver);
                        Data->Server->DrawGui("Server");
                    }
                    catch (const std::exception& ex) {
                        ReportExceptionAndContinue(ex);
                    }
                }
            }

            // Log
            if (!Data->HideControls) {
                ImGui::SetNextWindowCollapsed(App->Settings.CollapseLogOnStart, ImGuiCond_Once);
                ImGui::SetNextWindowPos(ImVec2(10, 300), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
                if (ImGui::Begin("Log", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
                    for (auto&& lines : log_buffer) {
                        if (ImGui::TreeNodeEx(lines.front().c_str(), (lines.size() < 2 ? ImGuiTreeNodeFlags_Leaf : 0) | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                            for (size_t i = 1; i < lines.size(); i++) {
                                ImGui::TextUnformatted(lines[i].c_str(), lines[i].c_str() + lines[i].size());
                            }
                            ImGui::TreePop();
                        }
                    }
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
                ImGui::End();
            }

            // Clients loop
            if (ImGui::IsAnyItemHovered()) {
                App->Input.ClearEvents();
            }

            for (auto* client : Data->Clients) {
                ShowExceptionMessageBox(true);
                try {
                    App->Render.ClearRenderTarget(COLOR_RGB(0, 0, 0));
                    client->MainLoop();
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
                ShowExceptionMessageBox(false);
            }

            App->EndFrame();

            // Process quit
            if (App->Settings.Quit) {
                for (auto* client : Data->Clients) {
                    client->Shutdown();
                    client->Release();
                }
                Data->Clients.clear();

                if (Data->Server != nullptr) {
                    stop_server();
                }

                break;
            }
        }

        ExitApp(true);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
