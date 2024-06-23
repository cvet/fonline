//=======================================================================
// Copyright (c) 2014 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include <chrono>

#include "graphs.hpp"
#include "demangle.hpp"

// chrono typedefs

// variadic policy runner

template <class Container>
inline void
run (Container&, std::size_t)
{
  //End of recursion
}

template <template <typename> class Test,
  template <typename> class ...Rest, class Container>
inline void
run (Container& container, std::size_t size)
{
  Test<Container> ()(container, size);
  run<Rest...> (container, size);
}

// benchmarking procedure

template <typename Container,
          typename DurationUnit,
  template <typename> class CreatePolicy,
  template <typename> class ...TestPolicy,
  typename Iter>
inline
void
bench (graphs::graph& g, const std::string& type, const Iter first, const Iter last)
{
  using namespace std::chrono;

  // Number of repetitions of each test
  constexpr std::size_t REPEAT = 7;

  // create an element to copy so the temporary creation
  // and initialization will not be accounted in a benchmark
  CreatePolicy<Container> creator { };
  std::for_each (first, last, [&](std::size_t size) {
    std::size_t duration = 0;
    for (std::size_t i = 0; i < REPEAT; ++i)
    {
      auto container = creator.make (size);

      time_point<high_resolution_clock> t0 = high_resolution_clock::now ();

      run<TestPolicy...> (container, size);

      time_point<high_resolution_clock> t1 = high_resolution_clock::now ();
      duration += static_cast<std::size_t> (duration_cast<DurationUnit> (t1 - t0).count ());
    }

    g.add_result (type, std::to_string (size), duration / REPEAT);
  });
}

template <typename Container>
struct container_name;

template <typename ContainersPack,
          typename DurationUnit,
          template <typename> class CreatePolicy,
          template <typename> class ...TestPolicy>
struct container_bencher;

template <template <typename...> class ContainerPackT,
          typename DurationUnit,
          template <typename> class CreatePolicy,
          template <typename> class ...TestPolicy>
struct container_bencher<ContainerPackT<>,
                         DurationUnit,
                         CreatePolicy,
                         TestPolicy...>
{
  template <typename Iter>
  void
  operator() (graphs::graph&, Iter, Iter, const std::string&)
  { }
};

template <template <typename...> class ContainerPackT,
          typename Container,
          typename ...Rest,
          typename DurationUnit,
          template <typename> class CreatePolicy,
          template <typename> class ...TestPolicy>
struct container_bencher<ContainerPackT<Container, Rest...>,
                         DurationUnit,
                         CreatePolicy,
                         TestPolicy...>
{
  template <typename Iter>
  void
  operator() (graphs::graph& g, Iter first, Iter last, const std::string& extra_name)
  {
    bench<Container, DurationUnit, CreatePolicy, TestPolicy...> (
      g,
      std::string (container_name<Container>::value).append (extra_name),
      first,
      last);

    container_bencher<ContainerPackT<Rest...>, DurationUnit, CreatePolicy, TestPolicy...> { } (
      g,
      first,
      last,
      extra_name);
  }
};

template <typename ContainersPack,
          typename DurationUnit,
          template <typename> class CreatePolicy,
          template <typename> class ...TestPolicy,
          typename Iter>
inline
void
bench_containers (graphs::graph& g,
                  const Iter first,
                  const Iter last,
                  const std::string& extra_name = "")
{
  container_bencher<ContainersPack, DurationUnit, CreatePolicy, TestPolicy...> { } (
    g,
    first,
    last,
    extra_name);
}

template <template <typename> class Benchmark>
inline
void
bench_types (graphs::graph_manager&)
{
  //Recursion end
}

template <template <typename> class Benchmark, typename T, typename ...Types>
inline
void
bench_types (graphs::graph_manager& graph_man)
{
  Benchmark<T>::run (graph_man);
  bench_types<Benchmark, Types...> (graph_man);
}

inline
bool
is_tag (int c)
{
  return std::isalnum (c) || c == '_';
}

inline
std::string
tag (std::string name)
{
  std::replace_if (begin (name), end (name), [](char c) { return !is_tag (c); }, '_');
  return name;
}

template <typename T>
inline
graphs::graph&
add_graph (graphs::graph_manager& graph_man, const std::string& testName, const std::string& unit)
{
  std::string title (testName + " - " + demangle (typeid (T).name ()));
  return graph_man.add_graph (tag (title), title, unit);
}
