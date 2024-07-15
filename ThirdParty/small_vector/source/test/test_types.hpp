/** test_types.hpp
 * Short description here.
 *
 * Copyright © 2021 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef GCH_SMALL_VECTOR_TEST_TYPES_HPP
#define GCH_SMALL_VECTOR_TEST_TYPES_HPP

#include "test_common.hpp"
#include "gch/small_vector.hpp"

#include <iostream>
#include <memory>
#include <stack>

namespace gch
{
  namespace test_types
  {
    template <typename Iter>
    constexpr
    std::reverse_iterator<Iter>
    make_reverse_it (Iter i)
    {
      return std::reverse_iterator<Iter>(i);
    }
    
    template <typename T>
    struct pointer_wrapper
    {
      using difference_type = std::ptrdiff_t;
      using value_type = T;
      using pointer = T *;
      using reference = T&;
      using iterator_category = std::random_access_iterator_tag;
#ifdef GCH_LIB_CONCEPTS
      using iterator_concept = std::contiguous_iterator_tag;
#endif

      pointer_wrapper            (void)                       = default;
      pointer_wrapper            (const pointer_wrapper&)     = default;
      pointer_wrapper            (pointer_wrapper&&) noexcept = default;
      pointer_wrapper& operator= (const pointer_wrapper&)     = default;
      pointer_wrapper& operator= (pointer_wrapper&&) noexcept = default;
      ~pointer_wrapper           (void)                       = default;

      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (pointer p) noexcept
        : m_ptr (p)
      { }

      template <typename U,
        typename std::enable_if<std::is_convertible<U *, pointer>::value>::type* = nullptr>
      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (const pointer_wrapper<U>& other) noexcept
        : m_ptr (other.base ()) { }

      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (std::nullptr_t) noexcept
        : m_ptr (nullptr) { }

      template <typename U = T,
        typename std::enable_if<std::is_const<U>::value, bool>::type = true>
      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (const void *p) noexcept
        : m_ptr (static_cast<pointer> (p)) { }

      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (void *p) noexcept
        : m_ptr (static_cast<pointer> (p)) { }

      GCH_IMPLICIT_CONVERSION
      operator typename std::conditional<std::is_const<T>::value,
                                         const T *,
                                         pointer>::type (void)
      {
        return m_ptr;
      }

      GCH_CPP14_CONSTEXPR
      pointer_wrapper&
      operator++ (void) noexcept
      {
        ++m_ptr;
        return *this;
      }

      GCH_CPP14_CONSTEXPR
      pointer_wrapper
      operator++ (int) noexcept
      {
        return pointer_wrapper (m_ptr++);
      }

      GCH_CPP14_CONSTEXPR
      pointer_wrapper&
      operator-- (void) noexcept
      {
        --m_ptr;
        return *this;
      }

      GCH_CPP14_CONSTEXPR
      pointer_wrapper
      operator-- (int) noexcept
      {
        return pointer_wrapper (m_ptr--);
      }

      GCH_CPP14_CONSTEXPR
      pointer_wrapper&
      operator+= (difference_type n) noexcept
      {
        m_ptr += n;
        return *this;
      }

      GCH_NODISCARD
      constexpr
      pointer_wrapper
      operator+ (difference_type n) const noexcept
      {
        return pointer_wrapper (m_ptr + n);
      }

      GCH_CPP14_CONSTEXPR
      pointer_wrapper&
      operator-= (difference_type n) noexcept
      {
        m_ptr -= n;
        return *this;
      }

      GCH_NODISCARD
      constexpr
      pointer_wrapper
      operator- (difference_type n) const noexcept
      {
        return pointer_wrapper (m_ptr - n);
      }

      GCH_NODISCARD
      constexpr
      reference
      operator* (void) const noexcept
      {
        return *m_ptr;
      }

      GCH_NODISCARD
      constexpr
      pointer
      operator-> (void) const noexcept
      {
        return m_ptr;
      }

      GCH_NODISCARD
      constexpr
      reference
      operator[] (difference_type n) const noexcept
      {
        return m_ptr[n];
      }

      GCH_NODISCARD
      constexpr
      pointer
      base (void) const noexcept
      {
        return m_ptr;
      }

    private:
      pointer m_ptr;
    };

    template <>
    struct pointer_wrapper<void>
    {
      using pointer = void *;

      pointer_wrapper            (void)                       = default;
      pointer_wrapper            (const pointer_wrapper&)     = default;
      pointer_wrapper            (pointer_wrapper&&) noexcept = default;
      pointer_wrapper& operator= (const pointer_wrapper&)     = default;
      pointer_wrapper& operator= (pointer_wrapper&&) noexcept = default;
      ~pointer_wrapper           (void)                       = default;

      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (pointer p) noexcept
        : m_ptr (p)
      { }

      template <typename U,
        typename std::enable_if<std::is_convertible<U *, pointer>::value>::type* = nullptr>
      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (const pointer_wrapper<U>& other) noexcept
        : m_ptr (other.base ())
      { }

      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (std::nullptr_t) noexcept
        : m_ptr (nullptr)
      { }

      GCH_IMPLICIT_CONVERSION
      operator pointer (void)
      {
        return m_ptr;
      }

      GCH_NODISCARD
      constexpr
      pointer
      operator-> (void) const noexcept
      {
        return m_ptr;
      }

      GCH_NODISCARD
      constexpr
      pointer
      base (void) const noexcept
      {
        return m_ptr;
      }

    private:
      pointer m_ptr;
    };

    template <>
    struct pointer_wrapper<const void>
    {
      using pointer = const void *;

      pointer_wrapper            (void)                       = default;
      pointer_wrapper            (const pointer_wrapper&)     = default;
      pointer_wrapper            (pointer_wrapper&&) noexcept = default;
      pointer_wrapper& operator= (const pointer_wrapper&)     = default;
      pointer_wrapper& operator= (pointer_wrapper&&) noexcept = default;
      ~pointer_wrapper           (void)                       = default;

      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (pointer p) noexcept
        : m_ptr (p)
      { }

      template <typename U,
        typename std::enable_if<std::is_convertible<U *, pointer>::value>::type* = nullptr>
      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (const pointer_wrapper<U>& other) noexcept
        : m_ptr (other.base ())
      { }

      constexpr GCH_IMPLICIT_CONVERSION
      pointer_wrapper (std::nullptr_t) noexcept
        : m_ptr (nullptr)
      { }

      GCH_IMPLICIT_CONVERSION
      operator pointer (void)
      {
        return m_ptr;
      }

      GCH_NODISCARD
      constexpr
      pointer
      operator-> (void) const noexcept
      {
        return m_ptr;
      }

      GCH_NODISCARD
      constexpr
      pointer
      base (void) const noexcept
      {
        return m_ptr;
      }

    private:
      pointer m_ptr;
    };

#ifdef GCH_LIB_THREE_WAY_COMPARISON

    // template <typename PointerLHS, typename PointerRHS>
    // constexpr
    // bool
    // operator== (const pointer_wrapper<PointerLHS>& lhs,
    //             const pointer_wrapper<PointerRHS>& rhs)
    // noexcept (noexcept (lhs.base () == rhs.base ()))
    // requires requires { { lhs.base () == rhs.base () } -> std::convertible_to<bool>; }
    // {
    //   return lhs.base () == rhs.base ();
    // }

    template <typename PointerLHS, typename PointerRHS>
    constexpr
    bool
    operator== (const pointer_wrapper<PointerLHS>& lhs,
                const pointer_wrapper<PointerRHS>& rhs) noexcept
    {
      return lhs.base () == rhs.base ();
    }

    template <typename PointerLHS, typename PointerRHS>
    constexpr
    auto
    operator<=> (const pointer_wrapper<PointerLHS>& lhs,
                 const pointer_wrapper<PointerRHS>& rhs)
    noexcept (noexcept (lhs.base () <=> rhs.base ()))
    requires std::three_way_comparable_with<PointerLHS, PointerRHS>
    {
      return lhs.base () <=> rhs.base ();
    }

    template <typename PointerLHS, typename PointerRHS>
    constexpr
    auto
    operator<=> (const pointer_wrapper<PointerLHS>& lhs,
                 const pointer_wrapper<PointerRHS>& rhs)
    noexcept (noexcept (lhs.base () < rhs.base ()) && noexcept (rhs.base () < lhs.base ()))
    requires (!std::three_way_comparable_with<PointerLHS, PointerRHS>)
    {
      return (lhs.base () < rhs.base ()) ? std::weak_ordering::less
                                         : (rhs.base () < lhs.base ()) ? std::weak_ordering::greater
                                                                       : std::weak_ordering::equivalent;
    }

#else

    template <typename PointerLHS, typename PointerRHS>
    constexpr
    bool
    operator== (const pointer_wrapper<PointerLHS>& lhs,
                const pointer_wrapper<PointerRHS>& rhs) noexcept
    {
      return lhs.base () == rhs.base ();
    }

    template <typename Pointer>
    constexpr
    bool
    operator== (const pointer_wrapper<Pointer>& lhs,
                const pointer_wrapper<Pointer>& rhs) noexcept
    {
      return lhs.base () == rhs.base ();
    }

    template <typename PointerLHS, typename PointerRHS>
    constexpr
    bool
    operator!= (const pointer_wrapper<PointerLHS>& lhs,
                const pointer_wrapper<PointerRHS>& rhs) noexcept
    {
      return lhs.base () != rhs.base ();
    }

    template <typename Pointer>
    constexpr
    bool
    operator!= (const pointer_wrapper<Pointer>& lhs,
                const pointer_wrapper<Pointer>& rhs) noexcept
    {
      return lhs.base () != rhs.base ();
    }

    template <typename PointerLHS, typename PointerRHS>
    constexpr
    bool
    operator< (const pointer_wrapper<PointerLHS>& lhs,
               const pointer_wrapper<PointerRHS>& rhs) noexcept
    {
      return lhs.base () < rhs.base ();
    }

    template <typename Pointer>
    constexpr
    bool
    operator< (const pointer_wrapper<Pointer>& lhs,
               const pointer_wrapper<Pointer>& rhs) noexcept
    {
      return lhs.base () < rhs.base ();
    }

    template <typename PointerLHS, typename PointerRHS>
    constexpr
    bool
    operator> (const pointer_wrapper<PointerLHS>& lhs,
               const pointer_wrapper<PointerRHS>& rhs) noexcept
    {
      return lhs.base () > rhs.base ();
    }

    template <typename Pointer>
    constexpr
    bool
    operator> (const pointer_wrapper<Pointer>& lhs,
               const pointer_wrapper<Pointer>& rhs) noexcept
    {
      return lhs.base () > rhs.base ();
    }

    template <typename PointerLHS, typename PointerRHS>
    constexpr
    bool
    operator<= (const pointer_wrapper<PointerLHS>& lhs,
                const pointer_wrapper<PointerRHS>& rhs) noexcept
    {
      return lhs.base () <= rhs.base ();
    }

    template <typename Pointer>
    constexpr
    bool
    operator<= (const pointer_wrapper<Pointer>& lhs,
                const pointer_wrapper<Pointer>& rhs) noexcept
    {
      return lhs.base () <= rhs.base ();
    }

    template <typename PointerLHS, typename PointerRHS>
    constexpr
    bool
    operator>= (const pointer_wrapper<PointerLHS>& lhs,
                const pointer_wrapper<PointerRHS>& rhs) noexcept
    {
      return lhs.base () >= rhs.base ();
    }

    template <typename Pointer>
    constexpr
    bool
    operator>= (const pointer_wrapper<Pointer>& lhs,
                const pointer_wrapper<Pointer>& rhs) noexcept
    {
      return lhs.base () >= rhs.base ();
    }

#endif

    template <typename PointerLHS, typename PointerRHS>
    constexpr
    auto
    operator- (const pointer_wrapper<PointerLHS>& lhs,
               const pointer_wrapper<PointerRHS>& rhs) noexcept
    -> decltype (lhs.base () - rhs.base ())
    {
      return lhs.base () - rhs.base ();
    }

    template <typename Pointer>
    constexpr
    auto
    operator- (const pointer_wrapper<Pointer>& lhs,
               const pointer_wrapper<Pointer>& rhs) noexcept
    -> decltype (lhs.base () - rhs.base ())
    {
      return lhs.base () - rhs.base ();
    }

    template <typename Pointer>
    constexpr
    pointer_wrapper<Pointer>
    operator+ (typename pointer_wrapper<Pointer>::difference_type n,
               const pointer_wrapper<Pointer>& it) noexcept
    {
      return it + n;
    }

    template <typename T>
    GCH_NODISCARD
    constexpr
    bool
    operator== (std::nullptr_t, const pointer_wrapper<T>& rhs) noexcept
    {
      return nullptr == rhs.base ();
    }

    template <typename T>
    GCH_NODISCARD
    constexpr
    bool
    operator!= (std::nullptr_t, const pointer_wrapper<T>& rhs) noexcept
    {
      return nullptr != rhs.base ();
    }

    template <typename T>
    GCH_NODISCARD
    constexpr
    bool
    operator== (const pointer_wrapper<T>& lhs, std::nullptr_t) noexcept
    {
      return lhs.base () == nullptr;
    }

    template <typename T>
    GCH_NODISCARD
    constexpr
    bool
    operator!= (const pointer_wrapper<T>& lhs, std::nullptr_t) noexcept
    {
      return lhs.base () != nullptr;
    }
  }
}

namespace std
{

  template <typename T>
  struct pointer_traits<gch::test_types::pointer_wrapper<T>>
  {
    using pointer = gch::test_types::pointer_wrapper<T>;
    using element_type = T;
    using difference_type = typename pointer::difference_type;

    template <typename U>
    using rebind = gch::test_types::pointer_wrapper<U>;

    static constexpr pointer pointer_to (element_type& r) noexcept
    {
      return pointer (std::addressof (r));
    }

    static constexpr
    T *
    to_address (pointer p) noexcept
    {
      return p.base ();
    }
  };

}

namespace gch
{

  template <typename T, typename Allocator>
  using small_vector_with_allocator =
    small_vector<T, default_buffer_size<Allocator>::value, Allocator>;

  template <typename T, template <typename ...> class AllocatorT, typename ...AArgs>
  using small_vector_with_allocator_tp = small_vector_with_allocator<T, AllocatorT<T, AArgs...>>;

  namespace test_types
  {

    template <typename Iterator>
    class single_pass_iterator
    {
    public:
      using difference_type   = typename std::iterator_traits<Iterator>::difference_type;
      using pointer           = Iterator;
      using reference         = typename std::iterator_traits<Iterator>::reference;
      using value_type        = typename std::iterator_traits<Iterator>::value_type;
      using iterator_category = std::input_iterator_tag;

      single_pass_iterator            (void)                            = default;
      single_pass_iterator            (const single_pass_iterator&)     = default;
      single_pass_iterator            (single_pass_iterator&&) noexcept = default;
      single_pass_iterator& operator= (const single_pass_iterator&)     = default;
      single_pass_iterator& operator= (single_pass_iterator&&) noexcept = default;
      ~single_pass_iterator           (void)                            = default;

      GCH_SMALL_VECTOR_TEST_CONSTEXPR explicit
      single_pass_iterator (Iterator p) noexcept
        : m_ptr (p)
      { }

      // LegacyIterator
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      single_pass_iterator&
      operator++ (void) noexcept
      {
        ++get_ptr ();
        return *this;
      }

      // EqualityComparable
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      bool
      operator== (const single_pass_iterator& other) const noexcept
      {
        return get_ptr () == other.get_ptr ();
      }

      // LegacyInputIterator
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      bool
      operator!= (const single_pass_iterator& other) const noexcept
      {
        return ! operator== (other);
      }

      // LegacyInputIterator
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      reference
      operator* (void) const noexcept
      {
        return *get_ptr ();
      }

      // LegacyInputIterator
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      pointer
      operator-> (void) const noexcept
      {
        return get_ptr ();
      }

      // LegacyInputIterator
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      single_pass_iterator
      operator++ (int) noexcept
      {
        return single_pass_iterator (get_ptr ()++, true);
      }

    private:
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      single_pass_iterator (pointer p, bool is_invalid) noexcept
        : m_ptr (p),
          m_is_invalid (is_invalid)
      { }

      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      pointer&
      get_ptr (void) noexcept
      {
        CHECK (! m_is_invalid && "The iterator has already been invalidated.");
        return m_ptr;
      }

      GCH_NODISCARD GCH_SMALL_VECTOR_TEST_CONSTEXPR
      const pointer&
      get_ptr (void) const noexcept
      {
        return const_cast<single_pass_iterator&> (*this).get_ptr ();
      }

      Iterator m_ptr { };
      bool    m_is_invalid = false;
    };

#ifdef GCH_LIB_CONCEPTS
    static_assert (std::input_iterator<single_pass_iterator<int *>>, "not input iterator.");
    static_assert (! std::forward_iterator<single_pass_iterator<int *>>, "forward iterator.");
#endif

    template <typename Iterator>
    GCH_SMALL_VECTOR_TEST_CONSTEXPR
    single_pass_iterator<Iterator>
    make_input_it (Iterator it) noexcept
    {
      return single_pass_iterator<Iterator> (it);
    }

    template <typename Iterator>
    class multi_pass_iterator
    {
    public:
      using difference_type   = typename std::iterator_traits<Iterator>::difference_type;
      using pointer           = Iterator;
      using reference         = typename std::iterator_traits<Iterator>::reference;
      using value_type        = typename std::iterator_traits<Iterator>::value_type;
      using iterator_category = std::forward_iterator_tag;

      multi_pass_iterator            (void)                           = default;
      multi_pass_iterator            (const multi_pass_iterator&)     = default;
      multi_pass_iterator            (multi_pass_iterator&&) noexcept = default;
      multi_pass_iterator& operator= (const multi_pass_iterator&)     = default;
      multi_pass_iterator& operator= (multi_pass_iterator&&) noexcept = default;
      ~multi_pass_iterator           (void)                           = default;

      GCH_SMALL_VECTOR_TEST_CONSTEXPR explicit
      multi_pass_iterator (Iterator p) noexcept
        : m_ptr (p)
      { }

      // LegacyIterator
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      multi_pass_iterator&
      operator++ (void) noexcept
      {
        ++m_ptr;
        return *this;
      }

      // EqualityComparable
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      bool
      operator== (const multi_pass_iterator& other) const noexcept
      {
        return m_ptr == other.m_ptr;
      }

      // LegacyInputIterator
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      bool
      operator!= (const multi_pass_iterator& other) const noexcept
      {
        return ! operator== (other);
      }

      // LegacyInputIterator
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      reference
      operator* (void) const noexcept
      {
        return *m_ptr;
      }

      // LegacyInputIterator
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      pointer
      operator-> (void) const noexcept
      {
        return m_ptr;
      }

      // LegacyInputIterator
      GCH_SMALL_VECTOR_TEST_CONSTEXPR
      multi_pass_iterator
      operator++ (int) noexcept
      {
        return multi_pass_iterator (m_ptr++, true);
      }

    private:
      Iterator m_ptr { };
    };

#ifdef GCH_LIB_CONCEPTS
    static_assert (std::forward_iterator<multi_pass_iterator<int *>>, "Not a forward iterator.");
    static_assert (! std::random_access_iterator<multi_pass_iterator<int *>>, "RAI iterator.");
#endif

    template <typename Iterator>
    GCH_SMALL_VECTOR_TEST_CONSTEXPR
    multi_pass_iterator<Iterator>
    make_fwd_it (Iterator it) noexcept
    {
      return multi_pass_iterator<Iterator> (it);
    }

    struct trivially_copyable_data_base
    {

#ifdef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
      constexpr
      trivially_copyable_data_base (void) noexcept
        : data { }
      { }
#else
      trivially_copyable_data_base (void) = default;
#endif

      constexpr
      trivially_copyable_data_base (int x) noexcept
        : data (x)
      { }

      int data;
    };

    constexpr
    bool
    operator== (const trivially_copyable_data_base& lhs, const trivially_copyable_data_base& rhs)
    {
      return lhs.data == rhs.data;
    }

    constexpr
    bool
    operator!= (const trivially_copyable_data_base& lhs, const trivially_copyable_data_base& rhs)
    {
      return ! (lhs == rhs);
    }

    constexpr
    bool
    operator< (const trivially_copyable_data_base& lhs, const trivially_copyable_data_base& rhs)
    {
      return lhs.data < rhs.data;
    }

#ifdef GCH_SMALL_VECTOR_TEST_HAS_CONSTEXPR
    static_assert (
      ! std::is_trivially_default_constructible<trivially_copyable_data_base>::value,
      "Unexpectedly trivially default constructible.");
#else
    static_assert (
      std::is_trivial<trivially_copyable_data_base>::value,
      "Unexpectedly not trivial.");

    static_assert (
      std::is_trivially_default_constructible<trivially_copyable_data_base>::value,
      "Unexpectedly not trivially default constructible.");
#endif

    static_assert (
      std::is_trivially_copyable<trivially_copyable_data_base>::value,
      "Unexpectedly not trivially copyable.");

    struct nontrivial_data_base
      : trivially_copyable_data_base
    {
      using trivially_copyable_data_base::trivially_copyable_data_base;

      constexpr
      nontrivial_data_base (void) noexcept
        : trivially_copyable_data_base (0)
      { }

      constexpr
      nontrivial_data_base (const nontrivial_data_base& other) noexcept
        : trivially_copyable_data_base (other)
      { }

      nontrivial_data_base& operator= (const nontrivial_data_base& other) = default;

      GCH_CPP20_CONSTEXPR
      ~nontrivial_data_base (void) noexcept
      {
        // Overwrite data to catch uninitialized errors.
        data = 0x7AFE;
      }
    };

    static_assert (
      ! std::is_trivially_default_constructible<nontrivial_data_base>::value ,
      "Unexpectedly trivially default constructible.");

    static_assert (
      ! std::is_trivially_copyable<nontrivial_data_base>::value,
      "Unexpectedly trivially copyable.");

    struct test_exception
      : std::exception
    {
      const char *
      what (void) const noexcept override
      {
        return "test exception";
      }
    };

#  define EXPECT_TEST_EXCEPTION(...)               \
GCH_TRY                                            \
{                                                  \
  EXPECT_THROW (__VA_ARGS__);                      \
}                                                  \
GCH_CATCH (const gch::test_types::test_exception&) \
{ } (void)0

    class exception_trigger
    {
      static
      exception_trigger&
      get (void) noexcept
      {
        static exception_trigger trigger;
        return trigger;
      }

    public:
      static
      void
      test (void)
      {
        exception_trigger& trigger = get ();

        if (trigger.m_stack.empty ())
        {
          ++trigger.m_count;
          return;
        }

        if (0 == trigger.m_stack.top ()--)
        {
          trigger.m_stack.pop ();
#ifdef GCH_EXCEPTIONS
          throw test_exception { };
#endif
        }
      }

      static
      void
      push (std::size_t n)
      {
        exception_trigger& trigger = get ();
        trigger.m_count = 0;
        trigger.m_stack.push (n);
      }

      static
      void
      reset (void) noexcept
      {
        get () = { };
      }

      static
      std::size_t
      extra_test_count (void) noexcept
      {
        return get ().m_count;
      }

      static
      bool
      has_pending_throws (void) noexcept
      {
        return ! get ().m_stack.empty ();
      }

    private:
      std::size_t             m_count;
      std::stack<std::size_t> m_stack;
    };

    struct triggering_base
      : trivially_copyable_data_base
    {
      using trivially_copyable_data_base::trivially_copyable_data_base;
    };

    struct triggering_copy_ctor
      : triggering_base
    {
      triggering_copy_ctor            (void)                            = default;
//    triggering_copy_ctor            (const triggering_copy_ctor&)     = impl;
      triggering_copy_ctor& operator= (const triggering_copy_ctor&)     = default;
      ~triggering_copy_ctor           (void)                            = default;

      triggering_copy_ctor (const triggering_copy_ctor& other)
        : triggering_base (other)
      {
        exception_trigger::test ();
      }

      using triggering_base::triggering_base;
    };

    struct triggering_copy
      : triggering_copy_ctor
    {
      triggering_copy            (void)                       = default;
      triggering_copy            (const triggering_copy&)     = default;
      triggering_copy            (triggering_copy&&)          = default;
//    triggering_copy& operator= (const triggering_copy&)     = impl;
      triggering_copy& operator= (triggering_copy&&) noexcept = default;
      ~triggering_copy           (void)                       = default;

      triggering_copy& operator= (const triggering_copy& other)
      {
        if (&other != this)
          triggering_copy_ctor::operator= (other);
        exception_trigger::test ();
        return *this;
      }

      using triggering_copy_ctor::triggering_copy_ctor;
    };

    struct triggering_move_ctor
      : triggering_base
    {
      triggering_move_ctor            (void)                            = default;
      triggering_move_ctor            (const triggering_move_ctor&)     = default;
//    triggering_move_ctor            (triggering_move_ctor&&) noexcept = impl;
      triggering_move_ctor& operator= (const triggering_move_ctor&)     = default;
      triggering_move_ctor& operator= (triggering_move_ctor&&) noexcept = default;
      ~triggering_move_ctor           (void)                            = default;

      triggering_move_ctor (triggering_move_ctor&& other) noexcept (false)
        : triggering_base ()
      {
        exception_trigger::test ();
        data = other.data;
        other.is_moved = true;
      }

      using triggering_base::triggering_base;

      bool is_moved = false;
    };

    struct triggering_move
      : triggering_move_ctor
    {
      triggering_move            (void)                       = default;
      triggering_move            (const triggering_move&)     = default;
      triggering_move            (triggering_move&&)          = default;
      triggering_move& operator= (const triggering_move&)     = default;
//    triggering_move& operator= (triggering_move&&) noexcept = impl;
      ~triggering_move           (void)                       = default;

      triggering_move& operator= (triggering_move&& other) noexcept (false)
      {
        exception_trigger::test ();
        triggering_move_ctor::operator= (std::move (other));
        is_moved = false;
        other.is_moved = true;
        return *this;
      }

      using triggering_move_ctor::triggering_move_ctor;
    };


    struct triggering_ctor
      : triggering_base
    {
      triggering_ctor& operator= (const triggering_ctor&)     = default;
      triggering_ctor& operator= (triggering_ctor&&) noexcept = default;

      triggering_ctor (const triggering_ctor& other)
        : triggering_base (other)
      {
        exception_trigger::test ();
      }

      triggering_ctor (triggering_ctor&& other) noexcept (false)
        : triggering_base (std::move (other))
      {
        // Some irrelevant code to quiet compiler warnings.
        delete new int;
      }

      using triggering_base::triggering_base;
    };

    struct triggering_copy_and_move
      : triggering_base
    {
      triggering_copy_and_move (void)                                           = default;
//    triggering_copy_and_move            (const triggering_copy_and_move&)     = impl;
//    triggering_copy_and_move            (triggering_copy_and_move&&)          = impl;
//    triggering_copy_and_move& operator= (const triggering_copy_and_move&)     = impl;
//    triggering_copy_and_move& operator= (triggering_copy_and_move&&) noexcept = impl;
      ~triggering_copy_and_move (void)                                          = default;

      triggering_copy_and_move (int i) noexcept
        : triggering_base (i)
      { }

      triggering_copy_and_move (const triggering_copy_and_move& other) noexcept (false)
        : triggering_base ()
      {
        assert (! other.is_moved);
        exception_trigger::test ();
        data = other.data;
      }

      triggering_copy_and_move (triggering_copy_and_move&& other) noexcept (false)
        : triggering_base ()
      {
        assert (! other.is_moved);
        exception_trigger::test ();
        data = other.data;
        other.is_moved = true;
      }

      triggering_copy_and_move& operator= (const triggering_copy_and_move& other) noexcept (false)
      {
        assert (! other.is_moved);
        exception_trigger::test ();
        data = other.data;
        is_moved = false;
        return *this;
      }

      triggering_copy_and_move& operator= (triggering_copy_and_move&& other) noexcept (false)
      {
        assert (! other.is_moved);
        exception_trigger::test ();
        data = other.data;
        is_moved = false;
        other.is_moved = true;
        return *this;
      }

      using triggering_base::triggering_base;

      bool is_moved = false;
    };

    struct triggering_type
      : triggering_copy_and_move
    {
      triggering_type (void) noexcept (false)
        : triggering_copy_and_move ()
      {
        exception_trigger::test ();
      }

      triggering_type (int i) noexcept (false)
        : triggering_copy_and_move ()
      {
        exception_trigger::test ();
        data = i;
      }

      using triggering_copy_and_move::triggering_copy_and_move;
    };

    struct trivial
    {
      int data;
    };

    static_assert (std::is_trivial<trivial>::value, "not trivial");

    using trivial_array = trivial[5];

    static_assert (std::is_trivial<trivial_array>::value, "not trivial");

    struct trivially_copyable
    {
      trivially_copyable            (void)                          = delete;
      trivially_copyable            (const trivially_copyable&)     = default;
      trivially_copyable            (trivially_copyable&&) noexcept = default;
      trivially_copyable& operator= (const trivially_copyable&)     = default;
      trivially_copyable& operator= (trivially_copyable&&) noexcept = default;
      ~trivially_copyable           (void)                          = default;
      int data;
    };

    static_assert (std::is_trivially_copyable<trivially_copyable>::value,
                   "not trivially copyable");

    using trivially_copyable_array = trivially_copyable[5];
    static_assert (std::is_trivially_copyable<trivially_copyable_array>::value,
                   "not trivially copyable");

    struct trivially_copyable_copy_ctor
    {
      trivially_copyable_copy_ctor            (void)                                    = delete;
      trivially_copyable_copy_ctor            (const trivially_copyable_copy_ctor&)     = default;
      trivially_copyable_copy_ctor            (trivially_copyable_copy_ctor&&) noexcept = delete;
      trivially_copyable_copy_ctor& operator= (const trivially_copyable_copy_ctor&)     = delete;
      trivially_copyable_copy_ctor& operator= (trivially_copyable_copy_ctor&&) noexcept = delete;
      ~trivially_copyable_copy_ctor           (void)                                    = default;
      int data;
    };

    static_assert (std::is_trivially_copyable<trivially_copyable_copy_ctor>::value,
                   "not trivially copyable");

    struct trivially_copyable_move_ctor
    {
      trivially_copyable_move_ctor            (void)                                    = delete;
      trivially_copyable_move_ctor            (const trivially_copyable_move_ctor&)     = delete;
      trivially_copyable_move_ctor            (trivially_copyable_move_ctor&&) noexcept = default;
      trivially_copyable_move_ctor& operator= (const trivially_copyable_move_ctor&)     = delete;
      trivially_copyable_move_ctor& operator= (trivially_copyable_move_ctor&&) noexcept = delete;
      ~trivially_copyable_move_ctor           (void)                                    = default;
      int data;
    };

    static_assert (std::is_trivially_copyable<trivially_copyable_move_ctor>::value,
                   "not trivially copyable");

    struct trivially_copyable_copy_assign
    {
      trivially_copyable_copy_assign            (void)                                      = delete;
      trivially_copyable_copy_assign            (const trivially_copyable_copy_assign&)     = delete;
      trivially_copyable_copy_assign            (trivially_copyable_copy_assign&&) noexcept = delete;
      trivially_copyable_copy_assign& operator= (const trivially_copyable_copy_assign&)     = default;
      trivially_copyable_copy_assign& operator= (trivially_copyable_copy_assign&&) noexcept = delete;
      ~trivially_copyable_copy_assign           (void)                                      = default;
      int data;
    };

    static_assert (std::is_trivially_copyable<trivially_copyable_copy_assign>::value,
                   "not trivially copyable");

    struct trivially_copyable_move_assign
    {
      trivially_copyable_move_assign            (void)                                      = delete;
      trivially_copyable_move_assign            (const trivially_copyable_move_assign&)     = delete;
      trivially_copyable_move_assign            (trivially_copyable_move_assign&&) noexcept = delete;
      trivially_copyable_move_assign& operator= (const trivially_copyable_move_assign&)     = delete;
      trivially_copyable_move_assign& operator= (trivially_copyable_move_assign&&) noexcept = default;
      ~trivially_copyable_move_assign           (void)                                      = default;
      int data;
    };

    static_assert (std::is_trivially_copyable<trivially_copyable_move_assign>::value,
                   "not trivially copyable");

    struct uncopyable
    {
      uncopyable            (void)                  = default;
      uncopyable            (const uncopyable&)     = delete;
      uncopyable            (uncopyable&&) noexcept = delete;
      uncopyable& operator= (const uncopyable&)     = delete;
      uncopyable& operator= (uncopyable&&) noexcept = delete;
      ~uncopyable           (void)                  = default;

      uncopyable (int x)
        : data (x) { }

      int data;
    };

    class non_trivial
    {
    public:
      constexpr
      non_trivial (void) noexcept
        : data (7)
      { }

      constexpr
      non_trivial (int x) noexcept
        : data (x)
      { }

      constexpr
      non_trivial (const non_trivial& other) noexcept
        : data (other.data)
      { }

      GCH_CPP14_CONSTEXPR
      non_trivial&
      operator= (const non_trivial& other) noexcept
      {
#ifdef GCH_LIB_IS_CONSTANT_EVALUATED
        if (std::is_constant_evaluated ())
        {
          data = other.data;
          return *this;
        }
#endif
        if (&other != this)
          data = other.data;
        return *this;
      }

      constexpr
      operator int (void) const noexcept
      {
        return data;
      }

      friend
      std::ostream&
      operator<< (std::ostream& o, const non_trivial& x)
      {
        return o << x.data;
      }

    private:
      int data;
    };

  }

}

#endif // GCH_SMALL_VECTOR_TEST_TYPES_HPP
