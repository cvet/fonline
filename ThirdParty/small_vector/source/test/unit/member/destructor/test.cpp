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
  // This is kind of a pointless test. It's just an explicit destructor call.
  using namespace gch::test_types;

  int count = 0;

  struct destruction_counter
    : trivially_copyable_data_base
  {
    destruction_counter            (const destruction_counter&)     = default;
    destruction_counter& operator= (const destruction_counter&)     = default;
//  ~destruction_counter           (void)                           = impl;

    using trivially_copyable_data_base::trivially_copyable_data_base;

    GCH_SMALL_VECTOR_TEST_CONSTEXPR
    destruction_counter (int x, int& count)
      : trivially_copyable_data_base (x),
        count_ptr (&count)
    { }

    GCH_SMALL_VECTOR_TEST_CONSTEXPR
    ~destruction_counter (void) noexcept
    {
      ++*count_ptr;
    }

    int *count_ptr;
  };

  gch::small_vector<destruction_counter, 3> v { { 1, count }, { 2, count }, { 3, count } };
  CHECK (3 == count);
  CHECK_IF_NOT_CONSTEXPR (v.inlined ());

  v.~small_vector ();
  CHECK (6 == count);

#ifdef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
  std::construct_at (
    &v,
    std::initializer_list<destruction_counter> {
      { 4, count },
      { 5, count },
      { 6, count },
      { 7, count },
    });
#else
  ::new (&v) decltype (v) ({ { 4, count }, { 5, count }, { 6, count }, { 7, count } });
#endif

  CHECK (decltype (v) { { 4, count }, { 5, count }, { 6, count }, { 7, count } } == v);
  CHECK (! v.inlined ());
  CHECK (18 == count);

  v.~small_vector ();
  CHECK (22 == count);

#ifdef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
  std::construct_at (&v);
#else
  ::new (&v) decltype (v) ();
#endif

  return 0;
}
