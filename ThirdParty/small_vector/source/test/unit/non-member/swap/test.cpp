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
  gch::small_vector<int> v { 1, 2, 3 };
  gch::small_vector<int> w { 1, 2 };

  auto non_member_call = [](decltype (v) x, decltype (w) y) {
    gch::swap (x, y);
    return x;
  };

  auto member_call = [](decltype (v) x, decltype (w) y) {
    x.swap (y);
    return x;
  };

  auto std_call = [](decltype (v) x, decltype (w) y) {
    std::swap (x, y);
    return x;
  };

  auto adl_call = [](decltype (v) x, decltype (w) y) {
    swap (x, y);
    return x;
  };

  CHECK (non_member_call (v, w) == member_call (v, w));
  CHECK (non_member_call (w, v) == member_call (w, v));

  CHECK (non_member_call (v, w) == std_call (v, w));
  CHECK (non_member_call (w, v) == std_call (w, v));

  CHECK (non_member_call (v, w) == adl_call (v, w));
  CHECK (non_member_call (w, v) == adl_call (w, v));

  return 0;
}
