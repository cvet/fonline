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

TEST_CASE("ExtendedTypes")
{
    SECTION("UColor")
    {
        const ucolor c1 {1, 2, 3, 4};
        CHECK(c1.comp.r == 1);
        CHECK(c1.comp.g == 2);
        CHECK(c1.comp.b == 3);
        CHECK(c1.comp.a == 4);

        const ucolor c2 {1, 2, 3, 4};
        const ucolor c3 {2, 2, 3, 4};
        CHECK(c1 == c2);
        CHECK((c1 < c3 || c3 < c1));
    }

    SECTION("IPosISize")
    {
        ipos32 p {10, 20};
        p += ipos32 {1, 2};
        CHECK(p == ipos32 {11, 22});

        CHECK((p - ipos32 {1, 2}) == ipos32 {10, 20});
        CHECK((p + 3) == ipos32 {14, 25});

        const isize32 sz {5, 6};
        CHECK(sz.square() == 30);
        CHECK(sz.is_valid_pos(0, 0));
        CHECK(sz.is_valid_pos(4, 5));
        CHECK_FALSE(sz.is_valid_pos(5, 0));
        CHECK_FALSE(sz.is_valid_pos(-1, 0));
    }

    SECTION("IRect")
    {
        const irect32 r {ipos32 {3, 4}, isize32 {10, 20}};
        CHECK(r.pos() == ipos32 {3, 4});
        CHECK(r.size() == isize32 {10, 20});
        CHECK_FALSE(r.is_zero());
        CHECK(irect32 {}.is_zero());
    }

    SECTION("FPosFSizeFRect")
    {
        const fpos32 p1 {3.0f, 4.0f};
        CHECK(is_float_equal(p1.dist(), 5.0f));
        CHECK(p1.round<int32>() == ipos32 {3, 4});

        const fsize32 s1 {2.5f, 4.0f};
        CHECK(is_float_equal(s1.square(), 10.0f));

        const frect32 fr {1.0f, 2.0f, 3.0f, 4.0f};
        CHECK(fr.pos() == fpos32 {1.0f, 2.0f});
        CHECK(fr.size() == fsize32 {3.0f, 4.0f});
    }
}

FO_END_NAMESPACE
