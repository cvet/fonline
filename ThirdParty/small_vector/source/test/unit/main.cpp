/** main.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "unit_test_common.hpp"
#include "test_allocators.hpp"

int
main (void)
{
#ifdef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
  static_assert (0 == test (), "Test failed.");
  return 0;
#else
  return test ();
#endif
}
