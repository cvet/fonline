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
//

#include "catch_amalgamated.hpp"

#include "Common.h"

FO_BEGIN_NAMESPACE

TEST_CASE("Compressor")
{
    SECTION("MaxCompressedSize")
    {
        CHECK(Compressor::CalculateMaxCompressedBufSize(0) >= 12);
        CHECK(Compressor::CalculateMaxCompressedBufSize(100) >= 100);
    }

    SECTION("CompressDecompressRoundtrip")
    {
        vector<uint8> src;
        src.reserve(4096);
        for (auto i = 0; i < 4096; i++) {
            src.emplace_back(static_cast<uint8>(i % 251));
        }

        const auto compressed = Compressor::Compress(src);
        CHECK_FALSE(compressed.empty());

        const auto restored = Compressor::Decompress(compressed, 2);
        CHECK(restored == src);
    }

    SECTION("DecompressInvalidData")
    {
        const vector<uint8> invalid = {0x01, 0x02, 0x03, 0x04, 0x05};
        CHECK_THROWS_AS((Compressor::Decompress(invalid, 2)), DecompressException);
    }

    SECTION("DecompressExpandsBufferWhenApproximationIsTooSmall")
    {
        vector<uint8> src(4096, 0x2A);

        const auto compressed = Compressor::Compress(src);
        REQUIRE(compressed.size() < src.size());

        const auto restored = Compressor::Decompress(compressed, 1);
        CHECK(restored == src);
    }

    SECTION("StreamRoundtrip")
    {
        vector<uint8> part1;
        vector<uint8> part2;
        for (auto i = 0; i < 128; i++) {
            part1.emplace_back(static_cast<uint8>(i));
            part2.emplace_back(static_cast<uint8>(255 - i));
        }

        StreamCompressor compressor;
        vector<uint8> comp1;
        vector<uint8> comp2;
        compressor.Compress(part1, comp1);
        compressor.Compress(part2, comp2);

        StreamDecompressor decompressor;
        vector<uint8> dec1;
        vector<uint8> dec2;
        decompressor.Decompress(comp1, dec1);
        decompressor.Decompress(comp2, dec2);

        CHECK(dec1 == part1);
        CHECK(dec2 == part2);

        compressor.Reset();
        decompressor.Reset();
    }
}

FO_END_NAMESPACE
