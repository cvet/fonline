#include "catch_amalgamated.hpp"

#include "DataSource.h"
#include "DiskFileSystem.h"

FO_BEGIN_NAMESPACE

static auto MakeTempDataSourceDir(string_view name) -> string
{
    const auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_string(base);
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
        CHECK_FALSE(non_recursive->IsFileExists("nested/child.txt"));
        CHECK(recursive->IsFileExists("nested/child.txt"));

        size_t size = 0;
        uint64 write_time = 0;
        CHECK(recursive->GetFileInfo("nested/child.txt", size, write_time));
        CHECK(size == 5);
        CHECK(write_time != 0);

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
        uint64 write_time = 0;

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

    SECTION("MaybeNotAvailableReturnsDummySources")
    {
        const auto maybe_dir = DataSource::MountDir("/tmp/lf_data_source_missing_dir", false, false, true);
        const auto maybe_pack = DataSource::MountPack("/tmp/lf_data_source_missing_pack", "MissingPack", true);

        REQUIRE(maybe_dir);
        REQUIRE(maybe_pack);

        size_t size = 123;
        uint64 write_time = 456;

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

        const auto files_list = DataSource::MountPack("ignored", "FilesList", false);
        REQUIRE(files_list);

        size_t size = 0;
        uint64 write_time = 0;
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
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("MissingMandatorySourcesThrow")
    {
        CHECK_THROWS_AS(DataSource::MountDir("/tmp/lf_data_source_missing_dir_required", false, false, false), DataSourceException);
        CHECK_THROWS_AS(DataSource::MountPack("/tmp/lf_data_source_missing_pack_required", "MissingPack", false), DataSourceException);
    }
}

FO_END_NAMESPACE
