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
    UpdaterFastClient(const ClientSettings& settings, const ContentUpdateManifest& manifest, const ContentUpdateFileInfo& file_info, string chunk_base_path);
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
    [[nodiscard]] auto GetVerifiedBytes() const noexcept -> uint64_t { return _verifiedBytes; }
    [[nodiscard]] auto GetContiguousChunkCount() const noexcept -> uint32_t;
    [[nodiscard]] auto WriteContiguousData(std::ostream& temp_file, uint64_t& written_bytes) -> bool;
    [[nodiscard]] auto AssembleFile(string_view temp_file_path) -> bool;

private:
    struct ChunkState
    {
        uint32_t Size {};
        uint64_t Hash {};
        bool Complete {};
        bool Requested {};
        int32_t AttemptCount {};
        nanotime RequestTime {};
        string Path;
    };

    struct SocketState
    {
        udp_socket Socket {};
        ContentUpdateFastCookie Cookie {};
        uint64_t ClientNonce {};
        int64_t CookieExpiresAt {};
        bool Busy {};
        uint32_t ChunkIndex {};
    };

    void Fail(string_view message);
    void LoadExistingChunks();
    void ProcessTimeouts();
    void ProcessReceives();
    void ProcessSends();
    void TryFinalize();

    [[nodiscard]] auto ReadChunkFile(uint32_t chunk_index, vector<uint8_t>& data) const -> bool;
    [[nodiscard]] auto ReadVerifiedChunkFile(uint32_t chunk_index, vector<uint8_t>& data) const -> bool;
    [[nodiscard]] auto VerifyExistingChunk(uint32_t chunk_index) const -> bool;
    [[nodiscard]] auto GetChunkPath(uint32_t chunk_index) const -> string;
    [[nodiscard]] auto FindNextChunkToRequest() const noexcept -> int32_t;
    [[nodiscard]] auto GetChunkEndpointIndex(const ChunkState& chunk) const -> int32_t;
    [[nodiscard]] auto GetMaxAttempts() const -> int32_t;

    ptr<const ClientSettings> _settings;
    uint32_t _sessionId {};
    uint32_t _fileIndex {};
    uint64_t _fileSize {};
    uint64_t _fileHash {};
    uint32_t _chunkSize {};
    string _chunkBasePath;
    vector<ContentUpdateEndpoint> _endpoints {};
    vector<ChunkState> _chunks {};
    vector<SocketState> _sockets {};
    vector<uint8_t> _receiveBuf {};
    uint64_t _verifiedBytes {};
    bool _finished {};
    bool _failed {};
    string _error;
};

FO_END_NAMESPACE
