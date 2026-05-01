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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ImGuiStuff.h"
#include "Server.h"
#include "Settings.h"

#if !FO_TESTING_APP
#include "SDL3/SDL_main.h"
#endif

FO_USING_NAMESPACE();

enum class WindowLayoutMode : uint8_t
{
    SingleTab, // only the active virtual window is shown; tabs switch which one
    Tile, // grid: server + 1 client side-by-side, or N clients only
    Cascade, // free-floating ImGui windows, user-positioned
};

constexpr int32_t TAB_BAR_HEIGHT_PX = 32;

#if !FO_TESTING_APP
int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto ServerApp(int argc, char** argv) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

    try {
        InitApp(numeric_cast<int32_t>(argc), argv, AppInitFlags::PrebakeResources);

        App->MainWindow.SetTitle("Server");
        App->MainWindow.SetSize({App->Settings.ServerWidth, App->Settings.ServerHeight});

        refcount_ptr<ServerEngine> server;
        vector<refcount_ptr<ClientEngine>> clients;
        vector<AppWindow*> client_windows;
        bool auto_start_triggered = false;
        bool start_client_triggered = false;
        bool hide_controls = false;

        WindowLayoutMode layout_mode = WindowLayoutMode::SingleTab;
        bool layout_init_dirty = true;
        bool os_size_saved = false;
        isize32 os_size_before_first_child {};

        list<pair<vector<string>, StackTraceData>> log_buffer;
        std::mutex log_buffer_locker;
        int32_t exception_count = 0;

        SetLogCallback("ServerApp", [&](LogType, string_view str) FO_DEFERRED {
            auto locker = std::unique_lock {log_buffer_locker};

            auto lines = strex(str).split('\n');
            log_buffer.emplace_back(std::move(lines), GetStackTrace());

            if (log_buffer.size() > numeric_cast<size_t>(App->Settings.MaxServerLogLines)) {
                log_buffer.pop_front();
            }

            if (str.find("Exception") != string_view::npos) {
                exception_count++;
            }
        });

        const auto start_server = [&server] { server = SafeAlloc::MakeRefCounted<ServerEngine>(App->Settings, GetServerResources(App->Settings)); };
        const auto stop_server = [&server] {
            server->Shutdown();
            server.reset();
        };

        const auto start_client = [&] {
            try {
                const auto title = strex("Client {}", clients.size() + 1).str();
                const auto client_size = isize32 {App->Settings.ScreenWidth, App->Settings.ScreenHeight};

                if (!os_size_saved) {
                    os_size_before_first_child = App->MainWindow.GetSize();
                    os_size_saved = true;

                    const auto target = isize32 {
                        std::max(os_size_before_first_child.width, client_size.width),
                        std::max(os_size_before_first_child.height, client_size.height + TAB_BAR_HEIGHT_PX),
                    };

                    if (target.width != os_size_before_first_child.width || target.height != os_size_before_first_child.height) {
                        App->MainWindow.SetSize(target);
                    }
                }

                auto* window = App->CreateChildWindow(client_size, title);
                FO_RUNTIME_ASSERT(window);

                auto client = SafeAlloc::MakeRefCounted<ClientEngine>(App->Settings, GetClientResources(App->Settings), *window);
                clients.emplace_back(std::move(client));
                client_windows.emplace_back(window);
                App->SetActiveWindow(window);

                // Two or more clients -> switch to Tile so the user can see them all at once.
                if (clients.size() >= 2 && layout_mode != WindowLayoutMode::Tile) {
                    layout_mode = WindowLayoutMode::Tile;
                    layout_init_dirty = true;
                }
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
            catch (...) {
                FO_UNKNOWN_EXCEPTION();
            }
        };

        if (!App->Settings.NoStart) {
            WriteLog("Auto start server");
        }

        // Gui loop
        while (true) {
            App->BeginFrame();

            // Autostart
            if (!App->Settings.NoStart && !server && !auto_start_triggered) {
                auto_start_triggered = true;
                start_server();
            }

            if (server && server->IsStarted() && App->Settings.AutoStartClientOnServer && !start_client_triggered) {
                start_client_triggered = true;
                WriteLog("Auto start embedded client");
                start_client();
            }

            const auto child_count = App->GetChildWindowsCount();
            const bool has_virtual_windows = (child_count > 0);
            const bool main_active = (App->GetActiveWindow() == &App->MainWindow);
            const bool show_server = [&] {
                if (layout_mode == WindowLayoutMode::SingleTab) {
                    return main_active;
                }
                if (layout_mode == WindowLayoutMode::Tile) {
                    return child_count < 2 || main_active;
                }
                return true; // Cascade
            }();

            // Compute layout rects for this frame.
            const auto& io = ImGui::GetIO();
            const float32_t host_w = io.DisplaySize.x;
            const float32_t host_h = io.DisplaySize.y;
            const float32_t tab_h = numeric_cast<float32_t>(TAB_BAR_HEIGHT_PX);
            const float32_t content_top = has_virtual_windows ? tab_h : 0.0f;
            const float32_t content_h = std::max(0.0f, host_h - content_top);

            irect32 main_rect {};

            if (!has_virtual_windows) {
                main_rect = {0, 0, iround<int32_t>(host_w), iround<int32_t>(host_h)};
            }
            else if (layout_mode == WindowLayoutMode::Tile && child_count == 1) {
                const auto half_w = iround<int32_t>(host_w * 0.5f);
                main_rect = {0, iround<int32_t>(content_top), half_w, iround<int32_t>(content_h)};

                if (auto* child = App->GetChildWindow(0); child != nullptr) {
                    child->SetDisplayRect({half_w, iround<int32_t>(content_top), iround<int32_t>(host_w) - half_w, iround<int32_t>(content_h)});
                }
            }
            else if (layout_mode == WindowLayoutMode::Tile) {
                int32_t cols = 1;

                while (cols * cols < numeric_cast<int32_t>(child_count)) {
                    cols++;
                }

                const int32_t rows = (numeric_cast<int32_t>(child_count) + cols - 1) / cols;
                const float32_t cell_w = host_w / numeric_cast<float32_t>(cols);
                const float32_t cell_h = content_h / numeric_cast<float32_t>(rows);

                for (size_t i = 0; i < child_count; i++) {
                    const auto idx = numeric_cast<int32_t>(i);
                    const int32_t row = idx / cols;
                    const int32_t col = idx % cols;

                    if (auto* child = App->GetChildWindow(i); child != nullptr) {
                        child->SetDisplayRect({
                            iround<int32_t>(numeric_cast<float32_t>(col) * cell_w),
                            iround<int32_t>(content_top + numeric_cast<float32_t>(row) * cell_h),
                            std::max(1, iround<int32_t>(cell_w)),
                            std::max(1, iround<int32_t>(cell_h)),
                        });
                    }
                }

                main_rect = {0, iround<int32_t>(content_top), iround<int32_t>(host_w), iround<int32_t>(content_h)};
            }
            else if (layout_mode == WindowLayoutMode::SingleTab) {
                const irect32 area = {0, iround<int32_t>(content_top), iround<int32_t>(host_w), iround<int32_t>(content_h)};
                main_rect = area;

                for (size_t i = 0; i < child_count; i++) {
                    if (auto* child = App->GetChildWindow(i); child != nullptr) {
                        child->SetDisplayRect(area);
                    }
                }
            }
            else { // Cascade
                main_rect = {0, iround<int32_t>(content_top), iround<int32_t>(host_w), iround<int32_t>(content_h)};
            }

            App->MainWindow.SetDisplayRect(main_rect);

            const auto rect_pos = ImVec2(numeric_cast<float32_t>(main_rect.x), numeric_cast<float32_t>(main_rect.y));
            const auto rect_size = ImVec2(numeric_cast<float32_t>(main_rect.width), numeric_cast<float32_t>(main_rect.height));

            if (has_virtual_windows) {
                constexpr ImGuiWindowFlags HOST_FIXED_FLAGS = //
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | //
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | //
                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | //
                    ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

                auto pop_styles = bundled_scope_exit {[]() noexcept { safe_call([] { ImGui::PopStyleVar(2); }); }};

                const auto draw_texture = [](AppWindow* w, ImVec2 region) {
                    auto* tex = w->GetRenderTexture();

                    if (tex == nullptr || region.x <= 0.0f || region.y <= 0.0f) {
                        return;
                    }

                    const ImVec2 uv0 = tex->FlippedHeight ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f);
                    const ImVec2 uv1 = tex->FlippedHeight ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f);
                    ImGui::Image(tex, region, uv0, uv1);
                };

                if (layout_mode == WindowLayoutMode::SingleTab) {
                    auto* shown = App->GetActiveWindow();

                    if (shown != nullptr && shown != &App->MainWindow) {
                        const auto r = shown->GetDisplayRect();
                        ImGui::SetNextWindowPos(ImVec2(numeric_cast<float32_t>(r.x), numeric_cast<float32_t>(r.y)), ImGuiCond_Always);
                        ImGui::SetNextWindowSize(ImVec2(numeric_cast<float32_t>(r.width), numeric_cast<float32_t>(r.height)), ImGuiCond_Always);

                        if (ImGui::Begin("##ImageHostSingle", nullptr, HOST_FIXED_FLAGS)) {
                            draw_texture(shown, ImGui::GetContentRegionAvail());
                        }

                        ImGui::End();
                    }
                }
                else if (layout_mode == WindowLayoutMode::Tile) {
                    for (size_t i = 0; i < child_count; i++) {
                        auto* child = App->GetChildWindow(i);

                        if (child == nullptr) {
                            continue;
                        }

                        const auto r = child->GetDisplayRect();
                        ImGui::SetNextWindowPos(ImVec2(numeric_cast<float32_t>(r.x), numeric_cast<float32_t>(r.y)), ImGuiCond_Always);
                        ImGui::SetNextWindowSize(ImVec2(numeric_cast<float32_t>(r.width), numeric_cast<float32_t>(r.height)), ImGuiCond_Always);

                        const auto label = strex("##ImageHostTile_{}", i).str();

                        if (ImGui::Begin(label.c_str(), nullptr, HOST_FIXED_FLAGS)) {
                            draw_texture(child, ImGui::GetContentRegionAvail());
                        }

                        ImGui::End();
                    }
                }
                else { // Cascade
                    constexpr ImGuiWindowFlags CASCADE_FLAGS = //
                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

                    for (size_t i = 0; i < child_count; i++) {
                        auto* child = App->GetChildWindow(i);

                        if (child == nullptr) {
                            continue;
                        }

                        const auto title = child->GetTitle().empty() ? strex("Window {}", i + 1).str() : child->GetTitle();
                        const auto label = strex("{}###ImageHostCascade_{}", title, i).str();

                        const auto cond = layout_init_dirty ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
                        const float32_t default_w = std::min(640.0f, host_w * 0.6f);
                        const float32_t default_h = std::min(480.0f, content_h * 0.6f);
                        const float32_t default_x = 40.0f + numeric_cast<float32_t>(i) * 30.0f;
                        const float32_t default_y = content_top + 40.0f + numeric_cast<float32_t>(i) * 30.0f;

                        ImGui::SetNextWindowPos(ImVec2(default_x, default_y), cond);
                        ImGui::SetNextWindowSize(ImVec2(default_w, default_h), cond);

                        if (ImGui::Begin(label.c_str(), nullptr, CASCADE_FLAGS)) {
                            draw_texture(child, ImGui::GetContentRegionAvail());

                            const auto img_min = ImGui::GetItemRectMin();
                            const auto img_max = ImGui::GetItemRectMax();
                            child->SetDisplayRect({
                                iround<int32_t>(img_min.x),
                                iround<int32_t>(img_min.y),
                                std::max(1, iround<int32_t>(img_max.x - img_min.x)),
                                std::max(1, iround<int32_t>(img_max.y - img_min.y)),
                            });
                        }

                        ImGui::End();
                    }
                }
            }

            if (show_server) {
                ImGuiWindowFlags server_flags = ImGuiWindowFlags_NoCollapse;
                const bool cascade_mode = (layout_mode == WindowLayoutMode::Cascade);

                if (!has_virtual_windows) {
                    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
                    server_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
                }
                else if (cascade_mode) {
                    ImGui::SetNextWindowPos(rect_pos, ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowSize(rect_size, ImGuiCond_FirstUseEver);
                }
                else {
                    ImGui::SetNextWindowPos(rect_pos, ImGuiCond_Always);
                    ImGui::SetNextWindowSize(rect_size, ImGuiCond_Always);
                    server_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
                }

                if (ImGui::Begin("Server", nullptr, server_flags)) {
                    if (has_virtual_windows && ImGui::IsWindowHovered()) {
                        App->SetActiveWindow(&App->MainWindow);
                    }

                    if (hide_controls) {
                        if (exception_count != 0) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                            ImGui::TextUnformatted(strex("Exceptions caught: {} (see log)", exception_count).c_str());
                            ImGui::PopStyleColor();
                        }
                        if (ImGui::Button("Restore controls")) {
                            hide_controls = false;
                        }
                    }
                    else {
                        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);

                        if (ImGui::CollapsingHeader("Controls")) {
                            // Buttons sit on a single horizontal row; each takes an equal share of the available width.
                            constexpr int32_t CONTROL_BTN_COUNT = 4;
                            const auto total_w = ImGui::GetContentRegionAvail().x;
                            const auto spacing_x = ImGui::GetStyle().ItemSpacing.x;
                            const auto btn_w = std::max(40.0f, (total_w - spacing_x * numeric_cast<float32_t>(CONTROL_BTN_COUNT - 1)) / numeric_cast<float32_t>(CONTROL_BTN_COUNT));
                            const auto btn_size = ImVec2(btn_w, 30.0f);

                            const auto imgui_progress_btn = [&btn_size](const char* title) {
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                                ImGui::Button(title, btn_size);
                                ImGui::PopStyleColor();
                            };

                            if (!server) {
                                if (ImGui::Button("Start server", btn_size)) {
                                    start_server();
                                }
                            }
                            else {
                                if (ImGui::Button("Stop server", btn_size)) {
                                    stop_server();
                                }
                            }

                            ImGui::SameLine();

                            if (ImGui::Button("Spawn client", btn_size)) {
                                start_client();
                            }

                            ImGui::SameLine();

                            if (ImGui::Button("Save log", btn_size)) {
                                string log_lines;

                                {
                                    auto locker = std::unique_lock {log_buffer_locker};

                                    for (const auto& lines : log_buffer | std::views::keys) {
                                        for (const auto& line : lines) {
                                            log_lines += line + '\n';
                                        }
                                    }
                                }

                                const auto time = nanotime::now().desc(true);
                                const string log_name = strex("FOnlineServer_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.log", "Log", time.year, time.month, time.day, time.hour, time.minute, time.second);
                                std::ofstream log_file {std::filesystem::path {fs_make_path(log_name)}, std::ios::binary | std::ios::trunc};

                                if (log_file) {
                                    log_file.write(log_lines.data(), static_cast<std::streamsize>(log_lines.size()));
                                }
                            }

                            ImGui::SameLine();

                            if (!App->IsQuitRequested()) {
                                if (ImGui::Button("Quit", btn_size)) {
                                    App->RequestQuit();
                                }
                            }
                            else {
                                imgui_progress_btn("Quitting...");
                            }

                            if (exception_count != 0) {
                                ImGui::Separator();
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                                ImGui::TextUnformatted(strex("Exceptions caught: {} (see log)", exception_count).c_str());
                                ImGui::PopStyleColor();
                            }
                        }

                        if (server) {
                            try {
                                server->DrawGui();
                            }
                            catch (const std::exception& ex) {
                                exception_count++;
                                ReportExceptionAndContinue(ex);
                            }
                            catch (...) {
                                FO_UNKNOWN_EXCEPTION();
                            }
                        }

                        ImGui::SetNextItemOpen(!App->Settings.CollapseLogOnStart, ImGuiCond_FirstUseEver);

                        if (ImGui::CollapsingHeader("Log")) {
                            const auto log_height = std::min(400.0f, std::max(rect_size.y * 0.4f, 150.0f));

                            if (ImGui::BeginChild("##LogChild", ImVec2(0.0f, log_height), ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
                                auto locker = std::unique_lock {log_buffer_locker};

                                for (const auto& [lines, st] : log_buffer) {
                                    if (ImGui::TreeNodeEx(lines.front().c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
                                        for (size_t i = 1; i < lines.size(); i++) {
                                            ImGui::TextUnformatted(lines[i].c_str(), lines[i].c_str() + lines[i].size());
                                        }
                                        for (const auto& st_line : strex(FormatStackTrace(st)).split('\n')) {
                                            ImGui::TextUnformatted(st_line.c_str(), st_line.c_str() + st_line.size());
                                        }

                                        ImGui::TreePop();
                                    }
                                }

                                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                                    ImGui::SetScrollHereY(1.0f);
                                }
                            }

                            ImGui::EndChild();
                        }
                    }
                }

                ImGui::End();
            }

            if (has_virtual_windows) {
                constexpr ImGuiWindowFlags TAB_BAR_FLAGS = //
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | //
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | //
                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(host_w, tab_h), ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.85f);

                if (ImGui::Begin("##WindowTabs", nullptr, TAB_BAR_FLAGS)) {
                    const auto draw_tab = [&](AppWindow* window, const string& label) {
                        const bool is_active = (App->GetActiveWindow() == window);

                        if (is_active) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                        }

                        if (ImGui::Button(label.c_str())) {
                            App->SetActiveWindow(window);
                        }

                        if (is_active) {
                            ImGui::PopStyleColor();
                        }

                        ImGui::SameLine();
                    };

                    draw_tab(&App->MainWindow, string {"Server"});

                    for (size_t i = 0; i < child_count; i++) {
                        if (auto* child = App->GetChildWindow(i); child != nullptr) {
                            const auto label = child->GetTitle().empty() ? strex("Client {}", i + 1).str() : child->GetTitle();
                            draw_tab(child, label);
                        }
                    }

                    ImGui::Dummy(ImVec2(16.0f, 0.0f));
                    ImGui::SameLine();

                    const auto layout_button = [&](const char* label, WindowLayoutMode mode) {
                        const bool is_active = (layout_mode == mode);

                        if (is_active) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                        }

                        if (ImGui::Button(label)) {
                            if (layout_mode != mode) {
                                layout_mode = mode;
                                layout_init_dirty = true;
                            }
                        }

                        if (is_active) {
                            ImGui::PopStyleColor();
                        }

                        ImGui::SameLine();
                    };

                    layout_button("Single", WindowLayoutMode::SingleTab);
                    layout_button("Tile", WindowLayoutMode::Tile);
                    layout_button("Cascade", WindowLayoutMode::Cascade);
                }

                ImGui::End();
            }

            // Mode-switch button is the only path that flips this; clear once consumed.
            if (layout_init_dirty) {
                layout_init_dirty = false;
            }

            // Clients loop
            vector<InputEvent> events;

            {
                InputEvent ev;
                while (App->Input.PollEvent(ev)) {
                    events.emplace_back(ev);
                }
            }

            FO_RUNTIME_ASSERT(clients.size() == client_windows.size());

            const auto* active_window = App->GetActiveWindow();

            for (size_t i = 0; i < clients.size(); i++) {
                auto& client = clients[i];
                auto* window = client_windows[i];

                try {
                    App->BeginWindowRender(window);
                    auto end_window_render = scope_exit([&]() noexcept { safe_call([] { App->EndWindowRender(); }); });

                    App->Input.ClearEvents();

                    if (window == active_window) {
                        for (const auto& ev : events) {
                            App->Input.PushEvent(ev, true);
                        }
                    }

                    client->MainLoop();
                }
                catch (const std::exception& ex) {
                    exception_count++;
                    ReportExceptionAndContinue(ex);
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }
            }

            App->EndFrame();

            // Process quit
            if (App->IsQuitRequested()) {
                for (auto& client : std::exchange(clients, {})) {
                    client->Shutdown();
                }

                for (auto* window : std::exchange(client_windows, {})) {
                    if (window != nullptr) {
                        App->DestroyChildWindow(window);
                    }
                }

                if (server) {
                    stop_server();
                }

                break;
            }
        }

        FO_RUNTIME_ASSERT(!server);
        FO_RUNTIME_ASSERT(clients.empty());
        ExitApp(App->GetRequestedQuitSuccess());
    }
    catch (const std::exception& ex) {
        SetLogCallback("", nullptr);
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        SetLogCallback("", nullptr);
        FO_UNKNOWN_EXCEPTION();
    }
}
