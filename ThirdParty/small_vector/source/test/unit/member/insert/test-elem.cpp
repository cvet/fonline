/** test-elem.cpp
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
  T val (-1);

  // Insert none while inlined.
  {
    gch::small_vector<T, 4, Allocator> v ({ T (1), T (2) }, alloc);
    auto pos = v.insert (v.begin (), 0, val);

    CHECK (v.begin () == pos);
    CHECK (decltype (v) { T (1), T (2) } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Insert none while allocated inlined.
  {
    gch::small_vector<T, 2, Allocator> v ({ T (1), T (2), T (3) }, alloc);
    auto pos = v.insert (v.begin (), 0, val);

    CHECK (v.begin () == pos);
    CHECK (decltype (v) { T (1), T (2), T (3) } == v);
    CHECK (! v.inlined ());
  }

  // Insert at the beginning without reallocating.
  {
    gch::small_vector<T, 4, Allocator> v ({ T (3), T (4) }, alloc);
    auto pos = v.insert (v.begin (), 2, val);

    CHECK (v.begin () == pos);
    CHECK (val == *pos);
    CHECK (decltype (v) { val, val, T (3), T (4) } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Insert in the middle without reallocating (only assign).
  {
    gch::small_vector<T, 5, Allocator> v ({ T (1), T (4), T (5) }, alloc);
    auto pos = v.insert (std::next(v.begin ()), 2, val);

    CHECK (std::next (v.begin ()) == pos);
    CHECK (val == *pos);
    CHECK (decltype (v) { T (1), val, val, T (4), T (5) } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Insert in the middle without reallocating (both assign and construct).
  {
    gch::small_vector<T, 4, Allocator> v ({ T (1), T (4) }, alloc);
    auto pos = v.insert (std::next(v.begin ()), 2, val);

    CHECK (std::next (v.begin ()) == pos);
    CHECK (val == *pos);
    CHECK (decltype (v) { T (1), val, val, T (4) } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Insert at the end without reallocating.
  {
    gch::small_vector<T, 4, Allocator> v ({ T (1), T (2) }, alloc);
    auto pos = v.insert (v.end (), 2, val);

    CHECK (std::next (v.begin (), 2) == pos);
    CHECK (val == *pos);
    CHECK (decltype (v) { T (1), T (2), val, val } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Insert one element at the end without reallocating.
  {
    gch::small_vector<T, 4, Allocator> v ({ T (1), T (2), T (3) }, alloc);
    auto pos = v.insert (v.end (), 1, val);

    CHECK (std::next (v.begin (), 3) == pos);
    CHECK (val == *pos);
    CHECK (decltype (v) { T (1), T (2), T (3), val } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Insert at the beginning while reallocating.
  {
    gch::small_vector<T, 4, Allocator> v ({ T (3), T (4), T (5) }, alloc);
    auto pos = v.insert (v.begin (), 2, val);

    CHECK (v.begin () == pos);
    CHECK (val == *pos);
    CHECK (decltype (v) { val, val, T (3), T (4), T (5) } == v);
    CHECK (! v.inlined ());
  }

  // Insert in the middle while reallocating.
  {
    gch::small_vector<T, 4, Allocator> v ({ T (1), T (4), T (5) }, alloc);
    auto pos = v.insert (std::next (v.begin ()), 2, val);

    CHECK (std::next (v.begin ()) == pos);
    CHECK (val == *pos);
    CHECK (decltype (v) { T (1), val, val, T (4), T (5) } == v);
    CHECK (! v.inlined ());
  }

  // Insert at the end while reallocating.
  {
    gch::small_vector<T, 4, Allocator> v ({ T (1), T (2), T (3) }, alloc);
    auto pos = v.insert (v.end (), 2, val);

    CHECK (std::next (v.begin (), 3) == pos);
    CHECK (val == *pos);
    CHECK (decltype (v) { T (1), T (2), T (3), val, val } == v);
    CHECK (! v.inlined ());
  }

  // Insert one element at the end while reallocating.
  {
    gch::small_vector<T, 4, Allocator> v ({ T (1), T (2), T (3), T (4) }, alloc);
    auto pos = v.insert (v.end (), 1, val);

    CHECK (std::next (v.begin (), 4) == pos);
    CHECK (val == *pos);
    CHECK (decltype (v) { T (1), T (2), T (3), T (4), val } == v);
    CHECK (! v.inlined ());
  }

  // Test aliasing (only assign).
  {
    gch::small_vector<T, 5, Allocator> v ({ T (1), T (4), T (5) }, alloc);
    auto pos = v.insert (std::next(v.begin ()), 2, v[2]);

    CHECK (std::next (v.begin ()) == pos);
    CHECK (T (5) == *pos);
    CHECK (decltype (v) { T (1), T (5), T (5), T (4), T (5) } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
  }

  // Test aliasing (both assign and construct).
  {
    gch::small_vector<T, 6, Allocator> v ({ T (1), T (4), T (5) }, alloc);
    auto pos = v.insert (std::next(v.begin ()), 3, v[2]);

    CHECK (std::next (v.begin ()) == pos);
    CHECK (T (5) == *pos);
    CHECK (decltype (v) { T (1), T (5), T (5), T (5), T (4), T (5) } == v);
    CHECK_IF_NOT_CONSTEXPR (v.inlined ());
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
  //     - Moving elements into uninitialized memory.                          (1)
  //     - Shifting initialized elements to the right.                         (2)
  //     - Assignment of the range.                                            (3)
  //     - Construction of a single element at the end (No change).            (4)
  //     - Construction of the range at the end.                               (5)
  //   Reallocation:
  //     - Allocation too large (No change).                                   (6)
  //     - Construction of the range (No change).                              (7)
  //     - Moving of elements before `pos`.                                    (8)
  //     - Moving of elements after `pos`.                                     (9)
  //     - Construction of the element at the end (No change).                 (10)
  //     - Moving of elements after construction of single element at the end. (11)

  using vector_type = gch::small_vector<triggering_copy_and_move, 7, verifying_allocator<triggering_copy_and_move>>;

  triggering_copy_and_move val (-1);

  // Check that it does not throw when appending zero elements.
  {
    vector_type v { 1, 2, 3 };
    vector_type v_save = v;

    exception_trigger::push (0);
    v.insert (v.begin (), 0, val);
    exception_trigger::reset ();

    // This should have no effect.
    CHECK (v == v_save);

    // Check the same thing while allocated.
    v.assign ({ 1, 2, 3, 4, 5, 6, 7, 8 });
    v_save = v;

    exception_trigger::push (0);
    v.insert (v.begin (), 0, val);
    exception_trigger::reset ();

    // This should have no effect.
    CHECK (v == v_save);
  }

  // Throw while moving elements into uninitialized memory. (1)
  {
    vector_type v { 1, 4, 5 };
    vector_type v_save = v;

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    // This should have no effect (in this particular case!).
    CHECK (v == v_save);

    exception_trigger::push (1);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    // This should have no effect (in this particular case!).
    CHECK (v == v_save);

    // Throw after creation of the temporary, while shifting into uninitialized.
    exception_trigger::push (2);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (3 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (  v[1].is_moved);
    CHECK (! v[2].is_moved);
  }

  // Throw while shifting elements to the right (index 0). (2)
  {
    vector_type v { 1, 4, 5, 6, 7 };

    exception_trigger::push (3);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (7 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (! v[2].is_moved);
    CHECK (  v[3].is_moved);
    CHECK (  v[4].is_moved);
    CHECK (! v[5].is_moved);
    CHECK (! v[6].is_moved);
  }

  // Throw while shifting elements to the right (index 1). (2)
  {
    vector_type v { 1, 4, 5, 6, 7 };

    exception_trigger::push (4);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (7 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (  v[2].is_moved);
    CHECK (  v[3].is_moved);
    CHECK (! v[4].is_moved);
    CHECK (! v[5].is_moved);
    CHECK (! v[6].is_moved);
  }

  // Throw during assignment of the range (count <= tail_size). (3)
  {
    vector_type v { 1, 4, 5 };
    vector_type v_save = v;

    // This should fully roll back.
    exception_trigger::push (3);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (v == v_save);

    // This should fully roll back.
    exception_trigger::push (4);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (v == v_save);

    // This will throw during the attempted rollback at index 0.
    exception_trigger::push (0);
    exception_trigger::push (3);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (5 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (  v[1].is_moved);
    CHECK (  v[2].is_moved);
    CHECK (! v[3].is_moved);
    CHECK (! v[4].is_moved);

    v = v_save;

    // This will throw during the attempted rollback at index 0.
    exception_trigger::push (0);
    exception_trigger::push (4);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (5 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (  v[2].is_moved);
    CHECK (! v[3].is_moved);
    CHECK (! v[4].is_moved);
    CHECK (v[1] == val);

    v = v_save;

    // This will throw during the attempted rollback at index 1.
    exception_trigger::push (1);
    exception_trigger::push (3);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (5 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (  v[2].is_moved);
    CHECK (  v[3].is_moved);
    CHECK (! v[4].is_moved);

    v = v_save;

    // This will throw during the attempted rollback at index 1.
    exception_trigger::push (1);
    exception_trigger::push (4);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (5 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (  v[2].is_moved);
    CHECK (  v[3].is_moved);
    CHECK (! v[4].is_moved);
    CHECK (v[1] == v_save[1]);
  }

  // Throw during assignment of the range (tail_size < count). (3)
  {
    vector_type v { 1, 2, 6, 7 };
    vector_type v_save = v;

    // This should have no effect (throws during copy of the range to the uninitialized section).
    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 3, val));

    CHECK (v == v_save);

    // Throw during creation of the temporary.
    exception_trigger::push (1);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 3, val));

    // This should have no effect (in this particular case!).
    CHECK (v == v_save);

    // Throw during move of the tail to the end.
    exception_trigger::push (2);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 3, val));

    // This should have no effect (in this particular case!).
    CHECK (v == v_save);

    // Throw during move of the tail to the end (at index 1).
    exception_trigger::push (3);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 3, val));

    CHECK (4 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (  v[2].is_moved);
    CHECK (! v[3].is_moved);

    v = v_save;

    // This should fully roll back (throws during assignment of rest of the range).
    exception_trigger::push (4);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 3, val));

    CHECK (v == v_save);

    // This should fully roll back (throws during assignment of rest of the range).
    exception_trigger::push (5);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 3, val));

    CHECK (v == v_save);

    // This will throw during the attempted rollback at tail index 0.
    exception_trigger::push (0);
    exception_trigger::push (4);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 3, val));

    CHECK (4 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (  v[2].is_moved);
    CHECK (  v[3].is_moved);

    v = v_save;

    // This will throw during the attempted rollback at tail index 1.
    exception_trigger::push (1);
    exception_trigger::push (4);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 3, val));

    CHECK (4 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (! v[2].is_moved);
    CHECK (  v[3].is_moved);
    CHECK (v[2] == v_save[2]);

    v = v_save;

    // This will throw during the attempted rollback at tail index 0.
    exception_trigger::push (0);
    exception_trigger::push (5);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 3, val));

    CHECK (4 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (! v[2].is_moved);
    CHECK (  v[3].is_moved);
    CHECK (v[2] == val);

    v = v_save;

    // This will throw during the attempted rollback at tail index 1.
    exception_trigger::push (1);
    exception_trigger::push (5);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 3, val));

    CHECK (4 == v.size ());
    CHECK (! v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (! v[2].is_moved);
    CHECK (  v[3].is_moved);
    CHECK (v[2] == v_save[2]);
  }

  // Throw during construction of a single element at the end (No change). (4)
  {
    vector_type v { 1, 2, 3 };
    vector_type v_save = v;

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), 1, val));

    CHECK (v == v_save);
  }

  // Throw during construction of a range at the end. (5)
  {
    // There should always be no effect in this case.
    vector_type v { 1, 2, 3 };
    vector_type v_save = v;

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), 2, val));

    CHECK (v == v_save);

    exception_trigger::push (1);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), 2, val));

    CHECK (v == v_save);
  }

  // Throw because of a length error. (6)
  {
    // This should have no effect in any case.

    gch::small_vector_with_allocator<std::int8_t, sized_allocator<std::int8_t, std::uint8_t>> w;
    CHECK (127U == w.max_size ());

    // This case should have no effect.
    w.assign (w.max_size (), 1);
    auto w_save = w;
    GCH_TRY
    {
      EXPECT_THROW (w.insert (w.end (), 2, -1));
    }
    GCH_CATCH (const std::length_error&)
    { }

    CHECK (w == w_save);

    // This case should append one element to the end, then throw.
    w.assign (w.max_size () - 1, 1);
    w_save = w;
    GCH_TRY
    {
      EXPECT_THROW (w.insert (w.begin (), 2, -1));
    }
    GCH_CATCH (const std::length_error&)
    { }

    CHECK (w == w_save);
  }

  // Throw during construction of the range while reallocating. (7)
  {
    // This should have no effect in any case.

    vector_type v { 1, 3, 4, 5, 6, 7 };
    vector_type v_save = v;

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (v == v_save);

    exception_trigger::push (1);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin ()), 2, val));

    CHECK (v == v_save);

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), 2, val));

    CHECK (v == v_save);

    exception_trigger::push (1);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), 2, val));

    CHECK (v == v_save);
  }

  // Throw during the move of elements which are to the left of `pos`. (8)
  {
    vector_type v { 1, 2, 5, 6, 7, 8 };
    vector_type v_save = v;

    exception_trigger::push (2);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 2, val));

    // This should have no effect (in this particular case!).
    CHECK (v == v_save);

    exception_trigger::push (3);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 2, val));

    CHECK (6 == v.size ());
    CHECK (  v[0].is_moved);
    CHECK (! v[1].is_moved);
    CHECK (! v[2].is_moved);
    CHECK (! v[3].is_moved);
    CHECK (! v[4].is_moved);
    CHECK (! v[5].is_moved);
  }

  // Throw during the move of elements which are to the right of `pos` (index 0). (9)
  {
    vector_type v { 1, 2, 5, 6, 7, 8 };

    exception_trigger::push (4);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 2, val));

    CHECK (6 == v.size ());
    CHECK (  v[0].is_moved);
    CHECK (  v[1].is_moved);
    CHECK (! v[2].is_moved);
    CHECK (! v[3].is_moved);
    CHECK (! v[4].is_moved);
    CHECK (! v[5].is_moved);
  }

  // Throw during the move of elements which are to the right of `pos` (index 1). (9)
  {
    vector_type v { 1, 2, 5, 6, 7, 8 };

    exception_trigger::push (5);
    EXPECT_TEST_EXCEPTION (v.insert (std::next (v.begin (), 2), 2, val));

    CHECK (6 == v.size ());
    CHECK (  v[0].is_moved);
    CHECK (  v[1].is_moved);
    CHECK (  v[2].is_moved);
    CHECK (! v[3].is_moved);
    CHECK (! v[4].is_moved);
    CHECK (! v[5].is_moved);
  }

  // Throw during construction of a single element at the end (while reallocating). (10)
  {
    vector_type v { 1, 2, 3, 4, 5, 6, 7 };
    vector_type v_save = v;

    exception_trigger::push (0);
    EXPECT_TEST_EXCEPTION (v.insert (v.end (), 1, val));

    CHECK (v == v_save);
  }

  // Throw during the move of elements to the new allocation after construction of one element at
  // the end. (11)
  {
    vector_type v { 1, 2, 3, 4, 5, 6, 7 };
    vector_type v_save = v;

    for (std::size_t i = 1; i <= v.size (); ++i)
    {
      exception_trigger::push (i);
      EXPECT_TEST_EXCEPTION (v.insert (v.end (), 1, val));

      CHECK (v == v_save);
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
