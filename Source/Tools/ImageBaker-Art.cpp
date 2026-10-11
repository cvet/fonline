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

        struct ArtHeader
        {
            int32_t Flags {};
            // Flags: 0x01 static, 0x02 critter delta, 0x04 font advance, 0x08 facade walk data, 0x10 eye-candy extension
            int32_t FrameRate {};
            int32_t RotationCount {};
            uint32_t PaletteList[4] {};
            int32_t ActionFrame {};
            int32_t FrameCount {};
            int32_t InfoList[8] {};
            int32_t SizeList[8] {};
            int32_t DataList[8] {};
        };
        ArtHeader header;

        struct ArtFrameInfo
        {
            int32_t FrameWidth {};
            int32_t FrameHeight {};
            int32_t FrameSize {};
            int32_t OffsetX {};
            int32_t OffsetY {};
            int32_t DeltaX {};
            int32_t DeltaY {};
        } frame_info;

        using ArtPalette = uint32_t[256];
        ArtPalette palette[4];

        reader.ReadObject(header);

        if ((header.Flags & 0x00000001) != 0) {
            header.RotationCount = 1;
        }

        // Load palettes
        int32_t palette_count = 0;

        for (int32_t i = 0; i < 4; i++) {
            if (header.PaletteList[i] != 0) {
                reader.ReadObjectArray<uint32_t>(palette[i]);
                palette_count++;
            }
        }
        if (palette_index >= palette_count) {
            palette_index = 0;
        }

        int32_t frm_fps = header.FrameRate;
        int32_t frm_count = header.FrameCount;
        FO_VERIFY_AND_THROW(frm_count > 0, "Frame count must be positive", frm_count);

        if (frm_from >= frm_count) {
            frm_from = frm_count - 1;
        }
        if (frm_to >= frm_count) {
            frm_to = frm_count - 1;
        }

        int32_t frm_count_target = std::max(frm_from, frm_to) - std::min(frm_from, frm_to) + 1;

        // Create animation
        FrameCollection collection;
        collection.SequenceSize = numeric_cast<uint16_t>(frm_count_target);
        collection.AnimTicks = numeric_cast<uint16_t>(frm_fps != 0 ? 1000 / frm_fps * frm_count_target : 0);
        collection.HaveDirs = header.RotationCount == 8;

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
            sequence.Frames.resize(collection.SequenceSize);

            int32_t dir_art = dir;

            if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                switch (dir_art) {
                case 0:
                    dir_art = 1;
                    break;
                case 1:
                    dir_art = 2;
                    break;
                case 2:
                    dir_art = 3;
                    break;
                case 3:
                    dir_art = 5;
                    break;
                case 4:
                    dir_art = 6;
                    break;
                case 5:
                    dir_art = 7;
                    break;
                default:
                    throw ImageBakerException("Invalid ART direction", fname);
                }
            }
            else {
                dir_art = (dir + 1) % 8;
            }

            // Read data
            int32_t frm_read = frm_from;
            int32_t frm_write = 0;

            while (true) {
                reader.SetCurPos(sizeof(ArtHeader) + sizeof(ArtPalette) * palette_count + sizeof(ArtFrameInfo) * dir_art * frm_count + sizeof(ArtFrameInfo) * frm_read);

                reader.ReadObject(frame_info);

                int32_t w = frame_info.FrameWidth;
                int32_t h = frame_info.FrameHeight;
                vector<uint8_t> data(numeric_cast<size_t>(w) * h * 4);
                span<uint32_t> pixels = bytes_to_objects<uint32_t>(span {data});

                auto& shot = sequence.Frames[frm_write];
                shot.Width = numeric_cast<uint16_t>(w);
                shot.Height = numeric_cast<uint16_t>(h);
                shot.NextX = numeric_cast<int16_t>((-frame_info.OffsetX + frame_info.FrameWidth / 2) * (mirror_hor ? -1 : 1));
                shot.NextY = numeric_cast<int16_t>((-frame_info.OffsetY + frame_info.FrameHeight) * (mirror_ver ? -1 : 1));

                uint32_t color = 0;

                auto art_get_color = [&color, &reader, &palette, &palette_index, &transparent]() {
                    uint8_t index = reader.GetUInt8();
                    color = palette[palette_index][index];
                    swap_object_bytes(color, 0, 2);

                    if (index == 0) {
                        color = 0;
                    }
                    else if (transparent) {
                        color |= std::max((color >> 16) & 0xFF, std::max((color >> 8) & 0xFF, color & 0xFF)) << 24;
                    }
                    else {
                        color |= 0xFF000000;
                    }
                };

                int32_t pos = 0;
                int32_t x = 0;
                int32_t y = 0;
                bool mirror = mirror_hor || mirror_ver;

                auto art_write_color = [&color, &pos, &x, &y, &w, &h, &pixels, &mirror, &mirror_hor, &mirror_ver]() {
                    if (mirror) {
                        pixels[(mirror_ver ? h - y - 1 : y) * w + (mirror_hor ? w - x - 1 : x)] = color;
                        x++;
                        if (x >= w) {
                            x = 0;
                            y++;
                        }
                    }
                    else {
                        pixels[pos] = color;
                        pos++;
                    }
                };

                if (w * h == frame_info.FrameSize) {
                    for (int32_t i = 0; i < frame_info.FrameSize; i++) {
                        art_get_color();
                        art_write_color();
                    }
                }
                else {
                    for (int32_t i = 0; i < frame_info.FrameSize; i++) {
                        uint8_t cmd = reader.GetUInt8();

                        if (cmd > 128) {
                            cmd -= 128;
                            i += cmd;
                            for (; cmd > 0; cmd--) {
                                art_get_color();
                                art_write_color();
                            }
                        }
                        else {
                            art_get_color();
                            for (; cmd > 0; cmd--) {
                                art_write_color();
                            }
                            i++;
                        }
                    }
                }

                shot.Data = std::move(data);

                if (frm_read == frm_to) {
                    break;
                }

                if (frm_to > frm_from) {
                    frm_read++;
                }
                else {
                    frm_read--;
                }

                frm_write++;
            }

            if (header.RotationCount != 8) {
                break;
            }
        }

        return collection;
    }
}

FO_END_NAMESPACE

#endif
