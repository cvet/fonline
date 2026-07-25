#include "catch_amalgamated.hpp"

#include "Compressor.h"
#include "DataSource.h"
#include "DiskFileSystem.h"

FO_BEGIN_NAMESPACE

static auto MakeTempDataSourceDir(string_view name) -> u8string
{
    const auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_u8string(base);
}

static auto WriteBinaryFixture(u8string_view path, vector<byte> content) -> bool
{
    return fs_write_file_bytes(path, content);
}

static auto WriteBinaryFixtureInDir(u8string_view dir, string_view relative_path, vector<byte> content) -> bool
{
    const u8string path = fs_combine_path(dir, relative_path);
    return WriteBinaryFixture(path.view(), std::move(content));
}

static auto MakeRawFixture(string_view content) -> vector<byte>
{
    const const_span<byte> bytes = make_byte_span(content);
    return vector<byte> {bytes.begin(), bytes.end()};
}

static auto WriteTextFixture(u8string_view path, u8string_view content) -> bool
{
    return fs_write_file_text(path, content);
}

static auto CalcZipCrc32(const_span<byte> data) noexcept -> uint32_t
{
    uint32_t crc = 0xFFFFFFFF;

    for (const byte value : data) {
        crc ^= std::to_integer<uint8_t>(value);

        for (size_t bit = 0; bit != 8; bit++) {
            crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xEDB88320 : 0);
        }
    }

    return crc ^ 0xFFFFFFFF;
}

static auto BufferAsString(const unique_del_nptr<const byte>& data, size_t size) noexcept -> string_view
{
    ptr<const byte> data_ptr = data;
    return data_ptr.reinterpret_as<char>().as_str(size);
}

static void AppendBytes(vector<byte>& output, const_span<byte> data)
{
    output.insert(output.end(), data.begin(), data.end());
}

static void AppendTextBytes(vector<byte>& output, string_view text)
{
    AppendBytes(output, make_byte_span(text));
}

static void AppendLe16(vector<byte>& output, uint16_t value)
{
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>(value & 0x00FF)));
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>((value >> 8) & 0x00FF)));
}

static void AppendLe32(vector<byte>& output, uint32_t value)
{
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>(value & 0x000000FF)));
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>((value >> 8) & 0x000000FF)));
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>((value >> 16) & 0x000000FF)));
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>((value >> 24) & 0x000000FF)));
}

struct StoredZipEntry
{
    string_view FileName {};
    string_view FileContent {};
    uint32_t ExternalAttributes {};
};

struct StoredZipCentralEntry
{
    string_view FileName {};
    string_view FileContent {};
    uint32_t Crc {};
    uint32_t LocalHeaderOffset {};
    uint32_t ExternalAttributes {};
};

static auto MakeStoredZip(std::initializer_list<StoredZipEntry> entries) -> vector<byte>
{
    vector<byte> zip;
    vector<StoredZipCentralEntry> central_entries;
    central_entries.reserve(entries.size());

    for (const auto& entry : entries) {
        const auto name_size = numeric_cast<uint16_t>(entry.FileName.size());
        const auto content_size = numeric_cast<uint32_t>(entry.FileContent.size());
        const auto crc = CalcZipCrc32(make_byte_span(entry.FileContent));
        const auto local_header_offset = numeric_cast<uint32_t>(zip.size());

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
        AppendTextBytes(zip, entry.FileName);
        AppendTextBytes(zip, entry.FileContent);

        central_entries.push_back(StoredZipCentralEntry {
            entry.FileName,
            entry.FileContent,
            crc,
            local_header_offset,
            entry.ExternalAttributes,
        });
    }

    const auto central_dir_offset = numeric_cast<uint32_t>(zip.size());

    for (const auto& entry : central_entries) {
        const auto name_size = numeric_cast<uint16_t>(entry.FileName.size());
        const auto content_size = numeric_cast<uint32_t>(entry.FileContent.size());

        AppendLe32(zip, 0x02014B50);
        AppendLe16(zip, 20);
        AppendLe16(zip, 20);
        AppendLe16(zip, 0);
        AppendLe16(zip, 0);
        AppendLe16(zip, 0);
        AppendLe16(zip, 0);
        AppendLe32(zip, entry.Crc);
        AppendLe32(zip, content_size);
        AppendLe32(zip, content_size);
        AppendLe16(zip, name_size);
        AppendLe16(zip, 0);
        AppendLe16(zip, 0);
        AppendLe16(zip, 0);
        AppendLe16(zip, 0);
        AppendLe32(zip, entry.ExternalAttributes);
        AppendLe32(zip, entry.LocalHeaderOffset);
        AppendTextBytes(zip, entry.FileName);
    }

    const auto central_dir_size = numeric_cast<uint32_t>(zip.size() - central_dir_offset);
    const auto entry_count = numeric_cast<uint16_t>(central_entries.size());

    AppendLe32(zip, 0x06054B50);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, entry_count);
    AppendLe16(zip, entry_count);
    AppendLe32(zip, central_dir_size);
    AppendLe32(zip, central_dir_offset);
    AppendLe16(zip, 0);

    return zip;
}

static auto MakeStoredZip(string_view file_name, string_view file_content) -> vector<byte>
{
    return MakeStoredZip({StoredZipEntry {file_name, file_content}});
}

static auto MakeStoredZipWithDeclaredSize(string_view file_name, string_view file_content, uint32_t declared_size) -> vector<byte>
{
    vector<byte> zip;
    const auto name_size = numeric_cast<uint16_t>(file_name.size());
    const auto crc = CalcZipCrc32(make_byte_span(file_content));

    AppendLe32(zip, 0x04034B50);
    AppendLe16(zip, 20);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe32(zip, crc);
    AppendLe32(zip, declared_size);
    AppendLe32(zip, declared_size);
    AppendLe16(zip, name_size);
    AppendLe16(zip, 0);
    AppendTextBytes(zip, file_name);
    AppendTextBytes(zip, file_content);

    const auto central_dir_offset = numeric_cast<uint32_t>(zip.size());

    AppendLe32(zip, 0x02014B50);
    AppendLe16(zip, 20);
    AppendLe16(zip, 20);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe32(zip, crc);
    AppendLe32(zip, declared_size);
    AppendLe32(zip, declared_size);
    AppendLe16(zip, name_size);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe16(zip, 0);
    AppendLe32(zip, 0);
    AppendLe32(zip, 0);
    AppendTextBytes(zip, file_name);

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

static auto MakeEmptyZip() -> vector<byte>
{
    vector<byte> zip;

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

static auto MakeFallout2DatEntry(string_view file_name, string_view file_payload, uint8_t type, uint32_t real_size, uint32_t packed_size, uint32_t offset) -> vector<byte>
{
    vector<byte> dat = MakeRawFixture(file_payload);
    vector<byte> tree;
    const auto name_size = numeric_cast<uint32_t>(file_name.size());

    AppendLe32(tree, 1);
    AppendLe32(tree, name_size);
    AppendTextBytes(tree, file_name);
    tree.emplace_back(static_cast<byte>(type));
    AppendLe32(tree, real_size);
    AppendLe32(tree, packed_size);
    AppendLe32(tree, offset);

    AppendBytes(dat, tree);

    const auto tree_size = numeric_cast<uint32_t>(tree.size());
    const auto dat_size = numeric_cast<uint32_t>(dat.size() + 8);

    AppendLe32(dat, tree_size);
    AppendLe32(dat, dat_size);

    return dat;
}

static auto MakeFallout2Dat(string_view file_name, string_view file_content) -> vector<byte>
{
    const auto content_size = numeric_cast<uint32_t>(file_content.size());

    return MakeFallout2DatEntry(file_name, file_content, 0, content_size, content_size, 0);
}

static auto MakeFallout2DatWithInvalidNameSize() -> vector<byte>
{
    vector<byte> dat;
    vector<byte> tree;

    AppendLe32(tree, 1);
    AppendLe32(tree, 4096);

    AppendBytes(dat, tree);

    const auto tree_size = numeric_cast<uint32_t>(tree.size());
    const auto dat_size = numeric_cast<uint32_t>(dat.size() + 8);

    AppendLe32(dat, tree_size);
    AppendLe32(dat, dat_size);

    return dat;
}

static auto MakeFallout2HeaderOnlyDat(uint32_t tree_size, uint32_t dat_size) -> vector<byte>
{
    vector<byte> dat;

    AppendLe32(dat, 0);
    AppendLe32(dat, tree_size);
    AppendLe32(dat, dat_size);

    return dat;
}

static auto MakeFallout1LikeDat() -> vector<byte>
{
    vector<byte> dat;

    AppendLe32(dat, 0x01000000);
    AppendLe32(dat, 0);
    AppendLe32(dat, 12);

    return dat;
}

static auto MakeArcanumMalformedDat(uint32_t tree_size) -> vector<byte>
{
    vector<byte> dat;

    AppendLe32(dat, 0x44415431);
    AppendLe32(dat, 0);
    AppendLe32(dat, tree_size);

    return dat;
}

static auto MakeArcanumDat(string_view file_name, string_view file_content) -> vector<byte>
{
    const auto packed_content = Compressor::Compress(make_byte_span(file_content));
    vector<byte> dat = packed_content;
    vector<byte> tree;
    const auto name_size = numeric_cast<uint32_t>(file_name.size());
    const auto real_size = numeric_cast<uint32_t>(file_content.size());
    const auto packed_size = numeric_cast<uint32_t>(packed_content.size());

    AppendLe32(tree, 1);
    AppendLe32(tree, name_size);
    AppendTextBytes(tree, file_name);
    AppendLe32(tree, 0);
    AppendLe32(tree, 2);
    AppendLe32(tree, real_size);
    AppendLe32(tree, packed_size);
    AppendLe32(tree, 0);

    const auto tree_size = numeric_cast<uint32_t>(tree.size() + 28);
    vector<byte> info_block;
    AppendLe32(info_block, 0);
    AppendLe32(info_block, 0);
    AppendLe32(info_block, 0);
    AppendLe32(info_block, 0);
    AppendLe32(info_block, 0x44415431);
    AppendLe32(info_block, 0);
    AppendLe32(info_block, tree_size);

    AppendBytes(dat, tree);
    AppendBytes(dat, info_block);

    return dat;
}

TEST_CASE("DataSource")
{
    SECTION("MountDirSupportsRecursiveAndNonRecursiveAccess")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_mount");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        const u8string root_path = fs_combine_path(temp_dir.view(), "root.txt");
        const u8string child_path = fs_combine_path(temp_dir.view(), "nested/child.txt");
        REQUIRE(WriteTextFixture(root_path, u8"root"));
        REQUIRE(WriteTextFixture(child_path, u8"child"));

        const auto non_recursive = DataSource::MountDir(temp_dir.view(), false, true, false);
        const auto recursive = DataSource::MountDir(temp_dir.view(), true, false, false);

        CHECK(non_recursive->IsDiskDir());
        CHECK_FALSE(non_recursive->GetPackName().empty());
        CHECK(recursive->IsDiskDir());
        CHECK(non_recursive->IsFileExists("root.txt"));
        CHECK_FALSE(non_recursive->IsFileExists("nested"));
        CHECK_FALSE(non_recursive->IsFileExists("nested/child.txt"));
        CHECK_FALSE(non_recursive->IsFileExists("missing.txt"));
        CHECK(recursive->IsFileExists("nested/child.txt"));

        size_t size = 0;
        uint64_t write_time = 0;
        CHECK(non_recursive->GetFileInfo("root.txt", size, write_time));
        CHECK(size == 4);
        CHECK(write_time != 0);
        CHECK_FALSE(non_recursive->GetFileInfo("nested/child.txt", size, write_time));
        CHECK_FALSE(non_recursive->GetFileInfo("missing.txt", size, write_time));

        const auto root_buf = non_recursive->OpenFile("root.txt", size, write_time);
        REQUIRE(root_buf);
        CHECK(BufferAsString(root_buf, size) == "root");
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
        CHECK(BufferAsString(buf, size) == "child");

        const auto non_recursive_names = non_recursive->GetFileNames("", false, "txt");
        REQUIRE(non_recursive_names.size() == 1);
        CHECK(non_recursive_names[0] == "root.txt");

        const auto recursive_names = recursive->GetFileNames("nested", true, "txt");
        REQUIRE(recursive_names.size() == 1);
        CHECK(recursive_names[0] == "nested/child.txt");

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("CachedDirHandlesMissingEntries")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_cached_missing");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        const u8string root_path = fs_combine_path(temp_dir.view(), "root.txt");
        const u8string nested_dir = fs_combine_path(temp_dir.view(), "nested");
        REQUIRE(WriteTextFixture(root_path, u8"root"));
        REQUIRE(fs_create_directories(nested_dir.view()));

        const auto cached = DataSource::MountDir(temp_dir.view(), false, false, false);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK(cached->IsDiskDir());
        CHECK(cached->IsFileExists("root.txt"));
        CHECK_FALSE(cached->IsFileExists("nested"));
        CHECK_FALSE(cached->IsFileExists("missing.txt"));
        CHECK_FALSE(cached->GetFileInfo("missing.txt", size, write_time));
        CHECK_FALSE(cached->OpenFile("missing.txt", size, write_time));

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("CachedDirHandlesStaleEntriesAtOpenTime")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_cached_stale");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        const u8string removed_path = fs_combine_path(temp_dir.view(), "removed.txt");
        const u8string truncated_path = fs_combine_path(temp_dir.view(), "truncated.txt");
        REQUIRE(WriteTextFixture(removed_path, u8"removed-data"));
        REQUIRE(WriteTextFixture(truncated_path, u8"truncated-data"));

        const auto cached = DataSource::MountDir(temp_dir.view(), false, false, false);

        CHECK(cached->IsFileExists("removed.txt"));
        CHECK(cached->IsFileExists("truncated.txt"));

        CHECK(fs_remove_file(removed_path.view()));
        REQUIRE(WriteTextFixture(truncated_path, u8"tiny"));

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_THROWS_AS(cached->OpenFile("removed.txt", size, write_time), DataSourceException);
        CHECK_FALSE(cached->OpenFile("truncated.txt", size, write_time));

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("DataSourceRefDelegatesToWrappedSource")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_ref");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        const u8string entry_path = fs_combine_path(temp_dir.view(), "entry.bin");
        REQUIRE(fs_write_file_bytes(entry_path.view(), string_to_byte_span("abc")));

        const auto mounted = DataSource::MountDir(temp_dir.view(), false, false, false);

        const DataSourceRef ds_ref {mounted};
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
        CHECK(BufferAsString(buf, size) == "abc");

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("ZipPackLoadsStoredEntries")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_zip_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        const u8string zip_path = fs_combine_path(temp_dir.view(), "Archive.zip");
        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(WriteBinaryFixture(zip_path.view(), MakeStoredZip("nested\\entry.txt", "zip-data")));

        const auto zip_pack = DataSource::MountPack(temp_dir.view(), u8"Archive", false);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_FALSE(zip_pack->IsDiskDir());
        CHECK(zip_pack->GetPackName() == zip_path.view());
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
        CHECK(BufferAsString(buf, size) == "zip-data");
        CHECK_FALSE(zip_pack->OpenFile("missing.txt", size, write_time));

        (void)fs_remove_dir_tree(temp_dir.view()); // best-effort: a mounted pack keeps the data file open until destroyed; Windows blocks deletion of open files
    }

    SECTION("ZipPackSkipsDirectoryEntriesAndFiltersMultipleFiles")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_zip_multi_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        const u8string zip_path = fs_combine_path(temp_dir.view(), "Multi.zip");
        REQUIRE(fs_create_directories(temp_dir.view()));
        const vector<byte> zip_content = MakeStoredZip({
            StoredZipEntry {"folder/", "", 0x10},
            StoredZipEntry {"folder/first.txt", "one"},
            StoredZipEntry {"folder/deeper/second.bin", "two"},
            StoredZipEntry {"root.txt", "root"},
        });
        REQUIRE(fs_write_file_bytes(zip_path.view(), zip_content));

        const auto zip_pack = DataSource::MountPack(temp_dir.view(), u8"Multi", false);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_FALSE(zip_pack->IsFileExists("folder/"));
        CHECK_FALSE(zip_pack->GetFileInfo("folder/", size, write_time));
        CHECK_FALSE(zip_pack->OpenFile("folder/", size, write_time));

        const auto root_names = zip_pack->GetFileNames("", false, "txt");
        REQUIRE(root_names.size() == 1);
        CHECK(root_names.front() == "root.txt");

        const auto folder_txt_names = zip_pack->GetFileNames("folder", false, "txt");
        REQUIRE(folder_txt_names.size() == 1);
        CHECK(folder_txt_names.front() == "folder/first.txt");

        const auto folder_recursive_names = zip_pack->GetFileNames("folder", true, "");
        REQUIRE(folder_recursive_names.size() == 2);
        CHECK(std::ranges::find(folder_recursive_names, "folder/first.txt") != folder_recursive_names.end());
        CHECK(std::ranges::find(folder_recursive_names, "folder/deeper/second.bin") != folder_recursive_names.end());
        CHECK(zip_pack->GetFileNames("", true, "dat").empty());

        const auto buf = zip_pack->OpenFile("folder/deeper/second.bin", size, write_time);
        REQUIRE(buf);
        CHECK(size == 3);
        CHECK(write_time != 0);
        CHECK(BufferAsString(buf, size) == "two");

        (void)fs_remove_dir_tree(temp_dir.view()); // best-effort: a mounted pack keeps the data file open until destroyed; Windows blocks deletion of open files
    }

    SECTION("BosPackUsesZipReader")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_bos_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        const u8string bos_path = fs_combine_path(temp_dir.view(), "BosPack.bos");
        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(WriteBinaryFixture(bos_path.view(), MakeStoredZip("entry.bin", "bos-data")));

        const auto bos_pack = DataSource::MountPack(temp_dir.view(), u8"BosPack", false);

        size_t size = 0;
        uint64_t write_time = 0;
        const auto buf = bos_pack->OpenFile("entry.bin", size, write_time);

        REQUIRE(buf);
        CHECK(size == 8);
        CHECK(write_time != 0);
        CHECK(BufferAsString(buf, size) == "bos-data");

        (void)fs_remove_dir_tree(temp_dir.view()); // best-effort: a mounted pack keeps the data file open until destroyed; Windows blocks deletion of open files
    }

    SECTION("DatPackLoadsPlainEntries")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_dat_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        const u8string dat_path = fs_combine_path(temp_dir.view(), "FalloutPack.dat");
        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(WriteBinaryFixture(dat_path.view(), MakeFallout2Dat("nested\\entry.txt", "dat-data")));

        const auto dat_pack = DataSource::MountPack(temp_dir.view(), u8"FalloutPack", false);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_FALSE(dat_pack->IsDiskDir());
        CHECK(dat_pack->GetPackName() == dat_path.view());
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
        CHECK(BufferAsString(buf, size) == "dat-data");
        CHECK_FALSE(dat_pack->OpenFile("missing.txt", size, write_time));

        (void)fs_remove_dir_tree(temp_dir.view()); // best-effort: a mounted pack keeps the data file open until destroyed; Windows blocks deletion of open files
    }

    SECTION("DatPackEntryReadErrorsThrow")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_dat_read_errors");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "TruncatedPlain.dat", MakeFallout2DatEntry("plain.txt", "short", 0, 4096, 4096, 0)));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "InvalidPacked.dat", MakeFallout2DatEntry("packed.txt", "not-deflated", 1, 32, 12, 0)));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "InvalidOffset.dat", MakeFallout2DatEntry("offset.txt", "payload", 0, 7, 7, 0xFFFFFFFF)));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "ShortPacked.dat", MakeFallout2DatEntry("short-packed.txt", "short", 1, 32, 4096, 0)));

        const auto plain_pack = DataSource::MountPack(temp_dir.view(), u8"TruncatedPlain", false);
        const auto packed_pack = DataSource::MountPack(temp_dir.view(), u8"InvalidPacked", false);
        const auto offset_pack = DataSource::MountPack(temp_dir.view(), u8"InvalidOffset", false);
        const auto short_packed_pack = DataSource::MountPack(temp_dir.view(), u8"ShortPacked", false);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK(plain_pack->IsFileExists("plain.txt"));
        CHECK(packed_pack->IsFileExists("packed.txt"));
        CHECK(offset_pack->IsFileExists("offset.txt"));
        CHECK(short_packed_pack->IsFileExists("short-packed.txt"));
        CHECK_THROWS_AS(plain_pack->OpenFile("plain.txt", size, write_time), DataSourceException);
        CHECK_THROWS_AS(packed_pack->OpenFile("packed.txt", size, write_time), DataSourceException);
        CHECK_THROWS_AS(offset_pack->OpenFile("offset.txt", size, write_time), DataSourceException);
        CHECK_THROWS_AS(short_packed_pack->OpenFile("short-packed.txt", size, write_time), DataSourceException);

        (void)fs_remove_dir_tree(temp_dir.view()); // best-effort: a mounted pack keeps the data file open until destroyed; Windows blocks deletion of open files
    }

    SECTION("DatPackTreeEdgeCases")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_dat_tree_edges");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "EmptyName.dat", MakeFallout2DatEntry("", "", 0, 0, 0, 0)));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "BadNameSize.dat", MakeFallout2DatWithInvalidNameSize()));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "Fallout1.dat", MakeFallout1LikeDat()));

        const auto empty_name_pack = DataSource::MountPack(temp_dir.view(), u8"EmptyName", false);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_FALSE(empty_name_pack->IsFileExists(""));
        CHECK_FALSE(empty_name_pack->GetFileInfo("", size, write_time));
        CHECK_FALSE(empty_name_pack->OpenFile("", size, write_time));
        CHECK(empty_name_pack->GetFileNames("", true, "").empty());

        CHECK_THROWS_AS(DataSource::MountPack(temp_dir.view(), u8"BadNameSize", false), DataSourceException);
        CHECK_THROWS_AS(DataSource::MountPack(temp_dir.view(), u8"Fallout1", false), DataSourceException);

        (void)fs_remove_dir_tree(temp_dir.view()); // best-effort: a mounted pack keeps the data file open until destroyed; Windows blocks deletion of open files
    }

    SECTION("DatPackRejectsMalformedTrees")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_dat_malformed_trees");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "FalloutWrongSize.dat", MakeFallout2HeaderOnlyDat(4, 4096)));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "FalloutTreeBeforeFile.dat", MakeFallout2HeaderOnlyDat(128, 12)));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "ArcanumTreeBeforeFile.dat", MakeArcanumMalformedDat(4096)));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "ArcanumMissingFileCount.dat", MakeArcanumMalformedDat(2)));

        CHECK_THROWS_AS(DataSource::MountPack(temp_dir.view(), u8"FalloutWrongSize", false), DataSourceException);
        CHECK_THROWS_AS(DataSource::MountPack(temp_dir.view(), u8"FalloutTreeBeforeFile", false), DataSourceException);
        CHECK_THROWS_AS(DataSource::MountPack(temp_dir.view(), u8"ArcanumTreeBeforeFile", false), DataSourceException);
        CHECK_THROWS_AS(DataSource::MountPack(temp_dir.view(), u8"ArcanumMissingFileCount", false), DataSourceException);

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("ArcanumDatLoadsCompressedEntries")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_arcanum_dat_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        constexpr string_view content = "compressed-dat-data";
        const u8string dat_path = fs_combine_path(temp_dir.view(), "ArcanumPack.dat");
        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(WriteBinaryFixture(dat_path.view(), MakeArcanumDat("deep\\packed.txt", content)));

        const auto dat_pack = DataSource::MountPack(temp_dir.view(), u8"ArcanumPack", false);

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
        CHECK(BufferAsString(buf, size) == content);

        (void)fs_remove_dir_tree(temp_dir.view()); // best-effort: a mounted pack keeps the data file open until destroyed; Windows blocks deletion of open files
    }

    SECTION("MalformedPacksThrow")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_malformed_pack");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "BrokenZip.zip", MakeEmptyZip()));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "InvalidZip.zip", MakeRawFixture("not a zip")));
        REQUIRE(WriteBinaryFixtureInDir(temp_dir.view(), "BrokenDat.dat", MakeRawFixture("bad")));

        CHECK_THROWS_AS(DataSource::MountPack(temp_dir.view(), u8"BrokenZip", false), DataSourceException);
        CHECK_THROWS(DataSource::MountPack(temp_dir.view(), u8"InvalidZip", false));
        CHECK_THROWS_AS(DataSource::MountPack(temp_dir.view(), u8"BrokenDat", false), DataSourceException);

        (void)fs_remove_dir_tree(temp_dir.view()); // best-effort: a mounted pack keeps the data file open until destroyed; Windows blocks deletion of open files
    }

    SECTION("ZipPackEntryReadErrorsThrow")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_zip_read_errors");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        const u8string zip_path = fs_combine_path(temp_dir.view(), "SizeMismatch.zip");
        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(WriteBinaryFixture(zip_path.view(), MakeStoredZipWithDeclaredSize("mismatch.txt", "tiny", 32)));

        const auto zip_pack = DataSource::MountPack(temp_dir.view(), u8"SizeMismatch", false);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK(zip_pack->IsFileExists("mismatch.txt"));
        CHECK(zip_pack->GetFileInfo("mismatch.txt", size, write_time));
        CHECK(size == 32);
        CHECK_THROWS_AS(zip_pack->OpenFile("mismatch.txt", size, write_time), DataSourceException);

        (void)fs_remove_dir_tree(temp_dir.view()); // best-effort: a mounted pack keeps the data file open until destroyed; Windows blocks deletion of open files
    }

    SECTION("EmbeddedPackAcceptsDefaultResourceArray")
    {
        const auto embedded = DataSource::MountPack(u8"", u8"Embedded", false);

        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_FALSE(embedded->IsDiskDir());
        CHECK(embedded->GetPackName() == u8"Embedded");
        CHECK_FALSE(embedded->IsFileExists("missing.txt"));
        CHECK_FALSE(embedded->GetFileInfo("missing.txt", size, write_time));
        CHECK_FALSE(embedded->OpenFile("missing.txt", size, write_time));
        CHECK(embedded->GetFileNames("", true, "txt").empty());
    }

    SECTION("MaybeNotAvailableReturnsDummySources")
    {
        const auto maybe_dir = DataSource::MountDir(u8"/tmp/lf_data_source_missing_dir", false, false, true);
        const auto maybe_pack = DataSource::MountPack(u8"/tmp/lf_data_source_missing_pack", u8"MissingPack", true);

        size_t size = 123;
        uint64_t write_time = 456;

        CHECK_FALSE(maybe_dir->IsDiskDir());
        CHECK(maybe_dir->GetPackName() == u8"Dummy");
        CHECK_FALSE(maybe_dir->IsFileExists("anything"));
        CHECK_FALSE(maybe_dir->GetFileInfo("anything", size, write_time));
        CHECK_FALSE(maybe_dir->OpenFile("anything", size, write_time));
        CHECK(maybe_dir->GetFileNames("", true, "txt").empty());

        CHECK(maybe_pack->GetPackName() == u8"Dummy");
        CHECK_FALSE(maybe_pack->IsFileExists("anything"));
    }

    SECTION("FilesListPackLoadsEntriesFromManifest")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_files_list");
        const bool removed_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before);

        const u8string listed_file_path = fs_combine_path(temp_dir.view(), "listed.txt");
        const u8string nested_file_path = fs_combine_path(temp_dir.view(), "nested/value.bin");
        const u8string shrinking_file_path = fs_combine_path(temp_dir.view(), "shrinking.txt");
        const u8string missing_file_path = fs_combine_path(temp_dir.view(), "missing.txt");
        const u8string manifest_path {u8"FilesTree.txt"};
        const string listed_file = utf8_to_char_string(listed_file_path.view());
        const string nested_file = utf8_to_char_string(nested_file_path.view());
        const string shrinking_file = utf8_to_char_string(shrinking_file_path.view());
        const string missing_file = utf8_to_char_string(missing_file_path.view());
        const string temp_dir_chars = utf8_to_char_string(temp_dir.view());
        CHECK_FALSE(fs_exists(manifest_path.view()));

        REQUIRE(WriteTextFixture(listed_file_path, u8"listed-data"));
        REQUIRE(fs_write_file_bytes(nested_file_path.view(), string_to_byte_span("nested-data")));
        REQUIRE(WriteTextFixture(shrinking_file_path, u8"shrinking-data"));
        REQUIRE(WriteTextFixture(manifest_path, u8strex("{}\n\n  \n{}\n{}\n", listed_file, nested_file, shrinking_file)));

        const auto files_list = DataSource::MountPack(u8"ignored", u8"FilesList", false);

        size_t size = 0;
        uint64_t write_time = 0;
        CHECK_FALSE(files_list->IsDiskDir());
        CHECK(files_list->GetPackName() == u8"@FilesList");
        CHECK(files_list->IsFileExists(listed_file));
        CHECK_FALSE(files_list->IsFileExists(missing_file));
        CHECK(files_list->GetFileInfo(listed_file, size, write_time));
        CHECK(size == 11);
        CHECK(write_time != 0);
        CHECK_FALSE(files_list->GetFileInfo(missing_file, size, write_time));

        const auto buf = files_list->OpenFile(listed_file, size, write_time);
        REQUIRE(buf);
        CHECK(BufferAsString(buf, size) == "listed-data");
        CHECK_FALSE(files_list->OpenFile(missing_file, size, write_time));

        const auto filtered = files_list->GetFileNames(temp_dir_chars, true, "bin");
        REQUIRE(filtered.size() == 1);
        CHECK(filtered[0] == nested_file);

        CHECK(fs_remove_file(listed_file_path.view()));
        CHECK_THROWS_AS(files_list->OpenFile(listed_file, size, write_time), DataSourceException);

        REQUIRE(WriteTextFixture(shrinking_file_path, u8"tiny"));
        CHECK_THROWS_AS(files_list->OpenFile(shrinking_file, size, write_time), DataSourceException);

        CHECK(fs_remove_file(manifest_path.view()));
        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("FilesListPackRejectsMissingManifestAndEntries")
    {
        const u8string temp_dir = MakeTempDataSourceDir("data_source_files_list_errors");
        const u8string missing_file_path = fs_combine_path(temp_dir.view(), "missing.txt");
        const u8string manifest_path {u8"FilesTree.txt"};
        const bool removed_manifest_before = fs_remove_file(manifest_path.view());
        const bool removed_dir_before = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_manifest_before, removed_dir_before);

        CHECK_THROWS_AS(DataSource::MountPack(u8"ignored", u8"FilesList", false), DataSourceException);

        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(WriteTextFixture(manifest_path, u8strex("{}\n", missing_file_path)));

        CHECK_THROWS_AS(DataSource::MountPack(u8"ignored", u8"FilesList", false), DataSourceException);

        CHECK(fs_remove_file(manifest_path.view()));
        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("MissingMandatorySourcesThrow")
    {
        CHECK_THROWS_AS(DataSource::MountDir(u8"/tmp/lf_data_source_missing_dir_required", false, false, false), DataSourceException);
        CHECK_THROWS_AS(DataSource::MountPack(u8"/tmp/lf_data_source_missing_pack_required", u8"MissingPack", false), DataSourceException);
        CHECK_THROWS(DataSource::MountPack(u8"/tmp/lf_data_source_missing_pack_required", u8"", false));
    }
}

FO_END_NAMESPACE
