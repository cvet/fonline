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

#include "UpdaterFastServer.h"

FO_BEGIN_NAMESPACE

UpdaterFastServer::UpdaterFastServer(ServerNetworkSettings& settings, const ContentUpdateManifest& manifest, const vector<vector<uint8>>& files_data) :
    _settings {&settings},
    _manifest {&manifest},
    _filesData {&files_data}
{
    FO_STACK_TRACE_ENTRY();

    _receiveBuf.resize(numeric_cast<size_t>(_settings->FastUpdateChunkSize) + 64);
}

auto UpdaterFastServer::Start() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_settings->FastUpdateEnabled || !_settings->FastUpdateServerEnabled || _settings->FastUpdateBindPort <= 0) {
        return false;
    }

    _started = _socket.bind(_settings->FastUpdateBindHost, numeric_cast<uint16>(_settings->FastUpdateBindPort));
    return _started;
}

void UpdaterFastServer::Stop() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _socket.close();
    _started = false;
}

void UpdaterFastServer::Poll()
{
    FO_STACK_TRACE_ENTRY();

    if (!_started) {
        return;
    }

    for (const auto i : iterate_range(64)) {
        ignore_unused(i);

        if (!_socket.can_read({})) {
            break;
        }

        string remote_host;
        uint16 remote_port = 0;
        const int32 received = _socket.receive_from(_receiveBuf, remote_host, remote_port);

        if (received <= 0) {
            break;
        }

        ContentUpdateChunkRequest request;

        if (!TryReadContentUpdateChunkRequestData({_receiveBuf.data(), numeric_cast<size_t>(received)}, request)) {
            continue;
        }

        if (request.SessionId != _manifest->SessionId || request.FileIndex >= _filesData->size() || request.FileIndex >= _manifest->Files.size()) {
            continue;
        }

        const auto& file_info = _manifest->Files[request.FileIndex];

        if (request.ChunkIndex >= file_info.ChunkHashes.size()) {
            continue;
        }

        const uint32 chunk_offset = GetContentUpdateChunkOffset(_manifest->ChunkSize, request.ChunkIndex);
        const uint32 chunk_size = GetContentUpdateChunkSize(file_info.Size, _manifest->ChunkSize, request.ChunkIndex);
        const auto& file_data = (*_filesData)[request.FileIndex];

        if (chunk_size == 0 || chunk_offset + chunk_size > file_data.size()) {
            continue;
        }

        const auto payload = const_span<uint8> {file_data.data() + chunk_offset, chunk_size};

        MakeContentUpdateChunkData(ContentUpdateChunkDataHeader {
            .SessionId = request.SessionId,
            .FileIndex = request.FileIndex,
            .ChunkIndex = request.ChunkIndex,
            .ChunkSize = chunk_size,
            .ChunkHash = file_info.ChunkHashes[request.ChunkIndex],
        }, payload, _sendBuf);

        _socket.send_to(remote_host, remote_port, _sendBuf);
    }
}

FO_END_NAMESPACE