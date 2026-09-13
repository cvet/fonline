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

#include "Containers.h"
#include "ExceptionHandling.h"
#include "RandomGenerator.h"

FO_BEGIN_NAMESPACE

TEST_CASE("RandomGenerator")
{
    SECTION("StateIsTheAdvertisedSize")
    {
        // The point of the type: the standard Mersenne engine spends 5000 bytes on the same job
        STATIC_REQUIRE(sizeof(random_generator) == 4 * sizeof(uint64_t));
    }

    SECTION("SeededSequenceIsPinned")
    {
        // Pinned so the sequence cannot drift between platforms, standard library versions, or refactors —
        // std::uniform_int_distribution is exactly what could not be pinned this way
        random_generator generator {42};

        CHECK(generator.next() == 0xD0764D4F4476689FULL);
        CHECK(generator.next() == 0x519E4174576F3791ULL);
        CHECK(generator.next() == 0xFBE07CFB0C24ED8CULL);
        CHECK(generator.next() == 0xB37D9F600CD835B8ULL);
        CHECK(generator.next() == 0xCB231C3874846A73ULL);
        CHECK(generator.next() == 0x968D9F004E50DE7DULL);

        random_generator bounded {42};
        vector<uint32_t> drawn;

        for (int32_t i = 0; i < 8; i++) {
            drawn.emplace_back(bounded.next_below(100));
        }

        CHECK(drawn == vector<uint32_t>({26, 34, 4, 5, 45, 30, 13, 5}));
    }

    SECTION("SameSeedRepeatsAndReseedRewinds")
    {
        random_generator first {12345};
        random_generator second {12345};

        for (int32_t i = 0; i < 100; i++) {
            CHECK(first.next() == second.next());
        }

        first.seed(12345);
        second.seed(12345);
        CHECK(first.next() == second.next());
    }

    SECTION("DifferentSeedsDiverge")
    {
        random_generator first {1};
        random_generator second {2};
        int32_t equal_draws = 0;

        for (int32_t i = 0; i < 64; i++) {
            if (first.next() == second.next()) {
                equal_draws++;
            }
        }

        CHECK(equal_draws == 0);
    }

    SECTION("BoundedDrawsStayInRange")
    {
        random_generator generator {7};

        for (int32_t i = 0; i < 20000; i++) {
            uint32_t below = generator.next_below(37);
            CHECK(below < 37);

            int32_t between = generator.next_between(-5, 5);
            CHECK(between >= -5);
            CHECK(between <= 5);
        }

        CHECK(generator.next_below(1) == 0);
        CHECK(generator.next_between(3, 3) == 3);
    }

    SECTION("BoundedDrawsCoverTheWholeRangeEvenly")
    {
        random_generator generator {99};
        std::array<int32_t, 10> buckets {};

        for (int32_t i = 0; i < 200000; i++) {
            buckets[static_cast<size_t>(generator.next_between(0, 9))]++;
        }

        for (int32_t count : buckets) {
            CHECK(count > 19000);
            CHECK(count < 21000);
        }
    }

    SECTION("NormalizedDrawStaysInsideTheUnitInterval")
    {
        random_generator generator {3};
        float64_t sum = 0.0;

        for (int32_t i = 0; i < 100000; i++) {
            float64_t value = generator.next_normalized();

            REQUIRE(value >= 0.0);
            REQUIRE(value < 1.0);

            sum += value;
        }

        CHECK(sum / 100000.0 > 0.49);
        CHECK(sum / 100000.0 < 0.51);
    }

    SECTION("InvalidRangesAreRejected")
    {
        random_generator generator {1};

        CHECK_THROWS_AS(generator.next_below(0), VerificationException);
        CHECK_THROWS_AS(generator.next_between(5, 4), VerificationException);
    }

    SECTION("DefaultConstructedGeneratorsDiverge")
    {
        random_generator first;
        random_generator second;

        CHECK(first.next() != second.next());
    }
}

TEST_CASE("RandomGeneratorStateCaptureAndRestore")
{
    SECTION("Restored state replays the exact sequence")
    {
        random_generator generator {2026};
        random_generator::state_data state = generator.capture_state();

        vector<uint64_t> expected;
        for (size_t i = 0; i < 16; i++) {
            expected.emplace_back(generator.next());
        }

        random_generator restored {1};
        restored.restore_state(state);

        for (uint64_t expected_value : expected) {
            CHECK(restored.next() == expected_value);
        }
    }

    SECTION("Capture reflects consumption")
    {
        random_generator generator {7};
        random_generator::state_data before = generator.capture_state();
        (void)generator.next();

        CHECK(generator.capture_state() != before);
    }

    SECTION("All-zero state is rejected and leaves the generator untouched")
    {
        random_generator generator {11};
        random_generator::state_data before = generator.capture_state();

        CHECK_THROWS(generator.restore_state(random_generator::state_data {}));
        CHECK(generator.capture_state() == before);
    }
}

FO_END_NAMESPACE
