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

  CHECK (gch::data (m) == gch::data (m));
  CHECK (gch::data (m) == gch::data (c));
  CHECK (gch::data (c) == gch::data (m));
  CHECK (gch::data (c) == gch::data (c));

  CHECK (gch::data (m) == m.data ());
  CHECK (gch::data (m) == c.data ());
  CHECK (gch::data (c) == m.data ());
  CHECK (gch::data (c) == c.data ());

  CHECK (m.data () == gch::data (m));
  CHECK (m.data () == gch::data (c));
  CHECK (c.data () == gch::data (m));
  CHECK (c.data () == gch::data (c));

#if defined (__cpp_lib_nonmember_container_access) && __cpp_lib_nonmember_container_access >= 201411L
  CHECK (std::data (m) == std::data (m));
  CHECK (std::data (m) == std::data (c));
  CHECK (std::data (c) == std::data (m));
  CHECK (std::data (c) == std::data (c));

  CHECK (std::data (m) == m.data ());
  CHECK (std::data (m) == c.data ());
  CHECK (std::data (c) == m.data ());
  CHECK (std::data (c) == c.data ());

  CHECK (m.data () == std::data (m));
  CHECK (m.data () == std::data (c));
  CHECK (c.data () == std::data (m));
  CHECK (c.data () == std::data (c));
#endif

  CHECK (data (m) == data (m));
  CHECK (data (m) == data (c));
  CHECK (data (c) == data (m));
  CHECK (data (c) == data (c));

  CHECK (data (m) == m.data ());
  CHECK (data (m) == c.data ());
  CHECK (data (c) == m.data ());
  CHECK (data (c) == c.data ());

  CHECK (m.data () == data (m));
  CHECK (m.data () == data (c));
  CHECK (c.data () == data (m));
  CHECK (c.data () == data (c));

  return 0;
}
