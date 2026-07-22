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

#include "catch_amalgamated.hpp"

#include "DiskFileSystem.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

static auto MakeTempMountedDir(string_view name) -> string
{
    const auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_string(base);
}

TEST_CASE("FileSystem")
{
    SECTION("MountedDirectorySupportsFilteringAndReading")
    {
        const string temp_dir = MakeTempMountedDir("filesystem_mount");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        REQUIRE(fs_write_file(strex(temp_dir).combine_path("texts/a.txt").str(), string_view {"alpha"}));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("texts/aroot.txt").str(), string_view {"not-root"}));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("texts/b.bin").str(), string_view {"beta"}));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("texts/nested/c.txt").str(), string_view {"gamma"}));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("maps/Generated/_compose.fomap").str(), string_view {"scratch"}));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("maps/Generated/Authored.fomap").str(), string_view {"authored"}));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("root.txt").str(), string_view {"root"}));

        FileSystem fs;
        fs.AddDirSource(temp_dir, true);

        CHECK(fs.IsFileExists("texts/a.txt"));
        CHECK_FALSE(fs.IsFileExists("missing.txt"));
        CHECK(fs.ReadFileText("texts/a.txt") == "alpha");

        const FileHeader header = fs.ReadFileHeader("texts/a.txt");
        REQUIRE(header);
        CHECK(header.GetNameNoExt() == "a");
        CHECK(header.GetPath() == "texts/a.txt");
        CHECK(header.GetSize() == 5);
        CHECK(header.GetDataSource()->IsDiskDir());

        const File file = fs.ReadFile("texts/b.bin");
        REQUIRE(file);
        CHECK(file.GetStr() == "beta");

        const FileCollection txt_files = fs.FilterFiles("txt");
        CHECK(txt_files.GetFilesCount() == 4);
        CHECK(txt_files.FindFileByName("a").GetStr() == "alpha");
        CHECK(txt_files.FindFileByPath("root.txt").GetStr() == "root");

        const FileCollection text_dir_files = fs.FilterFiles("", "texts", false);
        CHECK(text_dir_files.GetFilesCount() == 3);

        const FileCollection all_files = fs.GetAllFiles();
        CHECK(all_files.GetFilesCount() == 7);

        const vector<string> no_patterns;
        const vector<string> root_txt_patterns = {"*.txt"};
        const FileCollection root_txt_files = fs.FilterFiles(root_txt_patterns, no_patterns);
        REQUIRE(root_txt_files.GetFilesCount() == 1);
        CHECK(root_txt_files.FindFileByPath("root.txt"));

        const vector<string> recursive_txt_patterns = {"**/*.txt"};
        const FileCollection recursive_txt_files = fs.FilterFiles(recursive_txt_patterns, no_patterns);
        CHECK(recursive_txt_files.GetFilesCount() == 4);
        CHECK(recursive_txt_files.FindFileByPath("texts/nested/c.txt"));

        const vector<string> direct_text_patterns = {"texts/*.txt"};
        const FileCollection direct_text_files = fs.FilterFiles(direct_text_patterns, no_patterns);
        REQUIRE(direct_text_files.GetFilesCount() == 2);
        CHECK(direct_text_files.FindFileByPath("texts/a.txt"));

        const vector<string> root_name_patterns = {"**/root.txt"};
        const FileCollection root_name_files = fs.FilterFiles(root_name_patterns, no_patterns);
        REQUIRE(root_name_files.GetFilesCount() == 1);
        CHECK(root_name_files.FindFileByPath("root.txt"));

        const vector<string> map_patterns = {"maps\\**\\*.fomap"};
        const vector<string> scratch_map_patterns = {"**/_*.fomap"};
        const FileCollection authored_maps = fs.FilterFiles(map_patterns, scratch_map_patterns);
        REQUIRE(authored_maps.GetFilesCount() == 1);
        CHECK(authored_maps.FindFileByPath("maps/Generated/Authored.fomap"));

        const vector<string> single_character_patterns = {"**/?.txt"};
        const FileCollection single_character_names = fs.FilterFiles(single_character_patterns, no_patterns);
        CHECK(single_character_names.GetFilesCount() == 2);

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("FileReaderSupportsEndianReadsSeekingAndFragments")
    {
        const array<uint8_t, 15> data {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 'O', 'K'}};

        FileReader reader {{data.data(), data.size()}};

        CHECK(reader.GetUInt8() == 0x01);
        CHECK(reader.GetBEUInt16() == 0x0203);
        CHECK(reader.GetLEUInt16() == 0x0504);
        CHECK(reader.GetBEUInt32() == 0x06070809);
        CHECK(reader.GetLEUInt32() == 0x0D0C0B0A);
        CHECK(reader.GetCurPos() == 13);
        CHECK(reader.SeekFragment("O"));
        CHECK(reader.GetCurPos() == 13);
        CHECK(reader.GetStr().ends_with("OK"));

        reader.SetCurPos(0);
        reader.GoForward(4);
        CHECK(reader.GetCurPos() == 4);
        reader.GoBack(2);
        CHECK(reader.GetCurPos() == 2);
    }

    SECTION("FileReaderSupportsNullTerminatedAndTrailingFragmentCases")
    {
        const array<uint8_t, 11> data {{'H', 'i', 0, 'B', 'y', 'e', 0, 'O', 'K', '!', '!'}};

        FileReader reader {{data.data(), data.size()}};

        CHECK(reader.GetStrNT() == "Hi");
        CHECK(reader.GetCurPos() == 3);
        CHECK(reader.GetStrNT() == "Bye");
        CHECK(reader.GetCurPos() == 7);
        CHECK(reader.SeekFragment("OK!!"));
        CHECK(reader.GetCurPos() == 7);
        CHECK(reader.GetStr().ends_with("OK!!"));

        reader.SetCurPos(0);
        CHECK_FALSE(reader.SeekFragment("Missing"));
        CHECK(reader.GetCurPos() == 0);
    }

    SECTION("FileCollectionReportsMissingEntriesWithoutThrowing")
    {
        const string temp_dir = MakeTempMountedDir("filesystem_missing_lookup");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        REQUIRE(fs_write_file(strex(temp_dir).combine_path("entries/one.txt").str(), string_view {"one"}));

        FileSystem fs;
        fs.AddDirSource(temp_dir, true);

        const FileCollection files = fs.GetAllFiles();
        REQUIRE(files.GetFilesCount() == 1);
        CHECK(files.GetFileByIndex(0).GetNameNoExt() == "one");
        CHECK_FALSE(files.FindFileByName("missing"));
        CHECK_FALSE(files.FindFileByPath("entries/missing.txt"));

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("CachedDirectoryCanRefreshItsFileIndex")
    {
        const string temp_dir = MakeTempMountedDir("filesystem_refresh");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        REQUIRE(fs_write_file(strex(temp_dir).combine_path("first.txt").str(), string_view {"first"}));

        FileSystem fs;
        fs.AddDirSource(temp_dir, true);

        REQUIRE(fs.IsFileExists("first.txt"));
        REQUIRE_FALSE(fs.IsFileExists("second.txt"));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("second.txt").str(), string_view {"second"}));
        REQUIRE_FALSE(fs.IsFileExists("second.txt"));

        CHECK(fs.ReindexDataSources());

        CHECK(fs.IsFileExists("second.txt"));
        CHECK(fs.ReadFileText("second.txt") == "second");

        CHECK_FALSE(fs.ReindexDataSources());
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("first.txt").str(), string_view {"first updated"}));
        CHECK(fs.ReindexDataSources());
        CHECK(fs.ReadFileText("first.txt") == "first updated");

        REQUIRE(fs_remove_file(strex(temp_dir).combine_path("second.txt").str()));
        CHECK(fs.ReindexDataSources());
        CHECK_FALSE(fs.IsFileExists("second.txt"));
        CHECK_FALSE(fs.ReindexDataSources());
        CHECK(fs_remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
