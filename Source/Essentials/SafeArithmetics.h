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
#include "ExceptionHandling.h"

FO_BEGIN_NAMESPACE

// Float comparator
template<typename T>
    requires(std::floating_point<T>)
[[nodiscard]] constexpr auto float_abs(T f) noexcept -> T
{
    return f < 0 ? -f : f;
}

template<typename T>
    requires(std::floating_point<T>)
[[nodiscard]] constexpr auto is_float_equal(T f1, T f2, T epsilon = 1.0e-5f) noexcept -> bool
{
    if (float_abs(f1 - f2) <= epsilon) {
        return true;
    }
    return float_abs(f1 - f2) <= epsilon * std::max(float_abs(f1), float_abs(f2));
}

// Alignment, alignment must be a power of two
template<typename T>
    requires(std::is_unsigned_v<T>)
[[nodiscard]] constexpr auto align_up(T value, T alignment) noexcept -> T
{
    return (value + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]] constexpr auto alignment_for_size(size_t size) noexcept -> size_t
{
    static_assert((MAX_SERIALIZED_ALIGNMENT & (MAX_SERIALIZED_ALIGNMENT - 1)) == 0);

    return size != 0 ? std::min(size & (~size + 1), MAX_SERIALIZED_ALIGNMENT) : 1;
}

// Numeric cast
FO_DECLARE_EXCEPTION(OverflowException);
FO_DECLARE_EXCEPTION(DivisionByZeroException);

template<typename T, typename U>
    requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<U>)
[[nodiscard]] constexpr auto numeric_cast(U value) -> T
{
    static_assert(!std::same_as<T, bool> && !std::same_as<U, bool>, "Bool type is not convertible");

    if constexpr (std::floating_point<T> || std::floating_point<U>) {
        static_assert(std::floating_point<T>, "Use iround for float to int conversion");
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            throw OverflowException("Numeric cast overflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::max());
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            throw OverflowException("Numeric cast overflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::max());
        }
        if (value < static_cast<U>(std::numeric_limits<T>::min())) {
            throw OverflowException("Numeric cast underflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::min());
        }
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        if (value < 0) {
            throw OverflowException("Numeric cast underflow", typeid(U).name(), typeid(T).name(), value, 0);
        }
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            throw OverflowException("Numeric cast overflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::max());
        }
        if (value < 0) {
            throw OverflowException("Numeric cast underflow", typeid(U).name(), typeid(T).name(), value, 0);
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) == sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            throw OverflowException("Numeric cast overflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::max());
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) > sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            throw OverflowException("Numeric cast overflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::max());
        }
    }
    else {
        static_assert(false);
    }

    return static_cast<T>(value);
}

template<typename T, typename U>
    requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<U>)
[[nodiscard]] constexpr auto safe_numeric_cast(U value) noexcept -> T
{
    static_assert(!std::same_as<T, bool> && !std::same_as<U, bool>, "Bool type is not convertible");

    if constexpr (std::floating_point<T> || std::floating_point<U>) {
        static_assert(std::floating_point<T>, "Use iround for float to int conversion");
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        static_assert(false, "Safe conversion not possible");
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        static_assert(false, "Safe conversion not possible");
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        static_assert(false, "Safe conversion not possible");
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        static_assert(false, "Safe conversion not possible");
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) == sizeof(U)) {
        static_assert(false, "Safe conversion not possible");
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) > sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        static_assert(false, "Safe conversion not possible");
    }
    else {
        static_assert(false);
    }

    return static_cast<T>(value);
}

template<typename T, typename U>
[[nodiscard]] consteval auto const_numeric_cast(U value) noexcept -> T // NOLINT(bugprone-exception-escape)
{
    return numeric_cast<T>(value);
}

template<typename T, typename U>
    requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<U>)
[[nodiscard]] constexpr auto clamp_to(U value) noexcept -> T
{
    static_assert(!std::same_as<T, bool> && !std::same_as<U, bool>, "Bool type is not convertible");

    if constexpr (std::floating_point<T> || std::floating_point<U>) {
        static_assert(std::floating_point<T>, "Use iround for float to int conversion");
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            value = static_cast<U>(std::numeric_limits<T>::max());
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            value = static_cast<U>(std::numeric_limits<T>::max());
        }
        if (value < static_cast<U>(std::numeric_limits<T>::min())) {
            value = static_cast<U>(std::numeric_limits<T>::min());
        }
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        if (value < 0) {
            value = 0;
        }
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            value = static_cast<U>(std::numeric_limits<T>::max());
        }
        if (value < 0) {
            value = 0;
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) == sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            value = static_cast<U>(std::numeric_limits<T>::max());
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) > sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            value = static_cast<U>(std::numeric_limits<T>::max());
        }
    }
    else {
        static_assert(false);
    }

    return static_cast<T>(value);
}

template<typename T, typename U>
    requires(std::floating_point<U> && std::integral<T>)
[[nodiscard]] constexpr auto iround(U value) -> T
{
    if (!std::isfinite(value)) {
        throw OverflowException("Floating point rounding received non-finite value", typeid(U).name(), typeid(T).name(), value);
    }

    // std::llround is undefined for values outside the int64 range, so reject them before rounding.
    // The upper bound is exclusive: casting int64 max to floating point rounds it up to 2^63, which is already unrepresentable.
    constexpr U min_bound = static_cast<U>(std::numeric_limits<int64_t>::min());
    constexpr U max_bound = static_cast<U>(std::numeric_limits<int64_t>::max());

    if (value < min_bound || value >= max_bound) {
        throw OverflowException("Floating point rounding received value out of integer range", typeid(U).name(), typeid(T).name(), value);
    }

    return numeric_cast<T>(std::llround(value));
}

// Safe arithmetics
template<typename T, typename U>
    requires(std::is_arithmetic_v<T> && std::same_as<T, U>)
[[nodiscard]] constexpr auto checked_add(const U& value1, const U& value2) -> U
{
    if constexpr (std::integral<T>) {
        if constexpr (std::is_unsigned_v<T>) {
            if (value1 > std::numeric_limits<T>::max() - value2) {
                throw OverflowException("Integer addition overflow", typeid(T).name(), value1, value2);
            }
        }
        else {
            if (value2 > 0 && value1 > std::numeric_limits<T>::max() - value2) {
                throw OverflowException("Integer addition overflow", typeid(T).name(), value1, value2);
            }
            if (value2 < 0 && value1 < std::numeric_limits<T>::min() - value2) {
                throw OverflowException("Integer addition underflow", typeid(T).name(), value1, value2);
            }
        }
    }

    return numeric_cast<T>(value1 + value2);
}

template<typename T, typename U>
    requires(std::is_arithmetic_v<T> && std::same_as<T, U>)
[[nodiscard]] constexpr auto checked_sub(const U& value1, const U& value2) -> U
{
    if constexpr (std::integral<T>) {
        if constexpr (std::is_unsigned_v<T>) {
            if (value1 < value2) {
                throw OverflowException("Integer subtraction underflow", typeid(T).name(), value1, value2);
            }
        }
        else {
            if (value2 > 0 && value1 < std::numeric_limits<T>::min() + value2) {
                throw OverflowException("Integer subtraction underflow", typeid(T).name(), value1, value2);
            }
            if (value2 < 0 && value1 > std::numeric_limits<T>::max() + value2) {
                throw OverflowException("Integer subtraction overflow", typeid(T).name(), value1, value2);
            }
        }
    }

    return numeric_cast<T>(value1 - value2);
}

template<typename T, typename U>
    requires(std::is_arithmetic_v<T> && std::same_as<T, U>)
[[nodiscard]] constexpr auto checked_mul(const U& value1, const U& value2) -> U
{
    if constexpr (std::integral<T>) {
        if constexpr (std::is_unsigned_v<T>) {
            if (value2 != 0 && value1 > std::numeric_limits<T>::max() / value2) {
                throw OverflowException("Integer multiplication overflow", typeid(T).name(), value1, value2);
            }
        }
        else {
            if (value1 > 0) {
                if (value2 > 0) {
                    if (value1 > std::numeric_limits<T>::max() / value2) {
                        throw OverflowException("Integer multiplication overflow", typeid(T).name(), value1, value2);
                    }
                }
                else if (value2 < std::numeric_limits<T>::min() / value1) {
                    throw OverflowException("Integer multiplication underflow", typeid(T).name(), value1, value2);
                }
            }
            else if (value1 < 0) {
                if (value2 > 0) {
                    if (value1 < std::numeric_limits<T>::min() / value2) {
                        throw OverflowException("Integer multiplication underflow", typeid(T).name(), value1, value2);
                    }
                }
                else if (value2 < std::numeric_limits<T>::max() / value1) {
                    throw OverflowException("Integer multiplication overflow", typeid(T).name(), value1, value2);
                }
            }
        }
    }

    return numeric_cast<T>(value1 * value2);
}

template<typename T, typename U>
    requires(std::is_arithmetic_v<T> && std::same_as<T, U>)
[[nodiscard]] constexpr auto checked_div(const U& value1, const U& value2) -> U
{
    if constexpr (std::floating_point<T>) {
        if (std::fabs(value2) < std::numeric_limits<T>::epsilon()) {
            throw DivisionByZeroException("Float division by zero", typeid(T).name(), value1, value2);
        }
    }
    else {
        if (value2 == 0) {
            throw DivisionByZeroException("Integer division by zero", typeid(T).name(), value1, value2);
        }
    }

    return numeric_cast<T>(value1 / value2);
}

// Lerp
template<typename T, typename U = std::decay_t<T>>
    requires(std::floating_point<U>)
[[nodiscard]] constexpr auto lerp(T v1, T v2, float32_t t) -> U
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : v1 + (v2 - v1) * t);
}

template<typename T, typename U = std::decay_t<T>>
    requires(std::signed_integral<U>)
[[nodiscard]] constexpr auto lerp(T v1, T v2, float32_t t) -> U
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : v1 + iround<U>((v2 - v1) * t));
}

template<typename T, typename U = std::decay_t<T>>
    requires(std::unsigned_integral<U>)
[[nodiscard]] constexpr auto lerp(T v1, T v2, float32_t t) -> U
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : iround<U>(v1 * (1 - t) + v2 * t));
}

FO_END_NAMESPACE
