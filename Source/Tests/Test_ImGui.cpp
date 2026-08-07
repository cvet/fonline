//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "ImGuiStuff.h"
#include "Test_ImGuiHarness.h"

TEST_CASE("PersistedImGuiLayoutIsDeferredWithoutContext", "[imgui][settings]")
{
    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    CHECK_FALSE(ImGuiExt::LoadIniSettingsIfContext("[Window][Persisted]\nPos=0,0\nSize=1,1\n"));
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
