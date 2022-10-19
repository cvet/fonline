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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "AnyData.h"

TEST_CASE("AnyData")
{
    SECTION("Int 1")
    {
        AnyData::Value val = 1234;
        REQUIRE(AnyData::ValueToString(val) == "1234");
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::INT_VALUE));
    }

    SECTION("Float 1")
    {
        AnyData::Value val = 0.0f;
        REQUIRE(AnyData::ValueToString(val) == "0");
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::DOUBLE_VALUE));
    }

    SECTION("Float 2")
    {
        AnyData::Value val = 0.999999;
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::DOUBLE_VALUE));
    }

    SECTION("Float 3")
    {
        AnyData::Value val = 100000000.0;
        REQUIRE(AnyData::ValueToString(val) == "100000000");
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::DOUBLE_VALUE));
    }

    SECTION("Bool True")
    {
        AnyData::Value val = true;
        REQUIRE(AnyData::ValueToString(val) == "True");
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::BOOL_VALUE));
    }

    SECTION("Bool False")
    {
        AnyData::Value val = false;
        REQUIRE(AnyData::ValueToString(val) == "False");
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::BOOL_VALUE));
    }

    SECTION("String 1")
    {
        AnyData::Value val = string("ABCD");
        REQUIRE(AnyData::ValueToString(val) == "ABCD");
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::STRING_VALUE));
    }

    SECTION("String 2")
    {
        AnyData::Value val = string("AB   C D");
        REQUIRE(AnyData::ValueToString(val) == "AB   C D");
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::STRING_VALUE));
    }

    SECTION("String 3")
    {
        AnyData::Value val = string("   AB   C D");
        REQUIRE(AnyData::ValueToString(val) == "\"   AB   C D\"");
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::STRING_VALUE));
    }

    SECTION("String 4")
    {
        AnyData::Value val = string("ABCD ");
        REQUIRE(AnyData::ValueToString(val) == "\"ABCD \"");
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::STRING_VALUE));
    }

    SECTION("String 5")
    {
        AnyData::Value val = string("\tABCD");
        REQUIRE(AnyData::ValueToString(val) == "\"\tABCD\"");
        REQUIRE(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::STRING_VALUE));
    }

    SECTION("Array of ints")
    {
        AnyData::Array arr = {1, 2, 3, 4};
        REQUIRE(AnyData::ValueToString(arr) == "1 2 3 4");
        REQUIRE(arr == std::get<AnyData::ARRAY_VALUE>(AnyData::ParseValue(AnyData::ValueToString(arr), false, true, AnyData::INT_VALUE)));
    }

    SECTION("Array of strings")
    {
        AnyData::Array arr = {string("one"), string("two"), string("three")};
        REQUIRE(AnyData::ValueToString(arr) == "one two three");
        REQUIRE(arr == std::get<AnyData::ARRAY_VALUE>(AnyData::ParseValue(AnyData::ValueToString(arr), false, true, AnyData::STRING_VALUE)));
    }

    SECTION("Dict with ints")
    {
        AnyData::Dict dict;
        dict.emplace("key1", 1);
        dict.emplace("key2", 2);
        dict.emplace("key3", 3);
        REQUIRE(AnyData::ValueToString(dict) == "key1 1 key2 2 key3 3");
        REQUIRE(dict == std::get<AnyData::DICT_VALUE>(AnyData::ParseValue(AnyData::ValueToString(dict), true, false, AnyData::INT_VALUE)));
    }

    SECTION("Dict with strings")
    {
        AnyData::Dict dict;
        dict.emplace("key1", string("value 1"));
        dict.emplace("key2", string(" value 2 "));
        dict.emplace("key3", string("value3"));
        REQUIRE(AnyData::ValueToString(dict) == "key1 \"value 1\" key2 \" value 2 \" key3 value3");
        REQUIRE(dict == std::get<AnyData::DICT_VALUE>(AnyData::ParseValue(AnyData::ValueToString(dict), true, false, AnyData::STRING_VALUE)));
    }

    SECTION("Dict of array 1")
    {
        AnyData::Dict dict;
        AnyData::Array arr1 = {1, 2, 3};
        dict.emplace("key1", arr1);

        REQUIRE(AnyData::ValueToString(dict) == "key1 \"1 2 3\"");
        REQUIRE(dict == std::get<AnyData::DICT_VALUE>(AnyData::ParseValue(AnyData::ValueToString(dict), true, true, AnyData::INT_VALUE)));
    }

    SECTION("Dict of array 2")
    {
        AnyData::Dict dict;
        AnyData::Array arr1 = {string("one 1"), string("two 2"), string(" three 3 ")};
        AnyData::Array arr2 = {string("1"), string("2"), string("3")};
        dict.emplace("key1", arr1);
        dict.emplace("key2", arr2);

        REQUIRE(AnyData::ValueToString(dict) == "key1 \"\\\"one 1\\\" \\\"two 2\\\" \\\" three 3 \\\"\" key2 \"1 2 3\"");
        REQUIRE(dict == std::get<AnyData::DICT_VALUE>(AnyData::ParseValue(AnyData::ValueToString(dict), true, true, AnyData::STRING_VALUE)));
    }
}
