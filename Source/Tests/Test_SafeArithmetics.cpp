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

#include "Common.h"

FO_BEGIN_NAMESPACE

// Compile-time contract for deduced checked arithmetic
namespace TestSafeArithmeticContract
{
    template<typename Left, typename Right>
    concept CanDeduceCheckedArithmetic = requires(const Left& left, const Right& right) {
        { checked_add(left, right) } -> std::same_as<Left>;
        { checked_sub(left, right) } -> std::same_as<Left>;
        { checked_mul(left, right) } -> std::same_as<Left>;
        { checked_div(left, right) } -> std::same_as<Left>;
    };

    template<typename Left, typename Right>
    constexpr bool RejectsMixedOperands =
        !requires(const Left& left, const Right& right) { checked_add(left, right); } &&
        !requires(const Left& left, const Right& right) { checked_sub(left, right); } &&
        !requires(const Left& left, const Right& right) { checked_mul(left, right); } &&
        !requires(const Left& left, const Right& right) { checked_div(left, right); };

    template<typename T>
    constexpr bool RejectsExplicitTypeArgument =
        !requires(const T& left, const T& right) { checked_add<T>(left, right); } &&
        !requires(const T& left, const T& right) { checked_sub<T>(left, right); } &&
        !requires(const T& left, const T& right) { checked_mul<T>(left, right); } &&
        !requires(const T& left, const T& right) { checked_div<T>(left, right); };

    template<auto Value, typename T>
    constexpr bool RejectsValueArgument =
        !requires(const T& left, const T& right) { checked_add<Value>(left, right); } &&
        !requires(const T& left, const T& right) { checked_sub<Value>(left, right); } &&
        !requires(const T& left, const T& right) { checked_mul<Value>(left, right); } &&
        !requires(const T& left, const T& right) { checked_div<Value>(left, right); };

    static_assert(CanDeduceCheckedArithmetic<int32_t, int32_t>);
    static_assert(CanDeduceCheckedArithmetic<int64_t, int64_t>);
    static_assert(CanDeduceCheckedArithmetic<uint8_t, uint8_t>);
    static_assert(CanDeduceCheckedArithmetic<uint64_t, uint64_t>);
    static_assert(CanDeduceCheckedArithmetic<float32_t, float32_t>);
    static_assert(CanDeduceCheckedArithmetic<float64_t, float64_t>);

    static_assert(RejectsMixedOperands<int32_t, int64_t>);
    static_assert(RejectsMixedOperands<int64_t, int32_t>);
    static_assert(RejectsMixedOperands<int32_t, uint32_t>);
    static_assert(RejectsMixedOperands<uint32_t, int32_t>);
    static_assert(RejectsMixedOperands<float32_t, float64_t>);
    static_assert(RejectsMixedOperands<float64_t, float32_t>);
    static_assert(RejectsMixedOperands<int32_t, float32_t>);
    static_assert(RejectsMixedOperands<float32_t, int32_t>);
    static_assert(RejectsMixedOperands<int64_t, float64_t>);
    static_assert(RejectsMixedOperands<float64_t, int64_t>);
    static_assert(RejectsMixedOperands<bool, bool>);
    static_assert(RejectsMixedOperands<std::integral_constant<int32_t, 1>, int32_t>);

    static_assert(RejectsExplicitTypeArgument<int32_t>);
    static_assert(RejectsExplicitTypeArgument<int64_t>);
    static_assert(RejectsValueArgument<1, int32_t>);

    static_assert(checked_add(int32_t {2}, int32_t {3}) == 5);
    static_assert(checked_sub(int64_t {9}, int64_t {4}) == 5);
    static_assert(checked_mul(uint8_t {15}, uint8_t {17}) == uint8_t {255});
    static_assert(checked_div(int64_t {9}, int64_t {2}) == 4);
    static_assert(checked_mul(numeric_cast<int64_t>(int32_t {65535}), numeric_cast<int64_t>(int32_t {65535})) == int64_t {4294836225});
}

TEST_CASE("SafeArithmetics")
{
    SECTION("FloatCompare")
    {
        CHECK(is_float_equal(1.0f, 1.0f + 1.0e-7f));
        CHECK_FALSE(is_float_equal(1.0f, 1.1f));
    }

    SECTION("NumericCast")
    {
        CHECK(numeric_cast<int32_t>(uint16_t {123}) == 123);
        CHECK(clamp_to<uint8_t>(int32_t {-1}) == 0);
        CHECK(clamp_to<uint8_t>(int32_t {300}) == 255);
        CHECK(clamp_to<int8_t>(int32_t {127}) == 127);
        CHECK(clamp_to<int8_t>(int32_t {128}) == std::numeric_limits<int8_t>::max());
        CHECK(clamp_to<int8_t>(int32_t {-129}) == std::numeric_limits<int8_t>::min());
        CHECK(numeric_cast<int64_t>(uint32_t {4294967295u}) == 4294967295ll);

        CHECK_THROWS_AS((numeric_cast<int8_t>(int32_t {128})), OverflowException);
        CHECK_THROWS_AS((numeric_cast<uint8_t>(int32_t {-1})), OverflowException);
    }

    SECTION("CheckedDiv")
    {
        CHECK(checked_div(10, 2) == 5);
        CHECK_THROWS_AS((checked_div(10, 0)), DivisionByZeroException);
        CHECK_THROWS_AS((checked_div(1.0f, 0.0f)), DivisionByZeroException);
        CHECK_THROWS_AS((checked_div(1.0f, std::numeric_limits<float32_t>::epsilon() * 0.5f)), DivisionByZeroException);
        CHECK(checked_div(1.0f, std::numeric_limits<float32_t>::epsilon() * 2.0f) > 0.0f);
    }

    SECTION("CheckedIntegerOverflow")
    {
        CHECK(checked_add(100, 23) == 123);
        CHECK(checked_sub(100, 23) == 77);
        CHECK(checked_mul(12, 11) == 132);

        CHECK_THROWS_AS((checked_add(std::numeric_limits<int32_t>::max(), 1)), OverflowException);
        CHECK_THROWS_AS((checked_add(std::numeric_limits<int32_t>::min(), -1)), OverflowException);
        CHECK_THROWS_AS((checked_sub(std::numeric_limits<int32_t>::min(), 1)), OverflowException);
        CHECK_THROWS_AS((checked_sub(std::numeric_limits<int32_t>::max(), -1)), OverflowException);
        CHECK_THROWS_AS((checked_mul(std::numeric_limits<int32_t>::max(), 2)), OverflowException);
        CHECK_THROWS_AS((checked_mul(std::numeric_limits<int32_t>::min(), -1)), OverflowException);

        CHECK(checked_add(10u, 20u) == 30u);
        CHECK(checked_sub(20u, 10u) == 10u);
        CHECK(checked_mul(12u, 11u) == 132u);

        CHECK_THROWS_AS((checked_add(std::numeric_limits<uint32_t>::max(), 1u)), OverflowException);
        CHECK_THROWS_AS((checked_sub(0u, 1u)), OverflowException);
        CHECK_THROWS_AS((checked_mul(std::numeric_limits<uint32_t>::max(), 2u)), OverflowException);
    }

    SECTION("CheckedFloatingOverflow")
    {
        CHECK(checked_add(1.5f, 2.5f) == 4.0f);
        CHECK(checked_sub(1.5, 2.5) == -1.0);
        CHECK(checked_mul(-2.0f, 3.0f) == -6.0f);
        CHECK(checked_div(-7.0, 2.0) == -3.5);

        CHECK_THROWS_AS((checked_add(std::numeric_limits<float32_t>::max(), std::numeric_limits<float32_t>::max())), OverflowException);
        CHECK_THROWS_AS((checked_sub(-std::numeric_limits<float64_t>::max(), std::numeric_limits<float64_t>::max())), OverflowException);
        CHECK_THROWS_AS((checked_mul(std::numeric_limits<float32_t>::max(), 2.0f)), OverflowException);
        CHECK_THROWS_AS((checked_div(std::numeric_limits<float64_t>::max(), 0.5)), OverflowException);
        CHECK_THROWS_AS((checked_add(std::numeric_limits<float32_t>::quiet_NaN(), 1.0f)), OverflowException);
        CHECK_THROWS_AS((checked_mul(std::numeric_limits<float64_t>::infinity(), 0.0)), OverflowException);
        CHECK_THROWS_AS((checked_div(1.0f, std::numeric_limits<float32_t>::infinity())), OverflowException);
        CHECK_THROWS_AS((checked_div(std::numeric_limits<float64_t>::quiet_NaN(), 1.0)), OverflowException);
    }

    SECTION("CheckedSignedDivisionOverflow")
    {
        CHECK_THROWS_AS((checked_div(std::numeric_limits<int32_t>::min(), -1)), OverflowException);
        CHECK_THROWS_AS((checked_div(std::numeric_limits<int64_t>::min(), int64_t {-1})), OverflowException);
        CHECK(checked_div(std::numeric_limits<int32_t>::min(), 1) == std::numeric_limits<int32_t>::min());
        CHECK(checked_div(std::numeric_limits<int64_t>::min(), int64_t {2}) == std::numeric_limits<int64_t>::min() / 2);
    }

    SECTION("CheckedOperandsRemainUnchanged")
    {
        int32_t signed_left = 100;
        int32_t signed_right = 23;

        CHECK(checked_add(signed_left, signed_right) == 123);
        CHECK(checked_sub(signed_left, signed_right) == 77);
        CHECK(checked_mul(signed_left, signed_right) == 2300);
        CHECK(checked_div(signed_left, signed_right) == 4);
        CHECK(signed_left == 100);
        CHECK(signed_right == 23);

        int64_t min_value = std::numeric_limits<int64_t>::min();
        int64_t minus_one = -1;

        CHECK_THROWS_AS((checked_div(min_value, minus_one)), OverflowException);
        CHECK(min_value == std::numeric_limits<int64_t>::min());
        CHECK(minus_one == -1);

        uint32_t unsigned_left = 100u;
        uint32_t unsigned_right = 23u;

        CHECK(checked_add(unsigned_left, unsigned_right) == 123u);
        CHECK(checked_sub(unsigned_left, unsigned_right) == 77u);
        CHECK(checked_mul(unsigned_left, unsigned_right) == 2300u);
        CHECK(checked_div(unsigned_left, unsigned_right) == 4u);
        CHECK(unsigned_left == 100u);
        CHECK(unsigned_right == 23u);

        float32_t float_left = 1.5f;
        float32_t float_right = 2.5f;

        CHECK(checked_add(float_left, float_right) == 4.0f);
        CHECK(checked_div(float_left, 2.0f) == 0.75f);
        CHECK(float_left == 1.5f);
        CHECK(float_right == 2.5f);
    }

    SECTION("CheckedSignedAndUnsignedBoundaries")
    {
        CHECK(checked_add(std::numeric_limits<int64_t>::max(), int64_t {0}) == std::numeric_limits<int64_t>::max());
        CHECK(checked_add(std::numeric_limits<int64_t>::min(), int64_t {0}) == std::numeric_limits<int64_t>::min());
        CHECK(checked_add(std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::min()) == -1);
        CHECK(checked_sub(std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max()) == 0);
        CHECK(checked_sub(std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::min()) == 0);
        CHECK(checked_mul(std::numeric_limits<int64_t>::max(), int64_t {1}) == std::numeric_limits<int64_t>::max());
        CHECK(checked_mul(std::numeric_limits<int64_t>::min(), int64_t {1}) == std::numeric_limits<int64_t>::min());
        CHECK(checked_mul(std::numeric_limits<int64_t>::min(), int64_t {0}) == 0);
        CHECK(checked_div(std::numeric_limits<int64_t>::max(), int64_t {-1}) == -std::numeric_limits<int64_t>::max());
        CHECK(checked_div(int64_t {-9}, int64_t {4}) == -2);
        CHECK(checked_div(int64_t {9}, int64_t {-4}) == -2);
        CHECK(checked_div(int64_t {-9}, int64_t {-4}) == 2);

        CHECK_THROWS_AS((checked_add(std::numeric_limits<int64_t>::max(), int64_t {1})), OverflowException);
        CHECK_THROWS_AS((checked_add(std::numeric_limits<int64_t>::min(), int64_t {-1})), OverflowException);
        CHECK_THROWS_AS((checked_sub(std::numeric_limits<int64_t>::max(), int64_t {-1})), OverflowException);
        CHECK_THROWS_AS((checked_sub(std::numeric_limits<int64_t>::min(), int64_t {1})), OverflowException);
        CHECK_THROWS_AS((checked_mul(std::numeric_limits<int64_t>::max(), int64_t {2})), OverflowException);
        CHECK_THROWS_AS((checked_mul(std::numeric_limits<int64_t>::min(), int64_t {-1})), OverflowException);

        CHECK(checked_add(std::numeric_limits<uint64_t>::max(), uint64_t {0}) == std::numeric_limits<uint64_t>::max());
        CHECK(checked_sub(std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::max()) == 0);
        CHECK(checked_mul(std::numeric_limits<uint64_t>::max(), uint64_t {1}) == std::numeric_limits<uint64_t>::max());
        CHECK(checked_mul(std::numeric_limits<uint64_t>::max(), uint64_t {0}) == 0);
        CHECK(checked_div(std::numeric_limits<uint64_t>::max(), uint64_t {1}) == std::numeric_limits<uint64_t>::max());
        CHECK(checked_div(uint64_t {0}, std::numeric_limits<uint64_t>::max()) == 0);

        CHECK_THROWS_AS((checked_add(std::numeric_limits<uint64_t>::max(), uint64_t {1})), OverflowException);
        CHECK_THROWS_AS((checked_sub(uint64_t {0}, uint64_t {1})), OverflowException);
        CHECK_THROWS_AS((checked_mul(std::numeric_limits<uint64_t>::max(), uint64_t {2})), OverflowException);
        CHECK_THROWS_AS((checked_div(uint64_t {1}, uint64_t {0})), DivisionByZeroException);

        CHECK(checked_add(std::numeric_limits<int16_t>::max(), int16_t {0}) == std::numeric_limits<int16_t>::max());
        CHECK(checked_mul(int16_t {300}, int16_t {100}) == int16_t {30000});
        CHECK(checked_div(int16_t {-7}, int16_t {2}) == -3);
        CHECK_THROWS_AS((checked_add(std::numeric_limits<int16_t>::max(), int16_t {1})), OverflowException);
        CHECK_THROWS_AS((checked_mul(int16_t {300}, int16_t {110})), OverflowException);
        CHECK_THROWS_AS((checked_div(std::numeric_limits<int16_t>::min(), int16_t {-1})), OverflowException);

        CHECK(checked_add(uint8_t {200}, uint8_t {55}) == uint8_t {255});
        CHECK(checked_mul(uint8_t {15}, uint8_t {17}) == uint8_t {255});
        CHECK_THROWS_AS((checked_add(uint8_t {200}, uint8_t {56})), OverflowException);
        CHECK_THROWS_AS((checked_mul(uint8_t {16}, uint8_t {16})), OverflowException);
    }

    SECTION("CheckedFloatingValidAndUnderflow")
    {
        CHECK(checked_add(std::numeric_limits<float32_t>::max(), -std::numeric_limits<float32_t>::max()) == 0.0f);
        CHECK(checked_sub(std::numeric_limits<float32_t>::max(), std::numeric_limits<float32_t>::max()) == 0.0f);
        CHECK(checked_mul(std::numeric_limits<float32_t>::max(), 1.0f) == std::numeric_limits<float32_t>::max());
        CHECK(checked_mul(std::numeric_limits<float32_t>::max(), -1.0f) == -std::numeric_limits<float32_t>::max());
        CHECK(checked_div(std::numeric_limits<float32_t>::max(), 1.0f) == std::numeric_limits<float32_t>::max());
        CHECK(checked_div(std::numeric_limits<float64_t>::max(), 1.0) == std::numeric_limits<float64_t>::max());
        CHECK(checked_div(std::numeric_limits<float64_t>::max(), -2.0) == -std::numeric_limits<float64_t>::max() / 2.0);

        CHECK(checked_mul(std::numeric_limits<float32_t>::denorm_min(), 0.5f) == 0.0f);
        CHECK(checked_mul(std::numeric_limits<float64_t>::denorm_min(), 0.5) == 0.0);
        CHECK(checked_div(std::numeric_limits<float64_t>::denorm_min(), 2.0) == 0.0);
        CHECK(checked_add(std::numeric_limits<float32_t>::min(), -std::numeric_limits<float32_t>::min()) == 0.0f);
        CHECK(std::isfinite(checked_div(1.0f, std::numeric_limits<float32_t>::epsilon() * 2.0f)));
        CHECK(std::isfinite(checked_div(1.0, std::numeric_limits<float64_t>::epsilon() * 2.0)));
        CHECK(std::isfinite(checked_mul(std::numeric_limits<float32_t>::min(), std::numeric_limits<float32_t>::min())));

        CHECK_THROWS_AS((checked_div(1.0, 0.0)), DivisionByZeroException);
        CHECK_THROWS_AS((checked_div(1.0, std::numeric_limits<float64_t>::epsilon() * 0.5)), DivisionByZeroException);
    }

    SECTION("CheckedFloatingNonfiniteInputsAndResults")
    {
        float32_t nan32 = std::numeric_limits<float32_t>::quiet_NaN();
        float32_t inf32 = std::numeric_limits<float32_t>::infinity();
        float64_t nan64 = std::numeric_limits<float64_t>::quiet_NaN();
        float64_t inf64 = std::numeric_limits<float64_t>::infinity();

        CHECK_THROWS_AS((checked_add(nan32, 1.0f)), OverflowException);
        CHECK_THROWS_AS((checked_add(1.0, nan64)), OverflowException);
        CHECK_THROWS_AS((checked_sub(inf32, 1.0f)), OverflowException);
        CHECK_THROWS_AS((checked_sub(1.0, -inf64)), OverflowException);
        CHECK_THROWS_AS((checked_mul(nan32, 0.0f)), OverflowException);
        CHECK_THROWS_AS((checked_mul(2.0, inf64)), OverflowException);
        CHECK_THROWS_AS((checked_div(-inf32, 2.0f)), OverflowException);
        CHECK_THROWS_AS((checked_div(2.0, nan64)), OverflowException);

        CHECK_THROWS_AS((checked_add(std::numeric_limits<float64_t>::max(), std::numeric_limits<float64_t>::max())), OverflowException);
        CHECK_THROWS_AS((checked_sub(-std::numeric_limits<float32_t>::max(), std::numeric_limits<float32_t>::max())), OverflowException);
        CHECK_THROWS_AS((checked_mul(std::numeric_limits<float64_t>::max(), 2.0)), OverflowException);
        CHECK_THROWS_AS((checked_div(std::numeric_limits<float32_t>::max(), 0.5f)), OverflowException);
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

    SECTION("IRoundAndConstCast")
    {
        CHECK(iround<int32_t>(1.4f) == 1);
        CHECK(iround<int32_t>(1.6f) == 2);
        CHECK(iround<int32_t>(-1.6f) == -2);
        CHECK_THROWS_AS((iround<int32_t>(std::numeric_limits<float32_t>::infinity())), OverflowException);
        CHECK_THROWS_AS((iround<int32_t>(-std::numeric_limits<float32_t>::infinity())), OverflowException);
        CHECK_THROWS_AS((iround<int32_t>(std::numeric_limits<float32_t>::quiet_NaN())), OverflowException);
        CHECK_THROWS_AS((iround<int32_t>(std::numeric_limits<float64_t>::infinity())), OverflowException);
        CHECK_THROWS_AS((iround<int32_t>(-std::numeric_limits<float64_t>::infinity())), OverflowException);
        CHECK_THROWS_AS((iround<int32_t>(std::numeric_limits<float64_t>::quiet_NaN())), OverflowException);
        CHECK_THROWS_AS((iround<int64_t>(1e30f)), OverflowException);
        CHECK_THROWS_AS((iround<int64_t>(-1e30f)), OverflowException);
        CHECK_THROWS_AS((iround<int64_t>(1e300)), OverflowException);
        CHECK_THROWS_AS((iround<int64_t>(-1e300)), OverflowException);
        CHECK_THROWS_AS((iround<int64_t>(std::numeric_limits<float64_t>::max())), OverflowException);

        constexpr int32_t casted = const_numeric_cast<int32_t>(uint16_t {42});
        STATIC_REQUIRE(casted == 42);
    }
}

FO_END_NAMESPACE
