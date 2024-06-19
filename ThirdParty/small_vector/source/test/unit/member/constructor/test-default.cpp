/** test-default.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "unit_test_common.hpp"
#include "test_allocators.hpp"

GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test (void)
{
  using namespace gch::test_types;
  gch::small_vector<int, 3, allocator_with_id<int>> v;
  CHECK (allocator_with_id<int> () == v.get_allocator ());

  gch::small_vector<int, 3, allocator_with_id<int>> w (allocator_with_id<int> (1));
  CHECK (allocator_with_id<int> (1) == w.get_allocator ());

  return 0;
}
