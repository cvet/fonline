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

#include "UpdaterFastClient.h"

FO_BEGIN_NAMESPACE

static auto MakeOrderedEndpoints(const ContentUpdateManifest& manifest) -> vector<ContentUpdateEndpoint>
{
    FO_STACK_TRACE_ENTRY();

    auto endpoints = manifest.Endpoints;

    std::sort(endpoints.begin(), endpoints.end(), [](const ContentUpdateEndpoint& left, const ContentUpdateEndpoint& right) {
        return left.Priority > right.Priority;
    });

    return endpoints;
}

static auto ReadWholeDiskFile(string_view path, vector<uint8>& data) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto file = DiskFileSystem::OpenFile(path, false);

    if (!file) {
        return false;
    }

    const size_t file_size = file.GetSize();
    data.resize(file_size);
    return file.Read(data.data(), file_size);
}

UpdaterFastClient::UpdaterFastClient(const ClientSettings& settings, const ContentUpdateManifest& manifest, const ContentUpdateFileInfo& file_info, string target_path) :
    _settings {&settings},
    _sessionId {manifest.SessionId},
    _fileSize {file_info.Size},
    _fileHash {file_info.Hash},
    _chunkSize {manifest.ChunkSize},
    _targetPath {std::move(target_path)},
    _endpoints {MakeOrderedEndpoints(manifest)}
{
    FO_STACK_TRACE_ENTRY();

    _receiveBuf.resize(_chunkSize + 64);
    _fileIndex = numeric_cast<uint32>(&file_info - manifest.Files.data());

    _chunks.reserve(file_info.ChunkHashes.size());

    for (const auto chunk_index : iterate_range(file_info.ChunkHashes.size())) {
        ChunkState chunk;
        chunk.Hash = file_info.ChunkHashes[chunk_index];
        chunk.Size = GetContentUpdateChunkSize(_fileSize, _chunkSize, numeric_cast<uint32>(chunk_index));
        chunk.Path = GetChunkPath(numeric_cast<uint32>(chunk_index));
        _chunks.emplace_back(std::move(chunk));
    }

    const size_t sockets_count = std::min(_endpoints.size(), numeric_cast<size_t>(std::max(_settings->FastUpdateMaxSockets, 1)));
    _sockets.resize(sockets_count);

    size_t alive_sockets = 0;

    for (auto& socket_state : _sockets) {
        if (socket_state.Socket.bind({}, 0, false)) {
            alive_sockets++;
        }
    }

    if (alive_sockets == 0) {
        Fail("Fast updater UDP sockets are unavailable");
        return;
    }

    LoadExistingChunks();
    TryFinalize();
}

void UpdaterFastClient::Process()
{
    FO_STACK_TRACE_ENTRY();

    if (_failed || _finished) {
        return;
    }

    ProcessTimeouts();

    if (_failed || _finished) {
        return;
    }

    ProcessReceives();

    if (_failed || _finished) {
        return;
    }

    ProcessSends();
    TryFinalize();
}

void UpdaterFastClient::Cleanup()
{
    FO_STACK_TRACE_ENTRY();

    for (const auto chunk_index : iterate_range(_chunks.size())) {
        DiskFileSystem::DeleteFile(GetChunkPath(numeric_cast<uint32>(chunk_index)));
    }
}

auto UpdaterFastClient::GetContiguousChunkCount() const noexcept -> uint32
{
    FO_STACK_TRACE_ENTRY();

    uint32 result = 0;

    for (const auto& chunk : _chunks) {
        if (!chunk.Complete) {
            break;
        }

        result++;
    }

    return result;
}

auto UpdaterFastClient::WriteContiguousData(DiskFile& temp_file) -> bool
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8> chunk_data;
    const uint32 contiguous_chunks = GetContiguousChunkCount();

    for (const auto chunk_index : iterate_range(contiguous_chunks)) {
        if (!ReadChunkFile(numeric_cast<uint32>(chunk_index), chunk_data)) {
            return false;
        }

        if (!temp_file.Write(chunk_data)) {
            return false;
        }
    }

    Cleanup();
    return true;
}

auto UpdaterFastClient::AssembleFile(string_view temp_file_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto temp_file = DiskFileSystem::OpenFile(temp_file_path, true);

    if (!temp_file) {
        return false;
    }

    vector<uint8> chunk_data;

    for (const auto chunk_index : iterate_range(_chunks.size())) {
        if (!ReadChunkFile(numeric_cast<uint32>(chunk_index), chunk_data)) {
            return false;
        }

        if (!temp_file.Write(chunk_data)) {
            return false;
        }
    }

    Cleanup();
    return true;
}

void UpdaterFastClient::Fail(string_view message)
{
    FO_STACK_TRACE_ENTRY();

    _failed = true;
    _error = message;
}

void UpdaterFastClient::LoadExistingChunks()
{
    FO_STACK_TRACE_ENTRY();

    for (const auto chunk_index : iterate_range(_chunks.size())) {
        if (VerifyExistingChunk(numeric_cast<uint32>(chunk_index))) {
            _chunks[chunk_index].Complete = true;
            _verifiedBytes += _chunks[chunk_index].Size;
        }
        else {
            DiskFileSystem::DeleteFile(GetChunkPath(numeric_cast<uint32>(chunk_index)));
        }
    }
}

void UpdaterFastClient::ProcessTimeouts()
{
    FO_STACK_TRACE_ENTRY();

    const auto now = nanotime::now();
    const auto timeout = std::chrono::milliseconds {_settings->FastUpdateRequestTimeout};

    for (int32 socket_index = 0; socket_index < numeric_cast<int32>(_sockets.size()); socket_index++) {
        auto& socket_state = _sockets[numeric_cast<size_t>(socket_index)];

        if (!socket_state.Busy) {
            continue;
        }

        auto& chunk = _chunks[socket_state.ChunkIndex];

        if (chunk.Complete) {
            socket_state.Busy = false;
            continue;
        }

        if (now - chunk.RequestTime < timeout) {
            continue;
        }

        chunk.Requested = false;
        chunk.SocketIndex = -1;
        chunk.AttemptCount++;
        socket_state.Busy = false;

        const int32 max_attempts = std::max(_settings->FastUpdateMaxRetries, 1) * numeric_cast<int32>(_endpoints.size());

        if (chunk.AttemptCount >= max_attempts) {
            Fail("Fast updater request timeout");
            return;
        }
    }
}

void UpdaterFastClient::ProcessReceives()
{
    FO_STACK_TRACE_ENTRY();

    for (int32 socket_index = 0; socket_index < numeric_cast<int32>(_sockets.size()); socket_index++) {
        auto& socket_state = _sockets[numeric_cast<size_t>(socket_index)];

        while (socket_state.Socket.can_read({})) {
            string remote_host;
            uint16 remote_port = 0;
            const int32 received = socket_state.Socket.receive_from(_receiveBuf, remote_host, remote_port);

            if (received <= 0) {
                break;
            }

            ignore_unused(remote_host);
            ignore_unused(remote_port);

            ContentUpdateChunkDataHeader header;
            const uint8* payload_data = nullptr;
            size_t payload_size = 0;

            if (!TryReadContentUpdateChunkData({_receiveBuf.data(), numeric_cast<size_t>(received)}, header, payload_data, payload_size)) {
                continue;
            }

            if (header.SessionId != _sessionId || header.FileIndex != _fileIndex || header.ChunkIndex >= _chunks.size()) {
                continue;
            }

            auto& chunk = _chunks[header.ChunkIndex];

            if (header.ChunkSize != chunk.Size || header.ChunkHash != chunk.Hash || payload_size != chunk.Size) {
                continue;
            }

            const uint32 chunk_hash = Hashing::MurmurHash2(payload_data, payload_size);

            if (chunk_hash != chunk.Hash) {
                continue;
            }

            if (!chunk.Complete && !DiskFileSystem::WriteFile(chunk.Path, {payload_data, payload_size})) {
                Fail("Fast updater chunk write failed");
                return;
            }

            if (!chunk.Complete) {
                chunk.Complete = true;
                _verifiedBytes += chunk.Size;
            }

            chunk.Requested = false;
            chunk.SocketIndex = -1;

            if (socket_state.Busy && socket_state.ChunkIndex == header.ChunkIndex) {
                socket_state.Busy = false;
            }
        }
    }
}

void UpdaterFastClient::ProcessSends()
{
    FO_STACK_TRACE_ENTRY();

    for (int32 socket_index = 0; socket_index < numeric_cast<int32>(_sockets.size()); socket_index++) {
        auto& socket_state = _sockets[numeric_cast<size_t>(socket_index)];

        if (socket_state.Busy) {
            continue;
        }

        const int32 next_chunk_index = FindNextChunkToRequest();

        if (next_chunk_index < 0) {
            return;
        }

        auto& chunk = _chunks[numeric_cast<size_t>(next_chunk_index)];
        const int32 endpoint_index = GetChunkEndpointIndex(chunk);

        FO_RUNTIME_ASSERT(endpoint_index >= 0);
        FO_RUNTIME_ASSERT(endpoint_index < numeric_cast<int32>(_endpoints.size()));

        const auto request_data = MakeContentUpdateChunkRequestData(ContentUpdateChunkRequest {
            .SessionId = _sessionId,
            .FileIndex = _fileIndex,
            .ChunkIndex = numeric_cast<uint32>(next_chunk_index),
        });

        const auto& endpoint = _endpoints[numeric_cast<size_t>(endpoint_index)];
        const int32 sent = socket_state.Socket.send_to(endpoint.Host, endpoint.Port, request_data);

        if (sent != numeric_cast<int32>(request_data.size())) {
            chunk.AttemptCount++;

            const int32 max_attempts = std::max(_settings->FastUpdateMaxRetries, 1) * numeric_cast<int32>(_endpoints.size());

            if (chunk.AttemptCount >= max_attempts) {
                Fail("Fast updater send failed");
                return;
            }

            continue;
        }

        chunk.Requested = true;
        chunk.SocketIndex = socket_index;
        chunk.RequestTime = nanotime::now();
        socket_state.Busy = true;
        socket_state.ChunkIndex = numeric_cast<uint32>(next_chunk_index);
        socket_state.EndpointIndex = endpoint_index;
    }
}

void UpdaterFastClient::TryFinalize()
{
    FO_STACK_TRACE_ENTRY();

    if (_finished || _failed) {
        return;
    }

    for (const auto& chunk : _chunks) {
        if (!chunk.Complete) {
            return;
        }
    }

    _finished = true;
}

auto UpdaterFastClient::ReadChunkFile(uint32 chunk_index, vector<uint8>& data) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(chunk_index < _chunks.size());
    return ReadWholeDiskFile(GetChunkPath(chunk_index), data);
}

auto UpdaterFastClient::VerifyExistingChunk(uint32 chunk_index) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8> data;

    if (!ReadChunkFile(chunk_index, data)) {
        return false;
    }

    const auto& chunk = _chunks[chunk_index];

    if (data.size() != chunk.Size) {
        return false;
    }

    return Hashing::MurmurHash2(data.data(), data.size()) == chunk.Hash;
}

auto UpdaterFastClient::GetChunkPath(uint32 chunk_index) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("{}.__fastupd.{}", _targetPath, chunk_index);
}

auto UpdaterFastClient::FindNextChunkToRequest() const noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    for (int32 chunk_index = 0; chunk_index < numeric_cast<int32>(_chunks.size()); chunk_index++) {
        const auto& chunk = _chunks[numeric_cast<size_t>(chunk_index)];

        if (!chunk.Complete && !chunk.Requested) {
            return chunk_index;
        }
    }

    return -1;
}

auto UpdaterFastClient::GetChunkExpectedSize(uint32 chunk_index) const noexcept -> uint32
{
    FO_STACK_TRACE_ENTRY();

    return GetContentUpdateChunkSize(_fileSize, _chunkSize, chunk_index);
}

auto UpdaterFastClient::GetChunkEndpointIndex(const ChunkState& chunk) const -> int32
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_endpoints.empty());
    return chunk.AttemptCount % numeric_cast<int32>(_endpoints.size());
}

FO_END_NAMESPACE