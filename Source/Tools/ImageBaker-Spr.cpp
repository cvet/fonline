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

#include "ImageBaker-Spr.h"

#if FO_IMAGE_SPR

#include "Settings.h"

FO_BEGIN_NAMESPACE

namespace ImageBakerSpr
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    static constexpr timespan SPR_FRAME_DURATION {std::chrono::milliseconds {100}};

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(context);

        FrameCollection collection {};
        int32_t sequence_size {};
        uint32_t duration_ms {};
        bool have_dirs {};

        for (int32_t dir {0}; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            reader.SetCurPos(0);

            int32_t dir_spr {dir};

            if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                switch (dir_spr) {
                case 0:
                    dir_spr = 2;
                    break;
                case 1:
                    dir_spr = 3;
                    break;
                case 2:
                    dir_spr = 4;
                    break;
                case 3:
                    dir_spr = 6;
                    break;
                case 4:
                    dir_spr = 7;
                    break;
                case 5:
                    dir_spr = 0;
                    break;
                default:
                    dir_spr = 0;
                    break;
                }
            }
            else {
                dir_spr = (dir + 2) % 8;
            }

            // Color offsets: 0 other, 1 skin, 2 hair, 3 armor
            int32_t rgb_offs[4][3] {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

            // Format: fileName$[1,100,0,0][2,0,0,100]animName.spr
            auto first {opt.find_first_of('[')};
            auto last {opt.find_last_of(']')};
            auto seq_name {last != string::npos ? opt.substr(last + 1) : opt};

            while (first != string::npos) {
                last = opt.find_first_of(']', first);

                if (last == string::npos) {
                    break;
                }

                string entry {string(opt.substr(first + 1, last - first - 1))};

                for (char& ch : entry) {
                    if (ch == ',') {
                        ch = ' ';
                    }
                }

                istringstream ientry {make_stream_string(entry)};

                // Parse numbers
                int32_t rgb[4] {};

                if (ientry >> rgb[0] >> rgb[1] >> rgb[2] >> rgb[3]) {
                    // To one part
                    if (rgb[0] >= 0 && rgb[0] <= 3) {
                        rgb_offs[rgb[0]][0] = rgb[1]; // R
                        rgb_offs[rgb[0]][1] = rgb[2]; // G
                        rgb_offs[rgb[0]][2] = rgb[3]; // B
                    }
                    // To all parts
                    else {
                        for (auto& rgb_off : rgb_offs) {
                            rgb_off[0] = rgb[1]; // R
                            rgb_off[1] = rgb[2]; // G
                            rgb_off[2] = rgb[3]; // B
                        }
                    }
                }

                first = opt.find_first_of('[', last + 1);
            }

            // Read header
            char head[11] {};
            reader.ReadObjectArray<char>(head);

            if (head[8] != 0 || strcmp(head, "<sprite>") != 0) {
                throw ImageBakerException("Invalid SPR header", fname);
            }

            float32_t dimension_left {numeric_cast<float32_t>(reader.ReadUInt8()) * 6.7f};
            float32_t dimension_up {numeric_cast<float32_t>(reader.ReadUInt8()) * 7.6f};
            ignore_unused(dimension_up);
            float32_t dimension_right {numeric_cast<float32_t>(reader.ReadUInt8()) * 6.7f};
            int32_t center_x {reader.ReadLEInt32()};
            int32_t center_y {reader.ReadLEInt32()};
            reader.GoForward(2); // uint16 unknown1  sometimes it is 0, and sometimes it is 3
            reader.GoForward(1); // CHAR unknown2  0x64, other values were not observed

            float32_t ta {(127.0f / 2.0f) * DEG_TO_RAD_FLOAT}; // Tactics grid angle
            int32_t center_x_ex {iround<int32_t>((dimension_left * sinf(ta) + dimension_right * sinf(ta)) / 2.0f - dimension_left * sinf(ta))};
            int32_t center_y_ex {iround<int32_t>((dimension_left * cosf(ta) + dimension_right * cosf(ta)) / 2.0f)};

            int32_t anim_index {0};
            vector<int32_t> anim_frames {};
            anim_frames.reserve(1000);

            // Find sequence
            bool seq_founded {false};
            int32_t seq_cnt {reader.ReadLEInt32()};

            for (int32_t seq {0}; seq < seq_cnt; seq++) {
                // Find by name
                int32_t item_cnt {reader.ReadLEInt32()};
                FO_VERIFY_AND_THROW(item_cnt >= 0, "SPR sequence item count is negative", fname, item_cnt);

                size_t item_count {numeric_cast<size_t>(item_cnt)};
                reader.GoForward(checked_mul(item_count, sizeof(int16_t)));
                reader.GoForward(checked_mul(item_count, sizeof(int32_t)));

                int32_t name_len {reader.ReadLEInt32()};
                FO_VERIFY_AND_THROW(name_len >= 0, "ART sequence name length is negative", name_len);

                size_t name_length {numeric_cast<size_t>(name_len)};
                const_span<uint8_t> name_data {reader.GetCurDataSpan(name_length)};
                string name {string(!name_data.empty() ? make_ptr(name_data.data()).reinterpret_as<const char>().get() : "", name_data.size())};
                reader.GoForward(name_data.size());
                uint16_t index {reader.ReadLEUInt16()};

                if (seq_name.empty() || strvex(seq_name).compare_ignore_case(name)) {
                    anim_index = numeric_cast<int32_t>(index);

                    // Read frame numbers
                    size_t rewind_size {checked_add(checked_add(sizeof(uint16_t), name_length), checked_add(checked_add(sizeof(int32_t), checked_mul(sizeof(int32_t), item_count)), checked_mul(sizeof(int16_t), item_count)))};
                    reader.GoBack(rewind_size);

                    for (int32_t i {0}; i < item_cnt; i++) {
                        int16_t val {reader.ReadLEInt16()};

                        if (val >= 0) {
                            anim_frames.push_back(numeric_cast<int32_t>(val));
                        }
                        else if (val == -43) {
                            reader.GoForward(sizeof(int16_t) * 3);
                            i += 3;
                        }
                    }

                    // Animation founded, go forward
                    seq_founded = true;
                    break;
                }
            }

            if (!seq_founded) {
                throw ImageBakerException("Sequence for SPR not found", fname);
            }

            // Find animation
            reader.SetCurPos(0);

            for (int32_t i {0}; i <= anim_index; i++) {
                if (!reader.SeekFragment("<spranim>")) {
                    throw ImageBakerException("Spranim for SPR not found", fname);
                }

                reader.GoForward(12);
            }

            int32_t file_offset {reader.ReadLEInt32()};
            FO_VERIFY_AND_THROW(file_offset >= 0, "SPR image file offset is negative", fname, file_offset);

            int32_t collection_name_len {reader.ReadLEInt32()}; // Collection name
            FO_VERIFY_AND_THROW(collection_name_len >= 0, "SPR collection name length is negative", fname, collection_name_len);

            reader.GoForward(numeric_cast<size_t>(collection_name_len));
            int32_t frame_cnt {reader.ReadLEInt32()};
            int32_t dir_cnt {reader.ReadLEInt32()};
            FO_VERIFY_AND_THROW(frame_cnt > 0 && dir_cnt > 0, "Invalid SPR frame or direction count", fname, frame_cnt, dir_cnt);

            int64_t frame_slot_count_wide {checked_mul(checked_mul(numeric_cast<int64_t>(frame_cnt), numeric_cast<int64_t>(dir_cnt)), int64_t {4})};
            size_t frame_slot_count {numeric_cast<size_t>(frame_slot_count_wide)};
            FO_VERIFY_AND_THROW(checked_mul(frame_slot_count, sizeof(int32_t)) <= reader.GetSize() - reader.GetCurPos(), "Truncated SPR bounding box table", fname, frame_slot_count);

            vector<int32_t> bboxes {};
            bboxes.resize(frame_slot_count);

            if (!bboxes.empty()) {
                reader.ReadObjectArray(span<int32_t> {bboxes.data(), bboxes.size()});
            }

            // Fix dir
            if (dir_cnt != 8) {
                int32_t scaled_dir_count {checked_div(checked_mul(dir_cnt, int32_t {100}), int32_t {8})};
                dir_spr = checked_div(checked_mul(dir_spr, scaled_dir_count), int32_t {100});
            }
            if (dir_spr >= dir_cnt) {
                dir_spr = 0;
            }

            // Check wrong frames
            for (size_t i {0}; i < anim_frames.size();) {
                if (anim_frames[i] >= frame_cnt) {
                    anim_frames.erase(anim_frames.begin() + numeric_cast<ptrdiff_t>(i));
                }
                else {
                    i++;
                }
            }

            // Get images file
            reader.SetCurPos(numeric_cast<size_t>(file_offset));
            reader.GoForward(14); // <spranim_img>\0
            uint8_t type {reader.ReadUInt8()};
            reader.GoForward(1); // \0

            size_t data_len {0};
            size_t cur_pos {reader.GetCurPos()};

            if (reader.SeekFragment("<spranim_img>")) {
                data_len = reader.GetCurPos() - cur_pos;
            }
            else {
                data_len = reader.GetSize() - cur_pos;
            }

            reader.SetCurPos(cur_pos);

            bool packed {type == 0x32};
            vector<uint8_t> data {};

            if (packed) {
                // Unpack with zlib
                uint32_t unpacked_len {reader.ReadLEUInt32()};
                FO_VERIFY_AND_THROW(data_len != 0, "Packed SPR frame has zero data length");
                const_span<uint8_t> spr_data {reader.GetCurDataSpan(data_len)};
                size_t unpacked_size_hint {checked_add(numeric_cast<size_t>(unpacked_len) / data_len, size_t {1})};
                auto unpacked_data {compressor::decompress(spr_data, unpacked_size_hint)};

                if (unpacked_data.empty()) {
                    throw ImageBakerException("Can't unpack SPR data", fname);
                }

                data = unpacked_data;
            }
            else {
                data.resize(data_len);
                const_span<uint8_t> spr_data {reader.GetCurDataSpan(data_len)};

                if (!spr_data.empty()) {
                    memory::copy(data.data(), spr_data.data(), spr_data.size());
                }
            }

            auto fm_images {FileReader(data)};

            // Read palette
            typedef ucolor Palette[256];
            Palette palette[4] {};

            for (auto& i : palette) {
                uint32_t palette_count {fm_images.ReadLEUInt32()};

                if (palette_count <= 256) {
                    fm_images.ReadObjectArray(span<ucolor> {i, numeric_cast<size_t>(palette_count)});
                }
            }

            // Index data offsets
            vector<size_t> image_indices {};
            image_indices.resize(frame_slot_count);

            for (size_t cur {0}; fm_images.GetCurPos() != fm_images.GetSize();) {
                FO_VERIFY_AND_THROW(cur < image_indices.size(), "SPR frame index table exceeds its slot count", fname);
                uint8_t tag {fm_images.ReadUInt8()};

                if (tag == 1) {
                    // Valid index
                    image_indices[cur] = fm_images.GetCurPos();
                    fm_images.GoForward(8 + 8 + 8 + 1);
                    uint32_t frame_data_len {fm_images.ReadLEUInt32()};
                    fm_images.GoForward(numeric_cast<size_t>(frame_data_len));
                    cur++;
                }
                else if (tag == 0) {
                    // Empty index
                    cur++;
                }
                else {
                    break;
                }
            }

            // Create animation
            if (anim_frames.empty()) {
                anim_frames.push_back(0);
            }

            FO_VERIFY_AND_THROW(anim_frames.size() <= numeric_cast<size_t>(std::numeric_limits<uint16_t>::max()), "SPR animation frame count exceeds the serialized range", fname, anim_frames.size());

            if (dir == 0) {
                sequence_size = numeric_cast<int32_t>(anim_frames.size());
                duration_ms = numeric_cast<uint32_t>(checked_mul(int64_t {100}, numeric_cast<int64_t>(anim_frames.size())));
            }

            if (dir == 1) {
                have_dirs = true;
            }

            auto& sequence {dir == 0 ? collection.Main : collection.Dirs[dir - 1]};
            sequence.Frames.resize(anim_frames.size());

            for (auto& frame : sequence.Frames) {
                frame.Duration = SPR_FRAME_DURATION;
            }
            int64_t frame_index_stride {checked_mul(numeric_cast<int64_t>(frame_cnt), numeric_cast<int64_t>(dir_cnt))};
            int64_t dir_frame_start {checked_mul(numeric_cast<int64_t>(dir_spr), numeric_cast<int64_t>(frame_cnt))};
            int64_t frm_dir_start {checked_mul(numeric_cast<int64_t>(dir_spr), int64_t {4})};

            for (size_t anim_frame {0}; anim_frame < anim_frames.size(); anim_frame++) {
                auto& shot {sequence.Frames[anim_frame]};
                int32_t frm {anim_frames[anim_frame]};

                // Optimization, share frames
                bool founded {false};

                for (size_t i {0}, j {anim_frame}; i < j; i++) {
                    if (anim_frames[i] == frm) {
                        shot = sequence.Frames[i];
                        shot.Data.clear();
                        shot.Shared = true;
                        shot.SharedIndex = numeric_cast<int32_t>(i);
                        founded = true;
                        break;
                    }
                }

                if (founded) {
                    continue;
                }

                // Compute whole image size
                int32_t whole_width {0};
                int32_t whole_height {0};

                for (int32_t part {0}; part < 4; part++) {
                    int64_t frm_index_wide {type == 0x32 ? checked_add(checked_mul(frame_index_stride, numeric_cast<int64_t>(part)), checked_add(dir_frame_start, numeric_cast<int64_t>(frm))) : checked_add(checked_add(checked_mul(numeric_cast<int64_t>(frm), numeric_cast<int64_t>(dir_cnt)), frm_dir_start), numeric_cast<int64_t>(part))};
                    FO_VERIFY_AND_THROW(frm_index_wide >= 0 && numeric_cast<size_t>(frm_index_wide) < image_indices.size(), "SPR frame index is outside the index table", fname, frm_index_wide);

                    size_t frm_index {numeric_cast<size_t>(frm_index_wide)};

                    if (image_indices[frm_index] == 0) {
                        continue;
                    }

                    fm_images.SetCurPos(image_indices[frm_index]);

                    int32_t posx {fm_images.ReadLEInt32()};
                    int32_t posy {fm_images.ReadLEInt32()};
                    fm_images.GoForward(8);
                    int32_t width {fm_images.ReadLEInt32()};
                    int32_t height {fm_images.ReadLEInt32()};
                    FO_VERIFY_AND_THROW(posx >= 0 && posy >= 0 && width >= 0 && height >= 0, "Invalid SPR part placement or dimensions", fname, posx, posy, width, height);

                    whole_width = std::max(checked_add(width, posx), whole_width);
                    whole_height = std::max(checked_add(height, posy), whole_height);
                }

                if (whole_width == 0) {
                    whole_width = 1;
                }
                if (whole_height == 0) {
                    whole_height = 1;
                }

                // Allocate data
                FO_VERIFY_AND_THROW(whole_width > 0 && whole_height > 0 && whole_width <= numeric_cast<int32_t>(std::numeric_limits<uint16_t>::max()) && whole_height <= numeric_cast<int32_t>(std::numeric_limits<uint16_t>::max()), "SPR frame dimensions are outside the serialized range", fname, whole_width, whole_height);

                int64_t pixel_count {checked_mul(numeric_cast<int64_t>(whole_width), numeric_cast<int64_t>(whole_height))};
                size_t whole_data_size {numeric_cast<size_t>(checked_mul(pixel_count, int64_t {4}))};
                vector<uint8_t> whole_data {vector<uint8_t>(whole_data_size)};
                span<ucolor> whole_pixels {bytes_to_objects<ucolor>(span {whole_data})};

                for (int32_t part {0}; part < 4; part++) {
                    int64_t frm_index_wide {type == 0x32 ? checked_add(checked_mul(frame_index_stride, numeric_cast<int64_t>(part)), checked_add(dir_frame_start, numeric_cast<int64_t>(frm))) : checked_add(checked_add(checked_mul(numeric_cast<int64_t>(frm), numeric_cast<int64_t>(dir_cnt)), frm_dir_start), numeric_cast<int64_t>(part))};
                    FO_VERIFY_AND_THROW(frm_index_wide >= 0 && numeric_cast<size_t>(frm_index_wide) < image_indices.size(), "SPR frame index is outside the index table", fname, frm_index_wide);

                    size_t frm_index {numeric_cast<size_t>(frm_index_wide)};

                    if (image_indices[frm_index] == 0) {
                        continue;
                    }

                    fm_images.SetCurPos(image_indices[frm_index]);

                    int32_t posx {fm_images.ReadLEInt32()};
                    int32_t posy {fm_images.ReadLEInt32()};

                    char zar[8] {0};
                    fm_images.ReadObjectArray<char>(zar);
                    uint8_t subtype {std::bit_cast<uint8_t>(zar[6])};

                    int32_t width {fm_images.ReadLEInt32()};
                    int32_t height {fm_images.ReadLEInt32()};
                    ignore_unused(height);
                    uint8_t palette_present {fm_images.ReadUInt8()};
                    uint32_t rle_size {fm_images.ReadLEUInt32()};
                    size_t rle_size_value {numeric_cast<size_t>(rle_size)};
                    const_span<uint8_t> rle_data {fm_images.GetCurDataSpan(rle_size_value)};
                    fm_images.GoForward(rle_size_value);
                    uint8_t def_color {0};

                    if (zar[5] != 0 || strcmp(zar, "<zar>") != 0 || (subtype != 0x34 && subtype != 0x35) || palette_present == 1) {
                        throw ImageBakerException("Invalid SPR file innded ZAR header", fname);
                    }

                    FO_VERIFY_AND_THROW(posx >= 0 && posy >= 0 && width >= 0 && height >= 0, "Invalid SPR part placement or dimensions", fname, posx, posy, width, height);

                    int32_t part_right {checked_add(width, posx)};
                    int64_t pixel_pos_wide {checked_add(checked_mul(numeric_cast<int64_t>(posy), numeric_cast<int64_t>(whole_width)), numeric_cast<int64_t>(posx))};

                    size_t pixel_pos {numeric_cast<size_t>(pixel_pos_wide)};
                    int32_t x {posx};
                    int32_t y {posy};
                    size_t rle_pos {0};

                    while (rle_pos < rle_data.size()) {
                        int32_t control {numeric_cast<int32_t>(rle_data[rle_pos])};
                        rle_pos++;

                        int32_t control_mode {control & 3};
                        int32_t control_count {control >> 2};
                        int32_t payload_width {control_mode == 2 ? 2 : (control_mode == 0 ? 0 : 1)};
                        size_t payload_size {checked_mul(numeric_cast<size_t>(control_count), numeric_cast<size_t>(payload_width))};
                        FO_VERIFY_AND_THROW(payload_size <= rle_data.size() - rle_pos, "SPR RLE data is truncated", fname);

                        for (int32_t i {0}; i < control_count; i++) {
                            ucolor col {};

                            switch (control_mode) {
                            case 1:
                                col = palette[part][rle_data[rle_pos + numeric_cast<size_t>(i)]];
                                set_object_byte(col, 3, 0xFF);
                                break;
                            case 2:
                                col = palette[part][rle_data[rle_pos + numeric_cast<size_t>(2) * numeric_cast<size_t>(i)]];
                                set_object_byte(col, 3, rle_data[rle_pos + numeric_cast<size_t>(2) * numeric_cast<size_t>(i) + 1]);
                                break;
                            case 3:
                                col = palette[part][def_color];
                                set_object_byte(col, 3, rle_data[rle_pos + numeric_cast<size_t>(i)]);
                                break;
                            default:
                                break;
                            }

                            for (int32_t j {0}; j < 3; j++) {
                                if (rgb_offs[part][j] != 0) {
                                    size_t color_byte_index {numeric_cast<size_t>(2 - j)};
                                    int32_t val {checked_add(numeric_cast<int32_t>(get_object_byte(col, color_byte_index)), rgb_offs[part][j])};
                                    set_object_byte(col, color_byte_index, numeric_cast<uint8_t>(std::clamp(val, 0, 255)));
                                }
                            }

                            swap_object_bytes(col, 0, 2);
                            FO_VERIFY_AND_THROW(pixel_pos < whole_pixels.size(), "SPR RLE run exceeds frame bounds", fname);

                            if (part == 0) {
                                whole_pixels[pixel_pos] = col;
                            }
                            else if (col.comp.a >= 128) {
                                whole_pixels[pixel_pos] = col;
                            }

                            pixel_pos++;

                            if (++x >= part_right) {
                                x = posx;
                                y++;
                                int64_t next_pixel_pos_wide {checked_add(checked_mul(numeric_cast<int64_t>(y), numeric_cast<int64_t>(whole_width)), numeric_cast<int64_t>(x))};
                                pixel_pos = numeric_cast<size_t>(next_pixel_pos_wide);
                            }
                        }

                        if (control_mode != 0) {
                            rle_pos += numeric_cast<size_t>(control_count);

                            if (control_mode == 2) {
                                rle_pos += numeric_cast<size_t>(control_count);
                            }
                        }
                    }
                }

                int64_t bbox_base {checked_mul(checked_add(checked_mul(numeric_cast<int64_t>(frm), numeric_cast<int64_t>(dir_cnt)), numeric_cast<int64_t>(dir_spr)), int64_t {4})};
                FO_VERIFY_AND_THROW(bbox_base >= 0 && numeric_cast<size_t>(checked_add(bbox_base, int64_t {1})) < bboxes.size(), "SPR frame bounding box is outside the table", fname, bbox_base);

                size_t bbox_x_index {numeric_cast<size_t>(bbox_base)};
                size_t bbox_y_index {checked_add(bbox_x_index, size_t {1})};
                int64_t next_x {checked_add(checked_sub(numeric_cast<int64_t>(bboxes[bbox_x_index]), numeric_cast<int64_t>(center_x)), checked_add(numeric_cast<int64_t>(center_x_ex), numeric_cast<int64_t>(whole_width / 2)))};
                int64_t next_y {checked_add(checked_sub(numeric_cast<int64_t>(bboxes[bbox_y_index]), numeric_cast<int64_t>(center_y)), checked_add(numeric_cast<int64_t>(center_y_ex), numeric_cast<int64_t>(whole_height)))};

                shot.Size = {whole_width, whole_height};
                shot.Offset = {numeric_cast<int32_t>(next_x), numeric_cast<int32_t>(next_y)};
                shot.Data = std::move(whole_data);
            }

            if (dir_cnt == 1) {
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
