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
#include "SafeArithmetics.h"
#include "ServerConnection.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE

void UpdaterBackend::LoadFromClientResources(const GlobalSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    _updateFiles.clear();
    _commonUpdateFiles.clear();
    _commonUpdateFilesDesc.clear();
    _binaryTargetUpdateFiles.clear();
    _binaryTargetUpdateFilesDesc.clear();

    WriteLog("Load client data packs for synchronization");

    const auto add_sync_file = [&settings, this](string_view disk_path, string_view client_path, UpdateFileTarget target) -> UpdateFileInfo {
        UpdateFileData data {};

        auto file = fs_open_ifstream(disk_path);

        if (!file) {
            throw UpdaterException("Resource pack for client not found", disk_path);
        }

        const size_t file_size = stream_get_size(file);

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
            const auto file_hash = fs_hash_file(disk_path);

            if (!file_hash.has_value()) {
                throw UpdaterException("Can't hash resource pack for client", disk_path);
            }

            data.Hash = *file_hash;
        }

        _updateFiles.emplace_back(std::move(data));

        UpdateFileInfo info {};
        info.FileIndex = numeric_cast<uint32_t>(_updateFiles.size() - 1);
        info.ClientPath = string(client_path);
        info.Target = target;
        return info;
    };

    const auto client_resources_dir = std::filesystem::path {fs_make_path(settings.ClientResources)};

    for (const auto& resource_entry : settings.ClientResourceEntries) {
        if (resource_entry != "Embedded") {
            const auto pack_name = strex("{}.zip", resource_entry).str();
            const auto pack_disk_path = fs_path_to_string(client_resources_dir / pack_name);
            auto info = add_sync_file(pack_disk_path, pack_name, UpdateFileTarget::ClientResources);
            _commonUpdateFiles.emplace_back(std::move(info));
        }
    }

    const auto platform_binaries_dir = std::filesystem::path {fs_make_path(settings.PlatformBinaries)};
    const auto platform_binaries_path = fs_path_to_string(platform_binaries_dir);

    if (std::filesystem::exists(platform_binaries_dir)) {
        FO_VERIFY_AND_THROW(std::filesystem::is_directory(platform_binaries_dir), "Platform binaries path exists but is not a directory", platform_binaries_path);

        for (const auto& platform_entry : std::filesystem::directory_iterator {platform_binaries_dir}) {
            if (!platform_entry.is_directory()) {
                continue;
            }

            const auto binary_target_name = fs_path_to_string(platform_entry.path().filename());
            FO_VERIFY_AND_THROW(!binary_target_name.empty(), "Updater backend found a platform binaries directory entry with an empty target name", platform_binaries_path, fs_path_to_string(platform_entry.path()));

            for (const auto& binary_entry : std::filesystem::recursive_directory_iterator {platform_entry.path()}) {
                FO_VERIFY_AND_THROW(binary_entry.is_regular_file(), "Updater backend binary target contains a non-file entry", binary_target_name, fs_path_to_string(binary_entry.path()));
                const auto disk_path = fs_path_to_string(binary_entry.path());
                const auto client_file_name = fs_path_to_string(binary_entry.path().filename());
                auto info = add_sync_file(disk_path, client_file_name, UpdateFileTarget::ClientBinaries);
                _binaryTargetUpdateFiles[string(binary_target_name)].emplace_back(std::move(info));
            }
        }
    }

    const auto build_update_desc = [this](vector<uint8_t>& desc, nptr<const vector<UpdateFileInfo>> platform_files) {
        auto writer = DataWriter(desc);

        const auto write_file_info = [this, &writer](const UpdateFileInfo& info) {
            const auto& data = _updateFiles[info.FileIndex];
            writer.Write<int16_t>(numeric_cast<int16_t>(info.ClientPath.length()));
            writer.WriteStringBytes(info.ClientPath);
            writer.Write<uint64_t>(data.Size);
            writer.Write<uint64_t>(data.Hash);
            writer.Write<UpdateFileTarget>(info.Target);
            writer.Write<uint32_t>(info.FileIndex);
        };

        for (const auto& info : _commonUpdateFiles) {
            write_file_info(info);
        }

        if (platform_files) {
            for (const auto& info : *platform_files) {
                write_file_info(info);
            }
        }

        writer.Write<int16_t>(const_numeric_cast<int16_t>(-1));
    };

    build_update_desc(_commonUpdateFilesDesc, nullptr);

    for (auto& [binary_target_name, files] : _binaryTargetUpdateFiles) {
        auto& desc = _binaryTargetUpdateFilesDesc[binary_target_name];
        build_update_desc(desc, &files);
    }
}

auto UpdaterBackend::GetUpdateDescriptor(string_view binary_target_name) const -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    const auto desc_it = _binaryTargetUpdateFilesDesc.find(string(binary_target_name));
    return desc_it != _binaryTargetUpdateFilesDesc.end() ? desc_it->second : _commonUpdateFilesDesc;
}

void UpdaterBackend::ProcessUpdateFile(ptr<ServerConnection> connection, int32_t update_file_max_portion_size)
{
    FO_STACK_TRACE_ENTRY();

    auto in_buf = connection->ReadBuf();

    const auto file_index = in_buf->Read<uint32_t>();
    const auto start_offset = in_buf->Read<uint64_t>();

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
    const auto file_size = update_file.Size;

    if (start_offset > file_size) {
        WriteLog(LogType::Warning, "Wrong update file offset {}, file index {}, client host '{}'", start_offset, file_index, connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    const uint64_t update_portion_limit = numeric_cast<uint64_t>(update_file_max_portion_size);
    const uint64_t remaining_size = file_size - start_offset;
    const uint64_t update_portion = std::min(update_portion_limit, remaining_size);
    const size_t update_portion_size = numeric_cast<size_t>(update_portion);

    vector<uint8_t> disk_update_data {};

    if (update_portion_size != 0 && !update_file.InMemory) {
        disk_update_data.resize(update_portion_size);

        const auto read_update_file_portion = [](string_view disk_path, uint64_t start_offset, vector<uint8_t>& data) {
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

    auto out_buf = connection->WriteMsg(NetMessage::UpdateFileData);

    out_buf->Write(numeric_cast<int32_t>(update_portion_size));

    if (update_portion_size != 0) {
        if (update_file.InMemory) {
            const size_t offset = numeric_cast<size_t>(start_offset);
            FO_STRONG_ASSERT(offset < update_file.MemoryData.size(), "Byte offset is past the end of the update data buffer");
            ptr<const uint8_t> update_data = update_file.MemoryData.data() + offset;
            out_buf->Push(update_data, update_portion_size);
        }
        else {
            out_buf->Push(disk_update_data.data(), update_portion_size);
        }
    }
}

FO_END_NAMESPACE
