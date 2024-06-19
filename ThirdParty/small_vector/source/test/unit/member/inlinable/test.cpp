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
  gch::small_vector<int, 4> v;

  CHECK (v.inlinable ());

  v.append ({ 1, 2, 3, 4 });

  // It should still be inlinable.
  CHECK (v.inlinable ());

  v.push_back (5);
  CHECK (! v.inlinable ());

  v.erase (std::prev (v.end ()));
  CHECK (v.inlinable ());

  v.push_back (5);
  CHECK (! v.inlinable ());

  v.clear ();
  CHECK (v.inlinable ());

  return 0;
}
