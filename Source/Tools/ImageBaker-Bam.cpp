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

#include "ImageBaker-Bam.h"

#if FO_IMAGE_BAM

FO_BEGIN_NAMESPACE

namespace ImageBakerBam
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(context);

        // Format: fileName$5-6.bam
        string opt_str {string(opt)};
        istringstream idelim {make_stream_string(opt_str)};
        int32_t need_cycle {0};
        int32_t specific_frame {-1};
        idelim >> need_cycle;

        if (opt.find('-') != string::npos) {
            char ch {0};
            idelim >> ch >> specific_frame;

            if (specific_frame < 0) {
                specific_frame = -1;
            }
        }

        // Read signature
        char head[8] {};
        reader.ReadObjectArray<char>(head);

        if (head[0] != 'B' || head[1] != 'A' || head[2] != 'M') {
            throw ImageBakerException("Invalid BAM file header", fname);
        }

        // Packed
        vector<uint8_t> unpacked_data {};

        if (head[3] == 'C') {
            uint32_t unpacked_len {reader.ReadLEUInt32()};
            size_t data_len {reader.GetSize() - size_t {12}};

            vector<uint8_t> packed_data {reader.GetData()};
            packed_data.resize(data_len);
            FO_VERIFY_AND_THROW(packed_data.size() >= 2, "Packed data is too small to hold a zlib header", packed_data.size());
            packed_data[0] = 0x78;
            packed_data[1] = 0x9C;

            size_t unpacked_size_hint {checked_add(numeric_cast<size_t>(unpacked_len) / reader.GetSize(), size_t {1})};
            unpacked_data = compressor::decompress(packed_data, unpacked_size_hint);

            if (unpacked_data.empty()) {
                throw ImageBakerException("Cab't unpack BAM file", fname);
            }

            reader = FileReader(unpacked_data);
            reader.ReadObjectArray<char>(head);

            if (head[0] != 'B' || head[1] != 'A' || head[2] != 'M') {
                throw ImageBakerException("Invalid BAM file unpacked header", fname);
            }
        }

        // Read header
        int32_t frames_count {numeric_cast<int32_t>(reader.ReadLEUInt16())};
        int32_t cycles_count {numeric_cast<int32_t>(reader.ReadUInt8())};
        uint8_t compr_color {reader.ReadUInt8()};
        size_t frames_offset {numeric_cast<size_t>(reader.ReadLEUInt32())};
        size_t palette_offset {numeric_cast<size_t>(reader.ReadLEUInt32())};
        size_t lookup_table_offset {numeric_cast<size_t>(reader.ReadLEUInt32())};

        // Count whole frames
        FO_VERIFY_AND_THROW(need_cycle >= 0, "BAM cycle index is negative", fname, need_cycle);

        if (need_cycle >= cycles_count) {
            need_cycle = 0;
        }

        int32_t frame_cycle_offset {checked_add(checked_mul(frames_count, int32_t {12}), checked_mul(need_cycle, int32_t {4}))};
        reader.SetCurPos(checked_add(frames_offset, numeric_cast<size_t>(frame_cycle_offset)));
        int32_t cycle_frames {numeric_cast<int32_t>(reader.ReadLEUInt16())};
        int32_t table_index {numeric_cast<int32_t>(reader.ReadLEUInt16())};

        if (specific_frame != -1 && specific_frame >= cycle_frames) {
            specific_frame = 0;
        }

        // Create animation
        FrameCollection collection {};
        int32_t sequence_size {};
        uint32_t duration_ms {};
        bool have_dirs {};
        sequence_size = numeric_cast<int32_t>(specific_frame == -1 ? cycle_frames : 1);
        duration_ms = 100;

        collection.Main.Frames.resize(numeric_cast<size_t>(sequence_size));

        // Palette
        ucolor palette[256] {};
        reader.SetCurPos(palette_offset);
        reader.ReadObjectArray<ucolor>(palette);

        // Find in lookup table
        for (int32_t i {0}; i < cycle_frames; i++) {
            int32_t lookup_byte_offset {checked_mul(table_index, int32_t {2})};
            reader.SetCurPos(checked_add(lookup_table_offset, numeric_cast<size_t>(lookup_byte_offset)));
            table_index++;
            int32_t frame_index {numeric_cast<int32_t>(reader.ReadLEUInt16())};

            if (specific_frame != -1 && specific_frame != i) {
                continue;
            }

            // Get frame data
            int32_t frame_byte_offset {checked_mul(frame_index, int32_t {12})};
            reader.SetCurPos(checked_add(frames_offset, numeric_cast<size_t>(frame_byte_offset)));
            int32_t width {numeric_cast<int32_t>(reader.ReadLEUInt16())};
            int32_t height {numeric_cast<int32_t>(reader.ReadLEUInt16())};
            int32_t ox {numeric_cast<int32_t>(reader.ReadLEInt16())};
            int32_t oy {numeric_cast<int32_t>(reader.ReadLEInt16())};
            uint32_t data_offset_wire {reader.ReadLEUInt32()};
            bool rle {(data_offset_wire & 0x80000000) == 0};
            data_offset_wire &= 0x7FFFFFFF;
            size_t data_offset {numeric_cast<size_t>(data_offset_wire)};

            // Allocate data
            FO_VERIFY_AND_THROW(width > 0 && height > 0, "Invalid BAM frame dimensions", fname, width, height);

            int64_t pixel_count {checked_mul(numeric_cast<int64_t>(width), numeric_cast<int64_t>(height))};
            size_t data_size {numeric_cast<size_t>(checked_mul(pixel_count, int64_t {4}))};
            vector<uint8_t> data {vector<uint8_t>(data_size)};
            span<ucolor> pixels {bytes_to_objects<ucolor>(span {data})};
            size_t pixel_pos {0};

            // Fill it
            reader.SetCurPos(data_offset);

            for (int64_t k {0}; k < pixel_count;) {
                uint8_t index {reader.ReadUInt8()};
                ucolor color {palette[index]};

                if (color.comp.b == 255) {
                    color = ucolor::clear;
                }
                else {
                    color.comp.a = 255;
                }

                std::swap(color.comp.r, color.comp.b);

                if (rle && index == compr_color) {
                    int32_t copies {numeric_cast<int32_t>(reader.ReadUInt8())};
                    FO_VERIFY_AND_THROW(numeric_cast<size_t>(copies) < pixels.size() - pixel_pos, "BAM RLE run overflows its frame", fname);

                    for (int32_t m {0}; m <= copies; m++, k++) {
                        pixels[pixel_pos] = color;
                        pixel_pos++;
                    }
                }
                else {
                    FO_VERIFY_AND_THROW(pixel_pos < pixels.size(), "BAM frame data overflows its frame", fname);
                    pixels[pixel_pos] = color;
                    pixel_pos++;
                    k++;
                }
            }

            // Set in animation sequence
            size_t shot_index {specific_frame != -1 ? size_t {0} : numeric_cast<size_t>(i)};
            auto& shot {collection.Main.Frames[shot_index]};
            shot.Size = {width, height};
            shot.Offset = {checked_add(checked_sub(int32_t {0}, ox), width / 2), checked_add(checked_sub(int32_t {0}, oy), height)};
            shot.Data = std::move(data);

            if (specific_frame != -1) {
                break;
            }
        }

        // Raw formats without a playable clock use the default cadence; positive sub-ms
        // averages normalize to one ms per frame. Explicit FOFRM timelines are strict
        if (!have_dirs) {
            for (auto& sequence : collection.Dirs) {
                sequence.Frames.clear();
            }
        }

        for (uint8_t direction {0}; direction < ImageImport::GetDirectionCount(collection); direction++) {
            auto& sequence {direction == 0 ? collection.Main : collection.Dirs[direction - 1]};

            if (std::all_of(sequence.Frames.begin(), sequence.Frames.end(), [](const ImageImport::FrameShot& frame) { return frame.Duration == timespan::zero; })) {
                uint32_t count {numeric_cast<uint32_t>(sequence.Frames.size())};
                ImageImport::SetFrameSequenceDuration(sequence, std::chrono::milliseconds {duration_ms == 0 ? count * 100 : std::max(duration_ms, count)});
            }

            (void)ImageImport::GetDuration(sequence);
        }

        return collection;
    }
}

FO_END_NAMESPACE

#endif
