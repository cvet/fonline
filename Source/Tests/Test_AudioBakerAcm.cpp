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

#include "AudioBaker-Acm.h"

#if FO_AUDIO_ACM

FO_BEGIN_NAMESPACE

using namespace AudioBakerAcm;

// Synthetic streams pin the original unpacker's output across every coding and transform branch

// Block of eight columns, one row: first level with two rows, next level with four
static constexpr std::array<uint8_t, 69> STREAM_TWO_ROW_LEVELS = {0x97, 0x28, 0x03, 0x01, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x22, 0x56, 0x13, 0x00, 0x05, 0x01, //
    0x00, 0x06, 0x48, 0x15, 0x68, 0x88, 0x83, 0x42, 0xE8, 0xC9, 0x62, 0xA7, 0x02, 0x28, 0x8B, 0xD7, //
    0xB4, 0x59, 0x99, 0x34, 0xCE, 0xE6, 0xF0, 0x85, 0x37, 0x97, 0x86, 0xC9, 0x66, 0xF1, 0xE1, 0x0C, //
    0x20, 0xCD, 0x44, 0xAF, 0xB2, 0xCC, 0xFB, 0xF1, 0x68, 0xC1, 0x0C, 0xB0, 0xB7, 0x53, 0xC0, 0x18, //
    0xC9, 0x32, 0x4C, 0x7F, 0x1C};

// Block of sixteen columns, three rows: the first level opens with a lone pair before its quads
static constexpr std::array<uint8_t, 169> STREAM_ODD_ROW_LEVELS = {0x97, 0x28, 0x03, 0x01, 0x90, 0x00, 0x00, 0x00, 0x01, 0x00, 0x22, 0x56, 0x34, 0x00, 0x2E, 0x0D, //
    0x00, 0xC6, 0x12, 0x42, 0x54, 0x65, 0x9E, 0x60, 0xAC, 0xAF, 0x3B, 0xD7, 0x80, 0x00, 0x91, 0x8B, //
    0x0B, 0xA7, 0xD3, 0x56, 0x9A, 0xAB, 0xDA, 0xF9, 0x86, 0x75, 0x99, 0xAB, 0x46, 0x2B, 0x60, 0xAE, //
    0x70, 0xB3, 0x5F, 0xD6, 0x42, 0xE5, 0xB2, 0x68, 0xAC, 0xEE, 0x21, 0x49, 0xA4, 0x8A, 0xFE, 0x37, //
    0xD7, 0x0A, 0xE7, 0x47, 0xA9, 0xA1, 0x05, 0x94, 0xCD, 0x0A, 0x8C, 0x74, 0xDC, 0x3D, 0x25, 0x80, //
    0xB4, 0xF9, 0xE8, 0xFF, 0xD7, 0x97, 0xD5, 0xDE, 0xDB, 0xE0, 0xDF, 0xD5, 0xEC, 0x6D, 0x9D, 0x1D, //
    0x04, 0x03, 0x3D, 0xB1, 0xF8, 0xB2, 0x95, 0x35, 0x42, 0xEA, 0x9D, 0x41, 0x9C, 0xC6, 0x75, 0x00, //
    0x28, 0x31, 0x33, 0x3A, 0x55, 0xB5, 0x44, 0x43, 0x21, 0x13, 0x33, 0xE9, 0x2B, 0x34, 0x8F, 0xAF, //
    0x14, 0xF3, 0xDF, 0x9D, 0x85, 0xF2, 0xD6, 0x63, 0x62, 0xFB, 0x87, 0x72, 0xF8, 0x2D, 0xA9, 0x9C, //
    0x58, 0xBF, 0x94, 0xFC, 0x8C, 0xF7, 0x80, 0x04, 0x23, 0xC2, 0xEB, 0xC9, 0xD9, 0xEA, 0xE2, 0x96, //
    0xE4, 0x39, 0xE8, 0xBD, 0xEA, 0xB1, 0xF5, 0x6B, 0x01};

// Declares four blocks and carries two: the rest reads as zero bits
static constexpr std::array<uint8_t, 51> STREAM_TRUNCATED = {0x97, 0x28, 0x03, 0x01, 0x50, 0x00, 0x00, 0x00, 0x01, 0x00, 0x22, 0x56, 0x52, 0x00, 0x6B, 0x00, //
    0x00, 0x46, 0x21, 0x8A, 0x4C, 0x90, 0x5F, 0x21, 0x9A, 0x9A, 0x14, 0x0B, 0x01, 0xA6, 0x51, 0x4E, //
    0x05, 0x39, 0x54, 0x6B, 0x58, 0x3E, 0x47, 0x9B, 0x3B, 0x88, 0xB3, 0xD1, 0x69, 0x80, 0xF1, 0x6F, //
    0xD6, 0xCC, 0x03};

// One column per block: the values come out of the amplitude table untransformed
static constexpr std::array<uint8_t, 32> STREAM_UNTRANSFORMED = {0x97, 0x28, 0x03, 0x01, 0x1B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x22, 0x56, 0x90, 0x00, 0x85, 0x0C, //
    0x00, 0xBE, 0x1F, 0x60, 0x8C, 0x46, 0x7D, 0x40, 0xD0, 0x00, 0xC8, 0x23, 0xAB, 0x00, 0x4E, 0x03};

static auto HashSamples(const vector<int16_t>& samples) -> uint64_t
{
    uint64_t hash = 1469598103934665603ull;

    for (int16_t sample : samples) {
        hash ^= static_cast<uint16_t>(sample);
        hash *= 1099511628211ull;
    }

    return hash;
}

template<size_t N>
static auto DecodeStream(const std::array<uint8_t, N>& stream) -> vector<int16_t>
{
    Decoder decoder {const_span<uint8_t> {stream.data(), stream.size()}};
    CHECK(decoder.GetChannels() == 1);
    CHECK(decoder.GetSampleRate() == 22050);

    vector<int16_t> samples = decoder.Decode();
    CHECK(numeric_cast<int32_t>(samples.size()) == decoder.GetSampleCount());
    return samples;
}

TEST_CASE("AudioBakerAcm: decoder reproduces the original unpacker")
{
    SECTION("TwoRowLevels")
    {
        vector<int16_t> samples = DecodeStream(STREAM_TWO_ROW_LEVELS);
        REQUIRE(samples.size() == 32);
        CHECK(vector<int16_t>(samples.begin(), samples.begin() + 8) == vector<int16_t> {0, 8, -10, 36, 18, 80, 222, 128});
        CHECK(HashSamples(samples) == 0xF0F7592702AF4584ull);
    }

    SECTION("OddRowLevels")
    {
        vector<int16_t> samples = DecodeStream(STREAM_ODD_ROW_LEVELS);
        REQUIRE(samples.size() == 144);
        CHECK(vector<int16_t>(samples.begin(), samples.begin() + 8) == vector<int16_t> {0, 13, 26, 184, -53, 381, 1234, 6406});
        CHECK(HashSamples(samples) == 0x6B934813C5813382ull);
    }

    SECTION("TruncatedStreamReadsZeros")
    {
        vector<int16_t> samples = DecodeStream(STREAM_TRUNCATED);
        REQUIRE(samples.size() == 80);
        CHECK(HashSamples(samples) == 0xD494BAAEE5D6A562ull);
    }

    SECTION("Untransformed")
    {
        vector<int16_t> samples = DecodeStream(STREAM_UNTRANSFORMED);
        REQUIRE(samples.size() == 27);
        CHECK(vector<int16_t>(samples.begin(), samples.begin() + 12) == vector<int16_t> {0, 0, 0, 0, 0, 0, 0, 0, 0, -253, 0, 506});
        CHECK(HashSamples(samples) == 0xBFCBB04DA876E337ull);
    }
}

TEST_CASE("AudioBakerAcm: decoder rejects what it cannot decode")
{
    SECTION("NotAcm")
    {
        std::array<uint8_t, 16> wav = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' '};
        CHECK_THROWS_AS(Decoder(const_span<uint8_t> {wav.data(), wav.size()}), DecoderException);
    }

    SECTION("UndefinedColumnCoding")
    {
        // The undefined 5-bit column coding starts at bit 4 of byte 16, after the header and amplitude key
        std::array<uint8_t, 32> stream = STREAM_UNTRANSFORMED;
        REQUIRE((stream[16] & 0xF0) == 0);
        REQUIRE((stream[17] & 0x01) == 0);
        stream[16] = numeric_cast<uint8_t>(stream[16] | 0x10);

        Decoder decoder {const_span<uint8_t> {stream.data(), stream.size()}};
        CHECK_THROWS_AS(decoder.Decode(), DecoderException);
    }
}

TEST_CASE("AudioBakerAcm: loader follows the Fallout channel convention")
{
    SECTION("EffectIsMonoWhateverTheHeaderSays")
    {
        // Channel count field set to stereo, as hundreds of the Fallout effect headers have it over mono data
        std::array<uint8_t, 32> stream = STREAM_UNTRANSFORMED;
        stream[8] = 2;

        AudioBaker::PcmAudio pcm = Load("sound/SFX/SYNTHETIC.ACM", FileReader {const_span<uint8_t> {stream.data(), stream.size()}});
        CHECK(pcm.Channels == 1);
        CHECK(pcm.SampleRate == 22050);
        CHECK(pcm.Samples.size() == 27);
    }

    SECTION("MusicIsStereo")
    {
        AudioBaker::PcmAudio pcm = Load("Sound/Music/synthetic.acm", FileReader {const_span<uint8_t> {STREAM_TWO_ROW_LEVELS.data(), STREAM_TWO_ROW_LEVELS.size()}});
        CHECK(pcm.Channels == 2);
        CHECK(pcm.Samples.size() == 32);
    }

    SECTION("RejectsEmptySampleDataWithTheFilePath")
    {
        std::array<uint8_t, 32> stream = STREAM_UNTRANSFORMED;
        std::fill(stream.begin() + 4, stream.begin() + 8, uint8_t {0});

        for (string_view path : {"sound/SFX/EMPTY.ACM", "Sound/Music/Empty.acm"}) {
            CHECK_THROWS_WITH(Load(path, FileReader {const_span<uint8_t> {stream.data(), stream.size()}}), Catch::Matchers::ContainsSubstring(std::string {path}));
        }
    }

    SECTION("FailureNamesTheFile")
    {
        std::array<uint8_t, 4> garbage = {1, 2, 3, 4};

        try {
            (void)Load("sound/SFX/BROKEN.ACM", FileReader {const_span<uint8_t> {garbage.data(), garbage.size()}});
            FAIL("Decoding garbage must throw");
        }
        catch (const DecoderException& ex) {
            CHECK(string(ex.what()).find("sound/SFX/BROKEN.ACM") != string::npos);
        }
    }
}

FO_END_NAMESPACE

#endif
