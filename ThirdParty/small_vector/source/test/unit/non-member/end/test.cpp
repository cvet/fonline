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

  CHECK (gch::end (m) == gch::end (m));
  CHECK (gch::end (m) == gch::end (c));
  CHECK (gch::end (c) == gch::end (m));
  CHECK (gch::end (c) == gch::end (c));

  CHECK (gch::end (m) == m.end ());
  CHECK (gch::end (m) == c.end ());
  CHECK (gch::end (c) == m.end ());
  CHECK (gch::end (c) == c.end ());

  CHECK (m.end () == gch::end (m));
  CHECK (m.end () == gch::end (c));
  CHECK (c.end () == gch::end (m));
  CHECK (c.end () == gch::end (c));

  CHECK (std::end (m) == std::end (m));
  CHECK (std::end (m) == std::end (c));
  CHECK (std::end (c) == std::end (m));
  CHECK (std::end (c) == std::end (c));
  
  CHECK (std::end (m) == gch::end (m));
  CHECK (std::end (m) == gch::end (c));
  CHECK (std::end (c) == gch::end (m));
  CHECK (std::end (c) == gch::end (c));
  
  CHECK (gch::end (m) == std::end (m));
  CHECK (gch::end (m) == std::end (c));
  CHECK (gch::end (c) == std::end (m));
  CHECK (gch::end (c) == std::end (c));

  CHECK (std::end (m) == m.end ());
  CHECK (std::end (m) == c.end ());
  CHECK (std::end (c) == m.end ());
  CHECK (std::end (c) == c.end ());

  CHECK (m.end () == std::end (m));
  CHECK (m.end () == std::end (c));
  CHECK (c.end () == std::end (m));
  CHECK (c.end () == std::end (c));

  CHECK (end (m) == end (m));
  CHECK (end (m) == end (c));
  CHECK (end (c) == end (m));
  CHECK (end (c) == end (c));

  CHECK (end (m) == m.end ());
  CHECK (end (m) == c.end ());
  CHECK (end (c) == m.end ());
  CHECK (end (c) == c.end ());

  CHECK (m.end () == end (m));
  CHECK (m.end () == end (c));
  CHECK (c.end () == end (m));
  CHECK (c.end () == end (c));

  return 0;
}
