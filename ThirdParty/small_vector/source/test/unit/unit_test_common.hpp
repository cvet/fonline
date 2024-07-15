/** unit_test_common.hpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef SMALL_VECTOR_UNIT_TEST_COMMON_HPP
#define SMALL_VECTOR_UNIT_TEST_COMMON_HPP

#include "test_common.hpp"
#include "test_types.hpp"

#include <vector>

template <typename T, unsigned N, typename Allocator>
class vector_initializer
{
public:
  using value_type = gch::small_vector<T, N, Allocator>;

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  vector_initializer (std::initializer_list<T> data)
    : m_data (data)
  { }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  vector_initializer (std::initializer_list<T> data, void (*prepare)(value_type&))
    : m_data (data),
      m_prepare (prepare)
  { }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  typename value_type::const_iterator
  begin (void) const noexcept
  {
    return m_data.begin ();
  }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  typename value_type::const_iterator
  end (void) const noexcept
  {
    return m_data.end ();
  }

  GCH_SMALL_VECTOR_TEST_CONSTEXPR
  void
  operator() (value_type& v) const
  {
    if (m_prepare)
      m_prepare (v);
  }

private:
  // We use a small_vector to store the data so that we can test constexpr.
  value_type m_data;
  void (* m_prepare) (value_type&) = nullptr;
};

template <typename Functor>
struct exception_stability_verifier_base
{
  template <typename ...Args>
  bool
  test (std::vector<std::size_t>& test_counts, Args&&... args) const
  {
    using namespace gch::test_types;

    std::for_each (test_counts.rbegin (), test_counts.rend (), [&](std::size_t c) {
      exception_trigger::push (c);
    });

    GCH_TRY
    {
      m_functor (std::forward<Args> (args)...);
    }
    GCH_CATCH (const test_exception&)
    { }

    if (exception_trigger::has_pending_throws ())
    {
      exception_trigger::reset ();
      test_counts.pop_back ();
      return false;
    }
    else if (0 != exception_trigger::extra_test_count ())
      test_counts.push_back (0);
    else
      ++test_counts.back ();

    return true;
  }

  Functor m_functor;
};

template <typename Functor, typename ...VectorType>
class exception_safety_verifier;

template <typename Functor, typename T, typename Allocator, unsigned N>
class exception_safety_verifier<Functor, gch::small_vector<T, N, Allocator>>
  : exception_stability_verifier_base<Functor>
{
public:
  using base             = exception_stability_verifier_base<Functor>;
  using vector_init_type = vector_initializer<T, N, Allocator>;
  using vector_type      = gch::small_vector<T, N, Allocator>;

  exception_safety_verifier (Functor functor,
                             vector_init_type vi,
                             Allocator alloc,
                             bool strong = false)
    : base     { std::move (functor) },
      m_vi     (std::move (vi)),
      m_alloc  (std::move (alloc)),
      m_strong (strong)
  { }

  void
  operator() (void) const
  {
    using namespace gch::test_types;

    std::vector<std::size_t> test_counts;
    test_counts.push_back (0);

    gch::small_vector<T, N, Allocator> v_cmp (m_vi.begin (), m_vi.end (), m_alloc);

    do
    {
      gch::small_vector<T, N, Allocator> v (m_vi.begin (), m_vi.end (), m_alloc);
      m_vi (v);

      bool threw = base::test (test_counts, v);

      if (m_strong && threw)
        CHECK (v == v_cmp);
    } while (! test_counts.empty ());
  }

private:
  vector_init_type m_vi;
  Allocator        m_alloc;
  bool             m_strong;
};

template <typename Functor, typename T, typename Allocator, unsigned N, unsigned M>
class exception_safety_verifier<Functor,
                                gch::small_vector<T, N, Allocator>,
                                gch::small_vector<T, M, Allocator>>
  : exception_stability_verifier_base<Functor>
{
public:
  using base = exception_stability_verifier_base<Functor>;

  template <unsigned K>
  using vector_init_type = vector_initializer<T, K, Allocator>;

  template <unsigned K>
  using vector_type = gch::small_vector<T, K, Allocator>;

  exception_safety_verifier (Functor functor,
                             vector_init_type<N> ni,
                             vector_init_type<M> mi,
                             Allocator alloc_n,
                             Allocator alloc_m,
                             bool strong = false)
    : base      { std::move (functor) },
      m_ni      (std::move (ni)),
      m_mi      (std::move (mi)),
      m_alloc_n (std::move (alloc_n)),
      m_alloc_m (std::move (alloc_m)),
      m_strong  (strong)
  { }

  void
  operator() (void) const
  {
    using namespace gch::test_types;

    {
      gch::small_vector<T, N, Allocator> n_cmp (m_ni.begin (), m_ni.end (), m_alloc_n);

      std::vector<std::size_t> test_counts;
      test_counts.push_back (0);

      do
      {
        vector_type<N> n (m_ni.begin (), m_ni.end (), m_alloc_n);
        vector_type<M> m (m_mi.begin (), m_mi.end (), m_alloc_m);

        m_ni (n);
        m_mi (m);

        bool threw = base::test (test_counts, n, m);

        if (m_strong && threw)
          CHECK (n == n_cmp);

      } while (! test_counts.empty ());
    }

    {
      gch::small_vector<T, N, Allocator> n_cmp (m_mi.begin (), m_mi.end (), m_alloc_n);

      std::vector<std::size_t> test_counts;
      test_counts.push_back (0);

      do
      {
        vector_type<N> n (m_mi.begin (), m_mi.end (), m_alloc_n);
        vector_type<M> m (m_ni.begin (), m_ni.end (), m_alloc_m);

        m_ni (n);
        m_mi (m);

        bool threw = base::test (test_counts, n, m);

        if (m_strong && threw)
          CHECK (n == n_cmp);

      } while (! test_counts.empty ());
    }
  }

private:
  vector_init_type<N> m_ni;
  vector_init_type<M> m_mi;
  Allocator           m_alloc_n;
  Allocator           m_alloc_m;
  bool                m_strong;
};

template <typename Functor, typename T, unsigned N, typename Allocator = std::allocator<T>>
inline
void
verify_basic_exception_safety (Functor f,
                               vector_initializer<T, N, Allocator> vi,
                               Allocator alloc = Allocator ())
{
  exception_safety_verifier<Functor, gch::small_vector<T, N, Allocator>> {
    std::move (f),
    std::move (vi),
    std::move (alloc)
  } ();
}

template <typename Functor,
          typename T,
          unsigned N,
          unsigned M,
          typename Allocator = std::allocator<T>>
inline
void
verify_basic_exception_safety (Functor f,
                               vector_initializer<T, N, Allocator> ni,
                               vector_initializer<T, M, Allocator> mi,
                               Allocator alloc_n = Allocator (),
                               Allocator alloc_m = Allocator ())
{
  exception_safety_verifier<Functor,
                            gch::small_vector<T, N, Allocator>,
                            gch::small_vector<T, M, Allocator>> {
    std::move (f),
    std::move (ni),
    std::move (mi),
    std::move (alloc_n),
    std::move (alloc_m)
  } ();
}

template <typename Functor, typename T, unsigned N, typename Allocator = std::allocator<T>>
inline
void
verify_strong_exception_guarantee (Functor f,
                                   vector_initializer<T, N, Allocator> vi,
                                   Allocator alloc = Allocator ())
{
  exception_safety_verifier<Functor, gch::small_vector<T, N, Allocator>> {
    std::move (f),
    std::move (vi),
    std::move (alloc),
    true
  } ();
}

template <typename Functor,
          typename T,
          unsigned N,
          unsigned M,
          typename Allocator = std::allocator<T>>
inline
void
verify_strong_exception_guarantee (Functor f,
                                   vector_initializer<T, N, Allocator> ni,
                                   vector_initializer<T, M, Allocator> mi,
                                   Allocator alloc_n = Allocator (),
                                   Allocator alloc_m = Allocator ())
{
  exception_safety_verifier<Functor,
                            gch::small_vector<T, N, Allocator>,
                            gch::small_vector<T, M, Allocator>> {
    std::move (f),
    std::move (ni),
    std::move (mi),
    std::move (alloc_n),
    std::move (alloc_m),
    true
  } ();
}

template <template <typename, typename> class TesterT,
          template <typename ...> class AllocatorT, typename ...AArgs,
          typename std::enable_if<
                std::is_constructible<AllocatorT<int, AArgs...>, int>::value
            &&  std::is_constructible<TesterT<int, AllocatorT<int, AArgs...>>,
                                      AllocatorT<int, AArgs...>,
                                      AllocatorT<int, AArgs...>>::value
          >::type * = nullptr>
inline GCH_SMALL_VECTOR_TEST_CONSTEXPR
void
test_with_allocator (void)
{
  using namespace gch::test_types;
  TesterT<trivially_copyable_data_base, AllocatorT<trivially_copyable_data_base, AArgs...>> { } ();
  TesterT<nontrivial_data_base, AllocatorT<nontrivial_data_base, AArgs...>> { } ();

  AllocatorT<trivially_copyable_data_base, AArgs...> tc_alloc_v (1);
  AllocatorT<trivially_copyable_data_base, AArgs...> tc_alloc_w (2);
  TesterT<trivially_copyable_data_base, AllocatorT<trivially_copyable_data_base, AArgs...>> {
    tc_alloc_v,
    tc_alloc_w
  } ();

  AllocatorT<nontrivial_data_base, AArgs...> nt_alloc_v (3);
  AllocatorT<nontrivial_data_base, AArgs...> nt_alloc_w (4);
  TesterT<nontrivial_data_base, AllocatorT<nontrivial_data_base, AArgs...>> {
    nt_alloc_v,
    nt_alloc_w
  } ();

#ifdef GCH_SMALL_VECTOR_TEST_EXCEPTION_SAFETY_TESTING
  TesterT<triggering_type, AllocatorT<triggering_type, AArgs...>> { } ();

  AllocatorT<triggering_type, AArgs...> trig_alloc_v (5);
  AllocatorT<triggering_type, AArgs...> trig_alloc_w (6);
  TesterT<triggering_type, AllocatorT<triggering_type, AArgs...>> {
    trig_alloc_v,
    trig_alloc_w
  } ();
#endif
}

template <template <typename, typename> class TesterT,
  template <typename ...> class AllocatorT, typename ...AArgs,
          typename std::enable_if<
                std::is_constructible<AllocatorT<int, AArgs...>, int>::value
            &&! std::is_constructible<TesterT<int, AllocatorT<int, AArgs...>>,
                                      AllocatorT<int, AArgs...>,
                                      AllocatorT<int, AArgs...>>::value
          >::type * = nullptr>
inline GCH_SMALL_VECTOR_TEST_CONSTEXPR
void
test_with_allocator (void)
{
  using namespace gch::test_types;
  TesterT<trivially_copyable_data_base, AllocatorT<trivially_copyable_data_base, AArgs...>> { } ();
  TesterT<nontrivial_data_base, AllocatorT<nontrivial_data_base, AArgs...>> { } ();

  AllocatorT<trivially_copyable_data_base, AArgs...> tc_alloc (1);
  TesterT<trivially_copyable_data_base, AllocatorT<trivially_copyable_data_base, AArgs...>> {
    tc_alloc
  } ();

  AllocatorT<nontrivial_data_base, AArgs...> nt_alloc (2);
  TesterT<nontrivial_data_base, AllocatorT<nontrivial_data_base, AArgs...>> {
    nt_alloc
  } ();

#ifdef GCH_SMALL_VECTOR_TEST_EXCEPTION_SAFETY_TESTING
  TesterT<triggering_type, AllocatorT<triggering_type, AArgs...>> { } ();

  AllocatorT<triggering_type, AArgs...> trig_alloc (3);
  TesterT<triggering_type, AllocatorT<triggering_type, AArgs...>> {
    trig_alloc
  } ();

#endif
}

template <template <typename, typename> class TesterT,
          template <typename ...> class AllocatorT, typename ...AArgs,
          typename std::enable_if<! std::is_constructible<AllocatorT<int, AArgs...>, int>::value
          >::type * = nullptr>
inline GCH_SMALL_VECTOR_TEST_CONSTEXPR
void
test_with_allocator (void)
{
  using namespace gch::test_types;
  TesterT<trivially_copyable_data_base, AllocatorT<trivially_copyable_data_base, AArgs...>> { } ();
  TesterT<nontrivial_data_base, AllocatorT<nontrivial_data_base, AArgs...>> { } ();

#ifdef GCH_SMALL_VECTOR_TEST_EXCEPTION_SAFETY_TESTING
  TesterT<triggering_type, AllocatorT<triggering_type, AArgs...>> { } ();
#endif
}

GCH_SMALL_VECTOR_TEST_CONSTEXPR
int
test (void);

#ifdef GCH_SMALL_VECTOR_TEST_FILE
#  define QUOTED_HELPER(...) #__VA_ARGS__
#  define QUOTED(...) QUOTED_HELPER (__VA_ARGS__)
#  include QUOTED (GCH_SMALL_VECTOR_TEST_FILE)
#endif

#endif // SMALL_VECTOR_UNIT_TEST_COMMON_HPP
