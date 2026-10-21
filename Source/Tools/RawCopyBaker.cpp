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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "RawCopyBaker.h"
#include "Application.h"
#include "ImageDependencyGraph.h"

FO_BEGIN_NAMESPACE

RawCopyBaker::RawCopyBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx), NAME, BakingOutputKind::Resource)
{
}

RawCopyBaker::~RawCopyBaker()
{
}

auto RawCopyBaker::CollectResourceDependencies(const FileCollection& files, string_view path, const ResourceProviderCallback& providers) const -> vector<string>
{
    ignore_unused(providers);
    ignore_unused(files);
    string extension = strex(path).get_file_extension();
    vector<string> result;

    if (extension != "fofnt" && extension != "fnt") {
        return result;
    }

    auto resources = _context->GetPackBakedFiles() ? _context->GetPackBakedFiles() : _context->GetBakedFiles();
    FO_VERIFY_AND_THROW(resources, "Font dependency collection has no baked resources", path);
    File file = resources->ReadFile(path);
    FO_VERIFY_AND_THROW(file, "Font dependency descriptor is missing", path);

    if (extension == "fofnt") {
        istringstream input {make_stream_string(file.GetStr())};
        string line;

        while (getline(input, line)) {
            istringstream record {make_stream_string(line)};
            string key;
            string image;
            record >> key;

            if (key != "Image") {
                continue;
            }

            record >> image;
            FO_VERIFY_AND_THROW(!image.empty(), "Font Image dependency is empty", path);

            if (image.ends_with('*')) {
                image.pop_back();
            }

            result.emplace_back(ImageReferencePath(path, image));
            break;
        }
    }
    else {
        data_reader reader {file.GetDataSpan()};
        const_span<uint8_t> signature = reader.read_bytes(4);
        FO_VERIFY_AND_THROW(signature[0] == 'B' && signature[1] == 'M' && signature[2] == 'F' && signature[3] == 3, "Invalid BMFont dependency descriptor", path);

        while (reader.get_unread_size() != 0) {
            uint8_t block = reader.read<uint8_t>();
            uint32_t size = reader.read<uint32_t>();
            const_span<uint8_t> payload = reader.read_bytes(size);

            if (block != 3) {
                continue;
            }

            size_t start = 0;

            while (start < payload.size()) {
                auto end = std::find(payload.begin() + numeric_cast<ptrdiff_t>(start), payload.end(), uint8_t {});
                FO_VERIFY_AND_THROW(end != payload.end(), "BMFont page name is unterminated", path);
                size_t length = numeric_cast<size_t>(end - payload.begin()) - start;
                FO_VERIFY_AND_THROW(length != 0, "BMFont page name is empty", path);
                string image {make_ptr(&payload[start]).reinterpret_as<const char>().get(), length};
                result.emplace_back(ImageReferencePath(path, image));
                start += length + 1;
            }
        }
    }

    FO_VERIFY_AND_THROW(!result.empty(), "Font has no image dependencies", path);
    return result;
}

void RawCopyBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_TRACE_ZONE(Baking);

    // Collect files
    vector<File> filtered_files;

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            if (_context->IsImageOutputOwned(file_header.GetPath())) {
                continue;
            }

            string ext = strex(file_header.GetPath()).get_file_extension();
            auto it = std::ranges::find(_context->GetSettings()->Baking.RawCopyFileExtensions, ext);

            if (it == _context->GetSettings()->Baking.RawCopyFileExtensions.end()) {
                continue;
            }
            if (_context->HasBakeChecker() && !_context->CheckOutput(file_header.GetPath(), file_header.GetWriteTime())) {
                continue;
            }

            filtered_files.emplace_back(File::Load(file_header));
        }
    }
    else {
        if (_context->IsImageOutputOwned(target_path)) {
            return;
        }

        string ext = strex(target_path).get_file_extension();
        auto it = std::ranges::find(_context->GetSettings()->Baking.RawCopyFileExtensions, ext);

        if (it == _context->GetSettings()->Baking.RawCopyFileExtensions.end()) {
            return;
        }

        auto file = files.FindFileByPath(target_path);

        if (!file) {
            return;
        }
        if (_context->HasBakeChecker() && !_context->CheckOutput(file.GetPath(), file.GetWriteTime())) {
            return;
        }

        filtered_files.emplace_back(std::move(file));
    }

    if (filtered_files.empty()) {
        return;
    }

    // Process files
    for (const auto& file : filtered_files) {
        _context->WriteData(file.GetPath(), file.GetData());
    }
}

FO_END_NAMESPACE
