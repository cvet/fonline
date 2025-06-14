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

// ReSharper disable CppInconsistentNaming

#pragma once

#include "BasicCore.h"
#include "SafeArithmetics.h"
#include "StrongType.h"

FO_BEGIN_NAMESPACE();

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

    [[nodiscard]] constexpr auto operator==(const ucolor& other) const noexcept { return rgba == other.rgba; }
    [[nodiscard]] constexpr auto operator!=(const ucolor& other) const noexcept { return rgba != other.rgba; }
    [[nodiscard]] constexpr auto operator<(const ucolor& other) const noexcept { return rgba < other.rgba; }
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
///@ ExportValueType isize isize32 Layout = int32-width+int32-height
struct isize32
{
    constexpr isize32() noexcept = default;
    constexpr isize32(int32 width_, int32 height_) noexcept :
        width {width_},
        height {height_}
    {
    }

    [[nodiscard]] constexpr auto operator==(const isize32& other) const noexcept -> bool { return width == other.width && height == other.height; }
    [[nodiscard]] constexpr auto operator!=(const isize32& other) const noexcept -> bool { return width != other.width || height != other.height; }
    [[nodiscard]] constexpr auto GetSquare() const noexcept -> int32 { return width * height; }
    template<typename T>
    [[nodiscard]] constexpr auto IsValidPos(T pos) const noexcept -> bool
    {
        return pos.x >= 0 && pos.y >= 0 && pos.x < width && pos.y < height;
    }

    int32 width {};
    int32 height {};
};
static_assert(std::is_standard_layout_v<isize32>);
static_assert(sizeof(isize32) == 8);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE isize32, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE isize32, value.width >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE isize32);

///@ ExportValueType ipos ipos32 Layout = int32-x+int32-y
struct ipos32
{
    constexpr ipos32() noexcept = default;
    constexpr ipos32(int32 x_, int32 y_) noexcept :
        x {x_},
        y {y_}
    {
    }

    [[nodiscard]] constexpr auto operator==(const ipos32& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator!=(const ipos32& other) const noexcept -> bool { return x != other.x || y != other.y; }
    [[nodiscard]] constexpr auto operator+(const ipos32& other) const noexcept -> ipos32 { return {x + other.x, y + other.y}; }
    [[nodiscard]] constexpr auto operator-(const ipos32& other) const noexcept -> ipos32 { return {x - other.x, y - other.y}; }
    [[nodiscard]] constexpr auto operator*(const ipos32& other) const noexcept -> ipos32 { return {x * other.x, y * other.y}; }
    [[nodiscard]] constexpr auto operator/(const ipos32& other) const noexcept -> ipos32 { return {x / other.x, y / other.y}; }

    int32 x {};
    int32 y {};
};
static_assert(std::is_standard_layout_v<ipos32>);
static_assert(sizeof(ipos32) == 8);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos32, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE ipos32, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE ipos32);

///@ ExportValueType irect irect32 Layout = int32-x+int32-y+int32-width+int32-height
struct irect32
{
    constexpr irect32() noexcept = default;
    constexpr irect32(ipos32 pos, isize32 size) noexcept :
        x {pos.x},
        y {pos.y},
        width {size.width},
        height {size.height}
    {
    }
    constexpr irect32(int32 x_, int32 y_, isize32 size) noexcept :
        x {x_},
        y {y_},
        width {size.width},
        height {size.height}
    {
    }
    constexpr irect32(ipos32 pos, int32 width_, int32 height_) noexcept :
        x {pos.x},
        y {pos.y},
        width {width_},
        height {height_}
    {
    }
    constexpr irect32(int32 x_, int32 y_, int32 width_, int32 height_) noexcept :
        x {x_},
        y {y_},
        width {width_},
        height {height_}
    {
    }
    [[nodiscard]] constexpr auto operator==(const irect32& other) const noexcept -> bool { return x == other.x && y == other.y && width == other.width && height == other.height; }
    [[nodiscard]] constexpr auto operator!=(const irect32& other) const noexcept -> bool { return x != other.x || y != other.y || width != other.width || height != other.height; }

    int32 x {};
    int32 y {};
    int32 width {};
    int32 height {};
};
static_assert(std::is_standard_layout_v<irect32>);
static_assert(sizeof(irect32) == 16);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE irect32, "{} {} {} {}", value.x, value.y, value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE irect32, value.x >> value.y >> value.width >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE irect32);

///@ ExportValueType ipos16 ipos16 Layout = int16-x+int16-y
struct ipos16
{
    constexpr ipos16() noexcept = default;
    constexpr ipos16(int16 x_, int16 y_) noexcept :
        x {x_},
        y {y_}
    {
    }

    [[nodiscard]] constexpr auto operator==(const ipos16& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator!=(const ipos16& other) const noexcept -> bool { return x != other.x || y != other.y; }
    [[nodiscard]] constexpr auto operator+(const ipos16& other) const -> ipos16 { return {numeric_cast<int16>(x + other.x), numeric_cast<int16>(y + other.y)}; }
    [[nodiscard]] constexpr auto operator-(const ipos16& other) const -> ipos16 { return {numeric_cast<int16>(x - other.x), numeric_cast<int16>(y - other.y)}; }
    [[nodiscard]] constexpr auto operator*(const ipos16& other) const -> ipos16 { return {numeric_cast<int16>(x * other.x), numeric_cast<int16>(y * other.y)}; }
    [[nodiscard]] constexpr auto operator/(const ipos16& other) const -> ipos16 { return {numeric_cast<int16>(x / other.x), numeric_cast<int16>(y / other.y)}; }

    int16 x {};
    int16 y {};
};
static_assert(std::is_standard_layout_v<ipos16>);
static_assert(sizeof(ipos16) == 4);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos16, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE ipos16, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE ipos16);

///@ ExportValueType upos16 upos16 Layout = uint16-x+uint16-y
struct upos16
{
    constexpr upos16() noexcept = default;
    constexpr upos16(uint16 x_, uint16 y_) noexcept :
        x {x_},
        y {y_}
    {
    }

    [[nodiscard]] constexpr auto operator==(const upos16& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator!=(const upos16& other) const noexcept -> bool { return x != other.x || y != other.y; }
    [[nodiscard]] constexpr auto operator+(const upos16& other) const -> upos16 { return {numeric_cast<uint16>(x + other.x), numeric_cast<uint16>(y + other.y)}; }
    [[nodiscard]] constexpr auto operator-(const upos16& other) const -> upos16 { return {numeric_cast<uint16>(x - other.x), numeric_cast<uint16>(y - other.y)}; }
    [[nodiscard]] constexpr auto operator*(const upos16& other) const -> upos16 { return {numeric_cast<uint16>(x * other.x), numeric_cast<uint16>(y * other.y)}; }
    [[nodiscard]] constexpr auto operator/(const upos16& other) const -> upos16 { return {numeric_cast<uint16>(x / other.x), numeric_cast<uint16>(y / other.y)}; }

    uint16 x {};
    uint16 y {};
};
static_assert(std::is_standard_layout_v<upos16>);
static_assert(sizeof(upos16) == 4);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE upos16, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE upos16, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE upos16);

///@ ExportValueType ipos8 ipos8 Layout = int8-x+int8-y
struct ipos8
{
    constexpr ipos8() noexcept = default;
    constexpr ipos8(int8 x_, int8 y_) noexcept :
        x {x_},
        y {y_}
    {
    }

    [[nodiscard]] constexpr auto operator==(const ipos8& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator!=(const ipos8& other) const noexcept -> bool { return x != other.x || y != other.y; }
    [[nodiscard]] constexpr auto operator+(const ipos8& other) const -> ipos8 { return {numeric_cast<int8>(x + other.x), numeric_cast<int8>(y + other.y)}; }
    [[nodiscard]] constexpr auto operator-(const ipos8& other) const -> ipos8 { return {numeric_cast<int8>(x - other.x), numeric_cast<int8>(y - other.y)}; }
    [[nodiscard]] constexpr auto operator*(const ipos8& other) const -> ipos8 { return {numeric_cast<int8>(x * other.x), numeric_cast<int8>(y * other.y)}; }
    [[nodiscard]] constexpr auto operator/(const ipos8& other) const -> ipos8 { return {numeric_cast<int8>(x / other.x), numeric_cast<int8>(y / other.y)}; }

    int8 x {};
    int8 y {};
};
static_assert(std::is_standard_layout_v<ipos8>);
static_assert(sizeof(ipos8) == 2);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos8, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE ipos8, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE ipos8);

///@ ExportValueType fsize fsize32 Layout = float32-width+float32-height
struct fsize32
{
    constexpr fsize32() noexcept = default;
    constexpr fsize32(float32 width_, float32 height_) noexcept :
        width {width_},
        height {height_}
    {
    }
    constexpr fsize32(int32 width_, int32 height_) noexcept :
        width {safe_numeric_cast<float32>(width_)},
        height {safe_numeric_cast<float32>(height_)}
    {
    }
    constexpr explicit fsize32(isize32 size) noexcept :
        width {safe_numeric_cast<float32>(size.width)},
        height {safe_numeric_cast<float32>(size.height)}
    {
    }

    [[nodiscard]] constexpr auto operator==(const fsize32& other) const noexcept -> bool { return is_float_equal(width, other.width) && is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto operator!=(const fsize32& other) const noexcept -> bool { return !is_float_equal(width, other.width) || !is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto GetSquare() const noexcept -> float32 { return width * height; }
    template<typename T>
    [[nodiscard]] constexpr auto IsValidPos(T pos) const noexcept -> bool
    {
        return pos.x >= 0.0f && pos.y >= 0.0f && pos.x < width && pos.y < height;
    }

    float32 width {};
    float32 height {};
};
static_assert(std::is_standard_layout_v<fsize32>);
static_assert(sizeof(fsize32) == 8);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE fsize32, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE fsize32, value.width >> value.height);

///@ ExportValueType fpos fpos32 Layout = float32-x+float32-y
struct fpos32
{
    constexpr fpos32() noexcept = default;
    constexpr fpos32(float32 x_, float32 y_) noexcept :
        x {x_},
        y {y_}
    {
    }
    constexpr fpos32(int32 x_, int32 y_) noexcept :
        x {safe_numeric_cast<float32>(x_)},
        y {safe_numeric_cast<float32>(y_)}
    {
    }
    constexpr explicit fpos32(ipos32 pos) noexcept :
        x {safe_numeric_cast<float32>(pos.x)},
        y {safe_numeric_cast<float32>(pos.y)}
    {
    }

    [[nodiscard]] constexpr auto operator==(const fpos32& other) const noexcept -> bool { return is_float_equal(x, other.x) && is_float_equal(y, other.y); }
    [[nodiscard]] constexpr auto operator!=(const fpos32& other) const noexcept -> bool { return !is_float_equal(x, other.x) || !is_float_equal(y, other.y); }
    [[nodiscard]] constexpr auto operator+(const fpos32& other) const -> fpos32 { return {x + other.x, y + other.y}; }
    [[nodiscard]] constexpr auto operator-(const fpos32& other) const -> fpos32 { return {x - other.x, y - other.y}; }
    [[nodiscard]] constexpr auto operator*(const fpos32& other) const -> fpos32 { return {x * other.x, y * other.y}; }
    [[nodiscard]] constexpr auto operator/(const fpos32& other) const -> fpos32 { return {x / other.x, y / other.y}; }

    float32 x {};
    float32 y {};
};
static_assert(std::is_standard_layout_v<fpos32>);
static_assert(sizeof(fpos32) == 8);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE fpos32, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE fpos32, value.x >> value.y);

///@ ExportValueType frect frect32 Layout = float32-x+float32-y+float32-width+float32-height
struct frect32
{
    constexpr frect32() noexcept = default;
    constexpr frect32(fpos32 pos, fsize32 size) noexcept :
        x {pos.x},
        y {pos.y},
        width {size.width},
        height {size.height}
    {
    }
    constexpr frect32(float32 x_, float32 y_, fsize32 size) noexcept :
        x {x_},
        y {y_},
        width {size.width},
        height {size.height}
    {
    }
    constexpr frect32(fpos32 pos, float32 width_, float32 height_) noexcept :
        x {pos.x},
        y {pos.y},
        width {width_},
        height {height_}
    {
    }
    constexpr frect32(float32 x_, float32 y_, float32 width_, float32 height_) noexcept :
        x {x_},
        y {y_},
        width {width_},
        height {height_}
    {
    }
    constexpr frect32(int32 x_, int32 y_, int32 width_, int32 height_) noexcept :
        x {safe_numeric_cast<float32>(x_)},
        y {safe_numeric_cast<float32>(y_)},
        width {safe_numeric_cast<float32>(width_)},
        height {safe_numeric_cast<float32>(height_)}
    {
    }
    constexpr explicit frect32(irect32 size) noexcept :
        x {safe_numeric_cast<float32>(size.x)},
        y {safe_numeric_cast<float32>(size.y)},
        width {safe_numeric_cast<float32>(size.width)},
        height {safe_numeric_cast<float32>(size.height)}
    {
    }
    [[nodiscard]] constexpr auto operator==(const frect32& other) const noexcept -> bool { return is_float_equal(x, other.x) && is_float_equal(y, other.y) && is_float_equal(width, other.width) && is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto operator!=(const frect32& other) const noexcept -> bool { return !is_float_equal(x, other.x) || !is_float_equal(y, other.y) || !is_float_equal(width, other.width) || !is_float_equal(height, other.height); }

    float32 x {};
    float32 y {};
    float32 width {};
    float32 height {};
};
static_assert(std::is_standard_layout_v<frect32>);
static_assert(sizeof(frect32) == 16);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE frect32, "{} {} {} {}", value.x, value.y, value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE frect32, value.x >> value.y >> value.width >> value.height);

FO_END_NAMESPACE();
