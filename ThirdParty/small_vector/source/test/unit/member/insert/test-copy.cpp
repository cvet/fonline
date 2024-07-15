/** test-copy.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "unit_test_common.hpp"
#include "test_allocators.hpp"

struct explicitly_constructed
  : gch::test_types::nontrivial_data_base
{
  GCH_SMALL_VECTOR_TEST_CONSTEXPR explicit
  explicitly_constructed (int i) noexcept
    : gch::test_types::nontrivial_data_base (i)
  { }

  using gch::test_types::nontrivial_data_base::nontrivial_data_base;
};

template <typename T, typename Allocator = std::allocator<T>>
GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test_with_type (Allocator alloc = Allocator ())
{
  T values[] = {
    T (0),
    T (1),
    T (2),
    T (3),
    T (4),
    T (5),
    T (6),
  };

  // Insert at the beginning without reallocating.
  {
    gch::small_vector<T, 3, Allocator> v ({ T (2), T (3) }, alloc);
    auto pos = v.insert (v.begin (), values[1]);

    CHECK (v.begin () == pos);
    CHECK (T (1) == *pos);
    CHECK (decltype (v) { T (1), T (2), T (3) } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Insert in the middle without reallocating.
  {
    gch::small_vector<T, 3, Allocator> v ({ T (1), T (3) }, alloc);
    auto pos = v.insert (std::next (v.begin ()), values[2]);

    CHECK (std::next (v.begin ()) == pos);
    CHECK (T (2) == *pos);
    CHECK (decltype (v) { T (1), T (2), T (3) } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Insert at the end without reallocating.
  {
    gch::small_vector<T, 3, Allocator> v ({ T (1), T (2) }, alloc);
    auto pos = v.insert (v.end (), values[3]);

    CHECK (std::prev (v.end ()) == pos);
    CHECK (T (3) == *pos);
    CHECK (decltype (v) { T (1), T (2), T (3) } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Insert at the beginning while reallocating.
  {
    gch::small_vector<T, 3, Allocator> v ({ T (2), T (3), T (4) }, alloc);
    auto pos = v.insert (v.begin (), values[1]);

    CHECK (v.begin () == pos);
    CHECK (T (1) == *pos);
    CHECK (decltype (v) { T (1), T (2), T (3), T (4) } == v);
    CHECK (! v.inlined ());
  }

  // Insert in the middle while reallocating.
  {
    gch::small_vector<T, 3, Allocator> v ({ T (1), T (3), T (4) }, alloc);
    auto pos = v.insert (std::next (v.begin ()), values[2]);

    CHECK (std::next (v.begin ()) == pos);
    CHECK (T (2) == *pos);
    CHECK (decltype (v) { T (1), T (2), T (3), T (4) } == v);
    CHECK (! v.inlined ());
  }

  // Insert at the end while reallocating.
  {
    gch::small_vector<T, 3, Allocator> v ({ T (1), T (2), T (3) }, alloc);
    auto pos = v.insert (v.end (), values[4]);

    CHECK (std::prev (v.end ()) == pos);
    CHECK (T (4) == *pos);
    CHECK (decltype (v) { T (1), T (2), T (3), T (4) } == v);
    CHECK (! v.inlined ());
  }

  return 0;
}

static
int
test_exceptions (void)
{
  using namespace gch::test_types;

  // Ensure valid states after exceptions.
  // Cases (Exception when...):
  //   No reallocation:
  //     - Creation of the temporary (No change).                  (1)
  //     - Moving elements into uninitialized memory.              (2)
  //     - Shifting initialized elements to the right.             (3)
  //     - Assignment of the temporary.                            (4)
  //     - Construction of the element at the end (No change).     (5)
  //   Reallocation:
  //     - Allocation too large (No change).                       (6)
  //     - Construction of the element (No change).                (7)
  //     - Moving of elements before `pos`.                        (8)
  //     - Moving of elements after `pos`.                         (9)
  //     - Construction of the element at the end (No change).     (10)
  //     - Moving of elements after construction of one at the end (11)

  using vector_type = gch::small_vector<triggering_copy_and_move, 4, verifying_allocator<triggering_copy_and_move>>;

  triggering_copy_and_move values[] = { 0, 1, 2, 3, 4, 5, 6 };

  // Throw upon creation of the temporary. (1)
  {
    vector_type v { 1, 3, 4 };
    vector_type v_save = v;

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), values[2]));

    CHECK (v == v_save);
  }

  // Throw while moving elements into uninitialized memory. (2)
  {
    vector_type v { 1, 3, 4 };
    vector_type v_save = v;

    exception_trigger::push (1);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), values[2]));

    CHECK (v == v_save);
  }

  // Throw while shifting elements to the right. (3)
  {
    vector_type v { 1, 3, 4 };

    exception_trigger::push (2);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), values[2]));

    CHECK (4 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (  v[2].is_moved);
    CHECK (! v[3].is_moved);
  }

  // Throw while moving temporary into the element location. (4)
  {
    vector_type v { 1, 3, 4 };

    exception_trigger::push (3);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), values[2]));

    CHECK (4 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (  v[1].is_moved);
    CHECK (! v[2].is_moved);
    CHECK (! v[3].is_moved);
  }

  // Throw during construction of the element at the end. (5)
  {
    vector_type v { 1, 2, 3 };
    vector_type v_save = v;

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), values[4]));

    CHECK (v == v_save);
  }

  // Throw because of a length error. (6)
  {
    gch::small_vector_with_allocator<std::int8_t, sized_allocator<std::int8_t, std::uint8_t>> w;
    CHECK (127U == w.max_size ());
    w.assign (w.max_size (), 1);

    auto w_save = w;

    GCH_TRY
    {
      EXPECT_THROW (w.insert (w.end (), 2));
    }
    GCH_CATCH (const std::length_error&)
    { }

    CHECK (w == w_save);

    GCH_TRY
    {
      EXPECT_THROW (w.insert (w.begin (), 2));
    }
    GCH_CATCH (const std::length_error&)
    { }

    CHECK (w == w_save);

    GCH_TRY
    {
      EXPECT_THROW (w.insert (std::next (w.begin ()), 2));
    }
    GCH_CATCH (const std::length_error&)
    { }

    CHECK (w == w_save);
  }

  // Throw during construction of the element. (7)
  {
    vector_type v { 1, 2, 4, 5 };
    vector_type v_save = v;

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), values[6]));

    CHECK (v == v_save);

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), values[3]));

    CHECK (v == v_save);
  }

  // Throw during the move of elements to the new allocation which are to the left of `pos`. (8)
  {
    vector_type v { 1, 2, 4, 5 };

    exception_trigger::push (2);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), values[3]));

    CHECK (4 == v.size ());
    CHECK (  v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (! v[2].is_moved);
    CHECK (! v[3].is_moved);
  }

  // Throw during the move of elements to the new allocation which are to the right of `pos`. (9)
  {
    vector_type v { 1, 2, 4, 5 };

    exception_trigger::push (4);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), values[3]));

    CHECK (4 == v.size ());
    CHECK (  v[0].is_moved);
    CHECK (  v[1].is_moved);
    CHECK (  v[2].is_moved);
    CHECK (! v[3].is_moved);
  }

  // Throw during construction of the element at the end (while reallocating). (10)
  {
    vector_type v { 1, 2, 3, 4 };
    vector_type v_save = v;

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), values[5]));

    CHECK (v == v_save);
  }

  // Throw during the move of elements to the new allocation after construction of one element at
  // the end. (11)
  {
    vector_type v { 1, 2, 3, 4 };
    vector_type v_save = v;

    // Throw during copy of index 0.
    exception_trigger::push (1);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), values[5]));

    CHECK (v == v_save);

    // Throw during copy of index 1.
    exception_trigger::push (2);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), values[5]));

    CHECK (v == v_save);

    // Throw during copy of index 2.
    exception_trigger::push (3);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), values[5]));

    CHECK (v == v_save);

    // Throw during copy of index 3.
    exception_trigger::push (4);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), values[5]));

    CHECK (v == v_save);
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
  CHECK (0 == test_with_type<explicitly_constructed> ());

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
