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

#include "ImageBaker-Tga.h"

FO_BEGIN_NAMESPACE

namespace ImageBakerTga
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    static auto TgaLoad(span<const uint8_t> data, int32_t& result_width, int32_t& result_height) -> vector<uint8_t>;

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(fname);
        ignore_unused(opt);
        ignore_unused(context);

        int32_t width = 0;
        int32_t height = 0;
        const_span<uint8_t> tga_data = reader.GetDataSpan();
        auto data = TgaLoad(tga_data, width, height);

        FrameCollection collection;
        collection.SequenceSize = 1;
        collection.AnimTicks = 100;
        collection.Main.Frames.resize(numeric_cast<size_t>(collection.SequenceSize));
        collection.Main.Frames[0].Width = width;
        collection.Main.Frames[0].Height = height;
        collection.Main.Frames[0].Data = std::move(data);

        return collection;
    }

    static auto TgaLoad(span<const uint8_t> data, int32_t& result_width, int32_t& result_height) -> vector<uint8_t>
    {
        size_t cur_pos = 0;

        auto read_tga = [&](ptr<void> out, size_t len) {
            FO_VERIFY_AND_THROW(cur_pos <= data.size(), "TGA read position is outside the file");

            if (len > data.size() - cur_pos) {
                throw ImageBakerException("TGA reading error");
            }

            if (len != 0) {
                auto source = make_ptr(data.data() + cur_pos);
                memory::copy(out, source, len);
            }

            cur_pos += len;
        };

        // Load header
        uint8_t type = 0;
        uint8_t pixel_depth = 0;
        uint16_t width_wire = 0;
        uint16_t height_wire = 0;
        uint8_t unused_char = 0;
        uint16_t unused_short = 0;
        read_tga(&unused_char, 1);
        read_tga(&unused_char, 1);
        read_tga(&type, 1);
        read_tga(&unused_short, 2);
        read_tga(&unused_short, 2);
        read_tga(&unused_char, 1);
        read_tga(&unused_short, 2);
        read_tga(&unused_short, 2);
        read_tga(&width_wire, 2);
        read_tga(&height_wire, 2);
        read_tga(&pixel_depth, 1);
        read_tga(&unused_char, 1);

        int32_t width = numeric_cast<int32_t>(width_wire);
        int32_t height = numeric_cast<int32_t>(height_wire);

        // Check if the image is color indexed
        if (type == 1) {
            throw ImageBakerException("Indexed TGA is not supported");
        }

        // Check for TrueColor
        if (type != 2 && type != 10) {
            throw ImageBakerException("TGA TrueColor is not supported");
        }

        // Check for RGB(A)
        if (pixel_depth != 24 && pixel_depth != 32) {
            throw ImageBakerException("TGA support only 24 and 32 bpp");
        }

        FO_VERIFY_AND_THROW(width > 0 && height > 0, "TGA dimensions must be positive", width, height);

        // Read
        int32_t bpp = pixel_depth / 8;
        size_t bpp_size = numeric_cast<size_t>(bpp);
        int64_t pixel_count = checked_mul(numeric_cast<int64_t>(width), numeric_cast<int64_t>(height));
        size_t read_size = numeric_cast<size_t>(checked_mul(pixel_count, numeric_cast<int64_t>(bpp)));
        size_t result_size = numeric_cast<size_t>(checked_mul(pixel_count, int64_t {4}));
        vector<uint8_t> read_data;

        if (type == 2) {
            FO_VERIFY_AND_THROW(read_size <= data.size() - cur_pos, "Truncated TGA pixel payload", read_size, data.size() - cur_pos);
            read_data.resize(read_size);

            if (read_size != 0) {
                auto read_data_begin = make_ptr(read_data.data());
                read_tga(make_nptr(read_data_begin.get()).void_cast(), read_size);
            }
        }
        else {
            int64_t minimum_packet_count = checked_add(checked_div(checked_sub(pixel_count, int64_t {1}), int64_t {128}), int64_t {1});
            int64_t minimum_packet_size = checked_add(int64_t {1}, numeric_cast<int64_t>(bpp));
            size_t minimum_packet_bytes = numeric_cast<size_t>(checked_mul(minimum_packet_count, minimum_packet_size));
            FO_VERIFY_AND_THROW(minimum_packet_bytes <= data.size() - cur_pos, "Truncated TGA RLE payload", minimum_packet_bytes, data.size() - cur_pos);
            read_data.resize(read_size);

            size_t bytes_read = 0;

            while (bytes_read < read_size) {
                uint8_t packet_header = 0;
                read_tga(&packet_header, 1);

                int32_t run_length = checked_add(numeric_cast<int32_t>(packet_header & 0x7Fu), int32_t {1});
                int32_t run_bytes_wide = checked_mul(run_length, bpp);
                size_t run_bytes = numeric_cast<size_t>(run_bytes_wide);

                if ((packet_header & 0x80) != 0) {
                    FO_VERIFY_AND_THROW(run_bytes <= read_size - bytes_read, "TGA RLE run packet exceeds image data", run_bytes, read_size - bytes_read);

                    uint8_t color[4] = {};
                    read_tga(color, bpp_size);

                    for (size_t offset = 0; offset < run_bytes; offset += bpp_size) {
                        for (size_t c = 0; c < bpp_size; c++) {
                            read_data[bytes_read + offset + c] = color[c];
                        }
                    }

                    bytes_read += run_bytes;
                }
                else {
                    size_t to_copy = std::min(run_bytes, read_size - bytes_read);
                    FO_VERIFY_AND_THROW(run_bytes <= data.size() - cur_pos, "Truncated TGA RLE raw packet", run_bytes, data.size() - cur_pos);
                    read_tga(read_data.data() + bytes_read, to_copy);
                    cur_pos += run_bytes - to_copy;
                    bytes_read += run_bytes;
                }
            }
        }

        // Copy data
        vector<uint8_t> result(result_size);

        size_t source_row_size = numeric_cast<size_t>(checked_mul(numeric_cast<int64_t>(width), numeric_cast<int64_t>(bpp)));
        size_t result_row_size = numeric_cast<size_t>(checked_mul(numeric_cast<int64_t>(width), int64_t {4}));

        for (int32_t y = 0; y < height; y++) {
            int32_t flipped_y = checked_sub(checked_sub(height, y), int32_t {1});
            size_t source_row = checked_mul(numeric_cast<size_t>(y), source_row_size);
            size_t result_row = checked_mul(numeric_cast<size_t>(flipped_y), result_row_size);

            for (int32_t x = 0; x < width; x++) {
                size_t source_x = checked_mul(numeric_cast<size_t>(x), bpp_size);
                size_t result_x = checked_mul(numeric_cast<size_t>(x), size_t {4});
                size_t source_pos = checked_add(source_row, source_x);
                size_t result_pos = checked_add(result_row, result_x);
                result[result_pos + 0] = read_data[source_pos + 2];
                result[result_pos + 1] = read_data[source_pos + 1];
                result[result_pos + 2] = read_data[source_pos + 0];
                result[result_pos + 3] = bpp == 4 ? read_data[source_pos + 3] : 0xFF;
            }
        }

        // Return data
        result_width = width;
        result_height = height;

        return result;
    }
}

FO_END_NAMESPACE
