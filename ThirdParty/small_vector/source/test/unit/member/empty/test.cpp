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
  gch::small_vector<int, 1> v;

  CHECK (v.empty ());

  v.push_back (1);
  CHECK (! v.empty ());

  v.push_back (2);
  CHECK (! v.empty ());

  return 0;
}
