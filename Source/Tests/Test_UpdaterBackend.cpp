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

#include <chrono>
#include <filesystem>

#include "catch_amalgamated.hpp"

#include "DataSerialization.h"
#include "DiskFileSystem.h"
#include "ResourcePack.h"
#include "Test_BakerHelpers.h"
#include "UpdateDescriptor.h"
#include "UpdaterBackend.h"

FO_BEGIN_NAMESPACE

namespace UpdaterBackendTests
{
    static auto MakeTempDir(string_view name) -> string
    {
        auto base = std::filesystem::temp_directory_path() / std::format("lf_updater_backend_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
        return fs::path_to_string(base);
    }

    static auto HashString(string_view value) noexcept -> uint64_t
    {
        return fs::hash_data({reinterpret_cast<const uint8_t*>(value.data()), value.size()});
    }
}

TEST_CASE("UpdaterBackendUsesPlatformSpecificResourcePackInsteadOfCommonPack")
{
    using namespace UpdaterBackendTests;

    string root_dir = MakeTempDir("platform_resource");
    string client_resources_dir = strex(root_dir).combine_path("ClientResources").str();
    string platform_binaries_dir = strex(root_dir).combine_path("PlatformBinaries").str();
    string windows_target_dir = strex(platform_binaries_dir).combine_path("Windows-win64").str();
    auto cleanup = scope_exit([&root_dir]() noexcept { (void)fs::remove_dir_tree(root_dir); });

    REQUIRE(fs::create_directories(client_resources_dir));
    REQUIRE(fs::create_directories(windows_target_dir));

    vector<uint8_t> metadata = BakerTests::MakeEmptyMetadataBlob();
    constexpr string_view common_scripts = "common-scripts-pack";
    constexpr string_view windows_scripts = "windows-target-scripts";
    constexpr string_view windows_runtime = "windows-runtime-dll";
    string common_pack_path = strex(client_resources_dir).combine_path("Scripts.fores").str();
    string windows_pack_path = strex(windows_target_dir).combine_path("Scripts.fores").str();
    {
        ResourcePackWriter writer {common_pack_path};
        writer.AddFile("Common.txt", {reinterpret_cast<const uint8_t*>(common_scripts.data()), common_scripts.size()});
        writer.AddFile("Metadata.fometa-client", metadata);
        writer.Finish();
    }
    {
        ResourcePackWriter writer {windows_pack_path};
        writer.AddFile("Metadata.fometa-client", metadata);
        writer.AddFile("Windows.txt", {reinterpret_cast<const uint8_t*>(windows_scripts.data()), windows_scripts.size()});
        writer.Finish();
    }
    REQUIRE(fs::write_file(strex(windows_target_dir).combine_path("Game.dll").str(), windows_runtime));

    ResourcePackHeader common_header;
    ResourcePackHeader windows_header;
    REQUIRE(ReadResourcePackHeader(common_pack_path, common_header));
    REQUIRE(ReadResourcePackHeader(windows_pack_path, windows_header));

    auto settings = GlobalSettings(false);
    settings.ApplyDefaultSettings();
    BakerTests::OverrideSetting(settings.Baking.ClientResources, client_resources_dir);
    BakerTests::OverrideSetting(settings.Baking.PlatformBinaries, platform_binaries_dir);
    auto pack_config = ConfigFile("[ResourcePack]\nName = Scripts\nClientOnly = True\n");
    settings.ApplyConfigFile(pack_config, "");
    BakerTests::OverrideSetting(settings.ServerNetwork.UpdateFilesInMemory, true);

    UpdaterBackend updater_backend;
    updater_backend.LoadFromClientResources(settings, BakerTests::TEST_METADATA_VERSION);

    auto common_entries = ReadUpdateDescriptor(updater_backend.GetUpdateDescriptor("Unknown-target"));
    REQUIRE(common_entries.size() == 1);
    CHECK(common_entries.front().Name == "Scripts.fores");
    CHECK(common_entries.front().Hash == common_header.PackHash);
    CHECK(common_entries.front().Target == UpdateFileTarget::ClientResources);
    REQUIRE(common_entries.front().PackHeader.has_value());
    CHECK(common_entries.front().PackHeader->ContentHash == common_header.ContentHash);

    auto windows_entries = ReadUpdateDescriptor(updater_backend.GetUpdateDescriptor("Windows-win64"));
    auto scripts_entries = windows_entries | std::views::filter([](const UpdateDescriptorEntry& entry) { return entry.Name == "Scripts.fores"; });
    REQUIRE(std::ranges::distance(scripts_entries) == 1);
    const UpdateDescriptorEntry& scripts_entry = *scripts_entries.begin();
    CHECK(scripts_entry.Size == *fs::file_size(windows_pack_path));
    CHECK(scripts_entry.Hash == windows_header.PackHash);
    CHECK(scripts_entry.Target == UpdateFileTarget::ClientResources);
    REQUIRE(scripts_entry.PackHeader.has_value());
    CHECK(scripts_entry.PackHeader->ContentHash == windows_header.ContentHash);

    auto runtime_entry = std::ranges::find(windows_entries, "Game.dll", &UpdateDescriptorEntry::Name);
    REQUIRE(runtime_entry != windows_entries.end());
    CHECK(runtime_entry->Hash == HashString(windows_runtime));
    CHECK(runtime_entry->Target == UpdateFileTarget::ClientBinaries);
    CHECK_FALSE(runtime_entry->PackHeader.has_value());
}

TEST_CASE("UpdateDescriptorIsOneFormatForTheServerAndBothClientReaders")
{
    ResourcePackHeader header;
    header.PackHash = 0x1234;
    header.ContentHash = 0x5678;
    header.DataOffset = RESOURCE_PACK_HEADER_SIZE;
    header.VersionMajor = RESOURCE_PACK_VERSION_MAJOR;
    header.VersionMinor = RESOURCE_PACK_VERSION_MINOR;

    auto make_entry = [&header](string_view name, UpdateFileTarget target, bool with_header) {
        UpdateDescriptorEntry entry;
        entry.Name = string(name);
        entry.Size = 100;
        entry.Hash = header.PackHash;
        entry.Target = target;
        entry.FileIndex = 7;

        if (with_header) {
            entry.PackHeader = header;
        }

        return entry;
    };
    auto write_one = [](const UpdateDescriptorEntry& entry) {
        vector<uint8_t> desc;
        WriteUpdateDescriptor(desc, vector<UpdateDescriptorEntry> {entry});
        return desc;
    };

    SECTION("RoundTripsEveryField")
    {
        vector<uint8_t> desc;
        WriteUpdateDescriptor(desc, vector<UpdateDescriptorEntry> {make_entry("Sub/Art.fores", UpdateFileTarget::ClientResources, true), make_entry("Game.dll", UpdateFileTarget::ClientBinaries, false)});

        auto entries = ReadUpdateDescriptor(desc);
        REQUIRE(entries.size() == 2);
        CHECK(entries[0].Name == "Sub/Art.fores");
        CHECK(entries[0].Size == 100);
        CHECK(entries[0].Hash == 0x1234);
        CHECK(entries[0].FileIndex == 7);
        REQUIRE(entries[0].PackHeader.has_value());
        CHECK(entries[0].PackHeader->ContentHash == 0x5678);
        CHECK(entries[1].Name == "Game.dll");
        CHECK(entries[1].Target == UpdateFileTarget::ClientBinaries);
        CHECK_FALSE(entries[1].PackHeader.has_value());
    }

    SECTION("RefusesMismatchedHeaderAndTarget")
    {
        CHECK_THROWS(write_one(make_entry("Art.fores", UpdateFileTarget::ClientResources, false)));
        CHECK_THROWS(write_one(make_entry("Game.dll", UpdateFileTarget::ClientBinaries, true)));
    }

    SECTION("RefusesNamesAClientCannotPlaceOrMount")
    {
        CHECK_THROWS(ReadUpdateDescriptor(write_one(make_entry("../Art.fores", UpdateFileTarget::ClientResources, true))));
        CHECK_THROWS(ReadUpdateDescriptor(write_one(make_entry("Art.patch.fores", UpdateFileTarget::ClientResources, true))));
        CHECK_THROWS(ReadUpdateDescriptor(write_one(make_entry("Art.zip", UpdateFileTarget::ClientResources, true))));
    }

    SECTION("RefusesATruncatedOrTrailingDescriptor")
    {
        vector<uint8_t> desc = write_one(make_entry("Art.fores", UpdateFileTarget::ClientResources, true));
        vector<uint8_t> truncated {desc.begin(), desc.end() - 3};
        vector<uint8_t> trailing = desc;
        trailing.emplace_back(0);
        CHECK_THROWS(ReadUpdateDescriptor(truncated));
        CHECK_THROWS(ReadUpdateDescriptor(trailing));
    }
}

FO_END_NAMESPACE
