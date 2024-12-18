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
  gch::small_vector<int> v { 2 };

  CHECK (v.front () == 2);
  CHECK (v.front () == v[0]);

  v.push_back (3);

  CHECK (v.front () == 2);
  CHECK (v.front () == v[0]);

  v.insert (v.begin (), 1);

  CHECK (v.front () == 1);
  CHECK (v.front () == v[0]);

  return 0;
}
