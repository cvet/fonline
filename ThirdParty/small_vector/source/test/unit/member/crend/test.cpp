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
  gch::small_vector<int> v;

  CHECK (v.crend () == v.rend ());

  v.push_back (1);

  CHECK (1 == *std::prev (v.crend ()));
  CHECK (v.crend () == v.rend ());

  v.push_back (2);

  CHECK (1 == *std::prev (v.crend ()));
  CHECK (v.crend () == v.rend ());

  CHECK (v.capacity () == v.inline_capacity ());
  auto inlined_begin = v.begin ();

  static_assert (2 <= decltype (v)::inline_capacity_v, "gch::small_vector too small.");
  v.insert (v.crend ().base (), v.inline_capacity () - v.size () + 1, 3);

  CHECK (! v.inlined ());
  CHECK (v.begin () != inlined_begin);

  CHECK (3 == *std::prev (v.crend ()));
  CHECK (v.crend () == v.rend ());

  return 0;
}
