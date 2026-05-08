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

#include "ContentUpdater.h"

FO_BEGIN_NAMESPACE

static void WriteDatagramHeader(DataWriter& writer, ContentUpdateDatagramType type)
{
    FO_STACK_TRACE_ENTRY();

    writer.Write<uint32>(ContentUpdateDatagramSignature);
    writer.Write<uint16>(ContentUpdateDatagramVersion);
    writer.Write<uint8>(static_cast<uint8>(type));
}

static auto ReadAndCheckDatagramHeader(DataReader& reader, ContentUpdateDatagramType expected_type) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const uint32 signature = reader.Read<uint32>();
    const uint16 version = reader.Read<uint16>();
    const auto type = static_cast<ContentUpdateDatagramType>(reader.Read<uint8>());
    return signature == ContentUpdateDatagramSignature && version == ContentUpdateDatagramVersion && type == expected_type;
}

auto ParseContentUpdateEndpoint(string_view value) -> optional<ContentUpdateEndpoint>
{
    FO_STACK_TRACE_ENTRY();

    const auto parts = strex(value).split(':');

    if (parts.size() < 2 || parts.size() > 3) {
        return std::nullopt;
    }

    ContentUpdateEndpoint endpoint;
    endpoint.Host = parts[0];

    if (endpoint.Host.empty()) {
        return std::nullopt;
    }

    const int32 port_value = strex(parts[1]).to_int32();

    if (port_value <= 0 || port_value > std::numeric_limits<uint16>::max()) {
        return std::nullopt;
    }

    endpoint.Port = numeric_cast<uint16>(port_value);
    endpoint.Priority = parts.size() == 3 ? strex(parts[2]).to_int32() : 0;
    return endpoint;
}

void SerializeContentUpdateManifest(const ContentUpdateManifest& manifest, vector<uint8>& data)
{
    FO_STACK_TRACE_ENTRY();

    data.clear();

    DataWriter writer {data};
    writer.Write<uint32>(ContentUpdateManifestSignature);
    writer.Write<uint16>(ContentUpdateManifestVersion);
    writer.Write<uint8>(manifest.FastUpdateEnabled ? uint8 {1} : uint8 {0});
    writer.Write<uint8>(manifest.SelfHostedServerEnabled ? uint8 {1} : uint8 {0});
    writer.Write<uint32>(manifest.SessionId);
    writer.Write<uint32>(manifest.ChunkSize);
    writer.Write<uint32>(numeric_cast<uint32>(manifest.Endpoints.size()));

    for (const auto& endpoint : manifest.Endpoints) {
        writer.Write<uint16>(numeric_cast<uint16>(endpoint.Host.length()));
        writer.WritePtr(endpoint.Host.data(), endpoint.Host.length());
        writer.Write<uint16>(endpoint.Port);
        writer.Write<int32>(endpoint.Priority);
    }

    writer.Write<uint32>(numeric_cast<uint32>(manifest.Files.size()));

    for (const auto& file : manifest.Files) {
        writer.Write<uint16>(numeric_cast<uint16>(file.Name.length()));
        writer.WritePtr(file.Name.data(), file.Name.length());
        writer.Write<uint32>(file.Size);
        writer.Write<uint32>(file.Hash);
        writer.Write<uint32>(numeric_cast<uint32>(file.ChunkHashes.size()));

        for (const uint32 chunk_hash : file.ChunkHashes) {
            writer.Write<uint32>(chunk_hash);
        }
    }
}

auto DeserializeContentUpdateManifest(const_span<uint8> data) -> ContentUpdateManifest
{
    FO_STACK_TRACE_ENTRY();

    DataReader reader {data};

    const uint32 signature = reader.Read<uint32>();
    const uint16 version = reader.Read<uint16>();

    if (signature != ContentUpdateManifestSignature || version != ContentUpdateManifestVersion) {
        throw ContentUpdaterException("Invalid content update manifest header");
    }

    ContentUpdateManifest manifest;
    manifest.FastUpdateEnabled = reader.Read<uint8>() != 0;
    manifest.SelfHostedServerEnabled = reader.Read<uint8>() != 0;
    manifest.SessionId = reader.Read<uint32>();
    manifest.ChunkSize = reader.Read<uint32>();

    const uint32 endpoints_count = reader.Read<uint32>();
    manifest.Endpoints.reserve(endpoints_count);

    for (const auto i : iterate_range(endpoints_count)) {
        ignore_unused(i);

        const uint16 host_len = reader.Read<uint16>();
        FO_RUNTIME_ASSERT(host_len > 0);

        ContentUpdateEndpoint endpoint;
        endpoint.Host = string(reader.ReadPtr<char>(host_len), host_len);
        endpoint.Port = reader.Read<uint16>();
        endpoint.Priority = reader.Read<int32>();
        manifest.Endpoints.emplace_back(std::move(endpoint));
    }

    const uint32 files_count = reader.Read<uint32>();
    manifest.Files.reserve(files_count);

    for (const auto i : iterate_range(files_count)) {
        ignore_unused(i);

        const uint16 name_len = reader.Read<uint16>();
        FO_RUNTIME_ASSERT(name_len > 0);

        ContentUpdateFileInfo file;
        file.Name = string(reader.ReadPtr<char>(name_len), name_len);
        file.Size = reader.Read<uint32>();
        file.Hash = reader.Read<uint32>();

        const uint32 chunks_count = reader.Read<uint32>();
        file.ChunkHashes.reserve(chunks_count);

        for (const auto chunk_index : iterate_range(chunks_count)) {
            ignore_unused(chunk_index);
            file.ChunkHashes.emplace_back(reader.Read<uint32>());
        }

        manifest.Files.emplace_back(std::move(file));
    }

    reader.VerifyEnd();
    return manifest;
}

auto MakeContentUpdateChunkRequestData(const ContentUpdateChunkRequest& request) -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8> data;
    DataWriter writer {data};
    WriteDatagramHeader(writer, ContentUpdateDatagramType::ChunkRequest);
    writer.Write<uint32>(request.SessionId);
    writer.Write<uint32>(request.FileIndex);
    writer.Write<uint32>(request.ChunkIndex);
    return data;
}

auto TryReadContentUpdateChunkRequestData(const_span<uint8> data, ContentUpdateChunkRequest& request) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    try {
        DataReader reader {data};

        if (!ReadAndCheckDatagramHeader(reader, ContentUpdateDatagramType::ChunkRequest)) {
            return false;
        }

        request.SessionId = reader.Read<uint32>();
        request.FileIndex = reader.Read<uint32>();
        request.ChunkIndex = reader.Read<uint32>();
        reader.VerifyEnd();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

void MakeContentUpdateChunkData(const ContentUpdateChunkDataHeader& header, const_span<uint8> payload, vector<uint8>& data)
{
    FO_STACK_TRACE_ENTRY();

    data.clear();

    DataWriter writer {data};
    WriteDatagramHeader(writer, ContentUpdateDatagramType::ChunkData);
    writer.Write<uint32>(header.SessionId);
    writer.Write<uint32>(header.FileIndex);
    writer.Write<uint32>(header.ChunkIndex);
    writer.Write<uint32>(header.ChunkSize);
    writer.Write<uint32>(header.ChunkHash);
    writer.WritePtr(payload.data(), payload.size());
}

auto TryReadContentUpdateChunkData(const_span<uint8> data, ContentUpdateChunkDataHeader& header, const uint8*& payload_data, size_t& payload_size) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    payload_data = nullptr;
    payload_size = 0;

    try {
        DataReader reader {data};

        if (!ReadAndCheckDatagramHeader(reader, ContentUpdateDatagramType::ChunkData)) {
            return false;
        }

        header.SessionId = reader.Read<uint32>();
        header.FileIndex = reader.Read<uint32>();
        header.ChunkIndex = reader.Read<uint32>();
        header.ChunkSize = reader.Read<uint32>();
        header.ChunkHash = reader.Read<uint32>();
        payload_data = reader.ReadPtr<uint8>(header.ChunkSize);
        payload_size = header.ChunkSize;
        reader.VerifyEnd();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

auto GetContentUpdateChunkCount(uint32 file_size, uint32 chunk_size) noexcept -> uint32
{
    FO_STACK_TRACE_ENTRY();

    if (file_size == 0 || chunk_size == 0) {
        return 0;
    }

    return (file_size + chunk_size - 1) / chunk_size;
}

auto GetContentUpdateChunkSize(uint32 file_size, uint32 chunk_size, uint32 chunk_index) noexcept -> uint32
{
    FO_STACK_TRACE_ENTRY();

    if (chunk_size == 0) {
        return 0;
    }

    const uint32 offset = GetContentUpdateChunkOffset(chunk_size, chunk_index);

    if (offset >= file_size) {
        return 0;
    }

    return std::min(chunk_size, file_size - offset);
}

auto GetContentUpdateChunkOffset(uint32 chunk_size, uint32 chunk_index) noexcept -> uint32
{
    FO_STACK_TRACE_ENTRY();

    return chunk_index * chunk_size;
}

FO_END_NAMESPACE