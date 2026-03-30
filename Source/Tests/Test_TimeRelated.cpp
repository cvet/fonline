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

TEST_CASE("TimeRelated")
{
    SECTION("TimespanMath")
    {
        const timespan a {std::chrono::milliseconds {1500}};
        const timespan b {std::chrono::milliseconds {500}};

        CHECK((a + b).milliseconds() == 2000);
        CHECK((a - b).milliseconds() == 1000);
        CHECK(a.to_ms<int32>() == 1500);
        CHECK(a.to_sec<int32>() == 1);
        CHECK(is_float_equal(a.div<float32>(b), 3.0f));
        CHECK(a > b);
        CHECK(timespan::zero == timespan {});
    }

    SECTION("NanoAndSyncTime")
    {
        const nanotime n1 {timespan {std::chrono::seconds {2}}};
        const nanotime n2 {timespan {std::chrono::seconds {5}}};
        const timespan delta = n2 - n1;
        CHECK(delta.seconds() == 3);

        const synctime s1 {timespan {std::chrono::milliseconds {100}}};
        const synctime s2 = s1 + timespan {std::chrono::milliseconds {250}};
        CHECK((s2 - s1).milliseconds() == 250);
        CHECK(s2 > s1);
        CHECK(synctime::zero == synctime {});
    }

    SECTION("OffsetAndDescRoundtrip")
    {
        const auto now_desc = nanotime::now().desc(false);
        const auto off = make_time_offset( //
            now_desc.year, //
            now_desc.month, //
            now_desc.day, //
            now_desc.hour, //
            now_desc.minute, //
            now_desc.second, //
            now_desc.millisecond, //
            now_desc.microsecond, //
            now_desc.nanosecond, //
            false);

        const auto back = make_time_desc(off, false);

        CHECK(back.year == now_desc.year);
        CHECK(back.month == now_desc.month);
        CHECK(back.day == now_desc.day);
        CHECK(back.hour == now_desc.hour);
        CHECK(back.minute == now_desc.minute);
        CHECK(std::abs(back.second - now_desc.second) <= 1);
        CHECK(back.month >= 1);
        CHECK(back.month <= 12);
        CHECK(back.day >= 1);
        CHECK(back.day <= 31);
    }

    SECTION("TimeMeterPauseResume")
    {
        TimeMeter meter;
        std::this_thread::sleep_for(std::chrono::milliseconds {5});

        meter.Pause();
        const auto paused = meter.GetDuration().milliseconds();

        std::this_thread::sleep_for(std::chrono::milliseconds {5});
        const auto paused_after_wait = meter.GetDuration().milliseconds();
        CHECK(paused_after_wait == paused);

        meter.Resume();
        std::this_thread::sleep_for(std::chrono::milliseconds {5});
        CHECK(meter.GetDuration().milliseconds() >= paused);
    }

    SECTION("TimeMeterResumeWithoutPause")
    {
        TimeMeter meter;
        std::this_thread::sleep_for(std::chrono::milliseconds {5});

        const auto before_resume = meter.GetDuration().milliseconds();
        meter.Resume();

        std::this_thread::sleep_for(std::chrono::milliseconds {5});
        const auto after_resume = meter.GetDuration().milliseconds();

        CHECK(before_resume >= 1);
        CHECK(after_resume >= before_resume);
    }

    SECTION("TimeMeterPauseIsIdempotent")
    {
        TimeMeter meter;
        std::this_thread::sleep_for(std::chrono::milliseconds {5});

        meter.Pause();
        const auto first_pause = meter.GetDuration().milliseconds();

        std::this_thread::sleep_for(std::chrono::milliseconds {5});
        meter.Pause();
        const auto second_pause = meter.GetDuration().milliseconds();

        CHECK(second_pause == first_pause);
    }

    SECTION("DurationFormatterBranches")
    {
        const auto us_text = std::format("{}", timespan {std::chrono::nanoseconds {1234}});
        CHECK(us_text == "1.234 us");

        const auto ms_text = std::format("{}", timespan {std::chrono::microseconds {1234}});
        CHECK(ms_text == "1.234 ms");

        const auto sec_text = std::format("{}", timespan {std::chrono::milliseconds {1234}});
        CHECK(sec_text == "1.234 sec");

        const auto hms_text = std::format("{}", timespan {std::chrono::seconds {3661}});
        CHECK(hms_text == "01:01:01 sec");

        const auto day_text = std::format("{}", timespan {std::chrono::hours {49}});
        CHECK(day_text == "2 days 01:00:00 sec");
    }

    SECTION("TimespanAndSynctimeFormatter")
    {
        const auto ts_text = std::format("{}", timespan {std::chrono::milliseconds {1500}});
        CHECK(ts_text == "1.500 sec");

        const auto st_text = std::format("{}", synctime {timespan {std::chrono::milliseconds {2500}}});
        CHECK(st_text == "2.500 sec");
    }

    SECTION("InvalidDateInput")
    {
        CHECK_THROWS_AS((make_time_offset(2026, 13, 1, 0, 0, 0, 0, 0, 0, false)), GenericException);
        CHECK_THROWS_AS((make_time_offset(2026, 2, 30, 0, 0, 0, 0, 0, 0, false)), GenericException);
    }
}

FO_END_NAMESPACE
