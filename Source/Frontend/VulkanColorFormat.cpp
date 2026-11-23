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

#include "VulkanColorFormat.h"

#if FO_HAVE_VULKAN

FO_BEGIN_NAMESPACE

namespace VulkanColorFormat
{
    auto SelectSurfaceFormat(const_span<VkSurfaceFormatKHR> formats, VkFormat required_format) -> optional<VkSurfaceFormatKHR>
    {
        if (required_format != VK_FORMAT_UNDEFINED && required_format != VK_FORMAT_B8G8R8A8_UNORM && required_format != VK_FORMAT_R8G8B8A8_UNORM) {
            return std::nullopt;
        }

        if (formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED) {
            if (formats.front().colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return std::nullopt;
            }

            return VkSurfaceFormatKHR {.format = required_format == VK_FORMAT_UNDEFINED ? VK_FORMAT_B8G8R8A8_UNORM : required_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }

        for (VkFormat candidate : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}) {
            if (required_format != VK_FORMAT_UNDEFINED && required_format != candidate) {
                continue;
            }

            for (const VkSurfaceFormatKHR& surface_format : formats) {
                if (surface_format.format == candidate && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    return surface_format;
                }
            }
        }

        return std::nullopt;
    }

    void DecodePixels(VkFormat format, span<ucolor> pixels)
    {
        FO_VERIFY_AND_THROW(format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_R8G8B8A8_UNORM, "Unsupported Vulkan color format for pixel readback", format);

        if (format == VK_FORMAT_R8G8B8A8_UNORM) {
            return;
        }

        for (ucolor& pixel : pixels) {
            std::swap(pixel.comp.r, pixel.comp.b);
        }
    }

    void EncodePixels(VkFormat format, span<uint8_t> target, const_span<ucolor> source, isize32 size, size_t row_pitch_pixels)
    {
        FO_VERIFY_AND_THROW(format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_R8G8B8A8_UNORM, "Unsupported Vulkan color format for pixel upload", format);
        FO_VERIFY_AND_THROW(size.width >= 0 && size.height >= 0, "Vulkan pixel upload extents must be non-negative", size.width, size.height);

        if (size.width == 0 || size.height == 0) {
            return;
        }

        size_t width = numeric_cast<size_t>(size.width);
        size_t height = numeric_cast<size_t>(size.height);
        FO_VERIFY_AND_THROW(row_pitch_pixels >= width, "Vulkan pixel upload pitch is smaller than the active width", row_pitch_pixels, width);
        FO_VERIFY_AND_THROW(height - 1 <= (std::numeric_limits<size_t>::max() - width) / row_pitch_pixels, "Vulkan pixel upload pixel arithmetic overflows");

        size_t required_pixels = (height - 1) * row_pitch_pixels + width;
        FO_VERIFY_AND_THROW(required_pixels <= std::numeric_limits<size_t>::max() / sizeof(ucolor), "Vulkan pixel upload byte arithmetic overflows");

        size_t required_bytes = required_pixels * sizeof(ucolor);
        FO_VERIFY_AND_THROW(source.size() >= required_pixels, "Vulkan pixel upload source is smaller than the required pixels", source.size(), required_pixels);
        FO_VERIFY_AND_THROW(target.size() >= required_bytes, "Vulkan pixel upload target is smaller than the required bytes", target.size(), required_bytes);

        if (format == VK_FORMAT_R8G8B8A8_UNORM) {
            size_t row_bytes = width * sizeof(ucolor);

            for (size_t y = 0; y < height; y++) {
                size_t row_offset = y * row_pitch_pixels;
                memory::copy(target.data() + row_offset * sizeof(ucolor), source.data() + row_offset, row_bytes);
            }

            return;
        }

        for (size_t y = 0; y < height; y++) {
            size_t row_offset = y * row_pitch_pixels;

            for (size_t x = 0; x < width; x++) {
                size_t pixel_index = row_offset + x;
                size_t target_index = pixel_index * sizeof(ucolor);
                const ucolor& pixel = source[pixel_index];

                target[target_index + 0] = pixel.comp.b;
                target[target_index + 1] = pixel.comp.g;
                target[target_index + 2] = pixel.comp.r;
                target[target_index + 3] = pixel.comp.a;
            }
        }
    }
}

FO_END_NAMESPACE

#endif // FO_HAVE_VULKAN
