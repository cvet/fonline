// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

FO_BEGIN_NAMESPACE

TEST_CASE("GenericUtils")
{
    SECTION("WyHash")
    {
        auto data {make_ptr("abcdefg").reinterpret_as<uint8_t>()};
        auto data2 {make_ptr("abcdefh").reinterpret_as<uint8_t>()};

        CHECK(hashing_ex::hash(data.get(), 4) != 0);
        CHECK(hashing_ex::hash(data.get(), 4) != hashing_ex::hash(data.get(), 5));
        CHECK(hashing_ex::hash(data.get(), 7) != hashing_ex::hash(data2.get(), 7));
    }

    SECTION("StdRandom")
    {
        std::mt19937 rnd32 {}; // NOLINT(cert-msc51-cpp)
        for (int32_t i {1}; i < 10000; i++) {
            (void)rnd32();
        }

        CHECK(rnd32() == 4123659995);

        std::mt19937_64 rnd64 {}; // NOLINT(cert-msc51-cpp)
        for (int32_t i {1}; i < 10000; i++) {
            (void)rnd64();
        }

        CHECK(rnd64() == 9981545732273789042UL);
    }

    SECTION("lerp")
    {
        CHECK(lerp(5, 7, -1.0f) == 5);
        CHECK(lerp(5, 7, 0.0f) == 5);
        CHECK(lerp(5, 7, 0.24f) == 5);
        CHECK(lerp(5, 7, 0.26f) == 6);
        CHECK(lerp(5, 7, 0.74f) == 6);
        CHECK(lerp(5, 7, 0.76f) == 7);
        CHECK(lerp(5, 7, 1.0f) == 7);
        CHECK(lerp(5, 7, 2.0f) == 7);

        CHECK(lerp(7u, 5u, -1.0f) == 7);
        CHECK(lerp(7u, 5u, 0.0f) == 7);
        CHECK(lerp(7u, 5u, 0.24f) == 7);
        CHECK(lerp(7u, 5u, 0.26f) == 6);
        CHECK(lerp(7u, 5u, 0.74f) == 6);
        CHECK(lerp(7u, 5u, 0.76f) == 5);
        CHECK(lerp(7u, 5u, 1.0f) == 5);
        CHECK(lerp(7u, 5u, 2.0f) == 5);

        CHECK(lerp(11, 11, -2.0f) == 11);
        CHECK(lerp(11, 11, 0.5f) == 11);
        CHECK(lerp(11, 11, 2.0f) == 11);
    }

    SECTION("FloatCompare")
    {
        CHECK(is_float_equal(0.111f, 0.111f));
        CHECK(is_float_equal(-110.95667f, -110.95667f));
        CHECK(is_float_equal(0.95667f, 0.95667f));
        CHECK(!is_float_equal(1.0f, -1.0f));
        CHECK(is_float_equal(1.0f, 1.0f + 1.0e-6f));
        CHECK_FALSE(is_float_equal(1.0f, 1.0f + 1.0e-4f, 1.0e-5f));
        CHECK(is_float_equal(1.0, 1.0 + 1.0e-9));
    }
}

FO_END_NAMESPACE
