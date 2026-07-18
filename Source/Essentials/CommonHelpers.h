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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Containers.h"
#include "ExceptionHandling.h"
#include "MemorySystem.h"
#include "SmartPointers.h"
#include "StackTrace.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE

// Reinterpret an opaque `void` pointer back to a typed nullable borrow. Lives here (not in BasicCore) because
// the nullable-borrow return type `nptr<T>` needs the smart-pointer vocabulary. The pointer type is spelled as
// the caller expects (`cast_from_void<T*>(vp)`), and the result is `nptr<T>` — deref/pass it through the wrapper
// vocabulary. Raw `void*`, `ptr<void>`, and `nptr<void>` sources are accepted.
template<typename T, typename U>
    requires(std::is_pointer_v<T> && !std::is_void_v<remove_all_pointers_t<T>> && std::is_pointer_v<U> && std::is_void_v<remove_all_pointers_t<U>>)
[[nodiscard]] inline auto cast_from_void(U ptr) noexcept -> nptr<std::remove_pointer_t<T>>
{
    return nptr<std::remove_pointer_t<T>>(static_cast<T>(ptr));
}

template<typename T, typename U>
    requires(std::is_pointer_v<T> && !std::is_void_v<remove_all_pointers_t<T>> && std::is_void_v<std::remove_const_t<U>>)
[[nodiscard]] inline auto cast_from_void(ptr<U> ptr) noexcept -> nptr<std::remove_pointer_t<T>>
{
    return cast_from_void<T>(ptr.get());
}

template<typename T, typename U>
    requires(std::is_pointer_v<T> && !std::is_void_v<remove_all_pointers_t<T>> && std::is_void_v<std::remove_const_t<U>>)
[[nodiscard]] inline auto cast_from_void(nptr<U> ptr) noexcept -> nptr<std::remove_pointer_t<T>>
{
    return cast_from_void<T>(ptr.get());
}

// Noexcept wrappers
template<typename T, typename... Args>
inline void safe_call(const T& callable, Args&&... args) noexcept
{
    static_assert(!std::is_nothrow_invocable_v<T, Args...>);

    try {
        std::invoke(callable, std::forward<Args>(args)...);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

// Smart pointer helpers
template<typename T>
[[nodiscard]] inline auto require_refcount_ptr(refcount_nptr<T> value) -> refcount_ptr<T>
{
    FO_VERIFY_AND_THROW(value, "Refcounted pointer is null");
    return std::move(value).take_not_null();
}

// Ref holders
template<typename T>
struct ref_hold_ptr_type
{
    using type = ptr<typename T::element_type>;
};

template<typename T>
struct ref_hold_ptr_type<T*>
{
    using type = ptr<T>;
};

template<typename T>
using ref_hold_ptr_t = typename ref_hold_ptr_type<std::remove_cvref_t<T>>::type;

template<typename T>
class ref_hold_vector
{
public:
    static_assert(std::is_pointer_v<T> || requires { typename T::element_type; });

    explicit ref_hold_vector(size_t capacity) { _vec.reserve(capacity); }
    explicit ref_hold_vector(vector<T>&& vec) noexcept :
        _vec {std::move(vec)}
    {
        for (T& ref : _vec) {
            add_ref(ref);
        }
    }

    ref_hold_vector(const ref_hold_vector&) = delete;
    ref_hold_vector(ref_hold_vector&&) noexcept = default;
    auto operator=(const ref_hold_vector&) -> ref_hold_vector& = delete;
    auto operator=(ref_hold_vector&&) noexcept -> ref_hold_vector& = delete;

    [[nodiscard]] constexpr auto begin() noexcept { return _vec.begin(); }
    [[nodiscard]] constexpr auto end() noexcept { return _vec.end(); }
    [[nodiscard]] constexpr auto cbegin() const noexcept { return _vec.cbegin(); }
    [[nodiscard]] constexpr auto cend() const noexcept { return _vec.cend(); }
    [[nodiscard]] constexpr auto size() const noexcept -> size_t { return _vec.size(); }

    ~ref_hold_vector()
    {
        for (T& ref : _vec) {
            release_ref(ref);
        }
    }

    void add(T ref)
    {
        add_ref(ref);
        _vec.emplace_back(std::move(ref));
    }

private:
    [[nodiscard]] static auto get_ref(T& ref) noexcept -> nptr<typename ref_hold_ptr_t<T>::element_type>
    {
        using element_type = typename ref_hold_ptr_t<T>::element_type;

        if constexpr (std::is_pointer_v<T>) {
            return nptr<element_type> {ref};
        }
        else if constexpr (requires { ref.get_no_const(); }) {
            return nptr<element_type> {ref.get_no_const()};
        }
        else {
            return ref;
        }
    }

    static void add_ref(T& ref)
    {
        auto ref_ptr = get_ref(ref);
        FO_VERIFY_AND_THROW(ref_ptr, "Missing required reference");
        ref_ptr->AddRef();
    }

    static void release_ref(T& ref)
    {
        auto ref_ptr = get_ref(ref);
        FO_VERIFY_AND_THROW(ref_ptr, "Missing required reference");
        ref_ptr->Release();
    }

    vector<T> _vec {};
};

template<typename T>
    requires(std::is_pointer_v<T>)
[[nodiscard]] constexpr auto copy_hold_ref(vector<T>&& value) -> ref_hold_vector<ref_hold_ptr_t<T>>
{
    auto ref_vec = ref_hold_vector<ref_hold_ptr_t<T>>(value.size());
    for (T ref : value) {
        ref_vec.add(ref_hold_ptr_t<T>(ref));
    }
    return ref_vec;
}

template<typename T>
    requires(!std::is_pointer_v<T>)
[[nodiscard]] constexpr auto copy_hold_ref(vector<T>&& value) -> ref_hold_vector<ref_hold_ptr_t<T>>
{
    auto ref_vec = ref_hold_vector<ref_hold_ptr_t<T>>(value.size());
    for (auto&& ref : value) {
        ref_vec.add(ref_hold_ptr_t<T>(ref.get()));
    }
    return ref_vec;
}

template<typename T>
    requires(std::is_pointer_v<T>)
[[nodiscard]] constexpr auto copy_hold_ref(vector<T>& value) -> ref_hold_vector<ref_hold_ptr_t<T>>
{
    auto ref_vec = ref_hold_vector<ref_hold_ptr_t<T>>(value.size());
    for (T ref : value) {
        ref_vec.add(ref_hold_ptr_t<T>(ref));
    }
    return ref_vec;
}

template<typename T>
    requires(!std::is_pointer_v<T>)
[[nodiscard]] constexpr auto copy_hold_ref(vector<T>& value) -> ref_hold_vector<ref_hold_ptr_t<T>>
{
    auto ref_vec = ref_hold_vector<ref_hold_ptr_t<T>>(value.size());
    for (auto&& ref : value) {
        ref_vec.add(ref_hold_ptr_t<T>(ref.get()));
    }
    return ref_vec;
}

template<typename T>
    requires(std::is_pointer_v<T>)
[[nodiscard]] constexpr auto copy_hold_ref(span<T> value) -> ref_hold_vector<ref_hold_ptr_t<T>>
{
    auto ref_vec = ref_hold_vector<ref_hold_ptr_t<T>>(value.size());
    for (T ref : value) {
        ref_vec.add(ref_hold_ptr_t<T>(ref));
    }
    return ref_vec;
}

template<typename T>
    requires(!std::is_pointer_v<T>)
[[nodiscard]] constexpr auto copy_hold_ref(span<T> value) -> ref_hold_vector<ref_hold_ptr_t<T>>
{
    auto ref_vec = ref_hold_vector<ref_hold_ptr_t<T>>(value.size());
    for (auto&& ref : value) {
        ref_vec.add(ref_hold_ptr_t<T>(ref.get()));
    }
    return ref_vec;
}

template<typename T, typename U>
    requires(!std::is_pointer_v<U>)
[[nodiscard]] constexpr auto copy_hold_ref(unordered_map<T, U>& value) -> ref_hold_vector<ref_hold_ptr_t<U>>
{
    auto ref_vec = ref_hold_vector<ref_hold_ptr_t<U>>(value.size());
    for (auto&& ref : value | std::views::values) {
        ref_vec.add(ref_hold_ptr_t<U>(ref.get()));
    }
    return ref_vec;
}

template<typename T, typename U>
    requires(!std::is_pointer_v<U>)
[[nodiscard]] constexpr auto copy_hold_ref(unordered_map<T, U>&& value) -> ref_hold_vector<ref_hold_ptr_t<U>>
{
    auto ref_vec = ref_hold_vector<ref_hold_ptr_t<U>>(value.size());
    for (auto&& ref : value | std::views::values) {
        ref_vec.add(ref_hold_ptr_t<U>(ref.get()));
    }
    return ref_vec;
}

template<typename T>
    requires(!std::is_pointer_v<T>)
[[nodiscard]] constexpr auto copy_hold_ref(unordered_set<T>& value) -> ref_hold_vector<ref_hold_ptr_t<T>>
{
    auto ref_vec = ref_hold_vector<ref_hold_ptr_t<T>>(value.size());
    for (auto&& ref : value) {
        ref_vec.add(ref_hold_ptr_t<T>(ref.get_no_const()));
    }
    return ref_vec;
}

// RAII holder for a value-typed C resource bracketed by paired init/clear free functions (e.g. th_info_init / th_info_clear).
// The held value is default-zeroed, then InitFn(&Value) runs on construction and ClearFn(&Value) on destruction.
template<typename T, auto InitFn, auto ClearFn>
    requires(std::is_invocable_v<decltype(InitFn), T*> && std::is_invocable_v<decltype(ClearFn), T*>)
struct scoped_init_clear
{
    scoped_init_clear() noexcept { InitFn(&Value); }
    scoped_init_clear(const scoped_init_clear&) = delete;
    scoped_init_clear(scoped_init_clear&&) noexcept = delete;
    auto operator=(const scoped_init_clear&) -> scoped_init_clear& = delete;
    auto operator=(scoped_init_clear&&) noexcept -> scoped_init_clear& = delete;
    ~scoped_init_clear() noexcept { ClearFn(&Value); }

    T Value {};
};

// Vector helpers
template<std::ranges::range T>
constexpr void vec_add_unique_value(T& vec, typename T::value_type value)
{
    auto it = std::ranges::find(vec, value);
    FO_VERIFY_AND_THROW(it == vec.end(), "Unexpected entry found in vec");
    vec.emplace_back(std::move(value));
}

template<std::ranges::range T>
constexpr void vec_remove_unique_value(T& vec, typename T::value_type value)
{
    auto it = std::ranges::find(vec, value);
    FO_VERIFY_AND_THROW(it != vec.end(), "Lookup failed in vec");
    vec.erase(it);
}

template<std::ranges::range T, typename U>
constexpr void vec_remove_unique_value_if(T& vec, const U& predicate)
{
    auto it = std::ranges::find_if(vec, predicate);
    FO_VERIFY_AND_THROW(it != vec.end(), "Lookup failed in vec");
    vec.erase(it);
}

template<std::ranges::range T>
constexpr auto vec_safe_add_unique_value(T& vec, typename T::value_type value) noexcept -> bool
{
    if (auto it = std::ranges::find(vec, value); it == vec.end()) {
        vec.emplace_back(std::move(value));
        return true;
    }
    return false;
}

template<std::ranges::range T>
constexpr auto vec_safe_remove_unique_value(T& vec, typename T::value_type value) noexcept -> bool
{
    if (auto it = std::ranges::find(vec, value); it != vec.end()) {
        vec.erase(it);
        return true;
    }
    return false;
}

template<std::ranges::range T, typename U>
constexpr auto vec_safe_remove_unique_value_if(T& vec, const U& predicate) noexcept -> bool
{
    if (auto it = std::ranges::find_if(vec, predicate); it != vec.end()) {
        vec.erase(it);
        return true;
    }
    return false;
}

template<std::ranges::range T, typename U>
[[nodiscard]] constexpr auto vec_filter(T&& cont, const U& filter) -> vector<std::ranges::range_value_t<T>> // NOLINT(cppcoreguidelines-missing-std-forward)
{
    vector<std::ranges::range_value_t<T>> vec;
    vec.reserve(cont.size());
    for (auto&& value : cont) {
        if (static_cast<bool>(filter(value))) {
            vec.emplace_back(value);
        }
    }
    return vec;
}

template<std::ranges::range T, typename U>
[[nodiscard]] constexpr auto vec_transform(T&& cont, const U& transfromer) -> auto // NOLINT(cppcoreguidelines-missing-std-forward)
{
    using result_type = std::remove_cvref_t<std::invoke_result_t<U, std::ranges::range_reference_t<T>>>;
    vector<result_type> vec;
    vec.reserve(cont.size());
    for (auto&& value : cont) {
        vec.emplace_back(transfromer(value));
    }
    return vec;
}

template<std::ranges::range T, typename U>
[[nodiscard]] constexpr auto vec_exists(T&& cont, const U& value) noexcept -> bool // NOLINT(cppcoreguidelines-missing-std-forward)
{
    for (auto it = cont.begin(); it != cont.end(); ++it) {
        if (*it == value) {
            return true;
        }
    }
    return false;
}

template<std::ranges::range T, typename U>
[[nodiscard]] constexpr auto vec_sorted(T&& cont, const U& predicate) noexcept -> vector<std::ranges::range_value_t<T>> // NOLINT(cppcoreguidelines-missing-std-forward)
{
    vector<std::ranges::range_value_t<T>> vec;
    vec.reserve(cont.size());
    vec.assign(cont.begin(), cont.end());
    std::ranges::stable_sort(vec, predicate);
    return vec;
}

template<std::ranges::range T>
[[nodiscard]] constexpr auto to_vector(T&& cont) -> vector<std::ranges::range_value_t<T>> // NOLINT(cppcoreguidelines-missing-std-forward)
{
    if constexpr (std::same_as<std::remove_cvref_t<T>, vector<std::ranges::range_value_t<T>>>) {
        if (std::is_rvalue_reference_v<T>) {
            return cont;
        }
        else {
            return copy(cont);
        }
    }
    else {
        vector<std::ranges::range_value_t<T>> vec;
        vec.reserve(cont.size());
        vec.assign(cont.begin(), cont.end());
        return vec;
    }
}

template<typename T>
    requires(!std::is_void_v<T>)
[[nodiscard]] inline auto make_span(T* data, size_t byte_size) noexcept -> span<std::conditional_t<std::is_const_v<T>, const uint8_t, uint8_t>>
{
    using byte_type = std::conditional_t<std::is_const_v<T>, const uint8_t, uint8_t>;
    return span<byte_type> {reinterpret_cast<byte_type*>(data), byte_size};
}

[[nodiscard]] inline auto make_span(void* data, size_t byte_size) noexcept -> span<uint8_t>
{
    return span<uint8_t> {cast_from_void<uint8_t*>(data).get(), byte_size};
}

[[nodiscard]] inline auto make_span(const void* data, size_t byte_size) noexcept -> const_span<uint8_t>
{
    return const_span<uint8_t> {cast_from_void<const uint8_t*>(data).get(), byte_size};
}

template<typename P>
    requires(!std::is_pointer_v<std::remove_reference_t<P>> && requires(P&& p) { p.get(); })
[[nodiscard]] inline auto make_span(P&& data, size_t byte_size) noexcept
{
    return make_span(data.get(), byte_size);
}

template<typename R>
    requires(std::ranges::contiguous_range<R> && std::is_trivially_copyable_v<std::ranges::range_value_t<R>>)
[[nodiscard]] inline auto make_span(R&& range) noexcept
{
    return make_span(std::ranges::data(range), std::ranges::size(range) * sizeof(std::ranges::range_value_t<R>));
}

template<typename T>
[[nodiscard]] inline auto make_span(ptr<T> data, size_t length) noexcept -> span<T>
{
    return {data.get(), length};
}

template<typename T>
    requires(!std::is_void_v<T>)
[[nodiscard]] inline auto make_const_span(const T* data, size_t byte_size) noexcept -> const_span<uint8_t>
{
    return const_span<uint8_t> {reinterpret_cast<const uint8_t*>(data), byte_size};
}

[[nodiscard]] inline auto make_const_span(const void* data, size_t byte_size) noexcept -> const_span<uint8_t>
{
    return make_span(data, byte_size);
}

template<typename P>
    requires(!std::is_pointer_v<std::remove_reference_t<P>> && requires(P&& p) { p.get(); })
[[nodiscard]] inline auto make_const_span(P&& data, size_t byte_size) noexcept -> const_span<uint8_t>
{
    return make_const_span(data.get(), byte_size);
}

template<typename R>
    requires(std::ranges::contiguous_range<R> && std::is_trivially_copyable_v<std::ranges::range_value_t<R>>)
[[nodiscard]] inline auto make_const_span(R&& range) noexcept -> const_span<uint8_t>
{
    return make_const_span(std::ranges::data(range), std::ranges::size(range) * sizeof(std::ranges::range_value_t<R>));
}

[[nodiscard]] inline auto span_to_string(const_span<uint8_t> bytes) noexcept -> string_view
{
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

template<typename T>
[[nodiscard]] inline auto bytes_to_objects(span<uint8_t> data) noexcept -> span<T>
{
    if (data.empty()) {
        return {};
    }

    FO_STRONG_ASSERT(data.size() % sizeof(T) == 0, "Byte span size is not a whole multiple of the object size");
    auto bytes = make_nptr(data.data());
    FO_STRONG_ASSERT(bytes, "Byte span has a null pointer");
    ptr<T> values = bytes.reinterpret_as<T>();
    return make_span(values, data.size() / sizeof(T));
}

template<typename T>
[[nodiscard]] inline auto object_to_bytes(T& object) noexcept -> span<uint8_t>
{
    auto object_ptr = make_ptr(&object);
    ptr<uint8_t> bytes = object_ptr.template reinterpret_as<uint8_t>();
    return make_span(bytes, sizeof(T));
}

template<typename T>
[[nodiscard]] inline auto object_to_bytes(const T& object) noexcept -> const_span<uint8_t>
{
    auto object_ptr = make_ptr(&object);
    ptr<const uint8_t> bytes = object_ptr.template reinterpret_as<uint8_t>();
    return make_span(bytes, sizeof(T));
}

template<typename T>
[[nodiscard]] inline auto get_object_byte(T& object, size_t index) noexcept -> uint8_t
{
    span<uint8_t> bytes = object_to_bytes(object);
    FO_STRONG_ASSERT(index < bytes.size(), "Object byte index is out of range");
    return bytes[index];
}

template<typename T>
inline void set_object_byte(T& object, size_t index, uint8_t value) noexcept
{
    span<uint8_t> bytes = object_to_bytes(object);
    FO_STRONG_ASSERT(index < bytes.size(), "Object byte index is out of range");
    bytes[index] = value;
}

template<typename T>
inline void swap_object_bytes(T& object, size_t first_index, size_t second_index) noexcept
{
    span<uint8_t> bytes = object_to_bytes(object);
    FO_STRONG_ASSERT(first_index < bytes.size(), "First object byte index is out of range");
    FO_STRONG_ASSERT(second_index < bytes.size(), "Second object byte index is out of range");
    std::swap(bytes[first_index], bytes[second_index]);
}

FO_END_NAMESPACE
