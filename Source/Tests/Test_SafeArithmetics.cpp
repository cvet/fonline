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

TEST_CASE("SafeArithmetics")
{
    SECTION("FloatCompare")
    {
        CHECK(is_float_equal(1.0f, 1.0f + 1.0e-7f));
        CHECK_FALSE(is_float_equal(1.0f, 1.1f));
    }

    SECTION("NumericCast")
    {
        CHECK(numeric_cast<int32>(uint16 {123}) == 123);
        CHECK(clamp_to<uint8>(int32 {-1}) == 0);
        CHECK(clamp_to<uint8>(int32 {300}) == 255);
        CHECK(clamp_to<int8>(int32 {127}) == 127);
        CHECK(clamp_to<int8>(int32 {128}) == std::numeric_limits<int8>::max());
        CHECK(clamp_to<int8>(int32 {-129}) == std::numeric_limits<int8>::min());
        CHECK(numeric_cast<int64>(uint32 {4294967295u}) == 4294967295ll);

        CHECK_THROWS_AS((numeric_cast<int8>(int32 {128})), OverflowException);
        CHECK_THROWS_AS((numeric_cast<uint8>(int32 {-1})), OverflowException);
    }

    SECTION("CheckedDiv")
    {
        CHECK(checked_div<int32>(10, 2) == 5);
        CHECK_THROWS_AS((checked_div<int32>(10, 0)), DivisionByZeroException);
        CHECK_THROWS_AS((checked_div<float32>(1.0f, 0.0f)), DivisionByZeroException);
        CHECK_THROWS_AS((checked_div<float32>(1.0f, std::numeric_limits<float32>::epsilon() * 0.5f)), DivisionByZeroException);
        CHECK(checked_div<float32>(1.0f, std::numeric_limits<float32>::epsilon() * 2.0f) > 0.0f);
    }

    SECTION("CheckedIntegerOverflow")
    {
        CHECK(checked_add<int32>(100, 23) == 123);
        CHECK(checked_sub<int32>(100, 23) == 77);
        CHECK(checked_mul<int32>(12, 11) == 132);

        CHECK_THROWS_AS((checked_add<int32>(std::numeric_limits<int32>::max(), 1)), OverflowException);
        CHECK_THROWS_AS((checked_add<int32>(std::numeric_limits<int32>::min(), -1)), OverflowException);
        CHECK_THROWS_AS((checked_sub<int32>(std::numeric_limits<int32>::min(), 1)), OverflowException);
        CHECK_THROWS_AS((checked_sub<int32>(std::numeric_limits<int32>::max(), -1)), OverflowException);
        CHECK_THROWS_AS((checked_mul<int32>(std::numeric_limits<int32>::max(), 2)), OverflowException);
        CHECK_THROWS_AS((checked_mul<int32>(std::numeric_limits<int32>::min(), -1)), OverflowException);

        CHECK(checked_add<uint32>(10u, 20u) == 30u);
        CHECK(checked_sub<uint32>(20u, 10u) == 10u);
        CHECK(checked_mul<uint32>(12u, 11u) == 132u);

        CHECK_THROWS_AS((checked_add<uint32>(std::numeric_limits<uint32>::max(), 1u)), OverflowException);
        CHECK_THROWS_AS((checked_sub<uint32>(0u, 1u)), OverflowException);
        CHECK_THROWS_AS((checked_mul<uint32>(std::numeric_limits<uint32>::max(), 2u)), OverflowException);
    }

    SECTION("Lerp")
    {
        CHECK(lerp(10.0f, 20.0f, 0.5f) == 15.0f);
        CHECK(lerp(10, 20, 0.5f) == 15);
        CHECK(lerp(20u, 10u, 0.5f) == 15u);
        CHECK(lerp(5, 7, 0.24f) == 5);
        CHECK(lerp(5, 7, 0.26f) == 6);
        CHECK(lerp(5, 7, 0.76f) == 7);
        CHECK(lerp(20u, 10u, -1.0f) == 20u);
        CHECK(lerp(20u, 10u, 2.0f) == 10u);
    }

    SECTION("IterateRange")
    {
        auto sum = 0;
        for (const auto i : iterate_range(5)) {
            sum += i;
        }
        CHECK(sum == 10);

        const vector<int32> values = {4, 5, 6};
        size_t idx_sum = 0;
        for (const auto i : iterate_range(values)) {
            idx_sum += i;
        }
        CHECK(idx_sum == 3);
    }

    SECTION("IRoundAndConstCast")
    {
        CHECK(iround<int32>(1.4f) == 1);
        CHECK(iround<int32>(1.6f) == 2);
        CHECK(iround<int32>(-1.6f) == -2);

        constexpr int32 casted = const_numeric_cast<int32>(uint16 {42});
        STATIC_REQUIRE(casted == 42);
    }
}

FO_END_NAMESPACE
