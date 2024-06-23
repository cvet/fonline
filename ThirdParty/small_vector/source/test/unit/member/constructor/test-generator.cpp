/** test-generator.cpp
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
    T (*gs[]) (void) {
#ifndef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
      [] { static int val = 0; return T (++val); },
#endif
      [] { T val (1); return val;  },
      [] { return T (2); },
      [] { T val1 (3); T val2 (val1); return T (val2); },
    };

    std::for_each (std::begin (gs), std::end (gs), [&](T g (void)) {
      for (typename vector_type<2>::size_type i = 0; i < 5; ++i)
      {
        check<0> (i, g);
        check<1> (i, g);
        check<2> (i, g);
      }
    });

    return 0;
  }

private:
  template <unsigned N, typename Generator, typename U = T,
            typename std::enable_if<std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  void
  check (typename vector_type<N>::size_type count, Generator g)
  {
    verify_basic_exception_safety (
      [&] (vector_type<N>&)
      {
        vector_type<N> v (count, g);
        (void)v;
      },
      vector_init_type<N> { },
      m_alloc);

    verify_basic_exception_safety (
      [&] (vector_type<N>&)
      {
        vector_type<N> v (count, g, m_alloc);
        (void)v;
      },
      vector_init_type<N> { },
      m_alloc);
  }

  template <unsigned N, typename Generator, typename U = T,
            typename std::enable_if<! std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (typename vector_type<N>::size_type count, Generator g)
  {
    {
      vector_type<N> v (count, g);
      CHECK (v.size () == count);
    }
    {
      vector_type<N> v (count, g, m_alloc);
      CHECK (v.size () == count);
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
#endif

  return 0;
}
