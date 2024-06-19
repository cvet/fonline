/** test-elem.cpp
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
    check_no_inline ();
    check_equal_inline ();

    return 0;
  }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  int
  check_no_inline (void)
  {
    // Check vectors with no inline elements.
    T val (-1);
    check<0> ({ },       0, val);
    check<0> ({ 1 },     0, val);
    check<0> ({ 1, 2 },  0, val);
    check<0> ({ },       1, val);
    check<0> ({ 1 },     1, val);
    check<0> ({ 1, 2 },  1, val);
    check<0> ({ },       2, val);
    check<0> ({ 1 },     2, val);
    check<0> ({ 1, 2 },  2, val);
    return 0;
  }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  int
  check_equal_inline (void)
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

    T val (-1);

    for (typename vector_type<2>::size_type count = 0; count < 6; ++count)
      for (std::size_t i = 0; i < ns.size (); ++i)
        check (ns[i], count, val);

    return 0;
  }

private:
  template <unsigned N, typename U = T,
            typename std::enable_if<std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (vector_init_type<N> vi, typename vector_type<N>::size_type count, const T& val)
  {
    verify_basic_exception_safety (
      [&] (vector_type<N>& v) { v.assign (count, val); },
      vi,
      m_alloc);
  }

  template <unsigned N, typename U = T,
            typename std::enable_if<! std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (vector_init_type<N> vi, typename vector_type<N>::size_type count, const T& val)
  {
    vector_type<N> v_cmp (count, val);
    vector_type<N> v (vi.begin (), vi.end (), m_alloc);

    vi (v);

    v.assign (count, val);
    CHECK (v == v_cmp);
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
#endif

  return 0;
}
