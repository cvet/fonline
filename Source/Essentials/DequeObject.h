//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include "BasicCore.h"
#include "MemorySystem.h"

FO_BEGIN_NAMESPACE

// Element bytes held by one block. The standard deque fixes its block at 16 bytes, so every element wider
// than a pointer costs a heap block of its own
inline constexpr size_t DEQUE_BLOCK_BYTES = 512;

// Blocks never hold fewer elements than this, so a wide element still amortises its allocations
inline constexpr size_t DEQUE_MIN_BLOCK_ELEMENTS = 4;

// Behaves as std::deque over the operations the engine uses, with the block size chosen by the caller rather
// than fixed by the standard library. Growth at either end never moves an element, as the standard promises
template<typename T, size_t BlockBytes>
class basic_deque
{
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using allocator_type = SafeAllocator<T>;

    static constexpr size_type block_elements = BlockBytes / sizeof(T) > DEQUE_MIN_BLOCK_ELEMENTS ? BlockBytes / sizeof(T) : DEQUE_MIN_BLOCK_ELEMENTS;

    // Walks logical positions rather than storage, so crossing a block boundary needs no special case at the
    // call site. The block index and offset are derived from a compile-time constant divisor
    template<bool IsConst>
    class basic_iterator
    {
        friend class basic_deque;
        template<bool>
        friend class basic_iterator;

        using owner_type = std::conditional_t<IsConst, const basic_deque*, basic_deque*>;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;

        constexpr basic_iterator() noexcept = default;

        // A template so that it is never the copy constructor, which a constrained same-signature
        // constructor would suppress for the mutable instantiation
        template<bool OtherConst>
            requires(IsConst && !OtherConst)
        // ReSharper disable once CppNonExplicitConvertingConstructor
        constexpr basic_iterator(const basic_iterator<OtherConst>& other) noexcept :
            _owner {other._owner},
            _index {other._index}
        {
        }

        [[nodiscard]] constexpr auto operator*() const noexcept -> reference { return (*_owner)[_index]; }
        [[nodiscard]] constexpr auto operator->() const noexcept -> pointer { return &(*_owner)[_index]; }
        [[nodiscard]] constexpr auto operator[](difference_type offset) const noexcept -> reference { return (*_owner)[_index + static_cast<size_type>(offset)]; }

        constexpr auto operator++() noexcept -> basic_iterator&
        {
            ++_index;
            return *this;
        }
        constexpr auto operator++(int) noexcept -> basic_iterator
        {
            basic_iterator copy = *this;
            ++_index;
            return copy;
        }
        constexpr auto operator--() noexcept -> basic_iterator&
        {
            --_index;
            return *this;
        }
        constexpr auto operator--(int) noexcept -> basic_iterator
        {
            basic_iterator copy = *this;
            --_index;
            return copy;
        }

        constexpr auto operator+=(difference_type offset) noexcept -> basic_iterator&
        {
            _index = static_cast<size_type>(static_cast<difference_type>(_index) + offset);
            return *this;
        }
        constexpr auto operator-=(difference_type offset) noexcept -> basic_iterator& { return *this += -offset; }

        [[nodiscard]] constexpr auto operator+(difference_type offset) const noexcept -> basic_iterator
        {
            basic_iterator copy = *this;
            return copy += offset;
        }
        [[nodiscard]] constexpr auto operator-(difference_type offset) const noexcept -> basic_iterator
        {
            basic_iterator copy = *this;
            return copy -= offset;
        }
        [[nodiscard]] constexpr auto operator-(const basic_iterator& other) const noexcept -> difference_type { return static_cast<difference_type>(_index) - static_cast<difference_type>(other._index); }

        [[nodiscard]] constexpr auto operator==(const basic_iterator& other) const noexcept -> bool { return _index == other._index; }
        [[nodiscard]] constexpr auto operator<=>(const basic_iterator& other) const noexcept -> std::strong_ordering { return _index <=> other._index; }

    private:
        constexpr basic_iterator(owner_type owner, size_type index) noexcept :
            _owner {owner},
            _index {index}
        {
        }

        owner_type _owner {};
        size_type _index {};
    };

    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    basic_deque() noexcept = default;

    basic_deque(const basic_deque& other)
    {
        for (const T& value : other) {
            push_back(value);
        }
    }

    basic_deque(basic_deque&& other) noexcept :
        _blocks {other._blocks},
        _blockCount {other._blockCount},
        _blockCapacity {other._blockCapacity},
        _first {other._first},
        _size {other._size}
    {
        other._blocks = nullptr;
        other._blockCount = 0;
        other._blockCapacity = 0;
        other._first = 0;
        other._size = 0;
    }

    auto operator=(const basic_deque& other) -> basic_deque&
    {
        if (this != &other) {
            basic_deque copy = other;
            swap(copy);
        }

        return *this;
    }

    auto operator=(basic_deque&& other) noexcept -> basic_deque&
    {
        if (this != &other) {
            release_storage();

            _blocks = other._blocks;
            _blockCount = other._blockCount;
            _blockCapacity = other._blockCapacity;
            _first = other._first;
            _size = other._size;

            other._blocks = nullptr;
            other._blockCount = 0;
            other._blockCapacity = 0;
            other._first = 0;
            other._size = 0;
        }

        return *this;
    }

    ~basic_deque() { release_storage(); }

    [[nodiscard]] auto size() const noexcept -> size_type { return _size; }
    [[nodiscard]] auto empty() const noexcept -> bool { return _size == 0; }

    [[nodiscard]] auto operator[](size_type index) noexcept -> reference { return slot(_first + index); }
    [[nodiscard]] auto operator[](size_type index) const noexcept -> const_reference { return slot(_first + index); }
    [[nodiscard]] auto front() noexcept -> reference { return slot(_first); }
    [[nodiscard]] auto front() const noexcept -> const_reference { return slot(_first); }
    [[nodiscard]] auto back() noexcept -> reference { return slot(_first + _size - 1); }
    [[nodiscard]] auto back() const noexcept -> const_reference { return slot(_first + _size - 1); }

    [[nodiscard]] auto begin() noexcept -> iterator { return {this, 0}; }
    [[nodiscard]] auto begin() const noexcept -> const_iterator { return {this, 0}; }
    [[nodiscard]] auto cbegin() const noexcept -> const_iterator { return {this, 0}; }
    [[nodiscard]] auto end() noexcept -> iterator { return {this, _size}; }
    [[nodiscard]] auto end() const noexcept -> const_iterator { return {this, _size}; }
    [[nodiscard]] auto cend() const noexcept -> const_iterator { return {this, _size}; }
    [[nodiscard]] auto rbegin() noexcept -> reverse_iterator { return reverse_iterator {end()}; }
    [[nodiscard]] auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator {end()}; }
    [[nodiscard]] auto crbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator {cend()}; }
    [[nodiscard]] auto rend() noexcept -> reverse_iterator { return reverse_iterator {begin()}; }
    [[nodiscard]] auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator {begin()}; }
    [[nodiscard]] auto crend() const noexcept -> const_reverse_iterator { return const_reverse_iterator {cbegin()}; }

    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(std::move(value)); }
    void push_front(const T& value) { emplace_front(value); }
    void push_front(T&& value) { emplace_front(std::move(value)); }

    template<typename... Args>
    auto emplace_back(Args&&... args) -> reference
    {
        reserve_back();

        T* place = address(_first + _size);
        std::construct_at(place, std::forward<Args>(args)...);
        _size++;

        return *place;
    }

    template<typename... Args>
    auto emplace_front(Args&&... args) -> reference
    {
        reserve_front();

        T* place = address(_first - 1);
        std::construct_at(place, std::forward<Args>(args)...);
        _first--;
        _size++;

        return *place;
    }

    void pop_back() noexcept
    {
        _size--;
        std::destroy_at(address(_first + _size));
        trim_back_block();
    }

    void pop_front() noexcept
    {
        std::destroy_at(address(_first));
        _first++;
        _size--;
        trim_front_block();
    }

    // Shifts toward whichever end is nearer, so everything from that end onwards is invalidated, as the
    // standard container does. Returns the position the removed element occupied
    auto erase(const_iterator pos) -> iterator
    {
        size_type index = pos._index;

        if (index < _size - index - 1) {
            for (size_type i = index; i > 0; i--) {
                slot(_first + i) = std::move(slot(_first + i - 1));
            }

            pop_front();
        }
        else {
            for (size_type i = index; i + 1 < _size; i++) {
                slot(_first + i) = std::move(slot(_first + i + 1));
            }

            pop_back();
        }

        return {this, index};
    }

    void clear() noexcept
    {
        destroy_elements();
        release_blocks();
        _first = 0;
    }

    void swap(basic_deque& other) noexcept
    {
        std::swap(_blocks, other._blocks);
        std::swap(_blockCount, other._blockCount);
        std::swap(_blockCapacity, other._blockCapacity);
        std::swap(_first, other._first);
        std::swap(_size, other._size);
    }

private:
    [[nodiscard]] auto address(size_type slot_index) const noexcept -> T* { return _blocks[slot_index / block_elements] + slot_index % block_elements; }
    [[nodiscard]] auto slot(size_type slot_index) noexcept -> reference { return *address(slot_index); }
    [[nodiscard]] auto slot(size_type slot_index) const noexcept -> const_reference { return *address(slot_index); }

    void reserve_back()
    {
        if (_first + _size == _blockCount * block_elements) {
            reserve_blocks(_blockCount + 1);
            _blocks[_blockCount] = allocator_type().allocate(block_elements);
            _blockCount++;
        }
    }

    void reserve_front()
    {
        if (_first == 0) {
            reserve_blocks(_blockCount + 1);
            std::copy_backward(_blocks, _blocks + _blockCount, _blocks + _blockCount + 1);
            _blocks[0] = allocator_type().allocate(block_elements);
            _blockCount++;
            _first += block_elements;
        }
    }

    void trim_front_block() noexcept
    {
        if (_first >= block_elements) {
            allocator_type().deallocate(_blocks[0], block_elements);
            std::copy(_blocks + 1, _blocks + _blockCount, _blocks);
            _blockCount--;
            _first -= block_elements;
        }
    }

    void trim_back_block() noexcept
    {
        if (_blockCount != 0 && _first + _size <= (_blockCount - 1) * block_elements) {
            _blockCount--;
            allocator_type().deallocate(_blocks[_blockCount], block_elements);
        }
    }

    void reserve_blocks(size_type required)
    {
        if (required <= _blockCapacity) {
            return;
        }

        size_type new_capacity = _blockCapacity != 0 ? _blockCapacity * 2 : 4;

        if (new_capacity < required) {
            new_capacity = required;
        }

        T** new_blocks = SafeAllocator<T*>().allocate(new_capacity);
        std::copy(_blocks, _blocks + _blockCount, new_blocks);

        if (_blocks != nullptr) {
            SafeAllocator<T*>().deallocate(_blocks, _blockCapacity);
        }

        _blocks = new_blocks;
        _blockCapacity = new_capacity;
    }

    void destroy_elements() noexcept
    {
        for (size_type i = 0; i < _size; i++) {
            std::destroy_at(address(_first + i));
        }

        _size = 0;
    }

    void release_blocks() noexcept
    {
        for (size_type i = 0; i < _blockCount; i++) {
            allocator_type().deallocate(_blocks[i], block_elements);
        }

        _blockCount = 0;
    }

    void release_storage() noexcept
    {
        destroy_elements();
        release_blocks();

        if (_blocks != nullptr) {
            SafeAllocator<T*>().deallocate(_blocks, _blockCapacity);
            _blocks = nullptr;
            _blockCapacity = 0;
        }

        _first = 0;
    }

    T** _blocks {};
    size_type _blockCount {};
    size_type _blockCapacity {};
    size_type _first {};
    size_type _size {};
};

FO_END_NAMESPACE
