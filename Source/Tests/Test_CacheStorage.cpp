#include "catch_amalgamated.hpp"

#include "CacheStorage.h"
#include "DiskFileSystem.h"

FO_BEGIN_NAMESPACE

static auto MakeTempCacheDir(string_view name) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_u8string(base);
}

static void CheckCacheStorageContract(string_view temp_dir_name)
{
    FO_STACK_TRACE_ENTRY();

    u8string temp_dir = MakeTempCacheDir(temp_dir_name);
    bool removed_before = fs_remove_dir_tree(temp_dir);
    ignore_unused(removed_before);

    {
        CacheStorage cache {temp_dir};
        u8string unicode_text {u8"A\u042Fe\u0301\U0001F642\uFFFD\0B"};
        vector<byte> unicode_bytes {
            byte {0x41},
            byte {0xD0},
            byte {0xAF},
            byte {0x65},
            byte {0xCC},
            byte {0x81},
            byte {0xF0},
            byte {0x9F},
            byte {0x99},
            byte {0x82},
            byte {0xEF},
            byte {0xBF},
            byte {0xBD},
            byte {0x00},
            byte {0x42},
        };
        vector<byte> binary_payload {byte {0x00}, byte {0x80}, byte {0xFF}};
        vector<byte> ascii_bytes {byte {0x41}, byte {0x53}, byte {0x43}, byte {0x49}, byte {0x49}};

        cache.SetText("ascii-text", "ASCII");
        CHECK(cache.GetText("ascii-text") == u8"ASCII");
        CHECK(cache.GetBytes("ascii-text") == ascii_bytes);

        cache.SetText("text-to-bytes", unicode_text);
        CHECK(cache.HasEntry("text-to-bytes"));
        CHECK(cache.GetText("text-to-bytes") == unicode_text);
        CHECK(cache.GetBytes("text-to-bytes") == unicode_bytes);

        cache.SetBytes("bytes-to-text", unicode_bytes);
        CHECK(cache.GetBytes("bytes-to-text") == unicode_bytes);
        CHECK(cache.GetText("bytes-to-text") == unicode_text);

        cache.SetText("empty-text", u8"");
        cache.SetBytes("empty-bytes", {});
        CHECK(cache.HasEntry("empty-text"));
        CHECK(cache.HasEntry("empty-bytes"));
        CHECK(cache.GetText("empty-text").empty());
        CHECK(cache.GetBytes("empty-text").empty());
        CHECK(cache.GetText("empty-bytes").empty());
        CHECK(cache.GetBytes("empty-bytes").empty());

        cache.SetBytes("binary", binary_payload);
        CHECK(cache.GetBytes("binary") == binary_payload);
        CHECK_THROWS_AS((void)cache.GetText("binary"), TextValidationException);

        vector<byte> malformed_utf8 {byte {0xF0}, byte {0x28}, byte {0x8C}, byte {0x28}};
        cache.SetBytes("malformed", malformed_utf8);
        CHECK(cache.GetBytes("malformed") == malformed_utf8);

        try {
            (void)cache.GetText("malformed");
            FAIL("Malformed cache text was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::InvalidContinuationByte);
            CHECK(ex.offset() == 1);
        }

        std::u8string mutable_backing = u8"valid";
        optional<u8string_view> checked_view = u8string_view::TryFrom(mutable_backing);
        REQUIRE(checked_view.has_value());
        u8string_view stale_view = *checked_view;
        mutable_backing[0] = char8_t {0xFF};

        CHECK_THROWS_AS(cache.SetText("stale-new", stale_view), TextValidationException);
        CHECK_FALSE(cache.HasEntry("stale-new"));

        u8string original_text {u8"original"};
        cache.SetText("stale-existing", original_text);
        CHECK_THROWS_AS(cache.SetText("stale-existing", stale_view), TextValidationException);
        CHECK(cache.GetText("stale-existing") == original_text);

        CHECK_FALSE(cache.HasEntry("missing"));
        CHECK(cache.GetText("missing").empty());
        CHECK(cache.GetBytes("missing").empty());
        cache.RemoveEntry("missing");
        CHECK_FALSE(cache.HasEntry("missing"));

        cache.SetText("to-remove", u8"value");
        REQUIRE(cache.HasEntry("to-remove"));
        cache.RemoveEntry("to-remove");
        CHECK_FALSE(cache.HasEntry("to-remove"));
        CHECK(cache.GetText("to-remove").empty());
        CHECK(cache.GetBytes("to-remove").empty());
    }

    CHECK(fs_remove_dir_tree(temp_dir));
}

TEST_CASE("CacheStorage")
{
    SECTION("StrictTextAndBytesContractWithFileBackend")
    {
        CheckCacheStorageContract("cache_storage_file_contract");
    }

    SECTION("EntryNamesAreSanitizedForFileBackend")
    {
        u8string temp_dir = MakeTempCacheDir("cache_storage_sanitize");
        bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        CacheStorage cache {temp_dir};
        u8string payload {u8"payload"};
        cache.SetText("dir\\nested/file.txt", payload);

        CHECK(cache.HasEntry("dir\\nested/file.txt"));
        CHECK(cache.GetText("dir\\nested/file.txt") == payload);
        u8string nested_file = fs_path_to_u8string(std::filesystem::path {fs_make_path(temp_dir)} / std::filesystem::path {u8"dir_nested_file.txt"});
        CHECK(fs_exists(nested_file));

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("MoveConstructionPreservesAccess")
    {
        u8string temp_dir = MakeTempCacheDir("cache_storage_move");
        bool removed_before = fs_remove_dir_tree(temp_dir);
        ignore_unused(removed_before);

        CacheStorage original {temp_dir};
        u8string value {u8"value"};
        original.SetText("name", value);

        CacheStorage moved {std::move(original)};

        CHECK(moved.HasEntry("name"));
        CHECK(moved.GetText("name") == value);

        CHECK(fs_remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
