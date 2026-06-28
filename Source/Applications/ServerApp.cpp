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

FO_BEGIN_NAMESPACE
extern void ClientStartupSettingsHook(GlobalSettings& settings, int32_t client_index, bool embedded);
FO_END_NAMESPACE

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
[[maybe_unused]] static auto ServerApp(CommandLineArgs args) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

#if !FO_TESTING_APP
    const vector<CommandLineArg> args_holder = MakeCommandLineArgs(numeric_cast<int32_t>(argc), argv);
    const CommandLineArgs args {args_holder};
#endif

    try {
        InitApp(args, AppInitFlags::PrebakeResources);

        GetApp()->MainWindow.SetTitle("Server");

        const auto configured_client_size = isize32 {GetApp()->Settings.ScreenWidth, GetApp()->Settings.ScreenHeight};
        GetApp()->MainWindow.SetSize({GetApp()->Settings.ServerWidth, GetApp()->Settings.ServerHeight});

        refcount_nptr<ServerEngine> server {};
        vector<unique_ptr<GlobalSettings>> client_settings;
        vector<refcount_ptr<ClientEngine>> clients;
        vector<ptr<AppWindow>> client_windows;
        bool auto_start_triggered = false;
        bool start_client_triggered = false;
        bool hide_controls = false;

        WindowLayoutMode layout_mode = WindowLayoutMode::SingleTab;
        bool layout_init_dirty = true;
        bool os_size_saved = false;
        isize32 os_size_before_first_child {};

        list<pair<vector<string>, CatchedStackTraceData>> log_buffer;
        mutex log_buffer_locker;
        int32_t exception_count = 0;

        SetLogCallback("ServerApp", [&](LogType type, string_view str, nptr<const CatchedStackTraceData> st) FO_DEFERRED {
            scoped_lock locker {log_buffer_locker};

            auto lines = strex(str).split('\n');
            log_buffer.emplace_back(std::move(lines), st ? *st : CatchedStackTraceData {std::nullopt, GetStackTrace()});

            if (log_buffer.size() > numeric_cast<size_t>(GetApp()->Settings.MaxServerLogLines)) {
                log_buffer.pop_front();
            }

            if (type == LogType::Error) {
                exception_count++;
            }
        });

        const auto get_server = [&server]() -> ptr<ServerEngine> {
            FO_STACK_TRACE_ENTRY();

            FO_VERIFY_AND_THROW(server, "Server engine is not created");
            return server.as_ptr();
        };

        const auto start_server = [&server] {
            ptr<GlobalSettings> settings = &GetApp()->Settings;
            server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, GetServerResources(*settings));
        };
        const auto stop_server = [&server, &get_server] {
            ptr<ServerEngine> running_server = get_server();
            running_server->Shutdown();
            server.reset();
        };

        const auto start_client = [&] {
            try {
                const auto title = strex("Client {}", clients.size() + 1).str();
                const auto client_size = configured_client_size;

                if (!os_size_saved) {
                    os_size_before_first_child = GetApp()->MainWindow.GetSize();
                    os_size_saved = true;

                    const auto target = isize32 {
                        std::max(os_size_before_first_child.width, client_size.width),
                        std::max(os_size_before_first_child.height, client_size.height + TAB_BAR_HEIGHT_PX),
                    };

                    if (target.width != os_size_before_first_child.width || target.height != os_size_before_first_child.height) {
                        GetApp()->MainWindow.SetSize(target);
                    }
                }

                auto window = GetApp()->CreateChildWindow(client_size, title);

                const int32_t client_index = numeric_cast<int32_t>(clients.size()) + 1;
                unique_ptr<GlobalSettings> settings = SafeAlloc::MakeUnique<GlobalSettings>(false);
                settings->CopyFrom(GetApp()->Settings);
                ClientStartupSettingsHook(*settings, client_index, true);
                settings->ScreenWidth = client_size.width;
                settings->ScreenHeight = client_size.height;

                ptr<GlobalSettings> settings_ptr = settings.get();
                refcount_ptr<ClientEngine> client = SafeAlloc::MakeRefCounted<ClientEngine>(settings_ptr, GetClientResources(*settings), window);
                client_settings.emplace_back(std::move(settings));
                clients.emplace_back(std::move(client));
                client_windows.emplace_back(window);
                GetApp()->SetActiveWindow(window);

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

        if (!GetApp()->Settings.NoStart) {
            WriteLog("Auto start server");
        }

        // Gui loop
        while (true) {
            GetApp()->BeginFrame();

            // Autostart
            if (!GetApp()->Settings.NoStart && !server && !auto_start_triggered) {
                auto_start_triggered = true;
                start_server();
            }

            if (server && get_server()->IsStarted() && GetApp()->Settings.AutoStartClientOnServer > 0 && !start_client_triggered) {
                start_client_triggered = true;
                WriteLog("Auto start embedded client(s): {}", GetApp()->Settings.AutoStartClientOnServer);

                for (int32_t i = 0; i < GetApp()->Settings.AutoStartClientOnServer; i++) {
                    start_client();
                }
            }

            const auto child_count = GetApp()->GetChildWindowsCount();
            const bool has_virtual_windows = (child_count > 0);
            ptr<AppWindow> main_window = &GetApp()->MainWindow;
            const bool main_active = (GetApp()->GetActiveWindow() == main_window);
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

                if (nptr<AppWindow> nullable_child = GetApp()->GetChildWindow(0); nullable_child) {
                    auto child = nullable_child.as_ptr();
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

                    if (nptr<AppWindow> nullable_child = GetApp()->GetChildWindow(i); nullable_child) {
                        auto child = nullable_child.as_ptr();
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

                auto shown = GetApp()->GetActiveWindow();

                for (size_t i = 0; i < child_count; i++) {
                    if (nptr<AppWindow> nullable_child = GetApp()->GetChildWindow(i); nullable_child) {
                        auto child = nullable_child.as_ptr();
                        child->SetDisplayRect(nullable_child == shown ? area : irect32 {});
                    }
                }
            }
            else { // Cascade
                main_rect = {0, iround<int32_t>(content_top), iround<int32_t>(host_w), iround<int32_t>(content_h)};
            }

            GetApp()->MainWindow.SetDisplayRect(main_rect);

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

                auto pop_styles = scope_exit([]() noexcept { safe_call([] { ImGui::PopStyleVar(2); }); });

                const auto draw_texture = [](ptr<AppWindow> window, ImVec2 region) {
                    auto nullable_tex = window->GetRenderTexture();

                    if (!nullable_tex || region.x <= 0.0f || region.y <= 0.0f) {
                        return;
                    }

                    auto tex = nullable_tex.as_ptr();
                    if (tex->Size.width <= 0 || tex->Size.height <= 0) {
                        return;
                    }

                    const float32_t tex_w = numeric_cast<float32_t>(tex->Size.width);
                    const float32_t tex_h = numeric_cast<float32_t>(tex->Size.height);
                    const float32_t scale = std::min(region.x / tex_w, region.y / tex_h);
                    const auto img_size = ImVec2(tex_w * scale, tex_h * scale);
                    const auto cursor = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddRectFilled(cursor, ImVec2(cursor.x + region.x, cursor.y + region.y), IM_COL32(0, 0, 0, 255));

                    const auto img_origin = ImVec2(cursor.x + (region.x - img_size.x) * 0.5f, cursor.y + (region.y - img_size.y) * 0.5f);
                    ImGui::SetCursorScreenPos(img_origin);

                    const ImVec2 uv0 = tex->FlippedHeight ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f);
                    const ImVec2 uv1 = tex->FlippedHeight ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f);
                    ImGui::Image(tex.get(), img_size, uv0, uv1);

                    window->SetDisplayRect({
                        iround<int32_t>(img_origin.x),
                        iround<int32_t>(img_origin.y),
                        std::max(1, iround<int32_t>(img_size.x)),
                        std::max(1, iround<int32_t>(img_size.y)),
                    });
                };

                if (layout_mode == WindowLayoutMode::SingleTab) {
                    auto nullable_shown = GetApp()->GetActiveWindow();

                    if (nullable_shown && nullable_shown != &GetApp()->MainWindow) {
                        auto shown = nullable_shown.as_ptr();
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
                        auto nullable_child = GetApp()->GetChildWindow(i);

                        if (!nullable_child) {
                            continue;
                        }

                        auto child = nullable_child.as_ptr();
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
                        auto nullable_child = GetApp()->GetChildWindow(i);

                        if (!nullable_child) {
                            continue;
                        }

                        auto child = nullable_child.as_ptr();
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
                        ptr<AppWindow> hovered_server_window = &GetApp()->MainWindow;
                        GetApp()->SetActiveWindow(hovered_server_window);
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

                            const auto imgui_progress_btn = [&btn_size](string_view title) {
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                                ImGui::Button(title.data(), btn_size);
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
                                    scoped_lock locker {log_buffer_locker};

                                    for (const auto& lines : log_buffer | std::views::keys) {
                                        for (const auto& line : lines) {
                                            log_lines += line + '\n';
                                        }
                                    }
                                }

                                const auto time = nanotime::now().desc(true);
                                const string log_name = strex("FOnlineServer_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.log", "Log", time.year, time.month, time.day, time.hour, time.minute, time.second);
                                std::ofstream log_file {std::filesystem::path {fs_make_path(log_name)}, std::ios::binary | std::ios::trunc};

                                if (log_file && !log_lines.empty()) {
                                    ptr<const char> log_data = log_lines.data();
                                    log_file.write(log_data.get(), static_cast<std::streamsize>(log_lines.size()));
                                }
                            }

                            ImGui::SameLine();

                            if (!GetApp()->IsQuitRequested()) {
                                if (ImGui::Button("Quit", btn_size)) {
                                    GetApp()->RequestQuit();
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
                                ptr<ServerEngine> running_server = get_server();
                                running_server->DrawGui();
                            }
                            catch (const std::exception& ex) {
                                ReportExceptionAndContinue(ex);
                            }
                            catch (...) {
                                FO_UNKNOWN_EXCEPTION();
                            }
                        }

                        ImGui::SetNextItemOpen(!GetApp()->Settings.CollapseLogOnStart, ImGuiCond_FirstUseEver);

                        if (ImGui::CollapsingHeader("Log")) {
                            const auto log_height = std::min(400.0f, std::max(rect_size.y * 0.4f, 150.0f));

                            if (ImGui::BeginChild("##LogChild", ImVec2(0.0f, log_height), ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
                                scoped_lock locker {log_buffer_locker};

                                for (const auto& [lines, st] : log_buffer) {
                                    if (ImGui::TreeNodeEx(lines.front().c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
                                        for (size_t i = 1; i < lines.size(); i++) {
                                            ImGuiTextUnformatted(lines[i]);
                                        }

                                        const auto formatted = st.Origin.has_value() ? FormatStackTrace(st) : FormatStackTrace(st.Catched);

                                        for (const auto& st_line : strex(formatted).split('\n')) {
                                            ImGuiTextUnformatted(st_line);
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
                    const auto draw_tab = [&](ptr<AppWindow> window, const string& label) {
                        const bool is_active = (GetApp()->GetActiveWindow() == window);

                        if (is_active) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                        }

                        if (ImGui::Button(label.c_str())) {
                            GetApp()->SetActiveWindow(window);
                        }

                        if (is_active) {
                            ImGui::PopStyleColor();
                        }

                        ImGui::SameLine();
                    };

                    ptr<AppWindow> server_window = &GetApp()->MainWindow;
                    draw_tab(server_window, string {"Server"});

                    for (size_t i = 0; i < child_count; i++) {
                        if (nptr<AppWindow> nullable_child = GetApp()->GetChildWindow(i); nullable_child) {
                            auto child = nullable_child.as_ptr();
                            const string label = child->GetTitle().empty() ? strex("Client {}", i + 1).str() : string {child->GetTitle()};
                            draw_tab(child, label);
                        }
                    }

                    ImGui::Dummy(ImVec2(16.0f, 0.0f));
                    ImGui::SameLine();

                    const auto layout_button = [&](string_view label, WindowLayoutMode mode) {
                        const bool is_active = (layout_mode == mode);

                        if (is_active) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                        }

                        if (ImGui::Button(label.data())) {
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

                    ImGui::Dummy(ImVec2(16.0f, 0.0f));
                    ImGui::SameLine();

                    if (ImGui::Button("Spawn Client")) {
                        start_client();
                    }

                    ImGui::SameLine();

                    if (exception_count != 0) {
                        ImGui::Dummy(ImVec2(16.0f, 0.0f));
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.15f, 0.15f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));

                        if (ImGui::Button(strex("Exceptions: {}", exception_count).c_str())) {
                            ptr<AppWindow> exception_server_window = &GetApp()->MainWindow;
                            GetApp()->SetActiveWindow(exception_server_window);
                        }

                        ImGui::PopStyleColor(4);
                    }
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
                while (GetApp()->Input.PollEvent(ev)) {
                    events.emplace_back(ev);
                }
            }

            FO_VERIFY_AND_THROW(clients.size() == client_windows.size(), "Embedded client list and window list are out of sync", clients.size(), client_windows.size());

            auto active_window = GetApp()->GetActiveWindow();

            for (size_t i = 0; i < clients.size(); i++) {
                auto& client = clients[i];
                ptr<AppWindow> window = client_windows[i];

                try {
                    GetApp()->BeginWindowRender(window);
                    auto end_window_render = scope_exit([&]() noexcept { safe_call([] { GetApp()->EndWindowRender(); }); });

                    GetApp()->Input.ClearEvents();

                    if (active_window == window) {
                        for (const auto& ev : events) {
                            GetApp()->Input.PushEvent(ev, true);
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

            GetApp()->EndFrame();

            // Process quit
            if (GetApp()->IsQuitRequested()) {
                for (auto& client : std::exchange(clients, {})) {
                    client->Shutdown();
                }

                client_settings.clear();

                for (ptr<AppWindow> window : std::exchange(client_windows, {})) {
                    GetApp()->DestroyChildWindow(window);
                }

                if (server) {
                    stop_server();
                }

                break;
            }
        }

        FO_VERIFY_AND_THROW(!server, "Server is already set");
        FO_VERIFY_AND_THROW(clients.empty(), "Clients must be empty before this operation");
        FO_VERIFY_AND_THROW(client_settings.empty(), "Client settings must be empty before this operation");
        ExitApp(GetApp()->GetRequestedQuitSuccess());
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
