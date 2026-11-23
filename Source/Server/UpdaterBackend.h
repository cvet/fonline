// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ResourcePack.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(UpdaterException);

class Player;

class UpdaterBackend final
{
public:
    UpdaterBackend() = default;
    UpdaterBackend(const UpdaterBackend&) = delete;
    UpdaterBackend(UpdaterBackend&&) = delete;
    auto operator=(const UpdaterBackend&) -> UpdaterBackend& = delete;
    auto operator=(UpdaterBackend&&) -> UpdaterBackend& = delete;

    [[nodiscard]] auto GetUpdateDescriptor(string_view binary_target_name) const -> const_span<uint8_t>;

    void LoadFromClientResources(const GlobalSettings& settings, string_view server_metadata_version);
    void ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size);

private:
    static void VerifyClientResourcesMetadata(const GlobalSettings& settings, string_view server_metadata_version);

    struct UpdateFileData
    {
        bool InMemory {};
        string DiskPath {};
        vector<uint8_t> MemoryData {};
        uint64_t Size {};
        uint64_t Hash {};
        optional<ResourcePackHeader> PackHeader {};
        fs::disk_read_file File {};
    };

    struct UpdateFileInfo
    {
        uint32_t FileIndex {};
        string ClientPath {};
        UpdateFileTarget Target {UpdateFileTarget::ClientResources};
    };

    vector<UpdateFileData> _updateFiles {};
    vector<UpdateFileInfo> _commonUpdateFiles {};
    vector<uint8_t> _commonUpdateFilesDesc {};
    map<string, vector<UpdateFileInfo>> _platformTargetUpdateFiles {};
    map<string, vector<uint8_t>> _platformTargetUpdateFilesDesc {};
};

FO_END_NAMESPACE
