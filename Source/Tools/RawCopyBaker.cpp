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
#include "Log.h"
#include "StringUtils.h"

RawCopyBaker::RawCopyBaker(const BakerSettings& settings, string pack_name, BakeCheckerCallback bake_checker, AsyncWriteDataCallback write_data, const FileSystem* baked_files) :
    BaseBaker(settings, std::move(pack_name), std::move(bake_checker), std::move(write_data), baked_files)
{
    STACK_TRACE_ENTRY();
}

RawCopyBaker::~RawCopyBaker()
{
    STACK_TRACE_ENTRY();
}

auto RawCopyBaker::IsExtSupported(string_view ext) const -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto it = std::find(_settings->RawCopyFileExtensions.begin(), _settings->RawCopyFileExtensions.end(), ext);

    if (it != _settings->RawCopyFileExtensions.end()) {
        return true;
    }

    return false;
}

void RawCopyBaker::BakeFiles(FileCollection files)
{
    STACK_TRACE_ENTRY();

    while (files.MoveNext()) {
        auto file_header = files.GetCurFileHeader();
        string ext = strex(file_header.GetPath()).getFileExtension();

        if (!IsExtSupported(ext)) {
            continue;
        }

        if (_bakeChecker && !_bakeChecker(file_header.GetPath(), file_header.GetWriteTime())) {
            continue;
        }

        auto file = files.GetCurFile();
        _writeData(file.GetPath(), file.GetData());
    }
}
