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

  using diff_type = decltype (m)::difference_type;

  CHECK (gch::ssize (m) == gch::ssize (m));
  CHECK (gch::ssize (m) == gch::ssize (c));
  CHECK (gch::ssize (c) == gch::ssize (m));
  CHECK (gch::ssize (c) == gch::ssize (c));

  CHECK (gch::ssize (m) == static_cast<diff_type> (m.size ()));
  CHECK (gch::ssize (m) == static_cast<diff_type> (c.size ()));
  CHECK (gch::ssize (c) == static_cast<diff_type> (m.size ()));
  CHECK (gch::ssize (c) == static_cast<diff_type> (c.size ()));

  CHECK (static_cast<diff_type> (m.size ()) == gch::ssize (m));
  CHECK (static_cast<diff_type> (m.size ()) == gch::ssize (c));
  CHECK (static_cast<diff_type> (c.size ()) == gch::ssize (m));
  CHECK (static_cast<diff_type> (c.size ()) == gch::ssize (c));

#if defined (__cpp_lib_ssize) && __cpp_lib_ssize >= 201902L
  CHECK (std::ssize (m) == std::ssize (m));
  CHECK (std::ssize (m) == std::ssize (c));
  CHECK (std::ssize (c) == std::ssize (m));
  CHECK (std::ssize (c) == std::ssize (c));

  CHECK (std::ssize (m) == static_cast<diff_type> (m.size ()));
  CHECK (std::ssize (m) == static_cast<diff_type> (c.size ()));
  CHECK (std::ssize (c) == static_cast<diff_type> (m.size ()));
  CHECK (std::ssize (c) == static_cast<diff_type> (c.size ()));

  CHECK (static_cast<diff_type> (m.size ()) == std::ssize (m));
  CHECK (static_cast<diff_type> (m.size ()) == std::ssize (c));
  CHECK (static_cast<diff_type> (c.size ()) == std::ssize (m));
  CHECK (static_cast<diff_type> (c.size ()) == std::ssize (c));
#endif

  CHECK (ssize (m) == ssize (m));
  CHECK (ssize (m) == ssize (c));
  CHECK (ssize (c) == ssize (m));
  CHECK (ssize (c) == ssize (c));

  CHECK (ssize (m) == static_cast<diff_type> (m.size ()));
  CHECK (ssize (m) == static_cast<diff_type> (c.size ()));
  CHECK (ssize (c) == static_cast<diff_type> (m.size ()));
  CHECK (ssize (c) == static_cast<diff_type> (c.size ()));

  CHECK (static_cast<diff_type> (m.size ()) == ssize (m));
  CHECK (static_cast<diff_type> (m.size ()) == ssize (c));
  CHECK (static_cast<diff_type> (c.size ()) == ssize (m));
  CHECK (static_cast<diff_type> (c.size ()) == ssize (c));

  return 0;
}
