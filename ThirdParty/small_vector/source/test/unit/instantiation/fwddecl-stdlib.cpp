/** fwddecl-stdlib.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct allocator;
struct bidirectional_iterator_tag;
struct forward_iterator_tag;
struct input_iterator_tag;
struct iterator;
struct iterator_traits;
struct less;
struct pair;
struct random_access_iterator_tag;

#include <utility>

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wheader-hygiene"
#endif

using namespace std::rel_ops;

#ifdef __clang__
#  pragma clang diagnostic pop

#  include <memory>

template <typename T>
constexpr
bool
operator!= (const std::allocator<T>&, const std::allocator<T>&) noexcept
{
  return false;
}
#endif

#include "gch/small_vector.hpp"

template class gch::small_vector<int>;
