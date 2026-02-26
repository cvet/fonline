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

TEST_CASE("Containers")
{
    SECTION("Concepts")
    {
        STATIC_REQUIRE(vector_collection<vector<int32>>);
        STATIC_REQUIRE(map_collection<map<int32, int32>>);
    }

    SECTION("StringHash")
    {
        FO_HASH_NAMESPACE hash<string> hasher;
        CHECK(hasher(string {"hash-me"}) == hasher(string_view {"hash-me"}));
    }

    SECTION("VectorFormatterInt")
    {
        const vector<int32> values = {1, 2, 3};
        const auto text = std::format("{}", values);
        CHECK(text == "1 2 3");
    }

    SECTION("VectorFormatterString")
    {
        const vector<string> values = {"one", "two", "three"};
        const auto text = std::format("{}", values);
        CHECK(text == "one two three");
    }

    SECTION("VectorFormatterBool")
    {
        const vector<bool> values = {true, false, true};
        const auto text = std::format("{}", values);
        CHECK(text == "True False True");
    }
}

FO_END_NAMESPACE
