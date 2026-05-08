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

#include "SpriteStreaming.h"

FO_BEGIN_NAMESPACE

static constexpr uint32 SPRITE_CACHE_MAGIC = 0x53504348;
static constexpr uint32 SPRITE_CACHE_VERSION = 1;
static constexpr size_t SPRITE_CACHE_FILL_PORTION = 1024 * 1024;

class SpriteCacheDataSource final : public DataSource
{
public:
    explicit SpriteCacheDataSource(const SpriteStreaming* streaming) :
        _streaming {streaming}
    {
    }

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return "SpriteCache"; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override { return _streaming->IsFileExists(path); }
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool override { return _streaming->GetFileInfo(path, size, write_time); }
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override { return _streaming->OpenFile(path, size, write_time); }
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override { return _streaming->GetFileNames(dir, recursive, ext); }

private:
    raw_ptr<const SpriteStreaming> _streaming;
};

template<typename T>
static void AppendCacheValue(vector<uint8>& data, const T& value)
{
    FO_NO_STACK_TRACE_ENTRY();

    const size_t cur_size = data.size();
    data.resize(cur_size + sizeof(T));
    MemCopy(data.data() + cur_size, &value, sizeof(T));
}

template<typename T>
static auto ReadCacheValue(const uint8*& ptr, const uint8* end, bool& failed) -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    if (failed || numeric_cast<size_t>(end - ptr) < sizeof(T)) {
        failed = true;
        return {};
    }

    T value {};
    MemCopy(&value, ptr, sizeof(T));
    ptr += sizeof(T);
    return value;
}

SpriteStreaming::SpriteStreaming(ClientSettings& settings, ClientConnection& connection, HashResolver& hash_resolver, const FileSystem& resources) :
    _settings {&settings},
    _connection {&connection},
    _hashResolver {&hash_resolver},
    _enabled {settings.SpriteStreamEnabled}
{
    FO_STACK_TRACE_ENTRY();

    _manifestIndex.LoadFromResources(resources, settings.ClientResourceEntries, settings.CompatibilityVersion, settings.GameVersion);
    FO_RUNTIME_ASSERT(!_enabled || _manifestIndex.IsLoaded());
    _generationHash = _manifestIndex.GetGenerationHash();

    if (_enabled) {
        InitializeCache();
    }
}

auto SpriteStreaming::HasCompletedSprite(hstring path, AtlasType atlas_type) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _cacheEntries.find({path, atlas_type});
    return it != _cacheEntries.end() && it->second.Complete;
}

auto SpriteStreaming::MakeDataSource() -> unique_ptr<DataSource>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<SpriteCacheDataSource>(this);
}

auto SpriteStreaming::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetPathEntry(path) != nullptr;
}

auto SpriteStreaming::GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto* entry = GetPathEntry(path);

    if (entry == nullptr) {
        return false;
    }

    size = entry->TotalSize;
    write_time = fs_last_write_time(_indexFilePath);
    return true;
}

auto SpriteStreaming::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    FO_STACK_TRACE_ENTRY();

    const auto* entry = GetPathEntry(path);

    if (entry == nullptr) {
        return {};
    }

    size = entry->TotalSize;
    write_time = fs_last_write_time(_indexFilePath);

    auto file = fs_open_ifstream(_dataFilePath);

    if (!file) {
        return {};
    }

    FO_RUNTIME_ASSERT(entry->DataOffset <= numeric_cast<uint64>(std::numeric_limits<int32>::max()));
    FO_RUNTIME_ASSERT(stream_set_read_pos(file, numeric_cast<int32>(entry->DataOffset), std::ios_base::beg));

    auto buf = SafeAlloc::MakeUniqueArr<uint8>(size);

    if (!stream_read_exact(file, buf.get(), size)) {
        return {};
    }

    return unique_del_ptr<const uint8> {buf.release(), [](const uint8* p) FO_DEFERRED { delete[] p; }};
}

auto SpriteStreaming::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> result;

    for (const auto& [path, key] : _cacheEntriesByPath) {
        ignore_unused(key);

        const auto path_ext = strex(path).get_file_extension();

        if (!ext.empty() && path_ext != ext) {
            continue;
        }

        if (!dir.empty()) {
            const bool starts_with_dir = strex(path).starts_with(dir);

            if (!starts_with_dir) {
                continue;
            }

            if (!recursive) {
                const string_view trimmed = string_view(path).substr(dir.size());

                if (trimmed.find('/') != string_view::npos || trimmed.find('\\') != string_view::npos) {
                    continue;
                }
            }
        }

        result.emplace_back(path);
    }

    return result;
}

void SpriteStreaming::Process()
{
    FO_STACK_TRACE_ENTRY();

    if (!_enabled || !_connection->IsConnected()) {
        return;
    }

    while (!_requestQueue.empty()) {
        const auto [path, atlas_type] = _requestQueue.front();
        _requestQueue.pop_front();
        _queuedRequests.erase({path, atlas_type});

        if (_activeRequests.count({path, atlas_type}) != 0 || HasCompletedSprite(path, atlas_type)) {
            continue;
        }

        _connection->OutBuf->StartMsg(NetMessage::SpriteStreamRequest);
        _connection->OutBuf->Write(path);
        _connection->OutBuf->Write(std::to_underlying(atlas_type));
        _connection->OutBuf->EndMsg();

        _activeRequests.emplace(path, atlas_type);
    }
}

void SpriteStreaming::RequestSprite(hstring path, AtlasType atlas_type)
{
    FO_STACK_TRACE_ENTRY();

    if (!_enabled || !path || HasCompletedSprite(path, atlas_type)) {
        return;
    }
    if (_manifestIndex.FindEntry(path.as_str()) == nullptr) {
        return;
    }
    if (_queuedRequests.count({path, atlas_type}) != 0 || _activeRequests.count({path, atlas_type}) != 0) {
        return;
    }

    _requestQueue.emplace_back(path, atlas_type);
    _queuedRequests.emplace(path, atlas_type);
}

void SpriteStreaming::HandleData(hstring path, AtlasType atlas_type, uint32 data_offset, uint32 total_size, uint32 content_hash, const_span<uint8> data)
{
    FO_STACK_TRACE_ENTRY();

    if (!_enabled || !path || data.empty()) {
        return;
    }

    const auto* manifest_entry = _manifestIndex.FindEntry(path.as_str());

    if (manifest_entry != nullptr && (manifest_entry->FileSize != total_size || manifest_entry->ContentHash != content_hash)) {
        WriteLog("Sprite stream manifest mismatch, path '{}'", path);

        if (auto* entry = GetCacheEntry(path, atlas_type); entry != nullptr) {
            ResetCacheEntry(*entry);
        }

        _activeRequests.erase({path, atlas_type});
        return;
    }

    auto* entry = GetCacheEntry(path, atlas_type);

    if (entry == nullptr) {
        entry = AllocateCacheEntry(path, atlas_type, total_size);

        if (entry == nullptr) {
            WriteLog("Sprite cache is full, path '{}'", path);
            _activeRequests.erase({path, atlas_type});
            return;
        }
    }

    FO_RUNTIME_ASSERT(entry->TotalSize == total_size);

    if (entry->ContentHash != 0 && entry->ContentHash != content_hash) {
        WriteLog("Sprite stream data hash mismatch, path '{}', expected {}, got {}", path, entry->ContentHash, content_hash);
        ResetCacheEntry(*entry);
        _activeRequests.erase({path, atlas_type});
        return;
    }

    entry->ContentHash = content_hash;

    if (data_offset != entry->ReceivedSize) {
        WriteLog("Unexpected sprite stream data offset, path '{}', expected {}, got {}", path, entry->ReceivedSize, data_offset);
        ResetCacheEntry(*entry);
        _activeRequests.erase({path, atlas_type});
        return;
    }

    WriteChunk(*entry, data_offset, data);
    entry->ReceivedSize += numeric_cast<uint32>(data.size());
}

void SpriteStreaming::HandleComplete(hstring path, AtlasType atlas_type, uint32 total_size, uint32 content_hash)
{
    FO_STACK_TRACE_ENTRY();

    if (!_enabled || !path) {
        return;
    }

    const auto* manifest_entry = _manifestIndex.FindEntry(path.as_str());

    if (manifest_entry != nullptr && (manifest_entry->FileSize != total_size || manifest_entry->ContentHash != content_hash)) {
        WriteLog("Sprite stream manifest complete mismatch, path '{}'", path);

        if (auto* entry = GetCacheEntry(path, atlas_type); entry != nullptr) {
            ResetCacheEntry(*entry);
        }

        _activeRequests.erase({path, atlas_type});
        return;
    }

    auto* entry = GetCacheEntry(path, atlas_type);

    if (entry == nullptr) {
        WriteLog("Sprite stream complete without cache entry, path '{}'", path);
        _activeRequests.erase({path, atlas_type});
        return;
    }

    if (entry->TotalSize != total_size || entry->ReceivedSize != total_size || entry->ContentHash != content_hash) {
        WriteLog("Sprite stream complete with invalid size, path '{}', expected {}, received {}", path, entry->TotalSize, entry->ReceivedSize);
        ResetCacheEntry(*entry);
        _activeRequests.erase({path, atlas_type});
        return;
    }

    entry->Complete = true;
    _activeRequests.erase({path, atlas_type});
    SaveIndex();
}

void SpriteStreaming::HandleRejected(hstring path, AtlasType atlas_type, SpriteStreamRejectReason reason)
{
    FO_STACK_TRACE_ENTRY();

    if (!_enabled || !path) {
        return;
    }

    _activeRequests.erase({path, atlas_type});
    WriteLog("Sprite stream rejected, path '{}', reason {}", path, std::to_underlying(reason));
}

void SpriteStreaming::OnDisconnected()
{
    FO_STACK_TRACE_ENTRY();

    _requestQueue.clear();
    _queuedRequests.clear();
    ResetActiveEntries();
    _activeRequests.clear();
}

void SpriteStreaming::InitializeCache()
{
    FO_STACK_TRACE_ENTRY();

    EnsureCacheDirectory();
    EnsureDataFile();
    LoadIndex();
}

void SpriteStreaming::EnsureCacheDirectory()
{
    FO_STACK_TRACE_ENTRY();

    _cacheDir = fs_resolve_path(strex(_settings->CacheResources).combine_path(_settings->SpriteCacheDir));
    _dataFilePath = strex(_cacheDir).combine_path(_settings->SpriteCacheDataFile);
    _indexFilePath = strex(_cacheDir).combine_path(_settings->SpriteCacheIndexFile);

    if (!fs_is_dir(_cacheDir)) {
        fs_create_directories(_cacheDir);
        FO_RUNTIME_ASSERT(fs_is_dir(_cacheDir));
    }
}

void SpriteStreaming::EnsureDataFile()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_settings->SpriteCacheMaxSize > 0);

    bool recreate_data_file = !fs_exists(_dataFilePath);

    if (!recreate_data_file) {
        const auto file_size = fs_file_size(_dataFilePath);
        recreate_data_file = !file_size.has_value() || *file_size != numeric_cast<uint64>(_settings->SpriteCacheMaxSize);
    }

    if (!recreate_data_file) {
        return;
    }

    std::ofstream file {std::filesystem::path {fs_make_path(_dataFilePath)}, std::ios::binary | std::ios::trunc};
    FO_RUNTIME_ASSERT(file);

    vector<uint8> zeros;
    zeros.resize(SPRITE_CACHE_FILL_PORTION);

    int64 remaining = _settings->SpriteCacheMaxSize;

    while (remaining > 0) {
        const size_t portion = remaining >= numeric_cast<int64>(zeros.size()) ? zeros.size() : numeric_cast<size_t>(remaining);
        FO_RUNTIME_ASSERT(stream_write_exact(file, zeros.data(), portion));
        remaining -= numeric_cast<int64>(portion);
    }

    file.flush();
    FO_RUNTIME_ASSERT(file);

    ResetCacheState();
    SaveIndex();
}

void SpriteStreaming::LoadIndex()
{
    FO_STACK_TRACE_ENTRY();

    ResetCacheState();

    const auto content = fs_read_file(_indexFilePath);

    if (!content.has_value()) {
        SaveIndex();
        return;
    }

    const uint8* ptr = reinterpret_cast<const uint8*>(content->data());
    const uint8* end = ptr + content->size();
    bool failed = false;

    const uint32 magic = ReadCacheValue<uint32>(ptr, end, failed);
    const uint32 version = ReadCacheValue<uint32>(ptr, end, failed);
    const uint32 generation_hash = ReadCacheValue<uint32>(ptr, end, failed);
    const uint64 next_offset = ReadCacheValue<uint64>(ptr, end, failed);
    const uint32 entries_count = ReadCacheValue<uint32>(ptr, end, failed);

    if (failed || magic != SPRITE_CACHE_MAGIC || version != SPRITE_CACHE_VERSION || generation_hash != _generationHash) {
        WriteLog("Invalid sprite cache index, recreating '{}'", _indexFilePath);
        SaveIndex();
        return;
    }

    _nextDataOffset = next_offset;

    for (uint32 i = 0; i < entries_count && !failed; i++) {
        const uint32 path_len = ReadCacheValue<uint32>(ptr, end, failed);

        if (failed || numeric_cast<size_t>(end - ptr) < path_len) {
            failed = true;
            break;
        }

        string path;
        path.resize(path_len);
        MemCopy(path.data(), ptr, path_len);
        ptr += path_len;

        const uint8 atlas_value = ReadCacheValue<uint8>(ptr, end, failed);
        CacheEntry entry;
        entry.Path = path;
        entry.AtlasTypeValue = static_cast<AtlasType>(atlas_value);
        entry.DataOffset = ReadCacheValue<uint64>(ptr, end, failed);
        entry.TotalSize = ReadCacheValue<uint32>(ptr, end, failed);
        entry.ReceivedSize = ReadCacheValue<uint32>(ptr, end, failed);
        entry.ContentHash = ReadCacheValue<uint32>(ptr, end, failed);
        entry.Complete = ReadCacheValue<uint8>(ptr, end, failed) != 0;

        if (failed) {
            break;
        }

        const hstring hpath = _hashResolver->ToHashedString(path);
        _cacheEntries.emplace(pair {hpath, entry.AtlasTypeValue}, std::move(entry));
        _cacheEntriesByPath.emplace(path, pair {hpath, static_cast<AtlasType>(atlas_value)});
    }

    if (failed) {
        WriteLog("Sprite cache index truncated, recreating '{}'", _indexFilePath);
        ResetCacheState();
        SaveIndex();
    }
}

void SpriteStreaming::SaveIndex() const
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8> data;
    data.reserve(128 + _cacheEntries.size() * 64);

    AppendCacheValue(data, SPRITE_CACHE_MAGIC);
    AppendCacheValue(data, SPRITE_CACHE_VERSION);
    AppendCacheValue(data, _generationHash);
    AppendCacheValue(data, _nextDataOffset);
    AppendCacheValue(data, numeric_cast<uint32>(_cacheEntries.size()));

    for (const auto& [key, entry] : _cacheEntries) {
        ignore_unused(key);

        AppendCacheValue(data, numeric_cast<uint32>(entry.Path.size()));
        data.insert(data.end(), entry.Path.begin(), entry.Path.end());
        AppendCacheValue(data, std::to_underlying(entry.AtlasTypeValue));
        AppendCacheValue(data, entry.DataOffset);
        AppendCacheValue(data, entry.TotalSize);
        AppendCacheValue(data, entry.ReceivedSize);
        AppendCacheValue(data, entry.ContentHash);
        AppendCacheValue<uint8>(data, entry.Complete ? 1 : 0);
    }

    FO_RUNTIME_ASSERT(fs_write_file(_indexFilePath, data));
}

void SpriteStreaming::ResetCacheState()
{
    FO_STACK_TRACE_ENTRY();

    _nextDataOffset = 0;
    _cacheEntries.clear();
    _cacheEntriesByPath.clear();
}

auto SpriteStreaming::AllocateCacheEntry(hstring path, AtlasType atlas_type, uint32 total_size) -> CacheEntry*
{
    FO_STACK_TRACE_ENTRY();

    if (_nextDataOffset + total_size > numeric_cast<uint64>(_settings->SpriteCacheMaxSize)) {
        return nullptr;
    }

    CacheEntry entry;
    entry.Path = path.as_str();
    entry.AtlasTypeValue = atlas_type;
    entry.DataOffset = _nextDataOffset;
    entry.TotalSize = total_size;
    entry.ReceivedSize = 0;
    entry.ContentHash = 0;
    entry.Complete = false;

    _nextDataOffset += total_size;

    auto [it, inserted] = _cacheEntries.emplace(pair {path, atlas_type}, std::move(entry));
    FO_RUNTIME_ASSERT(inserted);
    FO_RUNTIME_ASSERT(_cacheEntriesByPath.count(it->second.Path) == 0);
    _cacheEntriesByPath.emplace(it->second.Path, pair {path, atlas_type});
    SaveIndex();
    return &it->second;
}

auto SpriteStreaming::GetCacheEntry(hstring path, AtlasType atlas_type) -> CacheEntry*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _cacheEntries.find({path, atlas_type});
    return it != _cacheEntries.end() ? &it->second : nullptr;
}

auto SpriteStreaming::GetCacheEntry(hstring path, AtlasType atlas_type) const -> const CacheEntry*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _cacheEntries.find({path, atlas_type});
    return it != _cacheEntries.end() ? &it->second : nullptr;
}

auto SpriteStreaming::GetPathEntry(string_view path) -> CacheEntry*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _cacheEntriesByPath.find(string {path});

    if (it == _cacheEntriesByPath.end()) {
        return nullptr;
    }

    const auto entry_it = _cacheEntries.find(it->second);
    return entry_it != _cacheEntries.end() && entry_it->second.Complete ? &entry_it->second : nullptr;
}

auto SpriteStreaming::GetPathEntry(string_view path) const -> const CacheEntry*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _cacheEntriesByPath.find(string {path});

    if (it == _cacheEntriesByPath.end()) {
        return nullptr;
    }

    const auto entry_it = _cacheEntries.find(it->second);
    return entry_it != _cacheEntries.end() && entry_it->second.Complete ? &entry_it->second : nullptr;
}

void SpriteStreaming::ResetCacheEntry(CacheEntry& entry)
{
    FO_STACK_TRACE_ENTRY();

    entry.ReceivedSize = 0;
    entry.ContentHash = 0;
    entry.Complete = false;
    SaveIndex();
}

void SpriteStreaming::ResetActiveEntries()
{
    FO_STACK_TRACE_ENTRY();

    bool index_changed = false;

    for (const auto& key : _activeRequests) {
        auto it = _cacheEntries.find(key);

        if (it == _cacheEntries.end()) {
            continue;
        }

        auto& entry = it->second;

        if (entry.ReceivedSize == 0 && entry.ContentHash == 0 && !entry.Complete) {
            continue;
        }

        entry.ReceivedSize = 0;
        entry.ContentHash = 0;
        entry.Complete = false;
        index_changed = true;
    }

    if (index_changed) {
        SaveIndex();
    }
}

void SpriteStreaming::WriteChunk(const CacheEntry& entry, uint32 data_offset, const_span<uint8> data) const
{
    FO_STACK_TRACE_ENTRY();

    const uint64 file_offset64 = entry.DataOffset + data_offset;
    FO_RUNTIME_ASSERT(file_offset64 <= numeric_cast<uint64>(std::numeric_limits<int32>::max()));

    std::fstream file {std::filesystem::path {fs_make_path(_dataFilePath)}, std::ios::in | std::ios::out | std::ios::binary};
    FO_RUNTIME_ASSERT(file);
    FO_RUNTIME_ASSERT(stream_set_write_pos(file, numeric_cast<int32>(file_offset64), std::ios_base::beg));
    FO_RUNTIME_ASSERT(stream_write_exact(file, data.data(), data.size()));
    file.flush();
    FO_RUNTIME_ASSERT(file);
}

FO_END_NAMESPACE
