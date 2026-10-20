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
        uint16_t sequence_size {};
        uint32_t anim_ticks {};
        bool have_dirs {};
        sequence_size = 1;
        anim_ticks = 100;

        collection.Main.Frames.resize(sequence_size);
        collection.Main.Frames[0].Width = numeric_cast<uint16_t>(width);
        collection.Main.Frames[0].Height = numeric_cast<uint16_t>(height);
        collection.Main.Frames[0].Data = std::move(data);

        // Raw formats without a playable clock use the default cadence; positive sub-ms
        // averages normalize to one ms per frame. Explicit FOFRM timelines are strict.
        if (!have_dirs) {
            for (auto& sequence : collection.Dirs) {
                sequence.Frames.clear();
            }
        }

        for (uint8_t direction = 0; direction < ImageImport::GetDirectionCount(collection); direction++) {
            auto& sequence = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

            if (std::all_of(sequence.Frames.begin(), sequence.Frames.end(), [](const ImageImport::FrameShot& frame) { return frame.DurationMs == 0; })) {
                uint32_t count = numeric_cast<uint32_t>(sequence.Frames.size());
                ImageImport::SetFrameSequenceDuration(sequence, anim_ticks == 0 ? count * 100 : std::max(anim_ticks, count));
            }

            (void)ImageImport::GetDurationMs(sequence);
        }

        return collection;
    }

    static auto TgaLoad(span<const uint8_t> data, int32_t& result_width, int32_t& result_height) -> vector<uint8_t>
    {
        size_t cur_pos = 0;

        auto read_tga = [&](ptr<void> out, size_t len) {
            if (cur_pos + len <= data.size()) {
                if (len != 0) {
                    auto source = make_ptr(data.data() + cur_pos);
                    memory::copy(out, source, len);
                }

                cur_pos += (len);
            }
            else {
                throw ImageBakerException("TGA reading error");
            }
        };

        // Load header
        uint8_t type = 0;
        uint8_t pixel_depth = 0;
        int16_t width = 0;
        int16_t height = 0;
        uint8_t unused_char = 0;
        int16_t unused_short = 0;
        read_tga(&unused_char, 1);
        read_tga(&unused_char, 1);
        read_tga(&type, 1);
        read_tga(&unused_short, 2);
        read_tga(&unused_short, 2);
        read_tga(&unused_char, 1);
        read_tga(&unused_short, 2);
        read_tga(&unused_short, 2);
        read_tga(&width, 2);
        read_tga(&height, 2);
        read_tga(&pixel_depth, 1);
        read_tga(&unused_char, 1);

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

        // Read
        int32_t bpp = pixel_depth / 8;
        int32_t read_size = height * width * bpp;
        vector<uint8_t> read_data;
        read_data.resize(read_size);

        if (type == 2) {
            if (read_size != 0) {
                auto read_data_begin = make_ptr(read_data.data());
                read_tga(make_nptr(read_data_begin.get()).void_cast(), numeric_cast<size_t>(read_size));
            }
        }
        else {
            int32_t bytes_read = 0;
            uint8_t header = 0;
            uint8_t color[4];

            while (bytes_read < read_size) {
                read_tga(&header, 1);

                if ((header & 0x00000080) != 0) {
                    header &= ~0x00000080;
                    read_tga(color, bpp);
                    int32_t run_len = (header + 1) * bpp;

                    for (int32_t i = 0; i < run_len; i += bpp) {
                        for (int32_t c = 0; c < bpp && bytes_read + i + c < read_size; c++) {
                            read_data[bytes_read + i + c] = color[c];
                        }
                    }

                    bytes_read += run_len;
                }
                else {
                    int32_t run_len = (header + 1) * bpp;
                    int32_t to_read;

                    if (bytes_read + run_len > read_size) {
                        to_read = read_size - bytes_read;
                    }
                    else {
                        to_read = run_len;
                    }

                    if (to_read != 0) {
                        auto read_data_pos = make_ptr(read_data.data() + numeric_cast<size_t>(bytes_read));
                        read_tga(make_nptr(read_data_pos.get()).void_cast(), numeric_cast<size_t>(to_read));
                    }
                    bytes_read += run_len;

                    if (bytes_read + run_len > read_size) {
                        cur_pos += run_len - to_read;
                    }
                }
            }
        }

        // Copy data
        vector<uint8_t> result;
        result.resize(numeric_cast<size_t>(width) * height * 4);

        for (int16_t y = 0; y < height; y++) {
            for (int16_t x = 0; x < width; x++) {
                int32_t i = (height - y - 1) * width + x;
                int32_t j = y * width + x;
                result[i * 4 + 0] = read_data[j * bpp + 2];
                result[i * 4 + 1] = read_data[j * bpp + 1];
                result[i * 4 + 2] = read_data[j * bpp + 0];
                result[i * 4 + 3] = bpp == 4 ? read_data[j * bpp + 3] : 0xFF;
            }
        }

        // Return data
        result_width = width;
        result_height = height;

        return result;
    }
}

FO_END_NAMESPACE
