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

#include "CacheStorage.h"
#include "DiskFileSystem.h"

FO_BEGIN_NAMESPACE

static auto MakeTempCacheDir(string_view name) -> string
{
    auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs::path_to_string(base);
}

TEST_CASE("CacheStorage")
{
    SECTION("StringAndBinaryEntriesRoundtrip")
    {
        string temp_dir = MakeTempCacheDir("cache_storage_roundtrip");
        bool removed_before = fs::remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        CacheStorage cache {temp_dir};
        vector<uint8_t> payload {{0x10, 0x20, 0x30, 0x40}};

        cache.SetString("greeting", "hello cache");
        REQUIRE(cache.SetDataChecked("folder/item.bin", payload));

        CHECK(cache.HasEntry("greeting"));
        CHECK(cache.HasEntry("folder/item.bin"));
        CHECK(cache.GetString("greeting") == "hello cache");
        CHECK(cache.GetData("folder/item.bin") == payload);

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("EntryNamesAreSanitizedForFileBackend")
    {
        string temp_dir = MakeTempCacheDir("cache_storage_sanitize");
        bool removed_before = fs::remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        CacheStorage cache {temp_dir};
        cache.SetString("dir\\nested/file.txt", "payload");

        CHECK(cache.HasEntry("dir\\nested/file.txt"));
        CHECK(cache.GetString("dir\\nested/file.txt") == "payload");
        CHECK(fs::exists(strex(temp_dir).combine_path("dir_nested_file.txt").str()));

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("RemoveEntryAndMissingEntriesBehaveGracefully")
    {
        string temp_dir = MakeTempCacheDir("cache_storage_remove");
        bool removed_before = fs::remove_dir_tree(temp_dir);
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

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("MoveConstructionPreservesAccess")
    {
        string temp_dir = MakeTempCacheDir("cache_storage_move");
        bool removed_before = fs::remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        CacheStorage original {temp_dir};
        original.SetString("name", "value");

        CacheStorage moved {std::move(original)};

        CHECK(moved.HasEntry("name"));
        CHECK(moved.GetString("name") == "value");

        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("BoundedReadClassifiesMissingAndRejectsSparseOversizeBeforeAllocation")
    {
        string temp_dir = MakeTempCacheDir("cache_storage_bounded");
        bool removed_before = fs::remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        CacheStorage cache {temp_dir};
        auto missing = cache.GetDataBounded("missing", 64);
        CHECK(missing.Status == CacheStorageReadStatus::Missing);
        CHECK(missing.Data.empty());

        cache.SetData("slot", vector<uint8_t> {1, 2, 3, 4});
        auto accepted = cache.GetDataBounded("slot", 4);
        CHECK(accepted.Status == CacheStorageReadStatus::Success);
        CHECK(accepted.Data == vector<uint8_t> {1, 2, 3, 4});

        string slot_path = strex(temp_dir).combine_path("slot");
        std::error_code resize_error;
        std::filesystem::resize_file(std::filesystem::path {fs::make_path(slot_path)}, 1024ULL * 1024ULL * 1024ULL, resize_error);
        REQUIRE_FALSE(resize_error);

        auto rejected = cache.GetDataBounded("slot", 64);
        CHECK(rejected.Status == CacheStorageReadStatus::TooLarge);
        CHECK(rejected.Data.empty());

        CHECK(fs::remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
