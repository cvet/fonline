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
            size_t data_len = reader.GetSize() - 12;

            vector<uint8_t> packed_data = reader.GetData();
            packed_data.resize(data_len);
            FO_VERIFY_AND_THROW(packed_data.size() >= 2, "Packed data is too small to hold a zlib header", packed_data.size());
            packed_data[0] = 0x78;
            packed_data[1] = 0x9C;

            unpacked_data = compressor::decompress(packed_data, unpacked_len / reader.GetSize() + 1);

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
        int32_t width = reader.GetLEUInt16(); // Width (pixels)
        int32_t height = reader.GetLEUInt16(); // Height (pixels)
        int32_t col = reader.GetLEUInt16(); // Columns (blocks)
        int32_t row = reader.GetLEUInt16(); // Rows (blocks)
        uint32_t block_size = reader.GetLEUInt32(); // Block size (pixels)
        ignore_unused(block_size);
        uint32_t palette_offset = reader.GetLEUInt32(); // Offset (from start of file) to palettes
        uint32_t tiles_offset = palette_offset + col * row * 256 * 4;
        uint32_t data_offset = tiles_offset + col * row * 4;

        // Allocate data
        vector<uint8_t> data(numeric_cast<size_t>(width) * height * 4);
        span<uint32_t> pixels = bytes_to_objects<uint32_t>(span {data});

        // Read image data
        uint32_t palette[256] = {0};
        int32_t block = 0;

        for (int32_t y = 0; y < row; y++) {
            for (int32_t x = 0; x < col; x++) {
                // Get palette for current block
                reader.SetCurPos(palette_offset + block * 256 * 4);
                reader.ReadObjectArray<uint32_t>(palette);

                // Set initial position
                reader.SetCurPos(tiles_offset + block * 4);
                reader.SetCurPos(data_offset + reader.GetLEUInt32());

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
                int32_t pos = y * 64 * width + x * 64;
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

                    pos += width - block_w;
                }

                // Go to next block
                block++;
            }
        }

        FrameCollection collection;
        uint16_t sequence_size {};
        uint32_t duration_ms {};
        bool have_dirs {};
        sequence_size = 1;
        duration_ms = 100;

        collection.Main.Frames.resize(sequence_size);
        collection.Main.Frames[0].Size = {width, height};
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

            if (std::all_of(sequence.Frames.begin(), sequence.Frames.end(), [](const ImageImport::FrameShot& frame) { return frame.Duration == timespan::zero; })) {
                uint32_t count = numeric_cast<uint32_t>(sequence.Frames.size());
                ImageImport::SetFrameSequenceDuration(sequence, std::chrono::milliseconds {duration_ms == 0 ? count * 100 : std::max(duration_ms, count)});
            }

            (void)ImageImport::GetDuration(sequence);
        }

        return collection;
    }
}

FO_END_NAMESPACE

#endif
