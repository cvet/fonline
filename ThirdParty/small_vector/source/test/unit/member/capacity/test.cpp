/** test.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "unit_test_common.hpp"
#include "test_allocators.hpp"

static GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test_4 (void)
{
  gch::small_vector<int, 4> v;
  CHECK (4 == v.capacity ());

  v.append ({ 1, 2, 3, 4 });

  // The capacity should not have increased.
  CHECK (4 == v.capacity ());

  v.push_back (5);

  // The capacity will have increased, but the exact amount of increase is unspecified.
  CHECK (4 != v.capacity ());

  return 0;
}

static GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test_3 (void)
{
  // Ensure that the starting capacity is not always 4.
  gch::small_vector<int, 3> v;
  CHECK (3 == v.capacity ());

  return 0;
}

template <typename Allocator>
static GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test_with_alloc (void)
{
  // Make sure the max capacity of a vector tops out at max_size.

  using alloc_type = typename std::allocator_traits<Allocator>::template rebind_alloc<int>;
  gch::small_vector<int, 4, alloc_type> v;

  for (typename std::allocator_traits<alloc_type>::size_type i = 0; i < v.max_size (); ++i)
  {
    v.reserve (i);
    CHECK (v.capacity () <= v.max_size ());
  }

  CHECK (v.capacity () == v.max_size ());

  return 0;
}

template <typename T>
struct tiny_test_allocator
  : std::allocator<T>
{
  using std::allocator<T>::allocator;

  template <typename U>
  struct rebind { using other = tiny_test_allocator<U>; };

  GCH_NODISCARD constexpr
  typename std::allocator<T>::size_type
  max_size (void) const noexcept
  {
    return 11;
  }
};

template <typename T>
constexpr
bool
operator!= (const tiny_test_allocator<T>& lhs,
            const tiny_test_allocator<T>& rhs) noexcept
{
  return ! (lhs == rhs);
}

template <typename T, typename U>
constexpr
bool
operator!= (const tiny_test_allocator<T>& lhs,
            const tiny_test_allocator<U>& rhs) noexcept
{
  return ! (lhs == rhs);
}

GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test (void)
{
  CHECK (0 == test_4 ());
  CHECK (0 == test_3 ());

  using size_alloc = gch::test_types::sized_allocator<int, std::uint8_t>;
  using r_size_alloc = typename std::allocator_traits<size_alloc>::template rebind_alloc<int>;
  gch::small_vector<int, 4, r_size_alloc> v;
  CHECK (std::numeric_limits<std::uint8_t>::max () / sizeof (int) == v.max_size ());

  CHECK (0 == test_with_alloc<gch::test_types::sized_allocator<int, std::uint8_t>> ());

  using tiny_alloc = tiny_test_allocator<int>;
  using r_tiny_alloc = typename std::allocator_traits<tiny_alloc>::template rebind_alloc<int>;
  gch::small_vector<int, 4, r_tiny_alloc> w;
  CHECK (r_tiny_alloc { }.max_size () == 11);
  CHECK (r_tiny_alloc { }.max_size () == w.max_size ());

  CHECK (0 == test_with_alloc<tiny_test_allocator<int>> ());

  return 0;
}
