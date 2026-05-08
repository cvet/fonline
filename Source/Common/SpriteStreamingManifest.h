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

#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(SpriteStreamingManifestException);

class SpriteStreamingManifest final
{
public:
    struct Entry
    {
        string Path {};
        uint32 FileSize {};
        uint32 ContentHash {};
    };

    static constexpr string_view_nt FILE_SUFFIX = ".fosssm";

    [[nodiscard]] static auto GetPackFileName(string_view pack_name) -> string;
    [[nodiscard]] static auto IsSpritePath(string_view path) -> bool;
    [[nodiscard]] static auto BuildData(const vector<Entry>& entries) -> vector<uint8>;
    [[nodiscard]] static auto ParseData(const_span<uint8> data) -> vector<Entry>;
};

class SpriteStreamingManifestIndex final
{
public:
    SpriteStreamingManifestIndex() = default;
    SpriteStreamingManifestIndex(const SpriteStreamingManifestIndex&) = delete;
    SpriteStreamingManifestIndex(SpriteStreamingManifestIndex&&) noexcept = default;
    auto operator=(const SpriteStreamingManifestIndex&) = delete;
    auto operator=(SpriteStreamingManifestIndex&&) noexcept -> SpriteStreamingManifestIndex& = default;
    ~SpriteStreamingManifestIndex() = default;

    void LoadFromResources(const FileSystem& resources, const vector<string>& pack_entries, string_view compatibility_version, string_view game_version);

    [[nodiscard]] auto IsLoaded() const noexcept -> bool { return _loaded; }
    [[nodiscard]] auto FindEntry(string_view path) const -> const SpriteStreamingManifest::Entry*;
    [[nodiscard]] auto GetGenerationHash() const noexcept -> uint32 { return _generationHash; }

private:
    bool _loaded {};
    uint32 _generationHash {};
    unordered_map<string, SpriteStreamingManifest::Entry> _entries {};
};

FO_END_NAMESPACE