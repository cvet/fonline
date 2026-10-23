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

#include "ImageBaker-Art.h"

#if FO_IMAGE_ART

#include "Settings.h"

FO_BEGIN_NAMESPACE

namespace ImageBakerArt
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(context);

        int32_t palette_index = 0; // 0..3
        bool transparent = false;
        bool mirror_hor = false;
        bool mirror_ver = false;
        int32_t frm_from = 0;
        int32_t frm_to = 100000;
        int32_t forced_rotation = -1;

        for (size_t i = 0; i < opt.length(); i++) {
            switch (opt[i]) {
            case '0':
                palette_index = 0;
                break;
            case '1':
                palette_index = 1;
                break;
            case '2':
                palette_index = 2;
                break;
            case '3':
                palette_index = 3;
                break;
            case 'R':
            case 'r':
                if (i + 1 < opt.length() && opt[i + 1] == '=') {
                    FO_VERIFY_AND_THROW(i + 2 < opt.length() && opt[i + 2] >= '0' && opt[i + 2] <= '7', "ART rotation must be a digit from 0 to 7", fname, opt);
                    forced_rotation = opt[i + 2] - '0';
                    i += 2;
                    FO_VERIFY_AND_THROW(i + 1 == opt.length() || string_view {"TtHhVvFfRr"}.find(opt[i + 1]) != string_view::npos, "Invalid ART option after rotation", fname, opt);
                }
                break;
            case 'T':
            case 't':
                transparent = true;
                break;
            case 'H':
            case 'h':
                mirror_hor = true;
                break;
            case 'V':
            case 'v':
                mirror_ver = true;
                break;
            case 'F':
            case 'f': {
                // name$1vf5-7.art
                size_t frame_spec_len = 0;

                while (i + 1 + frame_spec_len < opt.length()) {
                    char ch = opt[i + 1 + frame_spec_len];

                    if (!((ch >= '0' && ch <= '9') || ch == '-')) {
                        break;
                    }

                    frame_spec_len++;
                }

                if (frame_spec_len != 0) {
                    string frame_spec {opt.substr(i + 1, frame_spec_len)};
                    istringstream idelim(make_stream_string(frame_spec));
                    char ch = 0;

                    if (frame_spec.find('-') != string::npos) {
                        idelim >> frm_from >> ch >> frm_to;
                    }
                    else {
                        idelim >> frm_from;
                        frm_to = frm_from;
                    }

                    i += frame_spec_len;
                }
            } break;
            default:
                break;
            }
        }

        // ART stores all rotation-major frame tables before the rotation-major pixel data
        struct ArtHeader
        {
            uint32_t Flags {}; // 0x01 single rotation, 0x02 critter delta, 0x04 font advance, 0x08 facade walk data, 0x10 eye candy
            int32_t FrameRate {};
            int32_t Bpp {};
            uint32_t PaletteList[4] {}; // Non-zero when the palette slot is present
            int32_t ActionFrame {};
            int32_t FrameCount {};
            int32_t FrameTables[8] {}; // Runtime pointers, ignored
            int32_t DataSize[8] {};
            int32_t PixelTables[8] {}; // Runtime pointers, ignored
        };
        static_assert(sizeof(ArtHeader) == 132);

        struct ArtFrameInfo
        {
            int32_t Width {};
            int32_t Height {};
            int32_t DataSize {};
            int32_t HotX {};
            int32_t HotY {};
            int32_t DeltaX {};
            int32_t DeltaY {};
        };
        static_assert(sizeof(ArtFrameInfo) == 28);

        constexpr int32_t art_rotations = 8;

        ArtHeader header;
        reader.ReadObject(header);

        FO_VERIFY_AND_THROW(header.Bpp == 8, "Unsupported ART bit depth", fname, header.Bpp);
        FO_VERIFY_AND_THROW(header.FrameCount > 0, "Invalid ART frame count", fname, header.FrameCount);
        FO_VERIFY_AND_THROW(header.FrameRate >= 0, "Invalid ART frame rate", fname, header.FrameRate);

        int32_t stored_rotations = (header.Flags & 0x1) != 0 ? 1 : art_rotations;
        int32_t frm_count = header.FrameCount;

        // Palettes are stored only for present slots, in slot order
        using ArtPalette = array<uint32_t, 256>;
        array<ArtPalette, 4> palettes {};
        array<bool, 4> palette_present {};
        int32_t first_palette = -1;

        for (int32_t i = 0; i < 4; i++) {
            if (header.PaletteList[i] != 0) {
                reader.ReadObjectArray<uint32_t>(span {palettes[i]});
                palette_present[i] = true;

                if (first_palette == -1) {
                    first_palette = i;
                }
            }
        }

        FO_VERIFY_AND_THROW(first_palette != -1, "ART file has no palette", fname);

        if (!palette_present[palette_index]) {
            palette_index = first_palette;
        }

        size_t frame_table_count = numeric_cast<size_t>(numeric_cast<uint64_t>(stored_rotations) * numeric_cast<uint64_t>(frm_count));
        FO_VERIFY_AND_THROW(frame_table_count <= (reader.GetSize() - reader.GetCurPos()) / sizeof(ArtFrameInfo), "Truncated ART frame table", fname, frm_count, stored_rotations);

        vector<ArtFrameInfo> frame_infos(frame_table_count);
        reader.ReadObjectArray<ArtFrameInfo>(span {frame_infos});

        // Decode every stored frame into palette indices
        vector<vector<uint8_t>> frame_indices(frame_infos.size());

        for (size_t fi = 0; fi < frame_infos.size(); fi++) {
            const ArtFrameInfo& info = frame_infos[fi];
            FO_VERIFY_AND_THROW(info.Width >= 0 && info.Height >= 0 && info.Width <= std::numeric_limits<uint16_t>::max() && info.Height <= std::numeric_limits<uint16_t>::max(), "Invalid ART frame size", fname, fi, info.Width, info.Height);
            FO_VERIFY_AND_THROW(info.DataSize >= 0, "Invalid ART frame data size", fname, fi, info.DataSize);

            FileReader frame_reader {reader.GetCurDataSpan(numeric_cast<size_t>(info.DataSize))};
            reader.GoForward(numeric_cast<size_t>(info.DataSize));

            size_t pixel_count = numeric_cast<size_t>(info.Width) * numeric_cast<size_t>(info.Height);
            vector<uint8_t>& indices = frame_indices[fi];
            indices.assign(pixel_count, 0);

            if (numeric_cast<size_t>(info.DataSize) == pixel_count) {
                frame_reader.ReadObjectArray<uint8_t>(span {indices});
            }
            else if (info.DataSize > 0) {
                size_t written = 0;

                while (frame_reader.GetCurPos() < frame_reader.GetSize()) {
                    uint8_t value = frame_reader.GetUInt8();
                    size_t len = value & 0x7F;

                    FO_VERIFY_AND_THROW(written + len <= pixel_count, "ART RLE run overflows its frame", fname, fi);

                    if ((value & 0x80) != 0) {
                        frame_reader.ReadObjectArray<uint8_t>(span {indices}.subspan(written, len));
                    }
                    else {
                        uint8_t color_index = frame_reader.GetUInt8();
                        std::fill_n(indices.begin() + numeric_cast<ptrdiff_t>(written), len, color_index);
                    }

                    written += len;
                }

                FO_VERIFY_AND_THROW(written == pixel_count, "ART RLE data does not fill its frame", fname, fi, written, pixel_count);
            }
        }

        // Path categories identify the placement rules of the original ART renderer
        string lower_name = strex(fname).lower().str();
        bool critter_like = lower_name.starts_with("art/critter/") || lower_name.starts_with("art/monster/") || lower_name.starts_with("art/unique_npc/");
        bool wall_like = lower_name.starts_with("art/wall/") || lower_name.starts_with("art/portal/");
        bool roof = lower_name.starts_with("art/roof/");

        if (frm_from >= frm_count) {
            frm_from = frm_count - 1;
        }
        if (frm_to >= frm_count) {
            frm_to = frm_count - 1;
        }

        int32_t frm_count_target = std::max(frm_from, frm_to) - std::min(frm_from, frm_to) + 1;

        FrameCollection collection;
        uint16_t sequence_size {};
        uint32_t duration_ms {};
        bool have_dirs {};
        sequence_size = numeric_cast<uint16_t>(frm_count_target);
        // Sprite v3 preserves the complete imported duration in uint32 milliseconds
        int64_t imported_ticks = header.FrameRate > 0 ? numeric_cast<int64_t>(std::max(1, 1000 / header.FrameRate)) * frm_count_target : 0;
        duration_ms = numeric_cast<uint32_t>(imported_ticks);
        have_dirs = stored_rotations == art_rotations && forced_rotation < 0;

        int32_t dir_count = have_dirs ? GameSettings::MAP_DIR_COUNT : 1;

        for (int32_t dir = 0; dir < dir_count; dir++) {
            FrameSequence& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
            sequence.Frames.resize(sequence_size);

            // ART rotations run clockwise from north; square Engine directions start north-east
            int32_t rotation = 0;

            if (forced_rotation >= 0) {
                rotation = stored_rotations == art_rotations ? forced_rotation : 0;
            }
            else if (have_dirs) {
                if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                    constexpr int32_t hex_dir_to_rotation[6] = {1, 2, 3, 5, 6, 7};
                    rotation = hex_dir_to_rotation[dir];
                }
                else {
                    rotation = (dir + 1) % art_rotations;
                }
            }

            // Critter-like art draws rotations 1..3 as mirrored rotations 7..5
            bool rotation_mirrored = false;

            if (critter_like && stored_rotations == art_rotations && rotation > 0 && rotation < 4) {
                rotation = art_rotations - rotation;
                rotation_mirrored = true;
            }

            bool flip_hor = rotation_mirrored != mirror_hor;

            struct PlacedFrame
            {
                size_t Index {};
                int64_t HotX {};
                int64_t HotY {};
                int64_t DeltaX {};
                int64_t DeltaY {};
            };

            vector<PlacedFrame> placed;
            placed.reserve(numeric_cast<size_t>(frm_count_target));

            for (int32_t frm_read = frm_from;; frm_read += frm_to >= frm_from ? 1 : -1) {
                size_t index = numeric_cast<size_t>(rotation) * numeric_cast<size_t>(frm_count) + numeric_cast<size_t>(frm_read);
                const ArtFrameInfo& info = frame_infos[index];

                PlacedFrame frame {.Index = index, .HotX = info.HotX, .HotY = info.HotY, .DeltaX = info.DeltaX, .DeltaY = info.DeltaY};

                // Single-rotation walls still use the explicitly placed rotation for their hot spot shift
                int32_t placed_rotation = forced_rotation >= 0 ? forced_rotation : rotation;

                if (wall_like && (placed_rotation < 2 || placed_rotation > 5)) {
                    frame.HotX -= 40;
                    frame.HotY += 20;
                }

                if (rotation_mirrored) {
                    frame.HotX = info.Width - frame.HotX - 1;
                    frame.DeltaX = -frame.DeltaX;
                }

                if (mirror_hor) {
                    // Flipped art ids (flag 0x1): roofs anchor at their left edge, everything else mirrors around the tile
                    frame.HotX = roof ? 0 : info.Width - frame.HotX - 2;
                    frame.DeltaX = -frame.DeltaX;
                }

                if (mirror_ver) {
                    frame.HotY = info.Height - frame.HotY - 1;
                    frame.DeltaY = -frame.DeltaY;
                }

                placed.emplace_back(frame);

                if (frm_read == frm_to) {
                    break;
                }
            }

            // Frames carry individual hot spots, the engine sequence has one offset: place every frame on a shared
            // canvas where all hot spots coincide, then anchor the canvas at that point
            int64_t left = 0;
            int64_t top = 0;
            int64_t right = 0;
            int64_t bottom = 0;

            for (const PlacedFrame& frame : placed) {
                const ArtFrameInfo& info = frame_infos[frame.Index];
                left = std::max(left, frame.HotX);
                top = std::max(top, frame.HotY);
                right = std::max(right, info.Width - frame.HotX);
                bottom = std::max(bottom, info.Height - frame.HotY);
            }

            uint16_t canvas_w = numeric_cast<uint16_t>(std::max(left + right, int64_t {1}));
            uint16_t canvas_h = numeric_cast<uint16_t>(std::max(top + bottom, int64_t {1}));

            sequence.OffsX = numeric_cast<int16_t>(canvas_w / 2 - left);
            sequence.OffsY = numeric_cast<int16_t>(canvas_h - top);

            const ArtPalette& palette = palettes[palette_index];

            for (size_t frm_write = 0; frm_write < placed.size(); frm_write++) {
                const PlacedFrame& frame = placed[frm_write];
                const ArtFrameInfo& info = frame_infos[frame.Index];
                const vector<uint8_t>& indices = frame_indices[frame.Index];

                FrameShot& shot = sequence.Frames[frm_write];
                shot.Size = {numeric_cast<int32_t>(canvas_w), numeric_cast<int32_t>(canvas_h)};
                shot.Motion = {numeric_cast<int32_t>(frame.DeltaX), numeric_cast<int32_t>(frame.DeltaY)};

                vector<uint8_t> data(numeric_cast<size_t>(numeric_cast<uint64_t>(canvas_w) * canvas_h * 4));
                span<uint32_t> pixels = bytes_to_objects<uint32_t>(span {data});

                int64_t origin_x = left - frame.HotX;
                int64_t origin_y = top - frame.HotY;

                for (int32_t y = 0; y < info.Height; y++) {
                    for (int32_t x = 0; x < info.Width; x++) {
                        uint8_t index = indices[numeric_cast<size_t>(y) * numeric_cast<size_t>(info.Width) + numeric_cast<size_t>(x)];

                        if (index == 0) {
                            continue;
                        }

                        uint32_t color = palette[index];
                        swap_object_bytes(color, 0, 2);

                        if (transparent) {
                            color |= std::max((color >> 16) & 0xFF, std::max((color >> 8) & 0xFF, color & 0xFF)) << 24;
                        }
                        else {
                            color |= 0xFF000000;
                        }

                        int32_t src_x = flip_hor ? info.Width - x - 1 : x;
                        int32_t src_y = mirror_ver ? info.Height - y - 1 : y;
                        int64_t dst_x = origin_x + src_x;
                        int64_t dst_y = origin_y + src_y;

                        pixels[numeric_cast<size_t>(dst_y) * numeric_cast<size_t>(canvas_w) + numeric_cast<size_t>(dst_x)] = color;
                    }
                }

                shot.Data = std::move(data);
            }
        }

        // Raw formats without a playable clock use the default cadence; positive sub-ms
        // averages normalize to one ms per frame. Explicit FOFRM timelines are strict.
        if (!have_dirs) {
            for (auto& sequence : collection.Dirs) {
                sequence.Frames.clear();
            }
        }

        for (uint8_t direction = 0; direction < ImageImport::GetDirectionCount(collection); direction++) {
            auto& sequence = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

            if (std::all_of(sequence.Frames.begin(), sequence.Frames.end(), [](const ImageImport::FrameShot& frame) { return frame.Duration == timespan {}; })) {
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
