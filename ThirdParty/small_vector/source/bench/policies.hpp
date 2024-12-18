//=======================================================================
// Copyright (c) 2014 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

// create policies

//Create empty container

template <class Container>
struct Empty
{
  static
  Container
  make (std::size_t)
  {
    return Container ();
  }
};

//Prepare data for fill back

template <class Container>
struct EmptyPrepareBackup
{
  std::vector<typename Container::value_type> v;

  Container
  make (std::size_t size)
  {
    if (v.size () != size)
    {
      v.clear ();
      v.reserve (size);
      for (std::size_t i = 0; i < size; ++i)
        v.push_back ({ i });
    }

    return Container ();
  }
};

template <class Container>
struct Filled
{
  static
  Container
  make (std::size_t size)
  {
    return Container (size);
  }
};

template <class Container>
struct FilledRandom
{
  std::vector<typename Container::value_type> v;

  Container
  make (std::size_t size)
  {
    // prepare randomized data that will have all the integers from the range
    if (v.size () != size)
    {
      v.clear ();
      v.reserve (size);
      for (std::size_t i = 0 ; i < size ; ++i)
        v.push_back ({ i });
      std::shuffle (begin (v), end (v), std::mt19937 ());
    }

    // fill with randomized data
    Container container;
    for (std::size_t i = 0 ; i < size ; ++i)
      container.push_back (v[i]);

    return container;
  }
};

template <class Container>
struct FilledRandomInsert
{
  std::vector<typename Container::value_type> v;

  Container
  make (std::size_t size)
  {
    // prepare randomized data that will have all the integers from the range
    if (v.size () != size)
    {
      v.clear ();
      v.reserve (size);
      for (std::size_t i = 0 ; i < size ; ++i)
        v.push_back ({ i });
      std::shuffle (begin (v), end (v), std::mt19937 ());
    }

    // fill with randomized data
    Container container;
    for (std::size_t i = 0 ; i < size ; ++i)
      container.insert (v[i]);

    return container;
  }
};

template <class Container>
struct SmartFilled
{
  static
  std::unique_ptr<Container>
  make (std::size_t size)
  {
    return std::unique_ptr<Container> (new Container (size, typename Container::value_type ()));
  }
};

template <class Container>
struct BackupSmartFilled
{
  std::vector<typename Container::value_type> v;

  std::unique_ptr<Container>
  make (std::size_t size)
  {
    if (v.size () != size)
    {
      v.clear ();
      v.reserve (size);
      for (std::size_t i = 0 ; i < size ; ++i)
        v.push_back ({ i });
    }

    std::unique_ptr<Container> container (new Container ());

    for (std::size_t i = 0 ; i < size ; ++i)
      container->push_back (v[i]);

    return container;
  }
};

// testing policies

template <class Container>
struct NoOp
{
  void
  operator() (Container&, std::size_t)
  {
    //Nothing
  }
};

template <class Container>
struct ReserveSize
{
  void
  operator() (Container& c, std::size_t size)
  {
    c.reserve (size);
  }
};

template <class Container>
struct InsertSimple
{
  const typename Container::value_type value { };

  void
  operator() (Container& c, std::size_t size)
  {
    for (size_t i = 0 ; i < size ; ++i)
      c.insert (value);
  }
};

template <class Container>
struct FillBack
{
  const typename Container::value_type value { };

  void
  operator() (Container& c, std::size_t size)
  {
    for (size_t i = 0 ; i < size ; ++i)
      c.push_back (value);
  }
};

template <class Container>
struct FillBackBackup
{
  void
  operator() (Container& c, std::size_t size)
  {
    for (size_t i = 0 ; i < size ; ++i)
      c.push_back (EmptyPrepareBackup<Container>::v[i]);
  }
};

template <class Container>
struct FillBackInserter
{
  const typename Container::value_type value { };

  void
  operator() (Container& c, std::size_t size)
  {
    std::fill_n (std::back_inserter (c), size, value);
  }
};

template <class Container>
struct EmplaceBack
{
  void
  operator() (Container& c, std::size_t size)
  {
    for (size_t i = 0 ; i < size ; ++i)
      c.emplace_back ();
  }
};

template <class Container>
struct EmplaceBackMultiple
{
  void
  operator() (Container& c, std::size_t size)
  {
    for (size_t i = 0 ; i < 100000 ; ++i)
    {
      Container cc = c;
      for (size_t j = 0 ; j < size ; ++j)
        cc.emplace_back ();
    }
  }
};

template <class Container>
struct EmplaceInsertSimple
{
  void
  operator() (Container& c, std::size_t size)
  {
    for (size_t i = 0 ; i < size ; ++i)
      c.emplace ();
  }
};

template <class Container>
struct FillFront
{
  const typename Container::value_type value { };

  void
  operator() (Container& c, std::size_t size)
  {
    std::fill_n (std::front_inserter (c), size, value);
  }
};

template <class T>
struct FillFront<std::vector<T>>
{
  const T value { };

  void
  operator() (std::vector<T>& c, std::size_t size)
  {
    for (std::size_t i = 0 ; i < size ; ++i)
      c.insert (begin (c), value);
  }
};

template <class T>
struct FillFront<gch::small_vector<T>>
{
  const T value { };

  void
  operator() (gch::small_vector<T>& c, std::size_t size)
  {
    for (std::size_t i = 0 ; i < size ; ++i)
      c.insert (begin (c), value);
  }
};

template <class Container>
struct EmplaceFront
{
  void
  operator() (Container& c, std::size_t size)
  {
    for (size_t i = 0 ; i < size ; ++i)
      c.emplace_front ();
  }
};

template <class T>
struct EmplaceFront<gch::small_vector<T>>
{
  void
  operator() (gch::small_vector<T>& c, std::size_t size)
  {
    for (std::size_t i = 0 ; i < size ; ++i)
      c.emplace (begin (c));
  }
};

template <class T>
struct EmplaceFront<std::vector<T>>
{
  void
  operator() (std::vector<T>& c, std::size_t size)
  {
    for (std::size_t i = 0 ; i < size ; ++i)
      c.emplace (begin (c));
  }
};

template <class Container>
struct Find
{
  std::size_t X;

  void
  operator() (Container& c, std::size_t size)
  {
    for (std::size_t i = 0 ; i < size ; ++i)
    {
      // hand written comparison to eliminate temporary object creation
      if (std::find_if (std::begin (c), std::end (c),
                        [&](decltype (*std::begin (c)) v) { return v.a == i; }) == std::end (c))
      {
        ++X;
      }
    }
  }
};

template <class Container>
struct Insert
{
  void
  operator() (Container& c, std::size_t)
  {
    for (std::size_t i = 0 ; i < 1000 ; ++i)
    {
      // hand written comparison to eliminate temporary object creation
      auto it = std::find_if (std::begin (c), std::end (c),
                              [&](decltype (*std::begin (c)) v) { return v.a == i; });
      c.insert (it, values[i]);
    }
  }

  std::array<typename Container::value_type, 1000> values;
};

template <class Container>
struct Write
{
  void
  operator() (Container& c, std::size_t)
  {
    std::for_each (std::begin (c), std::end (c), [&](decltype (*std::begin (c)) v) {
      ++(v.a);
    });
  }
};

template <class Container>
struct Iterate
{
  void
  operator() (Container& c, std::size_t)
  {
    auto it = std::begin (c);
    auto end = std::end (c);

    while (it != end)
      ++it;
  }
};

template <class Container>
struct Erase
{
  void
  operator() (Container& c, std::size_t)
  {
    for (std::size_t i = 0 ; i < 1000 ; ++i)
    {
      // hand written comparison to eliminate temporary object creation
      c.erase (std::find_if (std::begin (c), std::end (c),
                             [&](decltype (*std::begin (c)) v) { return v.a == i; }));
    }
  }
};

template <class Container>
struct RemoveErase
{
  void
  operator() (Container& c, std::size_t)
  {
    // hand written comparison to eliminate temporary object creation
    c.erase (std::remove_if (std::begin (c), std::end (c),
                             [&](decltype (*std::begin (c)) v) { return v.a < 1000; }),
             std::end (c));
  }
};

//Sort the container

template <class Container>
struct Sort
{
  void
  operator() (Container& c, std::size_t)
  {
    std::sort (c.begin (), c.end ());
  }
};

//Reverse the container

template <class Container>
struct Reverse
{
  void
  operator() (Container& c, std::size_t)
  {
    std::reverse (c.begin (), c.end ());
  }
};

//Destroy the container

template <class Container>
struct SmartDelete
{
  void
  operator() (Container& c, std::size_t) { c.reset (); }
};

struct RandomBase
{
  RandomBase (std::size_t max)
    : distribution (0, max)
  { }

  std::mt19937 generator;
  std::uniform_int_distribution<std::size_t> distribution;
};

template <class Container>
struct RandomSortedInsert
  : RandomBase
{
  RandomSortedInsert (void)
    : RandomBase (std::numeric_limits<std::size_t>::max () - 1)
  { }

  void
  operator() (Container& c, std::size_t size)
  {
    for (std::size_t i = 0 ; i < size ; ++i)
    {
      auto val = distribution (generator);
      // hand written comparison to eliminate temporary object creation
      c.insert (
        std::find_if (begin (c), end (c), [&] (decltype (*begin (c)) v) { return v.a >= val; }),
        { val });
    }
  }
};

template <class Container>
struct RandomErase1
  : RandomBase
{
  RandomErase1 (void)
    : RandomBase (10000)
  { }

  void
  operator() (Container& c, std::size_t /*size*/)
  {
    auto it = c.begin ();

    while (it != c.end ())
    {
      if (distribution (generator) > 9900)
        it = c.erase (it);
      else
        ++it;
    }

    std::cout << c.size () << std::endl;
  }
};

template <class Container>
struct RandomErase10
  : RandomBase
{
  RandomErase10 (void)
    : RandomBase (10000)
  { }

  void
  operator() (Container& c, std::size_t /*size*/)
  {
    auto it = c.begin ();

    while (it != c.end ())
    {
      if (distribution (generator) > 9000)
        it = c.erase (it);
      else
        ++it;
    }
  }
};

template <class Container>
struct RandomErase25
  : RandomBase
{
  RandomErase25 (void)
    : RandomBase (10000)
  { }

  void
  operator() (Container& c, std::size_t /*size*/)
  {
    auto it = c.begin ();

    while (it != c.end ())
    {
      if (distribution (generator) > 7500)
        it = c.erase (it);
      else
        ++it;
    }
  }
};

template <class Container>
struct RandomErase50
  : RandomBase
{
  RandomErase50 (void)
    : RandomBase (10000)
  { }

  void
  operator() (Container& c, std::size_t /*size*/)
  {
    auto it = c.begin ();

    while (it != c.end ())
    {
      if (distribution (generator) > 5000)
        it = c.erase (it);
      else
        ++it;
    }
  }
};

// Note: This is probably erased completely for a vector
template <class Container>
struct Traversal
{
  void
  operator() (Container& c, std::size_t)
  {
    auto it = c.begin ();
    auto end = c.end ();

    while (it != end)
      ++it;
  }
};
