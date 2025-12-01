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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

RawCopyBaker::RawCopyBaker(BakerData& data) :
    BaseBaker(data)
{
    FO_STACK_TRACE_ENTRY();
}

RawCopyBaker::~RawCopyBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void RawCopyBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    // Collect files
    vector<File> filtered_files;

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            const string ext = strex(file_header.GetPath()).get_file_extension();
            const auto it = std::ranges::find(_settings->RawCopyFileExtensions, ext);

            if (it == _settings->RawCopyFileExtensions.end()) {
                continue;
            }
            if (_bakeChecker && !_bakeChecker(file_header.GetPath(), file_header.GetWriteTime())) {
                continue;
            }

            filtered_files.emplace_back(File::Load(file_header));
        }
    }
    else {
        const string ext = strex(target_path).get_file_extension();
        const auto it = std::ranges::find(_settings->RawCopyFileExtensions, ext);

        if (it == _settings->RawCopyFileExtensions.end()) {
            return;
        }

        auto file = files.FindFileByPath(target_path);

        if (!file) {
            return;
        }
        if (_bakeChecker && !_bakeChecker(file.GetPath(), file.GetWriteTime())) {
            return;
        }

        filtered_files.emplace_back(std::move(file));
    }

    if (filtered_files.empty()) {
        return;
    }

    // Process files
    for (const auto& file : filtered_files) {
        _writeData(file.GetPath(), file.GetData());
    }
}

FO_END_NAMESPACE();
