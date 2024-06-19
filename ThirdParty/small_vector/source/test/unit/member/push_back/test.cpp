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
    gch::small_vector<nontrivial_data_base, 3> v;
    v.push_back (1);

    nontrivial_data_base cref { 2 };
    v.push_back (cref);
    v.push_back (nontrivial_data_base { 3 });
    v.push_back ({ 4 });

    CHECK (decltype (v) { 1, 2, 3, 4 } == v);
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
      EXPECT_THROW (w.push_back (2));
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

    auto y_save = y;

    // Test without reallocation.
    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (y.push_back (3));

    CHECK (y == y_save);

    // Test with reallocation.
    y.push_back (3);
    CHECK (y.size () == y.capacity ());

    y_save = y;

    exception_trigger::push (0);
    GCH_TRY
    {
      // Throw when constructing `4` at the end of the new allocation.
      EXPECT_THROW (y.push_back (4));
    }
    GCH_CATCH (const test_exception&)
    { }

    CHECK (y == y_save);

    exception_trigger::push (1);
    GCH_TRY
    {
      // Throw when copying `1` to the new allocation.
      EXPECT_THROW (y.push_back (4));
    }
    GCH_CATCH (const test_exception&)
    { }

    CHECK (y == y_save);

    exception_trigger::push (2);
    GCH_TRY
    {
      // Throw when copying `2` to the new allocation.
      EXPECT_THROW (y.push_back (4));
    }
    GCH_CATCH (const test_exception&)
    { }

    CHECK (y == y_save);

    exception_trigger::push (3);
    GCH_TRY
    {
      // Throw when copying `3` to the new allocation.
      EXPECT_THROW (y.push_back (4));
    }
    GCH_CATCH (const test_exception&)
    { }

    CHECK (y == y_save);
  }

#endif

  return 0;
}
