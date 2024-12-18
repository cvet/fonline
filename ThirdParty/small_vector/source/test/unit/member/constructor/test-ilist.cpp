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
    check<0> ({ });
    check<0> ({ 1 });
    check<0> ({ 1, 2 });

    check<1> ({ });
    check<1> ({ 1 });
    check<1> ({ 1, 2 });

    check<2> ({ });
    check<2> ({ 1 });
    check<2> ({ 1, 2 });

    check<3> ({ });
    check<3> ({ 1 });
    check<3> ({ 1, 2 });

    return 0;
  }

private:
  template <unsigned N, typename U = T,
            typename std::enable_if<std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  void
  check (std::initializer_list<T> mi)
  {
    verify_basic_exception_safety (
      [&] (const vector_type<0>&)
      {
        vector_type<N> n (mi);
        (void)n;
      },
      vector_init_type<0> { },
      m_alloc);

    verify_basic_exception_safety (
      [&] (const vector_type<0>&)
      {
        vector_type<N> n (mi, m_alloc);
        (void)n;
      },
      vector_init_type<0> { },
      m_alloc);
  }

  template <unsigned N, typename U = T,
            typename std::enable_if<! std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (std::initializer_list<T> mi)
  {
    {
      vector_type<N> v (mi);
      CHECK (mi.size () == v.size () && std::equal (mi.begin (), mi.end (), v.begin ()));
    }
    {
      vector_type<N> v (mi, m_alloc);
      CHECK (mi.size () == v.size () && std::equal (mi.begin (), mi.end (), v.begin ()));
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
