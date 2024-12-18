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

  CHECK (gch::size (m) == gch::size (m));
  CHECK (gch::size (m) == gch::size (c));
  CHECK (gch::size (c) == gch::size (m));
  CHECK (gch::size (c) == gch::size (c));

  CHECK (gch::size (m) == m.size ());
  CHECK (gch::size (m) == c.size ());
  CHECK (gch::size (c) == m.size ());
  CHECK (gch::size (c) == c.size ());

  CHECK (m.size () == gch::size (m));
  CHECK (m.size () == gch::size (c));
  CHECK (c.size () == gch::size (m));
  CHECK (c.size () == gch::size (c));

#if defined (__cpp_lib_nonmember_container_access) && __cpp_lib_nonmember_container_access >= 201411L
  CHECK (std::size (m) == std::size (m));
  CHECK (std::size (m) == std::size (c));
  CHECK (std::size (c) == std::size (m));
  CHECK (std::size (c) == std::size (c));

  CHECK (std::size (m) == m.size ());
  CHECK (std::size (m) == c.size ());
  CHECK (std::size (c) == m.size ());
  CHECK (std::size (c) == c.size ());

  CHECK (m.size () == std::size (m));
  CHECK (m.size () == std::size (c));
  CHECK (c.size () == std::size (m));
  CHECK (c.size () == std::size (c));
#endif

  CHECK (size (m) == size (m));
  CHECK (size (m) == size (c));
  CHECK (size (c) == size (m));
  CHECK (size (c) == size (c));

  CHECK (size (m) == m.size ());
  CHECK (size (m) == c.size ());
  CHECK (size (c) == m.size ());
  CHECK (size (c) == c.size ());

  CHECK (m.size () == size (m));
  CHECK (m.size () == size (c));
  CHECK (c.size () == size (m));
  CHECK (c.size () == size (c));

  return 0;
}
