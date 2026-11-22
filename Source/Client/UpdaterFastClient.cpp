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

#include "ResourcePack.h"

FO_BEGIN_NAMESPACE

static constexpr int32_t MaxFastUpdateClientSockets = 64;

static auto MakeOrderedEndpoints(const ContentUpdateManifest& manifest) -> vector<ContentUpdateEndpoint>
{
    FO_TRACE_ZONE(Core);

    auto endpoints = manifest.Endpoints;

    std::sort(endpoints.begin(), endpoints.end(), [](const ContentUpdateEndpoint& left, const ContentUpdateEndpoint& right) { return left.Priority > right.Priority; });

    return endpoints;
}

static auto ReadWholeDiskFile(string_view path, vector<uint8_t>& data) -> bool
{
    FO_TRACE_ZONE(Core);

    auto file = fs::open_ifstream(path);

    if (!file) {
        return false;
    }

    const size_t file_size = fs::stream_get_size(file);
    data.resize(file_size);
    return fs::stream_read_exact(file, data);
}

UpdaterFastClient::UpdaterFastClient(const GlobalSettings& settings, const ContentUpdateManifest& manifest, const ContentUpdateFileInfo& file_info, string chunk_base_path) :
    _settings {make_ptr(&settings)},
    _sessionId {manifest.SessionId},
    _fileIndex {file_info.FileIndex},
    _fileSize {file_info.Size},
    _fileHash {file_info.Hash},
    _fileTarget {file_info.Target},
    _chunkSize {manifest.ChunkSize},
    _chunkBasePath {std::move(chunk_base_path)},
    _endpoints {MakeOrderedEndpoints(manifest)}
{
    FO_TRACE_ZONE(Core);

    if (_endpoints.empty() || _chunkSize == 0) {
        Fail("Fast updater manifest has no usable endpoints");
        return;
    }

    if (_chunkSize > ContentUpdateMaxChunkPayloadSize) {
        Fail("Fast updater manifest chunk size is too large");
        return;
    }

    _receiveBuf.resize(numeric_cast<size_t>(_chunkSize) + 64);

    const uint32_t expected_chunk_count = GetContentUpdateChunkCount(_fileSize, _chunkSize);

    if (file_info.ChunkHashes.size() != numeric_cast<size_t>(expected_chunk_count)) {
        Fail("Fast updater manifest chunk table is inconsistent");
        return;
    }

    _chunks.reserve(file_info.ChunkHashes.size());

    for (size_t chunk_index = 0; chunk_index < file_info.ChunkHashes.size(); chunk_index++) {
        ChunkState chunk;
        chunk.Hash = file_info.ChunkHashes[chunk_index];
        chunk.Size = GetContentUpdateChunkSize(_fileSize, _chunkSize, numeric_cast<uint32_t>(chunk_index));
        chunk.Path = GetChunkPath(numeric_cast<uint32_t>(chunk_index));
        _chunks.emplace_back(std::move(chunk));
    }

    const size_t configured_sockets_count = numeric_cast<size_t>(std::clamp(_settings->Network.FastUpdateMaxSockets, 1, MaxFastUpdateClientSockets));
    const size_t useful_sockets_count = std::max(_chunks.size(), size_t {1});
    const size_t sockets_count = std::min(configured_sockets_count, useful_sockets_count);
    _sockets.reserve(sockets_count);

    for (size_t socket_index = 0; socket_index != sockets_count; ++socket_index) {
        SocketState socket_state;
        array<uint8_t, sizeof(socket_state.ClientNonce)> nonce_bytes {};

        if (!FillContentUpdateSecureRandom(nonce_bytes)) {
            Fail("Fast updater secure random source is unavailable");
            return;
        }

        memory::copy(make_ptr(&socket_state.ClientNonce), nonce_bytes.data(), nonce_bytes.size());

        if (socket_state.ClientNonce == 0) {
            socket_state.ClientNonce = numeric_cast<uint64_t>(socket_index) + 1;
        }

        if (socket_state.Socket.bind("0.0.0.0", 0, false)) {
            _sockets.emplace_back(std::move(socket_state));
        }
    }

    if (_sockets.empty()) {
        Fail("Fast updater UDP sockets are unavailable");
        return;
    }

    LoadExistingChunks();
    TryFinalize();
}

void UpdaterFastClient::Process()
{
    FO_TRACE_ZONE(Core);

    if (_failed || _finished) {
        return;
    }

    // Drain already queued replies before expiring requests. A delayed client tick must not discard
    // a valid challenge or chunk that arrived before the timeout but was not processed yet
    ProcessReceives();

    if (_failed || _finished) {
        return;
    }

    ProcessTimeouts();

    if (_failed || _finished) {
        return;
    }

    ProcessSends();
    TryFinalize();
}

void UpdaterFastClient::Cleanup()
{
    FO_TRACE_ZONE(Core);

    for (size_t chunk_index = 0; chunk_index < _chunks.size(); chunk_index++) {
        (void)fs::remove_file(GetChunkPath(numeric_cast<uint32_t>(chunk_index)));
    }
}

auto UpdaterFastClient::GetContiguousChunkCount() const noexcept -> uint32_t
{
    FO_TRACE_ZONE(Core);

    uint32_t result = 0;

    for (const auto& chunk : _chunks) {
        if (!chunk.Complete) {
            break;
        }

        result++;
    }

    return result;
}

auto UpdaterFastClient::WriteContiguousData(fs::disk_write_file& temp_file, uint64_t& written_bytes) -> bool
{
    FO_TRACE_ZONE(Core);

    written_bytes = 0;

    vector<uint8_t> chunk_data;
    const uint32_t contiguous_chunks = GetContiguousChunkCount();

    for (uint32_t chunk_index = 0; chunk_index < contiguous_chunks; chunk_index++) {
        if (!ReadVerifiedChunkFile(numeric_cast<uint32_t>(chunk_index), chunk_data)) {
            break;
        }

        if (!chunk_data.empty() && !temp_file.write(chunk_data)) {
            return false;
        }

        written_bytes += chunk_data.size();
    }

    if (!temp_file.flush()) {
        return false;
    }

    Cleanup();
    return true;
}

auto UpdaterFastClient::AssembleFile(string_view temp_file_path) -> bool
{
    FO_TRACE_ZONE(Core);

    const auto dir = strex(temp_file_path).extract_dir().str();

    if (!dir.empty() && !fs::create_directories(dir)) {
        return false;
    }

    fs::disk_write_file temp_file {temp_file_path, fs::disk_write_mode::create};

    if (!temp_file) {
        return false;
    }

    vector<uint8_t> chunk_data;

    for (size_t chunk_index = 0; chunk_index < _chunks.size(); chunk_index++) {
        if (!ReadVerifiedChunkFile(numeric_cast<uint32_t>(chunk_index), chunk_data)) {
            return false;
        }

        if (!chunk_data.empty() && !temp_file.write(chunk_data)) {
            return false;
        }
    }

    if (!temp_file.flush()) {
        return false;
    }

    temp_file.close();

    const auto assembled_size = fs::file_size(temp_file_path);

    if (!assembled_size.has_value() || *assembled_size != _fileSize) {
        return false;
    }

    if (_fileTarget == UpdateFileTarget::ClientResources) {
        if (!VerifyResourcePackFile(temp_file_path, _fileHash)) {
            return false;
        }
    }
    else {
        const auto assembled_hash = fs::hash_file(temp_file_path);

        if (!assembled_hash.has_value() || *assembled_hash != _fileHash) {
            return false;
        }
    }

    Cleanup();
    return true;
}

void UpdaterFastClient::Fail(string_view message)
{
    FO_TRACE_ZONE(Core);

    _failed = true;
    _error = message;
}

void UpdaterFastClient::LoadExistingChunks()
{
    FO_TRACE_ZONE(Core);

    for (size_t chunk_index = 0; chunk_index < _chunks.size(); chunk_index++) {
        if (VerifyExistingChunk(numeric_cast<uint32_t>(chunk_index))) {
            _chunks[chunk_index].Complete = true;
            _verifiedBytes += _chunks[chunk_index].Size;
        }
        else {
            (void)fs::remove_file(GetChunkPath(numeric_cast<uint32_t>(chunk_index)));
        }
    }
}

void UpdaterFastClient::ProcessTimeouts()
{
    FO_TRACE_ZONE(Core);

    const auto now = nanotime::now();
    const auto timeout = std::chrono::milliseconds {std::max(_settings->Network.FastUpdateRequestTimeout, 1)};

    for (int32_t socket_index = 0; socket_index < numeric_cast<int32_t>(_sockets.size()); socket_index++) {
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
        chunk.AttemptCount++;
        socket_state.Busy = false;

        if (chunk.AttemptCount >= GetMaxAttempts()) {
            Fail("Fast updater request timeout");
            return;
        }
    }
}

void UpdaterFastClient::ProcessReceives()
{
    FO_TRACE_ZONE(Core);

    for (int32_t socket_index = 0; socket_index < numeric_cast<int32_t>(_sockets.size()); socket_index++) {
        auto& socket_state = _sockets[numeric_cast<size_t>(socket_index)];

        while (socket_state.Socket.can_read()) {
            string remote_host;
            uint16_t remote_port = 0;
            const int32_t received = socket_state.Socket.receive_from({_receiveBuf.data(), _receiveBuf.size()}, remote_host, remote_port);

            if (received <= 0) {
                break;
            }

            ContentUpdateCookieChallenge challenge;

            if (TryReadContentUpdateCookieChallengeData({_receiveBuf.data(), numeric_cast<size_t>(received)}, challenge)) {
                if (socket_state.Busy && challenge.SessionId == _sessionId && challenge.FileIndex == _fileIndex && challenge.ChunkIndex == socket_state.ChunkIndex && challenge.ChunkIndex < _chunks.size() && challenge.ClientNonce == socket_state.ClientNonce && challenge.CookieExpiresAt > 0) {
                    socket_state.Cookie = challenge.Cookie;
                    socket_state.CookieExpiresAt = challenge.CookieExpiresAt;
                    _chunks[challenge.ChunkIndex].Requested = false;
                    socket_state.Busy = false;
                }

                continue;
            }

            ignore_unused(remote_host);
            ignore_unused(remote_port);

            ContentUpdateChunkDataHeader header;
            const_span<uint8_t> payload {};

            if (!TryReadContentUpdateChunkData({_receiveBuf.data(), numeric_cast<size_t>(received)}, header, payload)) {
                continue;
            }

            if (header.SessionId != _sessionId || header.FileIndex != _fileIndex || header.ChunkIndex >= _chunks.size()) {
                continue;
            }

            auto& chunk = _chunks[header.ChunkIndex];

            if (header.ChunkSize != chunk.Size || header.ChunkHash != chunk.Hash || payload.size() != chunk.Size) {
                continue;
            }

            const uint64_t chunk_hash = fs::hash_data(payload);

            if (chunk_hash != chunk.Hash) {
                continue;
            }

            if (!chunk.Complete && !fs::write_file(chunk.Path, payload)) {
                Fail("Fast updater chunk write failed");
                return;
            }

            if (!chunk.Complete) {
                chunk.Complete = true;
                _verifiedBytes += chunk.Size;
            }

            chunk.Requested = false;

            if (socket_state.Busy && socket_state.ChunkIndex == header.ChunkIndex) {
                socket_state.Busy = false;
            }
        }
    }
}

void UpdaterFastClient::ProcessSends()
{
    FO_TRACE_ZONE(Core);

    for (int32_t socket_index = 0; socket_index < numeric_cast<int32_t>(_sockets.size()); socket_index++) {
        auto& socket_state = _sockets[numeric_cast<size_t>(socket_index)];

        if (socket_state.Busy) {
            continue;
        }

        const int32_t next_chunk_index = FindNextChunkToRequest();

        if (next_chunk_index < 0) {
            return;
        }

        auto& chunk = _chunks[numeric_cast<size_t>(next_chunk_index)];
        const int32_t endpoint_index = GetChunkEndpointIndex(chunk);

        FO_VERIFY_AND_THROW(endpoint_index >= 0, "Chunk endpoint index must be non-negative");
        FO_VERIFY_AND_THROW(endpoint_index < numeric_cast<int32_t>(_endpoints.size()), "Chunk endpoint index is out of the endpoints range");

        const auto request_data = MakeContentUpdateChunkRequestData(ContentUpdateChunkRequest {
            .SessionId = _sessionId,
            .FileIndex = _fileIndex,
            .ChunkIndex = numeric_cast<uint32_t>(next_chunk_index),
            .ClientNonce = socket_state.ClientNonce,
            .CookieExpiresAt = socket_state.CookieExpiresAt,
            .Cookie = socket_state.Cookie,
        });

        const auto& endpoint = _endpoints[numeric_cast<size_t>(endpoint_index)];
        const int32_t sent = socket_state.Socket.send_to(endpoint.Host, endpoint.Port, request_data);

        if (sent != numeric_cast<int32_t>(request_data.size())) {
            chunk.AttemptCount++;

            if (chunk.AttemptCount >= GetMaxAttempts()) {
                Fail("Fast updater send failed");
                return;
            }

            continue;
        }

        chunk.Requested = true;
        chunk.RequestTime = nanotime::now();
        socket_state.Busy = true;
        socket_state.ChunkIndex = numeric_cast<uint32_t>(next_chunk_index);
    }
}

void UpdaterFastClient::TryFinalize()
{
    FO_TRACE_ZONE(Core);

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

auto UpdaterFastClient::ReadChunkFile(uint32_t chunk_index, vector<uint8_t>& data) const -> bool
{
    FO_TRACE_ZONE(Core);

    FO_VERIFY_AND_THROW(chunk_index < _chunks.size(), "Chunk index is out of the chunk table range");
    return ReadWholeDiskFile(GetChunkPath(chunk_index), data);
}

auto UpdaterFastClient::ReadVerifiedChunkFile(uint32_t chunk_index, vector<uint8_t>& data) const -> bool
{
    FO_TRACE_ZONE(Core);

    FO_VERIFY_AND_THROW(chunk_index < _chunks.size(), "Chunk index is out of the chunk table range");

    const auto& chunk = _chunks[chunk_index];
    const auto file_size = fs::file_size(chunk.Path);

    if (!file_size.has_value() || *file_size != numeric_cast<uint64_t>(chunk.Size)) {
        return false;
    }

    if (!ReadChunkFile(chunk_index, data)) {
        return false;
    }

    return fs::hash_data(data) == chunk.Hash;
}

auto UpdaterFastClient::VerifyExistingChunk(uint32_t chunk_index) const -> bool
{
    FO_TRACE_ZONE(Core);

    vector<uint8_t> data;
    return ReadVerifiedChunkFile(chunk_index, data);
}

auto UpdaterFastClient::GetChunkPath(uint32_t chunk_index) const -> string
{
    FO_TRACE_ZONE(Core);

    return strex("{}.__fastupd.{}", _chunkBasePath, chunk_index);
}

auto UpdaterFastClient::FindNextChunkToRequest() const noexcept -> int32_t
{
    FO_TRACE_ZONE(Core);

    for (int32_t chunk_index = 0; chunk_index < numeric_cast<int32_t>(_chunks.size()); chunk_index++) {
        const auto& chunk = _chunks[numeric_cast<size_t>(chunk_index)];

        if (!chunk.Complete && !chunk.Requested) {
            return chunk_index;
        }
    }

    return -1;
}

auto UpdaterFastClient::GetChunkEndpointIndex(const ChunkState& chunk) const -> int32_t
{
    FO_TRACE_ZONE(Core);

    FO_VERIFY_AND_THROW(!_endpoints.empty(), "Fast updater endpoints must not be empty");
    return chunk.AttemptCount % numeric_cast<int32_t>(_endpoints.size());
}

auto UpdaterFastClient::GetMaxAttempts() const -> int32_t
{
    FO_TRACE_ZONE(Core);

    FO_VERIFY_AND_THROW(!_endpoints.empty(), "Fast updater endpoints must not be empty");

    const int64_t attempts_per_endpoint = numeric_cast<int64_t>(std::max(_settings->Network.FastUpdateMaxRetries, 0)) + 1;
    const int64_t endpoints_count = numeric_cast<int64_t>(_endpoints.size());
    const int32_t max_attempts = std::numeric_limits<int32_t>::max();

    if (attempts_per_endpoint > numeric_cast<int64_t>(max_attempts) / endpoints_count) {
        return max_attempts;
    }

    return numeric_cast<int32_t>(attempts_per_endpoint * endpoints_count);
}

FO_END_NAMESPACE
