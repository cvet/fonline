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
#include "Test_BakerHelpers.h"
#include "UpdaterBackend.h"

FO_BEGIN_NAMESPACE

namespace UpdaterBackendTests
{
    static auto MakeTempDir(string_view name) -> string
    {
        auto base = std::filesystem::temp_directory_path() / std::format("lf_updater_backend_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
        return fs_path_to_string(base);
    }

    struct DescriptorEntry
    {
        string Name;
        uint64_t Size {};
        uint64_t Hash {};
        UpdateFileTarget Target {};
    };

    static auto ReadDescriptor(const_span<uint8_t> data) -> vector<DescriptorEntry>
    {
        vector<DescriptorEntry> entries;
        DataReader reader {data};

        while (true) {
            int16_t name_size = reader.Read<int16_t>();
            if (name_size == -1) {
                break;
            }

            REQUIRE(name_size > 0);
            DescriptorEntry entry;
            entry.Name.resize(numeric_cast<size_t>(name_size));
            reader.ReadStringBytes(entry.Name);
            entry.Size = reader.Read<uint64_t>();
            entry.Hash = reader.Read<uint64_t>();
            entry.Target = reader.Read<UpdateFileTarget>();
            ignore_unused(reader.Read<uint32_t>());
            entries.emplace_back(std::move(entry));
        }

        reader.VerifyEnd();
        return entries;
    }

    static auto HashString(string_view value) noexcept -> uint64_t
    {
        return fs_hash_data({reinterpret_cast<const uint8_t*>(value.data()), value.size()});
    }
}

TEST_CASE("UpdaterBackendUsesPlatformSpecificResourcePackInsteadOfCommonPack")
{
    using namespace UpdaterBackendTests;

    string root_dir = MakeTempDir("platform_resource");
    string client_resources_dir = strex(root_dir).combine_path("ClientResources").str();
    string common_scripts_dir = strex(client_resources_dir).combine_path("Scripts").str();
    string platform_binaries_dir = strex(root_dir).combine_path("PlatformBinaries").str();
    string windows_target_dir = strex(platform_binaries_dir).combine_path("Windows-win64").str();
    auto cleanup = scope_exit([&root_dir]() noexcept { (void)fs_remove_dir_tree(root_dir); });

    REQUIRE(fs_create_directories(common_scripts_dir));
    REQUIRE(fs_create_directories(windows_target_dir));

    vector<uint8_t> metadata = BakerTests::MakeEmptyMetadataBlob();
    constexpr string_view common_scripts = "common-scripts-zip";
    constexpr string_view windows_scripts = "windows-target-scripts";
    constexpr string_view windows_runtime = "windows-runtime-dll";
    REQUIRE(fs_write_file(strex(client_resources_dir).combine_path("Scripts.zip").str(), common_scripts));
    REQUIRE(fs_write_file(strex(common_scripts_dir).combine_path("Metadata.fometa-client").str(), metadata));
    REQUIRE(fs_write_file(strex(windows_target_dir).combine_path("Scripts.zip").str(), windows_scripts));
    REQUIRE(fs_write_file(strex(windows_target_dir).combine_path("Game.dll").str(), windows_runtime));

    auto settings = GlobalSettings(false);
    settings.ApplyDefaultSettings();
    BakerTests::OverrideSetting(settings.ClientResources, client_resources_dir);
    BakerTests::OverrideSetting(settings.PlatformBinaries, platform_binaries_dir);
    BakerTests::OverrideSetting(settings.ClientResourceEntries, vector<string> {"Scripts"});
    BakerTests::OverrideSetting(settings.UpdateFilesInMemory, true);

    UpdaterBackend updater_backend;
    updater_backend.LoadFromClientResources(settings, BakerTests::TEST_METADATA_VERSION);

    auto common_entries = ReadDescriptor(updater_backend.GetUpdateDescriptor("Unknown-target"));
    REQUIRE(common_entries.size() == 1);
    CHECK(common_entries.front().Name == "Scripts.zip");
    CHECK(common_entries.front().Hash == HashString(common_scripts));
    CHECK(common_entries.front().Target == UpdateFileTarget::ClientResources);

    auto windows_entries = ReadDescriptor(updater_backend.GetUpdateDescriptor("Windows-win64"));
    auto scripts_entries = windows_entries | std::views::filter([](const DescriptorEntry& entry) { return entry.Name == "Scripts.zip"; });
    REQUIRE(std::ranges::distance(scripts_entries) == 1);
    const DescriptorEntry& scripts_entry = *scripts_entries.begin();
    CHECK(scripts_entry.Size == windows_scripts.size());
    CHECK(scripts_entry.Hash == HashString(windows_scripts));
    CHECK(scripts_entry.Target == UpdateFileTarget::ClientResources);

    auto runtime_entry = std::ranges::find(windows_entries, "Game.dll", &DescriptorEntry::Name);
    REQUIRE(runtime_entry != windows_entries.end());
    CHECK(runtime_entry->Hash == HashString(windows_runtime));
    CHECK(runtime_entry->Target == UpdateFileTarget::ClientBinaries);
}

FO_END_NAMESPACE
