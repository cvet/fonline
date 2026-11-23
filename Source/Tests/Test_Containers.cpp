// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

FO_BEGIN_NAMESPACE

TEST_CASE("Containers")
{
    SECTION("Concepts")
    {
        STATIC_REQUIRE(vector_collection<vector<int32_t>>);
        STATIC_REQUIRE(vector_collection<small_vector<int32_t, 4>>);
        STATIC_REQUIRE(map_collection<map<int32_t, int32_t>>);
        STATIC_REQUIRE(map_collection<unordered_map<int32_t, int32_t>>);
    }

    SECTION("StringHash")
    {
        hashing::hash<string> hasher {};
        CHECK(hasher(string {"hash-me"}) == hasher(string_view {"hash-me"}));
    }

    SECTION("VectorFormatterInt")
    {
        vector<int32_t> values {1, 2, 3};
        auto text {std::format("{}", values)};
        CHECK(text == "1 2 3");
    }

    SECTION("VectorFormatterChar")
    {
        vector<char> values {'A', ' '};
        std::string text {std::format("{}", values)};
        CHECK(text == "65 32");
    }

    SECTION("VectorFormatterByte")
    {
        vector<uint8_t> values {0, 65, 255};
        std::string text {std::format("{}", values)};
        CHECK(text == "0 65 255");
    }

    SECTION("VectorFormatterString")
    {
        vector<string> values {"one", "two", "three"};
        auto text {std::format("{}", values)};
        CHECK(text == "one two three");
    }

    SECTION("VectorFormatterBool")
    {
        vector<bool> values {true, false, true};
        auto text {std::format("{}", values)};
        CHECK(text == "True False True");
    }

    SECTION("VectorFormatterFloat")
    {
        // Pins the printf("%f") output the formatter produced while it went through std::to_string
        vector<float32_t> values {1.5f, -2.0f, 0.0f};
        auto text {std::format("{}", values)};
        CHECK(text == "1.500000 -2.000000 0.000000");
    }

    SECTION("VectorFormatterEmpty")
    {
        vector<int32_t> values {};
        auto text {std::format("{}", values)};
        CHECK(text.empty());
    }

    SECTION("SmallVectorFormatter")
    {
        small_vector<int32_t, 4> values {};
        values.push_back(7);
        values.push_back(8);
        values.push_back(9);

        auto text {std::format("{}", values)};
        CHECK(text == "7 8 9");
    }
}

FO_END_NAMESPACE
