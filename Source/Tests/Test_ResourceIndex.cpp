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

#include "ResourceIndex.h"
#include "ResourcePack.h"

FO_BEGIN_NAMESPACE

static auto MakeTempIndexDir(string_view name) -> string
{
    auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    string dir = fs_path_to_string(base);
    REQUIRE(fs_create_directories(dir));
    return dir;
}

static auto MakeBytes(string_view text) -> const_span<uint8_t>
{
    return const_span<uint8_t> {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

static void WritePack(string_view dir, string_view name, const vector<std::pair<string, string>>& files)
{
    ResourcePackWriter writer {strex(dir).combine_path(strex("{}.fores", name)).str()};

    for (const auto& [path, content] : files) {
        writer.AddFile(path, MakeBytes(content));
    }

    writer.Finish();
}

static auto BytesOf(string_view text) -> vector<uint8_t>
{
    return vector<uint8_t> {text.begin(), text.end()};
}

static auto ReadThroughIndex(const ResourceIndexSource& index, string_view path) -> vector<uint8_t>
{
    size_t size = 0;
    uint64_t write_time = 0;
    auto buf = index.OpenFile(path, size, write_time);
    REQUIRE(buf);
    auto data = const_span<uint8_t> {buf.get(), size};
    return vector<uint8_t> {data.begin(), data.end()};
}

TEST_CASE("ResourceIndex")
{
    SECTION("MergesPacksWithTheLastOneWinning")
    {
        string dir = MakeTempIndexDir("index_merge");
        string index_path = strex(dir).combine_path("Merged.foindex").str();

        WritePack(dir, "Base", {{"Shared.txt", "from base"}, {"OnlyBase.txt", "base only"}});
        WritePack(dir, "Over", {{"Shared.txt", "from over"}, {"OnlyOver.txt", "over only"}});

        vector<string> dirs {dir};
        vector<ResourceIndexPack> packs;
        vector<string> pack_paths;
        REQUIRE(ResolveResourceIndexPacks(dirs, vector<string> {"Base", "Over"}, packs, pack_paths));
        BuildResourceIndex(index_path, pack_paths, packs);

        {
            ResourceIndexSource index {index_path, dirs};

            // The later pack owns the shared path, which is the precedence the per-pack mounts already have
            CHECK(ReadThroughIndex(index, "Shared.txt") == BytesOf("from over"));
            CHECK(ReadThroughIndex(index, "OnlyBase.txt") == BytesOf("base only"));
            CHECK(ReadThroughIndex(index, "OnlyOver.txt") == BytesOf("over only"));

            auto snapshot = index.GetIndexSnapshot();
            REQUIRE(snapshot.has_value());
            CHECK(snapshot->size() == 3);
            CHECK(index.GetFileNames("", true, "txt").size() == 3);
            CHECK_FALSE(index.IsFileExists("Missing.txt"));
        }

        CHECK(fs_remove_dir_tree(dir));
    }

    SECTION("ReportsWhetherTheIndexStillDescribesTheDisk")
    {
        string dir = MakeTempIndexDir("index_current");
        string index_path = strex(dir).combine_path("Merged.foindex").str();
        vector<string> dirs {dir};
        vector<string> names {"Base", "Over"};

        WritePack(dir, "Base", {{"A.txt", "a"}});
        WritePack(dir, "Over", {{"B.txt", "b"}});

        // An absent index is the ordinary first-run answer, not an error
        CHECK_FALSE(IsResourceIndexCurrent(index_path, dirs, names));

        vector<ResourceIndexPack> packs;
        vector<string> pack_paths;
        REQUIRE(ResolveResourceIndexPacks(dirs, names, packs, pack_paths));
        BuildResourceIndex(index_path, pack_paths, packs);
        CHECK(IsResourceIndexCurrent(index_path, dirs, names));

        // Dropping a pack from the list changes the fold, so the same file no longer describes the request
        CHECK_FALSE(IsResourceIndexCurrent(index_path, dirs, vector<string> {"Base"}));

        // And so does rewriting one, because its hash travels in the key
        WritePack(dir, "Over", {{"B.txt", "changed"}});
        CHECK_FALSE(IsResourceIndexCurrent(index_path, dirs, names));

        CHECK(fs_remove_dir_tree(dir));
    }

    SECTION("RefusesAnIndexWhosePackMovedOn")
    {
        string dir = MakeTempIndexDir("index_stale");
        string index_path = strex(dir).combine_path("Merged.foindex").str();

        WritePack(dir, "Data", {{"File.txt", "first"}});

        vector<string> dirs {dir};
        vector<ResourceIndexPack> packs;
        vector<string> pack_paths;
        REQUIRE(ResolveResourceIndexPacks(dirs, vector<string> {"Data"}, packs, pack_paths));
        BuildResourceIndex(index_path, pack_paths, packs);

        // Rewriting the pack changes its hash, so the recorded tree no longer describes what is on disk
        WritePack(dir, "Data", {{"File.txt", "second"}});

        CHECK_THROWS(ResourceIndexSource {index_path, dirs});

        ResourceIndexHeader header;
        REQUIRE(ReadResourceIndexHeader(index_path, header));

        vector<ResourceIndexPack> current_packs;
        vector<string> current_paths;
        REQUIRE(ResolveResourceIndexPacks(dirs, vector<string> {"Data"}, current_packs, current_paths));
        CHECK(ComputeResourceIndexPackListHash(current_packs) != header.PackListHash);

        // The rebuild is the whole repair: same path, new tree, and the pack itself is never written to
        BuildResourceIndex(index_path, current_paths, current_packs);

        {
            ResourceIndexSource index {index_path, dirs};
            CHECK(ReadThroughIndex(index, "File.txt") == BytesOf("second"));
        }

        CHECK(fs_remove_dir_tree(dir));
    }

    SECTION("MountsAnIndexTooLargeForAnApproximateDecode")
    {
        // Small fixtures hide a decode sized by a multiplier instead of the exact length: the product only
        // becomes absurd once the index is real. 4000 entries is past that point and still quick
        static constexpr size_t ENTRY_COUNT = 4000;

        string dir = MakeTempIndexDir("index_large");
        string index_path = strex(dir).combine_path("Merged.foindex").str();

        {
            ResourcePackWriter writer {strex(dir).combine_path("Bulk.fores").str()};

            for (size_t i = 0; i < ENTRY_COUNT; ++i) {
                writer.AddFile(strex("CommonArt_Group/Scenery/Section{}/Sprite_{:05}.png", i % 16, i).str(), MakeBytes("payload"));
            }

            writer.Finish();
        }

        vector<string> dirs {dir};
        vector<ResourceIndexPack> packs;
        vector<string> pack_paths;
        REQUIRE(ResolveResourceIndexPacks(dirs, vector<string> {"Bulk"}, packs, pack_paths));
        BuildResourceIndex(index_path, pack_paths, packs);

        ResourceIndexHeader header;
        REQUIRE(ReadResourceIndexHeader(index_path, header));
        CHECK(header.EntryCount == ENTRY_COUNT);
        CHECK(header.IndexDecodedSize > 128 * 1024);

        {
            ResourceIndexSource index {index_path, dirs};
            CHECK(ReadThroughIndex(index, "CommonArt_Group/Scenery/Section0/Sprite_00000.png") == BytesOf("payload"));
            CHECK(index.GetIndexSnapshot()->size() == ENTRY_COUNT);
        }

        CHECK(fs_remove_dir_tree(dir));
    }

    SECTION("RejectsMalformedIndexes")
    {
        string dir = MakeTempIndexDir("index_malformed");
        string index_path = strex(dir).combine_path("Merged.foindex").str();

        WritePack(dir, "Data", {{"File.txt", "payload"}});

        vector<string> dirs {dir};
        vector<ResourceIndexPack> packs;
        vector<string> pack_paths;
        REQUIRE(ResolveResourceIndexPacks(dirs, vector<string> {"Data"}, packs, pack_paths));
        BuildResourceIndex(index_path, pack_paths, packs);

        auto original = fs_read_file(index_path);
        REQUIRE(original.has_value());

        string not_an_index = strex(dir).combine_path("NotAnIndex.foindex").str();
        REQUIRE(fs_write_file(not_an_index, string(RESOURCE_INDEX_HEADER_SIZE + 16, '\0')));
        CHECK_THROWS(ResourceIndexSource {not_an_index, dirs});

        ResourceIndexHeader ignored;
        CHECK_FALSE(ReadResourceIndexHeader(not_an_index, ignored));

        // One flipped header byte breaks the checksum, which is what stops a bad offset being believed
        string bad_header = strex(dir).combine_path("BadHeader.foindex").str();
        string bad_header_data = *original;
        bad_header_data[20] = static_cast<char>(bad_header_data[20] ^ 0xFF);
        REQUIRE(fs_write_file(bad_header, bad_header_data));
        CHECK_THROWS(ResourceIndexSource {bad_header, dirs});

        string truncated = strex(dir).combine_path("Truncated.foindex").str();
        REQUIRE(fs_write_file(truncated, original->substr(0, original->size() - 4)));
        CHECK_THROWS(ResourceIndexSource {truncated, dirs});

        CHECK(fs_remove_dir_tree(dir));
    }
}

// Hidden: a measurement, not an assertion. Run it by name when the numbers are wanted. The shape comes from
// the real corpus, and payloads are tiny on purpose - the builder reads pack indexes, never payloads
TEST_CASE("ResourceIndexCost", "[.]")
{
    static constexpr size_t PACK_COUNT = 32;
    static constexpr size_t ENTRIES_PER_PACK = 555;

    string dir = MakeTempIndexDir("index_cost");
    string index_path = strex(dir).combine_path("Merged.foindex").str();
    vector<string> dirs {dir};
    vector<string> names;

    auto elapsed_ms = [](auto started) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(); };

    auto write_started = std::chrono::steady_clock::now();

    for (size_t pack = 0; pack < PACK_COUNT; ++pack) {
        string pack_name = strex("Pack{}", pack).str();
        names.emplace_back(pack_name);

        ResourcePackWriter writer {strex(dir).combine_path(strex("{}.fores", pack_name)).str()};

        for (size_t entry = 0; entry < ENTRIES_PER_PACK; ++entry) {
            // Paths of the length the measured corpus averages, so the string pool is the size it would be
            writer.AddFile(strex("CommonArt_Group{}/Scenery/Section{}/Sprite_{}_{:04}.png", pack, entry % 16, pack, entry).str(), MakeBytes("payload"));
        }

        writer.Finish();
    }

    double write_ms = elapsed_ms(write_started);

    vector<ResourceIndexPack> packs;
    vector<string> pack_paths;
    auto resolve_started = std::chrono::steady_clock::now();
    REQUIRE(ResolveResourceIndexPacks(dirs, names, packs, pack_paths));
    double resolve_ms = elapsed_ms(resolve_started);

    auto build_started = std::chrono::steady_clock::now();
    BuildResourceIndex(index_path, pack_paths, packs);
    double build_ms = elapsed_ms(build_started);

    ResourceIndexHeader header;
    REQUIRE(ReadResourceIndexHeader(index_path, header));

    double mount_ms = 0.0;
    double lookup_ms = 0.0;
    double read_ms = 0.0;
    size_t entry_count = 0;

    // Scoped so the mounted tree releases its pack handles before the directory is removed
    {
        auto mount_started = std::chrono::steady_clock::now();
        ResourceIndexSource index {index_path, dirs};
        mount_ms = elapsed_ms(mount_started);

        // Bound to a name first: the snapshot comes back by value, and iterating *temporary would walk a
        // container that is already destroyed
        auto snapshot = index.GetIndexSnapshot();
        REQUIRE(snapshot.has_value());

        vector<string> lookup_paths;
        lookup_paths.reserve(snapshot->size());

        for (const auto& entry : *snapshot) {
            lookup_paths.emplace_back(entry.Path);
        }

        REQUIRE(lookup_paths.size() == header.EntryCount);

        entry_count = lookup_paths.size();

        auto lookup_started = std::chrono::steady_clock::now();
        size_t found = 0;

        for (const auto& path : lookup_paths) {
            found += index.IsFileExists(path) ? 1 : 0;
        }

        lookup_ms = elapsed_ms(lookup_started);
        CHECK(found == lookup_paths.size());

        auto read_started = std::chrono::steady_clock::now();

        for (const auto& path : lookup_paths) {
            CHECK(ReadThroughIndex(index, path).size() == 7);
        }

        read_ms = elapsed_ms(read_started);
    }

    WARN(strex("packs {}, entries {}\n"
               "  write packs   {:.1f} ms\n"
               "  resolve       {:.1f} ms ({} header reads)\n"
               "  build index   {:.1f} ms\n"
               "  mount index   {:.1f} ms\n"
               "  lookup all    {:.1f} ms ({:.2f} us each)\n"
               "  read all      {:.1f} ms ({:.2f} us each)\n"
               "  index stored  {} bytes, decoded {} bytes",
        PACK_COUNT, header.EntryCount, write_ms, resolve_ms, PACK_COUNT, build_ms, mount_ms, lookup_ms, lookup_ms * 1000.0 / static_cast<double>(entry_count), read_ms, read_ms * 1000.0 / static_cast<double>(entry_count), header.IndexStoredSize, header.IndexDecodedSize)
            .str());

    CHECK(fs_remove_dir_tree(dir));
}

FO_END_NAMESPACE
