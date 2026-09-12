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

#include "AudioBaker.h"
#include "Test_BakerHelpers.h"

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

FO_BEGIN_NAMESPACE

namespace AudioBakerTests
{
    constexpr uint16_t WAVE_FORMAT_EXTENSIBLE_TAG = 0xFFFE;

    struct DecodedAudio
    {
        int32_t Channels {};
        int32_t SampleRate {};
        vector<int16_t> Samples {};
    };

    struct WavChunk
    {
        string Id {};
        vector<uint8_t> Payload {};
    };

    struct MemoryVorbisReader
    {
        const_span<uint8_t> Data {};
        size_t Pos {};
    };

    static void AppendLe16(vector<uint8_t>& buf, uint16_t value)
    {
        buf.emplace_back(numeric_cast<uint8_t>(value & 0xFFu));
        buf.emplace_back(numeric_cast<uint8_t>((value >> 8u) & 0xFFu));
    }

    static void AppendLe32(vector<uint8_t>& buf, uint32_t value)
    {
        AppendLe16(buf, numeric_cast<uint16_t>(value & 0xFFFFu));
        AppendLe16(buf, numeric_cast<uint16_t>((value >> 16u) & 0xFFFFu));
    }

    static void AppendFourcc(vector<uint8_t>& buf, string_view fourcc)
    {
        REQUIRE(fourcc.size() == 4);

        for (char ch : fourcc) {
            buf.emplace_back(std::bit_cast<uint8_t>(ch));
        }
    }

    static auto MakeFormatChunk(uint16_t tag, uint16_t channels, uint32_t sample_rate, uint16_t bits) -> WavChunk
    {
        uint16_t block_align = numeric_cast<uint16_t>(channels * (bits / 8));

        WavChunk chunk {.Id = "fmt "};
        AppendLe16(chunk.Payload, tag);
        AppendLe16(chunk.Payload, channels);
        AppendLe32(chunk.Payload, sample_rate);
        AppendLe32(chunk.Payload, sample_rate * block_align);
        AppendLe16(chunk.Payload, block_align);
        AppendLe16(chunk.Payload, bits);

        return chunk;
    }

    // A file is assembled chunk by chunk so a test can state the exact layout an exporter produced
    static auto MakeWav(const vector<WavChunk>& chunks) -> vector<uint8_t>
    {
        vector<uint8_t> body;
        AppendFourcc(body, "WAVE");

        for (const WavChunk& chunk : chunks) {
            AppendFourcc(body, chunk.Id);
            AppendLe32(body, numeric_cast<uint32_t>(chunk.Payload.size()));
            body.insert(body.end(), chunk.Payload.begin(), chunk.Payload.end());

            if (chunk.Payload.size() % 2 != 0) {
                body.emplace_back(uint8_t {0});
            }
        }

        vector<uint8_t> wav;
        AppendFourcc(wav, "RIFF");
        AppendLe32(wav, numeric_cast<uint32_t>(body.size()));
        wav.insert(wav.end(), body.begin(), body.end());

        return wav;
    }

    static auto MakeSineFrames(size_t frame_count, int32_t channels, int32_t sample_rate) -> vector<int16_t>
    {
        constexpr float64_t frequency = 440.0;
        vector<int16_t> samples(frame_count * numeric_cast<size_t>(channels));

        for (size_t frame = 0; frame < frame_count; frame++) {
            float64_t phase = 2.0 * std::numbers::pi * frequency * numeric_cast<float64_t>(frame) / numeric_cast<float64_t>(sample_rate);
            auto value = numeric_cast<int16_t>(iround<int32_t>(std::sin(phase) * 20000.0));

            for (int32_t channel = 0; channel < channels; channel++) {
                samples[frame * numeric_cast<size_t>(channels) + numeric_cast<size_t>(channel)] = value;
            }
        }

        return samples;
    }

    static auto MakePcm16DataChunk(const vector<int16_t>& samples) -> WavChunk
    {
        WavChunk chunk {.Id = "data"};

        for (int16_t sample : samples) {
            AppendLe16(chunk.Payload, std::bit_cast<uint16_t>(sample));
        }

        return chunk;
    }

    static auto MakeMetadataChunk(string_view id, size_t size) -> WavChunk
    {
        WavChunk chunk {.Id = string(id)};
        chunk.Payload.assign(size, uint8_t {0x5A});
        return chunk;
    }

    static auto DecodeVorbis(const_span<uint8_t> data) -> DecodedAudio
    {
        MemoryVorbisReader reader {.Data = data};

        ov_callbacks callbacks;

        callbacks.read_func = [](void* output_buf, size_t size, size_t count, void* datasource) -> size_t {
            auto memory = cast_from_void<MemoryVorbisReader*>(datasource);
            size_t bytes_read = std::min(memory->Data.size() - memory->Pos, size * count);

            if (bytes_read > 0) {
                span<uint8_t> output = make_span(output_buf, bytes_read);
                const_span<uint8_t> input = memory->Data.subspan(memory->Pos, bytes_read);
                std::ranges::copy(input, output.begin());
                memory->Pos += bytes_read;
            }

            return bytes_read;
        };

        callbacks.seek_func = [](void* datasource, ogg_int64_t offset, int32_t whence) -> int32_t {
            auto memory = cast_from_void<MemoryVorbisReader*>(datasource);
            int64_t base = 0;

            switch (whence) {
            case SEEK_SET:
                base = 0;
                break;
            case SEEK_CUR:
                base = numeric_cast<int64_t>(memory->Pos);
                break;
            case SEEK_END:
                base = numeric_cast<int64_t>(memory->Data.size());
                break;
            default:
                return -1;
            }

            int64_t target = base + offset;

            if (target < 0 || target > numeric_cast<int64_t>(memory->Data.size())) {
                return -1;
            }

            memory->Pos = numeric_cast<size_t>(target);
            return 0;
        };

        callbacks.close_func = nullptr;

        callbacks.tell_func = [](void* datasource) -> long {
            auto memory = cast_from_void<const MemoryVorbisReader*>(datasource);
            return numeric_cast<long>(memory->Pos);
        };

        OggVorbis_File vorbis_file;
        int32_t open_result = ov_open_callbacks(make_ptr(&reader).void_cast(), &vorbis_file, nullptr, 0, callbacks);
        REQUIRE(open_result == 0);

        auto info = make_nptr(ov_info(&vorbis_file, -1));
        REQUIRE(info);

        DecodedAudio decoded;
        decoded.Channels = info->channels;
        decoded.SampleRate = numeric_cast<int32_t>(info->rate);

        std::array<uint8_t, 4096> buf {};

        while (true) {
            auto output = make_ptr(buf.data()).reinterpret_as<char>();
            long read = ov_read(&vorbis_file, output.get(), numeric_cast<int32_t>(buf.size()), 0, 2, 1, nullptr);
            REQUIRE(read >= 0);

            if (read == 0) {
                break;
            }

            for (size_t i = 0; i + 1 < numeric_cast<size_t>(read); i += 2) {
                decoded.Samples.emplace_back(std::bit_cast<int16_t>(numeric_cast<uint16_t>(buf[i] | (numeric_cast<uint32_t>(buf[i + 1]) << 8))));
            }
        }

        ov_clear(&vorbis_file);

        return decoded;
    }

    static auto MeasureRmsError(const vector<int16_t>& expected, const vector<int16_t>& actual) -> float64_t
    {
        size_t count = std::min(expected.size(), actual.size());
        REQUIRE(count > 0);

        float64_t squared_error = 0.0;

        for (size_t i = 0; i < count; i++) {
            float64_t diff = numeric_cast<float64_t>(expected[i]) - numeric_cast<float64_t>(actual[i]);
            squared_error += diff * diff;
        }

        return std::sqrt(squared_error / numeric_cast<float64_t>(count));
    }
}

TEST_CASE("AudioBaker")
{
    using namespace BakerTests;
    using namespace AudioBakerTests;

    constexpr int32_t sample_rate = 22050;
    constexpr size_t frame_count = 4410;

    SECTION("EncodesWavToVorbisUnderTheAuthoredPath")
    {
        vector<int16_t> samples = MakeSineFrames(frame_count, 1, sample_rate);

        TestRig rig;
        rig.AddSourceFile("Sfx/Shot.wav", MakeWav({MakeFormatChunk(1, 1, sample_rate, 16), MakePcm16DataChunk(samples)}));

        AudioBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.size() == 1);
        REQUIRE(rig.Outputs.contains("Sfx/Shot.wav"));

        const vector<uint8_t>& baked = rig.Outputs.at("Sfx/Shot.wav");
        REQUIRE(baked.size() > 4);
        CHECK(baked[0] == 'O');
        CHECK(baked[1] == 'g');
        CHECK(baked[2] == 'g');
        CHECK(baked[3] == 'S');

        DecodedAudio decoded = DecodeVorbis(baked);
        CHECK(decoded.Channels == 1);
        CHECK(decoded.SampleRate == sample_rate);
        CHECK(decoded.Samples.size() >= samples.size() - sample_rate / 10);

        // A perceptual codec never reproduces samples exactly, so the tone is compared by error energy
        CHECK(MeasureRmsError(samples, decoded.Samples) < 4000.0);
    }

    SECTION("AcceptsMetadataChunksInAnyOrder")
    {
        vector<int16_t> samples = MakeSineFrames(frame_count, 2, sample_rate);

        TestRig rig;
        vector<WavChunk> chunks {
            MakeMetadataChunk("JUNK", 30),
            MakeFormatChunk(1, 2, sample_rate, 16),
            MakePcm16DataChunk(samples),
            MakeMetadataChunk("LIST", 17),
            MakeMetadataChunk("id3 ", 12),
        };
        rig.AddSourceFile("Sfx/Rat.wav", MakeWav(chunks));

        AudioBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.contains("Sfx/Rat.wav"));
        DecodedAudio decoded = DecodeVorbis(rig.Outputs.at("Sfx/Rat.wav"));
        CHECK(decoded.Channels == 2);
        CHECK(decoded.SampleRate == sample_rate);
    }

    SECTION("AcceptsExtensibleAndWideSampleFormats")
    {
        vector<int16_t> samples = MakeSineFrames(frame_count, 1, sample_rate);

        TestRig rig;

        WavChunk extensible = MakeFormatChunk(WAVE_FORMAT_EXTENSIBLE_TAG, 1, sample_rate, 16);
        AppendLe16(extensible.Payload, 22);
        AppendLe16(extensible.Payload, 16);
        AppendLe32(extensible.Payload, 4);
        AppendLe16(extensible.Payload, 1);
        extensible.Payload.insert(extensible.Payload.end(), 14, uint8_t {0});
        rig.AddSourceFile("Sfx/Extensible.wav", MakeWav({extensible, MakePcm16DataChunk(samples)}));

        WavChunk float_data {.Id = "data"};

        for (int16_t sample : samples) {
            AppendLe32(float_data.Payload, std::bit_cast<uint32_t>(numeric_cast<float32_t>(sample) / 32768.0f));
        }

        rig.AddSourceFile("Sfx/Float.wav", MakeWav({MakeFormatChunk(3, 1, sample_rate, 32), float_data}));

        WavChunk wide_data {.Id = "data"};

        for (int16_t sample : samples) {
            uint16_t raw = std::bit_cast<uint16_t>(sample);
            wide_data.Payload.emplace_back(uint8_t {0});
            AppendLe16(wide_data.Payload, raw);
        }

        rig.AddSourceFile("Sfx/Wide.wav", MakeWav({MakeFormatChunk(1, 1, sample_rate, 24), wide_data}));

        AudioBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.size() == 3);

        for (string_view path : {"Sfx/Extensible.wav", "Sfx/Float.wav", "Sfx/Wide.wav"}) {
            REQUIRE(rig.Outputs.contains(string(path)));
            DecodedAudio decoded = DecodeVorbis(rig.Outputs.at(string(path)));
            CHECK(decoded.Channels == 1);
            CHECK(decoded.SampleRate == sample_rate);
            CHECK(MeasureRmsError(samples, decoded.Samples) < 4000.0);
        }
    }

    SECTION("RejectsUnsupportedWavEncoding")
    {
        TestRig rig;
        WavChunk adpcm_data {.Id = "data"};
        adpcm_data.Payload.assign(512, uint8_t {0x11});
        rig.AddSourceFile("Sfx/Adpcm.wav", MakeWav({MakeFormatChunk(2, 1, sample_rate, 4), adpcm_data}));

        AudioBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), AudioBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsWavWithoutSampleData")
    {
        TestRig rig;
        rig.AddSourceFile("Sfx/Empty.wav", MakeWav({MakeFormatChunk(1, 1, sample_rate, 16)}));

        AudioBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), AudioBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("PassesVorbisSourceThroughUnchanged")
    {
        vector<int16_t> samples = MakeSineFrames(frame_count, 1, sample_rate);

        TestRig source_rig;
        source_rig.AddSourceFile("Music/Theme.wav", MakeWav({MakeFormatChunk(1, 1, sample_rate, 16), MakePcm16DataChunk(samples)}));
        AudioBaker source_baker(source_rig.MakeContext());
        source_baker.BakeFiles(source_rig.GetAllSourceFiles(), "");
        vector<uint8_t> vorbis_data = source_rig.Outputs.at("Music/Theme.wav");

        TestRig rig;
        rig.AddSourceFile("Music/Theme.ogg", vorbis_data);

        AudioBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.contains("Music/Theme.ogg"));
        CHECK(rig.Outputs.at("Music/Theme.ogg") == vorbis_data);
    }

    SECTION("RejectsNonVorbisOggSource")
    {
        TestRig rig;
        vector<uint8_t> not_vorbis;
        AppendFourcc(not_vorbis, "OggS");
        not_vorbis.insert(not_vorbis.end(), 512, uint8_t {0x42});
        rig.AddSourceFile("Music/Opus.ogg", not_vorbis);

        AudioBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), AudioBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("EncodingIsDeterministic")
    {
        vector<int16_t> samples = MakeSineFrames(frame_count, 1, sample_rate);
        vector<uint8_t> wav = MakeWav({MakeFormatChunk(1, 1, sample_rate, 16), MakePcm16DataChunk(samples)});

        TestRig first_rig;
        first_rig.AddSourceFile("Sfx/Shot.wav", wav);
        AudioBaker first_baker(first_rig.MakeContext());
        first_baker.BakeFiles(first_rig.GetAllSourceFiles(), "");

        TestRig second_rig;
        second_rig.AddSourceFile("Sfx/Shot.wav", wav);
        AudioBaker second_baker(second_rig.MakeContext());
        second_baker.BakeFiles(second_rig.GetAllSourceFiles(), "");

        CHECK(first_rig.Outputs.at("Sfx/Shot.wav") == second_rig.Outputs.at("Sfx/Shot.wav"));
    }

    SECTION("RegisteredLoaderHandlesItsOwnExtension")
    {
        TestRig rig;
        rig.AddSourceFile("Sfx/Custom.snd", string_view {"custom"});

        AudioBaker baker(rig.MakeContext());
        baker.AddLoader([](string_view, FileReader) -> AudioBaker::PcmAudio { return AudioBaker::PcmAudio {.Channels = 1, .SampleRate = sample_rate, .Samples = MakeSineFrames(frame_count, 1, sample_rate)}; }, {"snd"});
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.contains("Sfx/Custom.snd"));
        DecodedAudio decoded = DecodeVorbis(rig.Outputs.at("Sfx/Custom.snd"));
        CHECK(decoded.Channels == 1);
        CHECK(decoded.SampleRate == sample_rate);
    }

    SECTION("BakesOnlyExplicitTarget")
    {
        vector<int16_t> samples = MakeSineFrames(frame_count, 1, sample_rate);
        vector<uint8_t> wav = MakeWav({MakeFormatChunk(1, 1, sample_rate, 16), MakePcm16DataChunk(samples)});

        TestRig rig;
        rig.AddSourceFile("Sfx/First.wav", wav);
        rig.AddSourceFile("Sfx/Second.wav", wav);

        AudioBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Sfx/Second.wav");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("Sfx/Second.wav"));
    }

    SECTION("SkipsUnrelatedAndMissingTargets")
    {
        TestRig rig;
        rig.AddSourceFile("Sfx/Notes.txt", string_view {"not audio"});

        AudioBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");
        CHECK(rig.Outputs.empty());

        baker.BakeFiles(rig.GetAllSourceFiles(), "Sfx/Notes.txt");
        CHECK(rig.Outputs.empty());

        baker.BakeFiles(rig.GetAllSourceFiles(), "Sfx/Missing.wav");
        CHECK(rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipEncoding")
    {
        vector<int16_t> samples = MakeSineFrames(frame_count, 1, sample_rate);

        TestRig rig;
        rig.AddSourceFile("Sfx/Shot.wav", MakeWav({MakeFormatChunk(1, 1, sample_rate, 16), MakePcm16DataChunk(samples)}));

        AudioBaker baker(rig.MakeContext("TestPack", [](string_view, uint64_t) { return false; }));
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsQualityOutsideTheEncoderRange")
    {
        vector<int16_t> samples = MakeSineFrames(frame_count, 1, sample_rate);

        TestRig rig;
        rig.AddSourceFile("Sfx/Shot.wav", MakeWav({MakeFormatChunk(1, 1, sample_rate, 16), MakePcm16DataChunk(samples)}));
        OverrideSetting(rig.Settings.AudioVorbisQuality, 2.0f);

        AudioBaker baker(rig.MakeContext());

        CHECK_THROWS(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK(rig.Outputs.empty());
    }

    SECTION("SetupBakersReturnsRequestedBaker")
    {
        TestRig rig;
        auto bakers = MakeRequestedBakers({string(AudioBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == AudioBaker::NAME);
        CHECK(bakers.front()->GetOrder() == 4);
    }
}

FO_END_NAMESPACE
