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

#pragma once

#include "Common.h"

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

    void LoadFromClientResources(const GlobalSettings& settings);
    void ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size);

private:
    struct UpdateFileData
    {
        bool InMemory {};
        string DiskPath;
        vector<uint8_t> MemoryData {};
        uint64_t Size {};
        uint64_t Hash {};
    };

    struct UpdateFileInfo
    {
        uint32_t FileIndex {};
        string ClientPath;
        UpdateFileTarget Target {UpdateFileTarget::ClientResources};
    };

    vector<UpdateFileData> _updateFiles {};
    vector<UpdateFileInfo> _commonUpdateFiles {};
    vector<uint8_t> _commonUpdateFilesDesc {};
    map<string, vector<UpdateFileInfo>> _binaryTargetUpdateFiles {};
    map<string, vector<uint8_t>> _binaryTargetUpdateFilesDesc {};
};

FO_END_NAMESPACE
