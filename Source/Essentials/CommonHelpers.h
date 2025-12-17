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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ExceptionHadling.h"
#include "MemorySystem.h"
#include "SmartPointers.h"
#include "StackTrace.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE();

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
template<typename T, typename U>
inline auto dynamic_ptr_cast(unique_ptr<U>&& p) noexcept -> unique_ptr<T> // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
{
    if (T* casted = dynamic_cast<T*>(p.get())) {
        (void)p.release();
        return unique_ptr<T> {casted};
    }
    return {};
}

template<typename T, typename U>
inline auto dynamic_ptr_cast(shared_ptr<U> p) noexcept -> shared_ptr<T>
{
    return std::dynamic_pointer_cast<T>(p.get_underlying());
}

template<typename T>
inline void make_if_not_exists(unique_ptr<T>& ptr)
{
    if (!ptr) {
        ptr = SafeAlloc::MakeUnique<T>();
    }
}

template<typename T>
inline void destroy_if_empty(unique_ptr<T>& ptr) noexcept
{
    if (ptr && ptr->empty()) {
        ptr.reset();
    }
}

// Ref holders
template<typename T>
class ref_hold_vector
{
public:
    explicit ref_hold_vector(size_t capacity) { _vec.reserve(capacity); }
    explicit ref_hold_vector(vector<T>&& vec) noexcept :
        _vec {std::move(vec)}
    {
    }

    ref_hold_vector(const ref_hold_vector&) = delete;
    ref_hold_vector(ref_hold_vector&&) noexcept = default;
    auto operator=(const ref_hold_vector&) -> ref_hold_vector& = delete;
    auto operator=(ref_hold_vector&&) noexcept -> ref_hold_vector& = delete;

    [[nodiscard]] constexpr auto begin() noexcept { return _vec.begin(); }
    [[nodiscard]] constexpr auto end() noexcept { return _vec.end(); }
    [[nodiscard]] constexpr auto cbegin() const noexcept { return _vec.cbegin(); }
    [[nodiscard]] constexpr auto cend() const noexcept { return _vec.cend(); }

    ~ref_hold_vector()
    {
        for (auto&& ref : _vec) {
            ref->Release();
        }
    }

    void add(const T& ref)
    {
        FO_RUNTIME_ASSERT(ref);
        ref->AddRef();
        _vec.emplace_back(ref);
    }

private:
    vector<T> _vec {};
};

template<typename T>
[[nodiscard]] constexpr auto copy_hold_ref(const vector<T>& value) -> ref_hold_vector<T>
{
    auto ref_vec = ref_hold_vector<T>(value.size());
    for (auto&& ref : value) {
        ref_vec.add(ref);
    }
    return ref_vec;
}

template<typename T>
[[nodiscard]] constexpr auto copy_hold_ref(vector<T>&& value) -> ref_hold_vector<T>
{
    auto ref_vec = ref_hold_vector<T>(std::move(value));
    for (auto&& ref : value) {
        ref_vec.add(ref);
    }
    return ref_vec;
}

template<typename T, typename U>
[[nodiscard]] constexpr auto copy_hold_ref(unordered_map<T, U>& value) -> ref_hold_vector<U>
{
    auto ref_vec = ref_hold_vector<U>(value.size());
    for (auto&& ref : value | std::views::values) {
        ref_vec.add(ref);
    }
    return ref_vec;
}

template<typename T, typename U>
[[nodiscard]] constexpr auto copy_hold_ref(unordered_map<T, raw_ptr<U>>& value) -> ref_hold_vector<U*>
{
    auto ref_vec = ref_hold_vector<U*>(value.size());
    for (auto&& ref : value | std::views::values) {
        ref_vec.add(ref.get());
    }
    return ref_vec;
}

template<typename T>
[[nodiscard]] constexpr auto copy_hold_ref(unordered_set<T>& value) -> ref_hold_vector<T>
{
    auto ref_vec = ref_hold_vector<T>(value.size());
    for (auto&& ref : value) {
        ref_vec.add(ref);
    }
    return ref_vec;
}

template<typename T>
[[nodiscard]] constexpr auto copy_hold_ref(unordered_set<raw_ptr<T>>& value) -> ref_hold_vector<T*>
{
    static_assert(!std::is_const_v<T>);
    auto ref_vec = ref_hold_vector<T*>(value.size());
    for (auto&& ref : value) {
        ref_vec.add(ref.get());
    }
    return ref_vec;
}

template<typename T>
[[nodiscard]] constexpr auto copy_hold_ref(span<raw_ptr<T>>&& value) -> ref_hold_vector<T*> // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
{
    static_assert(!std::is_const_v<T>);
    auto ref_vec = ref_hold_vector<T*>(value.size());
    for (auto&& ref : value) {
        ref_vec.add(ref.get());
    }
    return ref_vec;
}

template<typename T>
[[nodiscard]] constexpr auto copy_hold_ref(span<refcount_ptr<T>>&& value) -> ref_hold_vector<T*> // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
{
    static_assert(!std::is_const_v<T>);
    auto ref_vec = ref_hold_vector<T*>(value.size());
    for (auto&& ref : value) {
        ref_vec.add(ref.get());
    }
    return ref_vec;
}

// Vector helpers
template<std::ranges::range T>
constexpr void vec_add_unique_value(T& vec, typename T::value_type value)
{
    const auto it = std::ranges::find(vec, value);
    FO_RUNTIME_ASSERT(it == vec.end());
    vec.emplace_back(std::move(value));
}

template<std::ranges::range T>
constexpr void vec_remove_unique_value(T& vec, typename T::value_type value)
{
    const auto it = std::ranges::find(vec, value);
    FO_RUNTIME_ASSERT(it != vec.end());
    vec.erase(it);
}

template<std::ranges::range T, typename U>
constexpr void vec_remove_unique_value_if(T& vec, const U& predicate)
{
    const auto it = std::ranges::find_if(vec, predicate);
    FO_RUNTIME_ASSERT(it != vec.end());
    vec.erase(it);
}

template<std::ranges::range T>
constexpr auto vec_safe_add_unique_value(T& vec, typename T::value_type value) noexcept -> bool
{
    if (const auto it = std::ranges::find(vec, value); it == vec.end()) {
        vec.emplace_back(std::move(value));
        return true;
    }
    return false;
}

template<std::ranges::range T>
constexpr auto vec_safe_remove_unique_value(T& vec, typename T::value_type value) noexcept -> bool
{
    if (const auto it = std::ranges::find(vec, value); it != vec.end()) {
        vec.erase(it);
        return true;
    }
    return false;
}

template<std::ranges::range T, typename U>
constexpr auto vec_safe_remove_unique_value_if(T& vec, const U& predicate) noexcept -> bool
{
    if (const auto it = std::ranges::find_if(vec, predicate); it != vec.end()) {
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
    vector<decltype(transfromer(nullptr))> vec;
    vec.reserve(cont.size());
    for (auto&& value : cont) {
        vec.emplace_back(transfromer(value));
    }
    return vec;
}

template<std::ranges::range T, typename U>
[[nodiscard]] constexpr auto vec_exists(T&& cont, const U& value) noexcept -> bool // NOLINT(cppcoreguidelines-missing-std-forward)
{
    return std::ranges::find(cont, value) != cont.end();
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
auto to_vector(T&& cont) -> vector<std::ranges::range_value_t<T>> // NOLINT(cppcoreguidelines-missing-std-forward)
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

FO_END_NAMESPACE();
