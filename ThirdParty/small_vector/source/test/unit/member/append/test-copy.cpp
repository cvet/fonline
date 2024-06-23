/** test-copy.cpp
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
    check_unequal_inline ();
    check_no_inline ();
    check_equal_inline ();

    return 0;
  }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  int
  check_unequal_inline (void)
  {
    // Check vectors with the same number of inline elements.
    // Let N = 3, M = 5.
    // States to check:
    //   Combinations of (with repeats):
    //     Inlined:
    //       0 == K elements.    (1)
    //       K < N elements.     (2)
    //       N == K elements.    (3)
    //       N < K < M elements. (4) (only for M)
    //       M == K elements.    (5) (only for M)
    //     Allocated:
    //       0 == K elements.    (7)
    //       K < N elements.     (8)
    //       N == K elements.    (9)
    //       N < K < M elements. (10)
    //       M == K elements.    (11)
    //       M < K elements.     (12)
    //   for M and N (99 total cases).

    auto n_reserver = [](vector_type<2>& v) {
      v.reserve (3);
    };

    auto m_reserver = [](vector_type<4>& v) {
      v.reserve (5);
    };

    std::array<vector_init_type<2>, 9> ns {
      vector_init_type<2> { },
      { 1 },
      { 1, 2 },
      { { },               n_reserver },
      { { 1 },             n_reserver },
      { { 1, 2 },          n_reserver },
      { { 1, 2, 3 },       n_reserver },
      { { 1, 2, 3, 4 },    n_reserver },
      { { 1, 2, 3, 4, 5 }, n_reserver },
    };

    std::array<vector_init_type<4>, 11> ms {
      vector_init_type<4> { },
      { 1 },
      { 1, 2 },
      { 1, 2, 3 },
      { 1, 2, 3, 4 },
      { { },               m_reserver },
      { { 1 },             m_reserver },
      { { 1, 2 },          m_reserver },
      { { 1, 2, 3 },       m_reserver },
      { { 1, 2, 3, 4 },    m_reserver },
      { { 1, 2, 3, 4, 5 }, m_reserver },
    };

    for (std::size_t i = 0; i < ns.size (); ++i)
      for (std::size_t j = 0; j < ms.size (); ++j)
        check (ns[i], ms[j]);
    return 0;
  }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  int
  check_no_inline (void)
  {
    // Check vectors with no inline elements.
    check<0, 0> ({ },      { });
    check<0, 0> ({ 1 },    { });
    check<0, 0> ({ 1, 2 }, { });
    check<0, 0> ({ },      { 11 });
    check<0, 0> ({ 1 },    { 11 });
    check<0, 0> ({ 1, 2 }, { 11 });
    check<0, 0> ({ },      { 11, 22 });
    check<0, 0> ({ 1 },    { 11, 22 });
    check<0, 0> ({ 1, 2 }, { 11, 22 });
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
    // (28 total cases).

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
      { { 1, 2, 3 } },
      { { 1, 2, 3, 4 } },
    };

    std::array<vector_init_type<2>, 8> ms {
      vector_init_type<2> { },
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
    return 0;
  }

private:
  template <unsigned N, unsigned M, typename U = T,
            typename std::enable_if<std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  void
  check (vector_init_type<N> ni, vector_init_type<M> mi)
  {
    verify_strong_exception_guarantee (
      [](vector_type<N>& n, vector_type<M>& m) { n.append (m); },
      ni,
      mi,
      m_lhs_alloc,
      m_rhs_alloc);

    verify_strong_exception_guarantee (
      [](vector_type<N>& n, vector_type<M>& m) { m.append (n); },
      ni,
      mi,
      m_lhs_alloc,
      m_rhs_alloc);
  }

  template <unsigned N, unsigned M, typename U = T,
            typename std::enable_if<! std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (vector_init_type<N> ni, vector_init_type<M> mi)
  {
    vector_type<N> n_cmp (ni.begin (), ni.end (), m_lhs_alloc);
    vector_type<M> m_cmp (mi.begin (), mi.end (), m_rhs_alloc);
    n_cmp.insert (n_cmp.end (), mi.begin (), mi.end ());
    m_cmp.insert (m_cmp.end (), ni.begin (), ni.end ());
    {
      // vector_type<M> (mi) -> vector_type<N> (ni)
      vector_type<N> n (ni.begin (), ni.end (), m_lhs_alloc);
      vector_type<M> m (mi.begin (), mi.end (), m_rhs_alloc);

      ni (n);
      mi (m);

      n.append (m);
      CHECK (n == n_cmp);
    }
    {
      // vector_type<N> (ni) -> vector_type<M> (mi)
      vector_type<N> n (ni.begin (), ni.end (), m_lhs_alloc);
      vector_type<M> m (mi.begin (), mi.end (), m_rhs_alloc);

      ni (n);
      mi (m);

      m.append (n);
      CHECK (m == m_cmp);
    }
    {
      // vector_type<M> (ni) -> vector_type<N> (mi)
      vector_type<N> n (mi.begin (), mi.end (), m_lhs_alloc);
      vector_type<M> m (ni.begin (), ni.end (), m_rhs_alloc);

      ni (n);
      mi (m);

      n.append (m);
      CHECK (n == m_cmp);
    }
    {
      // vector_type<N> (mi) -> vector_type<M> (ni)
      vector_type<N> n (mi.begin (), mi.end (), m_lhs_alloc);
      vector_type<M> m (ni.begin (), ni.end (), m_rhs_alloc);

      ni (n);
      mi (m);

      m.append (n);
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

  // Throw because of a length error.
  gch::small_vector_with_allocator<std::int8_t, sized_allocator<std::int8_t, std::uint8_t>> v;
  decltype (v) w { 2, 3, 4, 5 };
  CHECK (127U == v.max_size ());
  v.assign (v.max_size (), 1);
  auto v_save = v;

  GCH_TRY
  {
    EXPECT_THROW (v.append (w));
  }
  GCH_CATCH (const std::length_error&)
  { }

  CHECK (v == v_save);

  // Make sure that no elements are getting appended before throwing.
  v.assign (v.max_size () - 1, 1);
  v_save = v;

  GCH_TRY
  {
    EXPECT_THROW (v.append (w));
  }
  GCH_CATCH (const std::length_error&)
  { }

  CHECK (v == v_save);

#endif

  return 0;
}
