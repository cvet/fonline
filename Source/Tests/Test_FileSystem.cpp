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

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "DiskFileSystem.h"
#include "FileSystem.h"
#include "ResourcePack.h"

FO_BEGIN_NAMESPACE

static auto MakeTempMountedDir(string_view name) -> string
{
    auto base {std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count())};
    return fs::path_to_string(base);
}

// Stands in for a pack: hands over its whole content, so a file system built from these indexes itself
class SnapshotTestSource final : public DataSource
{
public:
    explicit SnapshotTestSource(string name, map<string, string> files) :
        _name {std::move(name)},
        _files {std::move(files)}
    {
    }
    SnapshotTestSource(const SnapshotTestSource&) = delete;
    SnapshotTestSource(SnapshotTestSource&&) noexcept = delete;
    auto operator=(const SnapshotTestSource&) = delete;
    auto operator=(SnapshotTestSource&&) noexcept = delete;
    ~SnapshotTestSource() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _name; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override { return _files.contains(path); }

    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override
    {
        auto it {_files.find(path)};

        if (it == _files.end()) {
            return false;
        }

        size = it->second.size();
        write_time = 1000;
        return true;
    }

    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override
    {
        auto it {_files.find(path)};

        if (it == _files.end()) {
            return nullptr;
        }

        size = it->second.size();
        write_time = 1000;

        auto buf {safe_alloc::make_unique_arr<uint8_t>(size)};
        memory::copy(buf.get(), it->second.data(), size);

        auto released_buf {make_ptr<const uint8_t*>(buf.release())};
        return make_unique_del_ptr(released_buf, [](const uint8_t* raw_buf) noexcept {
            unique_arr_ptr<const uint8_t> owned_buf {raw_buf};
            ignore_unused(owned_buf);
        });
    }

    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override
    {
        ignore_unused(dir, recursive, ext);

        vector<string> names {};

        for (const auto& [path, content] : _files) {
            names.emplace_back(path);
        }

        return names;
    }

    [[nodiscard]] auto GetIndexSnapshot() const -> optional<vector<IndexedFile>> override
    {
        vector<IndexedFile> files {};

        for (const auto& [path, content] : _files) {
            files.emplace_back(IndexedFile {path, content.size(), 1000});
        }

        return files;
    }

private:
    string _name;
    map<string, string> _files;
};

TEST_CASE("FileSystem")
{
    // The index has to reproduce the probe order exactly: a source mounted later sits in front of the
    // others, so it takes every path it shares with them
    SECTION("IndexedLookupsFollowTheProbeOrder")
    {
        FileSystem resources {};
        resources.AddCustomSource(safe_alloc::make_unique<SnapshotTestSource>("Lower", map<string, string> {{"shared.txt", "lower"}, {"only-lower.txt", "kept"}}));
        resources.AddCustomSource(safe_alloc::make_unique<SnapshotTestSource>("Upper", map<string, string> {{"shared.txt", "upper-wins"}, {"only-upper.txt", "kept"}}));

        CHECK(resources.IsFileExists("shared.txt"));
        CHECK(resources.IsFileExists("only-lower.txt"));
        CHECK(resources.IsFileExists("only-upper.txt"));
        CHECK_FALSE(resources.IsFileExists("missing.txt"));

        CHECK(resources.ReadFileText("shared.txt") == "upper-wins");
        CHECK(resources.ReadFileText("only-lower.txt") == "kept");
        CHECK(resources.ReadFileText("missing.txt").empty());

        auto shared_header {resources.ReadFileHeader("shared.txt")};
        REQUIRE(shared_header);
        CHECK(shared_header.GetSize() == 10);
        CHECK(shared_header.GetDataSource()->GetPackName() == "Upper");
        CHECK_FALSE(resources.ReadFileHeader("missing.txt"));

        auto lower_header {resources.ReadFileHeader("only-lower.txt")};
        REQUIRE(lower_header);
        CHECK(lower_header.GetDataSource()->GetPackName() == "Lower");
    }

    // One source that cannot hand over a snapshot has to take the whole file system back to probing, or a
    // live directory mounted beside packs would answer from a snapshot taken before it changed
    SECTION("ALiveSourceDropsTheFileSystemBackToProbing")
    {
        string temp_dir {MakeTempMountedDir("file_system_mixed_sources")};
        bool removed_before {fs::remove_dir_tree(temp_dir)};
        ignore_unused(removed_before);

        REQUIRE(fs::write_file(strex(temp_dir).combine_path("on-disk.txt").str(), string_view {"disk"}));

        FileSystem resources {};
        resources.AddCustomSource(safe_alloc::make_unique<SnapshotTestSource>("Pack", map<string, string> {{"in-pack.txt", "pack"}}));
        resources.AddDirSource(temp_dir, true);

        CHECK(resources.ReadFileText("in-pack.txt") == "pack");
        CHECK(resources.ReadFileText("on-disk.txt") == "disk");
        CHECK(resources.IsFileExists("on-disk.txt"));
        CHECK_FALSE(resources.IsFileExists("missing.txt"));

        // A file that appears after mounting is invisible until the dir source reindexes, index or no index
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("late.txt").str(), string_view {"late"}));
        CHECK_FALSE(resources.IsFileExists("late.txt"));
        CHECK(resources.ReindexDataSources());
        CHECK(resources.ReadFileText("late.txt") == "late");

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("ReindexKeepsTheIndexAgreeingWithTheSources")
    {
        FileSystem resources {};
        resources.AddCustomSource(safe_alloc::make_unique<SnapshotTestSource>("Lower", map<string, string> {{"shared.txt", "lower"}}));
        resources.AddCustomSource(safe_alloc::make_unique<SnapshotTestSource>("Upper", map<string, string> {{"shared.txt", "upper"}}));

        ignore_unused(resources.ReindexDataSources());

        CHECK(resources.ReadFileText("shared.txt") == "upper");

        auto header {resources.ReadFileHeader("shared.txt")};
        REQUIRE(header);
        CHECK(header.GetDataSource()->GetPackName() == "Upper");
    }

    SECTION("CleanedFileSystemAnswersNothing")
    {
        FileSystem resources {};
        resources.AddCustomSource(safe_alloc::make_unique<SnapshotTestSource>("Pack", map<string, string> {{"in-pack.txt", "pack"}}));

        CHECK(resources.IsFileExists("in-pack.txt"));

        resources.CleanDataSources();

        CHECK_FALSE(resources.IsFileExists("in-pack.txt"));
        CHECK(resources.ReadFileText("in-pack.txt").empty());
        CHECK(resources.GetAllFiles().GetFilesCount() == 0);
    }

    SECTION("MountedDirectorySupportsFilteringAndReading")
    {
        string temp_dir {MakeTempMountedDir("filesystem_mount")};
        bool removed_before {fs::remove_dir_tree(temp_dir)};
        ignore_unused(removed_before);

        REQUIRE(fs::write_file(strex(temp_dir).combine_path("texts/a.txt").str(), string_view {"alpha"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("texts/aroot.txt").str(), string_view {"not-root"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("texts/b.bin").str(), string_view {"beta"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("texts/nested/c.txt").str(), string_view {"gamma"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("maps/Generated/_compose.fomap").str(), string_view {"scratch"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("maps/Generated/Authored.fomap").str(), string_view {"authored"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("root.txt").str(), string_view {"root"}));

        FileSystem fs {};
        fs.AddDirSource(temp_dir, true);

        CHECK(fs.IsFileExists("texts/a.txt"));
        CHECK_FALSE(fs.IsFileExists("missing.txt"));
        CHECK(fs.ReadFileText("texts/a.txt") == "alpha");

        FileHeader header {fs.ReadFileHeader("texts/a.txt")};
        REQUIRE(header);
        CHECK(header.GetNameNoExt() == "a");
        CHECK(header.GetPath() == "texts/a.txt");
        CHECK(header.GetSize() == 5);
        CHECK(header.GetDataSource()->IsDiskDir());

        File file {fs.ReadFile("texts/b.bin")};
        REQUIRE(file);
        CHECK(file.GetStr() == "beta");

        FileCollection txt_files {fs.FilterFiles("txt")};
        CHECK(txt_files.GetFilesCount() == 4);
        CHECK(txt_files.FindFileByName("a").GetStr() == "alpha");
        CHECK(txt_files.FindFileByPath("root.txt").GetStr() == "root");

        FileCollection text_dir_files {fs.FilterFiles("", "texts", false)};
        CHECK(text_dir_files.GetFilesCount() == 3);

        FileCollection all_files {fs.GetAllFiles()};
        CHECK(all_files.GetFilesCount() == 7);

        vector<string> no_patterns {};
        vector<string> root_txt_patterns {"*.txt"};
        FileCollection root_txt_files {fs.FilterFiles(root_txt_patterns, no_patterns)};
        REQUIRE(root_txt_files.GetFilesCount() == 1);
        CHECK(root_txt_files.FindFileByPath("root.txt"));

        vector<string> recursive_txt_patterns {"**/*.txt"};
        FileCollection recursive_txt_files {fs.FilterFiles(recursive_txt_patterns, no_patterns)};
        CHECK(recursive_txt_files.GetFilesCount() == 4);
        CHECK(recursive_txt_files.FindFileByPath("texts/nested/c.txt"));

        vector<string> direct_text_patterns {"texts/*.txt"};
        FileCollection direct_text_files {fs.FilterFiles(direct_text_patterns, no_patterns)};
        REQUIRE(direct_text_files.GetFilesCount() == 2);
        CHECK(direct_text_files.FindFileByPath("texts/a.txt"));

        vector<string> root_name_patterns {"**/root.txt"};
        FileCollection root_name_files {fs.FilterFiles(root_name_patterns, no_patterns)};
        REQUIRE(root_name_files.GetFilesCount() == 1);
        CHECK(root_name_files.FindFileByPath("root.txt"));

        vector<string> map_patterns {"maps\\**\\*.fomap"};
        vector<string> scratch_map_patterns {"**/_*.fomap"};
        FileCollection authored_maps {fs.FilterFiles(map_patterns, scratch_map_patterns)};
        REQUIRE(authored_maps.GetFilesCount() == 1);
        CHECK(authored_maps.FindFileByPath("maps/Generated/Authored.fomap"));

        vector<string> single_character_patterns {"**/?.txt"};
        FileCollection single_character_names {fs.FilterFiles(single_character_patterns, no_patterns)};
        CHECK(single_character_names.GetFilesCount() == 2);

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("FileReaderSupportsEndianReadsSeekingAndFragments")
    {
        array<uint8_t, 15> data {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 'O', 'K'}};

        FileReader reader {{data.data(), data.size()}};

        CHECK(reader.ReadUInt8() == 0x01);
        CHECK(reader.ReadBEUInt16() == 0x0203);
        CHECK(reader.ReadLEUInt16() == 0x0504);
        CHECK(reader.ReadBEUInt32() == 0x06070809);
        CHECK(reader.ReadLEUInt32() == 0x0D0C0B0A);
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
        array<uint8_t, 11> data {{'H', 'i', 0, 'B', 'y', 'e', 0, 'O', 'K', '!', '!'}};

        FileReader reader {{data.data(), data.size()}};

        CHECK(reader.ReadStrNT() == "Hi");
        CHECK(reader.GetCurPos() == 3);
        CHECK(reader.ReadStrNT() == "Bye");
        CHECK(reader.GetCurPos() == 7);
        CHECK(reader.SeekFragment("OK!!"));
        CHECK(reader.GetCurPos() == 7);
        CHECK(reader.GetStr().ends_with("OK!!"));

        reader.SetCurPos(0);
        CHECK_FALSE(reader.SeekFragment("Missing"));
        CHECK(reader.GetCurPos() == 0);
    }

    SECTION("FileReaderFailuresPreserveBoundsAndPosition")
    {
        array<uint8_t, 3> data {'a', 'b', 'c'};
        FileReader reader {{data.data(), data.size()}};
        reader.SetCurPos(2);

        auto check_read_error {[&](const auto& read, size_t requested) {
            try {
                read();
                FAIL("Out-of-bounds read succeeded");
            }
            catch (const FileSystemExeption& ex) {
                CHECK(ex.message() == "Invalid read size");
                REQUIRE(ex.params().size() == 3);
                CHECK(ex.params()[0] == "2");
                CHECK(ex.params()[1] == strex("{}", requested).str());
                CHECK(ex.params()[2] == "3");
            }

            CHECK(reader.GetCurPos() == 2);
        }};

        check_read_error([&] { (void)reader.ReadLEUInt16(); }, 2);
        check_read_error([&] { (void)reader.ReadBEUInt32(); }, 4);
        check_read_error([&] { (void)reader.GetCurDataSpan(5); }, 5);
        array<uint8_t, 4> output {};
        // FO_FILE_READER_COPY_DATA_NEGATIVE_TEST: Exercises CopyData bounds failure and unchanged reader position
        check_read_error([&] { reader.CopyData(output); }, 4);

        CHECK_THROWS(reader.ReadStrNT());
        CHECK(reader.GetCurPos() == 2);
        CHECK_THROWS(reader.GoForward(std::numeric_limits<size_t>::max()));
        CHECK(reader.GetCurPos() == 2);
    }

    SECTION("FileCollectionReportsMissingEntriesWithoutThrowing")
    {
        string temp_dir {MakeTempMountedDir("filesystem_missing_lookup")};
        bool removed_before {fs::remove_dir_tree(temp_dir)};
        ignore_unused(removed_before);

        REQUIRE(fs::write_file(strex(temp_dir).combine_path("entries/one.txt").str(), string_view {"one"}));

        FileSystem fs {};
        fs.AddDirSource(temp_dir, true);

        FileCollection files {fs.GetAllFiles()};
        REQUIRE(files.GetFilesCount() == 1);
        CHECK(files.GetFileByIndex(0).GetNameNoExt() == "one");
        CHECK_FALSE(files.FindFileByName("missing"));
        CHECK_FALSE(files.FindFileByPath("entries/missing.txt"));

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("CachedDirectoryCanRefreshItsFileIndex")
    {
        string temp_dir {MakeTempMountedDir("filesystem_refresh")};
        bool removed_before {fs::remove_dir_tree(temp_dir)};
        ignore_unused(removed_before);

        REQUIRE(fs::write_file(strex(temp_dir).combine_path("first.txt").str(), string_view {"first"}));

        FileSystem fs {};
        fs.AddDirSource(temp_dir, true);

        REQUIRE(fs.IsFileExists("first.txt"));
        REQUIRE_FALSE(fs.IsFileExists("second.txt"));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("second.txt").str(), string_view {"second"}));
        REQUIRE_FALSE(fs.IsFileExists("second.txt"));

        CHECK(fs.ReindexDataSources());

        CHECK(fs.IsFileExists("second.txt"));
        CHECK(fs.ReadFileText("second.txt") == "second");

        CHECK_FALSE(fs.ReindexDataSources());
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("first.txt").str(), string_view {"first updated"}));
        CHECK(fs.ReindexDataSources());
        CHECK(fs.ReadFileText("first.txt") == "first updated");

        REQUIRE(fs::remove_file(strex(temp_dir).combine_path("second.txt").str()));
        CHECK(fs.ReindexDataSources());
        CHECK_FALSE(fs.IsFileExists("second.txt"));
        CHECK_FALSE(fs.ReindexDataSources());
        CHECK(fs::remove_dir_tree(temp_dir));
    }

    // The explicit mode decides which artifact answers: a directory and an archive of one logical pack
    // may sit side by side, so neither layout may be selected by what the host binary happens to be
    SECTION("PackSourceModeSelectsDirectoryOrArchive")
    {
        string temp_dir {MakeTempMountedDir("filesystem_pack_mode")};
        bool removed_before {fs::remove_dir_tree(temp_dir)};
        ignore_unused(removed_before);

        auto cleanup {scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); })};

        REQUIRE(fs::write_file(strex(temp_dir).combine_path("MountPack/layout.txt").str(), string_view {"directory-layout"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("MountPack/directory-only.txt").str(), string_view {"directory-only"}));

        {
            string layout_payload {"archive-layout"};
            string archive_only_payload {"archive-only"};
            ResourcePackWriter writer {strex(temp_dir).combine_path("MountPack.fores").str()};
            writer.AddFile("layout.txt", make_const_span(layout_payload));
            writer.AddFile("archive-only.txt", make_const_span(archive_only_payload));
            writer.Finish();
        }

        FileSystem directory_resources {};
        directory_resources.AddPackSource(false, temp_dir, "MountPack");

        CHECK(directory_resources.ReadFileText("layout.txt") == "directory-layout");
        CHECK(directory_resources.ReadFileText("directory-only.txt") == "directory-only");
        CHECK_FALSE(directory_resources.IsFileExists("archive-only.txt"));

        FileHeader directory_header {directory_resources.ReadFileHeader("layout.txt")};
        REQUIRE(directory_header);
        CHECK(directory_header.GetDataSource()->IsDiskDir());

        FileSystem archive_resources {};
        archive_resources.AddPackSource(true, temp_dir, "MountPack");

        CHECK(archive_resources.ReadFileText("layout.txt") == "archive-layout");
        CHECK(archive_resources.ReadFileText("archive-only.txt") == "archive-only");
        CHECK_FALSE(archive_resources.IsFileExists("directory-only.txt"));

        FileHeader archive_header {archive_resources.ReadFileHeader("layout.txt")};
        REQUIRE(archive_header);
        CHECK_FALSE(archive_header.GetDataSource()->IsDiskDir());
    }

    // The bulk mount forwards the mode to every pack and keeps precedence: a pack added later sits in
    // front of the earlier ones, so it owns the paths both packs hold
    SECTION("AddPacksSourceForwardsModeAndLaterPackWins")
    {
        string temp_dir {MakeTempMountedDir("filesystem_packs_mode")};
        bool removed_before {fs::remove_dir_tree(temp_dir)};
        ignore_unused(removed_before);

        auto cleanup {scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); })};

        REQUIRE(fs::write_file(strex(temp_dir).combine_path("FirstPack/shared.txt").str(), string_view {"directory-first"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("FirstPack/first-only.txt").str(), string_view {"directory-first-only"}));
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("SecondPack/shared.txt").str(), string_view {"directory-second"}));

        {
            string first_payload {"archive-first"};
            string first_only_payload {"archive-first-only"};
            ResourcePackWriter writer {strex(temp_dir).combine_path("ArchiveFirst.fores").str()};
            writer.AddFile("shared.txt", make_const_span(first_payload));
            writer.AddFile("first-only.txt", make_const_span(first_only_payload));
            writer.Finish();
        }
        {
            string second_payload {"archive-second"};
            ResourcePackWriter writer {strex(temp_dir).combine_path("ArchiveSecond.fores").str()};
            writer.AddFile("shared.txt", make_const_span(second_payload));
            writer.Finish();
        }

        FileSystem directory_resources {};
        vector<string> directory_packs {"FirstPack", "SecondPack"};
        directory_resources.AddPacksSource(false, temp_dir, directory_packs);

        CHECK(directory_resources.ReadFileText("shared.txt") == "directory-second");
        CHECK(directory_resources.ReadFileText("first-only.txt") == "directory-first-only");

        FileSystem archive_resources {};
        vector<string> archive_packs {"ArchiveFirst", "ArchiveSecond"};
        archive_resources.AddPacksSource(true, temp_dir, archive_packs);

        CHECK(archive_resources.ReadFileText("shared.txt") == "archive-second");
        CHECK(archive_resources.ReadFileText("first-only.txt") == "archive-first-only");
    }

    // A selected layout without its own source fails instead of taking the other layout that is present;
    // the optional form of the same miss yields an empty source
    SECTION("PackSourceRejectsWrongLayoutAndOptionalMissingStaysEmpty")
    {
        string temp_dir {MakeTempMountedDir("filesystem_pack_layout")};
        bool removed_before {fs::remove_dir_tree(temp_dir)};
        ignore_unused(removed_before);

        auto cleanup {scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); })};

        REQUIRE(fs::write_file(strex(temp_dir).combine_path("DirectoryOnly/payload.txt").str(), string_view {"directory-without-archive"}));

        {
            string archive_payload {"archive-without-directory"};
            ResourcePackWriter writer {strex(temp_dir).combine_path("ArchiveOnly.fores").str()};
            writer.AddFile("payload.txt", make_const_span(archive_payload));
            writer.Finish();
        }

        FileSystem directory_required {};
        CHECK_THROWS_AS(directory_required.AddPackSource(false, temp_dir, "ArchiveOnly"), DataSourceException);
        CHECK(directory_required.GetAllFiles().GetFilesCount() == 0);

        FileSystem archive_required {};
        CHECK_THROWS_AS(archive_required.AddPackSource(true, temp_dir, "DirectoryOnly"), DataSourceException);
        CHECK(archive_required.GetAllFiles().GetFilesCount() == 0);

        FileSystem absent_required {};
        CHECK_THROWS_AS(absent_required.AddPackSource(false, temp_dir, "MissingPack"), DataSourceException);
        CHECK_THROWS_AS(absent_required.AddPackSource(true, temp_dir, "MissingPack"), DataSourceException);
        CHECK(absent_required.GetAllFiles().GetFilesCount() == 0);

        FileSystem directory_optional {};
        CHECK_NOTHROW(directory_optional.AddPackSource(false, temp_dir, "ArchiveOnly", true));
        CHECK_FALSE(directory_optional.IsFileExists("payload.txt"));
        CHECK(directory_optional.GetAllFiles().GetFilesCount() == 0);

        FileSystem archive_optional {};
        CHECK_NOTHROW(archive_optional.AddPackSource(true, temp_dir, "DirectoryOnly", true));
        CHECK_FALSE(archive_optional.IsFileExists("payload.txt"));
        CHECK(archive_optional.GetAllFiles().GetFilesCount() == 0);
    }
}

FO_END_NAMESPACE
