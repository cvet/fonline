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
  using namespace gch::test_types;

  {
    gch::small_vector<int, 3> v;
    CHECK (3 == v.capacity ());

    v.reserve (0);
    CHECK (3 == v.capacity ());

    v.reserve (1);
    CHECK (3 == v.capacity ());

    v.reserve (2);
    CHECK (3 == v.capacity ());

    v.reserve (3);
    CHECK (3 == v.capacity ());

    v.reserve (4);
    CHECK (4 <= v.capacity ());

    v.reserve (3);
    CHECK (3 < v.capacity ());

    v.reserve (2);
    CHECK (2 < v.capacity ());

    v.reserve (1);
    CHECK (1 < v.capacity ());

    v.reserve (0);
    CHECK (0 < v.capacity ());
  }

  {
    gch::small_vector_with_allocator<std::int8_t, sized_allocator<std::int8_t, std::uint8_t>> v;
    CHECK (127U == v.max_size ());
    v.reserve (v.max_size () / 2 + 1);

    // This allocation should snap to the maximum size.
    v.reserve (v.capacity () + 1);
    CHECK (v.capacity () == v.max_size ());
  }

#ifndef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR

  // Test strong exception guarantees when allocating over the maximum size.
  {
    gch::small_vector_with_allocator<std::int8_t, sized_allocator<std::int8_t, std::uint8_t>> w;
    CHECK (127U == w.max_size ());

    w.reserve (w.max_size ());
    CHECK (127U == w.capacity ());

    w.push_back (1);
    w.push_back (2);

    auto w_save = w;

    GCH_TRY
    {
      EXPECT_THROW (w.reserve (w.max_size () + 1));
    }
    GCH_CATCH (const std::length_error&)
    { }

    CHECK (w == w_save);
  }

  // Test strong exception guarantees when reallocating with a throwing copy constructor.
  {
    gch::small_vector<triggering_copy_ctor, 5> y { 1, 2, 3, 4 };
    auto y_save = y;

    exception_trigger::push (1);
    EXPECT_TEST_EXCEPTION (y.reserve (6));

    CHECK (y == y_save);
  }

#endif

  return 0;
}
