/** test.cpp
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
  tester (const Allocator& lhs_alloc, const Allocator& rhs_alloc)
    : m_lhs_alloc (lhs_alloc),
      m_rhs_alloc (rhs_alloc)
  { }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  int
  operator() (void)
  {
    check_with_inline_capacity<2> ();
    check_with_inline_capacity<0> ();
    return 0;
  }

private:
  template <unsigned N>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check_with_inline_capacity (void)
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
    // (28 total cases).

    auto reserver = [](vector_type<N>& v) {
      v.reserve (3);
    };

    std::array<vector_init_type<N>, 8> ns {
      vector_init_type<N> { },
      { 1 },
      { 1, 2 },
      { { },      reserver },
      { { 1 },    reserver },
      { { 1, 2 }, reserver },
      { { 1, 2, 3 } },
      { { 1, 2, 3, 4 } },
    };

    std::array<vector_init_type<N>, 8> ms {
      vector_init_type<N> { },
      { 11 },
      { 11, 22 },
      { { },        reserver },
      { { 11 },     reserver },
      { { 11, 22 }, reserver },
      { 11, 22, 33 },
      { 11, 22, 33, 44 },
    };

    for (std::size_t i = 0; i < ns.size (); ++i)
      for (std::size_t j = i; j < ms.size (); ++j)
        check (ns[i], ms[j]);
  }

  template <unsigned N, typename U = T,
            typename std::enable_if<std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  void
  check (vector_init_type<N> ni, vector_init_type<N> mi)
  {
    verify_basic_exception_safety (
      [] (vector_type<N>& n, vector_type<N>& m) { n.swap (m); },
      ni,
      mi,
      m_lhs_alloc,
      m_rhs_alloc);
  }

  template <unsigned N, typename U = T,
            typename std::enable_if<! std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (vector_init_type<N> ni, vector_init_type<N> mi)
  {
    vector_type<N> n_cmp (ni.begin (), ni.end (), m_lhs_alloc);
    vector_type<N> m_cmp (mi.begin (), mi.end (), m_rhs_alloc);
    {
      // vector_type<N> (mi) -> vector_type<N> (ni)
      vector_type<N> n (ni.begin (), ni.end (), m_lhs_alloc);
      vector_type<N> m (mi.begin (), mi.end (), m_rhs_alloc);

      ni (n);
      mi (m);

      n.swap (m);
      CHECK (n == m_cmp);
      CHECK (m == n_cmp);
    }
    {
      // vector_type<N> (ni) -> vector_type<N> (mi)
      vector_type<N> n (ni.begin (), ni.end (), m_lhs_alloc);
      vector_type<N> m (mi.begin (), mi.end (), m_rhs_alloc);

      ni (n);
      mi (m);

      m.swap (n);
      CHECK (n == m_cmp);
      CHECK (m == n_cmp);
    }
  }

  Allocator m_lhs_alloc;
  Allocator m_rhs_alloc;
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
