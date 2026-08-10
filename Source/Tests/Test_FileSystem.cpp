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

static_assert(std::same_as<decltype(std::declval<const File&>().GetData()), vector<byte>>);
static_assert(std::same_as<decltype(std::declval<const File&>().GetDataSpan()), const_span<byte>>);
static_assert(std::same_as<decltype(std::declval<const FileReader&>().GetData()), vector<byte>>);
static_assert(std::same_as<decltype(std::declval<const FileReader&>().GetDataSpan()), const_span<byte>>);
static_assert(std::constructible_from<FileReader, const_span<byte>>);
static_assert(!std::constructible_from<FileReader, const_span<uint8_t>>);

static auto MakeTempMountedDir(string_view name) -> u8string
{
    auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_u8string(base);
}

TEST_CASE("FileSystem")
{
    SECTION("MountedDirectorySupportsFilteringAndReading")
    {
        u8string temp_dir = MakeTempMountedDir("filesystem_mount");
        bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        u8string text_file_path = fs_combine_path(temp_dir.view(), "texts/a.txt");
        u8string alternate_text_file_path = fs_combine_path(temp_dir.view(), "texts/aroot.txt");
        u8string binary_file_path = fs_combine_path(temp_dir.view(), "texts/b.bin");
        u8string raw_file_path = fs_combine_path(temp_dir.view(), "texts/raw.bin");
        u8string nested_text_file_path = fs_combine_path(temp_dir.view(), "texts/nested/c.txt");
        u8string scratch_map_file_path = fs_combine_path(temp_dir.view(), "maps/Generated/_compose.fomap");
        u8string authored_map_file_path = fs_combine_path(temp_dir.view(), "maps/Generated/Authored.fomap");
        u8string root_file_path = fs_combine_path(temp_dir.view(), "root.txt");
        REQUIRE(fs_write_file_text(text_file_path.view(), u8"alpha"));
        REQUIRE(fs_write_file_text(alternate_text_file_path.view(), u8"not-root"));
        REQUIRE(fs_write_file_bytes(binary_file_path.view(), string_to_byte_span("beta")));
        vector<byte> raw_bytes = {byte {0x00}, byte {0x80}, byte {0xFF}};
        REQUIRE(fs_write_file_bytes(raw_file_path.view(), raw_bytes));
        REQUIRE(fs_write_file_text(nested_text_file_path.view(), u8"gamma"));
        REQUIRE(fs_write_file_text(scratch_map_file_path.view(), u8"scratch"));
        REQUIRE(fs_write_file_text(authored_map_file_path.view(), u8"authored"));
        REQUIRE(fs_write_file_text(root_file_path.view(), u8"root"));

        FileSystem fs;
        fs.AddDirSource(temp_dir, true);

        CHECK(fs.IsFileExists("texts/a.txt"));
        CHECK_FALSE(fs.IsFileExists("missing.txt"));
        CHECK(fs.ReadFileText("texts/a.txt") == u8string {u8"alpha"});

        FileHeader header = fs.ReadFileHeader("texts/a.txt");
        REQUIRE(header);
        CHECK(header.GetNameNoExt() == "a");
        CHECK(header.GetPath() == "texts/a.txt");
        CHECK(header.GetSize() == 5);
        CHECK(header.GetDataSource()->IsDiskDir());

        File file = fs.ReadFile("texts/b.bin");
        REQUIRE(file);
        CHECK(file.GetText() == u8string {u8"beta"});

        File raw_file = fs.ReadFile("texts/raw.bin");
        REQUIRE(raw_file);
        CHECK(raw_file.GetData() == raw_bytes);
        CHECK(std::ranges::equal(raw_file.GetDataSpan(), raw_bytes));
        FileReader raw_reader = raw_file.GetReader();
        CHECK(raw_reader.GetUInt8() == 0x00);
        CHECK(raw_reader.GetUInt8() == 0x80);
        CHECK(raw_reader.GetUInt8() == 0xFF);

        FileCollection txt_files = fs.FilterFiles("txt");
        CHECK(txt_files.GetFilesCount() == 4);
        CHECK(txt_files.FindFileByName("a").GetText() == u8string {u8"alpha"});
        CHECK(txt_files.FindFileByPath("root.txt").GetText() == u8string {u8"root"});

        FileCollection text_dir_files = fs.FilterFiles("", "texts", false);
        CHECK(text_dir_files.GetFilesCount() == 4);

        FileCollection all_files = fs.GetAllFiles();
        CHECK(all_files.GetFilesCount() == 8);

        vector<string> no_patterns;
        vector<string> root_txt_patterns = {"*.txt"};
        FileCollection root_txt_files = fs.FilterFiles(root_txt_patterns, no_patterns);
        REQUIRE(root_txt_files.GetFilesCount() == 1);
        CHECK(root_txt_files.FindFileByPath("root.txt"));

        vector<string> recursive_txt_patterns = {"**/*.txt"};
        FileCollection recursive_txt_files = fs.FilterFiles(recursive_txt_patterns, no_patterns);
        CHECK(recursive_txt_files.GetFilesCount() == 4);
        CHECK(recursive_txt_files.FindFileByPath("texts/nested/c.txt"));

        vector<string> direct_text_patterns = {"texts/*.txt"};
        FileCollection direct_text_files = fs.FilterFiles(direct_text_patterns, no_patterns);
        REQUIRE(direct_text_files.GetFilesCount() == 2);
        CHECK(direct_text_files.FindFileByPath("texts/a.txt"));

        vector<string> root_name_patterns = {"**/root.txt"};
        FileCollection root_name_files = fs.FilterFiles(root_name_patterns, no_patterns);
        REQUIRE(root_name_files.GetFilesCount() == 1);
        CHECK(root_name_files.FindFileByPath("root.txt"));

        vector<string> map_patterns = {"maps\\**\\*.fomap"};
        vector<string> scratch_map_patterns = {"**/_*.fomap"};
        FileCollection authored_maps = fs.FilterFiles(map_patterns, scratch_map_patterns);
        REQUIRE(authored_maps.GetFilesCount() == 1);
        CHECK(authored_maps.FindFileByPath("maps/Generated/Authored.fomap"));

        vector<string> single_character_patterns = {"**/?.txt"};
        FileCollection single_character_names = fs.FilterFiles(single_character_patterns, no_patterns);
        CHECK(single_character_names.GetFilesCount() == 2);

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("FileReaderSupportsEndianReadsSeekingAndFragments")
    {
        array<byte, 15> data {{byte {0x01}, byte {0x02}, byte {0x03}, byte {0x04}, byte {0x05}, byte {0x06}, byte {0x07}, byte {0x08}, byte {0x09}, byte {0x0A}, byte {0x0B}, byte {0x0C}, byte {0x0D}, byte {'O'}, byte {'K'}}};

        FileReader reader {data};

        CHECK(reader.GetUInt8() == 0x01);
        CHECK(reader.GetBEUInt16() == 0x0203);
        CHECK(reader.GetLEUInt16() == 0x0504);
        CHECK(reader.GetBEUInt32() == 0x06070809);
        CHECK(reader.GetLEUInt32() == 0x0D0C0B0A);
        CHECK(reader.GetCurPos() == 13);
        CHECK(reader.SeekFragment("O"));
        CHECK(reader.GetCurPos() == 13);
        u8string reader_text = reader.GetText();
        CHECK(reader_text.view().native_view().ends_with(u8"OK"));

        reader.SetCurPos(0);
        reader.GoForward(4);
        CHECK(reader.GetCurPos() == 4);
        reader.GoBack(2);
        CHECK(reader.GetCurPos() == 2);
    }

    SECTION("FileReaderSupportsNullTerminatedAndTrailingFragmentCases")
    {
        array<byte, 11> data {{byte {'H'}, byte {'i'}, byte {0}, byte {'B'}, byte {'y'}, byte {'e'}, byte {0}, byte {'O'}, byte {'K'}, byte {'!'}, byte {'!'}}};

        FileReader reader {data};

        CHECK(reader.GetStrNT() == "Hi");
        CHECK(reader.GetCurPos() == 3);
        CHECK(reader.GetStrNT() == "Bye");
        CHECK(reader.GetCurPos() == 7);
        CHECK(reader.SeekFragment("OK!!"));
        CHECK(reader.GetCurPos() == 7);
        u8string reader_text = reader.GetText();
        CHECK(reader_text.view().native_view().ends_with(u8"OK!!"));

        reader.SetCurPos(0);
        CHECK_FALSE(reader.SeekFragment("Missing"));
        CHECK(reader.GetCurPos() == 0);
    }

    SECTION("FileCollectionReportsMissingEntriesWithoutThrowing")
    {
        u8string temp_dir = MakeTempMountedDir("filesystem_missing_lookup");
        bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        u8string entry_path = fs_combine_path(temp_dir.view(), "entries/one.txt");
        REQUIRE(fs_write_file_text(entry_path.view(), u8"one"));

        FileSystem fs;
        fs.AddDirSource(temp_dir, true);

        FileCollection files = fs.GetAllFiles();
        REQUIRE(files.GetFilesCount() == 1);
        CHECK(files.GetFileByIndex(0).GetNameNoExt() == "one");
        CHECK_FALSE(files.FindFileByName("missing"));
        CHECK_FALSE(files.FindFileByPath("entries/missing.txt"));

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("CachedDirectoryCanRefreshItsFileIndex")
    {
        u8string temp_dir = MakeTempMountedDir("filesystem_refresh");
        bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        u8string first_path = fs_combine_path(temp_dir, "first.txt");
        u8string second_path = fs_combine_path(temp_dir, "second.txt");
        REQUIRE(fs_write_file_text(first_path, u8"first"));

        FileSystem fs;
        fs.AddDirSource(temp_dir, true);

        REQUIRE(fs.IsFileExists("first.txt"));
        REQUIRE_FALSE(fs.IsFileExists("second.txt"));
        REQUIRE(fs_write_file_text(second_path, u8"second"));
        REQUIRE_FALSE(fs.IsFileExists("second.txt"));

        CHECK(fs.ReindexDataSources());

        CHECK(fs.IsFileExists("second.txt"));
        CHECK(fs.ReadFileText("second.txt") == u8"second");

        CHECK_FALSE(fs.ReindexDataSources());
        REQUIRE(fs_write_file_text(first_path, u8"first updated"));
        CHECK(fs.ReindexDataSources());
        CHECK(fs.ReadFileText("first.txt") == u8"first updated");

        REQUIRE(fs_remove_file(second_path));
        CHECK(fs.ReindexDataSources());
        CHECK_FALSE(fs.IsFileExists("second.txt"));
        CHECK_FALSE(fs.ReindexDataSources());
        CHECK(fs_remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
