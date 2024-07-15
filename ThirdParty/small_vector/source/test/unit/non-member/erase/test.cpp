/** test.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "unit_test_common.hpp"

GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test (void)
{
  // Attempt to erase from an empty container.
  {
    gch::small_vector<int> v { };
    auto ret = erase (v, -1);

    CHECK (v.empty ());
    CHECK (0 == ret);
  }

  // Attempt to erase an element not contained by the container.
  {
    gch::small_vector<int> v { 1, 2, 3 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (0 == ret);
  }

  // Erase one element at the beginning.
  {
    gch::small_vector<int> v { -1, 1, 2, 3 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (1 == ret);
  }

  // Erase one element in the middle.
  {
    gch::small_vector<int> v { 1, 2, -1, 3 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (1 == ret);
  }

  // Erase one element at the end.
  {
    gch::small_vector<int> v { 1, 2, 3, -1 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (1 == ret);
  }

  // Erase multiple elements at the beginning.
  {
    gch::small_vector<int> v { -1, -1, 1, 2, 3 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (2 == ret);
  }

  // Erase multiple elements in the middle.
  {
    gch::small_vector<int> v { 1, 2, -1, -1, 3 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (2 == ret);
  }

  // Erase multiple elements at the end.
  {
    gch::small_vector<int> v { 1, 2, 3, -1, -1 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (2 == ret);
  }

  // Erase multiple elements located at different points in the container.
  {
    gch::small_vector<int> v { 1, -1, 2, -1, 3 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (2 == ret);
  }
  {
    gch::small_vector<int> v { -1, 1, -1, 2, -1, 3 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (3 == ret);
  }
  {
    gch::small_vector<int> v { 1, -1, 2, -1, 3, -1 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (3 == ret);
  }
  {
    gch::small_vector<int> v { -1, 1, -1, 2, -1, 3, -1 };
    auto ret = erase (v, -1);

    CHECK (decltype (v) { 1, 2, 3 } == v);
    CHECK (4 == ret);
  }

  // Erase all elements from a container.
  {
    gch::small_vector<int> v { -1, -1, -1 };
    auto ret = erase (v, -1);

    CHECK (v.empty ());
    CHECK (3 == ret);
  }

  return 0;
}
