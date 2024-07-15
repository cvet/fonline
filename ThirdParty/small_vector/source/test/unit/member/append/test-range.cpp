/** test-range.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "unit_test_common.hpp"
#include "test_allocators.hpp"

#include <array>

template <typename T, typename Allocator>
struct tester
{
  template <unsigned K>
  using vector_init_type = vector_initializer<T, K, Allocator>;

  template <unsigned K>
  using vector_type = gch::small_vector<T, K, Allocator>;

  tester (void) = default;

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  tester (const Allocator& alloc)
    : m_alloc (alloc)
  { }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  int
  operator() (void)
  {
    // Check vectors with the same number of inline elements.
    // Let N = 2, and let both vectors have N inline elements.
    // States to check:
    //   Combinations of (with repeats):
    //     Inlined:
    //       0 == K elements    (1)
    //       0 < K < N elements (2)
    //       N == K elements    (3)
    //     Allocated:
    //       0 == K elements    (4)
    //       0 < K < N elements (5)
    //       N == K elements    (6)
    //       N < K elements     (7)

    auto reserver = [](vector_type<2>& v) {
      v.reserve (3);
    };

    std::array<vector_init_type<2>, 8> ns {
      vector_init_type<2> { },
      { 1 },
      { 1, 2 },
      { { },      reserver },
      { { 1 },    reserver },
      { { 1, 2 }, reserver },
      { { 1, 2, 3 }, },
      { { 1, 2, 3, 4 }, },
    };

    for (std::size_t i = 0; i < ns.size (); ++i)
    {
      check (ns[i], { });
      check (ns[i], { 1 });
      check (ns[i], { 1, 2 });
      check (ns[i], { 1, 2, 3 });
    }

    // Check vectors with no inline elements.
    check<0> ({ },      { });
    check<0> ({ 1 },    { });
    check<0> ({ 1, 2 }, { });
    check<0> ({ },      { 11 });
    check<0> ({ 1 },    { 11 });
    check<0> ({ 1, 2 }, { 11 });
    check<0> ({ },      { 11, 22 });
    check<0> ({ 1 },    { 11, 22 });
    check<0> ({ 1, 2 }, { 11, 22 });

    return 0;
  }

private:
  template <unsigned N, typename U = T,
            typename std::enable_if<std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (vector_init_type<N> vi, std::initializer_list<T> wi)
  {
    using input_it = gch::test_types::single_pass_iterator<const T *>;
    using forward_it = gch::test_types::multi_pass_iterator<const T *>;

    verify_strong_exception_guarantee (
      [&](vector_type<N>& v) { v.append (wi.begin (), wi.end ()); },
      vi,
      m_alloc);

    verify_strong_exception_guarantee (
      [&](vector_type<N>& v) { v.append (input_it (&*wi.begin ()), input_it (&*wi.end ())); },
      vi,
      m_alloc);

    verify_strong_exception_guarantee (
      [&](vector_type<N>& v) { v.append (forward_it (&*wi.begin ()), forward_it (&*wi.end ())); },
      vi,
      m_alloc);
  }

  template <unsigned N, typename U = T,
            typename std::enable_if<! std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (vector_init_type<N> vi, std::initializer_list<T> wi)
  {
    using input_it = gch::test_types::single_pass_iterator<const T *>;
    using forward_it = gch::test_types::multi_pass_iterator<const T *>;

    vector_type<N> v_cmp (vi.begin (), vi.end ());
    v_cmp.insert (v_cmp.end (), wi.begin (), wi.end ());
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);

      vi (v);

      v.append (wi.begin (), wi.end ());
      CHECK (v == v_cmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);

      vi (v);

      v.append (input_it (&*wi.begin ()), input_it (&*wi.end ()));
      CHECK (v == v_cmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);

      vi (v);

      v.append (forward_it (&*wi.begin ()), forward_it (&*wi.end ()));
      CHECK (v == v_cmp);
    }
  }

  Allocator m_alloc;
};

GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test (void)
{
  using namespace gch::test_types;

  test_with_allocator<tester, std::allocator> ();
  test_with_allocator<tester, sized_allocator, std::uint8_t> ();
  test_with_allocator<tester, fancy_pointer_allocator> ();
  test_with_allocator<tester, allocator_with_id> ();
  test_with_allocator<tester, propagating_allocator_with_id> ();

#ifndef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR

  test_with_allocator<tester, verifying_allocator> ();
  test_with_allocator<tester, non_propagating_verifying_allocator> ();

  // Throw because of a length error.
  std::array<std::int8_t, 4> a { 2, 3, 4, 5 };
  using input_it = single_pass_iterator<std::int8_t *>;

  gch::small_vector_with_allocator<std::int8_t, sized_allocator<std::int8_t, std::uint8_t>> v;
  CHECK (127U == v.max_size ());
  v.assign (v.max_size (), 1);

  auto v_save = v;

  GCH_TRY
  {
    EXPECT_THROW (v.append (a.begin (), a.end ()));
  }
  GCH_CATCH (const std::length_error&)
  { }

  CHECK (v == v_save);

  GCH_TRY
  {
    EXPECT_THROW (v.append (input_it (a.data ()), input_it (std::next (a.data (), a.size ()))));
  }
  GCH_CATCH (const std::length_error&)
  { }

  CHECK (v == v_save);

  GCH_TRY
  {
    EXPECT_THROW (v.append (a.begin (), std::next (a.begin ())));
  }
  GCH_CATCH (const std::length_error&)
  { }

  CHECK (v == v_save);

  GCH_TRY
  {
    EXPECT_THROW (v.append (input_it (a.data ()), std::next (input_it (a.data ()))));
  }
  GCH_CATCH (const std::length_error&)
  { }

  CHECK (v == v_save);

  // Make sure that no elements are getting appended before throwing.
  v.assign (v.max_size () - 1, 1);
  v_save = v;

  GCH_TRY
  {
    EXPECT_THROW (v.append (a.begin (), a.end ()));
  }
  GCH_CATCH (const std::length_error&)
  { }

  CHECK (v == v_save);

  GCH_TRY
  {
    EXPECT_THROW (v.append (input_it (a.data ()), input_it (std::next (a.data (), a.size ()))));
  }
  GCH_CATCH (const std::length_error&)
  { }

  CHECK (v == v_save);

#endif

  return 0;
}
