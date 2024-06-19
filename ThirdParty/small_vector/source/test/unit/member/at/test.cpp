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
  gch::small_vector<int> v { 1, 2, 3 };

  CHECK (v.at (0) == v[0]);
  CHECK (v.at (0) == v.front ());

  CHECK (v.at (1) == v[1]);

  CHECK (v.at (v.size () - 1) == v[v.size () - 1]);
  CHECK (v.at (v.size () - 1) == v.back ());

  GCH_TRY
  {
    EXPECT_THROW (v.at (v.size ()));
  }
  GCH_CATCH (const std::out_of_range&)
  { }

  GCH_TRY
  {
    EXPECT_THROW (v.at (v.size () + 1));
  }
  GCH_CATCH (const std::out_of_range&)
  { }

  GCH_TRY
  {
    EXPECT_THROW (v.at (static_cast<decltype (v)::size_type> (-1)));
  }
  GCH_CATCH (const std::out_of_range&)
  { }

  return 0;
}
