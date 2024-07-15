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

  CHECK (gch::begin (m) == gch::begin (m));
  CHECK (gch::begin (m) == gch::begin (c));
  CHECK (gch::begin (c) == gch::begin (m));
  CHECK (gch::begin (c) == gch::begin (c));

  CHECK (gch::begin (m) == m.begin ());
  CHECK (gch::begin (m) == c.begin ());
  CHECK (gch::begin (c) == m.begin ());
  CHECK (gch::begin (c) == c.begin ());

  CHECK (m.begin () == gch::begin (m));
  CHECK (m.begin () == gch::begin (c));
  CHECK (c.begin () == gch::begin (m));
  CHECK (c.begin () == gch::begin (c));

  CHECK (std::begin (m) == std::begin (m));
  CHECK (std::begin (m) == std::begin (c));
  CHECK (std::begin (c) == std::begin (m));
  CHECK (std::begin (c) == std::begin (c));
  
  CHECK (std::begin (m) == gch::begin (m));
  CHECK (std::begin (m) == gch::begin (c));
  CHECK (std::begin (c) == gch::begin (m));
  CHECK (std::begin (c) == gch::begin (c));
  
  CHECK (gch::begin (m) == std::begin (m));
  CHECK (gch::begin (m) == std::begin (c));
  CHECK (gch::begin (c) == std::begin (m));
  CHECK (gch::begin (c) == std::begin (c));

  CHECK (std::begin (m) == m.begin ());
  CHECK (std::begin (m) == c.begin ());
  CHECK (std::begin (c) == m.begin ());
  CHECK (std::begin (c) == c.begin ());

  CHECK (m.begin () == std::begin (m));
  CHECK (m.begin () == std::begin (c));
  CHECK (c.begin () == std::begin (m));
  CHECK (c.begin () == std::begin (c));

  CHECK (begin (m) == begin (m));
  CHECK (begin (m) == begin (c));
  CHECK (begin (c) == begin (m));
  CHECK (begin (c) == begin (c));

  CHECK (begin (m) == m.begin ());
  CHECK (begin (m) == c.begin ());
  CHECK (begin (c) == m.begin ());
  CHECK (begin (c) == c.begin ());

  CHECK (m.begin () == begin (m));
  CHECK (m.begin () == begin (c));
  CHECK (c.begin () == begin (m));
  CHECK (c.begin () == begin (c));

  return 0;
}
