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

FO_BEGIN_NAMESPACE();

// Color type
///@ ExportValueType ucolor ucolor HardStrong HasValueAccessor Layout = uint32-value
struct ucolor
{
    using underlying_type = uint32;
    static constexpr bool is_strong_type = true;
    static constexpr string_view type_name = "ucolor";
    static constexpr string_view underlying_type_name = "uint32";

    constexpr ucolor() noexcept :
        rgba {}
    {
    }
    constexpr explicit ucolor(uint32 rgba_) noexcept :
        rgba {rgba_}
    {
    }
    constexpr ucolor(uint8 r_, uint8 g_, uint8 b_) noexcept :
        comp {r_, g_, b_, 255}
    {
    }
    constexpr ucolor(uint8 r_, uint8 g_, uint8 b_, uint8 a_) noexcept :
        comp {r_, g_, b_, a_}
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
        uint32 rgb : 24;
        uint32 rgba;
        components comp;
    };

    static const ucolor clear;
};
static_assert(sizeof(ucolor) == sizeof(uint32));
static_assert(std::is_standard_layout_v<ucolor>);
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
///@ ExportValueType isize isize HardStrong Layout = int32-width+int32-height
struct isize
{
    constexpr isize() noexcept = default;
    constexpr isize(int32 width_, int32 height_) noexcept :
        width {width_},
        height {height_}
    {
    }

    [[nodiscard]] constexpr auto operator==(const isize& other) const noexcept -> bool { return width == other.width && height == other.height; }
    [[nodiscard]] constexpr auto operator!=(const isize& other) const noexcept -> bool { return width != other.width || height != other.height; }
    [[nodiscard]] constexpr auto GetSquare() const noexcept -> int32 { return width * height; }
    template<typename T>
    [[nodiscard]] constexpr auto IsValidPos(T pos) const noexcept -> bool
    {
        return pos.x >= 0 && pos.y >= 0 && pos.x < width && pos.y < height;
    }

    int32 width {};
    int32 height {};
};
static_assert(std::is_standard_layout_v<isize>);
static_assert(sizeof(isize) == 8);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE isize, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE isize, value.width >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE isize);

///@ ExportValueType ipos ipos HardStrong Layout = int32-x+int32-y
struct ipos
{
    constexpr ipos() noexcept = default;
    constexpr ipos(int32 x_, int32 y_) noexcept :
        x {x_},
        y {y_}
    {
    }

    [[nodiscard]] constexpr auto operator==(const ipos& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator!=(const ipos& other) const noexcept -> bool { return x != other.x || y != other.y; }
    [[nodiscard]] constexpr auto operator+(const ipos& other) const noexcept -> ipos { return {x + other.x, y + other.y}; }
    [[nodiscard]] constexpr auto operator-(const ipos& other) const noexcept -> ipos { return {x - other.x, y - other.y}; }
    [[nodiscard]] constexpr auto operator*(const ipos& other) const noexcept -> ipos { return {x * other.x, y * other.y}; }
    [[nodiscard]] constexpr auto operator/(const ipos& other) const noexcept -> ipos { return {x / other.x, y / other.y}; }

    int32 x {};
    int32 y {};
};
static_assert(std::is_standard_layout_v<ipos>);
static_assert(sizeof(ipos) == 8);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE ipos, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE ipos);

///@ ExportValueType irect irect32 HardStrong Layout = int32-x+int32-y+int32-width+int32-height
struct irect32
{
    constexpr irect32() noexcept = default;
    constexpr irect32(ipos pos, isize size) noexcept :
        x {pos.x},
        y {pos.y},
        width {size.width},
        height {size.height}
    {
    }
    constexpr irect32(int32 x_, int32 y_, isize size) noexcept :
        x {x_},
        y {y_},
        width {size.width},
        height {size.height}
    {
    }
    constexpr irect32(ipos pos, int32 width_, int32 height_) noexcept :
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

///@ ExportValueType ipos16 ipos16 HardStrong Layout = int16-x+int16-y
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

///@ ExportValueType upos16 upos16 HardStrong Layout = uint16-x+uint16-y
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

///@ ExportValueType ipos8 ipos8 HardStrong Layout = int8-x+int8-y
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

///@ ExportValueType fsize fsize HardStrong Layout = float32-width+float32-height
struct fsize
{
    constexpr fsize() noexcept = default;
    constexpr fsize(float32 width_, float32 height_) noexcept :
        width {width_},
        height {height_}
    {
    }
    constexpr fsize(int32 width_, int32 height_) noexcept :
        width {safe_numeric_cast<float32>(width_)},
        height {safe_numeric_cast<float32>(height_)}
    {
    }
    constexpr explicit fsize(isize size) noexcept :
        width {safe_numeric_cast<float32>(size.width)},
        height {safe_numeric_cast<float32>(size.height)}
    {
    }

    [[nodiscard]] constexpr auto operator==(const fsize& other) const noexcept -> bool { return is_float_equal(width, other.width) && is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto operator!=(const fsize& other) const noexcept -> bool { return !is_float_equal(width, other.width) || !is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto GetSquare() const noexcept -> float32 { return width * height; }
    template<typename T>
    [[nodiscard]] constexpr auto IsValidPos(T pos) const noexcept -> bool
    {
        return pos.x >= 0.0f && pos.y >= 0.0f && pos.x < width && pos.y < height;
    }

    float32 width {};
    float32 height {};
};
static_assert(std::is_standard_layout_v<fsize>);
static_assert(sizeof(fsize) == 8);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE fsize, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE fsize, value.width >> value.height);

///@ ExportValueType fpos fpos HardStrong Layout = float32-x+float32-y
struct fpos
{
    constexpr fpos() noexcept = default;
    constexpr fpos(float32 x_, float32 y_) noexcept :
        x {x_},
        y {y_}
    {
    }
    constexpr fpos(int32 x_, int32 y_) noexcept :
        x {safe_numeric_cast<float32>(x_)},
        y {safe_numeric_cast<float32>(y_)}
    {
    }
    constexpr explicit fpos(ipos pos) noexcept :
        x {safe_numeric_cast<float32>(pos.x)},
        y {safe_numeric_cast<float32>(pos.y)}
    {
    }

    [[nodiscard]] constexpr auto operator==(const fpos& other) const noexcept -> bool { return is_float_equal(x, other.x) && is_float_equal(y, other.y); }
    [[nodiscard]] constexpr auto operator!=(const fpos& other) const noexcept -> bool { return !is_float_equal(x, other.x) || !is_float_equal(y, other.y); }
    [[nodiscard]] constexpr auto operator+(const fpos& other) const -> fpos { return {x + other.x, y + other.y}; }
    [[nodiscard]] constexpr auto operator-(const fpos& other) const -> fpos { return {x - other.x, y - other.y}; }
    [[nodiscard]] constexpr auto operator*(const fpos& other) const -> fpos { return {x * other.x, y * other.y}; }
    [[nodiscard]] constexpr auto operator/(const fpos& other) const -> fpos { return {x / other.x, y / other.y}; }

    float32 x {};
    float32 y {};
};
static_assert(std::is_standard_layout_v<fpos>);
static_assert(sizeof(fpos) == 8);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE fpos, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE fpos, value.x >> value.y);

///@ ExportValueType frect frect HardStrong Layout = float32-x+float32-y+float32-width+float32-height
struct frect
{
    constexpr frect() noexcept = default;
    constexpr frect(fpos pos, fsize size) noexcept :
        x {pos.x},
        y {pos.y},
        width {size.width},
        height {size.height}
    {
    }
    constexpr frect(float32 x_, float32 y_, fsize size) noexcept :
        x {x_},
        y {y_},
        width {size.width},
        height {size.height}
    {
    }
    constexpr frect(fpos pos, float32 width_, float32 height_) noexcept :
        x {pos.x},
        y {pos.y},
        width {width_},
        height {height_}
    {
    }
    constexpr frect(float32 x_, float32 y_, float32 width_, float32 height_) noexcept :
        x {x_},
        y {y_},
        width {width_},
        height {height_}
    {
    }
    [[nodiscard]] constexpr auto operator==(const frect& other) const noexcept -> bool { return is_float_equal(x, other.x) && is_float_equal(y, other.y) && is_float_equal(width, other.width) && is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto operator!=(const frect& other) const noexcept -> bool { return !is_float_equal(x, other.x) || !is_float_equal(y, other.y) || !is_float_equal(width, other.width) || !is_float_equal(height, other.height); }

    float32 x {};
    float32 y {};
    float32 width {};
    float32 height {};
};
static_assert(std::is_standard_layout_v<frect>);
static_assert(sizeof(frect) == 16);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE frect, "{} {} {} {}", value.x, value.y, value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE frect, value.x >> value.y >> value.width >> value.height);

// Flex rect
template<typename T>
struct TRect
{
    static_assert(std::is_arithmetic_v<T>);

    TRect() noexcept = default;
    template<typename T2>
        requires(std::is_floating_point_v<T2>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    TRect(const TRect<T2>& other) noexcept :
        Left(iround<T>(other.Left)),
        Top(iround<T>(other.Top)),
        Right(iround<T>(other.Right)),
        Bottom(iround<T>(other.Bottom))
    {
    }
    template<typename T2>
        requires(std::is_integral_v<T2>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    TRect(const TRect<T2>& other) noexcept :
        Left(numeric_cast<T>(other.Left)),
        Top(numeric_cast<T>(other.Top)),
        Right(numeric_cast<T>(other.Right)),
        Bottom(numeric_cast<T>(other.Bottom))
    {
    }
    TRect(T l, T t, T r, T b) noexcept :
        Left(l),
        Top(t),
        Right(r),
        Bottom(b)
    {
    }
    TRect(T l, T t, T r, T b, T ox, T oy) noexcept :
        Left(l + ox),
        Top(t + oy),
        Right(r + ox),
        Bottom(b + oy)
    {
    }
    TRect(const TRect& fr, T ox, T oy) noexcept :
        Left(fr.Left + ox),
        Top(fr.Top + oy),
        Right(fr.Right + ox),
        Bottom(fr.Bottom + oy)
    {
    }

    template<typename T2>
    auto operator=(const TRect<T2>& fr) noexcept -> TRect&
    {
        Left = numeric_cast<T>(fr.Left);
        Top = numeric_cast<T>(fr.Top);
        Right = numeric_cast<T>(fr.Right);
        Bottom = numeric_cast<T>(fr.Bottom);
        return *this;
    }

    [[nodiscard]] auto IsZero() const noexcept -> bool { return !Left && !Top && !Right && !Bottom; }
    [[nodiscard]] auto Width() const noexcept -> T { return Right - Left; }
    [[nodiscard]] auto Height() const noexcept -> T { return Bottom - Top; }
    [[nodiscard]] auto CenterX() const noexcept -> T { return Left + Width() / 2; }
    [[nodiscard]] auto CenterY() const noexcept -> T { return Top + Height() / 2; }

    [[nodiscard]] auto operator[](int32 index) -> T&
    {
        switch (index) {
        case 0:
            return Left;
        case 1:
            return Top;
        case 2:
            return Right;
        case 3:
            return Bottom;
        default:
            break;
        }
        throw InvalidOperationException(FO_LINE_STR);
    }

    [[nodiscard]] auto operator[](int32 index) const noexcept -> const T& { return (*const_cast<TRect<T>*>(this))[index]; }

    T Left {};
    T Top {};
    T Right {};
    T Bottom {};
};
using IRect = TRect<int32>; // Todo: move IRect to irect32
using FRect = TRect<float32>; // Todo: move FRect to frect

FO_END_NAMESPACE();
