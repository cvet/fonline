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

  CHECK (gch::default_buffer_size<std::allocator<int>>::value == v.inline_capacity ());
  CHECK (gch::default_buffer_size<std::allocator<int>>::value == decltype (v)::inline_capacity ());

  return 0;
}
