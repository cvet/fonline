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
    T val (-1);

    for (typename vector_type<2>::size_type i = 0; i < 5; ++i)
    {
      check<0> (i, val);
      check<1> (i, val);
      check<2> (i, val);
    }

    return 0;
  }

private:
  template <unsigned N, typename U = T,
            typename std::enable_if<std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  void
  check (typename vector_type<N>::size_type count, const T& val)
  {
    verify_basic_exception_safety (
      [&] (vector_type<N>&)
      {
        vector_type<N> v (count);
        (void)v;
      },
      vector_init_type<N> { },
      m_alloc);

    verify_basic_exception_safety (
      [&] (vector_type<N>&)
      {
        vector_type<N> v (count, m_alloc);
        (void)v;
      },
      vector_init_type<N> { },
      m_alloc);

    verify_basic_exception_safety (
      [&] (vector_type<N>&)
      {
        vector_type<N> v (count, val);
        (void)v;
      },
      vector_init_type<N> { },
      m_alloc);

    verify_basic_exception_safety (
      [&] (vector_type<N>&)
      {
        vector_type<N> v (count, val, m_alloc);
        (void)v;
      },
      vector_init_type<N> { },
      m_alloc);
  }

  template <unsigned N, typename U = T,
            typename std::enable_if<! std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (typename vector_type<N>::size_type count, const T& val)
  {
    {
      vector_type<N> v (count);
      CHECK (v.size () == count);
      CHECK (v.end () == std::find_if_not (v.begin (), v.end (), [](T& c) { return T () == c; }));
    }
    {
      vector_type<N> v (count, m_alloc);
      CHECK (v.size () == count);
      CHECK (v.end () == std::find_if_not (v.begin (), v.end (), [](T& c) { return T () == c; }));
    }
    {
      vector_type<N> v (count, val);
      CHECK (v.size () == count);
      CHECK (v.end () == std::find_if_not (v.begin (), v.end (), [&](T& c) { return c == val; }));
    }
    {
      vector_type<N> v (count, val, m_alloc);
      CHECK (v.size () == count);
      CHECK (v.end () == std::find_if_not (v.begin (), v.end (), [&](T& c) { return c == val; }));
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
  using sized_vec = gch::small_vector_with_allocator_tp<std::int8_t, sized_allocator, std::uint8_t>;
  GCH_TRY
  {
    EXPECT_THROW (sized_vec v (128U));
  }
  GCH_CATCH (const std::length_error&)
  { }

  GCH_TRY
  {
    EXPECT_THROW (sized_vec v (128U, static_cast<std::int8_t> (-1)));
  }
  GCH_CATCH (const std::length_error&)
  { }

#endif

  return 0;
}
