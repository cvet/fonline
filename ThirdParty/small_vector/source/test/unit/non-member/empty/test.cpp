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
  gch::small_vector<int> m;
  const auto& c = m;

  CHECK (gch::empty (m) == gch::empty (m));
  CHECK (gch::empty (m) == gch::empty (c));
  CHECK (gch::empty (c) == gch::empty (m));
  CHECK (gch::empty (c) == gch::empty (c));

  CHECK (gch::empty (m) == m.empty ());
  CHECK (gch::empty (m) == c.empty ());
  CHECK (gch::empty (c) == m.empty ());
  CHECK (gch::empty (c) == c.empty ());

  CHECK (m.empty () == gch::empty (m));
  CHECK (m.empty () == gch::empty (c));
  CHECK (c.empty () == gch::empty (m));
  CHECK (c.empty () == gch::empty (c));

#if defined (__cpp_lib_nonmember_container_access) && __cpp_lib_nonmember_container_access >= 201411L
  CHECK (std::empty (m) == std::empty (m));
  CHECK (std::empty (m) == std::empty (c));
  CHECK (std::empty (c) == std::empty (m));
  CHECK (std::empty (c) == std::empty (c));

  CHECK (std::empty (m) == m.empty ());
  CHECK (std::empty (m) == c.empty ());
  CHECK (std::empty (c) == m.empty ());
  CHECK (std::empty (c) == c.empty ());

  CHECK (m.empty () == std::empty (m));
  CHECK (m.empty () == std::empty (c));
  CHECK (c.empty () == std::empty (m));
  CHECK (c.empty () == std::empty (c));
#endif

  CHECK (empty (m) == empty (m));
  CHECK (empty (m) == empty (c));
  CHECK (empty (c) == empty (m));
  CHECK (empty (c) == empty (c));

  CHECK (empty (m) == m.empty ());
  CHECK (empty (m) == c.empty ());
  CHECK (empty (c) == m.empty ());
  CHECK (empty (c) == c.empty ());

  CHECK (m.empty () == empty (m));
  CHECK (m.empty () == empty (c));
  CHECK (c.empty () == empty (m));
  CHECK (c.empty () == empty (c));

  m.append ({ 1, 2, 3 });

  CHECK (gch::empty (m) == gch::empty (m));
  CHECK (gch::empty (m) == gch::empty (c));
  CHECK (gch::empty (c) == gch::empty (m));
  CHECK (gch::empty (c) == gch::empty (c));

  CHECK (gch::empty (m) == m.empty ());
  CHECK (gch::empty (m) == c.empty ());
  CHECK (gch::empty (c) == m.empty ());
  CHECK (gch::empty (c) == c.empty ());

  CHECK (m.empty () == gch::empty (m));
  CHECK (m.empty () == gch::empty (c));
  CHECK (c.empty () == gch::empty (m));
  CHECK (c.empty () == gch::empty (c));

#if defined (__cpp_lib_nonmember_container_access) && __cpp_lib_nonmember_container_access >= 201411L
  CHECK (std::empty (m) == std::empty (m));
  CHECK (std::empty (m) == std::empty (c));
  CHECK (std::empty (c) == std::empty (m));
  CHECK (std::empty (c) == std::empty (c));

  CHECK (std::empty (m) == m.empty ());
  CHECK (std::empty (m) == c.empty ());
  CHECK (std::empty (c) == m.empty ());
  CHECK (std::empty (c) == c.empty ());

  CHECK (m.empty () == std::empty (m));
  CHECK (m.empty () == std::empty (c));
  CHECK (c.empty () == std::empty (m));
  CHECK (c.empty () == std::empty (c));
#endif

  CHECK (empty (m) == empty (m));
  CHECK (empty (m) == empty (c));
  CHECK (empty (c) == empty (m));
  CHECK (empty (c) == empty (c));

  CHECK (empty (m) == m.empty ());
  CHECK (empty (m) == c.empty ());
  CHECK (empty (c) == m.empty ());
  CHECK (empty (c) == c.empty ());

  CHECK (m.empty () == empty (m));
  CHECK (m.empty () == empty (c));
  CHECK (c.empty () == empty (m));
  CHECK (c.empty () == empty (c));

  return 0;
}
