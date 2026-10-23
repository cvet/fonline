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

#include "ImageBaker-Frm.h"

#if FO_IMAGE_FRM

#include "Settings.h"

FO_BEGIN_NAMESPACE

namespace ImageBakerFrm
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    // clang-format off
alignas(ucolor) static const uint8_t FoPalette[] = {
    // Transparent color
    0x00, 0x00, 0x00, 0x00,
    // Default colors
    0xec, 0xec, 0xec, 0xff, 0xdc, 0xdc, 0xdc, 0xff, 0xcc, 0xcc, 0xcc, 0xff, 0xbc, 0xbc, 0xbc, 0xff, 0xb0, 0xb0, 0xb0, 0xff, 0xa0, 0xa0, 0xa0, 0xff, 0x90, 0x90, 0x90, 0xff,
    0x80, 0x80, 0x80, 0xff, 0x74, 0x74, 0x74, 0xff, 0x64, 0x64, 0x64, 0xff, 0x54, 0x54, 0x54, 0xff, 0x48, 0x48, 0x48, 0xff, 0x38, 0x38, 0x38, 0xff, 0x28, 0x28, 0x28, 0xff,
    0x20, 0x20, 0x20, 0xff, 0xfc, 0xec, 0xec, 0xff, 0xec, 0xd8, 0xd8, 0xff, 0xdc, 0xc4, 0xc4, 0xff, 0xd0, 0xb0, 0xb0, 0xff, 0xc0, 0xa0, 0xa0, 0xff, 0xb0, 0x90, 0x90, 0xff,
    0xa4, 0x80, 0x80, 0xff, 0x94, 0x70, 0x70, 0xff, 0x84, 0x60, 0x60, 0xff, 0x78, 0x54, 0x54, 0xff, 0x68, 0x44, 0x44, 0xff, 0x58, 0x38, 0x38, 0xff, 0x4c, 0x2c, 0x2c, 0xff,
    0x3c, 0x24, 0x24, 0xff, 0x2c, 0x18, 0x18, 0xff, 0x20, 0x10, 0x10, 0xff, 0xec, 0xec, 0xfc, 0xff, 0xd8, 0xd8, 0xec, 0xff, 0xc4, 0xc4, 0xdc, 0xff, 0xb0, 0xb0, 0xd0, 0xff,
    0xa0, 0xa0, 0xc0, 0xff, 0x90, 0x90, 0xb0, 0xff, 0x80, 0x80, 0xa4, 0xff, 0x70, 0x70, 0x94, 0xff, 0x60, 0x60, 0x84, 0xff, 0x54, 0x54, 0x78, 0xff, 0x44, 0x44, 0x68, 0xff,
    0x38, 0x38, 0x58, 0xff, 0x2c, 0x2c, 0x4c, 0xff, 0x24, 0x24, 0x3c, 0xff, 0x18, 0x18, 0x2c, 0xff, 0x10, 0x10, 0x20, 0xff, 0xfc, 0xb0, 0xf0, 0xff, 0xc4, 0x60, 0xa8, 0xff,
    0x68, 0x24, 0x60, 0xff, 0x4c, 0x14, 0x48, 0xff, 0x38, 0x0c, 0x34, 0xff, 0x28, 0x10, 0x24, 0xff, 0x24, 0x04, 0x24, 0xff, 0x1c, 0x0c, 0x18, 0xff, 0xfc, 0xfc, 0xc8, 0xff,
    0xfc, 0xfc, 0x7c, 0xff, 0xe4, 0xd8, 0x0c, 0xff, 0xcc, 0xb8, 0x1c, 0xff, 0xb8, 0x9c, 0x28, 0xff, 0xa4, 0x88, 0x30, 0xff, 0x90, 0x78, 0x24, 0xff, 0x7c, 0x68, 0x18, 0xff,
    0x6c, 0x58, 0x10, 0xff, 0x58, 0x48, 0x08, 0xff, 0x48, 0x38, 0x04, 0xff, 0x34, 0x28, 0x00, 0xff, 0x20, 0x18, 0x00, 0xff, 0xd8, 0xfc, 0x9c, 0xff, 0xb4, 0xd8, 0x84, 0xff,
    0x98, 0xb8, 0x70, 0xff, 0x78, 0x98, 0x5c, 0xff, 0x5c, 0x78, 0x48, 0xff, 0x40, 0x58, 0x34, 0xff, 0x28, 0x38, 0x20, 0xff, 0x70, 0x60, 0x50, 0xff, 0x54, 0x48, 0x34, 0xff,
    0x38, 0x30, 0x20, 0xff, 0x68, 0x78, 0x50, 0xff, 0x70, 0x78, 0x20, 0xff, 0x70, 0x68, 0x28, 0xff, 0x60, 0x60, 0x24, 0xff, 0x4c, 0x44, 0x24, 0xff, 0x38, 0x30, 0x20, 0xff,
    0x9c, 0xac, 0x9c, 0xff, 0x78, 0x94, 0x78, 0xff, 0x58, 0x7c, 0x58, 0xff, 0x40, 0x68, 0x40, 0xff, 0x38, 0x58, 0x58, 0xff, 0x30, 0x4c, 0x48, 0xff, 0x28, 0x44, 0x3c, 0xff,
    0x20, 0x3c, 0x2c, 0xff, 0x1c, 0x30, 0x24, 0xff, 0x14, 0x28, 0x18, 0xff, 0x10, 0x20, 0x10, 0xff, 0x18, 0x30, 0x18, 0xff, 0x10, 0x24, 0x0c, 0xff, 0x08, 0x1c, 0x04, 0xff,
    0x04, 0x14, 0x00, 0xff, 0x04, 0x0c, 0x00, 0xff, 0x8c, 0x9c, 0x9c, 0xff, 0x78, 0x94, 0x98, 0xff, 0x64, 0x88, 0x94, 0xff, 0x50, 0x7c, 0x90, 0xff, 0x40, 0x6c, 0x8c, 0xff,
    0x30, 0x58, 0x8c, 0xff, 0x2c, 0x4c, 0x7c, 0xff, 0x28, 0x44, 0x6c, 0xff, 0x20, 0x38, 0x5c, 0xff, 0x1c, 0x30, 0x4c, 0xff, 0x18, 0x28, 0x40, 0xff, 0x9c, 0xa4, 0xa4, 0xff,
    0x38, 0x48, 0x68, 0xff, 0x50, 0x58, 0x58, 0xff, 0x58, 0x68, 0x84, 0xff, 0x38, 0x40, 0x50, 0xff, 0xbc, 0xbc, 0xbc, 0xff, 0xac, 0xa4, 0x98, 0xff, 0xa0, 0x90, 0x7c, 0xff,
    0x94, 0x7c, 0x60, 0xff, 0x88, 0x68, 0x4c, 0xff, 0x7c, 0x58, 0x34, 0xff, 0x70, 0x48, 0x24, 0xff, 0x64, 0x3c, 0x14, 0xff, 0x58, 0x30, 0x08, 0xff, 0xfc, 0xcc, 0xcc, 0xff,
    0xfc, 0xb0, 0xb0, 0xff, 0xfc, 0x98, 0x98, 0xff, 0xfc, 0x7c, 0x7c, 0xff, 0xfc, 0x64, 0x64, 0xff, 0xfc, 0x48, 0x48, 0xff, 0xfc, 0x30, 0x30, 0xff, 0xfc, 0x00, 0x00, 0xff,
    0xe0, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x00, 0xff, 0xa8, 0x00, 0x00, 0xff, 0x90, 0x00, 0x00, 0xff, 0x74, 0x00, 0x00, 0xff, 0x58, 0x00, 0x00, 0xff, 0x40, 0x00, 0x00, 0xff,
    0xfc, 0xe0, 0xc8, 0xff, 0xfc, 0xc4, 0x94, 0xff, 0xfc, 0xb8, 0x78, 0xff, 0xfc, 0xac, 0x60, 0xff, 0xfc, 0x9c, 0x48, 0xff, 0xfc, 0x94, 0x2c, 0xff, 0xfc, 0x88, 0x14, 0xff,
    0xfc, 0x7c, 0x00, 0xff, 0xdc, 0x6c, 0x00, 0xff, 0xc0, 0x60, 0x00, 0xff, 0xa4, 0x50, 0x00, 0xff, 0x84, 0x44, 0x00, 0xff, 0x68, 0x34, 0x00, 0xff, 0x4c, 0x24, 0x00, 0xff,
    0x30, 0x18, 0x00, 0xff, 0xf8, 0xd4, 0xa4, 0xff, 0xd8, 0xb0, 0x78, 0xff, 0xc8, 0xa0, 0x64, 0xff, 0xbc, 0x90, 0x54, 0xff, 0xac, 0x80, 0x44, 0xff, 0x9c, 0x74, 0x34, 0xff,
    0x8c, 0x64, 0x28, 0xff, 0x7c, 0x58, 0x1c, 0xff, 0x70, 0x4c, 0x14, 0xff, 0x60, 0x40, 0x08, 0xff, 0x50, 0x34, 0x04, 0xff, 0x40, 0x28, 0x00, 0xff, 0x34, 0x20, 0x00, 0xff,
    0xfc, 0xe4, 0xb8, 0xff, 0xe8, 0xc8, 0x98, 0xff, 0xd4, 0xac, 0x7c, 0xff, 0xc4, 0x90, 0x64, 0xff, 0xb0, 0x74, 0x4c, 0xff, 0xa0, 0x5c, 0x38, 0xff, 0x90, 0x4c, 0x2c, 0xff,
    0x84, 0x3c, 0x20, 0xff, 0x78, 0x2c, 0x18, 0xff, 0x6c, 0x20, 0x10, 0xff, 0x5c, 0x14, 0x08, 0xff, 0x48, 0x0c, 0x04, 0xff, 0x3c, 0x04, 0x00, 0xff, 0xfc, 0xe8, 0xdc, 0xff,
    0xf8, 0xd4, 0xbc, 0xff, 0xf4, 0xc0, 0xa0, 0xff, 0xf0, 0xb0, 0x84, 0xff, 0xf0, 0xa0, 0x6c, 0xff, 0xf0, 0x94, 0x5c, 0xff, 0xd8, 0x80, 0x54, 0xff, 0xc0, 0x70, 0x48, 0xff,
    0xa8, 0x60, 0x40, 0xff, 0x90, 0x50, 0x38, 0xff, 0x78, 0x40, 0x30, 0xff, 0x60, 0x30, 0x24, 0xff, 0x48, 0x24, 0x1c, 0xff, 0x38, 0x18, 0x14, 0xff, 0x64, 0xe4, 0x64, 0xff,
    0x14, 0x98, 0x14, 0xff, 0x00, 0xa4, 0x00, 0xff, 0x50, 0x50, 0x48, 0xff, 0x00, 0x6c, 0x00, 0xff, 0x8c, 0x8c, 0x84, 0xff, 0x1c, 0x1c, 0x1c, 0xff, 0x68, 0x50, 0x38, 0xff,
    0x30, 0x28, 0x20, 0xff, 0x8c, 0x70, 0x60, 0xff, 0x48, 0x38, 0x28, 0xff, 0x0c, 0x0c, 0x0c, 0xff, 0x3c, 0x3c, 0x3c, 0xff, 0x6c, 0x74, 0x6c, 0xff, 0x78, 0x84, 0x78, 0xff,
    0x88, 0x94, 0x88, 0xff, 0x94, 0xa4, 0x94, 0xff, 0x58, 0x68, 0x60, 0xff, 0x60, 0x70, 0x68, 0xff, 0x3c, 0xf8, 0x00, 0xff, 0x38, 0xd4, 0x08, 0xff, 0x34, 0xb4, 0x10, 0xff,
    0x30, 0x94, 0x14, 0xff, 0x28, 0x74, 0x18, 0xff, 0xfc, 0xfc, 0xfc, 0xff, 0xf0, 0xec, 0xd0, 0xff, 0xd0, 0xb8, 0x88, 0xff, 0x98, 0x7c, 0x50, 0xff, 0x68, 0x58, 0x3c, 0xff,
    0x50, 0x40, 0x24, 0xff, 0x34, 0x28, 0x1c, 0xff, 0x18, 0x10, 0x0c, 0xff, 0x03, 0x03, 0x03, 0xff,
    // Slime
    0x00, 0x6c, 0x00, 0xff, 0x0b, 0x73, 0x07, 0xff, 0x1b, 0x7b, 0x0f, 0xff, 0x2b, 0x83, 0x1b, 0xff,
    // Monitors
    0x6b, 0x6b, 0x6f, 0xff, 0x63, 0x67, 0x7f, 0xff, 0x57, 0x6b, 0x8f, 0xff, 0x00, 0x93, 0xa3, 0xff, 0x6b, 0xbb, 0xff, 0xff,
    // FireSlow
    0xff, 0x00, 0x00, 0xff, 0xd7, 0x00, 0x00, 0xff, 0x93, 0x2b, 0x0b, 0xff, 0xff, 0x77, 0x00, 0xff, 0xff, 0x3b, 0x00, 0xff,
    // FireFast
    0x47, 0x00, 0x00, 0xff, 0x7b, 0x00, 0x00, 0xff, 0xb3, 0x00, 0x00, 0xff, 0x7b, 0x00, 0x00, 0xff, 0x47, 0x00, 0x00, 0xff,
    // Shoreline
    0x53, 0x3f, 0x2b, 0xff, 0x4b, 0x3b, 0x2b, 0xff, 0x43, 0x37, 0x27, 0xff, 0x3f, 0x33, 0x27, 0xff, 0x37, 0x2f, 0x23, 0xff, 0x33, 0x2b, 0x23, 0xff,
    // BlinkingRed
    0xfc, 0x00, 0x00, 0xff,
    // Unused
    0xff, 0xff, 0xff, 0xff
};
static_assert(sizeof(FoPalette) == 1024);
    // clang-format on

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        const FileCollection& files = *context.Files;

        ignore_unused(opt);

        reader.SetCurPos(0x4);
        uint16_t frm_fps = reader.GetBEUInt16();

        reader.SetCurPos(0x8);
        uint16_t frm_count = reader.GetBEUInt16();
        FO_VERIFY_AND_THROW(frm_count > 0, "Frame count must be positive", frm_count);

        FrameCollection collection;
        uint16_t sequence_size {};
        uint32_t duration_ms {};
        bool have_dirs {};
        sequence_size = frm_count;
        duration_ms = frm_fps != 0 ? 1000 / frm_fps * frm_count : 0;

        collection.NewName = strvex(fname).starts_with("art/critters/") ? strex(fname).lower().str() : string(fname);

        // Animated palette flags: Slime 0x01, Monitors 0x02, FireSlow 0x04, FireFast 0x08, Shoreline 0x10, BlinkingRed 0x20
        uint32_t anim_pix_type = 0;
        const uint8_t blinking_red_vals[10] = {254, 210, 165, 120, 75, 45, 90, 135, 180, 225};

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
            int32_t dir_frm = dir;

            if constexpr (GameSettings::SQUARE_GEOMETRY) {
                if (dir >= 3) {
                    dir_frm--;
                }
                if (dir >= 7) {
                    dir_frm--;
                }
            }

            reader.SetCurPos(0xA + dir_frm * 2);
            sequence.OffsX = reader.GetBEInt16();
            reader.SetCurPos(0x16 + dir_frm * 2);
            sequence.OffsY = reader.GetBEInt16();

            reader.SetCurPos(0x22 + dir_frm * 4);
            uint32_t offset = 0x3E + reader.GetBEUInt32();

            if (offset == 0x3E && dir_frm != 0) {
                if (dir > 1) {
                    throw ImageBakerException("FRM file truncated", fname);
                }

                break;
            }

            if (dir == 1) {
                have_dirs = true;
            }

            reader.SetCurPos(0x4);
            frm_fps = reader.GetBEUInt16();
            reader.SetCurPos(0x8);
            frm_count = reader.GetBEUInt16();
            FO_VERIFY_AND_THROW(frm_count > 0, "Frame count must be positive", fname, dir, frm_count);
            sequence_size = frm_count;
            sequence.Frames.resize(sequence_size);
            duration_ms = frm_fps != 0 ? std::max(uint32_t {1}, uint32_t {1000} / frm_fps) * frm_count : 0;
            anim_pix_type = 0;

            // Make palette
            const_span<ucolor> palette = bytes_to_objects<ucolor>(span {FoPalette});
            ucolor custom_palette[256];
            File palette_file = files.FindFileByPath(strex("{}.pal", strvex(fname).erase_file_extension()));

            if (palette_file) {
                auto palette_reader = palette_file.GetReader();

                for (auto& color : custom_palette) {
                    uint8_t r = palette_reader.GetUInt8() * 4;
                    uint8_t g = palette_reader.GetUInt8() * 4;
                    uint8_t b = palette_reader.GetUInt8() * 4;
                    color = ucolor {r, g, b, 0xFF};
                }

                custom_palette[0] = ucolor {0};
                palette = const_span<ucolor> {custom_palette};
            }

            for (uint16_t frm = 0; frm < frm_count; frm++) {
                auto& shot = sequence.Frames[frm];

                reader.SetCurPos(offset);
                uint16_t w = reader.GetBEUInt16();
                uint16_t h = reader.GetBEUInt16();
                shot.Size = {w, h};
                reader.GoForward(4); // Frame size
                shot.Motion = {reader.GetBEInt16(), reader.GetBEInt16()};

                // Allocate data
                shot.Data.resize(numeric_cast<size_t>(w) * h * 4);
                span<ucolor> pixels = bytes_to_objects<ucolor>(span {shot.Data});
                reader.SetCurPos(offset + 12);

                if (anim_pix_type == 0) {
                    for (size_t i = 0, j = numeric_cast<size_t>(w * h); i < j; i++) {
                        pixels[i] = palette[reader.GetUInt8()];
                    }
                }
                else {
                    for (size_t i = 0, j = numeric_cast<size_t>(w * h); i < j; i++) {
                        uint8_t index = reader.GetUInt8();

                        if (index >= 229 && index < 255) {
                            if (index >= 229 && index <= 232) {
                                index -= frm % 4;

                                if (index < 229) {
                                    index += 4;
                                }
                            }
                            else if (index >= 233 && index <= 237) {
                                index -= frm % 5;

                                if (index < 233) {
                                    index += 5;
                                }
                            }
                            else if (index >= 238 && index <= 242) {
                                index -= frm % 5;

                                if (index < 238) {
                                    index += 5;
                                }
                            }
                            else if (index >= 243 && index <= 247) {
                                index -= frm % 5;

                                if (index < 243) {
                                    index += 5;
                                }
                            }
                            else if (index >= 248 && index <= 253) {
                                index -= frm % 6;

                                if (index < 248) {
                                    index += 6;
                                }
                            }
                            else {
                                pixels[i] = ucolor {blinking_red_vals[frm % 10], 0, 0};
                                continue;
                            }
                        }

                        pixels[i] = palette[index];
                    }
                }

                // Check for animate pixels
                if (anim_pix_type == 0 && frm == 0 && dir == 0 && frm_count == 1 && palette.data() != custom_palette) {
                    reader.SetCurPos(offset + 12);

                    for (int32_t i = 0; i < w * h; i++) {
                        uint8_t index = reader.GetUInt8();

                        if (index < 229 || index == 255) {
                            continue;
                        }

                        if (index >= 229 && index <= 232) {
                            anim_pix_type |= 0x01;
                        }
                        else if (index >= 233 && index <= 237) {
                            anim_pix_type |= 0x02;
                        }
                        else if (index >= 238 && index <= 242) {
                            anim_pix_type |= 0x04;
                        }
                        else if (index >= 243 && index <= 247) {
                            anim_pix_type |= 0x08;
                        }
                        else if (index >= 248 && index <= 253) {
                            anim_pix_type |= 0x10;
                        }
                        else {
                            anim_pix_type |= 0x20;
                        }
                    }

                    if ((anim_pix_type & 0x01) != 0) {
                        duration_ms = 200;
                    }
                    if ((anim_pix_type & 0x04) != 0) {
                        duration_ms = 200;
                    }
                    if ((anim_pix_type & 0x10) != 0) {
                        duration_ms = 200;
                    }
                    if ((anim_pix_type & 0x08) != 0) {
                        duration_ms = 142;
                    }
                    if ((anim_pix_type & 0x02) != 0) {
                        duration_ms = 100;
                    }
                    if ((anim_pix_type & 0x20) != 0) {
                        duration_ms = 100;
                    }

                    if (anim_pix_type != 0) {
                        int32_t divs[4] {1, 1, 1, 1};

                        if ((anim_pix_type & 0x01) != 0) {
                            divs[0] = 4;
                        }
                        if ((anim_pix_type & 0x02) != 0) {
                            divs[1] = 5;
                        }
                        if ((anim_pix_type & 0x04) != 0) {
                            divs[1] = 5;
                        }
                        if ((anim_pix_type & 0x08) != 0) {
                            divs[1] = 5;
                        }
                        if ((anim_pix_type & 0x10) != 0) {
                            divs[2] = 6;
                        }
                        if ((anim_pix_type & 0x20) != 0) {
                            divs[3] = 10;
                        }

                        frm_count = 4;

                        for (int32_t i = 0; i < 4; i++) {
                            if (frm_count % divs[i] == 0) {
                                continue;
                            }

                            frm_count++;
                            i = -1;
                        }

                        duration_ms *= frm_count;
                        sequence_size = frm_count;
                        sequence.Frames.resize(sequence_size);
                    }
                }

                if (anim_pix_type == 0) {
                    offset += w * h + 12;
                }
            }

            uint32_t count = numeric_cast<uint32_t>(sequence.Frames.size());
            ImageImport::SetFrameSequenceDuration(sequence, std::chrono::milliseconds {duration_ms == 0 ? count * 100 : std::max(duration_ms, count)});
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

            if (std::all_of(sequence.Frames.begin(), sequence.Frames.end(), [](const ImageImport::FrameShot& frame) { return frame.Duration == timespan::zero; })) {
                uint32_t count = numeric_cast<uint32_t>(sequence.Frames.size());
                ImageImport::SetFrameSequenceDuration(sequence, std::chrono::milliseconds {duration_ms == 0 ? count * 100 : std::max(duration_ms, count)});
            }

            (void)ImageImport::GetDuration(sequence);
        }

        return collection;
    }

    auto LoadFrX(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        const FileCollection& files = *context.Files;

        ignore_unused(opt);

        File dir_file;

        // Load from frm
        reader.SetCurPos(0x4);
        uint16_t frm_fps = reader.GetBEUInt16();

        reader.SetCurPos(0x8);
        uint16_t frm_count = reader.GetBEUInt16();
        FO_VERIFY_AND_THROW(frm_count > 0, "Frame count must be positive", frm_count);

        FrameCollection collection;
        uint16_t sequence_size {};
        uint32_t duration_ms {};
        bool have_dirs {};
        sequence_size = frm_count;
        duration_ms = frm_fps != 0 ? 1000 / frm_fps * frm_count : 0;

        if (strvex(fname).starts_with("art/critters/")) {
            collection.NewName = strex("{}.fofrm", strex(fname).erase_file_extension().lower());
        }
        else {
            collection.NewName = strex("{}.frm", strvex(fname).erase_file_extension());
        }

        // Animated palette flags: Slime 0x01, Monitors 0x02, FireSlow 0x04, FireFast 0x08, Shoreline 0x10, BlinkingRed 0x20
        uint32_t anim_pix_type = 0;
        const uint8_t blinking_red_vals[10] = {254, 210, 165, 120, 75, 45, 90, 135, 180, 225};

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
            int32_t dir_frm = dir;

            if constexpr (GameSettings::SQUARE_GEOMETRY) {
                if (dir >= 3) {
                    dir_frm--;
                }
                if (dir >= 7) {
                    dir_frm--;
                }
            }

            if (dir_frm != 0) {
                string next_fname {fname.substr(0, fname.size() - 1)};
                next_fname += numeric_cast<char>('0' + dir_frm);

                dir_file = files.FindFileByPath(next_fname);

                if (dir_file) {
                    reader = dir_file.GetReader();
                }
                else {
                    if (dir > 1) {
                        throw ImageBakerException("FRX file not found", next_fname);
                    }

                    break;
                }
            }

            reader.SetCurPos(0xA + dir_frm * 2);
            sequence.OffsX = reader.GetBEInt16();
            reader.SetCurPos(0x16 + dir_frm * 2);
            sequence.OffsY = reader.GetBEInt16();

            reader.SetCurPos(0x22 + dir_frm * 4);
            uint32_t offset = 0x3E + reader.GetBEUInt32();

            if (dir == 1) {
                have_dirs = true;
            }

            reader.SetCurPos(0x4);
            frm_fps = reader.GetBEUInt16();
            reader.SetCurPos(0x8);
            frm_count = reader.GetBEUInt16();
            FO_VERIFY_AND_THROW(frm_count > 0, "Frame count must be positive", fname, dir, frm_count);
            sequence_size = frm_count;
            sequence.Frames.resize(sequence_size);
            duration_ms = frm_fps != 0 ? std::max(uint32_t {1}, uint32_t {1000} / frm_fps) * frm_count : 0;
            anim_pix_type = 0;

            // Make palette
            const_span<ucolor> palette = bytes_to_objects<ucolor>(span {FoPalette});
            ucolor custom_palette[256];
            File palette_file = files.FindFileByPath(strex("{}.pal", strvex(fname).erase_file_extension()));

            if (palette_file) {
                auto palette_reader = palette_file.GetReader();

                for (auto& color : custom_palette) {
                    uint8_t r = palette_reader.GetUInt8() * 4;
                    uint8_t g = palette_reader.GetUInt8() * 4;
                    uint8_t b = palette_reader.GetUInt8() * 4;
                    color = ucolor {r, g, b};
                }

                custom_palette[0] = ucolor {0};
                palette = const_span<ucolor> {custom_palette};
            }

            for (uint16_t frm = 0; frm < frm_count; frm++) {
                auto& shot = sequence.Frames[frm];

                reader.SetCurPos(offset);
                uint16_t w = reader.GetBEUInt16();
                uint16_t h = reader.GetBEUInt16();
                shot.Size = {w, h};
                reader.GoForward(4); // Frame size
                shot.Motion = {reader.GetBEInt16(), reader.GetBEInt16()};

                // Allocate data
                shot.Data.resize(numeric_cast<size_t>(w) * h * 4);
                span<ucolor> pixels = bytes_to_objects<ucolor>(span {shot.Data});
                reader.SetCurPos(offset + 12);

                if (anim_pix_type == 0) {
                    for (size_t i = 0, j = numeric_cast<size_t>(w * h); i < j; i++) {
                        pixels[i] = palette[reader.GetUInt8()];
                    }
                }
                else {
                    for (size_t i = 0, j = numeric_cast<size_t>(w * h); i < j; i++) {
                        uint8_t index = reader.GetUInt8();

                        if (index >= 229 && index < 255) {
                            if (index >= 229 && index <= 232) {
                                index -= frm % 4;

                                if (index < 229) {
                                    index += 4;
                                }
                            }
                            else if (index >= 233 && index <= 237) {
                                index -= frm % 5;

                                if (index < 233) {
                                    index += 5;
                                }
                            }
                            else if (index >= 238 && index <= 242) {
                                index -= frm % 5;

                                if (index < 238) {
                                    index += 5;
                                }
                            }
                            else if (index >= 243 && index <= 247) {
                                index -= frm % 5;

                                if (index < 243) {
                                    index += 5;
                                }
                            }
                            else if (index >= 248 && index <= 253) {
                                index -= frm % 6;
                                if (index < 248) {
                                    index += 6;
                                }
                            }
                            else {
                                pixels[i] = ucolor {blinking_red_vals[frm % 10], 0, 0};
                                continue;
                            }
                        }

                        pixels[i] = palette[index];
                    }
                }

                // Check for animate pixels
                if (anim_pix_type == 0 && frm == 0 && dir == 0 && frm_count == 1 && palette.data() != custom_palette) {
                    reader.SetCurPos(offset + 12);

                    for (int32_t i = 0; i < w * h; i++) {
                        uint8_t index = reader.GetUInt8();

                        if (index < 229 || index == 255) {
                            continue;
                        }

                        if (index >= 229 && index <= 232) {
                            anim_pix_type |= 0x01;
                        }
                        else if (index >= 233 && index <= 237) {
                            anim_pix_type |= 0x02;
                        }
                        else if (index >= 238 && index <= 242) {
                            anim_pix_type |= 0x04;
                        }
                        else if (index >= 243 && index <= 247) {
                            anim_pix_type |= 0x08;
                        }
                        else if (index >= 248 && index <= 253) {
                            anim_pix_type |= 0x10;
                        }
                        else {
                            anim_pix_type |= 0x20;
                        }
                    }

                    if ((anim_pix_type & 0x01) != 0) {
                        duration_ms = 200;
                    }
                    if ((anim_pix_type & 0x04) != 0) {
                        duration_ms = 200;
                    }
                    if ((anim_pix_type & 0x10) != 0) {
                        duration_ms = 200;
                    }
                    if ((anim_pix_type & 0x08) != 0) {
                        duration_ms = 142;
                    }
                    if ((anim_pix_type & 0x02) != 0) {
                        duration_ms = 100;
                    }
                    if ((anim_pix_type & 0x20) != 0) {
                        duration_ms = 100;
                    }

                    if (anim_pix_type != 0) {
                        int32_t divs[4];
                        divs[0] = 1;
                        divs[1] = 1;
                        divs[2] = 1;
                        divs[3] = 1;

                        if ((anim_pix_type & 0x01) != 0) {
                            divs[0] = 4;
                        }
                        if ((anim_pix_type & 0x02) != 0) {
                            divs[1] = 5;
                        }
                        if ((anim_pix_type & 0x04) != 0) {
                            divs[1] = 5;
                        }
                        if ((anim_pix_type & 0x08) != 0) {
                            divs[1] = 5;
                        }
                        if ((anim_pix_type & 0x10) != 0) {
                            divs[2] = 6;
                        }
                        if ((anim_pix_type & 0x20) != 0) {
                            divs[3] = 10;
                        }

                        frm_count = 4;

                        for (int32_t i = 0; i < 4; i++) {
                            if (frm_count % divs[i] == 0) {
                                continue;
                            }

                            frm_count++;
                            i = -1;
                        }

                        duration_ms *= frm_count;
                        sequence_size = frm_count;
                        sequence.Frames.resize(sequence_size);
                    }
                }

                if (anim_pix_type == 0) {
                    offset += w * h + 12;
                }
            }

            uint32_t count = numeric_cast<uint32_t>(sequence.Frames.size());
            ImageImport::SetFrameSequenceDuration(sequence, std::chrono::milliseconds {duration_ms == 0 ? count * 100 : std::max(duration_ms, count)});
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

            if (std::all_of(sequence.Frames.begin(), sequence.Frames.end(), [](const ImageImport::FrameShot& frame) { return frame.Duration == timespan::zero; })) {
                uint32_t count = numeric_cast<uint32_t>(sequence.Frames.size());
                ImageImport::SetFrameSequenceDuration(sequence, std::chrono::milliseconds {duration_ms == 0 ? count * 100 : std::max(duration_ms, count)});
            }

            (void)ImageImport::GetDuration(sequence);
        }

        return collection;
    }

    auto DiscoverDependencies(string_view fname, const FileCollection& files) -> vector<ImageImport::DependencyReference>
    {
        ignore_unused(files);
        return {{strex("{}.pal", strvex(fname).erase_file_extension()).str(), true}};
    }

    auto DiscoverFrXDependencies(string_view fname, const FileCollection& files) -> vector<ImageImport::DependencyReference>
    {
        auto dependencies = DiscoverDependencies(fname, files);

        for (int32_t direction = 1; direction < 6; direction++) {
            string path = strex("{}{}", fname.substr(0, fname.size() - 1), direction).str();
            dependencies.push_back({path, direction == 1});
            bool exists = std::ranges::any_of(files, [&path](const FileHeader& file) { return file.GetPath() == path; });

            if (direction == 1 && !exists) {
                break;
            }
        }

        return dependencies;
    }

    auto ResolveOutput(string_view fname) -> string
    {
        string ext = strex(fname).get_file_extension();
        bool is_critter = strvex(fname).starts_with("art/critters/");

        if (ext == "frm" && is_critter) {
            return strex(fname).lower();
        }
        if (ext == "fr0") {
            return is_critter ? strex("{}.fofrm", strex(fname).erase_file_extension().lower()).str() : strex("{}.frm", strvex(fname).erase_file_extension()).str();
        }

        return string(fname);
    }
}

FO_END_NAMESPACE

#endif
