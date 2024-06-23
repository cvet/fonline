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

  CHECK (gch::cbegin (m) == gch::cbegin (m));
  CHECK (gch::cbegin (m) == gch::cbegin (c));
  CHECK (gch::cbegin (c) == gch::cbegin (m));
  CHECK (gch::cbegin (c) == gch::cbegin (c));

  CHECK (gch::cbegin (m) == m.cbegin ());
  CHECK (gch::cbegin (m) == c.cbegin ());
  CHECK (gch::cbegin (c) == m.cbegin ());
  CHECK (gch::cbegin (c) == c.cbegin ());

  CHECK (m.cbegin () == gch::cbegin (m));
  CHECK (m.cbegin () == gch::cbegin (c));
  CHECK (c.cbegin () == gch::cbegin (m));
  CHECK (c.cbegin () == gch::cbegin (c));

#if defined (__cplusplus) && __cplusplus >= 201402L
  CHECK (std::cbegin (m) == std::cbegin (m));
  CHECK (std::cbegin (m) == std::cbegin (c));
  CHECK (std::cbegin (c) == std::cbegin (m));
  CHECK (std::cbegin (c) == std::cbegin (c));
  
  CHECK (std::cbegin (m) == gch::cbegin (m));
  CHECK (std::cbegin (m) == gch::cbegin (c));
  CHECK (std::cbegin (c) == gch::cbegin (m));
  CHECK (std::cbegin (c) == gch::cbegin (c));
  
  CHECK (gch::cbegin (m) == std::cbegin (m));
  CHECK (gch::cbegin (m) == std::cbegin (c));
  CHECK (gch::cbegin (c) == std::cbegin (m));
  CHECK (gch::cbegin (c) == std::cbegin (c));

  CHECK (std::cbegin (m) == m.cbegin ());
  CHECK (std::cbegin (m) == c.cbegin ());
  CHECK (std::cbegin (c) == m.cbegin ());
  CHECK (std::cbegin (c) == c.cbegin ());

  CHECK (m.cbegin () == std::cbegin (m));
  CHECK (m.cbegin () == std::cbegin (c));
  CHECK (c.cbegin () == std::cbegin (m));
  CHECK (c.cbegin () == std::cbegin (c));
#endif

  CHECK (cbegin (m) == cbegin (m));
  CHECK (cbegin (m) == cbegin (c));
  CHECK (cbegin (c) == cbegin (m));
  CHECK (cbegin (c) == cbegin (c));

  CHECK (cbegin (m) == m.cbegin ());
  CHECK (cbegin (m) == c.cbegin ());
  CHECK (cbegin (c) == m.cbegin ());
  CHECK (cbegin (c) == c.cbegin ());

  CHECK (m.cbegin () == cbegin (m));
  CHECK (m.cbegin () == cbegin (c));
  CHECK (c.cbegin () == cbegin (m));
  CHECK (c.cbegin () == cbegin (c));

  return 0;
}
