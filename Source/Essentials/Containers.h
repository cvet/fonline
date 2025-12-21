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
#include "MemorySystem.h"

FO_BEGIN_NAMESPACE();

// Basic types with safe allocator
using string = std::basic_string<char, std::char_traits<char>, SafeAllocator<char>>;
using wstring = std::basic_string<wchar_t, std::char_traits<wchar_t>, SafeAllocator<wchar_t>>;
using istringstream = std::basic_istringstream<char, std::char_traits<char>, SafeAllocator<char>>;
using ostringstream = std::basic_ostringstream<char, std::char_traits<char>, SafeAllocator<char>>;
using stringstream = std::basic_stringstream<char, std::char_traits<char>, SafeAllocator<char>>;

template<typename T>
using list = std::list<T, SafeAllocator<T>>;
template<typename T>
using deque = std::deque<T, SafeAllocator<T>>;

template<typename K, typename V, typename Cmp = std::less<>>
using map = std::map<K, V, Cmp, SafeAllocator<pair<const K, V>>>;
template<typename K, typename V, typename Cmp = std::less<>>
using multimap = std::multimap<K, V, Cmp, SafeAllocator<pair<const K, V>>>;
template<typename K, typename Cmp = std::less<>>
using set = std::set<K, Cmp, SafeAllocator<K>>;

template<typename K, typename V, typename H = FO_HASH_NAMESPACE hash<K>>
using unordered_map = ankerl::unordered_dense::segmented_map<K, V, H, std::equal_to<>, SafeAllocator<pair<K, V>>>;
template<typename K, typename V, typename H = FO_HASH_NAMESPACE hash<K>>
using unordered_multimap = std::unordered_multimap<K, V, H, std::equal_to<>, SafeAllocator<pair<const K, V>>>;
template<typename K, typename H = FO_HASH_NAMESPACE hash<K>>
using unordered_set = ankerl::unordered_dense::segmented_set<K, H, std::equal_to<>, SafeAllocator<K>>;

template<typename T, unsigned InlineCapacity>
using small_vector = gch::small_vector<T, InlineCapacity, SafeAllocator<T>>;
template<typename T>
using vector = std::vector<T, SafeAllocator<T>>;

// Template helpers
template<typename T>
concept is_vector_collection = is_specialization<T, std::vector>::value || has_member<T, &T::inlined> /*small_vector test*/;
template<typename T>
concept is_map_collection = is_specialization<T, std::map>::value || is_specialization<T, std::unordered_map>::value || is_specialization<T, ankerl::unordered_dense::segmented_map>::value;

// String formatter
FO_END_NAMESPACE();
template<>
struct FO_HASH_NAMESPACE hash<FO_NAMESPACE string>
{
    using is_transparent = void;
    using is_avalanching = void;
    auto operator()(FO_NAMESPACE string_view str) const noexcept -> uint64_t { return hash<FO_NAMESPACE string_view> {}(str); }
};
FO_BEGIN_NAMESPACE();

// Vector formatter
FO_END_NAMESPACE();
template<typename T>
    requires(FO_NAMESPACE is_vector_collection<T>)
struct std::formatter<T> : formatter<FO_NAMESPACE string_view> // NOLINT(cert-dcl58-cpp)
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        FO_NAMESPACE string result;

        for (const auto& e : value) {
            if constexpr (std::same_as<T, FO_NAMESPACE vector<FO_NAMESPACE string>>) {
                result += e + " ";
            }
            else if constexpr (std::same_as<T, FO_NAMESPACE vector<bool>>) {
                result += e ? "True " : "False ";
            }
            else {
                result += std::to_string(e) + " ";
            }
        }

        if (!result.empty()) {
            result.pop_back();
        }

        return formatter<FO_NAMESPACE string_view>::format(result, ctx);
    }
};
FO_BEGIN_NAMESPACE();

FO_END_NAMESPACE();
