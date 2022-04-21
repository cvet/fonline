//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "catch.hpp"

#include "GenericUtils.h"

TEST_CASE("Dummy 1")
{
    REQUIRE(1 == 1);

    SECTION("2") { REQUIRE(2 == 2); }
}

TEST_CASE("Dummy 2")
{
    REQUIRE(1 == 1);

    SECTION("2") { REQUIRE(2 == 2); }
}

TEST_CASE("MurmurHash2")
{
    const auto* data = reinterpret_cast<const uchar*>("abcdef");
    REQUIRE(Hashing::MurmurHash2(data, 6) == 1271458169);
    REQUIRE(Hashing::MurmurHash2_64(data, 6) == 13226566493390071673ULL);
}

TEST_CASE("StdRandom")
{
    std::mt19937 rnd32;
    for (auto i = 1; i < 10000; i++) {
        (void)rnd32();
    }
    REQUIRE(rnd32() == 4123659995);

    std::mt19937_64 rnd64;
    for (auto i = 1; i < 10000; i++) {
        (void)rnd64();
    }
    REQUIRE(rnd64() == 9981545732273789042UL);
}

TEST_CASE("xrange")
{
    auto t1 = 0;
    auto t2 = 0;
    for (const auto i : xrange(5)) {
        t1++;
        t2 += i;
    }
    REQUIRE(t1 == 5);
    REQUIRE(t2 == 10);

    const auto v = vector {3, 4, 5, 6, 7};
    auto t3 = 0;
    auto t4 = 0;
    for (const auto i : xrange(v)) {
        t3++;
        t4 += v[i];
    }
    REQUIRE(t3 == 5);
    REQUIRE(t4 == 25);
}
