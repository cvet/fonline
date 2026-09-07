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

#include "catch_amalgamated.hpp"

#include "CommonHelpers.h"
#include "DiskFileSystem.h"
#include "SafeArithmetics.h"

FO_BEGIN_NAMESPACE

static auto MakeTempTestDir(string_view name) -> string
{
    auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_string(base);
}

TEST_CASE("DiskFileSystem")
{
    SECTION("ReadWriteRenameAndRemoveRoundtrip")
    {
        string temp_dir = MakeTempTestDir("diskfs_roundtrip");
        string file_path = strex(temp_dir).combine_path("nested/data.txt").str();
        string renamed_path = strex(temp_dir).combine_path("nested/renamed.txt").str();
        string_view content {"hello filesystem"};

        bool removed_before_roundtrip = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before_roundtrip);

        REQUIRE(fs_write_file(file_path, content));
        CHECK(fs_exists(file_path));
        CHECK_FALSE(fs_is_dir(file_path));
        REQUIRE(fs_file_size(file_path).has_value());
        CHECK(*fs_file_size(file_path) == content.size());
        REQUIRE(fs_read_file(file_path).has_value());
        CHECK(*fs_read_file(file_path) == content);
        CHECK(fs_compare_file_content(file_path, {reinterpret_cast<const uint8_t*>(content.data()), content.size()}));

        REQUIRE(fs_rename(file_path, renamed_path));
        CHECK_FALSE(fs_exists(file_path));
        CHECK(fs_exists(renamed_path));
        REQUIRE(fs_remove_file(renamed_path));
        CHECK_FALSE(fs_exists(renamed_path));
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("IterateDirRespectsRecursiveFlag")
    {
        string temp_dir = MakeTempTestDir("diskfs_iterate");
        string top_file = strex(temp_dir).combine_path("top.txt").str();
        string nested_file = strex(temp_dir).combine_path("sub/nested.txt").str();

        bool removed_before_iterate = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before_iterate);
        REQUIRE(fs_write_file(top_file, string_view {"top"}));
        REQUIRE(fs_write_file(nested_file, string_view {"nested"}));

        vector<string> flat_entries;
        fs_iterate_dir(temp_dir, false, [&](string_view path, size_t, uint64_t) { flat_entries.emplace_back(path); });
        CHECK(flat_entries.size() == 1);
        CHECK(flat_entries.front() == "top.txt");

        vector<string> recursive_entries;
        fs_iterate_dir(temp_dir, true, [&](string_view path, size_t, uint64_t) { recursive_entries.emplace_back(path); });
        CHECK(recursive_entries.size() == 2);
        CHECK(std::ranges::find(recursive_entries, string {"top.txt"}) != recursive_entries.end());
        CHECK(std::ranges::find(recursive_entries, string {"sub/nested.txt"}) != recursive_entries.end());

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("TouchAndStreamHelpersWork")
    {
        string temp_dir = MakeTempTestDir("diskfs_stream");
        string file_path = strex(temp_dir).combine_path("touch.bin").str();

        bool removed_before_stream = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before_stream);
        REQUIRE(fs_create_directories(temp_dir));
        REQUIRE(fs_touch_file(file_path));
        CHECK(fs_exists(file_path));
        CHECK(fs_last_write_time(file_path) != 0);

        std::istringstream stream {std::string {"abcdef"}, std::ios::binary};
        CHECK(stream_get_size(stream) == 6);
        CHECK(stream_get_read_pos(stream) == 0);

        array<char, 3> buf {};
        auto buf_data = make_nptr(buf.data());
        REQUIRE(stream_read_exact(stream, make_span(buf_data, buf.size())));
        CHECK(string_view {buf.data(), buf.size()} == "abc");
        CHECK(stream_get_read_pos(stream) == 3);
        REQUIRE(stream_set_read_pos(stream, 1, std::ios_base::cur));
        CHECK(stream_get_read_pos(stream) == 4);
        REQUIRE(stream_read_exact(stream, make_span(buf_data, 2)));
        CHECK(string_view {buf.data(), 2} == "ef");

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("FileHashMatchesInMemoryReference")
    {
        string temp_dir = MakeTempTestDir("diskfs_hash");
        string file_path = strex(temp_dir).combine_path("hash.bin").str();
        bool removed_before_hash = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before_hash);

        auto check_hash = [&file_path](size_t size) {
            vector<uint8_t> data(size);

            for (size_t index = 0; index < size; index++) {
                data[index] = numeric_cast<uint8_t>((index * 37u + 11u) & 0xFFu);
            }

            REQUIRE(fs_write_file(file_path, data));
            REQUIRE(fs_hash_file(file_path).has_value());
            CHECK(*fs_hash_file(file_path) == fs_hash_data(data));
        };

        for (auto size : {size_t(0), size_t(1), size_t(3), size_t(4), size_t(15), size_t(16), size_t(17), size_t(47), size_t(48), size_t(49), size_t(63), size_t(64), size_t(65), size_t(96), size_t(97), size_t(70000)}) {
            check_hash(size);
        }

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("MakeWritablePathLayersRelativeUnderRoot")
    {
        string root = strex("/data").combine_path("user").str();
        string nested_relative = strex("Resources").combine_path("Sub").str();

        // Portable layout (empty root): the relative path is returned unchanged, written next to the exe
        CHECK(fs_make_writable_path("", "Cache") == "Cache");
        CHECK(fs_make_writable_path("", nested_relative) == nested_relative);

        // Installed layout: the relative path is layered under the writable root
        CHECK(fs_make_writable_path(root, "Cache") == strex(root).combine_path("Cache").str());
        CHECK(fs_make_writable_path(root, nested_relative) == strex(root).combine_path(nested_relative).str());

        // An already-absolute relative path is never relocated under the root, in either layout
        string absolute_input = MakeTempTestDir("diskfs_writable_abs");
        CHECK(fs_is_absolute_path(absolute_input));
        CHECK(fs_make_writable_path(root, absolute_input) == absolute_input);
        CHECK(fs_make_writable_path("", absolute_input) == absolute_input);
    }
}

// Which primitive lands the requested name and which keeps whatever the entry is already called, on both
// filesystem kinds — the split callers addressing files by exact name depend on
static auto IsCaseInsensitiveFs(string_view dir) -> bool
{
    FO_STACK_TRACE_ENTRY();

    string upper_probe = strex(dir).combine_path("CaseProbe.tmp").str();
    string lower_probe = strex(dir).combine_path("caseprobe.tmp").str();

    if (!fs_write_file(upper_probe, string_view {"probe"})) {
        return false;
    }

    bool case_insensitive = fs_exists(lower_probe);
    ignore_unused(fs_remove_file(upper_probe));

    return case_insensitive;
}

static auto HasExactDirEntry(string_view dir, string_view name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;

    for (const auto& entry : std::filesystem::directory_iterator {std::filesystem::path {fs_make_path(dir)}, ec}) {
        if (fs_path_to_string(entry.path().filename()) == name) {
            return true;
        }
    }

    return false;
}

TEST_CASE("DiskFileSystemNameCase")
{
    SECTION("RenameLandsTheRequestedNameOverADifferentlyCasedTarget")
    {
        string temp_dir = MakeTempTestDir("diskfs_case_rename");
        string upper_path = strex(temp_dir).combine_path("Data.txt").str();
        string lower_path = strex(temp_dir).combine_path("data.txt").str();
        string source_path = strex(temp_dir).combine_path("Source.tmp").str();

        ignore_unused(fs_remove_dir_tree(temp_dir));
        REQUIRE(fs_create_directories(temp_dir));

        bool case_insensitive = IsCaseInsensitiveFs(temp_dir);

        REQUIRE(fs_write_file(upper_path, string_view {"before"}));
        REQUIRE(fs_write_file(source_path, string_view {"after"}));
        REQUIRE(fs_rename(source_path, lower_path));

        // Renaming replaces the whole directory entry, so unlike an overwriting write it does establish the
        // requested spelling — Updater::ReplaceFileSafely relies on exactly this
        CHECK(HasExactDirEntry(temp_dir, "data.txt"));
        REQUIRE(fs_read_file(lower_path).has_value());
        CHECK(*fs_read_file(lower_path) == "after");
        CHECK_FALSE(fs_exists(source_path));

        if (case_insensitive) {
            CHECK_FALSE(HasExactDirEntry(temp_dir, "Data.txt"));
        }

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("WriteFileTargetsTheSameFileRegardlessOfNameCase")
    {
        string temp_dir = MakeTempTestDir("diskfs_case_write");
        string upper_path = strex(temp_dir).combine_path("Data.txt").str();
        string lower_path = strex(temp_dir).combine_path("data.txt").str();

        ignore_unused(fs_remove_dir_tree(temp_dir));
        REQUIRE(fs_create_directories(temp_dir));

        bool case_insensitive = IsCaseInsensitiveFs(temp_dir);

        REQUIRE(fs_write_file(upper_path, string_view {"first"}));
        REQUIRE(fs_write_file(lower_path, string_view {"second"}));

        // The content written last always wins under the name it was written with; which directory entry name
        // survives is deliberately not guaranteed, so a caller needing the exact name reconciles it itself
        REQUIRE(fs_read_file(lower_path).has_value());
        CHECK(*fs_read_file(lower_path) == "second");
        REQUIRE(fs_read_file(upper_path).has_value());
        CHECK(*fs_read_file(upper_path) == (case_insensitive ? "second" : "first"));

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("CreateDirectoriesKeepsAnExistingDifferentlyCasedDirectory")
    {
        string temp_dir = MakeTempTestDir("diskfs_case_dir");
        string upper_dir = strex(temp_dir).combine_path("Data").str();
        string lower_dir = strex(temp_dir).combine_path("data").str();

        ignore_unused(fs_remove_dir_tree(temp_dir));
        REQUIRE(fs_create_directories(temp_dir));

        bool case_insensitive = IsCaseInsensitiveFs(temp_dir);

        REQUIRE(fs_create_directories(upper_dir));
        REQUIRE(fs_create_directories(lower_dir));

        // Creating a directory never re-spells an existing one, so a case-only rename of a content directory keeps
        // the old spelling for every path underneath until a caller reconciles it, as the baker does
        CHECK(HasExactDirEntry(temp_dir, "Data"));
        CHECK(HasExactDirEntry(temp_dir, "data") == !case_insensitive);

        CHECK(fs_remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
