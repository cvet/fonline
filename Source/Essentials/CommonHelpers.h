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
inline auto dynamic_ptr_cast(unique_ptr<U>&& p) noexcept -> unique_ptr<T>
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
class ref_hold_vector : public vector<T>
{
public:
    static_assert(!std::is_polymorphic_v<vector<T>>);
    ref_hold_vector() noexcept = default;
    ref_hold_vector(const ref_hold_vector&) = delete;
    ref_hold_vector(ref_hold_vector&&) noexcept = default;
    auto operator=(const ref_hold_vector&) -> ref_hold_vector& = delete;
    auto operator=(ref_hold_vector&&) noexcept -> ref_hold_vector& = delete;
    ~ref_hold_vector()
    {
        for (auto&& ref : *this) {
            ref->Release();
        }
    }
};

template<typename T>
[[nodiscard]] constexpr auto copy_hold_ref(const vector<T>& value) -> ref_hold_vector<T>
{
    ref_hold_vector<T> ref_vec;
    ref_vec.reserve(value.size());
    for (auto&& ref : value) {
        FO_RUNTIME_ASSERT(ref);
        ref->AddRef();
        ref_vec.emplace_back(ref);
    }
    return ref_vec;
}

template<typename T, typename U>
[[nodiscard]] constexpr auto copy_hold_ref(const unordered_map<T, U>& value) -> ref_hold_vector<U>
{
    ref_hold_vector<U> ref_vec;
    ref_vec.reserve(value.size());
    for (auto&& [id, ref] : value) {
        FO_RUNTIME_ASSERT(ref);
        ref->AddRef();
        ref_vec.emplace_back(ref);
    }
    return ref_vec;
}

template<typename T>
[[nodiscard]] constexpr auto copy_hold_ref(const unordered_set<T>& value) -> ref_hold_vector<T>
{
    ref_hold_vector<T> ref_vec;
    ref_vec.reserve(value.size());
    for (auto&& ref : value) {
        FO_RUNTIME_ASSERT(ref);
        ref->AddRef();
        ref_vec.emplace_back(ref);
    }
    return ref_vec;
}

template<typename T>
[[nodiscard]] constexpr auto copy_hold_ref(const unordered_set<raw_ptr<T>>& value) -> ref_hold_vector<T*>
{
    static_assert(!std::is_const_v<T>);
    ref_hold_vector<T*> ref_vec;
    ref_vec.reserve(value.size());
    for (auto&& ref : value) {
        FO_RUNTIME_ASSERT(ref);
        ref->AddRef();
        ref_vec.emplace_back(ref.get_no_const());
    }
    return ref_vec;
}

// Vector helpers
template<typename T, typename T2>
[[nodiscard]] constexpr auto vec_static_cast(const vector<T2>& vec) -> vector<T>
{
    vector<T> result;
    result.reserve(vec.size());
    for (auto&& v : vec) {
        result.emplace_back(static_cast<T>(v));
    }
    return result;
}

template<typename T, typename T2>
[[nodiscard]] constexpr auto vec_dynamic_cast(const vector<T2>& value) -> vector<T>
{
    vector<T> result;
    result.reserve(value.size());
    for (auto&& v : value) {
        if (auto* casted = dynamic_cast<T>(v); casted != nullptr) {
            result.emplace_back(casted);
        }
    }
    return result;
}

template<typename T>
constexpr void vec_add_unique_value(T& vec, typename T::value_type value)
{
    const auto it = std::find(vec.begin(), vec.end(), value);
    FO_RUNTIME_ASSERT(it == vec.end());
    vec.emplace_back(std::move(value));
}

template<typename T>
constexpr void vec_remove_unique_value(T& vec, typename T::value_type value)
{
    const auto it = std::find(vec.begin(), vec.end(), value);
    FO_RUNTIME_ASSERT(it != vec.end());
    vec.erase(it);
}

template<typename T, typename U>
[[nodiscard]] constexpr auto vec_filter(const vector<T>& vec, const U& filter) -> vector<T>
{
    vector<T> result;
    result.reserve(vec.size());
    for (const auto& value : vec) {
        if (static_cast<bool>(filter(value))) {
            result.emplace_back(value);
        }
    }
    return result;
}

template<typename T, typename U>
[[nodiscard]] constexpr auto vec_filter_first(const vector<T>& vec, const U& filter) noexcept(std::is_nothrow_invocable_v<U>) -> T
{
    for (const auto& value : vec) {
        if (static_cast<bool>(filter(value))) {
            return value;
        }
    }
    return T {};
}

template<typename T, typename U>
[[nodiscard]] constexpr auto vec_transform(T& vec, const U& transfromer) -> auto
{
    vector<decltype(transfromer(nullptr))> result;
    result.reserve(vec.size());
    for (auto&& value : vec) {
        result.emplace_back(transfromer(value));
    }
    return result;
}

template<typename T>
[[nodiscard]] constexpr auto vec_exists(const vector<T>& vec, const T& value) noexcept -> bool
{
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

template<typename T, typename U>
[[nodiscard]] constexpr auto vec_exists(const vector<T>& vec, const U& predicate) noexcept -> bool
{
    return std::find_if(vec.begin(), vec.end(), predicate) != vec.end();
}

FO_END_NAMESPACE();
