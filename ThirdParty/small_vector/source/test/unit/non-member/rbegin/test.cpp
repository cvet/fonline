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
  gch::small_vector<int> m { 1, 2, 3 };
  const auto& c = m;

  CHECK (gch::rbegin (m) == gch::rbegin (m));
  CHECK (gch::rbegin (m) == gch::rbegin (c));
  CHECK (gch::rbegin (c) == gch::rbegin (m));
  CHECK (gch::rbegin (c) == gch::rbegin (c));

  CHECK (gch::rbegin (m) == m.rbegin ());
  CHECK (gch::rbegin (m) == c.rbegin ());
  CHECK (gch::rbegin (c) == m.rbegin ());
  CHECK (gch::rbegin (c) == c.rbegin ());

  CHECK (m.rbegin () == gch::rbegin (m));
  CHECK (m.rbegin () == gch::rbegin (c));
  CHECK (c.rbegin () == gch::rbegin (m));
  CHECK (c.rbegin () == gch::rbegin (c));

#if defined (__cplusplus) && __cplusplus >= 201402L
  CHECK (std::rbegin (m) == std::rbegin (m));
  CHECK (std::rbegin (m) == std::rbegin (c));
  CHECK (std::rbegin (c) == std::rbegin (m));
  CHECK (std::rbegin (c) == std::rbegin (c));

  CHECK (std::rbegin (m) == gch::rbegin (m));
  CHECK (std::rbegin (m) == gch::rbegin (c));
  CHECK (std::rbegin (c) == gch::rbegin (m));
  CHECK (std::rbegin (c) == gch::rbegin (c));

  CHECK (gch::rbegin (m) == std::rbegin (m));
  CHECK (gch::rbegin (m) == std::rbegin (c));
  CHECK (gch::rbegin (c) == std::rbegin (m));
  CHECK (gch::rbegin (c) == std::rbegin (c));

  CHECK (std::rbegin (m) == m.rbegin ());
  CHECK (std::rbegin (m) == c.rbegin ());
  CHECK (std::rbegin (c) == m.rbegin ());
  CHECK (std::rbegin (c) == c.rbegin ());

  CHECK (m.rbegin () == std::rbegin (m));
  CHECK (m.rbegin () == std::rbegin (c));
  CHECK (c.rbegin () == std::rbegin (m));
  CHECK (c.rbegin () == std::rbegin (c));
#endif

  CHECK (rbegin (m) == rbegin (m));
  CHECK (rbegin (m) == rbegin (c));
  CHECK (rbegin (c) == rbegin (m));
  CHECK (rbegin (c) == rbegin (c));

  CHECK (rbegin (m) == m.rbegin ());
  CHECK (rbegin (m) == c.rbegin ());
  CHECK (rbegin (c) == m.rbegin ());
  CHECK (rbegin (c) == c.rbegin ());

  CHECK (m.rbegin () == rbegin (m));
  CHECK (m.rbegin () == rbegin (c));
  CHECK (c.rbegin () == rbegin (m));
  CHECK (c.rbegin () == rbegin (c));

  return 0;
}
