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
  gch::small_vector<int> v;
  const auto& c = v;

  CHECK (v.end () == v.end ());
  CHECK (v.end () == v.cend ());
  CHECK (v.end () == v.begin ());
  CHECK (v.end () == v.cbegin ());

  CHECK (v.end () == c.end ());
  CHECK (v.end () == c.cend ());
  CHECK (v.end () == c.begin ());
  CHECK (v.end () == c.cbegin ());

  CHECK (c.end () == v.end ());
  CHECK (c.end () == v.cend ());
  CHECK (c.end () == v.begin ());
  CHECK (c.end () == v.cbegin ());

  CHECK (c.end () == c.end ());
  CHECK (c.end () == c.cend ());
  CHECK (c.end () == c.begin ());
  CHECK (c.end () == c.cbegin ());

  v.push_back (1);

  CHECK (1 == *std::prev (v.end ()));
  CHECK (1 == *std::prev (c.end ()));

  CHECK (v.end () == v.end ());
  CHECK (v.end () == v.cend ());
  CHECK (v.end () != v.begin ());
  CHECK (v.end () != v.cbegin ());

  CHECK (std::prev (v.end ()) == v.begin ());
  CHECK (std::prev (v.end ()) == v.cbegin ());

  CHECK (v.end () == c.end ());
  CHECK (v.end () == c.cend ());
  CHECK (v.end () != c.begin ());
  CHECK (v.end () != c.cbegin ());

  CHECK (std::prev (v.end ()) == c.begin ());
  CHECK (std::prev (v.end ()) == c.cbegin ());

  CHECK (c.end () == v.end ());
  CHECK (c.end () == v.cend ());
  CHECK (c.end () != v.begin ());
  CHECK (c.end () != v.cbegin ());

  CHECK (std::prev (c.end ()) == v.begin ());
  CHECK (std::prev (c.end ()) == v.cbegin ());

  CHECK (c.end () == c.end ());
  CHECK (c.end () == c.cend ());
  CHECK (c.end () != c.begin ());
  CHECK (c.end () != c.cbegin ());

  CHECK (std::prev (c.end ()) == c.begin ());
  CHECK (std::prev (c.end ()) == c.cbegin ());

  v.push_back (2);

  CHECK (2 == *std::prev (v.end ()));
  CHECK (2 == *std::prev (c.end ()));

  CHECK (v.end () == v.end ());
  CHECK (v.end () == v.cend ());
  CHECK (v.end () != v.begin ());
  CHECK (v.end () != v.cbegin ());

  CHECK (std::prev (v.end (), 2) == v.begin ());
  CHECK (std::prev (v.end (), 2) == v.cbegin ());

  CHECK (v.end () == c.end ());
  CHECK (v.end () == c.cend ());
  CHECK (v.end () != c.begin ());
  CHECK (v.end () != c.cbegin ());

  CHECK (std::prev (v.end (), 2) == c.begin ());
  CHECK (std::prev (v.end (), 2) == c.cbegin ());

  CHECK (c.end () == v.end ());
  CHECK (c.end () == v.cend ());
  CHECK (c.end () != v.begin ());
  CHECK (c.end () != v.cbegin ());

  CHECK (std::prev (c.end (), 2) == v.begin ());
  CHECK (std::prev (c.end (), 2) == v.cbegin ());

  CHECK (c.end () == c.end ());
  CHECK (c.end () == c.cend ());
  CHECK (c.end () != c.begin ());
  CHECK (c.end () != c.cbegin ());

  CHECK (std::prev (c.end (), 2) == c.begin ());
  CHECK (std::prev (c.end (), 2) == c.cbegin ());

  CHECK (v.capacity () == v.inline_capacity ());
  auto inlined_begin = c.begin ();

  static_assert (2 <= decltype (v)::inline_capacity_v, "gch::small_vector too small.");
  v.insert (v.end (), v.inline_capacity () - v.size () + 1, 3);

  CHECK (! v.inlined ());

  CHECK (v.begin () != inlined_begin);

  CHECK (3 == *std::prev (v.end ()));
  CHECK (3 == *std::prev (c.end ()));

  CHECK (v.end () == v.end ());
  CHECK (v.end () == v.cend ());
  CHECK (v.end () != v.begin ());
  CHECK (v.end () != v.cbegin ());

  CHECK (std::prev (v.end (), v.inline_capacity () + 1) == v.begin ());
  CHECK (std::prev (v.end (), v.inline_capacity () + 1) == v.cbegin ());

  CHECK (v.end () == c.end ());
  CHECK (v.end () == c.cend ());
  CHECK (v.end () != c.begin ());
  CHECK (v.end () != c.cbegin ());

  CHECK (std::prev (v.end (), v.inline_capacity () + 1) == c.begin ());
  CHECK (std::prev (v.end (), v.inline_capacity () + 1) == c.cbegin ());

  CHECK (c.end () == v.end ());
  CHECK (c.end () == v.cend ());
  CHECK (c.end () != v.begin ());
  CHECK (c.end () != v.cbegin ());

  CHECK (std::prev (c.end (), v.inline_capacity () + 1) == v.begin ());
  CHECK (std::prev (c.end (), v.inline_capacity () + 1) == v.cbegin ());

  CHECK (c.end () == c.end ());
  CHECK (c.end () == c.cend ());
  CHECK (c.end () != c.begin ());
  CHECK (c.end () != c.cbegin ());

  CHECK (std::prev (c.end (), v.inline_capacity () + 1) == c.begin ());
  CHECK (std::prev (c.end (), v.inline_capacity () + 1) == c.cbegin ());

  v.clear ();

  CHECK (v.end () == v.end ());
  CHECK (v.end () == v.cend ());
  CHECK (v.end () == v.begin ());
  CHECK (v.end () == v.cbegin ());

  CHECK (v.end () == c.end ());
  CHECK (v.end () == c.cend ());
  CHECK (v.end () == c.begin ());
  CHECK (v.end () == c.cbegin ());

  CHECK (c.end () == v.end ());
  CHECK (c.end () == v.cend ());
  CHECK (c.end () == v.begin ());
  CHECK (c.end () == v.cbegin ());

  CHECK (c.end () == c.end ());
  CHECK (c.end () == c.cend ());
  CHECK (c.end () == c.begin ());
  CHECK (c.end () == c.cbegin ());

  return 0;
}
