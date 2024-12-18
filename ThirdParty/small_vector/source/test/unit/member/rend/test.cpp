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

  CHECK (v.rend ().base () == v.begin ());

  CHECK (v.rend () == v.rend ());
  CHECK (v.rend () == v.crend ());
  CHECK (v.rend () == v.rbegin ());
  CHECK (v.rend () == v.crbegin ());

  CHECK (v.rend () == c.rend ());
  CHECK (v.rend () == c.crend ());
  CHECK (v.rend () == c.rbegin ());
  CHECK (v.rend () == c.crbegin ());

  CHECK (c.rend () == v.rend ());
  CHECK (c.rend () == v.crend ());
  CHECK (c.rend () == v.rbegin ());
  CHECK (c.rend () == v.crbegin ());

  CHECK (c.rend () == c.rend ());
  CHECK (c.rend () == c.crend ());
  CHECK (c.rend () == c.rbegin ());
  CHECK (c.rend () == c.crbegin ());

  v.push_back (1);

  CHECK (1 == *std::prev (v.rend ()));
  CHECK (1 == *std::prev (c.rend ()));

  CHECK (v.rend ().base () == v.begin ());

  CHECK (v.rend () == v.rend ());
  CHECK (v.rend () == v.crend ());
  CHECK (! (v.rend ()  ==  v.rbegin ()));
  CHECK (! (v.rend ()  ==  v.crbegin ()));

  CHECK (std::prev (v.rend ()) == v.rbegin ());
  CHECK (std::prev (v.rend ()) == v.crbegin ());

  CHECK (v.rend () == c.rend ());
  CHECK (v.rend () == c.crend ());
  CHECK (! (v.rend ()  ==  c.rbegin ()));
  CHECK (! (v.rend ()  ==  c.crbegin ()));

  CHECK (std::prev (v.rend ()) == c.rbegin ());
  CHECK (std::prev (v.rend ()) == c.crbegin ());

  CHECK (c.rend () == v.rend ());
  CHECK (c.rend () == v.crend ());
  CHECK (! (c.rend ()  ==  v.rbegin ()));
  CHECK (! (c.rend ()  ==  v.crbegin ()));

  CHECK (std::prev (c.rend ()) == v.rbegin ());
  CHECK (std::prev (c.rend ()) == v.crbegin ());

  CHECK (c.rend () == c.rend ());
  CHECK (c.rend () == c.crend ());
  CHECK (! (c.rend ()  ==  c.rbegin ()));
  CHECK (! (c.rend ()  ==  c.crbegin ()));

  CHECK (std::prev (c.rend ()) == c.rbegin ());
  CHECK (std::prev (c.rend ()) == c.crbegin ());

  v.push_back (2);

  CHECK (1 == *std::prev (v.rend ()));
  CHECK (1 == *std::prev (c.rend ()));

  CHECK (v.rend ().base () == v.begin ());

  CHECK (v.rend () == v.rend ());
  CHECK (v.rend () == v.crend ());
  CHECK (! (v.rend ()  ==  v.rbegin ()));
  CHECK (! (v.rend ()  ==  v.crbegin ()));

  CHECK (std::prev (v.rend (), 2) == v.rbegin ());
  CHECK (std::prev (v.rend (), 2) == v.crbegin ());

  CHECK (v.rend () == c.rend ());
  CHECK (v.rend () == c.crend ());
  CHECK (! (v.rend ()  ==  c.rbegin ()));
  CHECK (! (v.rend ()  ==  c.crbegin ()));

  CHECK (std::prev (v.rend (), 2) == c.rbegin ());
  CHECK (std::prev (v.rend (), 2) == c.crbegin ());

  CHECK (c.rend () == v.rend ());
  CHECK (c.rend () == v.crend ());
  CHECK (! (c.rend ()  ==  v.rbegin ()));
  CHECK (! (c.rend ()  ==  v.crbegin ()));

  CHECK (std::prev (c.rend (), 2) == v.rbegin ());
  CHECK (std::prev (c.rend (), 2) == v.crbegin ());

  CHECK (c.rend () == c.rend ());
  CHECK (c.rend () == c.crend ());
  CHECK (! (c.rend ()  ==  c.rbegin ()));
  CHECK (! (c.rend ()  ==  c.crbegin ()));

  CHECK (std::prev (c.rend (), 2) == c.rbegin ());
  CHECK (std::prev (c.rend (), 2) == c.crbegin ());

  CHECK (v.capacity () == v.inline_capacity ());
  auto inlined_rend = c.rend ();

  static_assert (2 <= decltype (v)::inline_capacity_v, "gch::small_vector too small.");
  v.insert (v.rend ().base (), v.inline_capacity () - v.size () + 1, 3);

  CHECK (! v.inlined ());

  CHECK (! (v.rend ()  ==  inlined_rend));

  CHECK (3 == *std::prev (v.rend ()));
  CHECK (3 == *std::prev (c.rend ()));

  CHECK (v.rend ().base () == v.begin ());

  CHECK (v.rend () == v.rend ());
  CHECK (v.rend () == v.crend ());
  CHECK (! (v.rend ()  ==  v.rbegin ()));
  CHECK (! (v.rend ()  ==  v.crbegin ()));

  CHECK (std::prev (v.rend (), v.inline_capacity () + 1) == v.rbegin ());
  CHECK (std::prev (v.rend (), v.inline_capacity () + 1) == v.crbegin ());

  CHECK (v.rend () == c.rend ());
  CHECK (v.rend () == c.crend ());
  CHECK (! (v.rend ()  ==  c.rbegin ()));
  CHECK (! (v.rend ()  ==  c.crbegin ()));

  CHECK (std::prev (v.rend (), v.inline_capacity () + 1) == c.rbegin ());
  CHECK (std::prev (v.rend (), v.inline_capacity () + 1) == c.crbegin ());

  CHECK (c.rend () == v.rend ());
  CHECK (c.rend () == v.crend ());
  CHECK (! (c.rend ()  ==  v.rbegin ()));
  CHECK (! (c.rend ()  ==  v.crbegin ()));

  CHECK (std::prev (c.rend (), v.inline_capacity () + 1) == v.rbegin ());
  CHECK (std::prev (c.rend (), v.inline_capacity () + 1) == v.crbegin ());

  CHECK (c.rend () == c.rend ());
  CHECK (c.rend () == c.crend ());
  CHECK (! (c.rend ()  ==  c.rbegin ()));
  CHECK (! (c.rend ()  ==  c.crbegin ()));

  CHECK (std::prev (c.rend (), v.inline_capacity () + 1) == c.rbegin ());
  CHECK (std::prev (c.rend (), v.inline_capacity () + 1) == c.crbegin ());

  v.clear ();

  CHECK (v.rend () == v.rend ());
  CHECK (v.rend () == v.crend ());
  CHECK (v.rend () == v.rbegin ());
  CHECK (v.rend () == v.crbegin ());

  CHECK (v.rend () == c.rend ());
  CHECK (v.rend () == c.crend ());
  CHECK (v.rend () == c.rbegin ());
  CHECK (v.rend () == c.crbegin ());

  CHECK (c.rend () == v.rend ());
  CHECK (c.rend () == v.crend ());
  CHECK (c.rend () == v.rbegin ());
  CHECK (c.rend () == v.crbegin ());

  CHECK (c.rend () == c.rend ());
  CHECK (c.rend () == c.crend ());
  CHECK (c.rend () == c.rbegin ());
  CHECK (c.rend () == c.crbegin ());

  return 0;
}
