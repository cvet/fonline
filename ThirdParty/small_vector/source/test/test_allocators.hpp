/** test-allocators.hpp
 * Copyright © 2022 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef SMALL_VECTOR_TEST_ALLOCATORS_HPP
#define SMALL_VECTOR_TEST_ALLOCATORS_HPP

#include "test_common.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace gch
{

  namespace test_types
  {

    template <typename ...>
    using void_t = void;

    template <typename, typename = void>
    struct has_ptr_traits_to_address
      : std::false_type
    { };

    template <typename P>
    struct has_ptr_traits_to_address<P,
          void_t<decltype (std::pointer_traits<P>::to_address (std::declval<P> ()))>>
      : std::true_type
    { };

    template <typename T>
    constexpr
    T *
    to_address (T *p) noexcept
    {
      static_assert (! std::is_function<T>::value, "value_ty is a function pointer.");
      return p;
    }

    template <typename Pointer,
      typename std::enable_if<has_ptr_traits_to_address<Pointer>::value>::type * = nullptr>
    constexpr
    auto
    to_address (const Pointer& p) noexcept
    -> decltype (std::pointer_traits<Pointer>::to_address (p))
    {
      return std::pointer_traits<Pointer>::to_address (p);
    }

    template <typename Pointer,
      typename std::enable_if<! has_ptr_traits_to_address<Pointer>::value>::type * = nullptr>
    constexpr
    auto
    to_address (const Pointer& p) noexcept
    -> decltype (gch::test_types::to_address (p.operator-> ()))
    {
      return gch::test_types::to_address (p.operator-> ());
    }

    template <typename T>
    struct allocator_difference_type_trait
    {
      using difference_type = T;
    };

    template <typename T>
    struct allocator_size_type_trait
    {
      using size_type = T;
    };

    template <typename T>
    struct allocator_pointer_trait
    {
      using pointer = T;
    };

    template <typename T, typename PartialTraits = std::allocator_traits<std::allocator<T>>>
    class base_allocator
    {
      template <typename U, typename Traits, typename Enable = void>
      struct rebind_traits
      {
        using type = Traits;
      };

      template <typename U, typename Traits>
      struct rebind_traits<
        U,
        Traits,
        void_t<typename Traits::template rebind_traits<U>>>
      {
        using type = typename Traits::template rebind_traits<U>;
      };

      using traits = typename rebind_traits<T, PartialTraits>::type;

      template <typename Traits, typename Enable = void>
      struct extracted_difference_type
      {
        using type = typename std::allocator_traits<std::allocator<T>>::difference_type;
      };

      template <typename Traits>
      struct extracted_difference_type<Traits, void_t<typename Traits::difference_type>>
      {
        using type = typename Traits::difference_type;
      };

      template <typename Traits, typename Enable = void>
      struct extracted_size_type
      {
        using type = typename std::allocator_traits<std::allocator<T>>::size_type;
      };

      template <typename Traits>
      struct extracted_size_type<Traits, void_t<typename Traits::size_type>>
      {
        using type = typename Traits::size_type;
      };

      template <typename Traits, typename Enable = void>
      struct extracted_pointer_type
      {
        using type = typename std::allocator_traits<std::allocator<T>>::pointer;
      };

      template <typename Traits>
      struct extracted_pointer_type<Traits, void_t<typename Traits::pointer>>
      {
        using type = typename Traits::pointer;
      };

    public:
      using value_type = T;
      using difference_type = typename extracted_difference_type<traits>::type;
      using size_type = typename extracted_size_type<traits>::type;
      using pointer = typename extracted_pointer_type<traits>::type;
      using propagate_on_container_move_assignment = std::true_type;
      using is_always_equal = std::true_type;

      base_allocator (void) = default;

      template <typename U>
      constexpr GCH_IMPLICIT_CONVERSION
      base_allocator (const base_allocator<U, PartialTraits>&) noexcept
      { }

      GCH_NODISCARD GCH_CPP20_CONSTEXPR
      pointer
      allocate (size_type n)
      {
        return pointer (std::allocator<value_type> ().allocate (n));
      }

      GCH_CPP20_CONSTEXPR
      void
      deallocate (pointer p, size_type n) noexcept
      {
        std::allocator<value_type> ().deallocate (gch::test_types::to_address (p), n);
      }
    };

    template <typename T, typename U, typename Traits>
    constexpr
    bool
    operator== (const base_allocator<T, Traits>&, const base_allocator<U, Traits>&) noexcept
    {
      return true;
    }

    template <typename T, typename Traits>
    constexpr
    bool
    operator!= (const base_allocator<T, Traits>&, const base_allocator<T, Traits>&) noexcept
    {
      return false;
    }

    template <typename T, typename U, typename Traits>
    constexpr
    bool
    operator!= (const base_allocator<T, Traits>&, const base_allocator<U, Traits>&) noexcept
    {
      return false;
    }

    template <typename T, typename PartialTraits = std::allocator_traits<std::allocator<T>>>
    class allocator_with_id
      : public base_allocator<T, PartialTraits>
    {
      using base = base_allocator<T, PartialTraits>;

    public:
      using is_always_equal = std::false_type;

      allocator_with_id (void) = default;

      constexpr explicit
      allocator_with_id (int id) noexcept
        : m_id (id)
      { }

      template <typename U>
      constexpr GCH_IMPLICIT_CONVERSION
      allocator_with_id (const allocator_with_id<U, PartialTraits>& other) noexcept
        : base (other),
          m_id (other.get_id ())
      { }

      GCH_NODISCARD constexpr
      int
      get_id (void) const noexcept
      {
        return m_id;
      }

    private:
      int m_id = 0;
    };

    template <typename T, typename U, typename Traits>
    constexpr
    bool
    operator== (const allocator_with_id<T, Traits>& lhs,
                const allocator_with_id<U, Traits>& rhs) noexcept
    {
      return lhs.get_id () == rhs.get_id ();
    }

    template <typename T, typename Traits>
    constexpr
    bool
    operator!= (const allocator_with_id<T, Traits>& lhs,
                const allocator_with_id<T, Traits>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    template <typename T, typename U, typename Traits>
    constexpr
    bool
    operator!= (const allocator_with_id<T, Traits>& lhs,
                const allocator_with_id<U, Traits>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    struct verifying_allocator_base
    {
      using allocation_map_type = std::unordered_map<void *, std::size_t>;
      using allocator_map_type = std::unordered_map<int, allocation_map_type>;

      using object_tracker_type = std::vector<void *>;

      static
      allocator_map_type&
      get_map (void)
      {
        auto deleter = [](allocator_map_type *map_ptr) noexcept {
          std::for_each (map_ptr->begin (), map_ptr->end (), [](
            const allocator_map_type::value_type& pair
          ) {
            assert (pair.second.empty () && "Some allocations were not freed.");
          });
          delete map_ptr;
        };

        static std::unique_ptr<allocator_map_type, decltype (deleter)> map (nullptr, deleter);

        if (! map)
          map.reset (new allocator_map_type);
        return *map;
      }

      static
      object_tracker_type&
      get_object_tracker (void)
      {
        auto deleter = [](object_tracker_type *ptr) noexcept {
          assert (ptr->empty () && "Not all objects were destroyed.");
          delete ptr;
        };

        static std::unique_ptr<object_tracker_type, decltype (deleter)> tkr (nullptr, deleter);

        if (! tkr)
          tkr.reset (new object_tracker_type);
        return *tkr;
      }

      template <typename T, typename Traits>
      static
      allocation_map_type&
      get_allocation_tracker (const allocator_with_id<T, Traits>& alloc)
      {
        auto allocator_it = get_map ().find (alloc.get_id ());
        assert (allocator_it != get_map ().end () && "The allocator has not been used yet.");
        return allocator_it->second;
      }

      template <typename T, typename Traits, typename Pointer>
      static
      void
      register_allocation (const allocator_with_id<T, Traits>& alloc, Pointer p, std::size_t n)
      {
        allocation_map_type& map = get_map ()[alloc.get_id ()];
        map.emplace (static_cast<void *> (gch::test_types::to_address (p)), n);
      }

      template <typename T, typename Traits, typename Pointer>
      static
      std::unordered_map<void *, std::size_t>::const_iterator
      verify_allocation (const allocator_with_id<T, Traits>& alloc, Pointer p, std::size_t n)
      {
        const allocation_map_type& map = get_allocation_tracker (alloc);

        auto allocation_it = map.find (static_cast<void *> (gch::test_types::to_address (p)));
        assert (allocation_it != map.end ()
            &&  "Could not find the allocation for the given allocator.");

        assert (allocation_it->second == n && "The allocation size did not match the given size.");
        return allocation_it;
      }

      template <typename T, typename Traits, typename Pointer>
      static
      void
      remove_allocation (const allocator_with_id<T, Traits>& alloc, Pointer p, std::size_t n)
      {
        allocation_map_type& map = get_allocation_tracker (alloc);
        map.erase (verify_allocation (alloc, p, n));
      }

      template <typename Pointer>
      static
      void
      register_object (Pointer p)
      {
        get_object_tracker ().push_back (static_cast<void *> (gch::test_types::to_address (p)));
      }

      template <typename Pointer>
      static
      std::vector<void *>::const_iterator
      verify_object (Pointer p)
      {
        const object_tracker_type& tkr = get_object_tracker ();

        auto *ptr = gch::test_types::to_address (p);

        auto found = std::find (
          make_reverse_it (tkr.end ()),
          make_reverse_it (tkr.begin ()),
          static_cast<void *> (ptr));
        assert (found.base () != tkr.begin () && "Object was not in its lifetime.");
        return std::prev (found.base ());
      }

      template <typename Pointer>
      static
      void
      remove_object (Pointer p)
      {
        get_object_tracker ().erase (verify_object (p));
      }
    };

    template <typename T, typename PartialTraits = std::allocator_traits<std::allocator<T>>>
    class verifying_allocator
      : public allocator_with_id<T, PartialTraits>,
        private verifying_allocator_base
    {
      using base = allocator_with_id<T, PartialTraits>;
      using alloc_traits = std::allocator_traits<base>;

    public:
      using propagate_on_container_copy_assignment = std::true_type;
      using propagate_on_container_swap = std::true_type;

      verifying_allocator            (void)                           = default;
      verifying_allocator            (const verifying_allocator&)     = default;
      verifying_allocator            (verifying_allocator&&) noexcept = default;
      verifying_allocator& operator= (const verifying_allocator&)     = default;
      verifying_allocator& operator= (verifying_allocator&&) noexcept = default;
      ~verifying_allocator           (void)                           = default;

      constexpr explicit
      verifying_allocator (int id) noexcept
        : base (id)
      { }

      template <typename U>
      constexpr GCH_IMPLICIT_CONVERSION
      verifying_allocator (const verifying_allocator<U, PartialTraits>& other) noexcept
        : base (other)
      { }

      GCH_NODISCARD
      typename alloc_traits::pointer
      allocate (typename alloc_traits::size_type n) noexcept (false)
      {
        typename alloc_traits::pointer ret = base::allocate (n);
        GCH_TRY
        {
          register_allocation (*this, ret, n);
        }
        GCH_CATCH (...)
        {
          base::deallocate (ret, n);
          GCH_THROW;
        }
        return ret;
      }

      void
      deallocate (typename alloc_traits::pointer p, typename alloc_traits::size_type n) noexcept
      {
        assert (nullptr != p);
        remove_allocation (*this, p, n);
        base::deallocate (p, n);
      }

      template <typename U, typename ...Args>
      void
      construct (U *p, Args&&... args) noexcept (false)
      {
        alloc_traits::construct (*this, p, std::forward<Args> (args)...);

        GCH_TRY
        {
          register_object (p);
        }
        GCH_CATCH (...)
        {
          alloc_traits::destroy (*this, p);
          GCH_THROW;
        }
      }

      template <typename U>
      void
      destroy (U *p)
      {
        assert (nullptr != p);
        remove_object (p);
        alloc_traits::destroy (*this, p);
      }
    };

    template <typename T, typename Traits>
    constexpr
    bool
    operator!= (const verifying_allocator<T, Traits>& lhs,
                const verifying_allocator<T, Traits>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    template <typename T, typename U, typename Traits>
    constexpr
    bool
    operator!= (const verifying_allocator<T, Traits>& lhs,
                const verifying_allocator<U, Traits>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    template <typename T, typename Traits = std::allocator_traits<std::allocator<T>>>
    struct non_propagating_verifying_allocator
      : verifying_allocator<T, Traits>
    {
      non_propagating_verifying_allocator (void) = default;

      template <typename U>
      constexpr GCH_IMPLICIT_CONVERSION
      non_propagating_verifying_allocator (
        const non_propagating_verifying_allocator<U, Traits>&) noexcept
      { }

      using verifying_allocator<T, Traits>::verifying_allocator;

      using propagate_on_container_move_assignment = std::false_type;
      using propagate_on_container_copy_assignment = std::false_type;
      using propagate_on_container_swap = std::false_type;
    };

    template <typename T, typename Traits>
    constexpr
    bool
    operator!= (const non_propagating_verifying_allocator<T, Traits>& lhs,
                const non_propagating_verifying_allocator<T, Traits>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    template <typename T, typename U, typename Traits>
    constexpr
    bool
    operator!= (const non_propagating_verifying_allocator<T, Traits>& lhs,
                const non_propagating_verifying_allocator<U, Traits>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    template <typename T, typename SizeType>
    struct sized_allocator
      : base_allocator<T, allocator_size_type_trait<SizeType>>
    {
      sized_allocator (void) = default;

      template <typename U>
      constexpr GCH_IMPLICIT_CONVERSION
      sized_allocator (const sized_allocator<U, SizeType>&) noexcept
      { }
    };

    template <typename T, typename SizeType>
    constexpr
    bool
    operator!= (const sized_allocator<T, SizeType>& lhs,
                const sized_allocator<T, SizeType>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    template <typename T, typename U, typename SizeType>
    constexpr
    bool
    operator!= (const sized_allocator<T, SizeType>& lhs,
                const sized_allocator<U, SizeType>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    template <typename T, typename Traits = allocator_pointer_trait<pointer_wrapper<T>>>
    struct fancy_pointer_allocator
      : base_allocator<T, Traits>
    {
      fancy_pointer_allocator (void) = default;

      template <typename U>
      constexpr GCH_IMPLICIT_CONVERSION
      fancy_pointer_allocator (const fancy_pointer_allocator<U, Traits>&) noexcept
      { }
    };

    template <typename T, typename Traits>
    constexpr
    bool
    operator!= (const fancy_pointer_allocator<T, Traits>& lhs,
                const fancy_pointer_allocator<T, Traits>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    template <typename T, typename U, typename Traits>
    constexpr
    bool
    operator!= (const fancy_pointer_allocator<T, Traits>& lhs,
                const fancy_pointer_allocator<U, Traits>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    template <typename T, typename Traits = std::allocator_traits<std::allocator<T>>>
    struct propagating_allocator_with_id
      : allocator_with_id<T, Traits>
    {
      using propagate_on_container_copy_assignment = std::true_type;
      using propagate_on_container_swap = std::true_type;

      propagating_allocator_with_id (void) = default;

      template <typename U>
      constexpr GCH_IMPLICIT_CONVERSION
      propagating_allocator_with_id (const propagating_allocator_with_id<U, Traits>&) noexcept
      { }

      using allocator_with_id<T, Traits>::allocator_with_id;
    };

    template <typename T, typename Traits>
    constexpr
    bool
    operator!= (const propagating_allocator_with_id<T, Traits>& lhs,
                const propagating_allocator_with_id<T, Traits>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

    template <typename T, typename U, typename Traits>
    constexpr
    bool
    operator!= (const propagating_allocator_with_id<T, Traits>& lhs,
                const propagating_allocator_with_id<U, Traits>& rhs) noexcept
    {
      return ! (lhs == rhs);
    }

  }

}

#endif // SMALL_VECTOR_TEST_ALLOCATORS_HPP
