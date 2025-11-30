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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

TEST_CASE("AnyData")
{
    SECTION("Int 1")
    {
        AnyData::Value val = numeric_cast<int64>(1234);
        CHECK(AnyData::ValueToString(val) == "1234");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Int64));
    }

    SECTION("Float 1")
    {
        AnyData::Value val = 0.0f;
        CHECK(AnyData::ValueToString(val) == "0");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Float64));
    }

    SECTION("Float 2")
    {
        AnyData::Value val = 0.999999;
        CHECK(AnyData::ValueToString(val) == "0.999999");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Float64));
    }

    SECTION("Float 3")
    {
        AnyData::Value val = 100000000.0;
        CHECK(AnyData::ValueToString(val) == "100000000");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Float64));
    }

    SECTION("Bool True")
    {
        AnyData::Value val = true;
        CHECK(AnyData::ValueToString(val) == "True");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Bool));
    }

    SECTION("Bool False")
    {
        AnyData::Value val = false;
        CHECK(AnyData::ValueToString(val) == "False");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Bool));
    }

    SECTION("String 1")
    {
        AnyData::Value val = string("ABCD");
        CHECK(AnyData::ValueToString(val) == "ABCD");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 2")
    {
        AnyData::Value val = string("AB   C D");
        CHECK(AnyData::ValueToString(val) == "AB   C D");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 3")
    {
        AnyData::Value val = string("   AB   C D");
        CHECK(AnyData::ValueToString(val) == "\"   AB   C D\"");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 4")
    {
        AnyData::Value val = string("ABCD ");
        CHECK(AnyData::ValueToString(val) == "\"ABCD \"");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 5")
    {
        AnyData::Value val = string("\tABCD");
        CHECK(AnyData::ValueToString(val) == "\"\tABCD\"");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("Array of ints")
    {
        AnyData::Array arr;
        arr.EmplaceBack(numeric_cast<int64>(1));
        arr.EmplaceBack(numeric_cast<int64>(2));
        arr.EmplaceBack(numeric_cast<int64>(3));
        arr.EmplaceBack(numeric_cast<int64>(4));
        CHECK(AnyData::ValueToString(arr.Copy()) == "1 2 3 4");
        CHECK(arr == AnyData::ParseValue(AnyData::ValueToString(arr.Copy()), false, true, AnyData::ValueType::Int64).AsArray());
    }

    SECTION("Array of strings")
    {
        AnyData::Array arr;
        arr.EmplaceBack(string("one"));
        arr.EmplaceBack(string("two"));
        arr.EmplaceBack(string("three"));
        CHECK(AnyData::ValueToString(arr.Copy()) == "one two three");
        CHECK(arr == AnyData::ParseValue(AnyData::ValueToString(arr.Copy()), false, true, AnyData::ValueType::String).AsArray());
    }

    SECTION("Dict with ints")
    {
        AnyData::Dict dict;
        dict.Emplace("key1", numeric_cast<int64>(1));
        dict.Emplace("key2", numeric_cast<int64>(2));
        dict.Emplace("key3", numeric_cast<int64>(3));
        CHECK(AnyData::ValueToString(dict.Copy()) == "key1 1 key2 2 key3 3");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, false, AnyData::ValueType::Int64).AsDict());
    }

    SECTION("Dict with strings")
    {
        AnyData::Dict dict;
        dict.Emplace("key1", string("value 1"));
        dict.Emplace("key2", string(" value 2 "));
        dict.Emplace("key3", string("value3"));
        CHECK(AnyData::ValueToString(dict.Copy()) == "key1 \"value 1\" key2 \" value 2 \" key3 value3");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, false, AnyData::ValueType::String).AsDict());
    }

    SECTION("Dict of array 1")
    {
        AnyData::Array arr1;
        arr1.EmplaceBack(numeric_cast<int64>(1));
        arr1.EmplaceBack(numeric_cast<int64>(2));
        arr1.EmplaceBack(numeric_cast<int64>(3));
        AnyData::Dict dict;
        dict.Emplace("key1", std::move(arr1));

        CHECK(AnyData::ValueToString(dict.Copy()) == "key1 \"1 2 3\"");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, true, AnyData::ValueType::Int64).AsDict());
    }

    SECTION("Dict of array 2")
    {
        AnyData::Array arr1;
        arr1.EmplaceBack(string("one 1"));
        arr1.EmplaceBack(string("two 2"));
        arr1.EmplaceBack(string(" three 3 "));
        AnyData::Array arr2;
        arr2.EmplaceBack(string("1"));
        arr2.EmplaceBack(string("2"));
        arr2.EmplaceBack(string("3"));
        AnyData::Dict dict;
        dict.Emplace("key1", std::move(arr1));
        dict.Emplace("key2", std::move(arr2));

        CHECK(AnyData::ValueToString(dict.Copy()) == "key1 \"\\\"one 1\\\" \\\"two 2\\\" \\\" three 3 \\\"\" key2 \"1 2 3\"");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, true, AnyData::ValueType::String).AsDict());
    }
}

FO_END_NAMESPACE();
