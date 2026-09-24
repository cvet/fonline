//      __________        __     ___________        ______ _____________
//     / ____/ __ \____  / /__  /  _/ ____/ /__    / ____// ____/ ____/ /__
//    / /_  / / / / __ \/ / _ \ / // __/ / / _ \  / __/  / /   / / __/ / _ \
//   / __/ / /_/ / / / / /  __// // /___/ /  __/ / /___ / /___/ /_/ / /  __/
//  /_/    \____/_/ /_/_/\___/___/_____/_/\___/ /_____/ \____/\____/_/\___/
//
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "AudioManager.h"

#include "Application.h"
#include "AudioBaker.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static auto MakeStereoBuffer(const vector<int16_t>& samples) -> vector<uint8_t>
{
    vector<uint8_t> buf;
    buf.resize(samples.size() * sizeof(int16_t));

    for (size_t i = 0; i < samples.size(); i++) {
        auto target = make_ptr(buf.data()).reinterpret_as<int16_t>();
        *target.offset(i) = samples[i];
    }

    return buf;
}

static auto ReadSample(const vector<uint8_t>& buf, size_t index) -> int16_t
{
    auto source = make_ptr(buf.data()).reinterpret_as<const int16_t>();
    return *source.offset(index);
}

TEST_CASE("AudioManagerPan")
{
    SECTION("PanningLeavesTheBufferTheSameSize")
    {
        vector<uint8_t> buf = MakeStereoBuffer({1000, -2000, 3000, -4000});
        AudioManager::ApplyPan(buf, -1.0f);

        CHECK(buf.size() == 4 * sizeof(int16_t));
    }

    SECTION("ACentredSoundPassesThroughUntouched")
    {
        vector<uint8_t> buf = MakeStereoBuffer({10000, -9000});
        AudioManager::ApplyPan(buf, 0.0f);

        CHECK(ReadSample(buf, 0) == 10000);
        CHECK(ReadSample(buf, 1) == -9000);
    }

    SECTION("HardLeftSilencesTheRightChannelAndKeepsTheLeftWhole")
    {
        vector<uint8_t> buf = MakeStereoBuffer({10000, 10000});
        AudioManager::ApplyPan(buf, -1.0f);

        CHECK(ReadSample(buf, 0) == 10000);
        CHECK(ReadSample(buf, 1) == 0);
    }

    SECTION("HardRightIsTheMirrorOfHardLeft")
    {
        vector<uint8_t> buf = MakeStereoBuffer({10000, 10000});
        AudioManager::ApplyPan(buf, 1.0f);

        CHECK(ReadSample(buf, 0) == 0);
        CHECK(ReadSample(buf, 1) == 10000);
    }

    SECTION("PartialPanFadesTheFarChannelWithoutEmptyingIt")
    {
        vector<uint8_t> buf = MakeStereoBuffer({10000, 10000});
        AudioManager::ApplyPan(buf, -0.5f);

        CHECK(ReadSample(buf, 0) == 10000);
        CHECK(ReadSample(buf, 1) == Catch::Approx(5000).margin(2));
    }

    SECTION("NoGainEverRisesAboveUnitySoALoudSampleCannotClip")
    {
        vector<uint8_t> buf = MakeStereoBuffer({32767, -32768, 32767, -32768});
        AudioManager::ApplyPan(buf, -1.0f);

        CHECK(ReadSample(buf, 0) == 32767);
        CHECK(ReadSample(buf, 1) == 0);
        CHECK(ReadSample(buf, 2) == 32767);
        CHECK(ReadSample(buf, 3) == 0);
    }

    SECTION("EveryFrameIsPannedNotJustTheFirst")
    {
        vector<uint8_t> buf = MakeStereoBuffer({8000, 8000, 6000, 6000, 4000, 4000});
        AudioManager::ApplyPan(buf, 1.0f);

        CHECK(ReadSample(buf, 0) == 0);
        CHECK(ReadSample(buf, 2) == 0);
        CHECK(ReadSample(buf, 4) == 0);
        CHECK(ReadSample(buf, 5) == 4000);
    }

    SECTION("AnEmptyBufferStaysEmpty")
    {
        vector<uint8_t> buf;
        AudioManager::ApplyPan(buf, -1.0f);

        CHECK(buf.empty());
    }
}

namespace AudioManagerDynamicTests
{
    constexpr int32_t SAMPLE_RATE = 22050;
    constexpr size_t FRAME_COUNT = 2205;
    constexpr size_t MIX_BUFFER_SIZE = 4096;
    constexpr int32_t MAX_PULLS = 64;

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
        for (char ch : fourcc) {
            buf.emplace_back(std::bit_cast<uint8_t>(ch));
        }
    }

    // A tenth of a second of stereo tone, the same in both channels, so a lean to one side reads as one
    // channel falling quiet while the other keeps its energy
    static auto MakeStereoToneWav() -> vector<uint8_t>
    {
        vector<uint8_t> samples;

        for (size_t frame = 0; frame < FRAME_COUNT; frame++) {
            float64_t phase = 2.0 * std::numbers::pi * 440.0 * numeric_cast<float64_t>(frame) / numeric_cast<float64_t>(SAMPLE_RATE);
            int16_t value = numeric_cast<int16_t>(iround<int32_t>(std::sin(phase) * 20000.0));

            AppendLe16(samples, std::bit_cast<uint16_t>(value));
            AppendLe16(samples, std::bit_cast<uint16_t>(value));
        }

        vector<uint8_t> body;
        AppendFourcc(body, "WAVE");
        AppendFourcc(body, "fmt ");
        AppendLe32(body, 16);
        AppendLe16(body, 1);
        AppendLe16(body, 2);
        AppendLe32(body, numeric_cast<uint32_t>(SAMPLE_RATE));
        AppendLe32(body, numeric_cast<uint32_t>(SAMPLE_RATE * 4));
        AppendLe16(body, 4);
        AppendLe16(body, 16);
        AppendFourcc(body, "data");
        AppendLe32(body, numeric_cast<uint32_t>(samples.size()));
        body.insert(body.end(), samples.begin(), samples.end());

        vector<uint8_t> wav;
        AppendFourcc(wav, "RIFF");
        AppendLe32(wav, numeric_cast<uint32_t>(body.size()));
        wav.insert(wav.end(), body.begin(), body.end());

        return wav;
    }

    // The mixer reads Ogg Vorbis and nothing else, so the fixture goes in through the baker that produces it
    // rather than through a hand-written stream
    static auto BakeToVorbis(const vector<uint8_t>& wav) -> vector<uint8_t>
    {
        BakerTests::TestRig rig;
        rig.AddSourceFile("Sfx/Test_Tone.wav", wav);

        AudioBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.contains("Sfx/Test_Tone.wav"));

        return rig.Outputs.at("Sfx/Test_Tone.wav");
    }

    static auto ChannelEnergy(const vector<uint8_t>& buf, size_t channel) -> int64_t
    {
        size_t frame_count = buf.size() / (sizeof(int16_t) * 2);
        auto samples = make_ptr(buf.data()).reinterpret_as<const int16_t>();
        int64_t energy = 0;

        for (size_t i = 0; i < frame_count; i++) {
            energy += std::abs(numeric_cast<int64_t>(*samples.offset(i * 2 + channel)));
        }

        return energy;
    }

    // Stands in for the sound card: it reports itself enabled, keeps the stream callback so the test decides
    // when a buffer is asked for, and records what it was handed to mix, which is where a sound's pan shows up
    class TestAudioDevice final : public IAppAudio
    {
    public:
        [[nodiscard]] auto IsEnabled() const -> bool override { return true; }

        auto ConvertAudio(int32_t channels, int32_t rate, vector<uint8_t>& buf) -> bool override
        {
            // The fixture is authored in the mixing layout already, so there is nothing to convert
            CHECK(channels == 2);
            CHECK(rate == SAMPLE_RATE);
            ignore_unused(buf);

            return true;
        }

        void SetSource(AudioStreamCallback stream_callback) override { _streamCallback = std::move(stream_callback); }

        void MixAudio(span<uint8_t> output, const_span<uint8_t> buf, int32_t volume) override
        {
            ignore_unused(output);
            MixedVolume = volume;
            Mixed.assign(buf.begin(), buf.end());
        }

        void LockDevice() override { }
        void UnlockDevice() override { }

        // Runs one device callback and hands back the buffer the mixer produced for the playing sound
        auto PullMixedBuffer() -> vector<uint8_t>
        {
            FO_VERIFY_AND_THROW(_streamCallback, "Audio source is not set");
            Mixed.clear();
            MixedVolume = 0;

            vector<uint8_t> output;
            output.resize(MIX_BUFFER_SIZE);
            _streamCallback(uint8_t {0}, span<uint8_t> {output.data(), output.size()});

            return Mixed;
        }

        vector<uint8_t> Mixed {};
        int32_t MixedVolume {};

    private:
        AudioStreamCallback _streamCallback {};
    };
}

TEST_CASE("AudioManagerDynamicPlacement")
{
    using namespace AudioManagerDynamicTests;

    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();

    FileSystem resources;
    auto data_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("AudioManagerResources");
    data_source->AddFile("Sfx/Test_Tone.wav", BakeToVorbis(MakeStereoToneWav()));
    resources.AddCustomSource(std::move(data_source));

    TestAudioDevice device;
    AudioManager audio(&settings, &resources, &device);

    uint32_t sound_id = audio.PlaySound("Sfx/Test_Tone.wav", 1.0f, -1.0f);
    REQUIRE(sound_id != 0);

    SECTION("APlacedSoundIsMixedWhereItWasPlaced")
    {
        vector<uint8_t> mixed = device.PullMixedBuffer();

        REQUIRE(!mixed.empty());
        CHECK(ChannelEnergy(mixed, 0) > 0);
        CHECK(ChannelEnergy(mixed, 1) == 0);
    }

    SECTION("AnUpdateMovesASoundThatIsAlreadyPlaying")
    {
        // The whole fixture decodes in one go, so a pan baked into the decoded buffer could never follow the
        // camera: this is the mirror of the placement above, taken from the same sound
        CHECK(audio.UpdateSound(sound_id, 1.0f, 1.0f));

        vector<uint8_t> mixed = device.PullMixedBuffer();

        REQUIRE(!mixed.empty());
        CHECK(ChannelEnergy(mixed, 0) == 0);
        CHECK(ChannelEnergy(mixed, 1) > 0);
    }

    SECTION("AnUpdateCarriesTheAttenuationAsWell")
    {
        CHECK(audio.UpdateSound(sound_id, 0.5f, 0.0f));
        (void)device.PullMixedBuffer();

        CHECK(device.MixedVolume == audio.GetSoundVolume() / 2);
    }

    SECTION("AFinishedSoundStopsAnsweringItsHandle")
    {
        bool still_playing = true;

        for (int32_t i = 0; i < MAX_PULLS && still_playing; i++) {
            (void)device.PullMixedBuffer();
            still_playing = audio.UpdateSound(sound_id, 1.0f, 0.0f);
        }

        CHECK(!still_playing);
    }

    SECTION("AHandleNothingIsPlayingUnderIsRefused")
    {
        CHECK(!audio.UpdateSound(sound_id + 1, 1.0f, 0.0f));
        CHECK(!audio.UpdateSound(0, 1.0f, 0.0f));
    }
}

FO_END_NAMESPACE
