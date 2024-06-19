/** test.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "unit_test_common.hpp"

GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test (void)
{
  {
    gch::small_vector<int, 2> v;
    CHECK (2 == v.capacity ());

    v.shrink_to_fit ();
    CHECK (2 == v.capacity ());

    v.push_back (1);
    v.shrink_to_fit ();
    CHECK (2 == v.capacity ());

    v.push_back (2);
    v.shrink_to_fit ();
    CHECK (2 == v.capacity ());

    v.push_back (3);
    v.shrink_to_fit ();
    CHECK (3 == v.capacity ());
    CHECK (! v.inlined ());

    v.pop_back ();
    v.shrink_to_fit ();
    CHECK (2 == v.capacity ());
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());

    v.pop_back ();
    v.shrink_to_fit ();
    CHECK (2 == v.capacity ());

    v.pop_back ();
    v.shrink_to_fit ();
    CHECK (2 == v.capacity ());
    CHECK (v.empty ());
  }
  {
    gch::small_vector<int, 0> w;
    CHECK (0 == w.capacity ());

    w.shrink_to_fit ();
    CHECK (0 == w.capacity ());

    w.push_back (1);
    w.shrink_to_fit ();
    CHECK (1 == w.capacity ());

    w.push_back (2);
    w.shrink_to_fit ();
    CHECK (2 == w.capacity ());

    w.push_back (3);
    w.shrink_to_fit ();
    CHECK (3 == w.capacity ());

    w.pop_back ();
    w.shrink_to_fit ();
    CHECK (2 == w.capacity ());

    w.pop_back ();
    w.shrink_to_fit ();
    CHECK (1 == w.capacity ());

    w.pop_back ();
    w.shrink_to_fit ();
    CHECK (0 == w.capacity ());
  }
  return 0;
}
