/** test.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "unit_test_common.hpp"

template <typename T, unsigned N, unsigned M>
GCH_SMALL_VECTOR_TEST_CONSTEXPR
void
test_with_capacity (void)
{
  // Test both empty.
  {
    gch::small_vector<T, N> v { };
    gch::small_vector<T, M> w { };
    CHECK (! (v < w));
    CHECK (! (w < v));
  }

  // Test v < w with one empty.
  {
    gch::small_vector<T, N> v { };
    gch::small_vector<T, M> w { 1, 2 };
    CHECK (   v < w);
    CHECK (! (w < v));
  }

  // Test v < w of same length.
  {
    gch::small_vector<T> v { 1, 2 };
    gch::small_vector<T> w { 2, 3 };
    CHECK (   v < w);
    CHECK (! (w < v));
  }

  // Test v < w of differing length.
  {
    gch::small_vector<T, N> v { 1, 2 };
    gch::small_vector<T, M> w { 2, 3, 4 };
    CHECK (   v < w);
    CHECK (! (w < v));
  }

  // Test v < w of same length where v == w for index = 0.
  {
    gch::small_vector<T, N> v { 1, 2 };
    gch::small_vector<T, M> w { 1, 3 };
    CHECK (   v < w);
    CHECK (! (w < v));
  }

  // Test v < w of differing length where v == w for all indices of v.
  {
    gch::small_vector<T, N> v { 1, 2 };
    gch::small_vector<T, M> w { 1, 2, 0 };
    CHECK (   v < w);
    CHECK (! (w < v));
  }

  // Test v == w.
  {
    gch::small_vector<T, N> v { 1, 2 };
    gch::small_vector<T, M> w { 1, 2 };
    CHECK (! (v < w));
    CHECK (! (w < v));
  }
}

template <typename T>
GCH_SMALL_VECTOR_TEST_CONSTEXPR
void
test_with_type (void)
{
  test_with_capacity<T, 0, 0> ();
  test_with_capacity<T, 0, 1> ();
  test_with_capacity<T, 0, 2> ();
  test_with_capacity<T, 0, 3> ();
  test_with_capacity<T, 0, 4> ();

  test_with_capacity<T, 1, 0> ();
  test_with_capacity<T, 1, 1> ();
  test_with_capacity<T, 1, 2> ();
  test_with_capacity<T, 1, 3> ();
  test_with_capacity<T, 1, 4> ();

  test_with_capacity<T, 2, 0> ();
  test_with_capacity<T, 2, 1> ();
  test_with_capacity<T, 2, 2> ();
  test_with_capacity<T, 2, 3> ();
  test_with_capacity<T, 2, 4> ();

  test_with_capacity<T, 3, 0> ();
  test_with_capacity<T, 3, 1> ();
  test_with_capacity<T, 3, 2> ();
  test_with_capacity<T, 3, 3> ();
  test_with_capacity<T, 3, 4> ();

  test_with_capacity<T, 4, 0> ();
  test_with_capacity<T, 4, 1> ();
  test_with_capacity<T, 4, 2> ();
  test_with_capacity<T, 4, 3> ();
  test_with_capacity<T, 4, 4> ();

  test_with_capacity<T, 5, 0> ();
  test_with_capacity<T, 5, 1> ();
  test_with_capacity<T, 5, 2> ();
  test_with_capacity<T, 5, 3> ();
  test_with_capacity<T, 5, 4> ();
}


GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test (void)
{
  test_with_type<int> ();
  test_with_type<gch::test_types::trivially_copyable_data_base> ();

  return 0;
}
