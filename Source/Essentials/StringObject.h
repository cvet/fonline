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

// Inline character budget of the engine string, set by the build system. The value is a tuning knob rather
// than a property of the type; Docs/Essentials.md records how it is measured and what raising it costs
inline constexpr size_t STRING_INLINE_CAPACITY = FO_STRING_INLINE_CAPACITY;

// Same byte budget as the narrow string, so the rarely used wide string is not the larger object
inline constexpr size_t WSTRING_INLINE_CAPACITY = (STRING_INLINE_CAPACITY + 1) / sizeof(wchar_t) - 1;

// Heap blocks come from rpmalloc, whose small size classes step in 16 bytes; a capacity that fills one
// spends the tail of the block on characters instead of on padding
inline constexpr size_t STRING_ALLOCATION_GRANULARITY = 16;

// Behaves as std::basic_string, with the small-string buffer sized by the caller instead of by the standard
// library. Capacity equal to the inline budget is the discriminator: heap growth never lands on that value
template<typename CharT, size_t InlineCapacity>
class basic_string
{
    static_assert(InlineCapacity >= 1);
    static_assert(std::is_trivial_v<CharT>);

public:
    using traits_type = std::char_traits<CharT>;
    using value_type = CharT;
    using allocator_type = safe_allocator<CharT>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = CharT&;
    using const_reference = const CharT&;
    using pointer = CharT*;
    using const_pointer = const CharT*;
    using iterator = CharT*;
    using const_iterator = const CharT*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using view_type = std::basic_string_view<CharT, traits_type>;

    static constexpr size_type npos = static_cast<size_type>(-1);
    static constexpr size_type inline_capacity = InlineCapacity;

    // Mirrors the standard constraint on the std::basic_string overloads that take any string-view-like type
    template<typename T>
    static constexpr bool view_like = std::is_convertible_v<const T&, view_type> && !std::is_convertible_v<const T&, const CharT*>;

    constexpr basic_string() noexcept { reset_to_empty(); }

    constexpr explicit basic_string(const allocator_type& alloc) noexcept
    {
        ignore_unused(alloc);
        reset_to_empty();
    }

    constexpr basic_string(size_type count, CharT ch)
    {
        reset_to_empty();
        assign_fill(count, ch);
    }

    constexpr basic_string(const CharT* s, size_type count)
    {
        reset_to_empty();
        assign_chars(s, count);
    }

    // ReSharper disable once CppNonExplicitConvertingConstructor
    constexpr basic_string(const CharT* s)
    {
        reset_to_empty();
        assign_chars(s, traits_type::length(s));
    }

    constexpr basic_string(std::initializer_list<CharT> chars)
    {
        reset_to_empty();
        assign_chars(chars.begin(), chars.size());
    }

    template<std::input_iterator InputIt>
    constexpr basic_string(InputIt first, InputIt last)
    {
        reset_to_empty();
        append_range(first, last);
    }

    template<typename T>
        requires(view_like<T>)
    constexpr explicit basic_string(const T& t)
    {
        reset_to_empty();
        view_type sv = t;
        assign_chars(sv.data(), sv.size());
    }

    template<typename T>
        requires(view_like<T>)
    constexpr basic_string(const T& t, size_type pos, size_type count)
    {
        reset_to_empty();
        view_type sv = view_type(t).substr(pos, count);
        assign_chars(sv.data(), sv.size());
    }

    constexpr basic_string(const basic_string& other, size_type pos, size_type count = npos)
    {
        reset_to_empty();
        view_type sv = other.view().substr(pos, count);
        assign_chars(sv.data(), sv.size());
    }

    constexpr basic_string(std::nullptr_t) = delete;

    constexpr basic_string(const basic_string& other)
    {
        reset_to_empty();
        assign_chars(other.data(), other.size());
    }

    constexpr basic_string(basic_string&& other) noexcept { steal_from(other); }

    constexpr ~basic_string() { release_heap(); }

    constexpr auto operator=(const basic_string& other) -> basic_string&
    {
        if (this != &other) {
            assign_chars(other.data(), other.size());
        }

        return *this;
    }

    constexpr auto operator=(basic_string&& other) noexcept -> basic_string&
    {
        if (this != &other) {
            release_heap();
            steal_from(other);
        }

        return *this;
    }

    constexpr auto operator=(const CharT* s) -> basic_string& { return assign(s); }
    constexpr auto operator=(CharT ch) -> basic_string& { return assign(1, ch); }
    constexpr auto operator=(std::initializer_list<CharT> chars) -> basic_string& { return assign(chars); }
    constexpr auto operator=(std::nullptr_t) -> basic_string& = delete;

    template<typename T>
        requires(view_like<T>)
    constexpr auto operator=(const T& t) -> basic_string&
    {
        return assign(t);
    }

    constexpr auto assign(size_type count, CharT ch) -> basic_string&
    {
        assign_fill(count, ch);
        return *this;
    }

    constexpr auto assign(const basic_string& other) -> basic_string& { return *this = other; }
    constexpr auto assign(basic_string&& other) noexcept -> basic_string& { return *this = std::move(other); }

    constexpr auto assign(const basic_string& other, size_type pos, size_type count = npos) -> basic_string&
    {
        view_type sv = other.view().substr(pos, count);
        assign_chars(sv.data(), sv.size());
        return *this;
    }

    constexpr auto assign(const CharT* s, size_type count) -> basic_string&
    {
        assign_chars(s, count);
        return *this;
    }

    constexpr auto assign(const CharT* s) -> basic_string&
    {
        assign_chars(s, traits_type::length(s));
        return *this;
    }

    constexpr auto assign(std::initializer_list<CharT> chars) -> basic_string&
    {
        assign_chars(chars.begin(), chars.size());
        return *this;
    }

    template<std::input_iterator InputIt>
    constexpr auto assign(InputIt first, InputIt last) -> basic_string&
    {
        basic_string replacement(first, last);
        swap(replacement);
        return *this;
    }

    template<typename T>
        requires(view_like<T>)
    constexpr auto assign(const T& t) -> basic_string&
    {
        view_type sv = t;
        assign_chars(sv.data(), sv.size());
        return *this;
    }

    template<typename T>
        requires(view_like<T>)
    constexpr auto assign(const T& t, size_type pos, size_type count = npos) -> basic_string&
    {
        view_type sv = view_type(t).substr(pos, count);
        assign_chars(sv.data(), sv.size());
        return *this;
    }

    [[nodiscard]] constexpr auto get_allocator() const noexcept -> allocator_type { return allocator_type(); }

    [[nodiscard]] constexpr auto at(size_type pos) -> CharT&
    {
        throw_if_out_of_range(pos);
        return mutable_data()[pos];
    }

    [[nodiscard]] constexpr auto at(size_type pos) const -> const CharT&
    {
        throw_if_out_of_range(pos);
        return data()[pos];
    }

    [[nodiscard]] constexpr auto operator[](size_type pos) noexcept -> CharT& { return mutable_data()[pos]; }
    [[nodiscard]] constexpr auto operator[](size_type pos) const noexcept -> const CharT& { return data()[pos]; }
    [[nodiscard]] constexpr auto front() noexcept -> CharT& { return mutable_data()[0]; }
    [[nodiscard]] constexpr auto front() const noexcept -> const CharT& { return data()[0]; }
    [[nodiscard]] constexpr auto back() noexcept -> CharT& { return mutable_data()[_size - 1]; }
    [[nodiscard]] constexpr auto back() const noexcept -> const CharT& { return data()[_size - 1]; }

    [[nodiscard]] constexpr auto data() noexcept -> CharT* { return mutable_data(); }
    [[nodiscard]] constexpr auto data() const noexcept -> const CharT* { return is_inlined() ? _data.inlined : _data.heap; }
    [[nodiscard]] constexpr auto c_str() const noexcept -> const CharT* { return data(); }

    // ReSharper disable once CppNonExplicitConversionOperator
    [[nodiscard]] constexpr operator view_type() const noexcept { return view_type(data(), _size); }

    [[nodiscard]] constexpr auto begin() noexcept -> iterator { return mutable_data(); }
    [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator { return data(); }
    [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator { return data(); }
    [[nodiscard]] constexpr auto end() noexcept -> iterator { return mutable_data() + _size; }
    [[nodiscard]] constexpr auto end() const noexcept -> const_iterator { return data() + _size; }
    [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator { return data() + _size; }
    [[nodiscard]] constexpr auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
    [[nodiscard]] constexpr auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
    [[nodiscard]] constexpr auto crbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(cend()); }
    [[nodiscard]] constexpr auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
    [[nodiscard]] constexpr auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
    [[nodiscard]] constexpr auto crend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(cbegin()); }

    [[nodiscard]] constexpr auto empty() const noexcept -> bool { return _size == 0; }
    [[nodiscard]] constexpr auto size() const noexcept -> size_type { return _size; }
    [[nodiscard]] constexpr auto length() const noexcept -> size_type { return _size; }
    [[nodiscard]] constexpr auto capacity() const noexcept -> size_type { return _capacity; }
    [[nodiscard]] constexpr auto is_inlined() const noexcept -> bool { return _capacity == InlineCapacity; }
    [[nodiscard]] static constexpr auto max_size() noexcept -> size_type { return static_cast<size_type>(PTRDIFF_MAX) / sizeof(CharT) - 1; }

    constexpr void reserve(size_type new_cap)
    {
        if (new_cap > _capacity) {
            reallocate(rounded_capacity(new_cap));
        }
    }

    constexpr void shrink_to_fit()
    {
        if (!is_inlined() && _size < _capacity) {
            if (_size <= InlineCapacity) {
                CharT* old_buf = _data.heap;
                size_type old_cap = _capacity;
                traits_type::copy(_data.inlined, old_buf, _size);
                _capacity = InlineCapacity;
                set_size(_size);
                deallocate_buffer(old_buf, old_cap);
            }
            else {
                reallocate(_size);
            }
        }
    }

    constexpr void clear() noexcept { set_size(0); }

    constexpr void resize(size_type count) { resize(count, CharT()); }

    constexpr void resize(size_type count, CharT ch)
    {
        if (count > _size) {
            grow_to(count);
            traits_type::assign(mutable_data() + _size, count - _size, ch);
        }

        set_size(count);
    }

    constexpr void push_back(CharT ch)
    {
        grow_to(_size + 1);
        mutable_data()[_size] = ch;
        set_size(_size + 1);
    }

    constexpr void pop_back() noexcept { set_size(_size - 1); }

    constexpr auto append(size_type count, CharT ch) -> basic_string&
    {
        grow_to(_size + count);
        traits_type::assign(mutable_data() + _size, count, ch);
        set_size(_size + count);
        return *this;
    }

    constexpr auto append(const basic_string& other) -> basic_string& { return append(other.data(), other.size()); }

    constexpr auto append(const basic_string& other, size_type pos, size_type count = npos) -> basic_string&
    {
        view_type sv = other.view().substr(pos, count);
        return append(sv.data(), sv.size());
    }

    constexpr auto append(const CharT* s, size_type count) -> basic_string&
    {
        append_chars(s, count);
        return *this;
    }

    constexpr auto append(const CharT* s) -> basic_string& { return append(s, traits_type::length(s)); }
    constexpr auto append(std::initializer_list<CharT> chars) -> basic_string& { return append(chars.begin(), chars.size()); }

    template<std::input_iterator InputIt>
    constexpr auto append(InputIt first, InputIt last) -> basic_string&
    {
        append_range(first, last);
        return *this;
    }

    template<typename T>
        requires(view_like<T>)
    constexpr auto append(const T& t) -> basic_string&
    {
        view_type sv = t;
        return append(sv.data(), sv.size());
    }

    template<typename T>
        requires(view_like<T>)
    constexpr auto append(const T& t, size_type pos, size_type count = npos) -> basic_string&
    {
        view_type sv = view_type(t).substr(pos, count);
        return append(sv.data(), sv.size());
    }

    constexpr auto operator+=(const basic_string& other) -> basic_string& { return append(other); }
    constexpr auto operator+=(CharT ch) -> basic_string& { return append(1, ch); }
    constexpr auto operator+=(const CharT* s) -> basic_string& { return append(s); }
    constexpr auto operator+=(std::initializer_list<CharT> chars) -> basic_string& { return append(chars); }

    template<typename T>
        requires(view_like<T>)
    constexpr auto operator+=(const T& t) -> basic_string&
    {
        return append(t);
    }

    constexpr auto insert(size_type index, size_type count, CharT ch) -> basic_string&
    {
        basic_string filler(count, ch);
        return insert(index, filler.data(), filler.size());
    }

    constexpr auto insert(size_type index, const CharT* s, size_type count) -> basic_string&
    {
        replace_chars(index, 0, s, count);
        return *this;
    }

    constexpr auto insert(size_type index, const CharT* s) -> basic_string& { return insert(index, s, traits_type::length(s)); }
    constexpr auto insert(size_type index, const basic_string& other) -> basic_string& { return insert(index, other.data(), other.size()); }

    constexpr auto insert(size_type index, const basic_string& other, size_type other_index, size_type count = npos) -> basic_string&
    {
        view_type sv = other.view().substr(other_index, count);
        return insert(index, sv.data(), sv.size());
    }

    template<typename T>
        requires(view_like<T>)
    constexpr auto insert(size_type index, const T& t) -> basic_string&
    {
        view_type sv = t;
        return insert(index, sv.data(), sv.size());
    }

    template<typename T>
        requires(view_like<T>)
    constexpr auto insert(size_type index, const T& t, size_type t_index, size_type count = npos) -> basic_string&
    {
        view_type sv = view_type(t).substr(t_index, count);
        return insert(index, sv.data(), sv.size());
    }

    constexpr auto insert(const_iterator pos, CharT ch) -> iterator { return insert(pos, 1, ch); }

    constexpr auto insert(const_iterator pos, size_type count, CharT ch) -> iterator
    {
        size_type index = offset_of(pos);
        (void)insert(index, count, ch);
        return mutable_data() + index;
    }

    constexpr auto insert(const_iterator pos, std::initializer_list<CharT> chars) -> iterator
    {
        size_type index = offset_of(pos);
        (void)insert(index, chars.begin(), chars.size());
        return mutable_data() + index;
    }

    template<std::input_iterator InputIt>
    constexpr auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator
    {
        size_type index = offset_of(pos);
        basic_string inserted(first, last);
        (void)insert(index, inserted.data(), inserted.size());
        return mutable_data() + index;
    }

    constexpr auto erase(size_type index = 0, size_type count = npos) -> basic_string&
    {
        throw_if_position_past_end(index);
        size_type erased = std::min(count, _size - index);
        traits_type::move(mutable_data() + index, data() + index + erased, _size - index - erased);
        set_size(_size - erased);
        return *this;
    }

    constexpr auto erase(const_iterator pos) -> iterator
    {
        size_type index = offset_of(pos);
        (void)erase(index, 1);
        return mutable_data() + index;
    }

    constexpr auto erase(const_iterator first, const_iterator last) -> iterator
    {
        size_type index = offset_of(first);
        (void)erase(index, static_cast<size_type>(last - first));
        return mutable_data() + index;
    }

    constexpr auto replace(size_type pos, size_type count, const CharT* s, size_type count2) -> basic_string&
    {
        replace_chars(pos, count, s, count2);
        return *this;
    }

    constexpr auto replace(size_type pos, size_type count, const CharT* s) -> basic_string& { return replace(pos, count, s, traits_type::length(s)); }
    constexpr auto replace(size_type pos, size_type count, const basic_string& other) -> basic_string& { return replace(pos, count, other.data(), other.size()); }

    constexpr auto replace(size_type pos, size_type count, const basic_string& other, size_type pos2, size_type count2 = npos) -> basic_string&
    {
        view_type sv = other.view().substr(pos2, count2);
        return replace(pos, count, sv.data(), sv.size());
    }

    constexpr auto replace(size_type pos, size_type count, size_type count2, CharT ch) -> basic_string&
    {
        basic_string filler(count2, ch);
        return replace(pos, count, filler.data(), filler.size());
    }

    template<typename T>
        requires(view_like<T>)
    constexpr auto replace(size_type pos, size_type count, const T& t) -> basic_string&
    {
        view_type sv = t;
        return replace(pos, count, sv.data(), sv.size());
    }

    template<typename T>
        requires(view_like<T>)
    constexpr auto replace(size_type pos, size_type count, const T& t, size_type pos2, size_type count2 = npos) -> basic_string&
    {
        view_type sv = view_type(t).substr(pos2, count2);
        return replace(pos, count, sv.data(), sv.size());
    }

    constexpr auto replace(const_iterator first, const_iterator last, const basic_string& other) -> basic_string& { return replace(offset_of(first), static_cast<size_type>(last - first), other); }
    constexpr auto replace(const_iterator first, const_iterator last, const CharT* s, size_type count2) -> basic_string& { return replace(offset_of(first), static_cast<size_type>(last - first), s, count2); }
    constexpr auto replace(const_iterator first, const_iterator last, const CharT* s) -> basic_string& { return replace(offset_of(first), static_cast<size_type>(last - first), s); }
    constexpr auto replace(const_iterator first, const_iterator last, size_type count2, CharT ch) -> basic_string& { return replace(offset_of(first), static_cast<size_type>(last - first), count2, ch); }
    constexpr auto replace(const_iterator first, const_iterator last, std::initializer_list<CharT> chars) -> basic_string& { return replace(offset_of(first), static_cast<size_type>(last - first), chars.begin(), chars.size()); }

    template<std::input_iterator InputIt>
    constexpr auto replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2) -> basic_string&
    {
        basic_string replacement(first2, last2);
        return replace(offset_of(first), static_cast<size_type>(last - first), replacement.data(), replacement.size());
    }

    template<typename T>
        requires(view_like<T>)
    constexpr auto replace(const_iterator first, const_iterator last, const T& t) -> basic_string&
    {
        return replace(offset_of(first), static_cast<size_type>(last - first), t);
    }

    [[nodiscard]] constexpr auto substr(size_type pos = 0, size_type count = npos) const -> basic_string { return basic_string(view().substr(pos, count)); }

    constexpr auto copy(CharT* dest, size_type count, size_type pos = 0) const -> size_type
    {
        view_type sv = view().substr(pos, count);
        traits_type::copy(dest, sv.data(), sv.size());
        return sv.size();
    }

    constexpr void swap(basic_string& other) noexcept
    {
        storage tmp_data = _data;
        _data = other._data;
        other._data = tmp_data;
        std::swap(_size, other._size);
        std::swap(_capacity, other._capacity);
    }

    [[nodiscard]] constexpr auto compare(const basic_string& other) const noexcept -> int32_t { return view().compare(other.view()); }
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const basic_string& other) const -> int32_t { return view().substr(pos, count).compare(other.view()); }
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const basic_string& other, size_type pos2, size_type count2 = npos) const -> int32_t { return view().substr(pos, count).compare(other.view().substr(pos2, count2)); }
    [[nodiscard]] constexpr auto compare(const CharT* s) const -> int32_t { return view().compare(s); }
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const CharT* s) const -> int32_t { return view().substr(pos, count).compare(s); }
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const CharT* s, size_type count2) const -> int32_t { return view().substr(pos, count).compare(view_type(s, count2)); }

    template<typename T>
        requires(view_like<T>)
    [[nodiscard]] constexpr auto compare(const T& t) const noexcept -> int32_t
    {
        return view().compare(view_type(t));
    }

    template<typename T>
        requires(view_like<T>)
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const T& t) const -> int32_t
    {
        return view().substr(pos, count).compare(view_type(t));
    }

    [[nodiscard]] constexpr auto starts_with(view_type sv) const noexcept -> bool { return view().starts_with(sv); }
    [[nodiscard]] constexpr auto starts_with(CharT ch) const noexcept -> bool { return view().starts_with(ch); }
    [[nodiscard]] constexpr auto starts_with(const CharT* s) const -> bool { return view().starts_with(s); }
    [[nodiscard]] constexpr auto ends_with(view_type sv) const noexcept -> bool { return view().ends_with(sv); }
    [[nodiscard]] constexpr auto ends_with(CharT ch) const noexcept -> bool { return view().ends_with(ch); }
    [[nodiscard]] constexpr auto ends_with(const CharT* s) const -> bool { return view().ends_with(s); }
    [[nodiscard]] constexpr auto contains(view_type sv) const noexcept -> bool { return view().find(sv) != npos; }
    [[nodiscard]] constexpr auto contains(CharT ch) const noexcept -> bool { return view().find(ch) != npos; }
    [[nodiscard]] constexpr auto contains(const CharT* s) const -> bool { return view().find(s) != npos; }

    [[nodiscard]] constexpr auto find(const basic_string& other, size_type pos = 0) const noexcept -> size_type { return view().find(other.view(), pos); }
    [[nodiscard]] constexpr auto find(const CharT* s, size_type pos, size_type count) const noexcept -> size_type { return view().find(s, pos, count); }
    [[nodiscard]] constexpr auto find(const CharT* s, size_type pos = 0) const noexcept -> size_type { return view().find(s, pos); }
    [[nodiscard]] constexpr auto find(CharT ch, size_type pos = 0) const noexcept -> size_type { return view().find(ch, pos); }
    [[nodiscard]] constexpr auto rfind(const basic_string& other, size_type pos = npos) const noexcept -> size_type { return view().rfind(other.view(), pos); }
    [[nodiscard]] constexpr auto rfind(const CharT* s, size_type pos, size_type count) const noexcept -> size_type { return view().rfind(s, pos, count); }
    [[nodiscard]] constexpr auto rfind(const CharT* s, size_type pos = npos) const noexcept -> size_type { return view().rfind(s, pos); }
    [[nodiscard]] constexpr auto rfind(CharT ch, size_type pos = npos) const noexcept -> size_type { return view().rfind(ch, pos); }
    [[nodiscard]] constexpr auto find_first_of(const basic_string& other, size_type pos = 0) const noexcept -> size_type { return view().find_first_of(other.view(), pos); }
    [[nodiscard]] constexpr auto find_first_of(const CharT* s, size_type pos, size_type count) const noexcept -> size_type { return view().find_first_of(s, pos, count); }
    [[nodiscard]] constexpr auto find_first_of(const CharT* s, size_type pos = 0) const noexcept -> size_type { return view().find_first_of(s, pos); }
    [[nodiscard]] constexpr auto find_first_of(CharT ch, size_type pos = 0) const noexcept -> size_type { return view().find_first_of(ch, pos); }
    [[nodiscard]] constexpr auto find_last_of(const basic_string& other, size_type pos = npos) const noexcept -> size_type { return view().find_last_of(other.view(), pos); }
    [[nodiscard]] constexpr auto find_last_of(const CharT* s, size_type pos, size_type count) const noexcept -> size_type { return view().find_last_of(s, pos, count); }
    [[nodiscard]] constexpr auto find_last_of(const CharT* s, size_type pos = npos) const noexcept -> size_type { return view().find_last_of(s, pos); }
    [[nodiscard]] constexpr auto find_last_of(CharT ch, size_type pos = npos) const noexcept -> size_type { return view().find_last_of(ch, pos); }
    [[nodiscard]] constexpr auto find_first_not_of(const basic_string& other, size_type pos = 0) const noexcept -> size_type { return view().find_first_not_of(other.view(), pos); }
    [[nodiscard]] constexpr auto find_first_not_of(const CharT* s, size_type pos, size_type count) const noexcept -> size_type { return view().find_first_not_of(s, pos, count); }
    [[nodiscard]] constexpr auto find_first_not_of(const CharT* s, size_type pos = 0) const noexcept -> size_type { return view().find_first_not_of(s, pos); }
    [[nodiscard]] constexpr auto find_first_not_of(CharT ch, size_type pos = 0) const noexcept -> size_type { return view().find_first_not_of(ch, pos); }
    [[nodiscard]] constexpr auto find_last_not_of(const basic_string& other, size_type pos = npos) const noexcept -> size_type { return view().find_last_not_of(other.view(), pos); }
    [[nodiscard]] constexpr auto find_last_not_of(const CharT* s, size_type pos, size_type count) const noexcept -> size_type { return view().find_last_not_of(s, pos, count); }
    [[nodiscard]] constexpr auto find_last_not_of(const CharT* s, size_type pos = npos) const noexcept -> size_type { return view().find_last_not_of(s, pos); }
    [[nodiscard]] constexpr auto find_last_not_of(CharT ch, size_type pos = npos) const noexcept -> size_type { return view().find_last_not_of(ch, pos); }

    template<typename T>
        requires(view_like<T>)
    [[nodiscard]] constexpr auto find(const T& t, size_type pos = 0) const noexcept -> size_type
    {
        return view().find(view_type(t), pos);
    }

    template<typename T>
        requires(view_like<T>)
    [[nodiscard]] constexpr auto rfind(const T& t, size_type pos = npos) const noexcept -> size_type
    {
        return view().rfind(view_type(t), pos);
    }

    template<typename T>
        requires(view_like<T>)
    [[nodiscard]] constexpr auto find_first_of(const T& t, size_type pos = 0) const noexcept -> size_type
    {
        return view().find_first_of(view_type(t), pos);
    }

    template<typename T>
        requires(view_like<T>)
    [[nodiscard]] constexpr auto find_last_of(const T& t, size_type pos = npos) const noexcept -> size_type
    {
        return view().find_last_of(view_type(t), pos);
    }

    template<typename T>
        requires(view_like<T>)
    [[nodiscard]] constexpr auto find_first_not_of(const T& t, size_type pos = 0) const noexcept -> size_type
    {
        return view().find_first_not_of(view_type(t), pos);
    }

    template<typename T>
        requires(view_like<T>)
    [[nodiscard]] constexpr auto find_last_not_of(const T& t, size_type pos = npos) const noexcept -> size_type
    {
        return view().find_last_not_of(view_type(t), pos);
    }

private:
    // Fundamental alignment is enough for a character array, and the pointer member already rounds the union
    // up to the pointer size
    union storage
    {
        CharT inlined[InlineCapacity + 1];
        CharT* heap;
    };

    [[nodiscard]] constexpr auto view() const noexcept -> view_type { return view_type(data(), _size); }
    [[nodiscard]] constexpr auto mutable_data() noexcept -> CharT* { return is_inlined() ? _data.inlined : _data.heap; }
    [[nodiscard]] constexpr auto offset_of(const_iterator pos) const noexcept -> size_type { return static_cast<size_type>(pos - data()); }

    constexpr void reset_to_empty() noexcept
    {
        _capacity = InlineCapacity;
        _size = 0;

        // Constant evaluation rejects a later read of an inline element that was never written, so the whole
        // array is activated there; at run time only the terminator matters
        if (std::is_constant_evaluated()) {
            for (size_type i = 0; i <= InlineCapacity; i++) {
                _data.inlined[i] = CharT();
            }
        }
        else {
            _data.inlined[0] = CharT();
        }
    }

    constexpr void steal_from(basic_string& other) noexcept
    {
        _data = other._data;
        _size = other._size;
        _capacity = other._capacity;
        other.reset_to_empty();
    }

    constexpr void set_size(size_type size) noexcept
    {
        _size = size;
        mutable_data()[size] = CharT();
    }

    constexpr void throw_if_out_of_range(size_type pos) const
    {
        if (pos >= _size) {
            throw std::out_of_range("String index out of range");
        }
    }

    constexpr void throw_if_position_past_end(size_type pos) const
    {
        if (pos > _size) {
            throw std::out_of_range("String position past the end");
        }
    }

    [[nodiscard]] constexpr auto points_into_buffer(const CharT* s) const noexcept -> bool
    {
        const CharT* first = data();
        return !std::less<const CharT*> {}(s, first) && std::less<const CharT*> {}(s, first + _capacity + 1);
    }

    [[nodiscard]] static constexpr auto allocate_buffer(size_type capacity) -> CharT* { return allocator_type().allocate(capacity + 1); }

    static constexpr void deallocate_buffer(CharT* buf, size_type capacity) noexcept { allocator_type().deallocate(buf, capacity + 1); }

    constexpr void release_heap() noexcept
    {
        if (!is_inlined()) {
            deallocate_buffer(_data.heap, _capacity);
        }
    }

    [[nodiscard]] static constexpr auto rounded_capacity(size_type capacity) -> size_type
    {
        constexpr size_type granularity = std::max<size_type>(STRING_ALLOCATION_GRANULARITY / sizeof(CharT), 1);

        if (capacity > max_size()) {
            throw std::length_error("String length exceeds the maximum");
        }

        size_type chars = (capacity + 1 + granularity - 1) / granularity * granularity;
        return std::min(chars - 1, max_size());
    }

    [[nodiscard]] static constexpr auto grown_capacity(size_type current, size_type required) -> size_type
    {
        size_type geometric = current + current / 2;
        return rounded_capacity(std::max(required, geometric));
    }

    constexpr void grow_to(size_type required)
    {
        if (required > _capacity) {
            reallocate(grown_capacity(_capacity, required));
        }
    }

    constexpr void reallocate(size_type new_cap)
    {
        CharT* buf = allocate_buffer(new_cap);
        traits_type::copy(buf, data(), _size);
        release_heap();
        _data.heap = buf;
        _capacity = new_cap;
        set_size(_size);
    }

    constexpr void assign_chars(const CharT* s, size_type count)
    {
        if (count > _capacity) {
            // The source may live inside our own buffer, so the new block is filled before the old one is freed
            size_type new_cap = grown_capacity(_capacity, count);
            CharT* buf = allocate_buffer(new_cap);
            traits_type::copy(buf, s, count);
            release_heap();
            _data.heap = buf;
            _capacity = new_cap;
        }
        else {
            traits_type::move(mutable_data(), s, count);
        }

        set_size(count);
    }

    constexpr void assign_fill(size_type count, CharT ch)
    {
        if (count > _capacity) {
            size_type new_cap = grown_capacity(_capacity, count);
            CharT* buf = allocate_buffer(new_cap);
            release_heap();
            _data.heap = buf;
            _capacity = new_cap;
        }

        traits_type::assign(mutable_data(), count, ch);
        set_size(count);
    }

    // A measurable range is written in one pass after a single growth step, as the standard string does; an
    // input-only range has no length to work with and falls back to element-wise growth
    template<std::input_iterator InputIt>
    constexpr void append_range(InputIt first, InputIt last)
    {
        if constexpr (std::forward_iterator<InputIt>) {
            size_type count = static_cast<size_type>(std::distance(first, last));

            if (count == 0) {
                return;
            }

            size_type new_size = _size + count;

            if (new_size > _capacity) {
                // The range may live inside our own buffer, so the new block is filled before the old one is freed
                size_type new_cap = grown_capacity(_capacity, new_size);
                CharT* buf = allocate_buffer(new_cap);
                traits_type::copy(buf, data(), _size);
                write_range(buf + _size, first, last);
                release_heap();
                _data.heap = buf;
                _capacity = new_cap;
            }
            else {
                write_range(mutable_data() + _size, first, last);
            }

            set_size(new_size);
        }
        else {
            for (; first != last; ++first) {
                push_back(static_cast<CharT>(*first));
            }
        }
    }

    template<std::input_iterator InputIt>
    static constexpr void write_range(CharT* dest, InputIt first, InputIt last)
    {
        for (; first != last; ++first, ++dest) {
            *dest = static_cast<CharT>(*first);
        }
    }

    constexpr void append_chars(const CharT* s, size_type count)
    {
        if (count == 0) {
            return;
        }

        size_type new_size = _size + count;

        if (new_size > _capacity) {
            // The source may live inside our own buffer, so the new block is filled before the old one is freed
            size_type new_cap = grown_capacity(_capacity, new_size);
            CharT* buf = allocate_buffer(new_cap);
            traits_type::copy(buf, data(), _size);
            traits_type::copy(buf + _size, s, count);
            release_heap();
            _data.heap = buf;
            _capacity = new_cap;
        }
        else {
            traits_type::copy(mutable_data() + _size, s, count);
        }

        set_size(new_size);
    }

    constexpr void replace_chars(size_type pos, size_type count, const CharT* s, size_type count2)
    {
        throw_if_position_past_end(pos);

        // A source taken from this very buffer would be shifted or freed mid-operation, so it is detached first
        if (points_into_buffer(s)) {
            basic_string detached(s, count2);
            replace_chars(pos, count, detached.data(), detached.size());
            return;
        }

        size_type removed = std::min(count, _size - pos);
        size_type new_size = _size - removed + count2;
        size_type tail = _size - pos - removed;

        if (new_size > _capacity) {
            size_type new_cap = grown_capacity(_capacity, new_size);
            CharT* buf = allocate_buffer(new_cap);
            traits_type::copy(buf, data(), pos);
            traits_type::copy(buf + pos, s, count2);
            traits_type::copy(buf + pos + count2, data() + pos + removed, tail);
            release_heap();
            _data.heap = buf;
            _capacity = new_cap;
        }
        else {
            CharT* buf = mutable_data();
            traits_type::move(buf + pos + count2, buf + pos + removed, tail);
            traits_type::copy(buf + pos, s, count2);
        }

        set_size(new_size);
    }

    storage _data;
    size_type _size;
    size_type _capacity;
};

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator==(const basic_string<CharT, InlineCapacity>& lhs, const basic_string<CharT, InlineCapacity>& rhs) noexcept -> bool
{
    return lhs.size() == rhs.size() && std::char_traits<CharT>::compare(lhs.data(), rhs.data(), lhs.size()) == 0;
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator==(const basic_string<CharT, InlineCapacity>& lhs, const CharT* rhs) -> bool
{
    return lhs.compare(rhs) == 0;
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator<=>(const basic_string<CharT, InlineCapacity>& lhs, const basic_string<CharT, InlineCapacity>& rhs) noexcept -> std::strong_ordering
{
    return lhs.compare(rhs) <=> 0;
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator<=>(const basic_string<CharT, InlineCapacity>& lhs, const CharT* rhs) -> std::strong_ordering
{
    return lhs.compare(rhs) <=> 0;
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(const basic_string<CharT, InlineCapacity>& lhs, const basic_string<CharT, InlineCapacity>& rhs) -> basic_string<CharT, InlineCapacity>
{
    basic_string<CharT, InlineCapacity> result;
    result.reserve(lhs.size() + rhs.size());
    (void)result.append(lhs);
    (void)result.append(rhs);
    return result;
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(const basic_string<CharT, InlineCapacity>& lhs, const CharT* rhs) -> basic_string<CharT, InlineCapacity>
{
    basic_string<CharT, InlineCapacity> result(lhs);
    (void)result.append(rhs);
    return result;
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(const basic_string<CharT, InlineCapacity>& lhs, CharT rhs) -> basic_string<CharT, InlineCapacity>
{
    basic_string<CharT, InlineCapacity> result(lhs);
    result.push_back(rhs);
    return result;
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(const CharT* lhs, const basic_string<CharT, InlineCapacity>& rhs) -> basic_string<CharT, InlineCapacity>
{
    basic_string<CharT, InlineCapacity> result(lhs);
    (void)result.append(rhs);
    return result;
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(CharT lhs, const basic_string<CharT, InlineCapacity>& rhs) -> basic_string<CharT, InlineCapacity>
{
    basic_string<CharT, InlineCapacity> result(1, lhs);
    (void)result.append(rhs);
    return result;
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(basic_string<CharT, InlineCapacity>&& lhs, const basic_string<CharT, InlineCapacity>& rhs) -> basic_string<CharT, InlineCapacity>
{
    (void)lhs.append(rhs);
    return std::move(lhs);
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(const basic_string<CharT, InlineCapacity>& lhs, basic_string<CharT, InlineCapacity>&& rhs) -> basic_string<CharT, InlineCapacity>
{
    (void)rhs.insert(0, lhs);
    return std::move(rhs);
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(basic_string<CharT, InlineCapacity>&& lhs, basic_string<CharT, InlineCapacity>&& rhs) -> basic_string<CharT, InlineCapacity>
{
    (void)lhs.append(rhs);
    return std::move(lhs);
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(basic_string<CharT, InlineCapacity>&& lhs, const CharT* rhs) -> basic_string<CharT, InlineCapacity>
{
    (void)lhs.append(rhs);
    return std::move(lhs);
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(basic_string<CharT, InlineCapacity>&& lhs, CharT rhs) -> basic_string<CharT, InlineCapacity>
{
    lhs.push_back(rhs);
    return std::move(lhs);
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(const CharT* lhs, basic_string<CharT, InlineCapacity>&& rhs) -> basic_string<CharT, InlineCapacity>
{
    (void)rhs.insert(0, lhs);
    return std::move(rhs);
}

template<typename CharT, size_t InlineCapacity>
[[nodiscard]] constexpr auto operator+(CharT lhs, basic_string<CharT, InlineCapacity>&& rhs) -> basic_string<CharT, InlineCapacity>
{
    (void)rhs.insert(0, 1, lhs);
    return std::move(rhs);
}

template<typename CharT, size_t InlineCapacity>
constexpr void swap(basic_string<CharT, InlineCapacity>& lhs, basic_string<CharT, InlineCapacity>& rhs) noexcept
{
    lhs.swap(rhs);
}

template<typename CharT, typename Traits, size_t InlineCapacity>
auto operator<<(std::basic_ostream<CharT, Traits>& stream, const basic_string<CharT, InlineCapacity>& value) -> std::basic_ostream<CharT, Traits>&
{
    return stream << std::basic_string_view<CharT, Traits>(value.data(), value.size());
}

// Extraction goes to the global namespace, beside the FO_DECLARE_TYPE_PARSER operators: an operator>> inside
// the engine namespace would hide every one of those from unqualified lookup in engine code
FO_END_NAMESPACE

template<typename CharT, typename Traits, size_t InlineCapacity>
auto operator>>(std::basic_istream<CharT, Traits>& stream, FO_NAMESPACE basic_string<CharT, InlineCapacity>& value) -> std::basic_istream<CharT, Traits>&
{
    std::basic_string<CharT, Traits, FO_NAMESPACE safe_allocator<CharT>> extracted;
    stream >> extracted;
    (void)value.assign(extracted.data(), extracted.size());
    return stream;
}

FO_BEGIN_NAMESPACE

template<typename CharT, typename Traits, size_t InlineCapacity>
auto getline(std::basic_istream<CharT, Traits>& stream, basic_string<CharT, InlineCapacity>& value, CharT delimiter) -> std::basic_istream<CharT, Traits>&
{
    std::basic_string<CharT, Traits, safe_allocator<CharT>> extracted;
    (void)std::getline(stream, extracted, delimiter);
    (void)value.assign(extracted.data(), extracted.size());
    return stream;
}

template<typename CharT, typename Traits, size_t InlineCapacity>
auto getline(std::basic_istream<CharT, Traits>& stream, basic_string<CharT, InlineCapacity>& value) -> std::basic_istream<CharT, Traits>&
{
    return getline(stream, value, stream.widen('\n'));
}

FO_END_NAMESPACE

template<typename CharT, size_t InlineCapacity>
struct std::formatter<FO_NAMESPACE basic_string<CharT, InlineCapacity>, CharT> : formatter<std::basic_string_view<CharT>, CharT> // NOLINT(cert-dcl58-cpp)
{
    template<typename FormatContext>
    constexpr auto format(const FO_NAMESPACE basic_string<CharT, InlineCapacity>& value, FormatContext& ctx) const
    {
        return formatter<std::basic_string_view<CharT>, CharT>::format(std::basic_string_view<CharT>(value.data(), value.size()), ctx);
    }
};

template<typename CharT, size_t InlineCapacity>
struct std::hash<FO_NAMESPACE basic_string<CharT, InlineCapacity>> // NOLINT(cert-dcl58-cpp)
{
    [[nodiscard]] constexpr auto operator()(const FO_NAMESPACE basic_string<CharT, InlineCapacity>& value) const noexcept -> size_t { return hash<std::basic_string_view<CharT>> {}(std::basic_string_view<CharT>(value.data(), value.size())); }
};
