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

#include "SpriteStreamingManifest.h"

FO_BEGIN_NAMESPACE

static constexpr uint32 SPRITE_STREAMING_MANIFEST_MAGIC = 0x53534D46;
static constexpr uint32 SPRITE_STREAMING_MANIFEST_VERSION = 1;

template<typename T>
static void AppendSpriteManifestValue(vector<uint8>& data, const T& value)
{
    FO_NO_STACK_TRACE_ENTRY();

    const size_t cur_size = data.size();
    data.resize(cur_size + sizeof(T));
    MemCopy(data.data() + cur_size, &value, sizeof(T));
}

template<typename T>
static auto ReadSpriteManifestValue(const uint8*& ptr, const uint8* end, bool& failed) -> T
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

static constexpr std::array<string_view, 12> SPRITE_STREAMING_EXTENSIONS = {"fofrm", "frm", "fr0", "rix", "art", "spr", "zar", "til", "mos", "bam", "png", "tga"};

static auto GetSpriteStreamingExtensions() -> const_span<string_view>
{
    FO_NO_STACK_TRACE_ENTRY();

    return SPRITE_STREAMING_EXTENSIONS;
}

auto SpriteStreamingManifest::GetPackFileName(string_view pack_name) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(pack_name).str().append(FILE_SUFFIX);
}

auto SpriteStreamingManifest::IsSpritePath(string_view path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const string ext = strex(path).get_file_extension();
    const auto exts = GetSpriteStreamingExtensions();
    return std::ranges::find(exts, string_view {ext}) != exts.end();
}

auto SpriteStreamingManifest::BuildData(const vector<Entry>& entries) -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    vector<Entry> sorted_entries = entries;

    std::ranges::sort(sorted_entries, [](const Entry& left, const Entry& right) {
        return left.Path < right.Path;
    });

    vector<uint8> data;
    data.reserve(64 + sorted_entries.size() * 48);

    AppendSpriteManifestValue(data, SPRITE_STREAMING_MANIFEST_MAGIC);
    AppendSpriteManifestValue(data, SPRITE_STREAMING_MANIFEST_VERSION);
    AppendSpriteManifestValue(data, numeric_cast<uint32>(sorted_entries.size()));

    for (const auto& entry : sorted_entries) {
        AppendSpriteManifestValue(data, numeric_cast<uint32>(entry.Path.size()));
        data.insert(data.end(), entry.Path.begin(), entry.Path.end());
        AppendSpriteManifestValue(data, entry.FileSize);
        AppendSpriteManifestValue(data, entry.ContentHash);
    }

    return data;
}

auto SpriteStreamingManifest::ParseData(const_span<uint8> data) -> vector<Entry>
{
    FO_STACK_TRACE_ENTRY();

    const uint8* ptr = data.data();
    const uint8* end = data.data() + data.size();
    bool failed = false;

    const uint32 magic = ReadSpriteManifestValue<uint32>(ptr, end, failed);
    const uint32 version = ReadSpriteManifestValue<uint32>(ptr, end, failed);
    const uint32 entries_count = ReadSpriteManifestValue<uint32>(ptr, end, failed);

    if (failed || magic != SPRITE_STREAMING_MANIFEST_MAGIC || version != SPRITE_STREAMING_MANIFEST_VERSION) {
        throw SpriteStreamingManifestException("Invalid sprite streaming manifest");
    }

    vector<Entry> entries;
    entries.reserve(entries_count);

    for (uint32 i = 0; i < entries_count; i++) {
        const uint32 path_len = ReadSpriteManifestValue<uint32>(ptr, end, failed);

        if (failed || numeric_cast<size_t>(end - ptr) < path_len) {
            failed = true;
            break;
        }

        Entry entry;
        entry.Path.resize(path_len);
        MemCopy(entry.Path.data(), ptr, path_len);
        ptr += path_len;
        entry.FileSize = ReadSpriteManifestValue<uint32>(ptr, end, failed);
        entry.ContentHash = ReadSpriteManifestValue<uint32>(ptr, end, failed);

        if (failed) {
            break;
        }

        entries.emplace_back(std::move(entry));
    }

    if (failed || ptr != end) {
        throw SpriteStreamingManifestException("Corrupted sprite streaming manifest");
    }

    return entries;
}

void SpriteStreamingManifestIndex::LoadFromResources(const FileSystem& resources, const vector<string>& pack_entries, string_view compatibility_version, string_view game_version)
{
    FO_STACK_TRACE_ENTRY();

    _loaded = false;
    _generationHash = 0;
    _entries.clear();

    vector<uint8> generation_data;
    generation_data.insert(generation_data.end(), compatibility_version.begin(), compatibility_version.end());
    generation_data.emplace_back('|');
    generation_data.insert(generation_data.end(), game_version.begin(), game_version.end());
    generation_data.emplace_back('|');

    for (const auto& pack_name : pack_entries) {
        const string manifest_path = SpriteStreamingManifest::GetPackFileName(pack_name);

        if (!resources.IsFileExists(manifest_path)) {
            continue;
        }

        const auto manifest_file = resources.ReadFile(manifest_path);

        if (!manifest_file) {
            continue;
        }

        generation_data.insert(generation_data.end(), manifest_file.GetBuf(), manifest_file.GetBuf() + manifest_file.GetSize());

        const auto entries = SpriteStreamingManifest::ParseData({manifest_file.GetBuf(), manifest_file.GetSize()});

        for (auto&& entry : entries) {
            if (_entries.count(entry.Path) != 0) {
                continue;
            }

            _entries.emplace(entry.Path, std::move(entry));
        }
    }

    if (generation_data.size() > compatibility_version.size() + game_version.size() + 2) {
        _loaded = true;
        _generationHash = Hashing::MurmurHash2(generation_data.data(), generation_data.size());
    }
}

auto SpriteStreamingManifestIndex::FindEntry(string_view path) const -> const SpriteStreamingManifest::Entry*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _entries.find(string {path});
    return it != _entries.end() ? &it->second : nullptr;
}

FO_END_NAMESPACE