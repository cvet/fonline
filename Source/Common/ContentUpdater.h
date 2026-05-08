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

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ContentUpdaterException);

constexpr uint32 ContentUpdateManifestSignature = 0x44505546; // FUPD
constexpr uint16 ContentUpdateManifestVersion = 1;
constexpr uint32 ContentUpdateDatagramSignature = 0x44505555; // UUPD
constexpr uint16 ContentUpdateDatagramVersion = 1;

enum class ContentUpdateDatagramType : uint8
{
    ChunkRequest = 1,
    ChunkData = 2,
};

struct ContentUpdateEndpoint
{
    string Host;
    uint16 Port {};
    int32 Priority {};
};

struct ContentUpdateFileInfo
{
    string Name;
    uint32 Size {};
    uint32 Hash {};
    vector<uint32> ChunkHashes {};
};

struct ContentUpdateManifest
{
    bool FastUpdateEnabled {};
    bool SelfHostedServerEnabled {};
    uint32 SessionId {};
    uint32 ChunkSize {};
    vector<ContentUpdateEndpoint> Endpoints {};
    vector<ContentUpdateFileInfo> Files {};
};

struct ContentUpdateChunkRequest
{
    uint32 SessionId {};
    uint32 FileIndex {};
    uint32 ChunkIndex {};
};

struct ContentUpdateChunkDataHeader
{
    uint32 SessionId {};
    uint32 FileIndex {};
    uint32 ChunkIndex {};
    uint32 ChunkSize {};
    uint32 ChunkHash {};
};

[[nodiscard]] auto ParseContentUpdateEndpoint(string_view value) -> optional<ContentUpdateEndpoint>;
void SerializeContentUpdateManifest(const ContentUpdateManifest& manifest, vector<uint8>& data);
[[nodiscard]] auto DeserializeContentUpdateManifest(const_span<uint8> data) -> ContentUpdateManifest;
[[nodiscard]] auto MakeContentUpdateChunkRequestData(const ContentUpdateChunkRequest& request) -> vector<uint8>;
[[nodiscard]] auto TryReadContentUpdateChunkRequestData(const_span<uint8> data, ContentUpdateChunkRequest& request) noexcept -> bool;
void MakeContentUpdateChunkData(const ContentUpdateChunkDataHeader& header, const_span<uint8> payload, vector<uint8>& data);
[[nodiscard]] auto TryReadContentUpdateChunkData(const_span<uint8> data, ContentUpdateChunkDataHeader& header, const uint8*& payload_data, size_t& payload_size) noexcept -> bool;
[[nodiscard]] auto GetContentUpdateChunkCount(uint32 file_size, uint32 chunk_size) noexcept -> uint32;
[[nodiscard]] auto GetContentUpdateChunkSize(uint32 file_size, uint32 chunk_size, uint32 chunk_index) noexcept -> uint32;
[[nodiscard]] auto GetContentUpdateChunkOffset(uint32 chunk_size, uint32 chunk_index) noexcept -> uint32;

FO_END_NAMESPACE