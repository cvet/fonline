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
#include "FileSystem.h"
#include "ResourcePack.h"
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

TEST_CASE("ClientUpdaterGivesUpOnAServerThatStopsAnswering")
{
    using namespace TestClientUpdater;

    REQUIRE(net_sockets::startup());
    uint16_t port = OfflineServerPort.fetch_add(1);

    // The system accepts the connection and nothing ever serves it, which is what a stopped or hung server, or a peer
    // gone from the network, looks like from the client
    tcp_server silent_server;
    REQUIRE(silent_server.listen("127.0.0.1", port, 8));

    GlobalSettings client_settings = MakeUpdaterClientSettings(port);
    string bake_output = PrepareUpdaterBakeOutput();
    auto cleanup_bake_output = scope_exit([&bake_output]() noexcept { fs::remove_dir_tree(bake_output); });
    BakerTests::OverrideSetting(client_settings.Baking.BakeOutput, bake_output);
    BakerTests::OverrideSetting(client_settings.ClientNetwork.PingTimeout, 300);

    Updater updater {&client_settings, &GetApp()->MainWindow};
    REQUIRE(WaitForUpdaterResult(updater));

    CHECK(updater.IsAborted());
    CHECK(updater.GetResult() == UpdaterResult::ConnectionFailed);
}

TEST_CASE("ClientUpdaterWaitsForAnotherClientsUpdate")
{
    using namespace TestClientUpdater;

    GlobalSettings settings = MakeUpdaterClientSettings(OfflineServerPort.fetch_add(1));
    string install = PrepareUpdaterBakeOutput();
    string writable = strex("{}_writable", install).str();
    auto cleanup = scope_exit([&]() noexcept {
        (void)fs::remove_dir_tree(install);
        (void)fs::remove_dir_tree(writable);
    });
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, install);
    settings.ApplyWritableRoot(writable);
    string resources = GetClientWritableResourceDir(settings);
    string live = strex(resources).combine_path("Held.fores").str();
    string backup = strex("{}{}", live, REPLACED_FILE_BACKUP_SUFFIX).str();
    REQUIRE(fs::write_file(backup, "previous"));

    // Held by a thread of its own, because on Windows the thread that owns the named mutex may take it again
    std::promise<bool> held;
    std::promise<void> release;
    std::thread other_client([&] {
        fs::disk_directory_lock lock {resources};
        held.set_value(static_cast<bool>(lock));
        release.get_future().wait();
    });
    auto join_other_client = scope_exit([&]() noexcept {
        safe_call([&] {
            release.set_value();
            other_client.join();
        });
    });
    REQUIRE(held.get_future().get());

    Updater updater {&settings, &GetApp()->MainWindow};

    for (int32_t i = 0; i < 20; i++) {
        CHECK_FALSE(updater.Process());
        coarse_sleep(std::chrono::milliseconds {5});
    }

    // Waiting is neither a failure nor a start: the interrupted replacement the other client may be finishing stays put
    CHECK_FALSE(updater.IsAborted());
    CHECK(fs::exists(backup));
    CHECK_FALSE(fs::exists(live));

    release.set_value();
    other_client.join();
    join_other_client.release();

    REQUIRE(WaitForUpdaterResult(updater));
    CHECK(updater.GetResult() == UpdaterResult::ConnectionFailed);
    CHECK(fs::read_file(live) == optional<string> {"previous"});
    CHECK_FALSE(fs::exists(backup));
}

TEST_CASE("ClientUpdaterRecoversNestedBackupsBeforeConnecting")
{
    using namespace TestClientUpdater;

    GlobalSettings settings = MakeUpdaterClientSettings(OfflineServerPort.fetch_add(1));
    string install = PrepareUpdaterBakeOutput();
    string writable = strex("{}_writable", install).str();
    auto cleanup = scope_exit([&]() noexcept {
        (void)fs::remove_dir_tree(install);
        (void)fs::remove_dir_tree(writable);
    });
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, install);
    settings.ApplyWritableRoot(writable);
    string resources = GetClientWritableResourceDir(settings);
    string binaries = GetClientBinaryDir(settings.Common.UserWritablePath);

    auto in_dir = [](string_view directory, string_view name) { return strex(directory).combine_path(name).str(); };
    auto backup_of = [](string_view name) { return strex("{}{}", name, REPLACED_FILE_BACKUP_SUFFIX).str(); };

    for (const string& directory : {resources, binaries}) {
        REQUIRE(fs::write_file(in_dir(directory, backup_of("Sub/Missing")), "previous"));
        REQUIRE(fs::write_file(in_dir(directory, backup_of("Sub/Current")), "previous"));
        REQUIRE(fs::write_file(in_dir(directory, "Sub/Current"), "current"));
        REQUIRE(fs::write_file(in_dir(directory, backup_of("Sub/")), "unrelated"));

        // A portable client sweeps the folder the player unpacked it into, where their own copies live too
        REQUIRE(fs::write_file(in_dir(directory, "Sub/Notes.txt"), "notes"));
        REQUIRE(fs::write_file(in_dir(directory, "Sub/Notes.txt-backup"), "player copy"));
    }

    Updater updater {&settings, &GetApp()->MainWindow};

    for (const string& directory : {resources, binaries}) {
        CHECK(fs::read_file(in_dir(directory, "Sub/Missing")) == optional<string> {"previous"});
        CHECK(fs::read_file(in_dir(directory, "Sub/Current")) == optional<string> {"current"});
        CHECK_FALSE(fs::exists(in_dir(directory, backup_of("Sub/Missing"))));
        CHECK_FALSE(fs::exists(in_dir(directory, backup_of("Sub/Current"))));
        CHECK(fs::read_file(in_dir(directory, backup_of("Sub/"))) == optional<string> {"unrelated"});
        CHECK(fs::read_file(in_dir(directory, "Sub/Notes.txt-backup")) == optional<string> {"player copy"});
    }
}

TEST_CASE("ClientResourcePackCurrencyFollowsTheEffectivePair")
{
    using namespace TestClientUpdater;

    GlobalSettings settings = MakeUpdaterClientSettings(OfflineServerPort.fetch_add(1));
    string install = PrepareUpdaterBakeOutput();
    string writable = strex("{}_writable", install).str();
    string remote = strex("{}_remote", install).str();
    auto cleanup = scope_exit([&]() noexcept {
        (void)fs::remove_dir_tree(install);
        (void)fs::remove_dir_tree(writable);
        (void)fs::remove_dir_tree(remote);
    });
    BakerTests::OverrideSetting(settings.Common.Packaged, true);
    BakerTests::OverrideSetting(settings.Baking.ClientResources, install);
    settings.ApplyWritableRoot(writable);
    REQUIRE(fs::create_directories(remote));

    string base_path = strex(install).combine_path("Art.fores").str();
    string target_path = strex(remote).combine_path("Art.fores").str();
    auto write_pack = [](string_view path, string_view content) {
        ResourcePackWriter writer {path};
        writer.AddFile("Shared.txt", {reinterpret_cast<const uint8_t*>("shared"), 6});
        writer.AddFile("Changed.txt", {reinterpret_cast<const uint8_t*>(content.data()), content.size()});
        writer.Finish();
    };
    write_pack(base_path, "installed");
    write_pack(target_path, "server");

    ResourcePackHeader base_header;
    REQUIRE(ReadResourcePackHeader(base_path, base_header));
    ResourcePackSource target {target_path};
    CHECK(IsClientResourcePackCurrent(settings, "Art", base_header.ContentHash));
    CHECK_FALSE(IsClientResourcePackCurrent(settings, "Art", target.GetContentHash()));

    // An installed base plus a committed patch never has the server base's size, so only the content identity
    // of the pair can tell the game client what the updater already knows
    string patch_path = GetClientResourcePatchPath(settings, "Art");
    REQUIRE(fs::create_directories(strex(patch_path).extract_dir().str()));
    ResourcePatchWriter writer {base_path, patch_path, target.GetEntryRefs(), target.GetContentHash()};
    fs::disk_read_file remote_file {target_path};
    fs::disk_directory_lock patch_lock {strex(patch_path).extract_dir().str()};
    writer.Begin(patch_lock);

    for (const ResourcePackEntryRef& entry : writer.GetDownloads()) {
        vector<uint8_t> payload(numeric_cast<size_t>(entry.StoredSize));
        REQUIRE(remote_file.read_at(entry.DataOffset, payload));
        writer.AddEncodedFile(payload);
    }

    writer.Finish();
    CHECK(IsClientResourcePackCurrent(settings, "Art", target.GetContentHash()));
    CHECK_FALSE(IsClientResourcePackCurrent(settings, "Art", base_header.ContentHash));
    CHECK_FALSE(IsClientResourcePackCurrent(settings, "Missing", target.GetContentHash()));
}

FO_END_NAMESPACE
