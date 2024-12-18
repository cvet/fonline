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
  gch::small_vector<int, 2> v { 1, 2 };
  CHECK (2 == v.size ());
  CHECK (2 == v.back ());

  v.pop_back ();
  CHECK (1 == v.size ());
  CHECK (1 == v.back ());

  v.pop_back ();
  CHECK (0 == v.size ());

  v.assign ({ 11, 22, 33 });
  CHECK (3 == v.size ());
  CHECK (33 == v.back ());

  v.pop_back ();
  CHECK (2 == v.size ());
  CHECK (22 == v.back ());

  v.pop_back ();
  CHECK (1 == v.size ());
  CHECK (11 == v.back ());

  v.pop_back ();
  CHECK (0 == v.size ());

  return 0;
}
