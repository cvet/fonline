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
  gch::small_vector<int> v { 1, 2, 3 };

  CHECK (v[0] == v.at (0));
  CHECK (v[0] == *v.begin ());
  CHECK (v[0] == *v.data ());
  CHECK (v[0] == v.front ());

  CHECK (v[1] == v.at (1));
  CHECK (v[1] == *std::next (v.begin ()));
  CHECK (v[1] == *std::next (v.data ()));
  CHECK (v[1] != v.front ());

  CHECK (v[v.size () - 1] == v.at (v.size () - 1));
  CHECK (v[v.size () - 1] == *std::next (v.begin (), static_cast<std::ptrdiff_t> (v.size () - 1)));
  CHECK (v[v.size () - 1] == *std::next (v.data (), static_cast<std::ptrdiff_t> (v.size () - 1)));
  CHECK (v[v.size () - 1] != v.front ());
  CHECK (v[v.size () - 1] == *std::prev (v.end ()));

  return 0;
}
