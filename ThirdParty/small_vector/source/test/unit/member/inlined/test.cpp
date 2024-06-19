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

#ifdef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
  CHECK (! v.inlined ());
  return 0;
#else
  CHECK (v.inlined ());

  v.append ({ 1, 2, 3, 4 });

  // It should still be inlined.
  CHECK (v.inlined ());

  v.push_back (5);
  CHECK (! v.inlined ());

  v.erase (std::prev (v.end ()));
  CHECK (! v.inlined ());

  v.shrink_to_fit ();
  CHECK (v.inlined ());

  v.clear ();
  CHECK (v.inlined ());

  return 0;
#endif
}
