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

  CHECK (v.rbegin ().base () == v.end ());

  CHECK (v.rbegin () == v.rbegin ());
  CHECK (v.rbegin () == v.crbegin ());
  CHECK (v.rbegin () == v.rend ());
  CHECK (v.rbegin () == v.crend ());

  CHECK (v.rbegin () == c.rbegin ());
  CHECK (v.rbegin () == c.crbegin ());
  CHECK (v.rbegin () == c.rend ());
  CHECK (v.rbegin () == c.crend ());

  CHECK (c.rbegin () == v.rbegin ());
  CHECK (c.rbegin () == v.crbegin ());
  CHECK (c.rbegin () == v.rend ());
  CHECK (c.rbegin () == v.crend ());

  CHECK (c.rbegin () == c.rbegin ());
  CHECK (c.rbegin () == c.crbegin ());
  CHECK (c.rbegin () == c.rend ());
  CHECK (c.rbegin () == c.crend ());

  v.push_back (1);

  CHECK (1 == *v.rbegin ());
  CHECK (1 == *c.rbegin ());

  CHECK (v.rbegin ().base () == v.end ());

  CHECK (v.rbegin () == v.rbegin ());
  CHECK (v.rbegin () == v.crbegin ());
  CHECK (! (v.rbegin ()  ==  v.rend ()));
  CHECK (! (v.rbegin ()  ==  v.crend ()));

  CHECK (std::next (v.rbegin ()) == v.rend ());
  CHECK (std::next (v.rbegin ()) == v.crend ());

  CHECK (v.rbegin () == c.rbegin ());
  CHECK (v.rbegin () == c.crbegin ());
  CHECK (! (v.rbegin ()  ==  c.rend ()));
  CHECK (! (v.rbegin ()  ==  c.crend ()));

  CHECK (std::next (v.rbegin ()) == c.rend ());
  CHECK (std::next (v.rbegin ()) == c.crend ());

  CHECK (c.rbegin () == v.rbegin ());
  CHECK (c.rbegin () == v.crbegin ());
  CHECK (! (c.rbegin ()  ==  v.rend ()));
  CHECK (! (c.rbegin ()  ==  v.crend ()));

  CHECK (std::next (c.rbegin ()) == v.rend ());
  CHECK (std::next (c.rbegin ()) == v.crend ());

  CHECK (c.rbegin () == c.rbegin ());
  CHECK (c.rbegin () == c.crbegin ());
  CHECK (! (c.rbegin ()  ==  c.rend ()));
  CHECK (! (c.rbegin ()  ==  c.crend ()));

  CHECK (std::next (c.rbegin ()) == c.rend ());
  CHECK (std::next (c.rbegin ()) == c.crend ());

  v.push_back (2);

  CHECK (2 == *v.rbegin ());
  CHECK (2 == *c.rbegin ());

  CHECK (v.rbegin ().base () == v.end ());

  CHECK (v.rbegin () == v.rbegin ());
  CHECK (v.rbegin () == v.crbegin ());
  CHECK (! (v.rbegin ()  ==  v.rend ()));
  CHECK (! (v.rbegin ()  ==  v.crend ()));

  CHECK (std::next (v.rbegin (), 2) == v.rend ());
  CHECK (std::next (v.rbegin (), 2) == v.crend ());

  CHECK (v.rbegin () == c.rbegin ());
  CHECK (v.rbegin () == c.crbegin ());
  CHECK (! (v.rbegin ()  ==  c.rend ()));
  CHECK (! (v.rbegin ()  ==  c.crend ()));

  CHECK (std::next (v.rbegin (), 2) == c.rend ());
  CHECK (std::next (v.rbegin (), 2) == c.crend ());

  CHECK (c.rbegin () == v.rbegin ());
  CHECK (c.rbegin () == v.crbegin ());
  CHECK (! (c.rbegin ()  ==  v.rend ()));
  CHECK (! (c.rbegin ()  ==  v.crend ()));

  CHECK (std::next (c.rbegin (), 2) == v.rend ());
  CHECK (std::next (c.rbegin (), 2) == v.crend ());

  CHECK (c.rbegin () == c.rbegin ());
  CHECK (c.rbegin () == c.crbegin ());
  CHECK (! (c.rbegin ()  ==  c.rend ()));
  CHECK (! (c.rbegin ()  ==  c.crend ()));

  CHECK (std::next (c.rbegin (), 2) == c.rend ());
  CHECK (std::next (c.rbegin (), 2) == c.crend ());

  CHECK (v.capacity () == v.inline_capacity ());
  auto inlined_rend = c.rend ();

  static_assert (2 <= decltype (v)::inline_capacity_v, "gch::small_vector too small.");
  v.insert (v.rbegin ().base (), v.inline_capacity () - v.size () + 1, 3);

  CHECK (! v.inlined ());

  CHECK (! (v.rend ()  ==  inlined_rend));

  CHECK (3 == *v.rbegin ());
  CHECK (3 == *c.rbegin ());

  CHECK (v.rbegin ().base () == v.end ());

  CHECK (v.rbegin () == v.rbegin ());
  CHECK (v.rbegin () == v.crbegin ());
  CHECK (! (v.rbegin ()  ==  v.rend ()));
  CHECK (! (v.rbegin ()  ==  v.crend ()));

  CHECK (std::next (v.rbegin (), v.inline_capacity () + 1) == v.rend ());
  CHECK (std::next (v.rbegin (), v.inline_capacity () + 1) == v.crend ());

  CHECK (v.rbegin () == c.rbegin ());
  CHECK (v.rbegin () == c.crbegin ());
  CHECK (! (v.rbegin ()  ==  c.rend ()));
  CHECK (! (v.rbegin ()  ==  c.crend ()));

  CHECK (std::next (v.rbegin (), v.inline_capacity () + 1) == c.rend ());
  CHECK (std::next (v.rbegin (), v.inline_capacity () + 1) == c.crend ());

  CHECK (c.rbegin () == v.rbegin ());
  CHECK (c.rbegin () == v.crbegin ());
  CHECK (! (c.rbegin ()  ==  v.rend ()));
  CHECK (! (c.rbegin ()  ==  v.crend ()));

  CHECK (std::next (c.rbegin (), v.inline_capacity () + 1) == v.rend ());
  CHECK (std::next (c.rbegin (), v.inline_capacity () + 1) == v.crend ());

  CHECK (c.rbegin () == c.rbegin ());
  CHECK (c.rbegin () == c.crbegin ());
  CHECK (! (c.rbegin ()  ==  c.rend ()));
  CHECK (! (c.rbegin ()  ==  c.crend ()));

  CHECK (std::next (c.rbegin (), v.inline_capacity () + 1) == c.rend ());
  CHECK (std::next (c.rbegin (), v.inline_capacity () + 1) == c.crend ());

  v.clear ();

  CHECK (v.rbegin ().base () == v.end ());

  CHECK (v.rbegin () == v.rbegin ());
  CHECK (v.rbegin () == v.crbegin ());
  CHECK (v.rbegin () == v.rend ());
  CHECK (v.rbegin () == v.crend ());

  CHECK (v.rbegin () == c.rbegin ());
  CHECK (v.rbegin () == c.crbegin ());
  CHECK (v.rbegin () == c.rend ());
  CHECK (v.rbegin () == c.crend ());

  CHECK (c.rbegin () == v.rbegin ());
  CHECK (c.rbegin () == v.crbegin ());
  CHECK (c.rbegin () == v.rend ());
  CHECK (c.rbegin () == v.crend ());

  CHECK (c.rbegin () == c.rbegin ());
  CHECK (c.rbegin () == c.crbegin ());
  CHECK (c.rbegin () == c.rend ());
  CHECK (c.rbegin () == c.crend ());

  return 0;
}
