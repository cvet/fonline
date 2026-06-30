#include "catch_amalgamated.hpp"

#include "Compressor.h"
#include "DataSource.h"
#include "DiskFileSystem.h"

FO_BEGIN_NAMESPACE

static auto MakeTempDataSourceDir(string_view name) -> string
{
    const auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_string(base);
}

static auto CalcZipCrc32(string_view data) noexcept -> uint32_t
{
    uint32_t crc = 0xFFFFFFFF;

    for (const char ch : data) {
        crc ^= numeric_cast<uint8_t>(ch);

        for (size_t bit = 0; bit != 8; bit++) {
            crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xEDB88320 : 0);
        }
    }

    return crc ^ 0xFFFFFFFF;
}

static void AppendLe16(string& output, uint16_t value)
{
    const auto byte0 = numeric_cast<uint8_t>(value & 0x00FF);
    const auto byte1 = numeric_cast<uint8_t>((value >> 8) & 0x00FF);

    output.append(reinterpret_cast<const char*>(&byte0), sizeof(byte0));
    output.append(reinterpret_cast<const char*>(&byte1), sizeof(byte1));
}

static void AppendLe32(string& output, uint32_t value)
{
    const auto byte0 = numeric_cast<uint8_t>(value & 0x000000FF);
    const auto byte1 = numeric_cast<uint8_t>((value >> 8) & 0x000000FF);
    const auto byte2 = numeric_cast<uint8_t>((value >> 16) & 0x000000FF);
    const auto byte3 = numeric_cast<uint8_t>((value >> 24) & 0x000000FF);

    output.append(reinterpret_cast<const char*>(&byte0), sizeof(byte0));
    output.append(reinterpret_cast<const char*>(&byte1), sizeof(byte1));
    output.append(reinterpret_cast<const char*>(&byte2), sizeof(byte2));
    output.append(reinterpret_cast<const char*>(&byte3), sizeof(byte3));
}

static auto MakeStoredZip(string_view file_name, string_view file_content) -> string
{
    string zip;
    const auto name_size = numeric_cast<uint16_t>(file_name.size());
    const auto content_size = numeric_cast<uint32_t>(file_content.size());
    const auto crc = CalcZipCrc32(file_content);

    AppendLe32(zip, 0x04034B50);
    AppendLe16(zip, 20);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe32(zip, crc);
    AppendLe32(zip, content_size);
    AppendLe32(zip, content_size);
    AppendLe16(zip, name_size);
    AppendLe16(zip, 0);
    zip.append(file_name);
    zip.append(file_content);

    const auto central_dir_offset = numeric_cast<uint32_t>(zip.size());

    AppendLe32(zip, 0x02014B50);
    AppendLe16(zip, 20);
    AppendLe16(zip, 20);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe32(zip, crc);
    AppendLe32(zip, content_size);
    AppendLe32(zip, content_size);
    AppendLe16(zip, name_size);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe32(zip, 0);
    AppendLe32(zip, 0);
    zip.append(file_name);

    const auto central_dir_size = numeric_cast<uint32_t>(zip.size() - central_dir_offset);

    AppendLe32(zip, 0x06054B50);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 1);
    AppendLe16(zip, 1);
    AppendLe32(zip, central_dir_size);
    AppendLe32(zip, central_dir_offset);
    AppendLe16(zip, 0);

    return zip;
}

static auto MakeEmptyZip() -> string
{
    string zip;

    AppendLe32(zip, 0x06054B50);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe32(zip, 0);
    AppendLe32(zip, 0);
    AppendLe16(zip, 0);

    return zip;
}

static auto MakeFallout2DatEntry(string_view file_name, string_view file_payload, uint8_t type, uint32_t real_size, uint32_t packed_size, uint32_t offset) -> string
{
    string dat {file_payload};
    string tree;
    const auto name_size = numeric_cast<uint32_t>(file_name.size());

    AppendLe32(tree, 1);
    AppendLe32(tree, name_size);
    tree.append(file_name);
    tree.append(reinterpret_cast<const char*>(&type), sizeof(type));
    AppendLe32(tree, real_size);
    AppendLe32(tree, packed_size);
    AppendLe32(tree, offset);

    dat.append(tree);

    const auto tree_size = numeric_cast<uint32_t>(tree.size());
    const auto dat_size = numeric_cast<uint32_t>(dat.size() + 8);

    AppendLe32(dat, tree_size);
    AppendLe32(dat, dat_size);

    return dat;
}

static auto MakeFallout2Dat(string_view file_name, string_view file_content) -> string
{
    const auto content_size = numeric_cast<uint32_t>(file_content.size());

    return MakeFallout2DatEntry(file_name, file_content, 0, content_size, content_size, 0);
}

static auto MakeFallout2DatWithInvalidNameSize() -> string
{
    string dat;
    string tree;

    AppendLe32(tree, 1);
    AppendLe32(tree, 4096);

    dat.append(tree);

    const auto tree_size = numeric_cast<uint32_t>(tree.size());
    const auto dat_size = numeric_cast<uint32_t>(dat.size() + 8);

    AppendLe32(dat, tree_size);
    AppendLe32(dat, dat_size);

    return dat;
}

static auto MakeFallout1LikeDat() -> string
{
    string dat;

    AppendLe32(dat, 0x01000000);
    AppendLe32(dat, 0);
    AppendLe32(dat, 12);

    return dat;
}

static auto MakeArcanumDat(string_view file_name, string_view file_content) -> string
{
    vector<uint8_t> plain_content;
    plain_content.reserve(file_content.size());

    for (const char ch : file_content) {
        plain_content.emplace_back(numeric_cast<uint8_t>(ch));
    }

    const auto packed_content = Compressor::Compress(plain_content);
    string dat {reinterpret_cast<const char*>(packed_content.data()), packed_content.size()};
    string tree;
    const auto name_size = numeric_cast<uint32_t>(file_name.size());
    const auto real_size = numeric_cast<uint32_t>(file_content.size());
    const auto packed_size = numeric_cast<uint32_t>(packed_content.size());

    AppendLe32(tree, 1);
    AppendLe32(tree, name_size);
    tree.append(file_name);
    AppendLe32(tree, 0);
    AppendLe32(tree, 2);
    AppendLe32(tree, real_size);
    AppendLe32(tree, packed_size);
    AppendLe32(tree, 0);

    const auto tree_size = numeric_cast<uint32_t>(tree.size() + 28);
    string info_block;
    AppendLe32(info_block, 0);
    AppendLe32(info_block, 0);
    AppendLe32(info_block, 0);
    AppendLe32(info_block, 0);
    AppendLe32(info_block, 0x44415431);
    AppendLe32(info_block, 0);
    AppendLe32(info_block, tree_size);

    dat.append(tree);
    dat.append(info_block);

    return dat;
}

TEST_CASE("DataSource")
{
    SECTION("MountDirSupportsRecursiveAndNonRecursiveAccess")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_mount");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        REQUIRE(fs_write_file(strex(temp_dir).combine_path("root.txt").str(), string_view {"root"}));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("nested/child.txt").str(), string_view {"child"}));

        const auto non_recursive = DataSource::MountDir(temp_dir, false, true, false);
        const auto recursive = DataSource::MountDir(temp_dir, true, false, false);

        REQUIRE(non_recursive);
        REQUIRE(recursive);

        CHECK(non_recursive->IsDiskDir());
        CHECK(recursive->IsDiskDir());
        CHECK(non_recursive->IsFileExists("root.txt"));
        CHECK_FALSE(non_recursive->IsFileExists("nested"));
        CHECK_FALSE(non_recursive->IsFileExists("nested/child.txt"));
        CHECK_FALSE(non_recursive->IsFileExists("missing.txt"));
        CHECK(recursive->IsFileExists("nested/child.txt"));

        size_t size = 0;
        uint64_t write_time = 0;
        CHECK_FALSE(non_recursive->GetFileInfo("nested/child.txt", size, write_time));
        CHECK_FALSE(non_recursive->GetFileInfo("missing.txt", size, write_time));
        CHECK_FALSE(non_recursive->OpenFile("nested/child.txt", size, write_time));
        CHECK_FALSE(non_recursive->OpenFile("missing.txt", size, write_time));
        CHECK(non_recursive->GetFileNames("nested", false, "txt").empty());

        CHECK(recursive->GetFileInfo("nested/child.txt", size, write_time));
        CHECK(size == 5);
        CHECK(write_time != 0);
        CHECK_FALSE(recursive->GetFileInfo("missing.txt", size, write_time));
        CHECK_FALSE(recursive->OpenFile("missing.txt", size, write_time));

        const auto buf = recursive->OpenFile("nested/child.txt", size, write_time);
        REQUIRE(buf);
        CHECK(string_view {reinterpret_cast<const char*>(buf.get()), size} == "child");

        const auto non_recursive_names = non_recursive->GetFileNames("", false, "txt");
        REQUIRE(non_recursive_names.size() == 1);
        CHECK(non_recursive_names[0] == "root.txt");

        const auto recursive_names = recursive->GetFileNames("nested", true, "txt");
        REQUIRE(recursive_names.size() == 1);
        CHECK(recursive_names[0] == "nested/child.txt");

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("CachedDirHandlesMissingEntries")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_cached_missing");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        REQUIRE(fs_write_file(strex(temp_dir).combine_path("root.txt").str(), string_view {"root"}));
        REQUIRE(fs_create_directories(strex(temp_dir).combine_path("nested").str()));

        const auto cached = DataSource::MountDir(temp_dir, false, false, false);
        REQUIRE(cached);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK(cached->IsDiskDir());
        CHECK(cached->IsFileExists("root.txt"));
        CHECK_FALSE(cached->IsFileExists("nested"));
        CHECK_FALSE(cached->IsFileExists("missing.txt"));
        CHECK_FALSE(cached->GetFileInfo("missing.txt", size, write_time));
        CHECK_FALSE(cached->OpenFile("missing.txt", size, write_time));

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("CachedDirHandlesStaleEntriesAtOpenTime")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_cached_stale");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        const string removed_path = strex(temp_dir).combine_path("removed.txt").str();
        const string truncated_path = strex(temp_dir).combine_path("truncated.txt").str();
        REQUIRE(fs_write_file(removed_path, string_view {"removed-data"}));
        REQUIRE(fs_write_file(truncated_path, string_view {"truncated-data"}));

        const auto cached = DataSource::MountDir(temp_dir, false, false, false);
        REQUIRE(cached);

        CHECK(cached->IsFileExists("removed.txt"));
        CHECK(cached->IsFileExists("truncated.txt"));

        CHECK(fs_remove_file(removed_path));
        REQUIRE(fs_write_file(truncated_path, string_view {"tiny"}));

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_THROWS_AS(cached->OpenFile("removed.txt", size, write_time), DataSourceException);
        CHECK_FALSE(cached->OpenFile("truncated.txt", size, write_time));

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("DataSourceRefDelegatesToWrappedSource")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_ref");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        REQUIRE(fs_write_file(strex(temp_dir).combine_path("entry.bin").str(), string_view {"abc"}));

        const auto mounted = DataSource::MountDir(temp_dir, false, false, false);
        REQUIRE(mounted);

        const DataSourceRef ds_ref {mounted.get()};
        size_t size = 0;
        uint64_t write_time = 0;

        CHECK(ds_ref.IsDiskDir());
        CHECK(ds_ref.IsFileExists("entry.bin"));
        CHECK(ds_ref.GetFileInfo("entry.bin", size, write_time));
        CHECK(size == 3);
        CHECK(write_time != 0);
        CHECK(ds_ref.GetPackName() == mounted->GetPackName());

        const auto buf = ds_ref.OpenFile("entry.bin", size, write_time);
        REQUIRE(buf);
        CHECK(string_view {reinterpret_cast<const char*>(buf.get()), size} == "abc");

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("ZipPackLoadsStoredEntries")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_zip_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        const string zip_path = strex(temp_dir).combine_path("Archive.zip").str();
        REQUIRE(fs_create_directories(temp_dir));
        REQUIRE(fs_write_file(zip_path, MakeStoredZip("nested\\entry.txt", "zip-data")));

        auto zip_pack = DataSource::MountPack(temp_dir, "Archive", false);
        REQUIRE(zip_pack);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_FALSE(zip_pack->IsDiskDir());
        CHECK(zip_pack->GetPackName() == zip_path);
        CHECK(zip_pack->IsFileExists("nested/entry.txt"));
        CHECK_FALSE(zip_pack->IsFileExists("missing.txt"));
        CHECK(zip_pack->GetFileInfo("nested/entry.txt", size, write_time));
        CHECK(size == 8);
        CHECK(write_time != 0);
        CHECK_FALSE(zip_pack->GetFileInfo("missing.txt", size, write_time));

        const auto root_names = zip_pack->GetFileNames("", false, "txt");
        CHECK(root_names.empty());

        const auto nested_names = zip_pack->GetFileNames("nested", false, "txt");
        REQUIRE(nested_names.size() == 1);
        CHECK(nested_names.front() == "nested/entry.txt");

        const auto normalized_names = zip_pack->GetFileNames("nested\\", true, "");
        REQUIRE(normalized_names.size() == 1);
        CHECK(normalized_names.front() == "nested/entry.txt");

        const auto buf = zip_pack->OpenFile("nested/entry.txt", size, write_time);
        REQUIRE(buf);
        CHECK(string_view {reinterpret_cast<const char*>(buf.get()), size} == "zip-data");
        CHECK_FALSE(zip_pack->OpenFile("missing.txt", size, write_time));

        zip_pack.reset();
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("BosPackUsesZipReader")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_bos_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        const string bos_path = strex(temp_dir).combine_path("BosPack.bos").str();
        REQUIRE(fs_create_directories(temp_dir));
        REQUIRE(fs_write_file(bos_path, MakeStoredZip("entry.bin", "bos-data")));

        auto bos_pack = DataSource::MountPack(temp_dir, "BosPack", false);
        REQUIRE(bos_pack);

        size_t size = 0;
        uint64_t write_time = 0;
        const auto buf = bos_pack->OpenFile("entry.bin", size, write_time);

        REQUIRE(buf);
        CHECK(size == 8);
        CHECK(write_time != 0);
        CHECK(string_view {reinterpret_cast<const char*>(buf.get()), size} == "bos-data");

        bos_pack.reset();
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("DatPackLoadsPlainEntries")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_dat_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        const string dat_path = strex(temp_dir).combine_path("FalloutPack.dat").str();
        REQUIRE(fs_create_directories(temp_dir));
        REQUIRE(fs_write_file(dat_path, MakeFallout2Dat("nested\\entry.txt", "dat-data")));

        auto dat_pack = DataSource::MountPack(temp_dir, "FalloutPack", false);
        REQUIRE(dat_pack);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_FALSE(dat_pack->IsDiskDir());
        CHECK(dat_pack->GetPackName() == dat_path);
        CHECK(dat_pack->IsFileExists("nested/entry.txt"));
        CHECK_FALSE(dat_pack->IsFileExists("missing.txt"));
        CHECK(dat_pack->GetFileInfo("nested/entry.txt", size, write_time));
        CHECK(size == 8);
        CHECK(write_time != 0);

        const auto nested_names = dat_pack->GetFileNames("nested", false, "txt");
        REQUIRE(nested_names.size() == 1);
        CHECK(nested_names.front() == "nested/entry.txt");

        const auto buf = dat_pack->OpenFile("nested/entry.txt", size, write_time);
        REQUIRE(buf);
        CHECK(string_view {reinterpret_cast<const char*>(buf.get()), size} == "dat-data");
        CHECK_FALSE(dat_pack->OpenFile("missing.txt", size, write_time));

        dat_pack.reset();
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("DatPackEntryReadErrorsThrow")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_dat_read_errors");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        REQUIRE(fs_create_directories(temp_dir));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("TruncatedPlain.dat").str(), MakeFallout2DatEntry("plain.txt", "short", 0, 4096, 4096, 0)));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("InvalidPacked.dat").str(), MakeFallout2DatEntry("packed.txt", "not-deflated", 1, 32, 12, 0)));

        auto plain_pack = DataSource::MountPack(temp_dir, "TruncatedPlain", false);
        auto packed_pack = DataSource::MountPack(temp_dir, "InvalidPacked", false);
        REQUIRE(plain_pack);
        REQUIRE(packed_pack);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK(plain_pack->IsFileExists("plain.txt"));
        CHECK(packed_pack->IsFileExists("packed.txt"));
        CHECK_THROWS_AS(plain_pack->OpenFile("plain.txt", size, write_time), DataSourceException);
        CHECK_THROWS_AS(packed_pack->OpenFile("packed.txt", size, write_time), DataSourceException);

        plain_pack.reset();
        packed_pack.reset();
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("DatPackTreeEdgeCases")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_dat_tree_edges");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        REQUIRE(fs_create_directories(temp_dir));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("EmptyName.dat").str(), MakeFallout2DatEntry("", "", 0, 0, 0, 0)));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("BadNameSize.dat").str(), MakeFallout2DatWithInvalidNameSize()));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("Fallout1.dat").str(), MakeFallout1LikeDat()));

        auto empty_name_pack = DataSource::MountPack(temp_dir, "EmptyName", false);
        REQUIRE(empty_name_pack);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_FALSE(empty_name_pack->IsFileExists(""));
        CHECK_FALSE(empty_name_pack->GetFileInfo("", size, write_time));
        CHECK_FALSE(empty_name_pack->OpenFile("", size, write_time));
        CHECK(empty_name_pack->GetFileNames("", true, "").empty());

        CHECK_THROWS_AS(DataSource::MountPack(temp_dir, "BadNameSize", false), DataSourceException);
        CHECK_THROWS_AS(DataSource::MountPack(temp_dir, "Fallout1", false), DataSourceException);

        empty_name_pack.reset();
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("ArcanumDatLoadsCompressedEntries")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_arcanum_dat_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        constexpr string_view content = "compressed-dat-data";
        const string dat_path = strex(temp_dir).combine_path("ArcanumPack.dat").str();
        REQUIRE(fs_create_directories(temp_dir));
        REQUIRE(fs_write_file(dat_path, MakeArcanumDat("deep\\packed.txt", content)));

        auto dat_pack = DataSource::MountPack(temp_dir, "ArcanumPack", false);
        REQUIRE(dat_pack);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK(dat_pack->IsFileExists("deep/packed.txt"));
        CHECK(dat_pack->GetFileInfo("deep/packed.txt", size, write_time));
        CHECK(size == content.size());
        CHECK(write_time != 0);

        const auto nested_names = dat_pack->GetFileNames("deep", true, "txt");
        REQUIRE(nested_names.size() == 1);
        CHECK(nested_names.front() == "deep/packed.txt");

        const auto buf = dat_pack->OpenFile("deep/packed.txt", size, write_time);
        REQUIRE(buf);
        CHECK(string_view {reinterpret_cast<const char*>(buf.get()), size} == content);

        dat_pack.reset();
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("MalformedPacksThrow")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_malformed_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        REQUIRE(fs_create_directories(temp_dir));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("BrokenZip.zip").str(), MakeEmptyZip()));
        REQUIRE(fs_write_file(strex(temp_dir).combine_path("BrokenDat.dat").str(), string_view {"bad"}));

        CHECK_THROWS_AS(DataSource::MountPack(temp_dir, "BrokenZip", false), DataSourceException);
        CHECK_THROWS_AS(DataSource::MountPack(temp_dir, "BrokenDat", false), DataSourceException);

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("EmbeddedPackAcceptsDefaultResourceArray")
    {
        auto embedded = DataSource::MountPack("", "Embedded", false);
        REQUIRE(embedded);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_FALSE(embedded->IsDiskDir());
        CHECK(embedded->GetPackName() == "Embedded");
        CHECK_FALSE(embedded->IsFileExists("missing.txt"));
        CHECK_FALSE(embedded->GetFileInfo("missing.txt", size, write_time));
        CHECK_FALSE(embedded->OpenFile("missing.txt", size, write_time));
        CHECK(embedded->GetFileNames("", true, "txt").empty());
    }

    SECTION("MaybeNotAvailableReturnsDummySources")
    {
        const auto maybe_dir = DataSource::MountDir("/tmp/lf_data_source_missing_dir", false, false, true);
        auto maybe_pack = DataSource::MountPack("/tmp/lf_data_source_missing_pack", "MissingPack", true);

        REQUIRE(maybe_dir);
        REQUIRE(maybe_pack);

        size_t size = 123;
        uint64_t write_time = 456;

        CHECK_FALSE(maybe_dir->IsDiskDir());
        CHECK(maybe_dir->GetPackName() == "Dummy");
        CHECK_FALSE(maybe_dir->IsFileExists("anything"));
        CHECK_FALSE(maybe_dir->GetFileInfo("anything", size, write_time));
        CHECK_FALSE(maybe_dir->OpenFile("anything", size, write_time));
        CHECK(maybe_dir->GetFileNames("", true, "txt").empty());

        CHECK(maybe_pack->GetPackName() == "Dummy");
        CHECK_FALSE(maybe_pack->IsFileExists("anything"));
    }

    SECTION("FilesListPackLoadsEntriesFromManifest")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_files_list");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        const string listed_file = strex(temp_dir).combine_path("listed.txt").str();
        const string nested_file = strex(temp_dir).combine_path("nested/value.bin").str();
        const string manifest_path = "FilesTree.txt";
        CHECK_FALSE(fs_exists(manifest_path));

        REQUIRE(fs_write_file(listed_file, string_view {"listed-data"}));
        REQUIRE(fs_write_file(nested_file, string_view {"nested-data"}));
        REQUIRE(fs_write_file(manifest_path, strex("{}\n{}\n", listed_file, nested_file).str()));

        auto files_list = DataSource::MountPack("ignored", "FilesList", false);
        REQUIRE(files_list);

        size_t size = 0;
        uint64_t write_time = 0;
        CHECK(files_list->GetPackName() == "@FilesList");
        CHECK(files_list->IsFileExists(listed_file));
        CHECK(files_list->GetFileInfo(listed_file, size, write_time));
        CHECK(size == 11);
        CHECK(write_time != 0);

        const auto buf = files_list->OpenFile(listed_file, size, write_time);
        REQUIRE(buf);
        CHECK(string_view {reinterpret_cast<const char*>(buf.get()), size} == "listed-data");

        const auto filtered = files_list->GetFileNames(temp_dir, true, "bin");
        REQUIRE(filtered.size() == 1);
        CHECK(filtered[0] == nested_file);

        CHECK(fs_remove_file(manifest_path));
        files_list.reset();
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("FilesListPackRejectsMissingManifestAndEntries")
    {
        const string temp_dir = MakeTempDataSourceDir("data_source_files_list_errors");
        const string manifest_path = "FilesTree.txt";
        const bool removed_manifest_before = fs_remove_file(manifest_path);
        const bool removed_dir_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_manifest_before, removed_dir_before);

        CHECK_THROWS_AS(DataSource::MountPack("ignored", "FilesList", false), DataSourceException);

        REQUIRE(fs_create_directories(temp_dir));
        REQUIRE(fs_write_file(manifest_path, strex("{}\n", strex(temp_dir).combine_path("missing.txt").str()).str()));

        CHECK_THROWS_AS(DataSource::MountPack("ignored", "FilesList", false), DataSourceException);

        CHECK(fs_remove_file(manifest_path));
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("MissingMandatorySourcesThrow")
    {
        CHECK_THROWS_AS(DataSource::MountDir("/tmp/lf_data_source_missing_dir_required", false, false, false), DataSourceException);
        CHECK_THROWS_AS(DataSource::MountPack("/tmp/lf_data_source_missing_pack_required", "MissingPack", false), DataSourceException);
        CHECK_THROWS(DataSource::MountPack("/tmp/lf_data_source_missing_pack_required", "", false));
    }
}

FO_END_NAMESPACE
