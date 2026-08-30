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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "catch_amalgamated.hpp"

#include "ImGuiStuff.h"
#include "Test_ImGuiHarness.h"

TEST_CASE("PersistedImGuiLayoutIsDeferredWithoutContext", "[imgui][settings]")
{
    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    CHECK_FALSE(ImGuiExt::LoadIniSettingsIfContext("[Window][Persisted]\nPos=0,0\nSize=1,1\n"));
}

TEST_CASE("FreshProportionalStretchTableHasFiniteLayout", "[imgui][tables]")
{
    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = FO_NAMESPACE scope_exit([]() noexcept {
        FO_NAMESPACE safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {800.0f, 600.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    ImGui::NewFrame();
    REQUIRE(ImGui::Begin("ProportionalTableWindow"));
    REQUIRE(ImGui::BeginTable("##FreshTable", 2, ImGuiTableFlags_SizingStretchProp));
    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 220.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("Key");
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted("Value");
    ImGui::EndTable();
    ImGui::End();
    ImGui::Render();

    FO_NAMESPACE nptr<ImGuiWindow> window = FO_NAMESPACE ImGuiTestHarness::FindWindow("ProportionalTableWindow");
    REQUIRE(window);
    CHECK(std::isfinite(window->DC.IdealMaxPos.x));
}

TEST_CASE("ImGuiTestHarnessPressesWidgetsByLabel", "[imgui][harness]")
{
    // The harness is what lets the panel tests take the branch behind a control, so it is pinned here
    // against a window this test owns instead of only through the panels that use it
    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = FO_NAMESPACE scope_exit([]() noexcept {
        FO_NAMESPACE safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {800.0f, 600.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    int32_t first_presses = 0;
    int32_t second_presses = 0;
    bool checked = false;
    bool section_body_drawn = false;

    auto draw_frame = [&] {
        ImGui::NewFrame();

        if (ImGui::Begin("HarnessWindow", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::Button("First")) {
                first_presses++;
            }
            if (ImGui::Button("Second")) {
                second_presses++;
            }

            ImGui::Checkbox("Flag", &checked);

            if (ImGui::CollapsingHeader("Folded")) {
                section_body_drawn = true;
                ImGui::TextUnformatted("body");
            }
        }

        ImGui::End();
        ImGui::Render();
    };

    draw_frame();
    CHECK(first_presses == 0);
    CHECK_FALSE(section_body_drawn);

    // Every press needs one frame to queue and one frame to meet the widget again
    REQUIRE(FO_NAMESPACE ImGuiTestHarness::ActivateItem("HarnessWindow", "First"));
    draw_frame();
    CHECK(first_presses == 1);

    // A second press must not be swallowed by the first one still owning the active id
    REQUIRE(FO_NAMESPACE ImGuiTestHarness::ActivateItem("HarnessWindow", "Second"));
    draw_frame();
    CHECK(second_presses == 1);

    REQUIRE(FO_NAMESPACE ImGuiTestHarness::ActivateItem("HarnessWindow", "Flag"));
    draw_frame();
    CHECK(checked);

    REQUIRE(FO_NAMESPACE ImGuiTestHarness::SetItemOpen("HarnessWindow", "Folded"));
    draw_frame();
    CHECK(section_body_drawn);

    CHECK(FO_NAMESPACE ImGuiTestHarness::FindWindow("NoSuchWindow") == nullptr);
    CHECK_FALSE(FO_NAMESPACE ImGuiTestHarness::ActivateItem("NoSuchWindow", "First"));
    CHECK_FALSE(FO_NAMESPACE ImGuiTestHarness::SetItemOpen("NoSuchWindow", "Folded"));
    CHECK(FO_NAMESPACE ImGuiTestHarness::SetWindowCollapsed("HarnessWindow", false));
}
