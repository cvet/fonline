/** test-elem.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "unit_test_common.hpp"
#include "test_allocators.hpp"

template <typename T, typename Allocator = std::allocator<T>>
GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test_with_type (Allocator alloc = Allocator ())
{
    using vector_type = gch::small_vector<T, 4, Allocator>;

  // Make sure it works for both iterator and const_iterator.
  {
    vector_type v ({ 1, 2, 3, 4 }, alloc);

    v.erase (v.begin ());
    CHECK (vector_type { 2, 3, 4 } == v);

    v.erase (v.cbegin ());
    CHECK (vector_type { 3, 4 } == v);
  }

  // Tests while inlined.
  {
    vector_type v ({ 1, 2, 3, 4 }, alloc);
    typename vector_type::iterator pos;
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());

    // Erase from the beginning.
    pos = v.erase (v.begin ());

    CHECK (vector_type { 2, 3, 4 } == v);
    CHECK (v.begin () == pos);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());

    // Erase from the middle.
    pos = v.erase (std::next (v.begin ()));

    CHECK (vector_type { 2, 4 } == v);
    CHECK (std::next (v.begin ()) == pos);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());

    // Erase from the end.
    pos = v.erase (std::prev (v.end ()));

    CHECK (vector_type { 2 } == v);
    CHECK (v.end () == pos);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Tests while allocated.
  {
    vector_type v ({ 1, 2, 3, 4, 5 }, alloc);
    typename vector_type::iterator pos;

    CHECK (! v.inlined ());

    // Erase from the beginning (while allocated).
    pos = v.erase (v.begin ());

    CHECK (vector_type { 2, 3, 4, 5 } == v);
    CHECK (v.begin () == pos);
    CHECK (! v.inlined ());

    // Erase from the middle (while allocated).
    pos = v.erase (std::next (v.begin ()));

    CHECK (vector_type { 2, 4, 5 } == v);
    CHECK (std::next (v.begin ()) == pos);
    CHECK (! v.inlined ());

    // Erase from the end (while allocated).
    pos = v.erase (std::prev (v.end ()));

    CHECK (vector_type { 2, 4 } == v);
    CHECK (v.end () == pos);
    CHECK (! v.inlined ());
  }

  return 0;
}

static
int
test_exceptions (void)
{
  using namespace gch::test_types;
  using vector_type =
    gch::small_vector<triggering_copy_and_move, 4, verifying_allocator<triggering_copy_and_move>>;

  // Expected results when throwing during erasure:
  //   The size of the container shall not change, however some elements may have been moved.

  // Tests while inlined.
  {
    vector_type v { 1, 2, 3, 4 };
    vector_type v_save = v;

    CHECK (v.inlined ());

    for (std::size_t pos_idx = 0; pos_idx < v.size (); ++pos_idx)
    {
      auto pos = std::next (v.begin (), static_cast<std::ptrdiff_t> (pos_idx));
      for (std::size_t j = 0; j < v.size () - pos_idx - 1; ++j)
      {
        exception_trigger::push (j);
        EXPECT_TEST_EXCEPTION (v.erase (pos));

        const std::size_t moved_pos = pos_idx + j - 1;
        for (std::size_t k = pos_idx; k < v.size () - 1; ++k)
        {
          if (k == moved_pos)
            CHECK (v[k + 1].is_moved);
          else
            CHECK (! v[k + 1].is_moved);
        }

        CHECK (v.inlined ());

        v = v_save;
      }
    }
  }

  // Tests while allocated.
  {
    vector_type v { 1, 2, 3, 4, 5 };
    vector_type v_save = v;

    CHECK (! v.inlined ());

    for (std::size_t pos_idx = 0; pos_idx < v.size (); ++pos_idx)
    {
      auto pos = std::next (v.begin (), static_cast<std::ptrdiff_t> (pos_idx));
      for (std::size_t j = 0; j < v.size () - pos_idx - 1; ++j)
      {
        exception_trigger::push (j);
        EXPECT_TEST_EXCEPTION (v.erase (pos));

        const std::size_t moved_pos = pos_idx + j - 1;
        for (std::size_t k = pos_idx; k < v.size () - 1; ++k)
        {
          if (k == moved_pos)
            CHECK (v[k + 1].is_moved);
          else
            CHECK (! v[k + 1].is_moved);
        }

        CHECK (! v.inlined ());

        v = v_save;
      }
    }
  }

  return 0;
}

GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test (void)
{
  using namespace gch::test_types;

  CHECK (0 == test_with_type<trivially_copyable_data_base> ());
  CHECK (0 == test_with_type<nontrivial_data_base> ());

  CHECK (0 == test_with_type<trivially_copyable_data_base,
                             sized_allocator<trivially_copyable_data_base, std::uint8_t>> ());

  CHECK (0 == test_with_type<nontrivial_data_base,
                             fancy_pointer_allocator<nontrivial_data_base>> ());

#ifndef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
  CHECK (0 == test_with_type<trivially_copyable_data_base,
                             verifying_allocator<trivially_copyable_data_base>> ());

  CHECK (0 == test_with_type<nontrivial_data_base, verifying_allocator<nontrivial_data_base>> ());

  CHECK (0 == test_with_type<trivially_copyable_data_base,
                             non_propagating_verifying_allocator<trivially_copyable_data_base>> ());

  CHECK (0 == test_with_type<nontrivial_data_base,
                             non_propagating_verifying_allocator<nontrivial_data_base>> ());
#endif

  CHECK (0 == test_with_type<trivially_copyable_data_base,
                             allocator_with_id<trivially_copyable_data_base>> ());

  CHECK (0 == test_with_type<nontrivial_data_base,
                             allocator_with_id<nontrivial_data_base>> ());

  CHECK (0 == test_with_type<trivially_copyable_data_base,
                             propagating_allocator_with_id<trivially_copyable_data_base>> ());

  CHECK (0 == test_with_type<nontrivial_data_base,
                             propagating_allocator_with_id<nontrivial_data_base>> ());

#ifdef GCH_SMALL_VECTOR_TEST_EXCEPTION_SAFETY_TESTING
  CHECK (0 == test_exceptions ());
#endif

  return 0;
}
