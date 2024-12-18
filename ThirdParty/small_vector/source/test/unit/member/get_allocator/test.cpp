/** test.cpp
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
  using alloc_type = gch::test_types::allocator_with_id<int>;
  gch::small_vector_with_allocator<int, alloc_type> v;

  CHECK (v.empty ());
  CHECK (alloc_type { } == v.get_allocator ());
  CHECK (alloc_type { } == v.get_allocator ());

  alloc_type alloc (1);
  gch::small_vector_with_allocator<int, alloc_type> w (alloc);

  CHECK (w.empty ());
  CHECK (alloc == w.get_allocator ());
  CHECK (alloc_type { } != w.get_allocator ());
  CHECK (v.get_allocator () != w.get_allocator ());

  return 0;
}
