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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ImGuiStuff.h"
#include "Server.h"
#include "Settings.h"

#if !FO_TESTING_APP
#include "SDL3/SDL_main.h"
#endif

struct ServerAppData
{
    unique_ptr<FOServer> Server {};
    vector<unique_ptr<FOClient>> Clients {};
    bool AutoStartTriggered {};
    bool HideControls {};
};
GLOBAL_DATA(ServerAppData, Data);

#if !FO_TESTING_APP
int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto ServerApp(int argc, char** argv) -> int
#endif
{
    STACK_TRACE_ENTRY();

    try {
        InitApp(argc, argv);

        list<vector<string>> log_buffer;
        std::mutex log_buffer_locker;

        SetLogCallback("ServerApp", [&log_buffer, &log_buffer_locker](string_view str) {
            if (auto lines = strex(str).split('\n'); !lines.empty()) {
                auto locker = std::unique_lock {log_buffer_locker};
                log_buffer.emplace_back(std::move(lines));
            }
        });

        const auto start_server = [] {
            // Server actually started in separate thread
            Data->Server = SafeAlloc::MakeUnique<FOServer>(App->Settings);
        };

        const auto stop_server = [] { Data->Server.reset(); };

        if (!App->Settings.NoStart) {
            WriteLog("Auto start server");
        }

        // Gui loop
        while (true) {
            App->BeginFrame();

            // Autostart
            if (!App->Settings.NoStart && !Data->Server && !Data->AutoStartTriggered) {
                Data->AutoStartTriggered = true;
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

                    if (!Data->Server && ImGui::Button("Start server", control_btn_size)) {
                        start_server();
                    }

                    if (Data->Server && ImGui::Button("Stop server", control_btn_size)) {
                        stop_server();
                    }

                    if (ImGui::Button("Spawn client", control_btn_size)) {
                        ShowExceptionMessageBox(true);
                        auto hide_msg_box = ScopeCallback([]() noexcept { ShowExceptionMessageBox(false); });

                        try {
                            auto client = SafeAlloc::MakeUnique<FOClient>(App->Settings, &App->MainWindow, false);
                            Data->Clients.emplace_back(std::move(client));
                            Data->HideControls = true;
                        }
                        catch (const std::exception& ex) {
                            ReportExceptionAndContinue(ex);
                        }
                        catch (...) {
                            UNKNOWN_EXCEPTION();
                        }
                    }

                    if (ImGui::Button("Create dump", control_btn_size)) {
                        CreateDumpMessage("ManualDump", "Manual");
                    }

                    if (ImGui::Button("Save log", control_btn_size)) {
                        string log_lines;

                        {
                            auto locker = std::unique_lock {log_buffer_locker};

                            for (const auto& lines : log_buffer) {
                                for (const auto& line : lines) {
                                    log_lines += line + '\n';
                                }
                            }
                        }

                        const auto time = time_point_t::now().desc();
                        const string log_name = strex("FOnlineServer_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.log", "Log", time.year, time.month, time.day, time.hour, time.minute, time.second);
                        DiskFileSystem::OpenFile(log_name, true).Write(log_lines);
                    }

                    if (!App->IsQuitRequested()) {
                        if (ImGui::Button("Quit", control_btn_size)) {
                            App->RequestQuit();
                        }
                    }
                    else {
                        imgui_progress_btn("Quitting...");
                    }
                }

                ImGui::End();
            }

            // Main loop
            if (Data->Server) {
                if (!Data->HideControls) {
                    try {
                        ImGui::SetNextWindowPos(ImVec2(10, 0), ImGuiCond_FirstUseEver);
                        Data->Server->DrawGui("Server");
                    }
                    catch (const std::exception& ex) {
                        ReportExceptionAndContinue(ex);
                    }
                    catch (...) {
                        UNKNOWN_EXCEPTION();
                    }
                }
            }

            // Log
            if (!Data->HideControls) {
                ImGui::SetNextWindowCollapsed(App->Settings.CollapseLogOnStart, ImGuiCond_Once);
                ImGui::SetNextWindowPos(ImVec2(10, 300), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);

                if (ImGui::Begin("Log", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
                    auto locker = std::unique_lock {log_buffer_locker};

                    for (const auto& lines : log_buffer) {
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
            vector<InputEvent> events;

            {
                InputEvent ev;
                while (App->Input.PollEvent(ev)) {
                    events.emplace_back(ev);
                }
            }

            for (auto& client : Data->Clients) {
                ShowExceptionMessageBox(true);
                auto hide_msg_box = ScopeCallback([]() noexcept { ShowExceptionMessageBox(false); });

                try {
                    App->Input.ClearEvents();

                    if (client == Data->Clients.back() && !ImGui::IsAnyItemHovered()) {
                        for (const auto& ev : events) {
                            App->Input.PushEvent(ev, true);
                        }
                    }

                    App->Render.ClearRenderTarget(ucolor::clear);

                    client->MainLoop();
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
                catch (...) {
                    UNKNOWN_EXCEPTION();
                }
            }

            App->EndFrame();

            // Process quit
            if (App->IsQuitRequested()) {
                Data->Clients.clear();

                if (Data->Server) {
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
    catch (...) {
        UNKNOWN_EXCEPTION();
    }
}
