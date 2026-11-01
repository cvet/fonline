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

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "VulkanColorFormat.h"

#if FO_HAVE_VULKAN

FO_BEGIN_NAMESPACE

static auto MakeSurfaceFormat(VkFormat format, VkColorSpaceKHR color_space) -> VkSurfaceFormatKHR
{
    return VkSurfaceFormatKHR {.format = format, .colorSpace = color_space};
}

TEST_CASE("VulkanColorFormatSelection", "[vulkan-color-format]")
{
    SECTION("RgbaOnlySurfaceSelectsRgba")
    {
        array<VkSurfaceFormatKHR, 1> formats {MakeSurfaceFormat(VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)};

        optional<VkSurfaceFormatKHR> selected = VulkanColorFormat::SelectSurfaceFormat(formats);

        REQUIRE(selected.has_value());
        CHECK(selected->format == VK_FORMAT_R8G8B8A8_UNORM);
        CHECK(selected->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    }

    SECTION("DefaultPreferenceIsBgraInEitherOrder")
    {
        array<VkSurfaceFormatKHR, 2> bgra_first {MakeSurfaceFormat(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR), MakeSurfaceFormat(VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)};
        array<VkSurfaceFormatKHR, 2> rgba_first {MakeSurfaceFormat(VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR), MakeSurfaceFormat(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)};

        optional<VkSurfaceFormatKHR> first_selected = VulkanColorFormat::SelectSurfaceFormat(bgra_first);
        optional<VkSurfaceFormatKHR> second_selected = VulkanColorFormat::SelectSurfaceFormat(rgba_first);

        REQUIRE(first_selected.has_value());
        CHECK(first_selected->format == VK_FORMAT_B8G8R8A8_UNORM);
        CHECK(first_selected->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        REQUIRE(second_selected.has_value());
        CHECK(second_selected->format == VK_FORMAT_B8G8R8A8_UNORM);
        CHECK(second_selected->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    }

    SECTION("RequiredStorageFormatIsRetained")
    {
        array<VkSurfaceFormatKHR, 2> formats {MakeSurfaceFormat(VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR), MakeSurfaceFormat(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)};

        optional<VkSurfaceFormatKHR> rgba_selected = VulkanColorFormat::SelectSurfaceFormat(formats, VK_FORMAT_R8G8B8A8_UNORM);
        optional<VkSurfaceFormatKHR> bgra_selected = VulkanColorFormat::SelectSurfaceFormat(formats, VK_FORMAT_B8G8R8A8_UNORM);

        REQUIRE(rgba_selected.has_value());
        CHECK(rgba_selected->format == VK_FORMAT_R8G8B8A8_UNORM);
        CHECK(rgba_selected->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        REQUIRE(bgra_selected.has_value());
        CHECK(bgra_selected->format == VK_FORMAT_B8G8R8A8_UNORM);
        CHECK(bgra_selected->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);

        array<VkSurfaceFormatKHR, 1> rgba_only {MakeSurfaceFormat(VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)};

        CHECK_FALSE(VulkanColorFormat::SelectSurfaceFormat(rgba_only, VK_FORMAT_B8G8R8A8_UNORM).has_value());
    }

    SECTION("UndefinedPairUsesRequiredOrDefaultStorage")
    {
        array<VkSurfaceFormatKHR, 1> formats {MakeSurfaceFormat(VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)};

        optional<VkSurfaceFormatKHR> default_selected = VulkanColorFormat::SelectSurfaceFormat(formats);
        optional<VkSurfaceFormatKHR> rgba_selected = VulkanColorFormat::SelectSurfaceFormat(formats, VK_FORMAT_R8G8B8A8_UNORM);
        optional<VkSurfaceFormatKHR> bgra_selected = VulkanColorFormat::SelectSurfaceFormat(formats, VK_FORMAT_B8G8R8A8_UNORM);

        REQUIRE(default_selected.has_value());
        CHECK(default_selected->format == VK_FORMAT_B8G8R8A8_UNORM);
        CHECK(default_selected->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        REQUIRE(rgba_selected.has_value());
        CHECK(rgba_selected->format == VK_FORMAT_R8G8B8A8_UNORM);
        CHECK(rgba_selected->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        REQUIRE(bgra_selected.has_value());
        CHECK(bgra_selected->format == VK_FORMAT_B8G8R8A8_UNORM);
        CHECK(bgra_selected->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    }

    SECTION("UndefinedPairRejectsNonSrgbColorSpace")
    {
        array<VkSurfaceFormatKHR, 1> formats {MakeSurfaceFormat(VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT)};

        CHECK_FALSE(VulkanColorFormat::SelectSurfaceFormat(formats).has_value());
        CHECK_FALSE(VulkanColorFormat::SelectSurfaceFormat(formats, VK_FORMAT_R8G8B8A8_UNORM).has_value());
    }

    SECTION("UnsupportedSurfaceListsAreRejected")
    {
        const_span<VkSurfaceFormatKHR> empty_formats {};
        array<VkSurfaceFormatKHR, 2> srgb_images {MakeSurfaceFormat(VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR), MakeSurfaceFormat(VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)};
        array<VkSurfaceFormatKHR, 2> other_color_spaces {MakeSurfaceFormat(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT), MakeSurfaceFormat(VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT)};
        array<VkSurfaceFormatKHR, 1> unsupported_image {MakeSurfaceFormat(VK_FORMAT_B8G8R8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)};
        array<VkSurfaceFormatKHR, 2> supported_formats {MakeSurfaceFormat(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR), MakeSurfaceFormat(VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)};

        CHECK_FALSE(VulkanColorFormat::SelectSurfaceFormat(empty_formats).has_value());
        CHECK_FALSE(VulkanColorFormat::SelectSurfaceFormat(srgb_images).has_value());
        CHECK_FALSE(VulkanColorFormat::SelectSurfaceFormat(other_color_spaces).has_value());
        CHECK_FALSE(VulkanColorFormat::SelectSurfaceFormat(unsupported_image).has_value());
        CHECK_FALSE(VulkanColorFormat::SelectSurfaceFormat(supported_formats, VK_FORMAT_B8G8R8A8_SRGB).has_value());
        CHECK_FALSE(VulkanColorFormat::SelectSurfaceFormat(supported_formats, VK_FORMAT_R8G8B8A8_SRGB).has_value());
    }
}

TEST_CASE("VulkanColorPixelReadback", "[vulkan-color-format]")
{
    SECTION("BgraReadbackSwapsRedAndBlue")
    {
        array<ucolor, 2> pixels {ucolor {3, 2, 1, 4}, ucolor {30, 20, 10, 40}};

        CHECK_NOTHROW(VulkanColorFormat::DecodePixels(VK_FORMAT_B8G8R8A8_UNORM, pixels));
        CHECK(pixels[0] == ucolor {1, 2, 3, 4});
        CHECK(pixels[1] == ucolor {10, 20, 30, 40});
    }

    SECTION("RgbaReadbackLeavesPixelsUnchanged")
    {
        array<ucolor, 2> pixels {ucolor {1, 2, 3, 4}, ucolor {10, 20, 30, 40}};

        CHECK_NOTHROW(VulkanColorFormat::DecodePixels(VK_FORMAT_R8G8B8A8_UNORM, pixels));
        CHECK(pixels[0] == ucolor {1, 2, 3, 4});
        CHECK(pixels[1] == ucolor {10, 20, 30, 40});
    }
}

TEST_CASE("VulkanColorPixelUpload", "[vulkan-color-format]")
{
    array<ucolor, 6> source {{ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}, ucolor {91, 92, 93, 94}, ucolor {9, 10, 11, 12}, ucolor {13, 14, 15, 16}, ucolor {95, 96, 97, 98}}};

    SECTION("BgraUploadHonorsPitchAndPadding")
    {
        array<uint8_t, 24> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 24> expected {3, 2, 1, 4, 7, 6, 5, 8, 0xCC, 0xCC, 0xCC, 0xCC, 11, 10, 9, 12, 15, 14, 13, 16, 0xCC, 0xCC, 0xCC, 0xCC};

        CHECK_NOTHROW(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source, {2, 2}, 3));
        CHECK(std::equal(target.begin(), target.end(), expected.begin(), expected.end()));
        CHECK(source[0] == ucolor {1, 2, 3, 4});
        CHECK(source[2] == ucolor {91, 92, 93, 94});
    }

    SECTION("RgbaUploadHonorsPitchAndPadding")
    {
        array<uint8_t, 24> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 24> expected {1, 2, 3, 4, 5, 6, 7, 8, 0xCC, 0xCC, 0xCC, 0xCC, 9, 10, 11, 12, 13, 14, 15, 16, 0xCC, 0xCC, 0xCC, 0xCC};

        CHECK_NOTHROW(VulkanColorFormat::EncodePixels(VK_FORMAT_R8G8B8A8_UNORM, target, source, {2, 2}, 3));
        CHECK(std::equal(target.begin(), target.end(), expected.begin(), expected.end()));
    }

    SECTION("BgraUploadAcceptsSourceWithoutLastRowPadding")
    {
        array<ucolor, 5> source_without_padding {ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}, ucolor {91, 92, 93, 94}, ucolor {9, 10, 11, 12}, ucolor {13, 14, 15, 16}};
        array<uint8_t, 20> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 20> expected {3, 2, 1, 4, 7, 6, 5, 8, 0xCC, 0xCC, 0xCC, 0xCC, 11, 10, 9, 12, 15, 14, 13, 16};

        CHECK_NOTHROW(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source_without_padding, {2, 2}, 3));
        CHECK(std::equal(target.begin(), target.end(), expected.begin(), expected.end()));
    }

    SECTION("RgbaUploadAcceptsSourceWithoutLastRowPadding")
    {
        array<ucolor, 5> source_without_padding {ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}, ucolor {91, 92, 93, 94}, ucolor {9, 10, 11, 12}, ucolor {13, 14, 15, 16}};
        array<uint8_t, 20> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 20> expected {1, 2, 3, 4, 5, 6, 7, 8, 0xCC, 0xCC, 0xCC, 0xCC, 9, 10, 11, 12, 13, 14, 15, 16};

        CHECK_NOTHROW(VulkanColorFormat::EncodePixels(VK_FORMAT_R8G8B8A8_UNORM, target, source_without_padding, {2, 2}, 3));
        CHECK(std::equal(target.begin(), target.end(), expected.begin(), expected.end()));
    }

    SECTION("BgraUploadAcceptsTargetWithoutLastRowPadding")
    {
        array<uint8_t, 20> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 20> expected {3, 2, 1, 4, 7, 6, 5, 8, 0xCC, 0xCC, 0xCC, 0xCC, 11, 10, 9, 12, 15, 14, 13, 16};

        CHECK_NOTHROW(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source, {2, 2}, 3));
        CHECK(std::equal(target.begin(), target.end(), expected.begin(), expected.end()));
    }

    SECTION("RgbaUploadAcceptsTargetWithoutLastRowPadding")
    {
        array<uint8_t, 20> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 20> expected {1, 2, 3, 4, 5, 6, 7, 8, 0xCC, 0xCC, 0xCC, 0xCC, 9, 10, 11, 12, 13, 14, 15, 16};

        CHECK_NOTHROW(VulkanColorFormat::EncodePixels(VK_FORMAT_R8G8B8A8_UNORM, target, source, {2, 2}, 3));
        CHECK(std::equal(target.begin(), target.end(), expected.begin(), expected.end()));
    }

    SECTION("BgraUploadUsesPackedPitch")
    {
        array<ucolor, 4> packed_source {ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}, ucolor {9, 10, 11, 12}, ucolor {13, 14, 15, 16}};
        array<uint8_t, 20> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 20> expected {3, 2, 1, 4, 7, 6, 5, 8, 11, 10, 9, 12, 15, 14, 13, 16, 0xCC, 0xCC, 0xCC, 0xCC};

        CHECK_NOTHROW(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, packed_source, {2, 2}, 2));
        CHECK(std::equal(target.begin(), target.end(), expected.begin(), expected.end()));
    }

    SECTION("RgbaUploadUsesPackedPitch")
    {
        array<ucolor, 4> packed_source {ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}, ucolor {9, 10, 11, 12}, ucolor {13, 14, 15, 16}};
        array<uint8_t, 20> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 20> expected {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0xCC, 0xCC, 0xCC, 0xCC};

        CHECK_NOTHROW(VulkanColorFormat::EncodePixels(VK_FORMAT_R8G8B8A8_UNORM, target, packed_source, {2, 2}, 2));
        CHECK(std::equal(target.begin(), target.end(), expected.begin(), expected.end()));
    }
}

TEST_CASE("VulkanColorPixelInputValidation", "[vulkan-color-format]")
{
    SECTION("NegativeExtentsAreRejectedWithoutWrites")
    {
        array<ucolor, 2> source {ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}};
        array<uint8_t, 16> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 16> original = target;

        CHECK_THROWS(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source, {0, -1}, 2));
        CHECK(std::equal(target.begin(), target.end(), original.begin(), original.end()));
        CHECK_THROWS(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source, {-1, 0}, 2));
        CHECK(std::equal(target.begin(), target.end(), original.begin(), original.end()));
    }

    SECTION("PitchBelowWidthIsRejectedWithoutWrites")
    {
        array<ucolor, 3> source {ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}, ucolor {9, 10, 11, 12}};
        array<uint8_t, 12> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 12> original = target;

        CHECK_THROWS(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source, {3, 1}, 2));
        CHECK(std::equal(target.begin(), target.end(), original.begin(), original.end()));
    }

    SECTION("ShortSourceIsRejectedWithoutWrites")
    {
        array<ucolor, 4> source {ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}, ucolor {9, 10, 11, 12}, ucolor {13, 14, 15, 16}};
        array<uint8_t, 24> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 24> original = target;

        CHECK_THROWS(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source, {2, 2}, 3));
        CHECK(std::equal(target.begin(), target.end(), original.begin(), original.end()));
    }

    SECTION("ShortTargetIsRejectedWithoutWrites")
    {
        array<ucolor, 6> source {ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}, ucolor {9, 10, 11, 12}, ucolor {13, 14, 15, 16}, ucolor {17, 18, 19, 20}, ucolor {21, 22, 23, 24}};
        array<uint8_t, 19> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 19> original = target;

        CHECK_THROWS(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source, {2, 2}, 3));
        CHECK(std::equal(target.begin(), target.end(), original.begin(), original.end()));
    }

    SECTION("MaximumPitchArithmeticIsRejectedWithoutWrites")
    {
        array<ucolor, 1> source {ucolor {1, 2, 3, 4}};
        array<uint8_t, 8> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 8> original = target;
        size_t maximum_pitch = std::numeric_limits<size_t>::max();

        CHECK_THROWS(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source, {2, 2}, maximum_pitch));
        CHECK(std::equal(target.begin(), target.end(), original.begin(), original.end()));
    }

    SECTION("EmptyRegionsAreNoOps")
    {
        array<ucolor, 1> source {ucolor {1, 2, 3, 4}};
        array<uint8_t, 8> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 8> original = target;

        CHECK_NOTHROW(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source, {0, 4}, 0));
        CHECK(std::equal(target.begin(), target.end(), original.begin(), original.end()));
        CHECK_NOTHROW(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_UNORM, target, source, {4, 0}, 4));
        CHECK(std::equal(target.begin(), target.end(), original.begin(), original.end()));
    }

    SECTION("UnsupportedDecodeLeavesPixelsUnchanged")
    {
        array<ucolor, 2> pixels {ucolor {11, 22, 33, 44}, ucolor {55, 66, 77, 88}};
        array<ucolor, 2> original = pixels;

        CHECK_THROWS(VulkanColorFormat::DecodePixels(VK_FORMAT_B8G8R8A8_SRGB, pixels));
        CHECK(pixels[0] == original[0]);
        CHECK(pixels[1] == original[1]);
        CHECK_THROWS(VulkanColorFormat::DecodePixels(VK_FORMAT_R8G8B8A8_SRGB, pixels));
        CHECK(pixels[0] == original[0]);
        CHECK(pixels[1] == original[1]);
    }

    SECTION("UnsupportedEncodeLeavesTargetUnchanged")
    {
        array<ucolor, 3> source {ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}, ucolor {9, 10, 11, 12}};
        array<uint8_t, 12> target {};
        target.fill(uint8_t {0xCC});
        array<uint8_t, 12> original = target;

        CHECK_THROWS(VulkanColorFormat::EncodePixels(VK_FORMAT_B8G8R8A8_SRGB, target, source, {3, 1}, 3));
        CHECK(std::equal(target.begin(), target.end(), original.begin(), original.end()));
    }
}

FO_END_NAMESPACE

#endif // FO_HAVE_VULKAN
