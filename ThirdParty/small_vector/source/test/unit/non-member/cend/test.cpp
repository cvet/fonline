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

  CHECK (gch::cend (m) == gch::cend (m));
  CHECK (gch::cend (m) == gch::cend (c));
  CHECK (gch::cend (c) == gch::cend (m));
  CHECK (gch::cend (c) == gch::cend (c));

  CHECK (gch::cend (m) == m.cend ());
  CHECK (gch::cend (m) == c.cend ());
  CHECK (gch::cend (c) == m.cend ());
  CHECK (gch::cend (c) == c.cend ());

  CHECK (m.cend () == gch::cend (m));
  CHECK (m.cend () == gch::cend (c));
  CHECK (c.cend () == gch::cend (m));
  CHECK (c.cend () == gch::cend (c));

#if defined (__cplusplus) && __cplusplus >= 201402L
  CHECK (std::cend (m) == std::cend (m));
  CHECK (std::cend (m) == std::cend (c));
  CHECK (std::cend (c) == std::cend (m));
  CHECK (std::cend (c) == std::cend (c));
  
  CHECK (std::cend (m) == gch::cend (m));
  CHECK (std::cend (m) == gch::cend (c));
  CHECK (std::cend (c) == gch::cend (m));
  CHECK (std::cend (c) == gch::cend (c));
  
  CHECK (gch::cend (m) == std::cend (m));
  CHECK (gch::cend (m) == std::cend (c));
  CHECK (gch::cend (c) == std::cend (m));
  CHECK (gch::cend (c) == std::cend (c));

  CHECK (std::cend (m) == m.cend ());
  CHECK (std::cend (m) == c.cend ());
  CHECK (std::cend (c) == m.cend ());
  CHECK (std::cend (c) == c.cend ());

  CHECK (m.cend () == std::cend (m));
  CHECK (m.cend () == std::cend (c));
  CHECK (c.cend () == std::cend (m));
  CHECK (c.cend () == std::cend (c));
#endif

  CHECK (cend (m) == cend (m));
  CHECK (cend (m) == cend (c));
  CHECK (cend (c) == cend (m));
  CHECK (cend (c) == cend (c));

  CHECK (cend (m) == m.cend ());
  CHECK (cend (m) == c.cend ());
  CHECK (cend (c) == m.cend ());
  CHECK (cend (c) == c.cend ());

  CHECK (m.cend () == cend (m));
  CHECK (m.cend () == cend (c));
  CHECK (c.cend () == cend (m));
  CHECK (c.cend () == cend (c));

  return 0;
}
