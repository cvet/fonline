//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "ImGuiStuff.h"

TEST_CASE("PersistedImGuiLayoutIsDeferredWithoutContext", "[imgui][settings]")
{
    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    CHECK_FALSE(ImGuiExt::LoadIniSettingsIfContext("[Window][Persisted]\nPos=0,0\nSize=1,1\n"));
}
