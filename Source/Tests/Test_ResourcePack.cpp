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

#include "ResourcePack.h"

FO_BEGIN_NAMESPACE

static auto MakeTempPackPath(string_view name) -> string
{
    auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}.fores", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_string(base);
}

static auto MakeBytes(string_view text) -> const_span<uint8_t>
{
    return const_span<uint8_t> {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

// Compressible far past the threshold, so the writer is forced down the Deflate path
static auto MakeCompressiblePayload() -> vector<uint8_t>
{
    return vector<uint8_t>(4096, uint8_t {0x41});
}

// Random enough that deflate cannot win, so the writer is forced to store it raw
static auto MakeIncompressiblePayload() -> vector<uint8_t>
{
    vector<uint8_t> payload(4096);
    uint32_t state = 0x12345678;

    for (size_t i = 0; i < payload.size(); ++i) {
        state = state * 1664525u + 1013904223u;
        payload[i] = static_cast<uint8_t>(state >> 24);
    }

    return payload;
}

static auto ReadWholeFile(const ResourcePackSource& pack, string_view path) -> vector<uint8_t>
{
    size_t size = 0;
    uint64_t write_time = 0;
    auto buf = pack.OpenFile(path, size, write_time);
    REQUIRE(buf);
    auto data = const_span<uint8_t> {buf.get(), size};
    return vector<uint8_t> {data.begin(), data.end()};
}

TEST_CASE("ResourcePack")
{
    SECTION("RoundtripsEveryEntry")
    {
        string pack_path = MakeTempPackPath("roundtrip");
        vector<uint8_t> compressible = MakeCompressiblePayload();
        vector<uint8_t> incompressible = MakeIncompressiblePayload();

        {
            ResourcePackWriter writer {pack_path};
            writer.AddFile("Zeta/Last.bin", MakeBytes("last"));
            writer.AddFile("Alpha/First.bin", const_span<uint8_t> {compressible.data(), compressible.size()});
            writer.AddFile("Alpha/Noise.bin", const_span<uint8_t> {incompressible.data(), incompressible.size()});
            writer.AddFile("Empty.bin", const_span<uint8_t> {});
            writer.Finish();
        }

        {
            ResourcePackSource pack {pack_path};

            CHECK(pack.IsFileExists("Alpha/First.bin"));
            CHECK(pack.IsFileExists("Empty.bin"));
            CHECK_FALSE(pack.IsFileExists("Missing.bin"));
            CHECK_FALSE(pack.IsDiskDir());

            size_t size = 0;
            uint64_t write_time = 0;
            REQUIRE(pack.GetFileInfo("Alpha/First.bin", size, write_time));
            CHECK(size == compressible.size());

            CHECK(ReadWholeFile(pack, "Alpha/First.bin") == compressible);
            CHECK(ReadWholeFile(pack, "Alpha/Noise.bin") == incompressible);
            CHECK(ReadWholeFile(pack, "Zeta/Last.bin").size() == 4);
            CHECK(ReadWholeFile(pack, "Empty.bin").empty());

            auto snapshot = pack.GetIndexSnapshot();
            REQUIRE(snapshot.has_value());
            CHECK(snapshot->size() == 4);

            CHECK(pack.GetFileNames("Alpha", true, "").size() == 2);
            CHECK(pack.GetFileNames("", true, "bin").size() == 4);
        }

        // A mounted source holds the file open, so removal only succeeds once it is gone
        CHECK(fs_remove_file(pack_path));
    }

    SECTION("StoresWhatDeflateCannotShrink")
    {
        string compressible_path = MakeTempPackPath("codec_deflate");
        string incompressible_path = MakeTempPackPath("codec_stored");
        vector<uint8_t> compressible = MakeCompressiblePayload();
        vector<uint8_t> incompressible = MakeIncompressiblePayload();

        {
            ResourcePackWriter writer {compressible_path};
            writer.AddFile("Payload.bin", const_span<uint8_t> {compressible.data(), compressible.size()});
            writer.Finish();
        }
        {
            ResourcePackWriter writer {incompressible_path};
            writer.AddFile("Payload.bin", const_span<uint8_t> {incompressible.data(), incompressible.size()});
            writer.Finish();
        }

        // The pack that could be shrunk is far smaller on disk; the one that could not is barely over its input
        auto compressible_size = fs_file_size(compressible_path);
        auto incompressible_size = fs_file_size(incompressible_path);
        REQUIRE(compressible_size.has_value());
        REQUIRE(incompressible_size.has_value());
        CHECK(*compressible_size < compressible.size() / 2);
        CHECK(*incompressible_size >= incompressible.size());
        CHECK(*incompressible_size < incompressible.size() + 512);

        CHECK(fs_remove_file(compressible_path));
        CHECK(fs_remove_file(incompressible_path));
    }

    SECTION("HeaderReadsWithoutTheIndex")
    {
        string pack_path = MakeTempPackPath("header");

        {
            ResourcePackWriter writer {pack_path};
            writer.AddFile("Payload.bin", MakeBytes("payload"));
            writer.Finish();
        }

        ResourcePackHeader header;
        REQUIRE(ReadResourcePackHeader(pack_path, header));
        CHECK(header.VersionMajor == RESOURCE_PACK_VERSION_MAJOR);
        CHECK(header.EntryCount == 1);
        CHECK(header.PackHash != 0);

        {
            ResourcePackSource pack {pack_path};
            CHECK(pack.GetPackHash() == header.PackHash);
        }

        CHECK(fs_remove_file(pack_path));
    }
    SECTION("RejectsMalformedPacks")
    {
        string pack_path = MakeTempPackPath("malformed");

        {
            ResourcePackWriter writer {pack_path};
            writer.AddFile("Payload.bin", MakeBytes("payload"));
            writer.Finish();
        }

        auto original = fs_read_file(pack_path);
        REQUIRE(original.has_value());
        REQUIRE(original->size() > RESOURCE_PACK_HEADER_SIZE);

        string not_a_pack = MakeTempPackPath("not_a_pack");
        REQUIRE(fs_write_file(not_a_pack, string(RESOURCE_PACK_HEADER_SIZE + 16, '\0')));
        CHECK_THROWS_AS(ResourcePackSource {not_a_pack}, DataSourceException);

        ResourcePackHeader ignored_header;
        CHECK_FALSE(ReadResourcePackHeader(not_a_pack, ignored_header));

        string too_short = MakeTempPackPath("too_short");
        REQUIRE(fs_write_file(too_short, original->substr(0, RESOURCE_PACK_HEADER_SIZE - 1)));
        CHECK_THROWS_AS(ResourcePackSource {too_short}, DataSourceException);

        // One flipped header byte breaks the header checksum, which is what stops a bad offset being believed
        string bad_header = MakeTempPackPath("bad_header");
        string bad_header_data = *original;
        bad_header_data[20] = static_cast<char>(bad_header_data[20] ^ 0xFF);
        REQUIRE(fs_write_file(bad_header, bad_header_data));
        CHECK_THROWS_AS(ResourcePackSource {bad_header}, DataSourceException);

        // A truncated body leaves the header intact, so the index read is what has to fail
        string truncated = MakeTempPackPath("truncated");
        REQUIRE(fs_write_file(truncated, original->substr(0, original->size() - 4)));
        CHECK_THROWS_AS(ResourcePackSource {truncated}, DataSourceException);

        CHECK(fs_remove_file(pack_path));
        CHECK(fs_remove_file(not_a_pack));
        CHECK(fs_remove_file(too_short));
        CHECK(fs_remove_file(bad_header));
        CHECK(fs_remove_file(truncated));
    }
    SECTION("RefusesADuplicatePath")
    {
        string pack_path = MakeTempPackPath("duplicate");
        ResourcePackWriter writer {pack_path};
        writer.AddFile("Payload.bin", MakeBytes("first"));
        writer.AddFile("Payload.bin", MakeBytes("second"));
        CHECK_THROWS(writer.Finish());
    }
}

FO_END_NAMESPACE
