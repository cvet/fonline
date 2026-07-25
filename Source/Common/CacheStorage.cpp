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

#include "CacheStorage.h"
#include "Compressor.h"

FO_BEGIN_NAMESPACE

class CacheStorageImpl
{
public:
    CacheStorageImpl() = default;
    CacheStorageImpl(const CacheStorageImpl&) = delete;
    CacheStorageImpl(CacheStorageImpl&&) noexcept = delete;
    auto operator=(const CacheStorageImpl&) = delete;
    auto operator=(CacheStorageImpl&&) noexcept = delete;
    virtual ~CacheStorageImpl() = default;

    [[nodiscard]] virtual auto HasEntry(string_view entry_name) const -> bool = 0;
    [[nodiscard]] virtual auto GetString(string_view entry_name) const -> string = 0;
    [[nodiscard]] virtual auto GetData(string_view entry_name) const -> vector<uint8_t> = 0;

    virtual void SetString(string_view entry_name, string_view str) = 0;
    virtual void SetData(string_view entry_name, const_span<uint8_t> data) = 0;
    virtual void RemoveEntry(string_view entry_name) = 0;
};

class FileCacheStorage final : public CacheStorageImpl
{
public:
    explicit FileCacheStorage(string_view real_path);
    FileCacheStorage(const FileCacheStorage&) = delete;
    FileCacheStorage(FileCacheStorage&&) noexcept = delete;
    auto operator=(const FileCacheStorage&) = delete;
    auto operator=(FileCacheStorage&&) noexcept = delete;
    ~FileCacheStorage() override = default;

    [[nodiscard]] auto HasEntry(string_view entry_name) const -> bool override;
    [[nodiscard]] auto GetString(string_view entry_name) const -> string override;
    [[nodiscard]] auto GetData(string_view entry_name) const -> vector<uint8_t> override;

    auto CreateCacheStorage() const -> bool;
    void SetString(string_view entry_name, string_view str) override;
    void SetData(string_view entry_name, const_span<uint8_t> data) override;
    void RemoveEntry(string_view entry_name) override;

private:
    [[nodiscard]] auto MakeCacheEntryPath(string_view work_path, string_view data_name) const -> string;

    string _workPath {};
};

CacheStorage::CacheStorage(string_view path) :
    _impl {SafeAlloc::MakeUnique<FileCacheStorage>(path)}
{
    FO_STACK_TRACE_ENTRY();
}

CacheStorage::CacheStorage(CacheStorage&&) noexcept = default;
CacheStorage::~CacheStorage() = default;

auto CacheStorage::HasEntry(string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _impl->HasEntry(entry_name);
}

auto CacheStorage::GetString(string_view entry_name) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return _impl->GetString(entry_name);
}

auto CacheStorage::GetData(string_view entry_name) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    return _impl->GetData(entry_name);
}

void CacheStorage::SetString(string_view entry_name, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetString(entry_name, str);
}

void CacheStorage::SetData(string_view entry_name, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetData(entry_name, data);
}

void CacheStorage::RemoveEntry(string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    _impl->RemoveEntry(entry_name);
}

// Cache entries are obfuscated, not encrypted. The goal is that a user poking around the cache
// directory sees neither what is cached (file names are hashed) nor its contents (compressed, then
// masked with a keystream derived from the entry name). Anyone determined can still recover it —
// the transform is deterministic and the key material ships in the binary. If the cache ever has to
// be tamper-proof rather than merely opaque, that needs an authentication tag, not a better scramble.
static constexpr uint64_t CACHE_OBFUSCATION_SEED = 0x9E3779B97F4A7C15ULL;
static constexpr array<uint8_t, 4> CACHE_ENTRY_MAGIC {'F', 'O', 'C', '1'};

static auto MakeCacheKeystreamState(string_view entry_name) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return hashing::hash<string_view> {}(entry_name) ^ CACHE_OBFUSCATION_SEED;
}

// SplitMix64 over the entry-derived state: cheap, no dependencies, and produces a different
// keystream per entry so identical payloads do not produce identical files.
static void ApplyCacheKeystream(span<uint8_t> data, uint64_t state) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t block = 0;

    for (size_t i = 0; i < data.size(); i++) {
        size_t byte_index = i % sizeof(uint64_t);

        if (byte_index == 0) {
            state += 0x9E3779B97F4A7C15ULL;
            uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            block = z ^ (z >> 31);
        }

        data[i] ^= numeric_cast<uint8_t>((block >> (byte_index * 8)) & 0xFFU);
    }
}

// On disk: magic, then the uncompressed size, then the compressed payload — the whole lot masked
// with the entry keystream. Compression alone already defeats `strings`; the mask stops the zlib
// header from advertising what the file is.
static auto EncodeCacheEntry(string_view entry_name, const_span<uint8_t> data) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    auto compressed = Compressor::Compress(data);

    vector<uint8_t> encoded;
    encoded.reserve(CACHE_ENTRY_MAGIC.size() + sizeof(uint64_t) + compressed.size());
    encoded.insert(encoded.end(), CACHE_ENTRY_MAGIC.begin(), CACHE_ENTRY_MAGIC.end());

    uint64_t original_size = numeric_cast<uint64_t>(data.size());

    for (size_t i = 0; i < sizeof(original_size); i++) {
        encoded.push_back(numeric_cast<uint8_t>((original_size >> (i * 8)) & 0xFFU));
    }

    encoded.insert(encoded.end(), compressed.begin(), compressed.end());

    ApplyCacheKeystream(span<uint8_t> {encoded.data(), encoded.size()}, MakeCacheKeystreamState(entry_name));
    return encoded;
}

static auto DecodeCacheEntry(string_view entry_name, vector<uint8_t> stored) -> optional<vector<uint8_t>>
{
    FO_STACK_TRACE_ENTRY();

    constexpr size_t header_size = CACHE_ENTRY_MAGIC.size() + sizeof(uint64_t);

    if (stored.size() < header_size) {
        return std::nullopt;
    }

    ApplyCacheKeystream(span<uint8_t> {stored.data(), stored.size()}, MakeCacheKeystreamState(entry_name));

    if (!std::equal(CACHE_ENTRY_MAGIC.begin(), CACHE_ENTRY_MAGIC.end(), stored.begin())) {
        // Foreign or corrupted file: treat as a cache miss rather than failing the caller.
        return std::nullopt;
    }

    uint64_t original_size = 0;

    for (size_t i = 0; i < sizeof(original_size); i++) {
        original_size |= numeric_cast<uint64_t>(stored[CACHE_ENTRY_MAGIC.size() + i]) << (i * 8);
    }

    auto payload = make_const_span(stored.data() + header_size, stored.size() - header_size);

    try {
        auto decompressed = Compressor::Decompress(payload, 4);

        if (decompressed.size() != original_size) {
            return std::nullopt;
        }

        return decompressed;
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

auto FileCacheStorage::MakeCacheEntryPath(string_view work_path, string_view data_name) const -> string
{
    FO_STACK_TRACE_ENTRY();

    // The file name reveals nothing about the entry it holds.
    uint64_t name_hash = hashing::hash<string_view> {}(data_name) ^ CACHE_OBFUSCATION_SEED;
    return strex(work_path).combine_path(strex("{:016x}.bin", name_hash).str());
}

FileCacheStorage::FileCacheStorage(string_view real_path)
{
    FO_STACK_TRACE_ENTRY();

    _workPath = fs_resolve_path(real_path);
}

auto FileCacheStorage::CreateCacheStorage() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!fs_is_dir(_workPath)) {
        fs_create_directories(_workPath);

        if (!fs_is_dir(_workPath)) {
            WriteLog(LogType::Warning, "Can't create dir for cache '{}'", _workPath);
            return false;
        }
    }

    return true;
}

auto FileCacheStorage::HasEntry(string_view entry_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    string path = MakeCacheEntryPath(_workPath, entry_name);
    return fs_exists(path);
}

auto FileCacheStorage::GetString(string_view entry_name) const -> string
{
    FO_STACK_TRACE_ENTRY();

    auto data = GetData(entry_name);

    if (data.empty()) {
        return {};
    }

    return string {span_to_string(make_const_span(data))};
}

auto FileCacheStorage::GetData(string_view entry_name) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    string path = MakeCacheEntryPath(_workPath, entry_name);
    auto stored = fs_read_file(path);

    if (!stored) {
        return {};
    }

    auto decoded = DecodeCacheEntry(entry_name, vector<uint8_t>(stored->begin(), stored->end()));

    if (!decoded.has_value()) {
        return {};
    }

    return std::move(*decoded);
}

void FileCacheStorage::SetString(string_view entry_name, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    if (!CreateCacheStorage()) {
        return;
    }

    SetData(entry_name, make_const_span(str));
}

void FileCacheStorage::SetData(string_view entry_name, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    if (!CreateCacheStorage()) {
        return;
    }

    auto encoded = EncodeCacheEntry(entry_name, data);
    string path = MakeCacheEntryPath(_workPath, entry_name);

    if (!fs_write_file(path, make_const_span(encoded))) {
        fs_remove_file(path);
        WriteLog(LogType::Warning, "Can't write cache at '{}'", path);
    }
}

void FileCacheStorage::RemoveEntry(string_view entry_name)
{
    FO_STACK_TRACE_ENTRY();

    string path = MakeCacheEntryPath(_workPath, entry_name);
    fs_remove_file(path);
}

FO_END_NAMESPACE
