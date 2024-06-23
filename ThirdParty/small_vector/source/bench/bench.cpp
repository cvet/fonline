//=======================================================================
// Copyright (c) 2014 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include "gch/small_vector.hpp"

#ifdef GCH_WITH_BOOST
#  include <boost/container/small_vector.hpp>
#endif

#ifdef GCH_WITH_LLVM
#  include <llvm/ADT/SmallVector.h>
#endif

#ifdef GCH_WITH_FOLLY
#  include <folly/small_vector.h>
#endif

#include <array>
#include <iostream>
#include <typeinfo>
#include <random>
#include <vector>

#include "bench.hpp"
#include "policies.hpp"

template <typename T, std::size_t BufferSize = static_cast<std::size_t> (-1)>
struct benched_containers
{
  using type = std::tuple<
    gch::small_vector<T, BufferSize>
    , std::vector<T>
#ifdef GCH_WITH_BOOST
    , boost::container::small_vector<T, BufferSize>
#endif
#ifdef GCH_WITH_LLVM
    , llvm::SmallVector<T, BufferSize>
#endif
#ifdef GCH_WITH_FOLLY
    , folly::small_vector<T, BufferSize>
#endif
    >;
};

template <typename T>
struct benched_containers<T, static_cast<std::size_t> (-1)>
{
  using type = std::tuple<
    gch::small_vector<T>
    , std::vector<T>
#ifdef GCH_WITH_BOOST
    , boost::container::small_vector<T, gch::default_buffer_size<std::allocator<T>>::value>
#endif
#ifdef GCH_WITH_LLVM
    , llvm::SmallVector<T>
#endif
#ifdef GCH_WITH_FOLLY
    , folly::small_vector<T>
#endif
    >;
};

template <typename T, std::size_t BufferSize = static_cast<std::size_t> (-1)>
using benched_containers_t = typename benched_containers<T, BufferSize>::type;

template <typename T, unsigned N, typename Allocator>
struct container_name<gch::small_vector<T, N, Allocator>>
{
  static constexpr
  const char *
  value = "gch::small_vector";
};

template <typename T, typename Allocator>
struct container_name<std::vector<T, Allocator>>
{
  static constexpr
  const char *
  value = "std::vector";
};

#ifdef GCH_WITH_BOOST

template <typename T, std::size_t N, typename Allocator>
struct container_name<boost::container::small_vector<T, N, Allocator>>
{
  static constexpr
  const char *
  value = "boost::container::small_vector";
};

#endif

#ifdef GCH_WITH_LLVM

template <typename T, unsigned N>
struct container_name<llvm::SmallVector<T, N>>
{
  static constexpr
  const char *
  value = "llvm::SmallVector";
};

#endif

#ifdef GCH_WITH_FOLLY

template <typename T, std::size_t N>
struct container_name<folly::small_vector<T, N>>
{
  static constexpr
  const char *
  value = "folly::small_vector";
};

#endif

using std::chrono::milliseconds;
using std::chrono::microseconds;

namespace
{

  template <typename T>
  constexpr bool
  is_trivial_of_size (std::size_t size)
  {
    return std::is_trivial<T>::value && sizeof (T) == size;
  }

  template <typename T>
  constexpr bool
  is_non_trivial_of_size (std::size_t size)
  {
    return
      !std::is_trivial<T>::value
        && sizeof (T) == size
        && std::is_copy_constructible<T>::value
        && std::is_copy_assignable<T>::value
        && std::is_move_constructible<T>::value
        && std::is_move_assignable<T>::value;
  }

  template <typename T>
  constexpr bool
  is_non_trivial_nothrow_movable ()
  {
    return
      !std::is_trivial<T>::value
        && std::is_nothrow_move_constructible<T>::value
        && std::is_nothrow_move_assignable<T>::value;
  }

  template <typename T>
  constexpr bool
  is_non_trivial_non_nothrow_movable ()
  {
    return
      !std::is_trivial<T>::value
        && std::is_move_constructible<T>::value
        && std::is_move_assignable<T>::value
        && !std::is_nothrow_move_constructible<T>::value
        && !std::is_nothrow_move_assignable<T>::value;
  }

  template <typename T>
  constexpr bool
  is_non_trivial_non_movable ()
  {
    return
      !std::is_trivial<T>::value
        && std::is_copy_constructible<T>::value
        && std::is_copy_assignable<T>::value
        && !std::is_move_constructible<T>::value
        && !std::is_move_assignable<T>::value;
  }

  template <typename T>
  constexpr bool
  is_small ()
  {
    return sizeof (T) <= sizeof (std::size_t);
  }
} //end of anonymous namespace

// tested types

// trivial type with parametrized size
template <int N>
struct Trivial
{
  std::size_t a;
  std::array<unsigned char, N - sizeof (a)> b;

  bool operator< (const Trivial& other) const { return a < other.a; }
};

template <>
struct Trivial<sizeof (std::size_t)>
{
  std::size_t a;

  bool operator< (const Trivial& other) const { return a < other.a; }
};

// non trivial, quite expensive to copy but easy to move (noexcept not set)
class NonTrivialStringMovable
{
private:
  std::string data { "some pretty long string to make sure it is not optimized with SSO" };

public:
  std::size_t a { 0 };

  NonTrivialStringMovable            (void)                                      = default;
  NonTrivialStringMovable            (const NonTrivialStringMovable&)            = default;
//NonTrivialStringMovable            (NonTrivialStringMovable&&) noexcept(false) = impl;
  NonTrivialStringMovable& operator= (const NonTrivialStringMovable&)            = default;
//NonTrivialStringMovable& operator= (NonTrivialStringMovable&&) noexcept(false) = impl;
  ~NonTrivialStringMovable           (void)                                      = default;

  NonTrivialStringMovable (NonTrivialStringMovable&& other) noexcept (false)
    : a (other.a)
  {
    delete new int;
  }

  NonTrivialStringMovable&
  operator= (NonTrivialStringMovable&& other) noexcept (false)
  {
    a = other.a;
    delete new int;
    return *this;
  }

  NonTrivialStringMovable (std::size_t a_)
    : a (a_)
  { }

  bool operator< (const NonTrivialStringMovable& other) const { return a < other.a; }
};

// non trivial, quite expensive to copy but easy to move (with noexcept)
class NonTrivialStringMovableNoExcept
{
private:
  std::string data { "some pretty long string to make sure it is not optimized with SSO" };

public:
  std::size_t a { 0 };

  NonTrivialStringMovableNoExcept            (void)                                       = default;
  NonTrivialStringMovableNoExcept            (const NonTrivialStringMovableNoExcept&)     = default;
  NonTrivialStringMovableNoExcept            (NonTrivialStringMovableNoExcept&&) noexcept = default;
  NonTrivialStringMovableNoExcept& operator= (const NonTrivialStringMovableNoExcept&)     = default;
//NonTrivialStringMovableNoExcept& operator= (NonTrivialStringMovableNoExcept&&) noexcept = impl;
  ~NonTrivialStringMovableNoExcept           (void)                                       = default;

  NonTrivialStringMovableNoExcept&
  operator= (NonTrivialStringMovableNoExcept&& other) noexcept
  {
    std::swap (data, other.data);
    std::swap (a, other.a);
    return *this;
  }

  NonTrivialStringMovableNoExcept (std::size_t a_)
    : a (a_)
  { }

  bool
  operator< (const NonTrivialStringMovableNoExcept& other) const
  {
    return a < other.a;
  }
};

// non trivial, quite expensive to copy and move
template <int N>
class NonTrivialArray
{
public:
  std::size_t a = 0;

private:
  std::array<unsigned char, N - sizeof (a)> b;

public:
  NonTrivialArray () = default;

  NonTrivialArray (std::size_t a_)
    : a (a_)
  { }

  ~NonTrivialArray () = default;

  bool
  operator< (const NonTrivialArray& other) const
  {
    return a < other.a;
  }
};

// type definitions for testing and invariants check
using TrivialSmall = Trivial<8>;      static_assert (is_trivial_of_size<TrivialSmall> (8),
                                                      "Invalid type");
using TrivialMedium = Trivial<32>;    static_assert (is_trivial_of_size<TrivialMedium> (32),
                                                       "Invalid type");
using TrivialLarge = Trivial<128>;    static_assert (is_trivial_of_size<TrivialLarge> (128),
                                                      "Invalid type");
using TrivialHuge = Trivial<1024>;    static_assert (is_trivial_of_size<TrivialHuge> (1024),
                                                     "Invalid type");
using TrivialMonster = Trivial<4 * 1024>;  static_assert (
  is_trivial_of_size<TrivialMonster> (4U * 1024U), "Invalid type");

static_assert (is_non_trivial_nothrow_movable<NonTrivialStringMovableNoExcept> (), "Invalid type");
static_assert (is_non_trivial_non_nothrow_movable<NonTrivialStringMovable> (), "Invalid type");

using NonTrivialArrayMedium = NonTrivialArray<32>;
static_assert (is_non_trivial_of_size<NonTrivialArrayMedium> (32), "Invalid type");

constexpr std::size_t small_sizes[] {
  1000,
  // 2000,
  3000,
  // 4000,
  5000,
  // 6000,
  7000,
  // 8000,
  9000,
  // 10000
};

constexpr std::size_t medium_sizes[] {
  10000,
  // 20000,
  30000,
  // 40000,
  50000,
  // 60000,
  70000,
  // 80000,
  90000,
  // 100000
};

constexpr std::size_t big_sizes[] {
  100000,
  // 200000,
  300000,
  // 400000,
  500000,
  // 600000,
  700000,
  // 800000,
  900000,
  // 1000000
};

namespace detail {
  template <std::size_t... Is>
  struct index_sequence
  { };

  template<std::size_t N, std::size_t... Is>
  struct make_index_sequence : make_index_sequence<N - 1, N - 1, Is...>
  { };

  template<std::size_t... Is>
  struct make_index_sequence<0, Is...> : index_sequence<Is...>
  { };

  template <class T, std::size_t N, std::size_t... I>
  static constexpr
  std::array<typename std::remove_const<T>::type, N>
  to_array_impl (T (&a)[N], index_sequence<I...>)
  {
    return { { a[I]... } };
  }
}

template <class T, std::size_t N>
static constexpr
std::array<typename std::remove_const<T>::type, N>
to_array (T (&raw)[N])
{
  return detail::to_array_impl (raw, detail::make_index_sequence<N> { });
}

// Define all benchmarks

template <typename T>
struct bench_fill_back
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "fill_back", "us");

    constexpr auto sizes = to_array (big_sizes);

    bench_containers<benched_containers_t<T>, microseconds, Empty, FillBack> (
      g,
      std::begin (sizes),
      std::end (sizes));

    bench_containers<benched_containers_t<T>, microseconds, Empty, ReserveSize, FillBack> (
      g,
      std::begin (sizes),
      std::end (sizes),
      " with reserve");
  }
};

template <typename T>
struct bench_emplace_back
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "emplace_back", "us");

    constexpr auto sizes = to_array (big_sizes);

    bench_containers<benched_containers_t<T>, microseconds, Empty, EmplaceBack> (
      g,
      std::begin (sizes),
      std::end (sizes));


    bench_containers<benched_containers_t<T>, microseconds, Empty, ReserveSize, EmplaceBack> (
      g,
      std::begin (sizes),
      std::end (sizes),
      " with reserve");
  }
};

template <typename T>
struct bench_emplace_back_multiple
{
  static void run (graphs::graph_manager& graph_man)
  {
    // constexpr std::size_t buf_size = gch::default_buffer_size<std::allocator<T>>::value;
    constexpr std::size_t buf_size = 5;

    graphs::graph& g = add_graph<T> (graph_man, "emplace_back multiple (buffer size: " + std::to_string (buf_size) + ")",
                  "us");

    std::vector<std::size_t> sizes;
    for (std::size_t i = 0; i <= buf_size + 5; ++i)
      sizes.push_back (i);

    bench_containers<benched_containers_t<T, buf_size>, microseconds, Empty, EmplaceBackMultiple> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_fill_front
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "fill_front", "us");
    constexpr auto sizes = to_array (medium_sizes);

    // it is too slow with bigger data types
    if (is_small<T> ())
    {
      bench_containers<benched_containers_t<T>, microseconds, Empty, FillFront> (
        g,
        std::begin (sizes),
        std::end (sizes));
      // bench<std::vector<T>, microseconds, Empty, FillFront> ("vector", std::begin (sizes), std::end (sizes));
    }

    // if (is_small<T> ())
    // {
    //   bench<gch::small_vector<T>, microseconds, Empty, FillFront> ("small_vector", std::begin (sizes), std::end (sizes));
    // }
  }
};

template <typename T>
struct bench_emplace_front
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "emplace_front", "us");
    constexpr auto sizes = to_array (medium_sizes);
    // it is too slow with bigger data types
    if (is_small<T> ())
    {
      bench_containers<benched_containers_t<T>, microseconds, Empty, EmplaceFront> (
        g,
        std::begin (sizes),
        std::end (sizes));
      // bench<std::vector<T>, microseconds, Empty, EmplaceFront> ("vector", std::begin (sizes), std::end (sizes));
    }

    // if (is_small<T> ())
    // {
    //   bench<gch::small_vector<T>, microseconds, Empty, EmplaceFront> ("small_vector", std::begin (sizes), std::end (sizes));
    // }
  }
};

template <typename T>
struct bench_linear_search
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "linear_search", "us");
    constexpr auto sizes = to_array (small_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, Find> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_random_insert
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "random_insert", "ms");
    constexpr auto sizes = to_array (medium_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, Insert> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_random_remove
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "random_remove", "us");
    constexpr auto sizes = to_array (medium_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, Erase> (
      g,
      std::begin (sizes),
      std::end (sizes));

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, RemoveErase> (
      g,
      std::begin (sizes),
      std::end (sizes),
      " (remove-erase)");
  }
};

template <typename T>
struct bench_sort
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "sort", "ms");
    constexpr auto sizes = to_array (medium_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, Sort> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_destruction
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "destruction", "us");
    constexpr auto sizes = to_array (medium_sizes);

    bench_containers<benched_containers_t<T>, microseconds, SmartFilled, SmartDelete> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_number_crunching
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "number_crunching", "ms");
    constexpr auto sizes = to_array (medium_sizes);

    bench_containers<benched_containers_t<T>, microseconds, Empty, RandomSortedInsert> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_erase_1
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "erase1", "us");
    constexpr auto sizes = to_array (small_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, RandomErase1> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_erase_10
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "erase10", "us");
    constexpr auto sizes = to_array (small_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, RandomErase10> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_erase_25
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "erase25", "us");
    constexpr auto sizes = to_array (small_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, RandomErase25> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_erase_50
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "erase50", "us");
    constexpr auto sizes = to_array (small_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, RandomErase50> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_traversal
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "traversal", "us");
    constexpr auto sizes = to_array (medium_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, Iterate> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_write
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "write", "us");
    constexpr auto sizes = to_array (medium_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, Write> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

template <typename T>
struct bench_find
{
  static void run (graphs::graph_manager& graph_man)
  {
    graphs::graph& g = add_graph<T> (graph_man, "find", "us");
    constexpr auto sizes = to_array (medium_sizes);

    bench_containers<benched_containers_t<T>, microseconds, FilledRandom, Find> (
      g,
      std::begin (sizes),
      std::end (sizes));
  }
};

//Launch the benchmark

template <typename ...Types>
graphs::graph_manager&
bench_all (graphs::graph_manager& graph_man)
{
  bench_types<bench_fill_back, Types...> (graph_man);
  bench_types<bench_emplace_back, Types...> (graph_man);
  bench_types<bench_emplace_back_multiple, Types...> (graph_man);
  // bench_types<bench_fill_front, Types...> (graph_man);
  // bench_types<bench_emplace_front, Types...> (graph_man);
  // bench_types<bench_linear_search, Types...> (graph_man);
  // bench_types<bench_write, Types...> (graph_man);
  bench_types<bench_random_insert, Types...> (graph_man);
  bench_types<bench_random_remove, Types...> (graph_man);
  // bench_types<bench_sort, Types...> (graph_man);
  // bench_types<bench_destruction, Types...> (graph_man);
  // bench_types<bench_erase_1, Types...> (graph_man);
  bench_types<bench_erase_10, Types...> (graph_man);
  // bench_types<bench_erase_25, Types...> (graph_man);
  // bench_types<bench_erase_50, Types...> (graph_man);

  // The following are really slow so run only for limited set of data
  // bench_types<bench_find, TrivialSmall, TrivialMedium, TrivialLarge> ();
  // bench_types<bench_number_crunching, TrivialSmall, TrivialMedium> ();
  return graph_man;
}

int
main (void)
{
  graphs::graph_manager graph_man;

  try
  {
    bench_all<
      TrivialSmall
      , TrivialMedium
      , TrivialLarge
      // , TrivialHuge
      // , TrivialMonster
      , NonTrivialStringMovable
      , NonTrivialStringMovableNoExcept
      // , NonTrivialArray<32>
      > (graph_man);

    graph_man.generate_output (graphs::graph_manager::output_type::GOOGLE);
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what () << std::endl;
    return 1;
  }

  return 0;
}
