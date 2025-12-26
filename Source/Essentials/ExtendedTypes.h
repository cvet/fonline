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
#include "SafeArithmetics.h"
#include "StrongType.h"

FO_BEGIN_NAMESPACE();

template<typename T>
concept pos_type = requires(T t) {
    requires std::is_arithmetic_v<decltype(t.x)>;
    requires std::is_arithmetic_v<decltype(t.y)>;
    requires std::is_same_v<decltype(t.x), decltype(t.y)>;
};

template<typename T>
concept size_type = requires(T t) {
    requires std::is_arithmetic_v<decltype(t.width)>;
    requires std::is_arithmetic_v<decltype(t.height)>;
    requires std::is_same_v<decltype(t.width), decltype(t.height)>;
};

template<typename T>
concept rect_type = requires(T t) {
    requires std::is_arithmetic_v<decltype(t.x)>;
    requires std::is_arithmetic_v<decltype(t.y)>;
    requires std::is_arithmetic_v<decltype(t.width)>;
    requires std::is_arithmetic_v<decltype(t.height)>;
    requires std::is_same_v<decltype(t.x), decltype(t.y)>;
    requires std::is_same_v<decltype(t.width), decltype(t.height)>;
};

// Color type
///@ ExportValueType ucolor ucolor Layout = uint32-value
struct ucolor
{
    using underlying_type = uint32;

    constexpr ucolor() noexcept :
        rgba {}
    {
    }
    constexpr explicit ucolor(uint32 rgba_) noexcept :
        rgba {rgba_}
    {
    }
    constexpr ucolor(uint8 r_, uint8 g_, uint8 b_) noexcept :
        comp {.r = r_, .g = g_, .b = b_, .a = 255}
    {
    }
    constexpr ucolor(uint8 r_, uint8 g_, uint8 b_, uint8 a_) noexcept :
        comp {.r = r_, .g = g_, .b = b_, .a = a_}
    {
    }
    constexpr explicit ucolor(const ucolor& other, uint8 a_) noexcept :
        rgba {other.rgba}
    {
        comp.a = a_;
    }

    [[nodiscard]] constexpr auto operator==(const ucolor& other) const noexcept -> bool { return rgba == other.rgba; }
    [[nodiscard]] constexpr auto operator<(const ucolor& other) const noexcept -> bool { return rgba < other.rgba; }
    [[nodiscard]] constexpr auto underlying_value() noexcept -> underlying_type& { return rgba; }
    [[nodiscard]] constexpr auto underlying_value() const noexcept -> const underlying_type& { return rgba; }

    struct components
    {
        uint8 r;
        uint8 g;
        uint8 b;
        uint8 a;
    };

    union
    {
        uint32 rgba;
        components comp;
    };

    static const ucolor clear;
};
static_assert(is_strong_type<ucolor>);
FO_DECLARE_TYPE_HASHER_EXT(FO_NAMESPACE ucolor, v.underlying_value());

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE ucolor> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE ucolor& value, FormatContext& ctx) const
    {
        FO_NAMESPACE string buf;
        std::format_to(std::back_inserter(buf), "0x{:x}", value.rgba);
        return formatter<FO_NAMESPACE string_view>::format(buf, ctx);
    }
};
FO_BEGIN_NAMESPACE();

// Position types
template<std::integral T>
struct ipos
{
    constexpr ipos() noexcept = default;
    constexpr ipos(T x_, T y_) noexcept :
        x {x_},
        y {y_}
    {
    }
    explicit constexpr ipos(pos_type auto other) noexcept :
        x {safe_numeric_cast<T>(other.x)},
        y {safe_numeric_cast<T>(other.y)}
    {
    }

    constexpr auto operator+=(const ipos& other) noexcept -> ipos&
    {
        x += other.x;
        y += other.y;
        return *this;
    }
    constexpr auto operator-=(const ipos& other) noexcept -> ipos&
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    constexpr auto operator*=(const ipos& other) noexcept -> ipos&
    {
        x *= other.x;
        y *= other.y;
        return *this;
    }
    constexpr auto operator/=(const ipos& other) noexcept -> ipos&
    {
        x /= other.x;
        y /= other.y;
        return *this;
    }
    constexpr auto operator+=(const T& other) noexcept -> ipos&
    {
        x += other;
        y += other;
        return *this;
    }
    constexpr auto operator-=(const T& other) noexcept -> ipos&
    {
        x -= other;
        y -= other;
        return *this;
    }
    constexpr auto operator*=(const T& other) noexcept -> ipos&
    {
        x *= other;
        y *= other;
        return *this;
    }
    constexpr auto operator/=(const T& other) noexcept -> ipos&
    {
        x /= other;
        y /= other;
        return *this;
    }

    [[nodiscard]] constexpr auto operator==(const ipos& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator<(const ipos& other) const noexcept -> bool { return std::tie(x, y) < std::tie(other.x, other.y); }
    [[nodiscard]] constexpr auto operator+(const ipos& other) const noexcept -> ipos { return {x + other.x, y + other.y}; }
    [[nodiscard]] constexpr auto operator-(const ipos& other) const noexcept -> ipos { return {x - other.x, y - other.y}; }
    [[nodiscard]] constexpr auto operator*(const ipos& other) const noexcept -> ipos { return {x * other.x, y * other.y}; }
    [[nodiscard]] constexpr auto operator/(const ipos& other) const noexcept -> ipos { return {x / other.x, y / other.y}; }
    [[nodiscard]] constexpr auto operator+(const T& other) const noexcept -> ipos { return {x + other, y + other}; }
    [[nodiscard]] constexpr auto operator-(const T& other) const noexcept -> ipos { return {x - other, y - other}; }
    [[nodiscard]] constexpr auto operator*(const T& other) const noexcept -> ipos { return {x * other, y * other}; }
    [[nodiscard]] constexpr auto operator/(const T& other) const noexcept -> ipos { return {x / other, y / other}; }
    [[nodiscard]] constexpr auto operator-() const noexcept -> ipos { return {-x, -y}; }
    // [[nodiscard]] constexpr auto operator+(const ipos& other) const -> ipos { return {checked_add(x, other.x), checked_add(y, other.y)}; }
    // [[nodiscard]] constexpr auto operator-(const ipos& other) const -> ipos { return {checked_sub(x, other.x), checked_sub(y, other.y)}; }
    // [[nodiscard]] constexpr auto operator*(const ipos& other) const -> ipos { return {checked_mul(x, other.x), checked_mul(y, other.y)}; }
    // [[nodiscard]] constexpr auto operator/(const ipos& other) const -> ipos { return {checked_div(x, other.x), checked_div(y, other.y)}; }

    [[nodiscard]] auto idist() const -> T { return iround<T>(std::sqrt(numeric_cast<float64>(x * x + y * y))); }
    [[nodiscard]] auto is_zero() const noexcept -> bool { return x == 0 && y == 0; }

    T x {};
    T y {};
};

template<std::integral T>
struct isize
{
    constexpr isize() noexcept = default;
    constexpr isize(T width_, T height_) noexcept :
        width {width_},
        height {height_}
    {
    }

    constexpr auto operator+=(const isize& other) noexcept -> isize&
    {
        width += other.width;
        height += other.height;
        return *this;
    }
    constexpr auto operator-=(const isize& other) noexcept -> isize&
    {
        width -= other.width;
        height -= other.height;
        return *this;
    }
    constexpr auto operator*=(const isize& other) noexcept -> isize&
    {
        width *= other.width;
        height *= other.height;
        return *this;
    }
    constexpr auto operator/=(const isize& other) noexcept -> isize&
    {
        width /= other.width;
        height /= other.height;
        return *this;
    }
    constexpr auto operator+=(const T& other) noexcept -> isize&
    {
        width += other;
        height += other;
        return *this;
    }
    constexpr auto operator-=(const T& other) noexcept -> isize&
    {
        width -= other;
        height -= other;
        return *this;
    }
    constexpr auto operator*=(const T& other) noexcept -> isize&
    {
        width *= other;
        height *= other;
        return *this;
    }
    constexpr auto operator/=(const T& other) noexcept -> isize&
    {
        width /= other;
        height /= other;
        return *this;
    }

    [[nodiscard]] constexpr auto operator==(const isize& other) const noexcept -> bool { return width == other.width && height == other.height; }
    [[nodiscard]] constexpr auto operator<(const isize& other) const noexcept -> bool { return std::tie(width, height) < std::tie(other.width, other.height); }
    [[nodiscard]] constexpr auto operator+(const isize& other) const noexcept -> isize { return {width + other.width, height + other.height}; }
    [[nodiscard]] constexpr auto operator-(const isize& other) const noexcept -> isize { return {width - other.width, height - other.height}; }
    [[nodiscard]] constexpr auto operator*(const isize& other) const noexcept -> isize { return {width * other.width, height * other.height}; }
    [[nodiscard]] constexpr auto operator/(const isize& other) const noexcept -> isize { return {width / other.width, height / other.height}; }
    [[nodiscard]] constexpr auto operator+(const T& other) const noexcept -> isize { return {width + other, height + other}; }
    [[nodiscard]] constexpr auto operator-(const T& other) const noexcept -> isize { return {width - other, height - other}; }
    [[nodiscard]] constexpr auto operator*(const T& other) const noexcept -> isize { return {width * other, height * other}; }
    [[nodiscard]] constexpr auto operator/(const T& other) const noexcept -> isize { return {width / other, height / other}; }
    [[nodiscard]] constexpr auto operator-() const noexcept -> isize { return {-width, -height}; }

    [[nodiscard]] constexpr auto square() const noexcept -> size_t { return width * height; }
    [[nodiscard]] constexpr auto is_valid_pos(std::integral auto x, std::integral auto y) const noexcept -> bool
    { //
        return std::cmp_greater_equal(x, 0) && std::cmp_greater_equal(y, 0) && std::cmp_less(x, width) && std::cmp_less(y, height);
    }
    [[nodiscard]] constexpr auto is_valid_pos(pos_type auto pos) const noexcept -> bool { return is_valid_pos(pos.x, pos.y); }
    [[nodiscard]] auto is_zero() const noexcept -> bool { return width == 0 && height == 0; }

    T width {};
    T height {};
};

template<std::integral T>
struct irect
{
    constexpr irect() noexcept = default;
    constexpr irect(ipos<T> pos, isize<T> size) noexcept :
        x {pos.x},
        y {pos.y},
        width {size.width},
        height {size.height}
    {
    }
    constexpr irect(T x_, T y_, isize<T> size) noexcept :
        x {x_},
        y {y_},
        width {size.width},
        height {size.height}
    {
    }
    constexpr irect(ipos<T> pos, T width_, T height_) noexcept :
        x {pos.x},
        y {pos.y},
        width {width_},
        height {height_}
    {
    }
    constexpr irect(T x_, T y_, T width_, T height_) noexcept :
        x {x_},
        y {y_},
        width {width_},
        height {height_}
    {
    }
    [[nodiscard]] constexpr auto operator==(const irect& other) const noexcept -> bool { return x == other.x && y == other.y && width == other.width && height == other.height; }
    [[nodiscard]] constexpr auto operator<(const irect& other) const noexcept -> bool { return std::tie(x, y, width, height) < std::tie(other.x, other.y, other.width, other.height); }
    [[nodiscard]] auto pos() const noexcept -> ipos<T> { return ipos<T>(x, y); }
    [[nodiscard]] auto size() const noexcept -> isize<T> { return isize<T>(width, height); }
    [[nodiscard]] auto is_zero() const noexcept -> bool { return x == 0 && y == 0 && width == 0 && height == 0; }

    void expand(const irect& other) noexcept { *this = expanded(other); }
    [[nodiscard]] auto expanded(const irect& other) const noexcept -> irect
    {
        const T x1 = std::min(x, other.x);
        const T y1 = std::min(y, other.y);
        const T x2 = std::max(x + width, other.x + other.width);
        const T y2 = std::max(y + height, other.y + other.height);
        return irect(x1, y1, x2 - y2, y2 - y1);
    }

    T x {};
    T y {};
    T width {};
    T height {};
};

// Floating types
template<std::floating_point T>
struct fpos
{
    constexpr fpos() noexcept = default;
    constexpr fpos(T x_, T y_) noexcept :
        x {x_},
        y {y_}
    {
    }
    constexpr fpos(int32 x_, int32 y_) noexcept :
        x {safe_numeric_cast<T>(x_)},
        y {safe_numeric_cast<T>(y_)}
    {
    }
    constexpr explicit fpos(pos_type auto pos) noexcept :
        x {safe_numeric_cast<T>(pos.x)},
        y {safe_numeric_cast<T>(pos.y)}
    {
    }

    constexpr auto operator+=(const fpos& other) noexcept -> fpos&
    {
        x += other.x;
        y += other.y;
        return *this;
    }
    constexpr auto operator-=(const fpos& other) noexcept -> fpos&
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    constexpr auto operator*=(const fpos& other) noexcept -> fpos&
    {
        x *= other.x;
        y *= other.y;
        return *this;
    }
    constexpr auto operator/=(const fpos& other) noexcept -> fpos&
    {
        x /= other.x;
        y /= other.y;
        return *this;
    }
    constexpr auto operator+=(const T& other) noexcept -> fpos&
    {
        x += other;
        y += other;
        return *this;
    }
    constexpr auto operator-=(const T& other) noexcept -> fpos&
    {
        x -= other;
        y -= other;
        return *this;
    }
    constexpr auto operator*=(const T& other) noexcept -> fpos&
    {
        x *= other;
        y *= other;
        return *this;
    }
    constexpr auto operator/=(const T& other) noexcept -> fpos&
    {
        x /= other;
        y /= other;
        return *this;
    }

    [[nodiscard]] constexpr auto operator==(const fpos& other) const noexcept -> bool { return is_float_equal(x, other.x) && is_float_equal(y, other.y); }
    [[nodiscard]] constexpr auto operator<(const fpos& other) const noexcept -> bool { return std::tie(x, y) < std::tie(other.x, other.y); }
    [[nodiscard]] constexpr auto operator+(const fpos& other) const noexcept -> fpos { return {x + other.x, y + other.y}; }
    [[nodiscard]] constexpr auto operator-(const fpos& other) const noexcept -> fpos { return {x - other.x, y - other.y}; }
    [[nodiscard]] constexpr auto operator*(const fpos& other) const noexcept -> fpos { return {x * other.x, y * other.y}; }
    [[nodiscard]] constexpr auto operator/(const fpos& other) const noexcept -> fpos { return {x / other.x, y / other.y}; }
    [[nodiscard]] constexpr auto operator+(const T& other) const noexcept -> fpos { return {x + other, y + other}; }
    [[nodiscard]] constexpr auto operator-(const T& other) const noexcept -> fpos { return {x - other, y - other}; }
    [[nodiscard]] constexpr auto operator*(const T& other) const noexcept -> fpos { return {x * other, y * other}; }
    [[nodiscard]] constexpr auto operator/(const T& other) const noexcept -> fpos { return {x / other, y / other}; }
    [[nodiscard]] constexpr auto operator-() const noexcept -> fpos { return {-x, -y}; }

    [[nodiscard]] auto dist() const noexcept -> T { return std::sqrt(x * x + y * y); }

    template<std::integral U>
    [[nodiscard]] auto round() const -> ipos<U>
    {
        return ipos<U>(iround<U>(x), iround<U>(y));
    }

    T x {};
    T y {};
};

template<std::floating_point T>
struct fsize
{
    constexpr fsize() noexcept = default;
    constexpr fsize(T width_, T height_) noexcept :
        width {width_},
        height {height_}
    {
    }
    constexpr fsize(std::integral auto width_, std::integral auto height_) noexcept :
        width {safe_numeric_cast<T>(width_)},
        height {safe_numeric_cast<T>(height_)}
    {
    }
    constexpr explicit fsize(size_type auto size) noexcept :
        width {safe_numeric_cast<T>(size.width)},
        height {safe_numeric_cast<T>(size.height)}
    {
    }

    constexpr auto operator+=(const fsize& other) noexcept -> fsize&
    {
        width += other.width;
        height += other.height;
        return *this;
    }
    constexpr auto operator-=(const fsize& other) noexcept -> fsize&
    {
        width -= other.width;
        height -= other.height;
        return *this;
    }
    constexpr auto operator*=(const fsize& other) noexcept -> fsize&
    {
        width *= other.width;
        height *= other.height;
        return *this;
    }
    constexpr auto operator/=(const fsize& other) noexcept -> fsize&
    {
        width /= other.width;
        height /= other.height;
        return *this;
    }
    constexpr auto operator+=(const T& other) noexcept -> fsize&
    {
        width += other;
        height += other;
        return *this;
    }
    constexpr auto operator-=(const T& other) noexcept -> fsize&
    {
        width -= other;
        height -= other;
        return *this;
    }
    constexpr auto operator*=(const T& other) noexcept -> fsize&
    {
        width *= other;
        height *= other;
        return *this;
    }
    constexpr auto operator/=(const T& other) noexcept -> fsize&
    {
        width /= other;
        height /= other;
        return *this;
    }

    [[nodiscard]] constexpr auto operator==(const fsize& other) const noexcept -> bool { return is_float_equal(width, other.width) && is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto operator<(const fsize& other) const noexcept -> bool { return std::tie(width, height) < std::tie(other.width, other.height); }
    [[nodiscard]] constexpr auto operator+(const fsize& other) const noexcept -> fsize { return {width + other.width, height + other.height}; }
    [[nodiscard]] constexpr auto operator-(const fsize& other) const noexcept -> fsize { return {width - other.width, height - other.height}; }
    [[nodiscard]] constexpr auto operator*(const fsize& other) const noexcept -> fsize { return {width * other.width, height * other.height}; }
    [[nodiscard]] constexpr auto operator/(const fsize& other) const noexcept -> fsize { return {width / other.width, height / other.height}; }
    [[nodiscard]] constexpr auto operator+(const T& other) const noexcept -> fsize { return {width + other, height + other}; }
    [[nodiscard]] constexpr auto operator-(const T& other) const noexcept -> fsize { return {width - other, height - other}; }
    [[nodiscard]] constexpr auto operator*(const T& other) const noexcept -> fsize { return {width * other, height * other}; }
    [[nodiscard]] constexpr auto operator/(const T& other) const noexcept -> fsize { return {width / other, height / other}; }
    [[nodiscard]] constexpr auto operator-() const noexcept -> fsize { return {-width, -height}; }

    [[nodiscard]] constexpr auto square() const noexcept -> T { return width * height; }
    [[nodiscard]] constexpr auto is_valid_pos(pos_type auto pos) const noexcept -> bool
    { //
        return std::cmp_greater_equal(pos.x, 0) && std::cmp_greater_equal(pos.y, 0) && std::cmp_less(pos.x, width) && std::cmp_less(pos.y, height);
    }

    T width {};
    T height {};
};

template<typename T>
struct frect
{
    constexpr frect() noexcept = default;
    constexpr frect(fpos<T> pos, fsize<T> size) noexcept :
        x {pos.x},
        y {pos.y},
        width {size.width},
        height {size.height}
    {
    }
    constexpr frect(T x_, T y_, fsize<T> size) noexcept :
        x {x_},
        y {y_},
        width {size.width},
        height {size.height}
    {
    }
    constexpr frect(fpos<T> pos, T width_, T height_) noexcept :
        x {pos.x},
        y {pos.y},
        width {width_},
        height {height_}
    {
    }
    constexpr frect(T x_, T y_, T width_, T height_) noexcept :
        x {x_},
        y {y_},
        width {width_},
        height {height_}
    {
    }
    constexpr frect(std::integral auto x_, std::integral auto y_, std::integral auto width_, std::integral auto height_) noexcept :
        x {safe_numeric_cast<T>(x_)},
        y {safe_numeric_cast<T>(y_)},
        width {safe_numeric_cast<T>(width_)},
        height {safe_numeric_cast<T>(height_)}
    {
    }
    constexpr explicit frect(rect_type auto size) noexcept :
        x {safe_numeric_cast<T>(size.x)},
        y {safe_numeric_cast<T>(size.y)},
        width {safe_numeric_cast<T>(size.width)},
        height {safe_numeric_cast<T>(size.height)}
    {
    }
    [[nodiscard]] constexpr auto operator==(const frect& other) const noexcept -> bool { return is_float_equal(x, other.x) && is_float_equal(y, other.y) && is_float_equal(width, other.width) && is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto operator<(const frect& other) const noexcept -> bool { return std::tie(x, y, width, height) < std::tie(other.x, other.y, other.width, other.height); }
    [[nodiscard]] auto pos() const noexcept -> fpos<T> { return fpos<T>(x, y); }
    [[nodiscard]] auto size() const noexcept -> fsize<T> { return fsize<T>(width, height); }

    void expand(const frect& other) noexcept { *this = expanded(other); }
    [[nodiscard]] auto expanded(const frect& other) const noexcept -> frect
    {
        const T x1 = std::min(x, other.x);
        const T y1 = std::min(y, other.y);
        const T x2 = std::max(x + width, other.x + other.width);
        const T y2 = std::max(y + height, other.y + other.height);
        return frect(x1, y1, x2 - y2, y2 - y1);
    }

    T x {};
    T y {};
    T width {};
    T height {};
};

///@ ExportValueType ipos8 ipos8 Layout = int8-x+int8-y
using ipos8 = ipos<int8>;
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos8, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE ipos8, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE ipos8);

///@ ExportValueType ipos16 ipos16 Layout = int16-x+int16-y
using ipos16 = ipos<int16>;
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos16, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE ipos16, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE ipos16);

///@ ExportValueType ipos ipos32 Layout = int32-x+int32-y
using ipos32 = ipos<int32>;
static_assert(sizeof(ipos32) == 8 && std ::is_standard_layout_v<ipos32>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos32, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE ipos32, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE ipos32);

///@ ExportValueType isize isize32 Layout = int32-width+int32-height
using isize32 = isize<int32>;
static_assert(sizeof(isize32) == 8 && std ::is_standard_layout_v<isize32>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE isize32, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE isize32, value.width >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE isize32);

///@ ExportValueType irect irect32 Layout = int32-x+int32-y+int32-width+int32-height
using irect32 = irect<int32>;
static_assert(sizeof(irect32) == 16 && std ::is_standard_layout_v<irect32>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE irect32, "{} {} {} {}", value.x, value.y, value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE irect32, value.x >> value.y >> value.width >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE irect32);

///@ ExportValueType fpos fpos32 Layout = float32-x+float32-y
using fpos32 = fpos<float32>;
static_assert(sizeof(fpos32) == 8 && std::is_standard_layout_v<fpos32>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE fpos32, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE fpos32, value.x >> value.y);

///@ ExportValueType fsize fsize32 Layout = float32-width+float32-height
using fsize32 = fsize<float32>;
static_assert(sizeof(fsize32) == 8 && std::is_standard_layout_v<fsize32>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE fsize32, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE fsize32, value.width >> value.height);

///@ ExportValueType frect frect32 Layout = float32-x+float32-y+float32-width+float32-height
using frect32 = frect<float32>;
static_assert(sizeof(frect32) == 16 && std::is_standard_layout_v<frect32>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE frect32, "{} {} {} {}", value.x, value.y, value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE frect32, value.x >> value.y >> value.width >> value.height);

FO_END_NAMESPACE();
