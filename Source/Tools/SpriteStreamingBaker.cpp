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

#include "SpriteStreamingBaker.h"

#include "SpriteStreamingManifest.h"

FO_BEGIN_NAMESPACE

SpriteStreamingBaker::SpriteStreamingBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx))
{
    FO_STACK_TRACE_ENTRY();
}

void SpriteStreamingBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    vector<FileHeader> filtered_files;
    uint64 max_write_time = 0;

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            if (!SpriteStreamingManifest::IsSpritePath(file_header.GetPath())) {
                continue;
            }

            filtered_files.emplace_back(file_header.Copy());
            max_write_time = std::max(max_write_time, file_header.GetWriteTime());
        }
    }
    else {
        if (!SpriteStreamingManifest::IsSpritePath(target_path)) {
            return;
        }

        const auto file = files.FindFileByPath(target_path);

        if (!file) {
            return;
        }

        filtered_files.emplace_back(file.Copy());
        max_write_time = file.GetWriteTime();
    }

    if (filtered_files.empty()) {
        return;
    }

    const string manifest_path = SpriteStreamingManifest::GetPackFileName(_context->PackName);

    if (_context->BakeChecker && !_context->BakeChecker(manifest_path, max_write_time)) {
        return;
    }

    vector<SpriteStreamingManifest::Entry> manifest_entries;
    manifest_entries.reserve(filtered_files.size());

    for (const auto& file_header : filtered_files) {
        const string baked_path = strex(_context->Settings->BakeOutput).combine_path(_context->PackName).combine_path(file_header.GetPath());
        const auto baked_data = fs_read_file(baked_path);

        FO_RUNTIME_ASSERT_STR(baked_data.has_value(), strex("Sprite streaming manifest source not found '{}'", baked_path));

        SpriteStreamingManifest::Entry entry;
        entry.Path = file_header.GetPath();
        entry.FileSize = numeric_cast<uint32>(baked_data->size());
        entry.ContentHash = Hashing::MurmurHash2(baked_data->data(), baked_data->size());
        manifest_entries.emplace_back(std::move(entry));
    }

    const auto manifest_data = SpriteStreamingManifest::BuildData(manifest_entries);
    _context->WriteData(manifest_path, manifest_data);
}

FO_END_NAMESPACE