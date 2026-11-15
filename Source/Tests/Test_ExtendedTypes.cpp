// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

FO_BEGIN_NAMESPACE

TEST_CASE("ExtendedTypes")
{
    SECTION("UColor")
    {
        ucolor c1 {1, 2, 3, 4};
        CHECK(c1.comp.r == 1);
        CHECK(c1.comp.g == 2);
        CHECK(c1.comp.b == 3);
        CHECK(c1.comp.a == 4);

        ucolor c2 {1, 2, 3, 4};
        ucolor c3 {2, 2, 3, 4};
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

        isize32 sz {5, 6};
        CHECK(sz.square<int32_t>() == 30);
        CHECK(sz.is_valid_pos(0, 0));
        CHECK(sz.is_valid_pos(4, 5));
        CHECK_FALSE(sz.is_valid_pos(5, 0));
        CHECK_FALSE(sz.is_valid_pos(-1, 0));

        // The caller names the area type: int64 keeps areas past the 32-bit range, a narrower type rejects them
        isize32 huge_size {70000, 70000};
        isize32 negative_size {-1, 5};
        CHECK(huge_size.square<int64_t>() == int64_t {4900000000});
        CHECK(negative_size.square<int64_t>() == -5);
        CHECK_THROWS_AS(huge_size.square<int32_t>(), OverflowException);
        CHECK_THROWS_AS(huge_size.square<uint32_t>(), OverflowException);
        CHECK_THROWS_AS(negative_size.square<size_t>(), OverflowException);
    }

    SECTION("IRect")
    {
        irect32 r {ipos32 {3, 4}, isize32 {10, 20}};
        CHECK(r.pos() == ipos32 {3, 4});
        CHECK(r.size() == isize32 {10, 20});
        CHECK_FALSE(r.is_zero());
        CHECK(irect32 {}.is_zero());

        irect32 other {ipos32 {10, 2}, isize32 {5, 5}};
        irect32 expanded {r.expanded(other)};
        CHECK(expanded == irect32(3, 2, 12, 22));

        irect32 expanded_in_place {r};
        expanded_in_place.expand(other);
        CHECK(expanded_in_place == expanded);
    }

    SECTION("FPosFSizeFRect")
    {
        fpos32 p1 {3.0f, 4.0f};
        CHECK(is_float_equal(p1.dist(), 5.0f));
        CHECK(p1.round<int32_t>() == ipos32 {3, 4});

        fsize32 s1 {2.5f, 4.0f};
        CHECK(is_float_equal(s1.square(), 10.0f));

        frect32 fr {1.0f, 2.0f, 3.0f, 4.0f};
        CHECK(fr.pos() == fpos32 {1.0f, 2.0f});
        CHECK(fr.size() == fsize32 {3.0f, 4.0f});
    }
}

FO_END_NAMESPACE
