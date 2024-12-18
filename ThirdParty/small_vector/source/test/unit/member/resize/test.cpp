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
  // Cases:
  //   0 == new_size       => erase all elements.      (1)
  //   capacity < new_size => reallocate.              (2)
  //   size < new_size     => append to uninitialized. (3)
  //   size <= new_size    => erase to end.            (4)

  using namespace gch::test_types;

  // (1)
  {
    gch::small_vector<int, 4> v { 1, 2, 3 };
    v.resize (0);
    CHECK (0 == v.size ());
  }

  // (2)
  {
    gch::small_vector<int, 4> v { 1, 2, 3 };
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());

    v.resize (5);
    CHECK (5 == v.size ());
    CHECK (decltype(v) { 1, 2, 3, 0, 0 } == v);
    CHECK (! v.inlined ());
  }

  // (3)
  {
    gch::small_vector<int, 4> v { 1, 2 };
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());

    v.resize (4);
    CHECK (4 == v.size ());
    CHECK (decltype(v) { 1, 2, 0, 0 } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());

    // Check that it also works when allocated.
    gch::small_vector<int, 0> w { 1, 2 };
    CHECK (! w.inlined ());

    w.resize (4);
    CHECK (4 == w.size ());
    CHECK (decltype(w) { 1, 2, 0, 0 } == w);
    CHECK (! w.inlined ());
  }

  // (4)
  {
    gch::small_vector<int, 4> v { 1, 2, 3 };
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());

    v.resize (1);
    CHECK (1 == v.size ());
    CHECK (decltype(v) { 1 } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());

    // Check that it also works when allocated.
    gch::small_vector<int, 0> w { 1, 2, 3, 4 };
    CHECK (! w.inlined ());

    w.resize (2);
    CHECK (2 == w.size ());
    CHECK (decltype(w) { 1, 2 } == w);
    CHECK (! w.inlined ());
  }

#ifndef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR

  // Test strong exception guarantees when allocating over the maximum size.
  {
    gch::small_vector_with_allocator<std::int8_t, sized_allocator<std::int8_t, std::uint8_t>> w;
    CHECK (127U == w.max_size ());
    w.assign (w.max_size (), 1);

    auto w_save = w;

    GCH_TRY
    {
      EXPECT_THROW (w.resize (w.max_size () + 1));
    }
    GCH_CATCH (const std::length_error&)
    { }

    CHECK (w == w_save);
  }

  // Test strong exception guarantees when pushing with a throwing copy constructor.
  {
    gch::small_vector<triggering_copy_ctor, 3, verifying_allocator<triggering_copy_ctor>> y {
      1,
      2
    };

    // Test without reallocation.
    exception_trigger::push (0);

    // Make sure this doesn't throw. We should be default constructing, not copying.
    y.resize (3);

    exception_trigger::reset ();
  }

  // Throw when copying to index 0.
  {
    gch::small_vector<triggering_copy_ctor, 3, verifying_allocator<triggering_copy_ctor>> y {
      1,
      2
    };

    auto y_save = y;

    // Test with reallocation.
    exception_trigger::push (0);
    GCH_TRY
    {
      // Throw when copying to index 0.
      EXPECT_THROW (y.resize (4));
    }
    GCH_CATCH (const test_exception&)
    { }

    CHECK (y == y_save);
  }

  // Throw when copying to index 1.
  {
    gch::small_vector<triggering_copy_ctor, 3, verifying_allocator<triggering_copy_ctor>> y {
      1,
      2
    };

    auto y_save = y;

    exception_trigger::push (1);
    EXPECT_TEST_EXCEPTION (y.resize (4));

    CHECK (y == y_save);
  }

#endif

  return 0;
}
