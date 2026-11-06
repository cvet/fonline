// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

// Atlas and render-target dumps land in the process working directory under a timestamped name the caller
// cannot predict, so a test records the set present up front and drops only what its own run produced
namespace TexDumpArtifacts
{
    inline auto CollectDumpDirs() -> set<string>
    {
        set<string> dirs {};
        std::error_code ec {};

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(".", ec)) {
            string name {fs::path_to_string(entry.path().filename())};

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
