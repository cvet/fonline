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

#include "ImageWriter.h"

FO_BEGIN_NAMESPACE

// PNG chunk checksums are the same CRC-32 zip uses, recomputed here independently of zlib so the test
// proves the written bytes rather than agreeing with the writer's own helper
static auto CalcPngCrc32(const_span<uint8_t> data) noexcept -> uint32_t
{
    uint32_t crc = 0xFFFFFFFF;

    for (uint8_t byte : data) {
        crc ^= byte;

        for (size_t bit = 0; bit != 8; bit++) {
            crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xEDB88320 : 0);
        }
    }

    return crc ^ 0xFFFFFFFF;
}

TEST_CASE("ImageWriter")
{
    SECTION("WriteSimpleTgaCreatesFileWithExpectedHeader")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_image_writer_tests" / std::to_string(std::random_device {}());
        auto file_path = temp_root / "nested" / "sample.tga";

        isize32 image_size {2, 1};
        vector<ucolor> pixels;
        pixels.emplace_back(ucolor {1, 2, 3, 4});
        pixels.emplace_back(ucolor {5, 6, 7, 8});

        ImageWriter::WriteSimpleTga(string(file_path.string()), image_size, pixels);

        REQUIRE(std::filesystem::exists(file_path));
        CHECK(std::filesystem::file_size(file_path) == 18 + pixels.size() * sizeof(uint32_t));

        std::ifstream input(file_path, std::ios::binary);
        REQUIRE(input);

        std::array<uint8_t, 18> header {};
        input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
        REQUIRE(input.gcount() == static_cast<std::streamsize>(header.size()));

        CHECK(header[2] == 2);
        CHECK(header[12] == 2);
        CHECK(header[13] == 0);
        CHECK(header[14] == 1);
        CHECK(header[15] == 0);
        CHECK(header[16] == 32);
        CHECK(header[17] == 0x20);

        std::array<uint32_t, 2> stored_pixels {};
        input.read(reinterpret_cast<char*>(stored_pixels.data()), static_cast<std::streamsize>(sizeof(stored_pixels)));
        REQUIRE(input.gcount() == static_cast<std::streamsize>(sizeof(stored_pixels)));

        // A TrueColor TGA stores pixels in B, G, R, A order, so the writer swaps red and blue
        auto to_bgra = [](ucolor c) -> uint32_t {
            std::swap(c.comp.r, c.comp.b);
            return c.rgba;
        };

        CHECK(stored_pixels[0] == to_bgra(pixels[0]));
        CHECK(stored_pixels[1] == to_bgra(pixels[1]));

        input.close();

        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("WriteSimplePngCreatesDecodableFile")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_image_writer_tests" / std::to_string(std::random_device {}());
        auto file_path = temp_root / "nested" / "sample.png";

        isize32 image_size {2, 2};
        vector<ucolor> pixels;
        pixels.emplace_back(ucolor {1, 2, 3, 4});
        pixels.emplace_back(ucolor {5, 6, 7, 8});
        pixels.emplace_back(ucolor {9, 10, 11, 12});
        pixels.emplace_back(ucolor {13, 14, 15, 16});

        ImageWriter::WriteSimplePng(string(file_path.string()), image_size, pixels);

        REQUIRE(std::filesystem::exists(file_path));

        auto file_size = numeric_cast<size_t>(std::filesystem::file_size(file_path));
        vector<uint8_t> file_data(file_size);
        std::ifstream input(file_path, std::ios::binary);
        REQUIRE(input);
        input.read(reinterpret_cast<char*>(file_data.data()), static_cast<std::streamsize>(file_data.size()));
        REQUIRE(input.gcount() == static_cast<std::streamsize>(file_data.size()));
        input.close();

        const uint8_t expected_signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        REQUIRE(file_data.size() > sizeof(expected_signature));
        CHECK(std::equal(std::begin(expected_signature), std::end(expected_signature), file_data.begin()));

        auto read_big_endian = [&file_data](size_t offset) -> uint32_t {
            return (numeric_cast<uint32_t>(file_data[offset]) << 24) | (numeric_cast<uint32_t>(file_data[offset + 1]) << 16) | //
                (numeric_cast<uint32_t>(file_data[offset + 2]) << 8) | numeric_cast<uint32_t>(file_data[offset + 3]);
        };

        // Chunks follow the signature as length, type, payload, checksum - walk them to reach IHDR and IDAT
        map<string, std::pair<size_t, size_t>> chunks;
        size_t offset = sizeof(expected_signature);

        while (offset + 12 <= file_data.size()) {
            size_t payload_size = numeric_cast<size_t>(read_big_endian(offset));
            string type(reinterpret_cast<const char*>(file_data.data()) + offset + 4, 4);
            REQUIRE(offset + 12 + payload_size <= file_data.size());
            CHECK(read_big_endian(offset + 8 + payload_size) == CalcPngCrc32(const_span<uint8_t>(file_data).subspan(offset + 4, payload_size + 4)));
            chunks.emplace(type, std::pair {offset + 8, payload_size});
            offset += 12 + payload_size;
        }

        CHECK(offset == file_data.size());
        REQUIRE(chunks.count("IHDR") == 1);
        REQUIRE(chunks.count("IDAT") == 1);
        REQUIRE(chunks.count("IEND") == 1);

        auto header = chunks["IHDR"];
        REQUIRE(header.second == 13);
        CHECK(read_big_endian(header.first) == 2);
        CHECK(read_big_endian(header.first + 4) == 2);
        CHECK(file_data[header.first + 8] == 8);
        CHECK(file_data[header.first + 9] == 6);
        CHECK(file_data[header.first + 10] == 0);
        CHECK(file_data[header.first + 11] == 0);
        CHECK(file_data[header.first + 12] == 0);
        CHECK(chunks["IEND"].second == 0);

        auto image_data = chunks["IDAT"];
        auto compressed = const_span<uint8_t>(file_data).subspan(image_data.first, image_data.second);
        vector<uint8_t> scanlines = compressor::decompress(compressed, 4);

        // Each scanline is its filter byte followed by the pixels in R, G, B, A order, unswapped
        REQUIRE(scanlines.size() == (2 * sizeof(ucolor) + 1) * 2);
        CHECK(scanlines[0] == 0);
        CHECK(scanlines[1 + 2 * sizeof(ucolor)] == 0);

        for (size_t i = 0; i < pixels.size(); i++) {
            size_t pixel_offset = (i / 2) * (2 * sizeof(ucolor) + 1) + 1 + (i % 2) * sizeof(ucolor);
            CHECK(scanlines[pixel_offset] == pixels[i].comp.r);
            CHECK(scanlines[pixel_offset + 1] == pixels[i].comp.g);
            CHECK(scanlines[pixel_offset + 2] == pixels[i].comp.b);
            CHECK(scanlines[pixel_offset + 3] == pixels[i].comp.a);
        }

        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }
}

FO_END_NAMESPACE
