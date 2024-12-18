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

  CHECK (gch::rend (m) == gch::rend (m));
  CHECK (gch::rend (m) == gch::rend (c));
  CHECK (gch::rend (c) == gch::rend (m));
  CHECK (gch::rend (c) == gch::rend (c));

  CHECK (gch::rend (m) == m.rend ());
  CHECK (gch::rend (m) == c.rend ());
  CHECK (gch::rend (c) == m.rend ());
  CHECK (gch::rend (c) == c.rend ());

  CHECK (m.rend () == gch::rend (m));
  CHECK (m.rend () == gch::rend (c));
  CHECK (c.rend () == gch::rend (m));
  CHECK (c.rend () == gch::rend (c));

#if defined (__cplusplus) && __cplusplus >= 201402L
  CHECK (std::rend (m) == std::rend (m));
  CHECK (std::rend (m) == std::rend (c));
  CHECK (std::rend (c) == std::rend (m));
  CHECK (std::rend (c) == std::rend (c));

  CHECK (std::rend (m) == gch::rend (m));
  CHECK (std::rend (m) == gch::rend (c));
  CHECK (std::rend (c) == gch::rend (m));
  CHECK (std::rend (c) == gch::rend (c));

  CHECK (gch::rend (m) == std::rend (m));
  CHECK (gch::rend (m) == std::rend (c));
  CHECK (gch::rend (c) == std::rend (m));
  CHECK (gch::rend (c) == std::rend (c));

  CHECK (std::rend (m) == m.rend ());
  CHECK (std::rend (m) == c.rend ());
  CHECK (std::rend (c) == m.rend ());
  CHECK (std::rend (c) == c.rend ());

  CHECK (m.rend () == std::rend (m));
  CHECK (m.rend () == std::rend (c));
  CHECK (c.rend () == std::rend (m));
  CHECK (c.rend () == std::rend (c));
#endif

  CHECK (rend (m) == rend (m));
  CHECK (rend (m) == rend (c));
  CHECK (rend (c) == rend (m));
  CHECK (rend (c) == rend (c));

  CHECK (rend (m) == m.rend ());
  CHECK (rend (m) == c.rend ());
  CHECK (rend (c) == m.rend ());
  CHECK (rend (c) == c.rend ());

  CHECK (m.rend () == rend (m));
  CHECK (m.rend () == rend (c));
  CHECK (c.rend () == rend (m));
  CHECK (c.rend () == rend (c));

  return 0;
}
