//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#pragma once

#include "Common.h"

#include "ContentUpdater.h"
#include "DiskFileSystem.h"
#include "NetSockets.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

class UpdaterFastClient final
{
public:
    UpdaterFastClient() = delete;
    UpdaterFastClient(const ClientSettings& settings, const ContentUpdateManifest& manifest, const ContentUpdateFileInfo& file_info, string target_path);
    UpdaterFastClient(const UpdaterFastClient&) = delete;
    UpdaterFastClient(UpdaterFastClient&&) noexcept = delete;
    auto operator=(const UpdaterFastClient&) = delete;
    auto operator=(UpdaterFastClient&&) noexcept = delete;
    ~UpdaterFastClient() = default;

    void Process();
    void Cleanup();

    [[nodiscard]] auto IsFinished() const noexcept -> bool { return _finished; }
    [[nodiscard]] auto IsFailed() const noexcept -> bool { return _failed; }
    [[nodiscard]] auto GetError() const noexcept -> string_view { return _error; }
    [[nodiscard]] auto GetVerifiedBytes() const noexcept -> size_t { return _verifiedBytes; }
    [[nodiscard]] auto GetContiguousChunkCount() const noexcept -> uint32;
    [[nodiscard]] auto WriteContiguousData(DiskFile& temp_file) -> bool;
    [[nodiscard]] auto AssembleFile(string_view temp_file_path) -> bool;

private:
    struct ChunkState
    {
        uint32 Size {};
        uint32 Hash {};
        bool Complete {};
        bool Requested {};
        int32 AttemptCount {};
        int32 SocketIndex {-1};
        nanotime RequestTime {};
        string Path;
    };

    struct SocketState
    {
        udp_socket Socket {};
        bool Busy {};
        uint32 ChunkIndex {};
        int32 EndpointIndex {-1};
    };

    void Fail(string_view message);
    void LoadExistingChunks();
    void ProcessTimeouts();
    void ProcessReceives();
    void ProcessSends();
    void TryFinalize();

    [[nodiscard]] auto ReadChunkFile(uint32 chunk_index, vector<uint8>& data) const -> bool;
    [[nodiscard]] auto VerifyExistingChunk(uint32 chunk_index) const -> bool;
    [[nodiscard]] auto GetChunkPath(uint32 chunk_index) const -> string;
    [[nodiscard]] auto FindNextChunkToRequest() const noexcept -> int32;
    [[nodiscard]] auto GetChunkExpectedSize(uint32 chunk_index) const noexcept -> uint32;
    [[nodiscard]] auto GetChunkEndpointIndex(const ChunkState& chunk) const -> int32;

    raw_ptr<const ClientSettings> _settings;
    uint32 _sessionId {};
    uint32 _fileIndex {};
    uint32 _fileSize {};
    uint32 _fileHash {};
    uint32 _chunkSize {};
    string _targetPath;
    vector<ContentUpdateEndpoint> _endpoints {};
    vector<ChunkState> _chunks {};
    vector<SocketState> _sockets {};
    vector<uint8> _receiveBuf {};
    size_t _verifiedBytes {};
    bool _finished {};
    bool _failed {};
    string _error;
};

FO_END_NAMESPACE