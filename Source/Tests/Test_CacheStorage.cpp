#include "catch_amalgamated.hpp"

#include "CacheStorage.h"
#include "DiskFileSystem.h"

FO_BEGIN_NAMESPACE

static auto MakeTempCacheDir(string_view name) -> string
{
    const auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_string(base);
}

TEST_CASE("CacheStorage")
{
    SECTION("StringAndBinaryEntriesRoundtrip")
    {
        const string temp_dir = MakeTempCacheDir("cache_storage_roundtrip");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        CacheStorage cache {temp_dir};
        const vector<uint8> payload {{0x10, 0x20, 0x30, 0x40}};

        cache.SetString("greeting", "hello cache");
        cache.SetData("folder/item.bin", payload);

        CHECK(cache.HasEntry("greeting"));
        CHECK(cache.HasEntry("folder/item.bin"));
        CHECK(cache.GetString("greeting") == "hello cache");
        CHECK(cache.GetData("folder/item.bin") == payload);

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("EntryNamesAreSanitizedForFileBackend")
    {
        const string temp_dir = MakeTempCacheDir("cache_storage_sanitize");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        CacheStorage cache {temp_dir};
        cache.SetString("dir\\nested/file.txt", "payload");

        CHECK(cache.HasEntry("dir\\nested/file.txt"));
        CHECK(cache.GetString("dir\\nested/file.txt") == "payload");
        CHECK(fs_exists(strex(temp_dir).combine_path("dir_nested_file.txt").str()));

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("RemoveEntryAndMissingEntriesBehaveGracefully")
    {
        const string temp_dir = MakeTempCacheDir("cache_storage_remove");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        CacheStorage cache {temp_dir};
        cache.SetString("value", "to-remove");

        REQUIRE(cache.HasEntry("value"));
        cache.RemoveEntry("value");

        CHECK_FALSE(cache.HasEntry("value"));
        CHECK(cache.GetString("value").empty());
        CHECK(cache.GetData("value").empty());

        cache.RemoveEntry("missing");
        CHECK_FALSE(cache.HasEntry("missing"));

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("MoveConstructionPreservesAccess")
    {
        const string temp_dir = MakeTempCacheDir("cache_storage_move");
        const bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        CacheStorage original {temp_dir};
        original.SetString("name", "value");

        CacheStorage moved {std::move(original)};

        CHECK(moved.HasEntry("name"));
        CHECK(moved.GetString("name") == "value");

        CHECK(fs_remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
