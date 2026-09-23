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

#include "UpdaterBackend.h"
#include "DataSerialization.h"
#include "DiskFileSystem.h"
#include "FileSystem.h"
#include "Logging.h"
#include "MetadataRegistration.h"
#include "Player.h"
#include "ResourcePack.h"
#include "SafeArithmetics.h"
#include "ServerConnection.h"
#include "StringUtils.h"
#include "UpdateDescriptor.h"

FO_BEGIN_NAMESPACE

void UpdaterBackend::LoadFromClientResources(const GlobalSettings& settings, string_view server_metadata_version)
{
    FO_STACK_TRACE_ENTRY();

    logging::write("Load client data packs for synchronization");

    // Built into locals and swapped in by a no-throw tail, so a throw mid-load leaves every member untouched
    // instead of half-rebuilt
    vector<UpdateFileData> update_files;
    vector<UpdateFileInfo> common_update_files;
    vector<uint8_t> common_update_files_desc;
    map<string, vector<UpdateFileInfo>> platform_target_update_files;
    map<string, vector<uint8_t>> platform_target_update_files_desc;
    set<string> client_resource_pack_names;

    auto add_sync_file = [&settings, &update_files](string_view disk_path, string_view client_path, UpdateFileTarget target) -> UpdateFileInfo {
        UpdateFileData data {};

        data.File = fs::disk_read_file {disk_path};
        FO_VERIFY_AND_THROW(data.File, "Client update file is missing", disk_path);
        data.DiskPath = string(disk_path);
        data.Size = data.File.get_size();

        bool is_resource_pack = target == UpdateFileTarget::ClientResources;

        if (is_resource_pack) {
            ResourcePackHeader pack_header;
            FO_VERIFY_AND_THROW(ReadResourcePackHeader(data.File, pack_header), "Client update resource header is invalid", disk_path);
            data.Hash = pack_header.PackHash;
            data.PackHeader = pack_header;
        }

        if (settings.ServerNetwork.UpdateFilesInMemory) {
            data.InMemory = true;
            data.MemoryData.resize(numeric_cast<size_t>(data.Size));
            FO_VERIFY_AND_THROW(data.File.read_at(0, data.MemoryData), "Can't read client update file", disk_path);

            if (!is_resource_pack) {
                data.Hash = fs::hash_data(data.MemoryData);
            }

            data.File.close();
        }
        else if (!is_resource_pack) {
            auto hash = fs::hash_file(disk_path);
            FO_VERIFY_AND_THROW(hash, "Can't hash client update file", disk_path);
            data.Hash = *hash;
        }

        update_files.emplace_back(std::move(data));

        UpdateFileInfo info {};
        info.FileIndex = numeric_cast<uint32_t>(update_files.size() - 1);
        info.ClientPath = string(client_path);
        info.Target = target;
        return info;
    };

    auto client_resources_dir = std::filesystem::path {fs::make_path(settings.Baking.ClientResources)};

    for (const auto& resource_entry : settings.GetClientResourcePacks()) {
        if (resource_entry != EMBEDDED_PACK_NAME) {
            string pack_name = strex("{}.fores", resource_entry).str();
            client_resource_pack_names.emplace(pack_name);
            string pack_disk_path = fs::path_to_string(client_resources_dir / fs::make_path(pack_name));
            auto info = add_sync_file(pack_disk_path, pack_name, UpdateFileTarget::ClientResources);
            common_update_files.emplace_back(std::move(info));
        }
    }

    VerifyClientResourcesMetadata(settings, server_metadata_version);

    auto platform_binaries_dir = std::filesystem::path {fs::make_path(settings.Baking.PlatformBinaries)};
    string platform_binaries_path = fs::path_to_string(platform_binaries_dir);

    if (std::filesystem::exists(platform_binaries_dir)) {
        FO_VERIFY_AND_THROW(std::filesystem::is_directory(platform_binaries_dir), "Platform binaries path exists but is not a directory", platform_binaries_path);

        for (const auto& platform_entry : std::filesystem::directory_iterator {platform_binaries_dir}) {
            if (!platform_entry.is_directory()) {
                continue;
            }

            string binary_target_name = fs::path_to_string(platform_entry.path().filename());
            FO_VERIFY_AND_THROW(!binary_target_name.empty(), "Updater backend found a platform binaries directory entry with an empty target name", platform_binaries_path, fs::path_to_string(platform_entry.path()));

            for (const auto& binary_entry : std::filesystem::recursive_directory_iterator {platform_entry.path()}) {
                FO_VERIFY_AND_THROW(binary_entry.is_regular_file(), "Updater backend binary target contains a non-file entry", binary_target_name, fs::path_to_string(binary_entry.path()));
                string disk_path = fs::path_to_string(binary_entry.path());
                string client_file_name = fs::path_to_string(binary_entry.path().filename());
                auto target = client_resource_pack_names.contains(client_file_name) ? UpdateFileTarget::ClientResources : UpdateFileTarget::ClientBinaries;
                auto info = add_sync_file(disk_path, client_file_name, target);
                platform_target_update_files[string(binary_target_name)].emplace_back(std::move(info));
            }
        }
    }

    auto build_update_desc = [&update_files, &common_update_files](vector<uint8_t>& desc, nptr<const vector<UpdateFileInfo>> platform_files) {
        vector<UpdateDescriptorEntry> entries;

        auto add_entry = [&update_files, &entries](const UpdateFileInfo& info) {
            const auto& data = update_files[info.FileIndex];
            UpdateDescriptorEntry entry;
            entry.Name = info.ClientPath;
            entry.Size = data.Size;
            entry.Hash = data.Hash;
            entry.Target = info.Target;
            entry.FileIndex = info.FileIndex;
            entry.PackHeader = data.PackHeader;
            entries.emplace_back(std::move(entry));
        };

        for (const auto& info : common_update_files) {
            bool overridden = platform_files && std::ranges::any_of(*platform_files, [&info](const UpdateFileInfo& platform_info) { return platform_info.Target == info.Target && platform_info.ClientPath == info.ClientPath; });

            if (!overridden) {
                add_entry(info);
            }
        }

        if (platform_files) {
            for (const auto& info : *platform_files) {
                add_entry(info);
            }
        }

        WriteUpdateDescriptor(desc, entries);
    };

    build_update_desc(common_update_files_desc, nullptr);

    for (auto& [binary_target_name, files] : platform_target_update_files) {
        auto& desc = platform_target_update_files_desc[binary_target_name];
        build_update_desc(desc, &files);
    }

    // Container swap is unconditionally noexcept for the engine containers and their stateless allocator, so
    // this tail cannot leave the members half-updated
    _updateFiles.swap(update_files);
    _commonUpdateFiles.swap(common_update_files);
    _commonUpdateFilesDesc.swap(common_update_files_desc);
    _platformTargetUpdateFiles.swap(platform_target_update_files);
    _platformTargetUpdateFilesDesc.swap(platform_target_update_files_desc);
}

void UpdaterBackend::VerifyClientResourcesMetadata(const GlobalSettings& settings, string_view server_metadata_version)
{
    FO_STACK_TRACE_ENTRY();

    // The server runs on its own resource directory and hands out another one, so a deploy that refreshed only
    // one of them would hand every synced client a property layout this server cannot talk to
    FileSystem client_resources;

    for (const string& name : settings.GetClientResourcePacks()) {
        if (name != EMBEDDED_PACK_NAME) {
            client_resources.AddCustomSource(safe_alloc::make_unique<ResourcePackSource>(strex(settings.Baking.ClientResources).combine_path(strex("{}.fores", name)).str()));
        }
    }

    vector<uint8_t> metadata_bin = ReadMetadataBin(&client_resources, "Client");
    string client_metadata_version = ReadMetadataVersion(metadata_bin);

    if (client_metadata_version != server_metadata_version) {
        throw UpdaterException("Distributed client resources were baked apart from the server resources", settings.Baking.ClientResources, client_metadata_version, settings.Baking.ServerResources, server_metadata_version);
    }

    logging::write("Client data packs match the server metadata version {}", client_metadata_version);
}

auto UpdaterBackend::GetUpdateDescriptor(string_view binary_target_name) const -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    auto desc_it = _platformTargetUpdateFilesDesc.find(string(binary_target_name));
    return desc_it != _platformTargetUpdateFilesDesc.end() ? desc_it->second : _commonUpdateFilesDesc;
}

void UpdaterBackend::ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size)
{
    FO_STACK_TRACE_ENTRY();

    auto connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    auto file_index = in_buf->Read<uint32_t>();
    auto start_offset = in_buf->Read<uint64_t>();
    uint64_t requested_size = in_buf->Read<uint64_t>();
    uint64_t expected_hash = in_buf->Read<uint64_t>();

    in_buf.Unlock();

    if (file_index >= _updateFiles.size()) {
        logging::write(logging::type::warning, "Wrong file index {}, from host '{}'", file_index, connection->GetHost());
        connection->HardDisconnect(DisconnectReason::UpdaterError);
        return;
    }

    if (update_file_max_portion_size <= 0) {
        logging::write(logging::type::warning, "Wrong update file max portion size {}, client host '{}'", update_file_max_portion_size, connection->GetHost());
        connection->HardDisconnect(DisconnectReason::UpdaterError);
        return;
    }

    const auto& update_file = _updateFiles[file_index];
    uint64_t file_size = update_file.Size;

    if (expected_hash != update_file.Hash || start_offset > file_size || requested_size > file_size - start_offset) {
        logging::write(logging::type::warning, "Wrong update file offset {}, file index {}, client host '{}'", start_offset, file_index, connection->GetHost());
        connection->HardDisconnect(DisconnectReason::UpdaterError);
        return;
    }

    uint64_t update_portion_limit = numeric_cast<uint64_t>(update_file_max_portion_size);
    uint64_t remaining_size = requested_size;
    uint64_t update_portion = std::min(update_portion_limit, remaining_size);
    size_t update_portion_size = numeric_cast<size_t>(update_portion);

    vector<uint8_t> disk_update_data {};

    if (update_portion_size != 0 && !update_file.InMemory) {
        disk_update_data.resize(update_portion_size);

        if (!update_file.File.read_at(start_offset, disk_update_data)) {
            logging::write("Can't read pinned client update file {} at {}", update_file.DiskPath, start_offset);
            connection->HardDisconnect(DisconnectReason::UpdaterError);
            return;
        }
    }

    const_span<uint8_t> update_data {};

    if (update_portion_size != 0) {
        if (update_file.InMemory) {
            size_t offset = numeric_cast<size_t>(start_offset);
            FO_STRONG_ASSERT(offset < update_file.MemoryData.size(), "Byte offset is past the end of the update data buffer");
            update_data = {update_file.MemoryData.data() + offset, update_portion_size};
        }
        else {
            update_data = {disk_update_data.data(), update_portion_size};
        }
    }

    player->Send_UpdateFileData(update_data);
}

FO_END_NAMESPACE
