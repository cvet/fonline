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

  CHECK (v.begin () == v.begin ());
  CHECK (v.begin () == v.cbegin ());
  CHECK (v.begin () == v.end ());
  CHECK (v.begin () == v.cend ());

  CHECK (v.begin () == c.begin ());
  CHECK (v.begin () == c.cbegin ());
  CHECK (v.begin () == c.end ());
  CHECK (v.begin () == c.cend ());

  CHECK (c.begin () == v.begin ());
  CHECK (c.begin () == v.cbegin ());
  CHECK (c.begin () == v.end ());
  CHECK (c.begin () == v.cend ());

  CHECK (c.begin () == c.begin ());
  CHECK (c.begin () == c.cbegin ());
  CHECK (c.begin () == c.end ());
  CHECK (c.begin () == c.cend ());

  v.push_back (1);

  CHECK (1 == *v.begin ());
  CHECK (1 == *c.begin ());

  CHECK (v.begin () == v.begin ());
  CHECK (v.begin () == v.cbegin ());
  CHECK (v.begin () != v.end ());
  CHECK (v.begin () != v.cend ());

  CHECK (std::next (v.begin ()) == v.end ());
  CHECK (std::next (v.begin ()) == v.cend ());

  CHECK (v.begin () == c.begin ());
  CHECK (v.begin () == c.cbegin ());
  CHECK (v.begin () != c.end ());
  CHECK (v.begin () != c.cend ());

  CHECK (std::next (v.begin ()) == c.end ());
  CHECK (std::next (v.begin ()) == c.cend ());

  CHECK (c.begin () == v.begin ());
  CHECK (c.begin () == v.cbegin ());
  CHECK (c.begin () != v.end ());
  CHECK (c.begin () != v.cend ());

  CHECK (std::next (c.begin ()) == v.end ());
  CHECK (std::next (c.begin ()) == v.cend ());

  CHECK (c.begin () == c.begin ());
  CHECK (c.begin () == c.cbegin ());
  CHECK (c.begin () != c.end ());
  CHECK (c.begin () != c.cend ());

  CHECK (std::next (c.begin ()) == c.end ());
  CHECK (std::next (c.begin ()) == c.cend ());

  v.push_back (2);

  CHECK (1 == *v.begin ());
  CHECK (1 == *c.begin ());

  CHECK (v.begin () == v.begin ());
  CHECK (v.begin () == v.cbegin ());
  CHECK (v.begin () != v.end ());
  CHECK (v.begin () != v.cend ());

  CHECK (std::next (v.begin (), 2) == v.end ());
  CHECK (std::next (v.begin (), 2) == v.cend ());

  CHECK (v.begin () == c.begin ());
  CHECK (v.begin () == c.cbegin ());
  CHECK (v.begin () != c.end ());
  CHECK (v.begin () != c.cend ());

  CHECK (std::next (v.begin (), 2) == c.end ());
  CHECK (std::next (v.begin (), 2) == c.cend ());

  CHECK (c.begin () == v.begin ());
  CHECK (c.begin () == v.cbegin ());
  CHECK (c.begin () != v.end ());
  CHECK (c.begin () != v.cend ());

  CHECK (std::next (c.begin (), 2) == v.end ());
  CHECK (std::next (c.begin (), 2) == v.cend ());

  CHECK (c.begin () == c.begin ());
  CHECK (c.begin () == c.cbegin ());
  CHECK (c.begin () != c.end ());
  CHECK (c.begin () != c.cend ());

  CHECK (std::next (c.begin (), 2) == c.end ());
  CHECK (std::next (c.begin (), 2) == c.cend ());

  CHECK (v.capacity () == v.inline_capacity ());
  auto inlined_begin = c.begin ();

  static_assert (2 <= decltype (v)::inline_capacity_v, "gch::small_vector too small.");
  v.insert (v.begin (), v.inline_capacity () - v.size () + 1, 3);

  CHECK (! v.inlined ());

  CHECK (v.begin () != inlined_begin);

  CHECK (3 == *v.begin ());
  CHECK (3 == *c.begin ());

  CHECK (v.begin () == v.begin ());
  CHECK (v.begin () == v.cbegin ());
  CHECK (v.begin () != v.end ());
  CHECK (v.begin () != v.cend ());

  CHECK (std::next (v.begin (), v.inline_capacity () + 1) == v.end ());
  CHECK (std::next (v.begin (), v.inline_capacity () + 1) == v.cend ());

  CHECK (v.begin () == c.begin ());
  CHECK (v.begin () == c.cbegin ());
  CHECK (v.begin () != c.end ());
  CHECK (v.begin () != c.cend ());

  CHECK (std::next (v.begin (), v.inline_capacity () + 1) == c.end ());
  CHECK (std::next (v.begin (), v.inline_capacity () + 1) == c.cend ());

  CHECK (c.begin () == v.begin ());
  CHECK (c.begin () == v.cbegin ());
  CHECK (c.begin () != v.end ());
  CHECK (c.begin () != v.cend ());

  CHECK (std::next (c.begin (), v.inline_capacity () + 1) == v.end ());
  CHECK (std::next (c.begin (), v.inline_capacity () + 1) == v.cend ());

  CHECK (c.begin () == c.begin ());
  CHECK (c.begin () == c.cbegin ());
  CHECK (c.begin () != c.end ());
  CHECK (c.begin () != c.cend ());

  CHECK (std::next (c.begin (), v.inline_capacity () + 1) == c.end ());
  CHECK (std::next (c.begin (), v.inline_capacity () + 1) == c.cend ());

  v.clear ();

  CHECK (v.begin () == v.begin ());
  CHECK (v.begin () == v.cbegin ());
  CHECK (v.begin () == v.end ());
  CHECK (v.begin () == v.cend ());

  CHECK (v.begin () == c.begin ());
  CHECK (v.begin () == c.cbegin ());
  CHECK (v.begin () == c.end ());
  CHECK (v.begin () == c.cend ());

  CHECK (c.begin () == v.begin ());
  CHECK (c.begin () == v.cbegin ());
  CHECK (c.begin () == v.end ());
  CHECK (c.begin () == v.cend ());

  CHECK (c.begin () == c.begin ());
  CHECK (c.begin () == c.cbegin ());
  CHECK (c.begin () == c.end ());
  CHECK (c.begin () == c.cend ());

  return 0;
}
