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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

// Atlas and render-target dumps land in the process working directory under a timestamped name the caller
// cannot predict, so a test records the set present up front and drops only what its own run produced
namespace TexDumpArtifacts
{
    [[nodiscard]] inline auto CollectDumpDirs() -> set<string>
    {
        set<string> dirs;
        std::error_code ec;

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(".", ec)) {
            string name = fs::path_to_string(entry.path().filename());

            if (name.starts_with("TexDump_") && entry.is_directory(ec)) {
                dirs.emplace(std::move(name));
            }
        }

        return dirs;
    }

    inline void RemoveNewDumpDirs(const set<string>& present_before)
    {
        for (const string& dir : CollectDumpDirs()) {
            if (!present_before.contains(dir)) {
                (void)fs::remove_dir_tree(dir);
            }
        }
    }
}

FO_END_NAMESPACE
