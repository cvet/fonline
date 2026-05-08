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

#include "ClientConnection.h"
#include "DataSource.h"
#include "Settings.h"
#include "SpriteStreamingManifest.h"
#include "TextureAtlas.h"

FO_BEGIN_NAMESPACE

class SpriteStreaming final
{
public:
    SpriteStreaming() = delete;
    SpriteStreaming(ClientSettings& settings, ClientConnection& connection, HashResolver& hash_resolver, const FileSystem& resources);
    SpriteStreaming(const SpriteStreaming&) = delete;
    SpriteStreaming(SpriteStreaming&&) noexcept = delete;
    auto operator=(const SpriteStreaming&) = delete;
    auto operator=(SpriteStreaming&&) noexcept = delete;
    ~SpriteStreaming() = default;

    [[nodiscard]] auto IsEnabled() const noexcept -> bool { return _enabled; }
    [[nodiscard]] auto HasCompletedSprite(hstring path, AtlasType atlas_type) const -> bool;
    [[nodiscard]] auto MakeDataSource() -> unique_ptr<DataSource>;
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>;

    void Process();
    void RequestSprite(hstring path, AtlasType atlas_type);
    void HandleData(hstring path, AtlasType atlas_type, uint32 data_offset, uint32 total_size, uint32 content_hash, const_span<uint8> data);
    void HandleComplete(hstring path, AtlasType atlas_type, uint32 total_size, uint32 content_hash);
    void HandleRejected(hstring path, AtlasType atlas_type, SpriteStreamRejectReason reason);
    void OnDisconnected();

private:
    struct CacheEntry
    {
        string Path {};
        AtlasType AtlasTypeValue {};
        uint64 DataOffset {};
        uint32 TotalSize {};
        uint32 ReceivedSize {};
        uint32 ContentHash {};
        bool Complete {};
    };

    void InitializeCache();
    void EnsureCacheDirectory();
    void EnsureDataFile();
    void LoadIndex();
    void SaveIndex() const;
    void ResetCacheState();
    auto AllocateCacheEntry(hstring path, AtlasType atlas_type, uint32 total_size) -> CacheEntry*;
    auto GetCacheEntry(hstring path, AtlasType atlas_type) -> CacheEntry*;
    auto GetCacheEntry(hstring path, AtlasType atlas_type) const -> const CacheEntry*;
    auto GetPathEntry(string_view path) -> CacheEntry*;
    auto GetPathEntry(string_view path) const -> const CacheEntry*;
    void ResetCacheEntry(CacheEntry& entry);
    void ResetActiveEntries();
    void WriteChunk(const CacheEntry& entry, uint32 data_offset, const_span<uint8> data) const;

    raw_ptr<ClientSettings> _settings;
    raw_ptr<ClientConnection> _connection;
    raw_ptr<HashResolver> _hashResolver;
    bool _enabled {};
    string _cacheDir {};
    string _dataFilePath {};
    string _indexFilePath {};
    uint32 _generationHash {};
    SpriteStreamingManifestIndex _manifestIndex {};
    uint64 _nextDataOffset {};
    deque<pair<hstring, AtlasType>> _requestQueue {};
    unordered_set<pair<hstring, AtlasType>> _queuedRequests {};
    unordered_set<pair<hstring, AtlasType>> _activeRequests {};
    unordered_map<pair<hstring, AtlasType>, CacheEntry> _cacheEntries {};
    unordered_map<string, pair<hstring, AtlasType>> _cacheEntriesByPath {};
};

FO_END_NAMESPACE
