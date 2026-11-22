// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

FO_BEGIN_NAMESPACE

TEST_CASE("Compressor")
{
    SECTION("MaxCompressedSize")
    {
        CHECK(compressor::calculate_max_compressed_buf_size(0) >= 12);
        CHECK(compressor::calculate_max_compressed_buf_size(100) >= 100);
    }

    SECTION("CompressDecompressRoundtrip")
    {
        vector<uint8_t> src {};
        src.reserve(4096);

        for (int32_t i {0}; i < 4096; i++) {
            src.emplace_back(static_cast<uint8_t>(i % 251));
        }

        auto compressed {compressor::compress(src)};
        CHECK_FALSE(compressed.empty());

        auto restored {compressor::decompress(compressed, 2)};
        CHECK(restored == src);
    }

    SECTION("DecompressInvalidData")
    {
        vector<uint8_t> invalid {0x01, 0x02, 0x03, 0x04, 0x05};
        CHECK_THROWS_AS((compressor::decompress(invalid, 2)), DecompressException);
    }

    SECTION("DecompressExpandsBufferWhenApproximationIsTooSmall")
    {
        vector<uint8_t> src {vector<uint8_t>(4096, 0x2A)};

        auto compressed {compressor::compress(src)};
        REQUIRE(compressed.size() < src.size());

        auto restored {compressor::decompress(compressed, 1)};
        CHECK(restored == src);
    }

    SECTION("StreamRoundtrip")
    {
        vector<uint8_t> part1 {};
        vector<uint8_t> part2 {};

        for (int32_t i {0}; i < 128; i++) {
            part1.emplace_back(static_cast<uint8_t>(i));
            part2.emplace_back(static_cast<uint8_t>(255 - i));
        }

        stream_compressor stream {};
        vector<uint8_t> comp1 {};
        vector<uint8_t> comp2 {};
        stream.compress(part1, comp1);
        stream.compress(part2, comp2);

        stream_decompressor decompressor {};
        vector<uint8_t> dec1 {};
        vector<uint8_t> dec2 {};
        decompressor.decompress(comp1, dec1);
        decompressor.decompress(comp2, dec2);

        CHECK(dec1 == part1);
        CHECK(dec2 == part2);

        stream.reset();
        decompressor.reset();
    }

    SECTION("StreamRejectsMalformedInitialInputWithTypedException")
    {
        stream_decompressor decompressor {};
        vector<uint8_t> result {};
        vector<uint8_t> invalid {0x00, 0x00};

        CHECK_THROWS_AS(decompressor.decompress(invalid, result), DecompressException);
    }

    SECTION("StreamRejectsMalformedContinuationInputWithTypedException")
    {
        vector<uint8_t> source {vector<uint8_t>(4096, 0x2A)};
        stream_compressor stream {};
        vector<uint8_t> compressed {};
        stream.compress(source, compressed);
        REQUIRE(compressed.size() * 2 < source.size());

        compressed.emplace_back(0x06);

        stream_decompressor decompressor {};
        vector<uint8_t> result {};
        CHECK_THROWS_AS(decompressor.decompress(compressed, result), DecompressException);
    }
}

FO_END_NAMESPACE
