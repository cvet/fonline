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

#include "ImageBaker-Mos.h"

#if FO_IMAGE_MOS

FO_BEGIN_NAMESPACE

namespace ImageBakerMos
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(opt);
        ignore_unused(context);

        // Read signature
        char head[8];
        reader.ReadObjectArray<char>(head);

        if (head[0] != 'M' || head[1] != 'O' || head[2] != 'S') {
            throw ImageBakerException("Invalid MOS file header", fname);
        }

        // Packed
        vector<uint8_t> unpacked_data;

        if (head[3] == 'C') {
            uint32_t unpacked_len = reader.GetLEUInt32();
            size_t data_len = reader.GetSize() - size_t {12};

            vector<uint8_t> packed_data = reader.GetData();
            packed_data.resize(data_len);
            FO_VERIFY_AND_THROW(packed_data.size() >= 2, "Packed data is too small to hold a zlib header", packed_data.size());
            packed_data[0] = 0x78;
            packed_data[1] = 0x9C;

            size_t unpacked_size_hint = checked_add(numeric_cast<size_t>(unpacked_len) / reader.GetSize(), size_t {1});
            unpacked_data = compressor::decompress(packed_data, unpacked_size_hint);

            if (unpacked_data.empty()) {
                throw ImageBakerException("Can't unpack MOS file", fname);
            }

            reader = FileReader(unpacked_data);
            reader.ReadObjectArray<char>(head);

            if (head[0] != 'M' || head[1] != 'O' || head[2] != 'S') {
                throw ImageBakerException("Invalid MOS file unpacked header", fname);
            }
        }

        // Read header
        int32_t width = numeric_cast<int32_t>(reader.GetLEUInt16()); // Width (pixels)
        int32_t height = numeric_cast<int32_t>(reader.GetLEUInt16()); // Height (pixels)
        int32_t col = numeric_cast<int32_t>(reader.GetLEUInt16()); // Columns (blocks)
        int32_t row = numeric_cast<int32_t>(reader.GetLEUInt16()); // Rows (blocks)
        uint32_t block_size = reader.GetLEUInt32(); // Block size (pixels)
        ignore_unused(block_size);
        int64_t palette_offset_wide = numeric_cast<int64_t>(reader.GetLEUInt32()); // Offset (from start of file) to palettes
        int64_t block_count = checked_mul(numeric_cast<int64_t>(col), numeric_cast<int64_t>(row));
        int64_t palette_data_size = checked_mul(block_count, int64_t {256 * 4});
        int64_t tile_index_size = checked_mul(block_count, int64_t {4});
        int64_t tiles_offset_wide = checked_add(palette_offset_wide, palette_data_size);
        int64_t data_offset_wide = checked_add(tiles_offset_wide, tile_index_size);
        size_t palette_offset = numeric_cast<size_t>(palette_offset_wide);
        size_t tiles_offset = numeric_cast<size_t>(tiles_offset_wide);
        size_t data_offset = numeric_cast<size_t>(data_offset_wide);

        // Allocate data
        FO_VERIFY_AND_THROW(width > 0 && height > 0, "Invalid MOS dimensions", fname, width, height);

        int64_t pixel_count = checked_mul(numeric_cast<int64_t>(width), numeric_cast<int64_t>(height));
        size_t data_size = numeric_cast<size_t>(checked_mul(pixel_count, int64_t {4}));
        vector<uint8_t> data(data_size);
        span<uint32_t> pixels = bytes_to_objects<uint32_t>(span {data});

        // Read image data
        uint32_t palette[256] = {0};
        int64_t block = 0;

        for (int32_t y = 0; y < row; y++) {
            for (int32_t x = 0; x < col; x++) {
                // Get palette for current block
                size_t palette_block_offset = checked_mul(numeric_cast<size_t>(block), size_t {256 * 4});
                reader.SetCurPos(checked_add(palette_offset, palette_block_offset));
                reader.ReadObjectArray<uint32_t>(palette);

                // Set initial position
                size_t tile_block_offset = checked_mul(numeric_cast<size_t>(block), size_t {4});
                reader.SetCurPos(checked_add(tiles_offset, tile_block_offset));
                size_t block_data_offset = numeric_cast<size_t>(reader.GetLEUInt32());
                reader.SetCurPos(checked_add(data_offset, block_data_offset));

                // Calculate current block size
                int32_t block_w = x == col - 1 ? width % 64 : 64;
                int32_t block_h = y == row - 1 ? height % 64 : 64;

                if (block_w == 0) {
                    block_w = 64;
                }
                if (block_h == 0) {
                    block_h = 64;
                }

                // Read data
                int64_t row_pixel_offset = checked_mul(checked_mul(numeric_cast<int64_t>(y), int64_t {64}), numeric_cast<int64_t>(width));
                int64_t column_pixel_offset = checked_mul(numeric_cast<int64_t>(x), int64_t {64});
                size_t pos = numeric_cast<size_t>(checked_add(row_pixel_offset, column_pixel_offset));

                for (int32_t yy = 0; yy < block_h; yy++) {
                    for (int32_t xx = 0; xx < block_w; xx++) {
                        uint32_t color = palette[reader.GetUInt8()];

                        if (color == 0xFF00) {
                            color = 0; // Green is transparent
                        }
                        else {
                            color |= 0xFF000000;
                        }

                        pixels[pos] = color;
                        pos++;
                    }

                    int32_t row_gap = checked_sub(width, block_w);
                    pos = checked_add(pos, numeric_cast<size_t>(row_gap));
                }

                // Go to next block
                block++;
            }
        }

        FrameCollection collection;
        collection.SequenceSize = 1;
        collection.AnimTicks = 100;
        collection.Main.Frames.resize(numeric_cast<size_t>(collection.SequenceSize));
        collection.Main.Frames[0].Width = width;
        collection.Main.Frames[0].Height = height;
        collection.Main.Frames[0].Data = std::move(data);
        return collection;
    }
}

FO_END_NAMESPACE

#endif
