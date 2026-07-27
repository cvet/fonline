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

#include "CommonHelpers.h"
#include "DiskFileSystem.h"
#include "SafeArithmetics.h"

FO_BEGIN_NAMESPACE

static_assert(std::same_as<decltype(fs_read_file_text(u8string_view {})), optional<u8string>>);
using FsWriteAsciiText = bool (*)(u8string_view, string_view);
static_assert(!std::is_invocable_r_v<bool, FsWriteAsciiText, string_view, string_view>);

static auto MakeTempTestDir(string_view name) -> u8string
{
    const auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_u8string(base);
}

TEST_CASE("DiskFileSystem")
{
    SECTION("TextReadWriteRenameAndRemoveRoundtrip")
    {
        const auto temp_dir = MakeTempTestDir("diskfs_roundtrip");
        const auto file_path = fs_combine_path(temp_dir.view(), "nested/data.txt");
        const auto renamed_path = fs_combine_path(temp_dir.view(), "nested/renamed.txt");
        constexpr char8_t content_literal[] = u8"Привет 🌍\0filesystem";
        const u8string content = u8string::FromChecked(std::u8string_view {content_literal, std::size(content_literal) - 1});

        const auto removed_before_roundtrip = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before_roundtrip);

        REQUIRE(fs_write_file_text(file_path.view(), content.view()));
        CHECK(fs_exists(file_path.view()));
        CHECK_FALSE(fs_is_dir(file_path.view()));
        REQUIRE(fs_file_size(file_path.view()).has_value());
        CHECK(*fs_file_size(file_path.view()) == content.size());
        REQUIRE(fs_read_file_text(file_path.view()).has_value());
        CHECK(*fs_read_file_text(file_path.view()) == content);

        REQUIRE(fs_rename(file_path.view(), renamed_path.view()));
        CHECK_FALSE(fs_exists(file_path.view()));
        CHECK(fs_exists(renamed_path.view()));
        REQUIRE(fs_remove_file(renamed_path.view()));
        CHECK_FALSE(fs_exists(renamed_path.view()));
        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("TextReadRejectsMalformedUtf8WithoutChangingByteAccess")
    {
        const auto temp_dir = MakeTempTestDir("diskfs_invalid_utf8");
        const auto file_path = fs_combine_path(temp_dir.view(), "invalid.txt");
        const vector<byte> malformed {byte {0xC3}, byte {0x28}};

        const auto removed_before_test = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before_test);

        REQUIRE(fs_write_file_bytes(file_path.view(), malformed));
        CHECK_THROWS_AS(fs_read_file_text(file_path.view()), TextValidationException);
        REQUIRE(fs_read_file_bytes(file_path.view()).has_value());
        CHECK(*fs_read_file_bytes(file_path.view()) == malformed);
        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("BinaryReadWriteCompareAndHashRoundtrip")
    {
        const auto temp_dir = MakeTempTestDir("diskfs_binary_roundtrip");
        const auto file_path = fs_combine_path(temp_dir.view(), "nested/data.bin");
        const vector<byte> content {byte {0x00}, byte {0x80}, byte {0xFF}};
        const vector<byte> different_content {byte {0x00}, byte {0x80}, byte {0xFE}};

        const auto removed_before_roundtrip = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before_roundtrip);

        CHECK_FALSE(fs_read_file_bytes(file_path.view()).has_value());
        REQUIRE(fs_write_file_bytes(file_path.view(), content));

        const auto read_content = fs_read_file_bytes(file_path.view());
        REQUIRE(read_content.has_value());
        CHECK(*read_content == content);
        CHECK(fs_compare_file_bytes(file_path.view(), content));
        CHECK_FALSE(fs_compare_file_bytes(file_path.view(), different_content));

        constexpr uint64_t expected_hash = UINT64_C(0xD79A37186A9DDA16);
        CHECK(fs_hash_bytes(content) == expected_hash);
        REQUIRE(fs_hash_file(file_path.view()).has_value());
        CHECK(*fs_hash_file(file_path.view()) == expected_hash);

        REQUIRE(fs_write_file_bytes(file_path.view(), {}));
        const auto empty_content = fs_read_file_bytes(file_path.view());
        REQUIRE(empty_content.has_value());
        CHECK(empty_content->empty());
        CHECK(fs_compare_file_bytes(file_path.view(), {}));

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("IterateDirRespectsRecursiveFlag")
    {
        const auto temp_dir = MakeTempTestDir("diskfs_iterate");
        const auto top_file = fs_combine_path(temp_dir.view(), "top.txt");
        const auto nested_file = fs_combine_path(temp_dir.view(), "sub/nested.txt");

        const auto removed_before_iterate = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before_iterate);
        REQUIRE(fs_write_file_text(top_file.view(), u8"top"));
        REQUIRE(fs_write_file_text(nested_file.view(), u8"nested"));

        vector<u8string> flat_entries;
        fs_iterate_dir(temp_dir.view(), false, [&](u8string_view path, size_t, uint64_t) { flat_entries.emplace_back(path); });
        CHECK(flat_entries.size() == 1);
        CHECK(flat_entries.front() == u8string {u8"top.txt"});

        vector<u8string> recursive_entries;
        fs_iterate_dir(temp_dir.view(), true, [&](u8string_view path, size_t, uint64_t) { recursive_entries.emplace_back(path); });
        CHECK(recursive_entries.size() == 2);
        CHECK(std::ranges::find(recursive_entries, u8string {u8"top.txt"}) != recursive_entries.end());
        CHECK(std::ranges::find(recursive_entries, u8string {u8"sub/nested.txt"}) != recursive_entries.end());

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("TouchAndStreamHelpersWork")
    {
        const auto temp_dir = MakeTempTestDir("diskfs_stream");
        const auto file_path = fs_combine_path(temp_dir.view(), "touch.bin");

        const auto removed_before_stream = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before_stream);
        REQUIRE(fs_create_directories(temp_dir.view()));
        REQUIRE(fs_touch_file(file_path.view()));
        CHECK(fs_exists(file_path.view()));
        CHECK(fs_last_write_time(file_path.view()) != 0);

        std::istringstream stream {string {"abcdef"}, std::ios::binary};
        CHECK(stream_get_size(stream) == 6);
        CHECK(stream_get_read_pos(stream) == 0);

        array<byte, 3> buf {};
        REQUIRE(stream_read_exact(stream, buf));
        const array<byte, 3> expected_prefix {byte {'a'}, byte {'b'}, byte {'c'}};
        CHECK(buf == expected_prefix);
        CHECK(stream_get_read_pos(stream) == 3);
        REQUIRE(stream_set_read_pos(stream, 1, std::ios_base::cur));
        CHECK(stream_get_read_pos(stream) == 4);
        REQUIRE(stream_read_exact(stream, span<byte> {buf}.first(2)));
        CHECK(buf[0] == byte {'e'});
        CHECK(buf[1] == byte {'f'});

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("FileHashMatchesInMemoryReference")
    {
        const auto temp_dir = MakeTempTestDir("diskfs_hash");
        const auto file_path = fs_combine_path(temp_dir.view(), "hash.bin");
        const auto removed_before_hash = fs_remove_dir_tree(temp_dir.view());
        ignore_unused(removed_before_hash);

        const auto check_hash = [&file_path](size_t size) {
            vector<byte> data(size);

            for (size_t index = 0; index < size; index++) {
                data[index] = byte {numeric_cast<uint8_t>((index * 37u + 11u) & 0xFFu)};
            }

            REQUIRE(fs_write_file_bytes(file_path.view(), data));
            REQUIRE(fs_hash_file(file_path.view()).has_value());
            CHECK(*fs_hash_file(file_path.view()) == fs_hash_bytes(data));
        };

        for (auto size : {size_t(0), size_t(1), size_t(3), size_t(4), size_t(15), size_t(16), size_t(17), size_t(47), size_t(48), size_t(49), size_t(63), size_t(64), size_t(65), size_t(96), size_t(97), size_t(70000)}) {
            check_hash(size);
        }

        CHECK(fs_remove_dir_tree(temp_dir.view()));
    }

    SECTION("MakeWritablePathLayersRelativeUnderRoot")
    {
        const u8string root {u8"/data/user"};
        const u8string nested_relative {u8"Resources/Sub"};
        const u8string empty_path;
        const u8string cache_path {u8"Cache"};
        const u8string expected_cache_path = fs_combine_path(root.view(), "Cache");
        const u8string expected_nested_path = fs_combine_path(root.view(), "Resources/Sub");

        // Portable layout (empty root): the relative path is returned unchanged, written next to the exe.
        CHECK(fs_make_writable_path(empty_path.view(), cache_path.view()) == cache_path);
        CHECK(fs_make_writable_path(empty_path.view(), nested_relative.view()) == nested_relative);

        // Installed layout: the relative path is layered under the writable root.
        CHECK(fs_make_writable_path(root.view(), cache_path.view()) == expected_cache_path);
        CHECK(fs_make_writable_path(root.view(), nested_relative.view()) == expected_nested_path);

        // An already-absolute relative path is never relocated under the root, in either layout.
        const auto absolute_input = MakeTempTestDir("diskfs_writable_abs");
        CHECK(fs_is_absolute_path(absolute_input.view()));
        CHECK(fs_make_writable_path(root.view(), absolute_input.view()) == absolute_input);
        CHECK(fs_make_writable_path(empty_path.view(), absolute_input.view()) == absolute_input);
    }
}

FO_END_NAMESPACE
