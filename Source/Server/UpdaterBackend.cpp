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

#include "UpdaterBackend.h"
#include "DataSerialization.h"
#include "DiskFileSystem.h"
#include "Logging.h"
#include "Player.h"
#include "SafeArithmetics.h"
#include "ServerConnection.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE

void UpdaterBackend::LoadFromClientResources(const GlobalSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Load client data packs for synchronization");

    // Build the update state into function-locals first, then swap it into the members in a no-throw
    // commit tail. This keeps the load strongly exception-safe: any throw mid-load unwinds the locals
    // and leaves all five members untouched instead of half-cleared/half-rebuilt.
    vector<UpdateFileData> update_files;
    vector<UpdateFileInfo> common_update_files;
    vector<uint8_t> common_update_files_desc;
    map<string, vector<UpdateFileInfo>> binary_target_update_files;
    map<string, vector<uint8_t>> binary_target_update_files_desc;

    auto add_sync_file = [&settings, &update_files](string_view disk_path, string_view client_path, UpdateFileTarget target) -> UpdateFileInfo {
        UpdateFileData data {};

        auto file = fs_open_ifstream(disk_path);

        if (!file) {
            throw UpdaterException("Resource pack for client not found", disk_path);
        }

        size_t file_size = stream_get_size(file);

        if (settings.UpdateFilesInMemory) {
            data.InMemory = true;
            data.MemoryData.resize(file_size);

            if (!stream_read_exact(file, data.MemoryData)) {
                throw UpdaterException("Can't read resource pack for client", disk_path);
            }

            data.Size = numeric_cast<uint64_t>(file_size);
            data.Hash = fs_hash_data(data.MemoryData);
        }
        else {
            data.DiskPath = string(disk_path);
            data.Size = numeric_cast<uint64_t>(file_size);
            auto file_hash = fs_hash_file(disk_path);

            if (!file_hash.has_value()) {
                throw UpdaterException("Can't hash resource pack for client", disk_path);
            }

            data.Hash = *file_hash;
        }

        update_files.emplace_back(std::move(data));

        UpdateFileInfo info {};
        info.FileIndex = numeric_cast<uint32_t>(update_files.size() - 1);
        info.ClientPath = string(client_path);
        info.Target = target;
        return info;
    };

    auto client_resources_dir = std::filesystem::path {fs_make_path(settings.ClientResources)};

    for (const auto& resource_entry : settings.ClientResourceEntries) {
        if (resource_entry != "Embedded") {
            string pack_name = strex("{}.zip", resource_entry).str();
            string pack_disk_path = fs_path_to_string(client_resources_dir / pack_name);
            auto info = add_sync_file(pack_disk_path, pack_name, UpdateFileTarget::ClientResources);
            common_update_files.emplace_back(std::move(info));
        }
    }

    auto platform_binaries_dir = std::filesystem::path {fs_make_path(settings.PlatformBinaries)};
    string platform_binaries_path = fs_path_to_string(platform_binaries_dir);

    if (std::filesystem::exists(platform_binaries_dir)) {
        FO_VERIFY_AND_THROW(std::filesystem::is_directory(platform_binaries_dir), "Platform binaries path exists but is not a directory", platform_binaries_path);

        for (const auto& platform_entry : std::filesystem::directory_iterator {platform_binaries_dir}) {
            if (!platform_entry.is_directory()) {
                continue;
            }

            string binary_target_name = fs_path_to_string(platform_entry.path().filename());
            FO_VERIFY_AND_THROW(!binary_target_name.empty(), "Updater backend found a platform binaries directory entry with an empty target name", platform_binaries_path, fs_path_to_string(platform_entry.path()));

            for (const auto& binary_entry : std::filesystem::recursive_directory_iterator {platform_entry.path()}) {
                FO_VERIFY_AND_THROW(binary_entry.is_regular_file(), "Updater backend binary target contains a non-file entry", binary_target_name, fs_path_to_string(binary_entry.path()));
                string disk_path = fs_path_to_string(binary_entry.path());
                string client_file_name = fs_path_to_string(binary_entry.path().filename());
                auto info = add_sync_file(disk_path, client_file_name, UpdateFileTarget::ClientBinaries);
                binary_target_update_files[string(binary_target_name)].emplace_back(std::move(info));
            }
        }
    }

    auto build_update_desc = [&update_files, &common_update_files](vector<uint8_t>& desc, nptr<const vector<UpdateFileInfo>> platform_files) {
        auto writer = DataWriter(desc);

        auto write_file_info = [&update_files, &writer](const UpdateFileInfo& info) {
            const auto& data = update_files[info.FileIndex];
            writer.Write<int16_t>(numeric_cast<int16_t>(info.ClientPath.length()));
            writer.WriteStringBytes(info.ClientPath);
            writer.Write<uint64_t>(data.Size);
            writer.Write<uint64_t>(data.Hash);
            writer.Write<UpdateFileTarget>(info.Target);
            writer.Write<uint32_t>(info.FileIndex);
        };

        for (const auto& info : common_update_files) {
            write_file_info(info);
        }

        if (platform_files) {
            for (const auto& info : *platform_files) {
                write_file_info(info);
            }
        }

        writer.Write<int16_t>(const_numeric_cast<int16_t>(-1));
    };

    build_update_desc(common_update_files_desc, nullptr);

    for (auto& [binary_target_name, files] : binary_target_update_files) {
        auto& desc = binary_target_update_files_desc[binary_target_name];
        build_update_desc(desc, &files);
    }

    // No-throw commit tail: swap the fully-built locals into the members. Container swap is
    // unconditionally noexcept for the engine containers with their stateless allocator, so this
    // tail cannot throw and cannot itself leave the members in a half-updated state.
    _updateFiles.swap(update_files);
    _commonUpdateFiles.swap(common_update_files);
    _commonUpdateFilesDesc.swap(common_update_files_desc);
    _binaryTargetUpdateFiles.swap(binary_target_update_files);
    _binaryTargetUpdateFilesDesc.swap(binary_target_update_files_desc);
}

auto UpdaterBackend::GetUpdateDescriptor(string_view binary_target_name) const -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    auto desc_it = _binaryTargetUpdateFilesDesc.find(string(binary_target_name));
    return desc_it != _binaryTargetUpdateFilesDesc.end() ? desc_it->second : _commonUpdateFilesDesc;
}

void UpdaterBackend::ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size)
{
    FO_STACK_TRACE_ENTRY();

    auto connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    auto file_index = in_buf->Read<uint32_t>();
    auto start_offset = in_buf->Read<uint64_t>();

    in_buf.Unlock();

    if (file_index >= _updateFiles.size()) {
        WriteLog(LogType::Warning, "Wrong file index {}, from host '{}'", file_index, connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    if (update_file_max_portion_size <= 0) {
        WriteLog(LogType::Warning, "Wrong update file max portion size {}, client host '{}'", update_file_max_portion_size, connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    const auto& update_file = _updateFiles[file_index];
    uint64_t file_size = update_file.Size;

    if (start_offset > file_size) {
        WriteLog(LogType::Warning, "Wrong update file offset {}, file index {}, client host '{}'", start_offset, file_index, connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    uint64_t update_portion_limit = numeric_cast<uint64_t>(update_file_max_portion_size);
    uint64_t remaining_size = file_size - start_offset;
    uint64_t update_portion = std::min(update_portion_limit, remaining_size);
    size_t update_portion_size = numeric_cast<size_t>(update_portion);

    vector<uint8_t> disk_update_data {};

    if (update_portion_size != 0 && !update_file.InMemory) {
        disk_update_data.resize(update_portion_size);

        auto read_update_file_portion = [](string_view disk_path, uint64_t start_offset, vector<uint8_t>& data) {
            FO_STACK_TRACE_ENTRY();

            auto file = fs_open_ifstream(disk_path);

            if (!file) {
                return false;
            }

            file.seekg(numeric_cast<std::streamoff>(start_offset), std::ios::beg);

            if (!file) {
                return false;
            }

            return stream_read_exact(file, data);
        };

        if (!read_update_file_portion(update_file.DiskPath, start_offset, disk_update_data)) {
            WriteLog(LogType::Warning, "Can't read update file '{}', file index {}, client host '{}'", update_file.DiskPath, file_index, connection->GetHost());
            connection->HardDisconnect();
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
