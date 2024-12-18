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

  CHECK (gch::crend (m) == gch::crend (m));
  CHECK (gch::crend (m) == gch::crend (c));
  CHECK (gch::crend (c) == gch::crend (m));
  CHECK (gch::crend (c) == gch::crend (c));

  CHECK (gch::crend (m) == m.crend ());
  CHECK (gch::crend (m) == c.crend ());
  CHECK (gch::crend (c) == m.crend ());
  CHECK (gch::crend (c) == c.crend ());

  CHECK (m.crend () == gch::crend (m));
  CHECK (m.crend () == gch::crend (c));
  CHECK (c.crend () == gch::crend (m));
  CHECK (c.crend () == gch::crend (c));

#if defined (__cplusplus) && __cplusplus >= 201402L
  CHECK (std::crend (m) == std::crend (m));
  CHECK (std::crend (m) == std::crend (c));
  CHECK (std::crend (c) == std::crend (m));
  CHECK (std::crend (c) == std::crend (c));

  CHECK (std::crend (m) == gch::crend (m));
  CHECK (std::crend (m) == gch::crend (c));
  CHECK (std::crend (c) == gch::crend (m));
  CHECK (std::crend (c) == gch::crend (c));

  CHECK (gch::crend (m) == std::crend (m));
  CHECK (gch::crend (m) == std::crend (c));
  CHECK (gch::crend (c) == std::crend (m));
  CHECK (gch::crend (c) == std::crend (c));

  CHECK (std::crend (m) == m.crend ());
  CHECK (std::crend (m) == c.crend ());
  CHECK (std::crend (c) == m.crend ());
  CHECK (std::crend (c) == c.crend ());

  CHECK (m.crend () == std::crend (m));
  CHECK (m.crend () == std::crend (c));
  CHECK (c.crend () == std::crend (m));
  CHECK (c.crend () == std::crend (c));
#endif

  CHECK (crend (m) == crend (m));
  CHECK (crend (m) == crend (c));
  CHECK (crend (c) == crend (m));
  CHECK (crend (c) == crend (c));

  CHECK (crend (m) == m.crend ());
  CHECK (crend (m) == c.crend ());
  CHECK (crend (c) == m.crend ());
  CHECK (crend (c) == c.crend ());

  CHECK (m.crend () == crend (m));
  CHECK (m.crend () == crend (c));
  CHECK (c.crend () == crend (m));
  CHECK (c.crend () == crend (c));

  return 0;
}
