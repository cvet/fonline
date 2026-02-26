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

using test_meter = strong_type<int32, struct test_meter_tag, strong_type_bool_test_tag, strong_type_arithmetics_tag>;
using test_id = strong_type<uint32, struct test_id_tag>;
static_assert(some_strong_type<test_meter>);
static_assert(some_strong_type<test_id>);

TEST_CASE("StrongType")
{
    SECTION("ComparisonsAndBool")
    {
        const test_meter a {10};
        const test_meter b {20};

        CHECK(a < b);
        CHECK(a <= b);
        CHECK(b > a);
        CHECK(b >= a);
        CHECK(static_cast<bool>(a));
        CHECK_FALSE(static_cast<bool>(test_meter {}));
    }

    SECTION("Arithmetics")
    {
        const test_meter a {10};
        const test_meter b {4};

        CHECK((a + b).underlying_value() == 14);
        CHECK((a - b).underlying_value() == 6);
        CHECK((a * b).underlying_value() == 40);
        CHECK((a / b).underlying_value() == 2);
    }

    SECTION("UnderlyingValue")
    {
        test_meter value {7};
        value.underlying_value() = 11;
        CHECK(value.underlying_value() == 11);
    }

    SECTION("Hashing")
    {
        unordered_set<test_id> values;
        values.emplace(test_id {42});
        values.emplace(test_id {42});
        values.emplace(test_id {100});

        CHECK(values.size() == 2);
        CHECK(values.count(test_id {42}) == 1);
        CHECK(values.count(test_id {100}) == 1);
    }
}

FO_END_NAMESPACE
