/** test-move.cpp
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
  tester (const Allocator& alloc, const Allocator& init_alloc)
    : m_alloc (alloc),
      m_init_alloc (init_alloc)
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

    for (std::size_t i = 0; i < ns.size (); ++i)
    {
      check<0> (ns[i]);
      check<1> (ns[i]);
      check<2> (ns[i]);
      check<3> (ns[i]);
      check<4> (ns[i]);
      check<5> (ns[i]);
    }

    // Check vectors with no inline elements.
    check<0, 0> ({ });
    check<0, 0> ({ 1 });
    check<0, 0> ({ 1, 2 });

    return 0;
  }

private:
  template <unsigned N, unsigned M, typename U = T,
            typename std::enable_if<std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  void
  check (vector_init_type<M> mi)
  {
    verify_basic_exception_safety (
      [] (vector_type<M>& m)
      {
        vector_type<N> n (std::move (m));
        (void)n;
      },
      mi,
      m_alloc);

    verify_basic_exception_safety (
      [&] (vector_type<M>& m)
      {
        vector_type<N> n (std::move (m), m_init_alloc);
        (void)n;
      },
      mi,
      m_alloc);
  }

  template <unsigned N, unsigned M, typename U = T,
            typename std::enable_if<! std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (vector_init_type<M> mi)
  {
    vector_type<M> m_cmp (mi.begin (), mi.end ());
    {
      vector_type<M> m (mi.begin (), mi.end (), m_alloc);
      mi (m);

      vector_type<N> v (std::move (m));
      CHECK (v == m_cmp);
    }
    {
      vector_type<M> m (mi.begin (), mi.end (), m_alloc);
      mi (m);

      vector_type<N> v (std::move (m), m_init_alloc);
      CHECK (v == m_cmp);
    }
  }

  Allocator m_alloc;
  Allocator m_init_alloc;
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
