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
#include "UpdaterBackend.h"

FO_BEGIN_NAMESPACE

namespace UpdaterBackendTests
{
    static auto MakeTempDir(string_view name) -> string
    {
        auto base = std::filesystem::temp_directory_path() / std::format("lf_updater_backend_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
        return fs::path_to_string(base);
    }

    struct DescriptorEntry
    {
        string Name;
        uint64_t Size {};
        uint64_t Hash {};
        UpdateFileTarget Target {};
        uint32_t PackHeaderSize {};
    };

    static auto ReadDescriptor(const_span<uint8_t> data) -> vector<DescriptorEntry>
    {
        vector<DescriptorEntry> entries;
        data_reader reader {data};

        while (true) {
            int16_t name_size = reader.read<int16_t>();
            if (name_size == -1) {
                break;
            }

            REQUIRE(name_size > 0);
            DescriptorEntry entry;
            entry.Name.resize(numeric_cast<size_t>(name_size));
            reader.read_string_bytes(entry.Name);
            entry.Size = reader.read<uint64_t>();
            entry.Hash = reader.read<uint64_t>();
            entry.Target = reader.read<UpdateFileTarget>();
            ignore_unused(reader.read<uint32_t>());
            entry.PackHeaderSize = reader.read<uint32_t>();
            ignore_unused(reader.read_bytes(entry.PackHeaderSize));
            entries.emplace_back(std::move(entry));
        }

        reader.verify_end();
        return entries;
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
    BakerTests::OverrideSetting(settings.Baking.ClientResourceEntries, vector<string> {"Scripts"});
    BakerTests::OverrideSetting(settings.ServerNetwork.UpdateFilesInMemory, true);

    UpdaterBackend updater_backend;
    updater_backend.LoadFromClientResources(settings, BakerTests::TEST_METADATA_VERSION);

    auto common_entries = ReadDescriptor(updater_backend.GetUpdateDescriptor("Unknown-target"));
    REQUIRE(common_entries.size() == 1);
    CHECK(common_entries.front().Name == "Scripts.fores");
    CHECK(common_entries.front().Hash == common_header.PackHash);
    CHECK(common_entries.front().Target == UpdateFileTarget::ClientResources);
    CHECK(common_entries.front().PackHeaderSize == RESOURCE_PACK_HEADER_SIZE);

    auto windows_entries = ReadDescriptor(updater_backend.GetUpdateDescriptor("Windows-win64"));
    auto scripts_entries = windows_entries | std::views::filter([](const DescriptorEntry& entry) { return entry.Name == "Scripts.fores"; });
    REQUIRE(std::ranges::distance(scripts_entries) == 1);
    const DescriptorEntry& scripts_entry = *scripts_entries.begin();
    CHECK(scripts_entry.Size == *fs::file_size(windows_pack_path));
    CHECK(scripts_entry.Hash == windows_header.PackHash);
    CHECK(scripts_entry.Target == UpdateFileTarget::ClientResources);
    CHECK(scripts_entry.PackHeaderSize == RESOURCE_PACK_HEADER_SIZE);

    auto runtime_entry = std::ranges::find(windows_entries, "Game.dll", &DescriptorEntry::Name);
    REQUIRE(runtime_entry != windows_entries.end());
    CHECK(runtime_entry->Hash == HashString(windows_runtime));
    CHECK(runtime_entry->Target == UpdateFileTarget::ClientBinaries);
    CHECK(runtime_entry->PackHeaderSize == 0);
}

FO_END_NAMESPACE
