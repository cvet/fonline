# gch::small_vector

This is a vector container implementation with a small buffer optimization. It doesn't have any
dependencies unlike the `boost::container::small_vector` and `llvm::SmallVector` implementations
and may be used as a drop-in header (along with the license).

Performance is about on par with the [other implementations](#Other-Implementations), but with
stricter conformance to the named requirements as specified in the standard. Consequently, this
implementation is close to API compatible with `std::vector`, and in many cases can be used as a
drop-in replacement.

This library is compatible with C++11 and up, with `constexpr` and `concept` support for C++20.

## Technical Overview

```c++
template <typename T, 
          unsigned InlineCapacity = default_buffer_size<std::allocator<T>>::value, 
          typename Allocator      = std::allocator<T>>
class small_vector;
```

A `small_vector` is a contiguous sequence container with a certain amount of dedicated storage on
the stack.  When this storage is filled up, it switches to allocating on the heap.

A `small_vector` supports insertion and erasure operations in 𝓞(1) the end, and 𝓞(n) in the middle.
It also meets the requirements of *Container*, *AllocatorAwareContainer*, *SequenceContainer*,
*ContiguousContainer*, and *ReversibleContainer*.

Template arguments may be used to define the type of stored elements, the number of elements to be
stored on the stack, and the type of allocator to be used.

When compiling with C++20 support, `small_vector` may be used in `constexpr` expressions.

A `small_vector` may be instantiated with an incomplete type `T`, but only if `InlineCapacity` is
equal to `0`.

When compiling with support for `concept`s with complete type `T`, instantiation of a `small_vector`
requires that `Allocator` meet the *Allocator* named requirements. Member functions are also
constrained by various `concept`s when `T` is complete.

The default argument for `InlineCapacity` is computed to be the number of elements needed to size
the object at 64 bytes. If the size of `T` makes this impossible to do with a non-zero number of
elements, the value of `InlineCapacity` is set to `1`.

## Usage

You have a few options to import this library into your project. It is a single-header library,
so you should be able to use it without too much fuss.

### As a Drop-In Header

Just to drop `source/include/gch/small_vector.hpp` and `docs/LICENSE` into your project.

### As a Git Submodule

You can add the project as a submodule with

```commandline
git submodule add -b main git@github.com:gharveymn/small_vector.git external/small_vector
```

and then add it as a subdirectory to your project by adding something like

```cmake
add_subdirectory (external/small_vector)
target_link_libraries (<target> PRIVATE gch::small_vector)
```

to `CMakeLists.txt`.

### As a System Library

You can install the library with (Unix-specific, remove `sudo` on Windows)

```commandline
git clone git@github.com:gharveymn/small_vector.git
cmake -D GCH_SMALL_VECTOR_ENABLE_TESTS=OFF -B small_vector/build -S small_vector
sudo cmake --install small_vector/build
```

and then link it to your project by adding

```cmake
find_package (small_vector REQUIRED)
target_link_libraries (<target> PRIVATE gch::small_vector)
```

to `CMakeLists.txt`.

## Q&A

### Can I specify the `size_type` like with `folly::small_vector`?

Not directly no, but `gch::small_vector::size_type` is derived from `std::allocator_traits`,
so you can just write a wrapper around whatever allocator you're using to modify it. This can be
done with something like

```c++
template <typename T> 
struct tiny_allocator 
  : std::allocator<T> 
{ 
  using size_type = std::uint16_t;
  
  using std::allocator<T>::allocator;

  // You don't need either of the following when compiling for C++20.
  template <typename U>
  struct rebind { using other = tiny_allocator<U>; };

  void
  max_size (void) = delete;
};

int
main (void)
{
  small_vector<int> vs;
  std::cout << "std::allocator<int>:"                         << '\n'
            << "  sizeof (vs):     " << sizeof (vs)           << '\n'
            << "  Inline capacity: " << vs.inline_capacity () << '\n'
            << "  Maximum size:    " << vs.max_size ()        << "\n\n";

  small_vector<int, default_buffer_size_v<tiny_allocator<int>>, tiny_allocator<int>> vt;
  std::cout << "tiny_allocator<int>:"                         << '\n'
            << "  sizeof (vt):     " << sizeof (vt)           << '\n'
            << "  Inline capacity: " << vt.inline_capacity () << '\n'
            << "  Maximum size:    " << vt.max_size ()        << std::endl;
}
```

Output:

```text
std::allocator<int>:
  sizeof (vs):     64
  Inline capacity: 10
  Maximum size:    4611686018427387903

tiny_allocator<int>:
  sizeof (vt):     64
  Inline capacity: 12
  Maximum size:    16383
```

### How can I use this with my STL container template templates?

You can create a homogeneous template wrapper with something like

```c++
template <typename T, typename Allocator = std::allocator<T>,
          typename InlineCapacityType =
            std::integral_constant<unsigned, default_buffer_size<Allocator>::value>>
using small_vector_ht = small_vector<T, InlineCapacityType::value, Allocator>;

template <template <typename ...> class VectorT>
void
f (void)
{
  VectorT<int> x;
  VectorT<std::string> y;
  /* ... */
}

void
g (void)
{
  f<std::vector> ();
  f<small_vector_ht> ();
}
```

I don't include this in the header because too much choice can often cause confusion about how
things are supposed to be used.

### How do I use this in a `constexpr`?

In C++20, just as with `std::vector`, you cannot create a `constexpr` object of type
`small_vector`. However you can use it *inside* a `constexpr`. That is,

```c++
// Allowed.
constexpr 
int 
f (void)
{
  small_vector<int> v { 1, 2, 3 };
  return std::accumulate (v.begin (), v.end (), 0);
}

// Not allowed.
// constexpr small_vector<int> w { };
```

### How do I disable the `concept`s?

You can define the preprocessor directive `GCH_DISABLE_CONCEPTS` with your compiler. In CMake:

```cmake
target_compile_definitions (<target_name> PRIVATE GCH_DISABLE_CONCEPTS)
```

These are a bit experimental at the moment, so if something is indeed incorrect please feel free to send
me a note to fix it.

### How can I enable the pretty-printer for GDB?

Assuming you have installed the library, you can add the following to either `$HOME/.gdbinit` or
`$(pwd)/.gdbinit`.

```gdb
python
import sys
sys.path.append("/usr/local/share/gch/python")
from gch.gdb.prettyprinters import small_vector
end
```

If you installed the library to another location, simply adjust the path as needed.

As a side-note,
I haven't found a way to automatically install pretty printers for GDB, so if someone knows how to
do so, please let me know.

### Why do I get bad performance compared to other implementations when using this with a throwing move constructor?

This implementation provides the same strong exception guarantees as the STL vector. So, in some
cases copies will be made instead of moves when relocating elements if the move constructor of the
value type may throw.

You can disable the strong exception guarantees by defining
`GCH_NO_STRONG_EXCEPTION_GUARANTEES` before including the header.

## Brief

In the interest of succinctness, this brief is prepared with declaration decorations compatible
with C++20. The `constexpr` and `concept` features will not be available for other versions of the
standard.

Also note that I've omitted the namespacing and template arguments in the `concept`s  used in
most of the `requires` statements. Those arguments involve `value_type`, `small_vector`, and `allocator_type`, in the
obvious fashion.

```c++
namespace gch
{
  namespace concepts
  {
    template <typename T> concept MoveAssignable;
    template <typename T> concept CopyAssignable;
    template <typename T> concept Swappable;

    template <typename T, typename X, typename A, typename ...Args>
    concept EmplaceConstructible;

    template <typename T, typename X, typename A> concept DefaultInsertable;
    template <typename T, typename X, typename A> concept MoveInsertable;
    template <typename T, typename X, typename A> concept CopyInsertable;
    template <typename T, typename X, typename A> concept Erasable;

    template <typename A, typename T> concept AllocatorFor;
    template <typename A> concept Allocator;
  }

  // A class used to calculate the default number of elements in inline storage using a heuristic.
  template <typename Allocator>
  requires concepts::Allocator<Allocator>
  struct default_buffer_size;

  template <typename Allocator>
  inline constexpr
  unsigned
  default_buffer_size_v = default_buffer_size<Allocator>::value;

  // A contiguous iterator (just a pointer wrapper).
  template <typename Pointer, typename DifferenceType>
  class small_vector_iterator;

  template <typename T,
            unsigned InlineCapacity = default_buffer_size_v<std::allocator<T>>,
            typename Allocator      = std::allocator<T>>
  requires concepts::AllocatorFor<Allocator, T>
  class small_vector
  {
  public:
    using value_type             = T;
    using allocator_type         = Allocator;
    using size_type              = typename std::allocator_traits<Allocator>::size_type;
    using difference_type        = /* min { signed size_type, alloc_traits::difference_type } */;
    using reference              =       value_type&;
    using const_reference        = const value_type&;
    using pointer                = typename std::allocator_traits<allocator_type>::pointer;
    using const_pointer          = typename std::allocator_traits<allocator_type>::const_pointer;

    using iterator               = small_vector_iterator<pointer, difference_type>;
    using const_iterator         = small_vector_iterator<const_pointer, difference_type>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /* construction */
    constexpr
    small_vector (void)
      noexcept (noexcept (allocator_type ()))
      requires concepts::DefaultConstructible<allocator_type>;

    constexpr
    small_vector (const small_vector& other)
      requires CopyInsertable && CopyAssignable;

    constexpr
    small_vector (small_vector&& other) 
      noexcept (std::is_nothrow_move_constructible<value_type>::value || 0 == InlineCapacity)
      requires MoveInsertable;

    constexpr explicit
    small_vector (const allocator_type& alloc) noexcept;

    constexpr
    small_vector (const small_vector& other, const allocator_type& alloc)
      requires CopyInsertable;

    constexpr
    small_vector (small_vector&& other, const allocator_type& alloc)
      requires MoveInsertable;

    constexpr explicit
    small_vector (size_type count, const allocator_type& alloc = allocator_type ())
      requires DefaultInsertable;

    constexpr
    small_vector (size_type count, const_reference value,
                  const allocator_type& alloc = allocator_type ())
      requires CopyInsertable;
    
    template <std::copy_constructible Generator>
    requires std::invocable<Generator&>
         &&  EmplaceConstructible<std::invoke_result_t<Generator&>>
    GCH_CPP20_CONSTEXPR
    small_vector (size_type count, Generator g, const allocator_type& alloc = allocator_type ());

    template <std::input_iterator InputIt>
    requires EmplaceConstructible<std::iter_reference_t<InputIt>>
         &&  (std::forward_iterator<InputIt> || MoveInsertable)
    constexpr
    small_vector (InputIt first, InputIt last, const allocator_type& alloc = allocator_type ());

    constexpr
    small_vector (std::initializer_list<value_type> init,
                  const allocator_type& alloc = allocator_type ())
      requires EmplaceConstructible<const_reference>;

    template <unsigned I>
    requires CopyInsertable && CopyAssignable
    constexpr explicit
    small_vector (const small_vector<T, I, Allocator>& other);

    template <unsigned LessI>
    requires (LessI < InlineCapacity) && MoveInsertable
    constexpr explicit
    small_vector (small_vector<T, LessI, Allocator>&& other)
      noexcept (std::is_nothrow_move_constructible<value_type>::value);

    template <unsigned GreaterI>
    requires (InlineCapacity < GreaterI) && MoveInsertable
    constexpr explicit
    small_vector (small_vector<T, GreaterI, Allocator>&& other);

    template <unsigned I>
    requires CopyInsertable
    constexpr
    small_vector (const small_vector<T, I, Allocator>& other, const allocator_type& alloc);

    template <unsigned I>
    requires MoveInsertable
    constexpr
    small_vector (small_vector<T, I, Allocator>&& other, const allocator_type& alloc);

    /* destruction */
    constexpr
    ~small_vector (void)
      requires Erasable;

    /* assignment */
    constexpr
    small_vector&
    operator= (const small_vector& other)
      requires CopyInsertable && CopyAssignable;

    constexpr
    small_vector&
    operator= (small_vector&& other) 
      noexcept (  (  std::is_same<std::allocator<value_type>, Allocator>::value
                 ||  std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
                 ||  std::allocator_traits<Allocator>::is_always_equal::value
                  )
              &&  (  (  std::is_nothrow_move_assignable<value_type>::value
                    &&  std::is_nothrow_move_constructible<value_type>::value
                     )
                 ||  InlineCapacity == 0
                  )
               )
      requires MoveInsertable && MoveAssignable;

    constexpr
    small_vector&
    operator= (std::initializer_list<value_type> ilist)
      requires CopyInsertable && CopyAssignable;

    constexpr
    void
    assign (size_type count, const_reference value)
      requires CopyInsertable && CopyAssignable;

    template <std::input_iterator InputIt>
    requires EmplaceConstructible<std::iter_reference_t<InputIt>>
         &&  (std::forward_iterator<InputIt> || MoveInsertable)
    constexpr
    void
    assign (InputIt first, InputIt last);

    constexpr
    void
    assign (std::initializer_list<value_type> ilist)
      requires EmplaceConstructible<const_reference>;

    template <unsigned I>
    requires CopyInsertable && CopyAssignable
    constexpr
    void
    assign (const small_vector<T, I, Allocator>& other);

    constexpr
    void
    assign (small_vector&& other)
      noexcept (  (  std::is_same<std::allocator<value_type>, Allocator>::value
                 ||  std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
                 ||  std::allocator_traits<Allocator>::is_always_equal::value
                  )
              &&  (  (  std::is_nothrow_move_assignable<value_type>::value
                    &&  std::is_nothrow_move_constructible<value_type>::value
                     )
                 ||  InlineCapacity == 0
                  )
               )
      requires MoveInsertable && MoveAssignable;

    template <unsigned LessI>
    requires (LessI < InlineCapacity) && MoveInsertable && MoveAssignable
    constexpr
    void
    assign (small_vector<T, LessI, Allocator>&& other)
      noexcept (  (  std::is_same<std::allocator<value_type>, Allocator>::value
                 ||  std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
                 ||  std::allocator_traits<Allocator>::is_always_equal::value
                  )
              &&  std::is_nothrow_move_assignable<value_type>::value
              &&  std::is_nothrow_move_constructible<value_type>::value
               );

    template <unsigned GreaterI>
    requires (InlineCapacity < GreaterI) && MoveInsertable && MoveAssignable
    constexpr
    void
    assign (small_vector<T, GreaterI, Allocator>&& other);

    constexpr
    void
    swap (small_vector& other)
      noexcept (  (  std::is_same<std::allocator<value_type>, Allocator>::value
                 ||  std::allocator_traits<Allocator>::propagate_on_container_swap::value
                 ||  std::allocator_traits<Allocator>::is_always_equal::value
                  )
              &&  (  (  std::is_nothrow_move_constructible<value_type>::value
                    &&  std::is_nothrow_move_assignable<value_type>::value
                    &&  std::is_nothrow_swappable<value_type>::value
                     )
                 ||  InlineCapacity == 0
                  )
               )
      requires (MoveInsertable && MoveAssignable && Swappable)
           ||  (  (  std::is_same<std::allocator<value_type>, Allocator>::value
                 ||  std::allocator_traits<Allocator>::propagate_on_container_swap::value
                 ||  std::allocator_traits<Allocator>::is_always_equal::value
                  )
              &&  InlineCapacity == 0
               )

    /* iteration */
    constexpr iterator               begin   (void)       noexcept;
    constexpr const_iterator         begin   (void) const noexcept;
    constexpr const_iterator         cbegin  (void) const noexcept;

    constexpr iterator               end     (void)       noexcept;
    constexpr const_iterator         end     (void) const noexcept;
    constexpr const_iterator         cend    (void) const noexcept;

    constexpr reverse_iterator       rbegin  (void)       noexcept;
    constexpr const_reverse_iterator rbegin  (void) const noexcept;
    constexpr const_reverse_iterator crbegin (void) const noexcept;

    constexpr reverse_iterator       rend    (void)       noexcept;
    constexpr const_reverse_iterator rend    (void) const noexcept;
    constexpr const_reverse_iterator crend   (void) const noexcept;

    /* access */
    constexpr reference       at (size_type pos);
    constexpr const_reference at (size_type pos) const;

    constexpr reference       operator[] (size_type pos);
    constexpr const_reference operator[] (size_type pos) const;

    constexpr reference       front (void);
    constexpr const_reference front (void) const;

    constexpr reference       back  (void);
    constexpr const_reference back  (void) const;

    constexpr pointer         data  (void)       noexcept;
    constexpr const_pointer   data  (void) const noexcept;

    /* state information */
    [[nodiscard]]
    constexpr bool empty (void) const noexcept;

    constexpr size_type      size          (void) const noexcept;
    constexpr size_type      max_size      (void) const noexcept;
    constexpr size_type      capacity      (void) const noexcept;
    constexpr allocator_type get_allocator (void) const noexcept;

    /* insertion */
    constexpr
    iterator
    insert (const_iterator pos, const_reference value)
      requires CopyInsertable && CopyAssignable;

    constexpr
    iterator
    insert (const_iterator pos, value_type&& value)
      requires MoveInsertable && MoveAssignable;

    constexpr
    iterator
    insert (const_iterator pos, size_type count, const_reference value)
      requires CopyInsertable && CopyAssignable;

    template <std::input_iterator InputIt>
    requires EmplaceConstructible<std::iter_reference_t<InputIt>>
         &&  MoveInsertable
         &&  MoveAssignable
    constexpr
    iterator
    insert (const_iterator pos, InputIt first, InputIt last);
    
    constexpr
    iterator
    insert (const_iterator pos, std::initializer_list<value_type> ilist)
      requires EmplaceConstructible<const_reference>
           &&  MoveInsertable
           &&  MoveAssignable;

    template <typename ...Args>
      requires EmplaceConstructible<Args...>
           &&  MoveInsertable
           &&  MoveAssignable
    constexpr
    iterator
    emplace (const_iterator pos, Args&&... args);

    constexpr
    iterator
    erase (const_iterator pos)
      requires MoveAssignable && Erasable;

    constexpr
    iterator
    erase (const_iterator first, const_iterator last)
      requires MoveAssignable && Erasable;

    constexpr
    void
    push_back (const_reference value)
      requires CopyInsertable;

    constexpr
    void
    push_back (value_type&& value)
      requires MoveInsertable;

    template <typename ...Args>
    requires EmplaceConstructible<Args...> && MoveInsertable
    constexpr
    reference
    emplace_back (Args&&... args);

    constexpr
    void
    pop_back (void)
      requires Erasable;

    /* global state modification */
    constexpr
    void
    reserve (size_type new_cap)
      requires MoveInsertable;

    constexpr
    void
    shrink_to_fit (void)
      requires MoveInsertable;

    constexpr
    void
    clear (void) noexcept
      requires Erasable;

    constexpr
    void
    resize (size_type count)
      requires MoveInsertable && DefaultInsertable;

    constexpr
    void
    resize (size_type count, const_reference value)
      requires CopyInsertable;

    /* non-standard */
    [[nodiscard]] constexpr bool      inlined         (void) const noexcept;
    [[nodiscard]] constexpr bool      inlinable       (void) const noexcept;
    [[nodiscard]] constexpr size_type inline_capacity (void) const noexcept;

    template <std::input_iterator InputIt>
    requires EmplaceConstructible<std::iter_reference_t<InputIt>>
         &&  MoveInsertable
    constexpr
    small_vector&
    append (InputIt first, InputIt last);

    constexpr
    small_vector&
    append (std::initializer_list<value_type> ilist)
      requires EmplaceConstructible<const_reference>
           &&  MoveInsertable;

    template <unsigned I>
    constexpr
    small_vector&
    append (const small_vector<T, I, Allocator>& other)
      requires CopyInsertable;

    template <unsigned I>
    constexpr
    small_vector&
    append (small_vector<T, I, Allocator>&& other)
      requires MoveInsertable;
  };

  /* non-member functions */

  template <typename T, unsigned InlineCapacityLHS, unsigned InlineCapacityRHS, typename Allocator>
  constexpr
  bool
  operator== (const small_vector<T, InlineCapacityLHS, Allocator>& lhs,
              const small_vector<T, InlineCapacityRHS, Allocator>& rhs);

  template <typename T, unsigned InlineCapacityLHS, unsigned InlineCapacityRHS, typename Allocator>
  constexpr
  auto
  operator<=> (const small_vector<T, InlineCapacityLHS, Allocator>& lhs,
               const small_vector<T, InlineCapacityRHS, Allocator>& rhs);

  /* insert other comparison boilerplate here if not using C++20 */

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  void
  swap (small_vector<T, InlineCapacity, Allocator>& lhs,
        small_vector<T, InlineCapacity, Allocator>& rhs)
    noexcept (noexcept (lhs.swap (rhs)))
    requires MoveInsertable && Swappable;

  template <typename T, unsigned InlineCapacity, typename Allocator, typename U>
  constexpr
  typename small_vector<T, InlineCapacity, Allocator>::size_type
  erase (small_vector<T, InlineCapacity, Allocator>& c, const U& value);

  template <typename T, unsigned InlineCapacity, typename Allocator, typename Pred>
  constexpr
  typename small_vector<T, InlineCapacity, Allocator>::size_type
  erase_if (small_vector<T, InlineCapacity, Allocator>& c, Pred pred);

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  begin (small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (v.begin ());

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  begin (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (v.begin ());

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  cbegin (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (begin (v));

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  end (small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (v.end ());

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  end (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (v.end ());

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  cend (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (end (v));

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  rbegin (small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (rbegin (v));

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  rbegin (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (rbegin (v));

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  crbegin (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (rbegin (v));

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  rend (small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (v.rend ());

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  rend (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (v.rend ());

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  crend (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (rend (v));

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  size (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (v.size ());

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  ssize (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> std::common_type_t<std::ptrdiff_t, std::make_signed_t<decltype (v.size ())>>;

  template <typename T, unsigned InlineCapacity, typename Allocator>
  [[nodiscard]] constexpr
  auto
  empty (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (v.empty ())

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  data (small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (v.data ())

  template <typename T, unsigned InlineCapacity, typename Allocator>
  constexpr
  auto
  data (const small_vector<T, InlineCapacity, Allocator>& v) noexcept
    -> decltype (v.data ())

  template <typename InputIt,
            unsigned InlineCapacity = default_buffer_size_v<
              std::allocator<typename std::iterator_traits<InputIt>::value_type>>,
            typename Allocator = std::allocator<typename std::iterator_traits<InputIt>::value_type>>
  small_vector (InputIt, InputIt, Allocator = Allocator ())
    -> small_vector<typename std::iterator_traits<InputIt>::value_type, InlineCapacity, Allocator>;
}
```

## Other Implementations

- [boost::container::small_vector](https://www.boost.org/doc/libs/1_60_0/doc/html/boost/container/small_vector.html)
- [llvm::SmallVector](https://llvm.org/doxygen/classllvm_1_1SmallVector.html)
- [folly::small_vector](https://github.com/facebook/folly/blob/main/folly/docs/small_vector.md)

## License

This project may be modified and distributed under the terms of the MIT license. See the [LICENSE](LICENSE)
file for details.
