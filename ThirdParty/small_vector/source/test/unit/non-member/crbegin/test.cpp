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

  CHECK (gch::crbegin (m) == gch::crbegin (m));
  CHECK (gch::crbegin (m) == gch::crbegin (c));
  CHECK (gch::crbegin (c) == gch::crbegin (m));
  CHECK (gch::crbegin (c) == gch::crbegin (c));

  CHECK (gch::crbegin (m) == m.crbegin ());
  CHECK (gch::crbegin (m) == c.crbegin ());
  CHECK (gch::crbegin (c) == m.crbegin ());
  CHECK (gch::crbegin (c) == c.crbegin ());

  CHECK (m.crbegin () == gch::crbegin (m));
  CHECK (m.crbegin () == gch::crbegin (c));
  CHECK (c.crbegin () == gch::crbegin (m));
  CHECK (c.crbegin () == gch::crbegin (c));

#if defined (__cplusplus) && __cplusplus >= 201402L
  CHECK (std::crbegin (m) == std::crbegin (m));
  CHECK (std::crbegin (m) == std::crbegin (c));
  CHECK (std::crbegin (c) == std::crbegin (m));
  CHECK (std::crbegin (c) == std::crbegin (c));

  CHECK (std::crbegin (m) == gch::crbegin (m));
  CHECK (std::crbegin (m) == gch::crbegin (c));
  CHECK (std::crbegin (c) == gch::crbegin (m));
  CHECK (std::crbegin (c) == gch::crbegin (c));

  CHECK (gch::crbegin (m) == std::crbegin (m));
  CHECK (gch::crbegin (m) == std::crbegin (c));
  CHECK (gch::crbegin (c) == std::crbegin (m));
  CHECK (gch::crbegin (c) == std::crbegin (c));

  CHECK (std::crbegin (m) == m.crbegin ());
  CHECK (std::crbegin (m) == c.crbegin ());
  CHECK (std::crbegin (c) == m.crbegin ());
  CHECK (std::crbegin (c) == c.crbegin ());

  CHECK (m.crbegin () == std::crbegin (m));
  CHECK (m.crbegin () == std::crbegin (c));
  CHECK (c.crbegin () == std::crbegin (m));
  CHECK (c.crbegin () == std::crbegin (c));
#endif

  CHECK (crbegin (m) == crbegin (m));
  CHECK (crbegin (m) == crbegin (c));
  CHECK (crbegin (c) == crbegin (m));
  CHECK (crbegin (c) == crbegin (c));

  CHECK (crbegin (m) == m.crbegin ());
  CHECK (crbegin (m) == c.crbegin ());
  CHECK (crbegin (c) == m.crbegin ());
  CHECK (crbegin (c) == c.crbegin ());

  CHECK (m.crbegin () == crbegin (m));
  CHECK (m.crbegin () == crbegin (c));
  CHECK (c.crbegin () == crbegin (m));
  CHECK (c.crbegin () == crbegin (c));

  return 0;
}
