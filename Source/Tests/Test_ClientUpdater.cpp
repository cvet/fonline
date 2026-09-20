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

#include "Application.h"
#include "Settings.h"
#include "Test_BakerHelpers.h"
#include "Updater.h"

FO_BEGIN_NAMESPACE

namespace TestClientUpdater
{
    // Deliberately away from the ports the other suites bind: this suite wants one nobody listens on
    static std::atomic_uint16_t OfflineServerPort {49500};

    static auto MakeUpdaterClientSettings(uint16_t port) -> GlobalSettings
    {
        FO_STACK_TRACE_ENTRY();

        GlobalSettings settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);
        BakerTests::OverrideSetting(settings.Network.ServerPort, port);

        return settings;
    }

    // The updater draws its own screen before it has downloaded anything, so it reads a font off disk
    // at construction; without one it cannot be built at all
    static auto PrepareUpdaterBakeOutput() -> string
    {
        FO_STACK_TRACE_ENTRY();

        std::chrono::steady_clock::rep suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        string dir_name = strex("fo_client_updater_offline_{}", suffix).str();
        std::filesystem::path base = std::filesystem::temp_directory_path() / std::filesystem::path {fs::make_path(dir_name)};
        string bake_dir = fs::path_to_string(base);
        string fonts_dir = strex(bake_dir).combine_path("Embedded/Fonts").str();

        REQUIRE(fs::create_directories(fonts_dir));

        constexpr string_view default_font = R"(Version 2
Image Default.png
YAdvance 1

Letter ' '
  PositionX 0
  PositionY 0
  Width 1
  Height 1
  XAdvance 1

End
)";

        REQUIRE(fs::write_file(strex(fonts_dir).combine_path("Default.fofnt").str(), default_font));

        vector<uint8_t> default_font_sprite = BakerTests::MakeMinimalBakedSprite();
        REQUIRE(fs::write_file(strex(fonts_dir).combine_path("Default.png").str(), default_font_sprite));

        return bake_dir;
    }

    static auto WaitForUpdaterResult(Updater& updater) -> bool
    {
        FO_STACK_TRACE_ENTRY();

        for (int32_t i = 0; i < 2000; i++) {
            if (updater.Process()) {
                return true;
            }

            coarse_sleep(std::chrono::milliseconds {2});
        }

        return false;
    }
}

TEST_CASE("ClientUpdaterMeetsAnOfflineServerAsAConnectionFailure")
{
    using namespace TestClientUpdater;

    uint16_t port = OfflineServerPort.fetch_add(1);
    GlobalSettings client_settings = MakeUpdaterClientSettings(port);
    string bake_output = PrepareUpdaterBakeOutput();
    auto cleanup_bake_output = scope_exit([&bake_output]() noexcept { fs::remove_dir_tree(bake_output); });
    BakerTests::OverrideSetting(client_settings.Baking.BakeOutput, bake_output);

    // Nothing is listening on this port: the player starts the client while the server is down
    Updater updater {&client_settings, &GetApp()->MainWindow};
    REQUIRE(WaitForUpdaterResult(updater));

    CHECK(updater.IsAborted());
    CHECK(updater.GetResult() == UpdaterResult::ConnectionFailed);

    // The generic Failed result would tell the player to reinstall a client that is not at fault, and
    // would file one crash report for every server restart
    CHECK_FALSE(IsUpdaterFailureReportable(updater.GetResult()));
}

FO_END_NAMESPACE
