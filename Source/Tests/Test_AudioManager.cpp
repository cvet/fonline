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

FO_END_NAMESPACE
