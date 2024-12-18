/** incomplete-type.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "gch/small_vector.hpp"

struct incomplete_type;

struct incomplete_holder
{
  gch::small_vector<incomplete_type, 1> x;
};

struct incomplete_type
{
  static
  incomplete_holder
  f (void) noexcept
  {
    return incomplete_holder { };
  }
};
