/** test-range.cpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "unit_test_common.hpp"
#include "test_allocators.hpp"

#include <array>

template <typename T, typename Allocator>
struct tester
{
  template <unsigned K>
  using vector_init_type = vector_initializer<T, K, Allocator>;

  template <unsigned K>
  using vector_type = gch::small_vector<T, K, Allocator>;

  tester (void) = default;

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  tester (const Allocator& alloc)
    : m_alloc (alloc)
  { }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  int
  operator() (void)
  {
    check_no_inline ();
    check_equal_inline ();

    return 0;
  }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  int
  check_no_inline (void)
  {
    // Check vectors with no inline elements.
    check<0> ({ },      { });
    check<0> ({ 1 },    { });
    check<0> ({ 1, 2 }, { });
    check<0> ({ },      { 11 });
    check<0> ({ 1 },    { 11 });
    check<0> ({ 1, 2 }, { 11 });
    check<0> ({ },      { 11, 22 });
    check<0> ({ 1 },    { 11, 22 });
    check<0> ({ 1, 2 }, { 11, 22 });
    return 0;
  }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  int
  check_equal_inline (void)
  {
    // Check vectors with the same number of inline elements.
    // Let N = 2, and let both vectors have N inline elements.
    // States to check:
    //   Combinations of (with repeats):
    //     Inlined:
    //       0 == K elements    (1)
    //       0 < K < N elements (2)
    //       N == K elements    (3)
    //     Allocated:
    //       0 == K elements    (4)
    //       0 < K < N elements (5)
    //       N == K elements    (6)
    //       N < K elements     (7)

    auto reserver = [](vector_type<2>& v) {
      v.reserve (3);
    };

    std::array<vector_init_type<2>, 8> ns {
      vector_init_type<2> { },
      { 1 },
      { 1, 2 },
      { { },      reserver },
      { { 1 },    reserver },
      { { 1, 2 }, reserver },
      { { 1, 2, 3 }, },
      { { 1, 2, 3, 4 }, },
    };

    for (std::size_t i = 0; i < ns.size (); ++i)
    {
      check (ns[i], { });
      check (ns[i], { 1 });
      check (ns[i], { 1, 2 });
      check (ns[i], { 1, 2, 3 });
    }

    return 0;
  }

private:

  template <unsigned N, typename U = T,
            typename std::enable_if<std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (vector_init_type<N> vi, std::initializer_list<T> wi)
  {
    using namespace gch::test_types;

    verify_basic_exception_safety (
      [&] (vector_type<N>& v) { v.assign (wi.begin (), wi.end ()); },
      vi,
      m_alloc);

    verify_basic_exception_safety (
      [&] (vector_type<N>& v)
      {
        v.assign (make_input_it (wi.begin ()), make_input_it (wi.end ()));
      },
      vi,
      m_alloc);

    verify_basic_exception_safety (
      [&] (vector_type<N>& v) { v.assign (make_fwd_it (wi.begin ()), make_fwd_it (wi.end ())); },
      vi,
      m_alloc);

    verify_basic_exception_safety (
      [&] (vector_type<N>& v)
      {
        v.assign (make_reverse_it (wi.end ()),
                  make_reverse_it (wi.begin ()));
      },
      vi,
      m_alloc);

    verify_basic_exception_safety (
      [&] (vector_type<N>& v)
      {
        v.assign (make_input_it (make_reverse_it (wi.end ())),
                  make_input_it (make_reverse_it (wi.begin ())));
      },
      vi,
      m_alloc);

    verify_basic_exception_safety (
      [&] (vector_type<N>& v)
      {
        v.assign (make_fwd_it (make_reverse_it (wi.end ())),
                  make_fwd_it (make_reverse_it (wi.begin ())));
      },
      vi,
      m_alloc);

    {
      vector_type<N> w (wi.begin (), wi.end ());
      verify_basic_exception_safety (
        [=] (vector_type<N>& v)
        {
          v.assign (std::make_move_iterator (w.begin ()),
                    std::make_move_iterator (w.end ()));
        },
        vi,
        m_alloc);
    }
    {
      vector_type<N> w (wi.begin (), wi.end ());
      verify_basic_exception_safety (
        [=] (vector_type<N>& v)
        {
          v.assign (std::make_move_iterator (make_input_it (w.begin ())),
                    std::make_move_iterator (make_input_it (w.end ())));
        },
        vi,
        m_alloc);
    }
    {
      vector_type<N> w (wi.begin (), wi.end ());
      verify_basic_exception_safety (
        [=] (vector_type<N>& v)
        {
          v.assign (std::make_move_iterator (make_fwd_it (w.begin ())),
                    std::make_move_iterator (make_fwd_it (w.end ())));
        },
        vi,
        m_alloc);
    }
    {
      vector_type<N> w (wi.begin (), wi.end ());
      verify_basic_exception_safety (
        [=] (vector_type<N>& v)
        {
          v.assign (
            make_input_it (make_reverse_it (std::make_move_iterator (w.end ()))),
            make_input_it (make_reverse_it (std::make_move_iterator (w.begin ()))));
        },
        vi,
        m_alloc);
    }
    {
      vector_type<N> w (wi.begin (), wi.end ());
      verify_basic_exception_safety (
        [=] (vector_type<N>& v)
        {
          v.assign (
            make_fwd_it (make_reverse_it (std::make_move_iterator (w.end ()))),
            make_fwd_it (make_reverse_it (std::make_move_iterator (w.begin ()))));
        },
        vi,
        m_alloc);
    }
  }

  template <unsigned N, typename U = T,
            typename std::enable_if<! std::is_base_of<gch::test_types::triggering_base, U>::value
            >::type * = nullptr>
  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  check (vector_init_type<N> vi, std::initializer_list<T> wi)
  {
    vector_type<N> v_cmp (wi);
    vector_type<N> v_rcmp (make_reverse_it (wi.end ()),
                           make_reverse_it (wi.begin ()));
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);

      vi (v);

      v.assign (wi.begin (), wi.end ());
      CHECK (v == v_cmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);

      vi (v);

      v.assign (make_input_it (wi.begin ()), make_input_it (wi.end ()));
      CHECK (v == v_cmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);

      vi (v);

      v.assign (make_fwd_it (wi.begin ()), make_fwd_it (wi.end ()));
      CHECK (v == v_cmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (std::make_move_iterator (w.begin ()),
                std::make_move_iterator (w.end ()));
      CHECK (v == v_cmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (std::make_move_iterator (make_input_it (w.begin ())),
                std::make_move_iterator (make_input_it (w.end ())));
      CHECK (v == v_cmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (std::make_move_iterator (make_fwd_it (w.begin ())),
                std::make_move_iterator (make_fwd_it (w.end ())));
      CHECK (v == v_cmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (w.rbegin (), w.rend ());
      CHECK (v == v_rcmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (make_input_it (w.rbegin ()), make_input_it (w.rend ()));
      CHECK (v == v_rcmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (make_fwd_it (w.rbegin ()), make_fwd_it (w.rend ()));
      CHECK (v == v_rcmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (std::make_move_iterator (w.rbegin ()),
                std::make_move_iterator (w.rend ()));
      CHECK (v == v_rcmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (std::make_move_iterator (make_input_it (w.rbegin ())),
                std::make_move_iterator (make_input_it (w.rend ())));
      CHECK (v == v_rcmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (std::make_move_iterator (make_fwd_it (w.rbegin ())),
                std::make_move_iterator (make_fwd_it (w.rend ())));
      CHECK (v == v_rcmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (make_reverse_it (std::make_move_iterator (w.end ())),
                make_reverse_it (std::make_move_iterator (w.begin ())));
      CHECK (v == v_rcmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (make_input_it (make_reverse_it (std::make_move_iterator (w.end ()))),
                make_input_it (make_reverse_it (std::make_move_iterator (w.begin ()))));
      CHECK (v == v_rcmp);
    }
    {
      vector_type<N> v (vi.begin (), vi.end (), m_alloc);
      vector_type<N> w (wi.begin (), wi.end ());

      vi (v);

      v.assign (make_fwd_it (make_reverse_it (std::make_move_iterator (w.end ()))),
                make_fwd_it (make_reverse_it (std::make_move_iterator (w.begin ()))));
      CHECK (v == v_rcmp);
    }
  }

  Allocator m_alloc;
};

GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test (void)
{
  using namespace gch::test_types;

  test_with_allocator<tester, std::allocator> ();
  test_with_allocator<tester, sized_allocator, std::uint8_t> ();
  test_with_allocator<tester, fancy_pointer_allocator> ();
  test_with_allocator<tester, allocator_with_id> ();
  test_with_allocator<tester, propagating_allocator_with_id> ();

#ifndef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
  test_with_allocator<tester, verifying_allocator> ();
  test_with_allocator<tester, non_propagating_verifying_allocator> ();
#endif

  // Test assignment with a range of values that are not assignable to the elements.
  struct wrapped
  {
    long data;
  };

  struct multiple_args
  {
    GCH_SMALL_VECTOR_TEST_CONSTEXPR
    multiple_args (int x, wrapped y, std::size_t z) noexcept
      : value (static_cast<std::size_t> (x) ^ static_cast<std::size_t> (y.data) ^ z)
    { }

    GCH_SMALL_VECTOR_TEST_CONSTEXPR explicit
    multiple_args (std::tuple<int, wrapped, std::size_t> t) noexcept
      : multiple_args (std::get<0> (t), std::get<1> (t), std::get<2> (t))
    { }

    std::size_t value;
  };

  int         x[] {  0, 1, 2, 3 };
  wrapped     y[] { { 4 }, { 5 }, { 6 }, { 7 } };
  std::size_t z[] {  8, 9, 10, 11 };

  std::array<std::tuple<int, wrapped, std::size_t>, 4> ts;
  for (std::size_t i = 0; i < 4; ++i)
    ts[i] = std::make_tuple (x[i], y[i], z[i]);

  gch::small_vector<multiple_args> v;
  v.emplace_back (x[0], y[0], z[0]);
  v.emplace_back (x[1], y[1], z[1]);

  v.assign (std::prev (ts.end ()), ts.end ());

  CHECK (1 == v.size () && multiple_args { x[3], y[3], z[3] }.value == v[0].value);

  v.assign (ts.begin (), std::prev (ts.end ()));

  CHECK (3 == v.size ());
  for (std::size_t i = 0; i < 3; ++i)
    CHECK (multiple_args { x[i], y[i], z[i] }.value == v[i].value);

  return 0;
}
