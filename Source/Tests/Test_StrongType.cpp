// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

FO_BEGIN_NAMESPACE

using test_meter = strong_type<int32_t, struct test_meter_tag, strong_type_bool_test_tag, strong_type_arithmetics_tag>;
using test_id = strong_type<uint32_t, struct test_id_tag>;
static_assert(some_strong_type<test_meter>);
static_assert(some_strong_type<test_id>);

TEST_CASE("StrongType")
{
    SECTION("ComparisonsAndBool")
    {
        test_meter a {10};
        test_meter b {20};

        CHECK(a < b);
        CHECK(a <= b);
        CHECK(b > a);
        CHECK(b >= a);
        CHECK(static_cast<bool>(a));
        CHECK_FALSE(static_cast<bool>(test_meter {}));
    }

    SECTION("Arithmetics")
    {
        test_meter a {10};
        test_meter b {4};

        CHECK((a + b).underlying_value() == 14);
        CHECK((a - b).underlying_value() == 6);
        CHECK((a * b).underlying_value() == 40);
        CHECK((a / b).underlying_value() == 2);
        CHECK((a % b).underlying_value() == 2);
    }

    SECTION("UnderlyingValue")
    {
        test_meter value {7};
        value.underlying_value() = 11;
        CHECK(value.underlying_value() == 11);
    }

    SECTION("Hashing")
    {
        unordered_set<test_id> values {};
        values.emplace(test_id {42});
        values.emplace(test_id {42});
        values.emplace(test_id {100});

        CHECK(values.size() == 2);
        CHECK(values.count(test_id {42}) == 1);
        CHECK(values.count(test_id {100}) == 1);
    }

    SECTION("Formatting")
    {
        test_meter value {123};
        CHECK(std::format("{}", value) == "123");
    }

    SECTION("StreamExtraction")
    {
        std::istringstream input {"77"};
        test_meter value {};

        input >> value;

        CHECK(value.underlying_value() == 77);
    }

    SECTION("StreamExtractionFailureResetsToDefault")
    {
        std::istringstream input {"oops"};
        test_meter value {55};

        input >> value;

        CHECK(input.fail());
        CHECK(value.underlying_value() == 0);
    }
}

FO_END_NAMESPACE
