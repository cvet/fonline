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

#include "catch_amalgamated.hpp"

#include "DataSerialization.h"
#include "ImageBaker.h"
#include "SpriteResource.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

struct BakedImageFrame
{
    ipos32 Offset {};
    uint16_t Width {};
    uint16_t Height {};
    int16_t NextX {};
    int16_t NextY {};
    uint32_t DurationMs {};
    vector<uint8_t> Data {};
    SpriteMeshKind MeshKind {SpriteMeshKind::Quad};
    SpriteMeshData Mesh {};
};

struct BakedImageSequence
{
    uint16_t SequenceSize {};
    uint32_t AnimTicks {};
    vector<BakedImageFrame> Frames {};
};

static void AppendLe16(vector<uint8_t>& buf, uint16_t value)
{
    buf.emplace_back(numeric_cast<uint8_t>(value & 0xFFu));
    buf.emplace_back(numeric_cast<uint8_t>((value >> 8u) & 0xFFu));
}

static void AppendLeInt16(vector<uint8_t>& buf, int16_t value)
{
    AppendLe16(buf, std::bit_cast<uint16_t>(value));
}

static void AppendLe32(vector<uint8_t>& buf, uint32_t value)
{
    buf.emplace_back(numeric_cast<uint8_t>(value & 0xFFu));
    buf.emplace_back(numeric_cast<uint8_t>((value >> 8u) & 0xFFu));
    buf.emplace_back(numeric_cast<uint8_t>((value >> 16u) & 0xFFu));
    buf.emplace_back(numeric_cast<uint8_t>((value >> 24u) & 0xFFu));
}

static void AppendLeInt32(vector<uint8_t>& buf, int32_t value)
{
    AppendLe32(buf, std::bit_cast<uint32_t>(value));
}

static void AppendBe16(vector<uint8_t>& buf, uint16_t value)
{
    buf.emplace_back(numeric_cast<uint8_t>((value >> 8u) & 0xFFu));
    buf.emplace_back(numeric_cast<uint8_t>(value & 0xFFu));
}

static void AppendBeInt16(vector<uint8_t>& buf, int16_t value)
{
    AppendBe16(buf, std::bit_cast<uint16_t>(value));
}

static void AppendBe32(vector<uint8_t>& buf, uint32_t value)
{
    buf.emplace_back(numeric_cast<uint8_t>((value >> 24u) & 0xFFu));
    buf.emplace_back(numeric_cast<uint8_t>((value >> 16u) & 0xFFu));
    buf.emplace_back(numeric_cast<uint8_t>((value >> 8u) & 0xFFu));
    buf.emplace_back(numeric_cast<uint8_t>(value & 0xFFu));
}

static void SetBe16(vector<uint8_t>& buf, size_t pos, uint16_t value)
{
    buf[pos + 0] = numeric_cast<uint8_t>((value >> 8u) & 0xFFu);
    buf[pos + 1] = numeric_cast<uint8_t>(value & 0xFFu);
}

static void SetBeInt16(vector<uint8_t>& buf, size_t pos, int16_t value)
{
    SetBe16(buf, pos, std::bit_cast<uint16_t>(value));
}

static void SetBe32(vector<uint8_t>& buf, size_t pos, uint32_t value)
{
    buf[pos + 0] = numeric_cast<uint8_t>((value >> 24u) & 0xFFu);
    buf[pos + 1] = numeric_cast<uint8_t>((value >> 16u) & 0xFFu);
    buf[pos + 2] = numeric_cast<uint8_t>((value >> 8u) & 0xFFu);
    buf[pos + 3] = numeric_cast<uint8_t>(value & 0xFFu);
}

static void SetLe32(vector<uint8_t>& buf, size_t pos, uint32_t value)
{
    buf[pos + 0] = numeric_cast<uint8_t>(value & 0xFFu);
    buf[pos + 1] = numeric_cast<uint8_t>((value >> 8u) & 0xFFu);
    buf[pos + 2] = numeric_cast<uint8_t>((value >> 16u) & 0xFFu);
    buf[pos + 3] = numeric_cast<uint8_t>((value >> 24u) & 0xFFu);
}

struct FrmFrameSpec
{
    uint16_t Width {};
    uint16_t Height {};
    int16_t NextX {};
    int16_t NextY {};
    vector<uint8_t> Indices {};
};

static void AppendFrmFrames(vector<uint8_t>& data, const vector<FrmFrameSpec>& frames)
{
    for (const auto& frame : frames) {
        AppendBe16(data, frame.Width);
        AppendBe16(data, frame.Height);
        AppendBe32(data, numeric_cast<uint32_t>(frame.Indices.size()));
        AppendBeInt16(data, frame.NextX);
        AppendBeInt16(data, frame.NextY);
        data.insert(data.end(), frame.Indices.begin(), frame.Indices.end());
    }
}

[[nodiscard]] static auto MakeRawTga(uint16_t width, uint16_t height, uint8_t pixel_depth, const vector<uint8_t>& pixels) -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.reserve(18 + pixels.size());
    data.emplace_back(0); // ID length
    data.emplace_back(0); // No color map
    data.emplace_back(2); // Uncompressed true-color image
    AppendLe16(data, 0);
    AppendLe16(data, 0);
    data.emplace_back(0);
    AppendLe16(data, 0);
    AppendLe16(data, 0);
    AppendLe16(data, width);
    AppendLe16(data, height);
    data.emplace_back(pixel_depth);
    data.emplace_back(0);
    data.insert(data.end(), pixels.begin(), pixels.end());
    return data;
}

[[nodiscard]] static auto MakeRleTga(uint16_t width, uint16_t height, uint8_t pixel_depth, const vector<uint8_t>& packets) -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.reserve(18 + packets.size());
    data.emplace_back(0);
    data.emplace_back(0);
    data.emplace_back(10); // RLE true-color image
    AppendLe16(data, 0);
    AppendLe16(data, 0);
    data.emplace_back(0);
    AppendLe16(data, 0);
    AppendLe16(data, 0);
    AppendLe16(data, width);
    AppendLe16(data, height);
    data.emplace_back(pixel_depth);
    data.emplace_back(0);
    data.insert(data.end(), packets.begin(), packets.end());
    return data;
}

[[nodiscard]] static auto MakeFrm(uint16_t fps, const vector<FrmFrameSpec>& frames, int16_t offs_x = 0, int16_t offs_y = 0) -> vector<uint8_t>
{
    vector<uint8_t> data(0x3E);

    SetBe16(data, 0x4, fps);
    SetBe16(data, 0x8, numeric_cast<uint16_t>(frames.size()));
    SetBeInt16(data, 0xA, offs_x);
    SetBeInt16(data, 0x16, offs_y);
    SetBe32(data, 0x22, 0);
    AppendFrmFrames(data, frames);

    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeFrmWithDirTable(uint16_t fps, const vector<FrmFrameSpec>& frames, int32_t dirs_to_fill) -> vector<uint8_t>
{
    auto data = MakeFrm(fps, frames, 1, -1);

    for (int32_t dir = 1; dir < dirs_to_fill; dir++) {
        size_t dir_pos = numeric_cast<size_t>(dir) * 2;
        size_t data_offset = data.size() - 0x3E;

        SetBeInt16(data, 0xA + dir_pos, numeric_cast<int16_t>(10 + dir));
        SetBeInt16(data, 0x16 + dir_pos, numeric_cast<int16_t>(-10 - dir));
        SetBe32(data, 0x22 + numeric_cast<size_t>(dir) * 4, numeric_cast<uint32_t>(data_offset));
        AppendFrmFrames(data, frames);
    }

    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeFrxDir(uint16_t fps, uint8_t dir, const vector<FrmFrameSpec>& frames, int16_t offs_x, int16_t offs_y) -> vector<uint8_t>
{
    auto data = MakeFrm(fps, frames);
    size_t dir_pos = numeric_cast<size_t>(dir) * 2;

    SetBeInt16(data, 0xA + dir_pos, offs_x);
    SetBeInt16(data, 0x16 + dir_pos, offs_y);
    SetBe32(data, 0x22 + numeric_cast<size_t>(dir) * 4, 0);

    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeFrmPalette() -> vector<uint8_t>
{
    vector<uint8_t> palette(256 * 3);

    for (uint8_t index = 1; index <= GameSettings::MAP_DIR_COUNT; index++) {
        uint8_t color_base = numeric_cast<uint8_t>((index - 1) * 3 + 1);
        size_t palette_pos = numeric_cast<size_t>(index) * 3;
        palette[palette_pos + 0] = color_base;
        palette[palette_pos + 1] = numeric_cast<uint8_t>(color_base + 1);
        palette[palette_pos + 2] = numeric_cast<uint8_t>(color_base + 2);
    }

    return palette;
}

[[nodiscard]] static auto MakePaletteTransparencyPng() -> vector<uint8_t>
{
    return {
        0x89,
        0x50,
        0x4E,
        0x47,
        0x0D,
        0x0A,
        0x1A,
        0x0A,
        0x00,
        0x00,
        0x00,
        0x0D,
        0x49,
        0x48,
        0x44,
        0x52,
        0x00,
        0x00,
        0x00,
        0x02,
        0x00,
        0x00,
        0x00,
        0x01,
        0x01,
        0x03,
        0x00,
        0x00,
        0x00,
        0xCE,
        0xEC,
        0xED,
        0xC9,
        0x00,
        0x00,
        0x00,
        0x06,
        0x50,
        0x4C,
        0x54,
        0x45,
        0xFF,
        0x00,
        0x00,
        0x00,
        0xFF,
        0x00,
        0xD2,
        0x87,
        0xEF,
        0x71,
        0x00,
        0x00,
        0x00,
        0x02,
        0x74,
        0x52,
        0x4E,
        0x53,
        0x00,
        0xFF,
        0x5B,
        0x91,
        0x22,
        0xB5,
        0x00,
        0x00,
        0x00,
        0x0A,
        0x49,
        0x44,
        0x41,
        0x54,
        0x78,
        0x9C,
        0x63,
        0x70,
        0x00,
        0x00,
        0x00,
        0x42,
        0x00,
        0x41,
        0x29,
        0x37,
        0xF4,
        0xEF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x49,
        0x45,
        0x4E,
        0x44,
        0xAE,
        0x42,
        0x60,
        0x82,
    };
}

[[maybe_unused]] [[nodiscard]] static auto MakeRix() -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.resize(0xA + 256 * 3 + 1);
    data[4] = 1;
    data[6] = 1;

    size_t palette_index = 0xA + 1 * 3;
    data[palette_index + 0] = 1;
    data[palette_index + 1] = 2;
    data[palette_index + 2] = 3;
    data[0xA + 256 * 3] = 1;

    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeSimpleZar() -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.insert(data.end(), {'<', 'z', 'a', 'r', '>', '\0'});
    data.emplace_back(0);
    data.emplace_back(0);
    AppendLe32(data, 1);
    AppendLe32(data, 1);
    data.emplace_back(0);
    AppendLe32(data, 1);
    data.emplace_back(4);
    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeZarWithPaletteCount(uint32_t palette_count) -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.insert(data.end(), {'<', 'z', 'a', 'r', '>', '\0'});
    data.emplace_back(0x34);
    data.emplace_back(0);
    AppendLe32(data, 1);
    AppendLe32(data, 1);
    data.emplace_back(1);
    AppendLe32(data, palette_count);
    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakePaletteZar() -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.insert(data.end(), {'<', 'z', 'a', 'r', '>', '\0'});
    data.emplace_back(0x34);
    data.emplace_back(0);
    AppendLe32(data, 3);
    AppendLe32(data, 1);
    data.emplace_back(1);
    AppendLe32(data, 2);
    AppendLe32(data, 0);
    AppendLe32(data, 0x00332211);
    data.emplace_back(1);

    vector<uint8_t> rle = {0x05, 1, 0x06, 1, 0x7F, 0x07, 0x40};
    AppendLe32(data, numeric_cast<uint32_t>(rle.size()));
    data.insert(data.end(), rle.begin(), rle.end());
    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeSimpleMos() -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.insert(data.end(), {'M', 'O', 'S', ' ', '\0', '\0', '\0', '\0'});
    AppendLe16(data, 2);
    AppendLe16(data, 1);
    AppendLe16(data, 1);
    AppendLe16(data, 1);
    AppendLe32(data, 64);
    AppendLe32(data, 24);

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t color = 0;

        if (i == 1) {
            color = 0x00030201;
        }
        else if (i == 2) {
            color = 0x0000FF00;
        }

        AppendLe32(data, color);
    }

    AppendLe32(data, 0);
    data.emplace_back(1);
    data.emplace_back(2);
    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeSimpleBam() -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.insert(data.end(), {'B', 'A', 'M', ' ', '\0', '\0', '\0', '\0'});
    AppendLe16(data, 1);
    data.emplace_back(1);
    data.emplace_back(3);
    AppendLe32(data, 24);
    AppendLe32(data, 40);
    AppendLe32(data, 1064);

    AppendLe16(data, 4);
    AppendLe16(data, 1);
    AppendLe16(data, 1);
    AppendLe16(data, 2);
    AppendLe32(data, 1066);

    AppendLe16(data, 1);
    AppendLe16(data, 0);

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t color = 0;

        if (i == 1) {
            color = 0x00332211;
        }
        else if (i == 2) {
            color = 0x00FF0807;
        }
        else if (i == 3) {
            color = 0x00060504;
        }

        AppendLe32(data, color);
    }

    AppendLe16(data, 0);
    data.emplace_back(1);
    data.emplace_back(2);
    data.emplace_back(3);
    data.emplace_back(1);
    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeMultiFrameBam() -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.insert(data.end(), {'B', 'A', 'M', ' ', '\0', '\0', '\0', '\0'});
    AppendLe16(data, 2);
    data.emplace_back(1);
    data.emplace_back(3);
    AppendLe32(data, 24);
    AppendLe32(data, 52);
    AppendLe32(data, 1076);

    AppendLe16(data, 1);
    AppendLe16(data, 1);
    AppendLe16(data, 0);
    AppendLe16(data, 0);
    AppendLe32(data, 1080);

    AppendLe16(data, 2);
    AppendLe16(data, 1);
    AppendLe16(data, 2);
    AppendLe16(data, 3);
    AppendLe32(data, 0x80000000U | 1081U);

    AppendLe16(data, 2);
    AppendLe16(data, 0);

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t color = 0;

        if (i == 1) {
            color = 0x00332211;
        }
        else if (i == 3) {
            color = 0x00060504;
        }

        AppendLe32(data, color);
    }

    AppendLe16(data, 0);
    AppendLe16(data, 1);
    data.emplace_back(1);
    data.emplace_back(3);
    data.emplace_back(1);
    return data;
}

static void AppendTilPrefix(vector<uint8_t>& data)
{
    data.insert(data.end(), {'<', 't', 'i', 'l', 'e', '>', '\0'});
    data.emplace_back(0);
    data.insert(data.end(), 11, 0);
    AppendLe32(data, 1);
    AppendLe32(data, 1);
}

[[maybe_unused]] [[nodiscard]] static auto MakeSimpleTil() -> vector<uint8_t>
{
    vector<uint8_t> data;
    AppendTilPrefix(data);
    SetLe32(data, data.size() - 8, 3);
    data.insert(data.end(), {'<', 't', 'i', 'l', 'e', 'd', 'a', 't', 'a', '>'});
    data.insert(data.end(), 3, 0);
    AppendLe32(data, 1);

    data.insert(data.end(), {'<', 'z', 'a', 'r', '>', '\0'});
    data.emplace_back(0x34);
    data.emplace_back(0);
    AppendLe32(data, 3);
    AppendLe32(data, 1);
    data.emplace_back(1);
    AppendLe32(data, 2);
    AppendLe32(data, 0);
    AppendLe32(data, 0x00332211);
    data.emplace_back(1);

    vector<uint8_t> rle = {0x05, 1, 0x06, 1, 0x7F, 0x07, 0x40};
    AppendLe32(data, numeric_cast<uint32_t>(rle.size()));
    data.insert(data.end(), rle.begin(), rle.end());
    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeTilWithoutTiledata() -> vector<uint8_t>
{
    vector<uint8_t> data;
    AppendTilPrefix(data);
    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeTilWithBadZarHeader() -> vector<uint8_t>
{
    vector<uint8_t> data;
    AppendTilPrefix(data);
    data.insert(data.end(), {'<', 't', 'i', 'l', 'e', 'd', 'a', 't', 'a', '>'});
    data.insert(data.end(), 3, 0);
    AppendLe32(data, 1);
    data.insert(data.end(), {'b', 'a', 'd', 'z', 'a', 'r'});
    return data;
}

[[maybe_unused]] [[nodiscard]] static auto MakeTilWithPaletteCount(uint32_t palette_count) -> vector<uint8_t>
{
    vector<uint8_t> data;
    AppendTilPrefix(data);
    data.insert(data.end(), {'<', 't', 'i', 'l', 'e', 'd', 'a', 't', 'a', '>'});
    data.insert(data.end(), 3, 0);
    AppendLe32(data, 1);
    data.insert(data.end(), {'<', 'z', 'a', 'r', '>', '\0'});
    data.emplace_back(0x34);
    data.emplace_back(0);
    AppendLe32(data, 1);
    AppendLe32(data, 1);
    data.emplace_back(1);
    AppendLe32(data, palette_count);
    return data;
}

#if FO_IMAGE_ART
// Arcanum ART layout (arcanum-ce first_party/tig/src/art.c): 132-byte header, one 256-entry palette per present
// slot, the 28-byte frame headers of all stored rotations, then the pixel data of all stored rotations and frames
struct ArtFrameSpec
{
    int32_t Width {};
    int32_t Height {};
    int32_t HotX {};
    int32_t HotY {};
    int32_t DeltaX {};
    int32_t DeltaY {};
    vector<uint8_t> Indices {};
    vector<uint8_t> Stored {}; // Explicit stored bytes (RLE); empty means raw indices
};

// Palette slot s maps color index i to 0x00RRGGBB with RR = s * 64 + i * 4, GG = i * 4 + 1, BB = i * 4 + 2
[[nodiscard]] static auto ArtTestColor(int32_t slot, int32_t index) -> vector<uint8_t>
{
    return {numeric_cast<uint8_t>(slot * 64 + index * 4), numeric_cast<uint8_t>(index * 4 + 1), numeric_cast<uint8_t>(index * 4 + 2)};
}

[[nodiscard]] static auto MakeArt(uint32_t flags, int32_t fps, const array<bool, 4>& palettes, int32_t frame_count, const vector<ArtFrameSpec>& frames) -> vector<uint8_t>
{
    vector<uint8_t> data;
    AppendLe32(data, flags);
    AppendLeInt32(data, fps);
    AppendLeInt32(data, 8); // Bits per pixel

    for (bool present : palettes) {
        AppendLe32(data, present ? 1 : 0);
    }

    AppendLeInt32(data, 0); // Action frame
    AppendLeInt32(data, frame_count);

    for (int32_t i = 0; i < 24; i++) {
        AppendLeInt32(data, 0); // Frame table pointers, data sizes, pixel table pointers
    }

    for (int32_t slot = 0; slot < 4; slot++) {
        if (palettes[numeric_cast<size_t>(slot)]) {
            for (int32_t i = 0; i < 256; i++) {
                uint32_t rr = numeric_cast<uint32_t>((slot * 64 + i * 4) & 0xFF);
                uint32_t gg = numeric_cast<uint32_t>((i * 4 + 1) & 0xFF);
                uint32_t bb = numeric_cast<uint32_t>((i * 4 + 2) & 0xFF);
                AppendLe32(data, (rr << 16) | (gg << 8) | bb);
            }
        }
    }

    for (const ArtFrameSpec& frame : frames) {
        int32_t stored_size = numeric_cast<int32_t>(frame.Stored.empty() ? frame.Indices.size() : frame.Stored.size());
        AppendLeInt32(data, frame.Width);
        AppendLeInt32(data, frame.Height);
        AppendLeInt32(data, stored_size);
        AppendLeInt32(data, frame.HotX);
        AppendLeInt32(data, frame.HotY);
        AppendLeInt32(data, frame.DeltaX);
        AppendLeInt32(data, frame.DeltaY);
    }

    for (const ArtFrameSpec& frame : frames) {
        const vector<uint8_t>& bytes = frame.Stored.empty() ? frame.Indices : frame.Stored;
        data.insert(data.end(), bytes.begin(), bytes.end());
    }

    return data;
}

[[nodiscard]] static auto MakeSimpleArt() -> vector<uint8_t>
{
    return MakeArt(0x1, 10, {true, true, false, false}, 1, {ArtFrameSpec {.Width = 2, .Height = 1, .HotX = 0, .HotY = 1, .DeltaX = 3, .DeltaY = -2, .Indices = {1, 2}}});
}

[[nodiscard]] static auto MakeRleFrameRangeArt() -> vector<uint8_t>
{
    // Frame 1 RLE: run of two index 1, then a one-byte literal of index 2
    return MakeArt(0x1, 10, {true, false, false, false}, 2,
        {
            ArtFrameSpec {},
            ArtFrameSpec {.Width = 3, .Height = 1, .HotX = 1, .HotY = 1, .DeltaX = 2, .DeltaY = -1, .Stored = {0x02, 0x01, 0x81, 0x02}},
        });
}

[[nodiscard]] static auto MakeRotationArt() -> vector<uint8_t>
{
    vector<ArtFrameSpec> frames;

    for (int32_t rotation = 0; rotation < 8; rotation++) {
        frames.emplace_back(ArtFrameSpec {.Width = 1, .Height = 1, .DeltaX = rotation, .DeltaY = -rotation, .Indices = {numeric_cast<uint8_t>(rotation + 1)}});
    }

    return MakeArt(0, 5, {true, false, false, false}, 1, frames);
}

#endif

static void AppendSprPalette(vector<uint8_t>& data, uint32_t color)
{
    AppendLe32(data, 2);
    AppendLe32(data, 0);
    AppendLe32(data, color);
}

static void AppendSprImage(vector<uint8_t>& data, int32_t pos_x, int32_t pos_y, int32_t width, int32_t height, const vector<uint8_t>& rle)
{
    data.emplace_back(1);
    AppendLeInt32(data, pos_x);
    AppendLeInt32(data, pos_y);
    data.insert(data.end(), {'<', 'z', 'a', 'r', '>', '\0'});
    data.emplace_back(0x34);
    data.emplace_back(0);
    AppendLeInt32(data, width);
    AppendLeInt32(data, height);
    data.emplace_back(0);
    AppendLe32(data, numeric_cast<uint32_t>(rle.size()));
    data.insert(data.end(), rle.begin(), rle.end());
}

[[nodiscard]] static auto MakeSprWithSequenceFrames(const vector<int16_t>& sequence_frames, int32_t dir_count = 1) -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.insert(data.end(), {'<', 's', 'p', 'r', 'i', 't', 'e', '>', '\0', '\0', '\0'});
    data.emplace_back(0); // Left dimension
    data.emplace_back(0); // Up dimension
    data.emplace_back(0); // Right dimension
    AppendLeInt32(data, 0); // Center X
    AppendLeInt32(data, 0); // Center Y
    AppendLe16(data, 0);
    data.emplace_back(0x64);

    AppendLeInt32(data, 1); // Sequence count
    AppendLeInt32(data, numeric_cast<int32_t>(sequence_frames.size())); // Sequence item count

    for (int16_t frame : sequence_frames) {
        AppendLeInt16(data, frame);
    }

    for (size_t i = 0; i < sequence_frames.size(); i++) {
        AppendLeInt32(data, 0);
    }

    AppendLeInt32(data, 4);
    data.insert(data.end(), {'w', 'a', 'l', 'k'});
    AppendLe16(data, 0); // Animation index

    data.insert(data.end(), {'<', 's', 'p', 'r', 'a', 'n', 'i', 'm', '>', '\0', '\0', '\0'});
    size_t file_offset_pos = data.size();
    AppendLe32(data, 0);
    AppendLeInt32(data, 0); // Collection name length
    AppendLeInt32(data, 1); // Frame count
    AppendLeInt32(data, dir_count); // Direction count

    for (int32_t i = 0; i < dir_count; i++) {
        AppendLeInt32(data, 0); // BBox x
        AppendLeInt32(data, 0); // BBox y
        AppendLeInt32(data, 0);
        AppendLeInt32(data, 0);
    }

    SetLe32(data, file_offset_pos, numeric_cast<uint32_t>(data.size()));

    data.insert(data.end(), {'<', 's', 'p', 'r', 'a', 'n', 'i', 'm', '_', 'i', 'm', 'g', '>', '\0'});
    data.emplace_back(0x31);
    data.emplace_back(0);

    AppendSprPalette(data, 0x00332211);
    AppendSprPalette(data, 0x00281E14);
    AppendSprPalette(data, 0);
    AppendSprPalette(data, 0);

    AppendSprImage(data, 0, 0, 3, 1, vector<uint8_t> {0x05, 1, 0x06, 1, 0x7F, 0x07, 0x40});
    AppendSprImage(data, 1, 0, 2, 1, vector<uint8_t> {0x05, 1, 0x06, 1, 0x7F});
    data.emplace_back(0);
    data.emplace_back(0);

    return data;
}

[[maybe_unused]] static void ReplaceFirstTagByte(vector<uint8_t>& data, string_view tag, uint8_t replacement)
{
    auto it = std::search(data.begin(), data.end(), tag.begin(), tag.end(), [](uint8_t lhs, char rhs) { return lhs == numeric_cast<uint8_t>(rhs); });
    FO_VERIFY_AND_THROW(it != data.end(), "Test tag not found", tag);
    *it = replacement;
}

[[maybe_unused]] [[nodiscard]] static auto MakeSimpleSpr() -> vector<uint8_t>
{
    return MakeSprWithSequenceFrames({0});
}

static void AddSourceBinaryFile(BakerTests::TestRig& rig, string_view path, const vector<uint8_t>& data, uint64_t write_time = 1)
{
    string bytes;
    bytes.resize(data.size());

    if (!data.empty()) {
        memory::copy(bytes.data(), data.data(), data.size());
    }

    rig.AddSourceFile(path, bytes, write_time);
}

[[nodiscard]] static auto ReadDirectionSequence(const SpriteResourceData& resource, size_t dir) -> BakedImageSequence
{
    const auto& direction = resource.Directions[dir];
    BakedImageSequence result;
    result.SequenceSize = numeric_cast<uint16_t>(direction.Frames.size());
    result.AnimTicks = GetDurationMs(resource.Animation.Sprite->Directions[dir]);

    for (size_t index = 0; index < direction.Frames.size(); index++) {
        const auto& slot = direction.Frames[index];
        size_t leaf = index;

        while (direction.Frames[leaf].SharedFrameIndex) {
            leaf = *direction.Frames[leaf].SharedFrameIndex;
        }

        const auto& pixels = direction.Frames[leaf];
        BakedImageFrame frame;
        frame.Offset = slot.Offset;
        frame.Width = numeric_cast<uint16_t>(slot.Size.width);
        frame.Height = numeric_cast<uint16_t>(slot.Size.height);
        frame.NextX = numeric_cast<int16_t>(slot.NextOffset.x);
        frame.NextY = numeric_cast<int16_t>(slot.NextOffset.y);
        frame.DurationMs = slot.DurationMs;

        for (ucolor pixel : pixels.Pixels) {
            frame.Data.insert(frame.Data.end(), {pixel.comp.r, pixel.comp.g, pixel.comp.b, pixel.comp.a});
        }

        frame.MeshKind = !pixels.Mesh ? SpriteMeshKind::Quad : pixels.Mesh->Indices.empty() ? SpriteMeshKind::Empty : SpriteMeshKind::Mesh;

        if (pixels.Mesh) {
            frame.Mesh = *pixels.Mesh;
        }

        result.Frames.emplace_back(std::move(frame));
    }

    return result;
}

[[nodiscard]] static auto ReadSingleDirSequence(const vector<uint8_t>& baked_data) -> BakedImageSequence
{
    auto resource = ReadSpriteResource(baked_data);
    REQUIRE(resource.Directions.size() == 1);

    return ReadDirectionSequence(resource, 0);
}

[[nodiscard]] static auto ReadSingleFrame(const vector<uint8_t>& baked_data) -> BakedImageFrame
{
    auto sequence = ReadSingleDirSequence(baked_data);
    REQUIRE(sequence.Frames.size() == 1);

    return sequence.Frames.front();
}

static void ConfigureSpriteMesh(BakerTests::TestRig& rig, bool enabled = true, int32_t alpha_threshold = 0, int32_t max_triangles = 4096, float32_t area_savings_weight = 64.0f)
{
    BakerTests::OverrideSetting(rig.Settings.SpriteMesh.Enabled, enabled);
    BakerTests::OverrideSetting(rig.Settings.SpriteMesh.AlphaThreshold, alpha_threshold);
    BakerTests::OverrideSetting(rig.Settings.SpriteMesh.MaxTriangles, max_triangles);
    BakerTests::OverrideSetting(rig.Settings.SpriteMesh.AreaSavingsWeight, area_savings_weight);
}

[[nodiscard]] static auto BakeAlphaMask(BakerTests::TestRig& rig, const vector<string>& rows, string_view path = "gfx/mesh.mask", ipos32 offset = {}) -> vector<uint8_t>
{
    REQUIRE(!rows.empty());
    REQUIRE(!rows.front().empty());

    uint16_t width = numeric_cast<uint16_t>(rows.front().size());
    uint16_t height = numeric_cast<uint16_t>(rows.size());
    vector<uint8_t> rgba(numeric_cast<size_t>(width) * height * 4);

    for (uint16_t y = 0; y < height; y++) {
        REQUIRE(rows[y].size() == width);

        for (uint16_t x = 0; x < width; x++) {
            uint8_t alpha = 0;

            if (rows[y][x] == '#') {
                alpha = 255;
            }
            else if (rows[y][x] == '+') {
                alpha = 128;
            }
            else {
                REQUIRE(rows[y][x] == '.');
            }

            size_t pixel_pos = (numeric_cast<size_t>(y) * width + x) * 4;
            rgba[pixel_pos + 0] = 255;
            rgba[pixel_pos + 1] = 255;
            rgba[pixel_pos + 2] = 255;
            rgba[pixel_pos + 3] = alpha;
        }
    }

    rig.AddSourceFile(path, "mask");

    ImageBaker baker {rig.MakeContext()};
    baker.AddLoader(
        [width, height, rgba, offset](string_view fname, string_view opt, FileReader reader, const ImageImport::Context& files) {
            ignore_unused(fname, opt, reader, files);

            ImageImport::FrameCollection collection;
            [[maybe_unused]] uint16_t sequence_size {};
            uint32_t duration_ms {100};
            bool have_dirs {};
            sequence_size = 1;
            duration_ms = 100;
            collection.Main.OffsX = numeric_cast<int16_t>(offset.x);
            collection.Main.OffsY = numeric_cast<int16_t>(offset.y);
            collection.Main.Frames.resize(1);
            collection.Main.Frames[0] = ImageImport::FrameShot {.Width = width, .Height = height, .Data = rgba};

            if (!have_dirs) {
                for (auto& direction : collection.Dirs) {
                    direction.Frames.clear();
                }
            }
            else {
                for (int32_t direction = GameSettings::MAP_DIR_COUNT - 1; direction < ImageImport::MAX_DIRS_MINUS_ONE; direction++) {
                    collection.Dirs[direction].Frames.clear();
                }
            }

            for (int32_t direction = 0; direction < (have_dirs ? GameSettings::MAP_DIR_COUNT : 1); direction++) {
                auto& frames = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

                for (size_t index = 0; index < frames.Frames.size(); index++) {
                    auto& shot = frames.Frames[index];

                    if (shot.Shared && shot.SharedIndex < index) {
                        uint16_t reference = shot.SharedIndex;
                        shot = frames.Frames[reference];
                        shot.Data.clear();
                        shot.Shared = true;
                        shot.SharedIndex = reference;
                    }
                }

                ImageImport::SetFrameSequenceDuration(frames, duration_ms);
            }

            return collection;
        },
        {"mask"});
    baker.BakeFiles(rig.GetAllSourceFiles(), path);

    REQUIRE(rig.Outputs.contains(string(path)));
    return rig.Outputs.at(string(path));
}

[[nodiscard]] static auto BakedMeshDoubleArea(const BakedImageFrame& frame) -> int64_t
{
    int64_t double_area = 0;

    for (size_t i = 0; i < frame.Mesh.Indices.size(); i += 3) {
        REQUIRE(i + 2 < frame.Mesh.Indices.size());
        ipos32 a = frame.Mesh.Vertices.at(frame.Mesh.Indices[i]);
        ipos32 b = frame.Mesh.Vertices.at(frame.Mesh.Indices[i + 1]);
        ipos32 c = frame.Mesh.Vertices.at(frame.Mesh.Indices[i + 2]);
        int64_t cross = numeric_cast<int64_t>(b.x - a.x) * (c.y - a.y) - numeric_cast<int64_t>(b.y - a.y) * (c.x - a.x);
        double_area += std::abs(cross);
    }

    return double_area;
}

[[nodiscard]] static auto IsPointInsideBakedMesh(const BakedImageFrame& frame, fpos32 point) -> bool
{
    for (size_t i = 0; i < frame.Mesh.Indices.size(); i += 3) {
        ipos32 a = frame.Mesh.Vertices.at(frame.Mesh.Indices[i]);
        ipos32 b = frame.Mesh.Vertices.at(frame.Mesh.Indices[i + 1]);
        ipos32 c = frame.Mesh.Vertices.at(frame.Mesh.Indices[i + 2]);
        float32_t cross_ab = numeric_cast<float32_t>(b.x - a.x) * (point.y - a.y) - numeric_cast<float32_t>(b.y - a.y) * (point.x - a.x);
        float32_t cross_bc = numeric_cast<float32_t>(c.x - b.x) * (point.y - b.y) - numeric_cast<float32_t>(c.y - b.y) * (point.x - b.x);
        float32_t cross_ca = numeric_cast<float32_t>(a.x - c.x) * (point.y - c.y) - numeric_cast<float32_t>(a.y - c.y) * (point.x - c.x);
        bool has_negative = cross_ab < 0.0f || cross_bc < 0.0f || cross_ca < 0.0f;
        bool has_positive = cross_ab > 0.0f || cross_bc > 0.0f || cross_ca > 0.0f;

        if (!(has_negative && has_positive)) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] static auto SourcePointToBakedMeshPoint(const BakedImageFrame& frame, fpos32 source_point) -> fpos32
{
    return {source_point.x - numeric_cast<float32_t>(frame.Mesh.SourceOffset.x), source_point.y - numeric_cast<float32_t>(frame.Mesh.SourceOffset.y)};
}

TEST_CASE("ImageBaker")
{
    using namespace BakerTests;
    string_view fofrm_direction_prefix = FO_GEOMETRY == 1 ? "HexDir" : "SquareDir";

#if FO_IMAGE_ART
    SECTION("BakesStaticArtHotSpotAsSequenceOffset")
    {
        // A frame's hot spot is the map anchor: the engine draws the sprite bottom-centre at hex + offset
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/static.art", MakeSimpleArt());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/static.art");

        REQUIRE(rig.Outputs.contains("gfx/static.art"));
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/static.art"));

        CHECK(sequence.SequenceSize == 1);
        CHECK(sequence.AnimTicks == 100);
        REQUIRE(sequence.Frames.size() == 1);
        CHECK(sequence.Frames[0].Offset == ipos32 {1, 0});
        CHECK(sequence.Frames[0].Width == 2);
        CHECK(sequence.Frames[0].Height == 1);
        CHECK(sequence.Frames[0].NextX == 3);
        CHECK(sequence.Frames[0].NextY == -2);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0x04, 0x05, 0x06, 0xFF, 0x08, 0x09, 0x0A, 0xFF});
    }
#endif

#if FO_IMAGE_ART
    SECTION("RejectsArtRleOverrunningItsFrame")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/broken.art", MakeArt(0x1, 10, {true, false, false, false}, 1, {ArtFrameSpec {.Width = 2, .Height = 1, .Stored = {0x05, 0x01, 0x00}}}));

        ImageBaker baker {rig.MakeContext()};
        CHECK_THROWS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/broken.art"));
    }
#endif

#if FO_IMAGE_ART
    SECTION("BakesFofrmAliasOfSingleArtRotation")
    {
        for (string_view dir : {string_view {"gfx"}, string_view {"art/critter"}}) {
            bool critter = dir == "art/critter";
            string fofrm_path = strex("{}/dirs_r3.fofrm", dir).str();

            TestRig rig;
            rig.AddSourceFile(fofrm_path, "Source = dirs$r=3.art\n");
            AddSourceBinaryFile(rig, strex("{}/dirs.art", dir).str(), MakeRotationArt());

            ImageBaker baker {rig.MakeContext()};
            baker.BakeFiles(rig.GetAllSourceFiles(), fofrm_path);

            REQUIRE(rig.Outputs.contains(fofrm_path));
            auto sequence = ReadSingleDirSequence(rig.Outputs.at(fofrm_path));
            int32_t source = critter ? 5 : 3;

            CHECK(sequence.SequenceSize == 1);
            CHECK(sequence.AnimTicks == 200);
            REQUIRE(sequence.Frames.size() == 1);
            CHECK(sequence.Frames[0].Offset == ipos32 {0, 1});
            CHECK(sequence.Frames[0].NextX == (critter ? -source : source));
            CHECK(sequence.Frames[0].NextY == -source);

            vector<uint8_t> expected = ArtTestColor(0, source + 1);
            expected.emplace_back(255);
            CHECK(sequence.Frames[0].Data == expected);
        }
    }
#endif

#if FO_IMAGE_ART
    SECTION("SelectsArtRotationInOrdinaryFofrmFrameOptions")
    {
        vector<ArtFrameSpec> frames;

        for (int32_t rotation = 0; rotation < 8; rotation++) {
            frames.emplace_back(ArtFrameSpec {.Width = 1, .Height = 1, .DeltaX = rotation, .DeltaY = -rotation, .Indices = {numeric_cast<uint8_t>(rotation + 1)}});
        }

        TestRig rig;
        rig.AddSourceFile("gfx/options.fofrm", "Frame = dirs$3R=3Hf0.art\n");
        AddSourceBinaryFile(rig, "gfx/dirs.art", MakeArt(0, 10, {true, true, true, true}, 1, frames));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/options.fofrm");

        auto frame = ReadSingleFrame(rig.Outputs.at("gfx/options.fofrm"));
        vector<uint8_t> expected = ArtTestColor(3, 4);
        expected.emplace_back(255);
        expected.insert(expected.begin(), 4, 0);
        CHECK(frame.Width == 2);
        CHECK(frame.Height == 1);
        CHECK(frame.Data == expected);
        CHECK(frame.NextX == -3);
        CHECK(frame.NextY == -3);
    }
#endif

    SECTION("ForwardsAliasOptionsThroughRegisteredLoader")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/custom.fofrm", "Source = custom$r=3.toy\n");
        rig.AddSourceFile("gfx/custom.toy", "Custom source");
        ImageBaker baker {rig.MakeContext()};
        string observed_name;
        string observed_options;
        baker.AddLoader(
            [&observed_name, &observed_options](string_view fname, string_view opt, FileReader reader, const ImageImport::Context& files) {
                ignore_unused(reader, files);
                observed_name = fname;
                observed_options = opt;
                ImageImport::FrameCollection collection;
                [[maybe_unused]] uint16_t sequence_size {};
                uint32_t duration_ms {100};
                bool have_dirs {};
                sequence_size = 1;
                duration_ms = 123;
                collection.Main.Frames.emplace_back(ImageImport::FrameShot {.Width = 1, .Height = 1, .Data = {1, 2, 3, 255}});

                if (!have_dirs) {
                    for (auto& direction : collection.Dirs) {
                        direction.Frames.clear();
                    }
                }
                else {
                    for (int32_t direction = GameSettings::MAP_DIR_COUNT - 1; direction < ImageImport::MAX_DIRS_MINUS_ONE; direction++) {
                        collection.Dirs[direction].Frames.clear();
                    }
                }

                for (int32_t direction = 0; direction < (have_dirs ? GameSettings::MAP_DIR_COUNT : 1); direction++) {
                    auto& frames = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

                    for (size_t index = 0; index < frames.Frames.size(); index++) {
                        auto& shot = frames.Frames[index];

                        if (shot.Shared && shot.SharedIndex < index) {
                            uint16_t reference = shot.SharedIndex;
                            shot = frames.Frames[reference];
                            shot.Data.clear();
                            shot.Shared = true;
                            shot.SharedIndex = reference;
                        }
                    }

                    ImageImport::SetFrameSequenceDuration(frames, duration_ms);
                }

                return collection;
            },
            {"toy"});
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/custom.fofrm");

        CHECK(observed_name == "gfx/custom.toy");
        CHECK(observed_options == "r=3");
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/custom.fofrm"));
        CHECK(sequence.AnimTicks == 123);
        CHECK(sequence.Frames.at(0).Data == vector<uint8_t> {1, 2, 3, 255});
    }

#if FO_IMAGE_ART
    SECTION("PreservesLegacyUnknownRAndPaletteSelection")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/palette.fofrm", "Source = palette$r3.art\n");
        AddSourceBinaryFile(rig, "gfx/palette.art", MakeArt(1, 10, {true, true, true, true}, 1, {ArtFrameSpec {.Width = 1, .Height = 1, .Indices = {1}}}));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/palette.fofrm");

        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/palette.fofrm"));
        vector<uint8_t> expected = ArtTestColor(3, 1);
        expected.emplace_back(255);
        CHECK(sequence.Frames.at(0).Data == expected);
    }
#endif

#if FO_IMAGE_ART
    SECTION("AppliesWholeArtSourceMetadataAndItsOutputName")
    {
        TestRig rig;
        rig.AddSourceFile("whole.fofrm", "Source = dirs.art\nFps = 1\nOffset = 100 0\n");
        AddSourceBinaryFile(rig, "dirs.art", MakeRotationArt());
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "whole.fofrm");
        auto sprite = ReadSpriteResource(rig.Outputs.at("whole.fofrm"));
        REQUIRE(sprite.Directions.size() == GameSettings::MAP_DIR_COUNT);
        REQUIRE(sprite.Animation.Sprite);
        CHECK(GetDurationMs(sprite.Animation.Sprite->Directions[0]) == 1000);
        CHECK(sprite.Directions.front().Frames.size() == 1);
        CHECK(sprite.Directions.front().Frames.front().Offset == ipos32 {100, 1});
    }
#endif

    SECTION("ResolvesOrdinaryFofrmFramesAtPackRoot")
    {
        TestRig rig;
        rig.AddSourceFile("root.fofrm", "Frame = frame.tga\n");
        AddSourceBinaryFile(rig, "frame.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "root.fofrm");
        auto frame = ReadSingleFrame(rig.Outputs.at("root.fofrm"));
        CHECK(frame.Data == vector<uint8_t> {255, 0, 0, 255});
    }

#if FO_IMAGE_ART
    SECTION("AlignsChangingArtHotSpotsWithoutLosingFrameDeltas")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "anchor.art",
            MakeArt(1, 10, {true, false, false, false}, 2,
                {
                    ArtFrameSpec {.Width = 2, .Height = 1, .HotY = 1, .DeltaX = 3, .Indices = {1, 2}},
                    ArtFrameSpec {.Width = 2, .Height = 1, .HotX = 1, .HotY = 1, .DeltaX = -1, .Indices = {2, 1}},
                }));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "anchor.art");
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("anchor.art"));
        REQUIRE(sequence.Frames.size() == 2);
        CHECK(sequence.Frames[0].Offset == ipos32 {0, 0});
        CHECK(sequence.Frames[1].Offset == sequence.Frames[0].Offset);
        CHECK(sequence.Frames[0].Width == 3);
        CHECK(sequence.Frames[0].NextX == 3);
        CHECK(sequence.Frames[1].NextX == -1);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0, 0, 0, 0, 4, 5, 6, 255, 8, 9, 10, 255});
        CHECK(sequence.Frames[1].Data == vector<uint8_t> {8, 9, 10, 255, 4, 5, 6, 255, 0, 0, 0, 0});
    }
#endif

#if FO_IMAGE_ART
    SECTION("SupportsLongFrameIndexedArtAndSparsePalettes")
    {
        TestRig rig;
        vector<ArtFrameSpec> frames(10001, ArtFrameSpec {.Width = 1, .Height = 1, .Indices = {1}});
        frames.back().Indices = {2};
        AddSourceBinaryFile(rig, "long.art", MakeArt(1, 1, {false, false, true, false}, 10001, frames));
        rig.AddSourceFile("last.fofrm", "Source=long$F10000.art\n");
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "last.fofrm");
        auto selected = ReadSingleDirSequence(rig.Outputs.at("last.fofrm"));
        CHECK(selected.SequenceSize == 1);
        CHECK(selected.AnimTicks == 1000);
        CHECK(selected.Frames.at(0).Data == vector<uint8_t> {136, 9, 10, 255});

        baker.BakeFiles(rig.GetAllSourceFiles(), "long.art");
        CHECK_FALSE(rig.Outputs.contains("long.art"));
        TestRig standalone;
        AddSourceBinaryFile(standalone, "long.art", MakeArt(1, 1, {false, false, true, false}, 10001, frames));
        ImageBaker standalone_baker {standalone.MakeContext()};
        standalone_baker.BakeFiles(standalone.GetAllSourceFiles(), "long.art");
        auto sprite = ReadSpriteResource(standalone.Outputs.at("long.art"));
        REQUIRE(sprite.Animation.Sprite);
        CHECK(GetDurationMs(sprite.Animation.Sprite->Directions[0]) == 10001000);
        CHECK(sprite.Directions.at(0).Frames.size() == 10001);
    }
#endif

#if FO_IMAGE_ART
    SECTION("RejectsInvalidArtRotationOptions")
    {
        for (string_view rotation : {"", "8", "-1", "3x", "33"}) {
            TestRig rig;
            rig.AddSourceFile("gfx/invalid.fofrm", strex("Source = source$r={}.art\n", rotation).str());
            AddSourceBinaryFile(rig, "gfx/source.art", MakeSimpleArt());

            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/invalid.fofrm"));
        }
    }
#endif

    SECTION("RejectsRecursiveAndMissingFofrmAliases")
    {
        for (string_view source : {"loop.fofrm", "missing.art"}) {
            TestRig rig;
            rig.AddSourceFile("gfx/loop.fofrm", strex("Source = {}\n", source).str());

            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/loop.fofrm"));
        }
    }

#if FO_IMAGE_ART
    SECTION("RejectsTruncatedArtTablesAndPixelStreams")
    {
        vector<uint8_t> truncated_table = MakeArt(1, 10, {true, false, false, false}, std::numeric_limits<int32_t>::max(), {});
        vector<uint8_t> truncated_pixels = MakeSimpleArt();
        truncated_pixels.pop_back();
        vector<uint8_t> truncated_rle = MakeArt(1, 10, {true, false, false, false}, 1, {ArtFrameSpec {.Width = 3, .Height = 1, .Stored = {0x82, 1}}});
        vector<uint8_t> short_rle = MakeArt(1, 10, {true, false, false, false}, 1, {ArtFrameSpec {.Width = 3, .Height = 1, .Stored = {1, 1}}});

        for (const auto& data : {truncated_table, truncated_pixels, truncated_rle, short_rle}) {
            TestRig rig;
            AddSourceBinaryFile(rig, "gfx/broken.art", data);

            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/broken.art"));
        }
    }
#endif

#if FO_IMAGE_ART
    SECTION("RejectsArtHotSpotsOutsideBakedRange")
    {
        for (int32_t hot_x : {std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()}) {
            TestRig rig;
            AddSourceBinaryFile(rig, "gfx/broken.art", MakeArt(1, 10, {true, false, false, false}, 1, {ArtFrameSpec {.Width = 1, .Height = 1, .HotX = hot_x, .Indices = {1}}}));

            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/broken.art"));
        }
    }
#endif

#if FO_IMAGE_ART
    SECTION("BakesWallHotSpotShiftForPlacedRotation")
    {
        // Wall rotations 0/1/6/7 use the original renderer hot spot shift even with one stored rotation
        TestRig rig;
        rig.AddSourceFile("art/wall/w_r0.fofrm", "Source = w$r=0.art\n");
        rig.AddSourceFile("art/wall/w_r2.fofrm", "Source = w$r=2.art\n");
        AddSourceBinaryFile(rig, "art/wall/w.art", MakeArt(0x1, 10, {true, false, false, false}, 1, {ArtFrameSpec {.Width = 1, .Height = 1, .Indices = {1}}}));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "art/wall/w_r0.fofrm");
        baker.BakeFiles(rig.GetAllSourceFiles(), "art/wall/w_r2.fofrm");

        REQUIRE(rig.Outputs.contains("art/wall/w_r0.fofrm"));
        REQUIRE(rig.Outputs.contains("art/wall/w_r2.fofrm"));
        auto shifted = ReadSingleDirSequence(rig.Outputs.at("art/wall/w_r0.fofrm"));
        auto plain = ReadSingleDirSequence(rig.Outputs.at("art/wall/w_r2.fofrm"));

        REQUIRE(shifted.Frames.size() == 1);
        REQUIRE(plain.Frames.size() == 1);
        CHECK(plain.Frames[0].Width == 1);
        CHECK(plain.Frames[0].Height == 1);
        CHECK(plain.Frames[0].Offset == ipos32 {0, 1});
        CHECK(shifted.Frames[0].Width == 41);
        CHECK(shifted.Frames[0].Height == 20);
        CHECK(shifted.Frames[0].Offset == ipos32 {20, 0});
    }
#endif

#if FO_IMAGE_ART
    SECTION("BakesArtRotationsAndMirrorsCritterRotations")
    {
        // ART rotation 0 faces screen north and goes clockwise; square direction 0 is north-east. Critter-like art
        // draws rotations 1..3 as mirrored rotations 7..5 (art.c tig_art_frame_data)
        for (string_view path : {string_view {"gfx/dirs.art"}, string_view {"art/critter/dirs.art"}}) {
            bool critter = path.starts_with("art/critter/");

            TestRig rig;
            AddSourceBinaryFile(rig, path, MakeRotationArt());

            ImageBaker baker {rig.MakeContext()};
            baker.BakeFiles(rig.GetAllSourceFiles(), path);

            REQUIRE(rig.Outputs.contains(string(path)));
            auto resource = ReadSpriteResource(rig.Outputs.at(string(path)));
            REQUIRE(resource.Directions.size() == GameSettings::MAP_DIR_COUNT);

            for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
                int32_t rotation = 0;

                if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                    constexpr int32_t hex_dir_to_rotation[6] = {1, 2, 3, 5, 6, 7};
                    rotation = hex_dir_to_rotation[dir];
                }
                else {
                    rotation = (dir + 1) % 8;
                }

                bool mirrored = critter && rotation > 0 && rotation < 4;
                int32_t source = mirrored ? 8 - rotation : rotation;
                auto sequence = ReadDirectionSequence(resource, numeric_cast<size_t>(dir));
                REQUIRE(sequence.Frames.size() == 1);
                CHECK(sequence.AnimTicks == 200);
                const auto& frame = sequence.Frames[0];
                CHECK(frame.Offset == ipos32 {0, 1});
                CHECK(frame.Width == 1);
                CHECK(frame.Height == 1);
                CHECK(frame.NextX == (mirrored ? -source : source));
                CHECK(frame.NextY == -source);
                auto expected = ArtTestColor(0, source + 1);
                expected.emplace_back(255);
                CHECK(frame.Data == expected);
            }
        }
    }
#endif

    SECTION("Setup")
    {
        TestRig rig;
        auto bakers = MakeRequestedBakers({string(ImageBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == ImageBaker::NAME);
        CHECK(bakers.front()->GetOrder() == 4);
        CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
    }

    SECTION("SpriteMeshMasterSwitchAndEmptyKind")
    {
        TestRig disabled;
        ConfigureSpriteMesh(disabled, false);
        BakedImageFrame disabled_frame = ReadSingleFrame(BakeAlphaMask(disabled, {"...", "...", "..."}));
        CHECK(disabled_frame.MeshKind == SpriteMeshKind::Quad);
        CHECK(disabled_frame.Mesh.Vertices.empty());
        CHECK(disabled_frame.Mesh.Indices.empty());

        TestRig enabled;
        ConfigureSpriteMesh(enabled);
        BakedImageFrame empty_frame = ReadSingleFrame(BakeAlphaMask(enabled, {"...", "...", "..."}));
        CHECK(empty_frame.MeshKind == SpriteMeshKind::Empty);
        CHECK(empty_frame.Mesh.Vertices.empty());
        CHECK(empty_frame.Mesh.Indices.empty());

        TestRig opaque;
        ConfigureSpriteMesh(opaque);
        CHECK(ReadSingleFrame(BakeAlphaMask(opaque, {"##", "##"})).MeshKind == SpriteMeshKind::Quad);

        TestRig dilation_fills_frame;
        ConfigureSpriteMesh(dilation_fills_frame);
        BakedImageFrame dilation_fills_frame_result = ReadSingleFrame(BakeAlphaMask(dilation_fills_frame, {"...", ".#.", "..."}));
        REQUIRE(dilation_fills_frame_result.MeshKind == SpriteMeshKind::Mesh);
        CHECK(dilation_fills_frame_result.Mesh.Indices.size() <= 6);
    }

    SECTION("SpriteMeshConcaveContour")
    {
        TestRig rig;
        ConfigureSpriteMesh(rig);
        BakedImageSequence sequence = ReadSingleDirSequence(BakeAlphaMask(rig, {"........", ".####...", ".####...", ".##.....", ".##.....", ".##.....", "........", "........"}));
        const BakedImageFrame& frame = sequence.Frames.front();

        REQUIRE(frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(frame.Mesh.Indices.size() > 6);
        CHECK(frame.Mesh.Indices.size() % 3 == 0);
        CHECK(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {1.5f, 1.5f})));
        CHECK_FALSE(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {4.5f, 4.5f})));
    }

    SECTION("SpriteMeshPreservesHole")
    {
        TestRig rig;
        ConfigureSpriteMesh(rig);
        BakedImageSequence sequence = ReadSingleDirSequence(BakeAlphaMask(rig, {"#########", "#.......#", "#.......#", "#.......#", "#.......#", "#.......#", "#.......#", "#.......#", "#########"}));
        const BakedImageFrame& frame = sequence.Frames.front();

        REQUIRE(frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {0.5f, 0.5f})));
        CHECK_FALSE(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {4.5f, 4.5f})));
    }

    SECTION("SpriteMeshPreservesSeparateIslands")
    {
        TestRig rig;
        ConfigureSpriteMesh(rig);
        BakedImageSequence sequence = ReadSingleDirSequence(BakeAlphaMask(rig, {".........", ".........", ".#.....#.", ".........", "........."}));
        const BakedImageFrame& frame = sequence.Frames.front();

        REQUIRE(frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {1.5f, 2.5f})));
        CHECK(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {7.5f, 2.5f})));
        CHECK_FALSE(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {4.5f, 2.5f})));
    }

    SECTION("SpriteMeshSettingsControlGeneration")
    {
        TestRig threshold;
        ConfigureSpriteMesh(threshold, true, 128);
        BakedImageFrame threshold_frame = ReadSingleFrame(BakeAlphaMask(threshold, {"...", ".+.", "..."}));
        CHECK(threshold_frame.MeshKind == SpriteMeshKind::Empty);

        TestRig below_threshold;
        ConfigureSpriteMesh(below_threshold, true, 127);
        BakedImageFrame below_threshold_frame = ReadSingleDirSequence(BakeAlphaMask(below_threshold, {".....", ".....", "..+..", ".....", "....."})).Frames.front();
        CHECK(below_threshold_frame.MeshKind == SpriteMeshKind::Mesh);

        TestRig dilation;
        ConfigureSpriteMesh(dilation);
        BakedImageFrame dilation_frame = ReadSingleFrame(BakeAlphaMask(dilation, {".....", ".....", "..#..", ".....", "....."}));
        REQUIRE(dilation_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(BakedMeshDoubleArea(dilation_frame) == 2);

        vector<string> diagonal_rows(40, string(40, '.'));
        for (size_t i = 8; i < 32; i++) {
            diagonal_rows[i][i] = '#';
        }

        TestRig simplified_diagonal;
        ConfigureSpriteMesh(simplified_diagonal);
        BakedImageFrame simplified_diagonal_frame = ReadSingleFrame(BakeAlphaMask(simplified_diagonal, diagonal_rows));

        REQUIRE(simplified_diagonal_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(simplified_diagonal_frame.Mesh.Vertices.size() <= 12);

        vector<string> shallow_notch_rows(30, string(30, '#'));
        for (size_t y = 23; y < 30; y++) {
            for (size_t x = 5; x < 25; x++) {
                shallow_notch_rows[y][x] = '.';
            }
        }

        TestRig shallow_notch;
        ConfigureSpriteMesh(shallow_notch);
        BakedImageFrame shallow_notch_frame = ReadSingleFrame(BakeAlphaMask(shallow_notch, shallow_notch_rows));

        REQUIRE(shallow_notch_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK_FALSE(IsPointInsideBakedMesh(shallow_notch_frame, SourcePointToBakedMeshPoint(shallow_notch_frame, {15.5f, 29.5f})));

        vector<string> shallow_diagonal_rows(40, string(80, '.'));
        for (size_t x = 2; x < 78; x++) {
            size_t y = 8 + (x - 2) * 19 / 75;
            shallow_diagonal_rows[y][x] = '#';
        }

        TestRig shallow_diagonal;
        ConfigureSpriteMesh(shallow_diagonal);
        BakedImageFrame shallow_diagonal_frame = ReadSingleFrame(BakeAlphaMask(shallow_diagonal, shallow_diagonal_rows));

        REQUIRE(shallow_diagonal_frame.MeshKind == SpriteMeshKind::Mesh);
        for (size_t y = 0; y < shallow_diagonal_rows.size(); y++) {
            for (size_t x = 0; x < shallow_diagonal_rows[y].size(); x++) {
                if (shallow_diagonal_rows[y][x] == '#') {
                    CAPTURE(x, y);
                    CHECK(IsPointInsideBakedMesh(shallow_diagonal_frame, SourcePointToBakedMeshPoint(shallow_diagonal_frame, {numeric_cast<float32_t>(x) + 0.01f, numeric_cast<float32_t>(y) + 0.01f})));
                    CHECK(IsPointInsideBakedMesh(shallow_diagonal_frame, SourcePointToBakedMeshPoint(shallow_diagonal_frame, {numeric_cast<float32_t>(x) + 0.99f, numeric_cast<float32_t>(y) + 0.01f})));
                    CHECK(IsPointInsideBakedMesh(shallow_diagonal_frame, SourcePointToBakedMeshPoint(shallow_diagonal_frame, {numeric_cast<float32_t>(x) + 0.01f, numeric_cast<float32_t>(y) + 0.99f})));
                    CHECK(IsPointInsideBakedMesh(shallow_diagonal_frame, SourcePointToBakedMeshPoint(shallow_diagonal_frame, {numeric_cast<float32_t>(x) + 0.99f, numeric_cast<float32_t>(y) + 0.99f})));
                }
            }
        }

        vector<string> edge_coverage_rows(40, string(80, '.'));
        uint32_t edge_random_state = 3;
        int32_t edge_y = 9;

        for (size_t x = 2; x < 78; x++) {
            edge_coverage_rows[numeric_cast<size_t>(edge_y)][x] = '#';
            edge_random_state = edge_random_state * 1664525u + 1013904223u;
            edge_y = std::clamp(edge_y + numeric_cast<int32_t>((edge_random_state >> 30u) & 3u) - 1, 1, 38);
        }

        TestRig edge_coverage;
        ConfigureSpriteMesh(edge_coverage);
        BakedImageSequence edge_coverage_sequence = ReadSingleDirSequence(BakeAlphaMask(edge_coverage, edge_coverage_rows));
        const BakedImageFrame& edge_coverage_frame = edge_coverage_sequence.Frames.front();

        REQUIRE(edge_coverage_frame.MeshKind == SpriteMeshKind::Mesh);
        for (size_t sample_y = 0; sample_y < edge_coverage_rows.size(); sample_y++) {
            for (size_t sample_x = 0; sample_x < edge_coverage_rows[sample_y].size(); sample_x++) {
                if (edge_coverage_rows[sample_y][sample_x] != '#') {
                    continue;
                }

                CAPTURE(sample_x, sample_y);
                CHECK(IsPointInsideBakedMesh(edge_coverage_frame, SourcePointToBakedMeshPoint(edge_coverage_frame, {numeric_cast<float32_t>(sample_x) + 0.01f, numeric_cast<float32_t>(sample_y) + 0.01f})));
                CHECK(IsPointInsideBakedMesh(edge_coverage_frame, SourcePointToBakedMeshPoint(edge_coverage_frame, {numeric_cast<float32_t>(sample_x) + 0.99f, numeric_cast<float32_t>(sample_y) + 0.01f})));
                CHECK(IsPointInsideBakedMesh(edge_coverage_frame, SourcePointToBakedMeshPoint(edge_coverage_frame, {numeric_cast<float32_t>(sample_x) + 0.01f, numeric_cast<float32_t>(sample_y) + 0.99f})));
                CHECK(IsPointInsideBakedMesh(edge_coverage_frame, SourcePointToBakedMeshPoint(edge_coverage_frame, {numeric_cast<float32_t>(sample_x) + 0.99f, numeric_cast<float32_t>(sample_y) + 0.99f})));
            }
        }

        vector<string> diamond_rows(41, string(81, '.'));
        for (size_t y = 0; y < diamond_rows.size(); y++) {
            int32_t distance = std::abs(numeric_cast<int32_t>(y) - 20);
            int32_t half_width = (20 - distance) * 2;
            for (int32_t x = 40 - half_width; x <= 40 + half_width; x++) {
                diamond_rows[y][numeric_cast<size_t>(x)] = '#';
            }
        }

        TestRig preferred_quad;
        ConfigureSpriteMesh(preferred_quad, true, 0, 2);
        BakedImageSequence preferred_quad_sequence = ReadSingleDirSequence(BakeAlphaMask(preferred_quad, diamond_rows));
        REQUIRE(preferred_quad_sequence.Frames.size() == 1);
        const BakedImageFrame& preferred_quad_frame = preferred_quad_sequence.Frames.front();

        REQUIRE(preferred_quad_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(preferred_quad_frame.Mesh.SourceSize == isize32 {81, 41});
        CHECK(preferred_quad_frame.Width <= 91);
        CHECK(preferred_quad_frame.Height <= 51);
        CHECK(preferred_quad_frame.Mesh.Vertices.size() == 4);
        CHECK(preferred_quad_frame.Mesh.Indices.size() == 6);

        for (size_t y = 0; y < diamond_rows.size(); y++) {
            for (size_t x = 0; x < diamond_rows[y].size(); x++) {
                if (diamond_rows[y][x] == '#') {
                    CAPTURE(x, y);
                    CHECK(IsPointInsideBakedMesh(preferred_quad_frame, SourcePointToBakedMeshPoint(preferred_quad_frame, {numeric_cast<float32_t>(x) + 0.5f, numeric_cast<float32_t>(y) + 0.5f})));
                }
            }
        }

        vector<string> wedge_rows(61, string(61, '.'));
        for (size_t y = 5; y <= 55; y++) {
            size_t right = 60 - y;
            for (size_t x = 5; x <= right; x++) {
                wedge_rows[y][x] = '#';
            }
        }

        TestRig preferred_triangle;
        ConfigureSpriteMesh(preferred_triangle, true, 0, 1);
        BakedImageSequence preferred_triangle_sequence = ReadSingleDirSequence(BakeAlphaMask(preferred_triangle, wedge_rows));
        REQUIRE(preferred_triangle_sequence.Frames.size() == 1);
        const BakedImageFrame& preferred_triangle_frame = preferred_triangle_sequence.Frames.front();

        REQUIRE(preferred_triangle_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(preferred_triangle_frame.Mesh.SourceSize == isize32 {61, 61});
        CHECK(preferred_triangle_frame.Mesh.Vertices.size() == 3);
        CHECK(preferred_triangle_frame.Mesh.Indices.size() == 3);

        vector<string> rectangle_rows(31, string(61, '.'));
        for (size_t y = 5; y <= 25; y++) {
            for (size_t x = 5; x <= 55; x++) {
                rectangle_rows[y][x] = '#';
            }
        }

        TestRig preferred_rectangle;
        ConfigureSpriteMesh(preferred_rectangle, true, 0, 2);
        BakedImageFrame preferred_rectangle_frame = ReadSingleDirSequence(BakeAlphaMask(preferred_rectangle, rectangle_rows, "gfx/mesh.mask", {7, -8})).Frames.front();

        REQUIRE(preferred_rectangle_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(preferred_rectangle_frame.Width < 61);
        CHECK(preferred_rectangle_frame.Height < 31);
        CHECK(preferred_rectangle_frame.Mesh.SourceSize == isize32 {61, 31});
        CHECK(preferred_rectangle_frame.Offset.x == 7 + numeric_cast<int32_t>(preferred_rectangle_frame.Width) / 2 - 61 / 2 + preferred_rectangle_frame.Mesh.SourceOffset.x);
        CHECK(preferred_rectangle_frame.Offset.y == -8 + numeric_cast<int32_t>(preferred_rectangle_frame.Height) - 31 + preferred_rectangle_frame.Mesh.SourceOffset.y);
        CHECK(preferred_rectangle_frame.Mesh.Vertices.size() == 4);
        CHECK(preferred_rectangle_frame.Mesh.Indices.size() == 6);

        vector<ipos32> adaptive_tree_hull {{0, 115}, {13, 81}, {33, 29}, {49, 0}, {55, 0}, {58, 7}, {86, 67}, {106, 120}, {106, 141}, {88, 159}, {23, 159}, {0, 137}};
        vector<string> adaptive_tree_rows(160, string(107, '.'));
        for (size_t y = 0; y < adaptive_tree_rows.size(); y++) {
            for (size_t x = 0; x < adaptive_tree_rows[y].size(); x++) {
                int64_t sample_x = numeric_cast<int64_t>(x) * 2 + 1;
                int64_t sample_y = numeric_cast<int64_t>(y) * 2 + 1;
                bool inside = true;

                for (size_t i = 0; i < adaptive_tree_hull.size(); i++) {
                    ipos32 a = adaptive_tree_hull[i];
                    ipos32 b = adaptive_tree_hull[(i + 1) % adaptive_tree_hull.size()];
                    int64_t cross = numeric_cast<int64_t>(b.x - a.x) * (sample_y - numeric_cast<int64_t>(a.y) * 2) - numeric_cast<int64_t>(b.y - a.y) * (sample_x - numeric_cast<int64_t>(a.x) * 2);
                    inside = inside && cross >= 0;
                }

                if (inside) {
                    adaptive_tree_rows[y][x] = '#';
                }
            }
        }

        TestRig adaptive_padding;
        ConfigureSpriteMesh(adaptive_padding, true, 0, 4096, 1.0f);
        BakedImageSequence adaptive_padding_sequence = ReadSingleDirSequence(BakeAlphaMask(adaptive_padding, adaptive_tree_rows));
        REQUIRE(adaptive_padding_sequence.Frames.size() == 1);
        const BakedImageFrame& adaptive_padding_frame = adaptive_padding_sequence.Frames.front();

        REQUIRE(adaptive_padding_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(adaptive_padding_frame.Mesh.SourceSize == isize32 {107, 160});
        CHECK(adaptive_padding_frame.Mesh.SourceOffset.x >= -20);
        CHECK(adaptive_padding_frame.Mesh.SourceOffset.y >= -20);
        CHECK((adaptive_padding_frame.Mesh.SourceOffset.x < 0 || adaptive_padding_frame.Mesh.SourceOffset.y < 0 || adaptive_padding_frame.Mesh.SourceOffset.x + adaptive_padding_frame.Width > 107 || adaptive_padding_frame.Mesh.SourceOffset.y + adaptive_padding_frame.Height > 160));
        CHECK(adaptive_padding_frame.Mesh.Vertices.size() == 3);
        CHECK(adaptive_padding_frame.Mesh.Indices.size() == 3);

        vector<ipos32> pentagon_hull {{50, 5}, {95, 38}, {78, 95}, {22, 95}, {5, 38}};
        vector<string> pentagon_rows(101, string(101, '.'));
        for (size_t y = 0; y < pentagon_rows.size(); y++) {
            for (size_t x = 0; x < pentagon_rows[y].size(); x++) {
                int64_t sample_x = numeric_cast<int64_t>(x) * 2 + 1;
                int64_t sample_y = numeric_cast<int64_t>(y) * 2 + 1;
                bool inside = true;

                for (size_t i = 0; i < pentagon_hull.size(); i++) {
                    ipos32 a = pentagon_hull[i];
                    ipos32 b = pentagon_hull[(i + 1) % pentagon_hull.size()];
                    int64_t cross = numeric_cast<int64_t>(b.x - a.x) * (sample_y - numeric_cast<int64_t>(a.y) * 2) - numeric_cast<int64_t>(b.y - a.y) * (sample_x - numeric_cast<int64_t>(a.x) * 2);
                    inside = inside && cross >= 0;
                }

                if (inside) {
                    pentagon_rows[y][x] = '#';
                }
            }
        }

        TestRig enclosing_budget;
        ConfigureSpriteMesh(enclosing_budget, true, 0, 3);
        BakedImageFrame enclosing_budget_frame = ReadSingleDirSequence(BakeAlphaMask(enclosing_budget, pentagon_rows)).Frames.front();
        REQUIRE(enclosing_budget_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(enclosing_budget_frame.Mesh.Indices.size() == 9);

        vector<string> door_rows(61, string(101, '.'));
        for (size_t y = 5; y <= 55; y++) {
            for (size_t x = 5; x <= 19; x++) {
                door_rows[y][x] = '#';
            }
            for (size_t x = 81; x <= 95; x++) {
                door_rows[y][x] = '#';
            }
        }

        TestRig component_budget;
        ConfigureSpriteMesh(component_budget, true, 0, 4);
        BakedImageFrame component_budget_frame = ReadSingleDirSequence(BakeAlphaMask(component_budget, door_rows)).Frames.front();
        REQUIRE(component_budget_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(component_budget_frame.Mesh.Indices.size() == 12);
        CHECK_FALSE(IsPointInsideBakedMesh(component_budget_frame, SourcePointToBakedMeshPoint(component_budget_frame, {55.5f, 35.5f})));

        TestRig low_area_weight;
        ConfigureSpriteMesh(low_area_weight, true, 0, 4, 1.0f);
        BakedImageFrame low_area_weight_frame = ReadSingleDirSequence(BakeAlphaMask(low_area_weight, door_rows)).Frames.front();
        REQUIRE(low_area_weight_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(low_area_weight_frame.Mesh.Indices.size() == 6);
        CHECK(IsPointInsideBakedMesh(low_area_weight_frame, SourcePointToBakedMeshPoint(low_area_weight_frame, {55.5f, 35.5f})));

        TestRig high_area_weight;
        ConfigureSpriteMesh(high_area_weight, true, 0, 4, 64.0f);
        BakedImageFrame high_area_weight_frame = ReadSingleDirSequence(BakeAlphaMask(high_area_weight, door_rows)).Frames.front();
        REQUIRE(high_area_weight_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(high_area_weight_frame.Mesh.Indices.size() == 12);
        CHECK_FALSE(IsPointInsideBakedMesh(high_area_weight_frame, SourcePointToBakedMeshPoint(high_area_weight_frame, {55.5f, 35.5f})));

        TestRig triangle_uncapped;
        ConfigureSpriteMesh(triangle_uncapped);
        BakedImageFrame triangle_uncapped_frame = ReadSingleDirSequence(BakeAlphaMask(triangle_uncapped, {".......", ".#####.", ".#.....", ".#.....", ".#####.", ".......", "......."})).Frames.front();
        REQUIRE(triangle_uncapped_frame.MeshKind == SpriteMeshKind::Mesh);

        TestRig triangle_capped;
        ConfigureSpriteMesh(triangle_capped, true, 0, 2);
        BakedImageFrame triangle_capped_frame = ReadSingleDirSequence(BakeAlphaMask(triangle_capped, {".......", ".#####.", ".#.....", ".#.....", ".#####.", ".......", "......."})).Frames.front();
        REQUIRE(triangle_capped_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(triangle_capped_frame.Mesh.Indices.size() <= 6);

        TestRig unprofitable;
        ConfigureSpriteMesh(unprofitable);
        BakedImageFrame unprofitable_frame = ReadSingleDirSequence(BakeAlphaMask(unprofitable, {".....", ".##..", ".##..", ".##..", "....."})).Frames.front();
        CHECK(unprofitable_frame.MeshKind == SpriteMeshKind::Mesh);
    }

    SECTION("SpriteMeshAutomaticSimplificationPreservesLShape")
    {
        vector<string> rows(123, string(64, '.'));
        for (size_t x = 0; x < 64; x++) {
            size_t top = 17 - x * 15 / 63;
            size_t bottom = 48 - x * 15 / 63;
            for (size_t y = top; y <= bottom; y++) {
                rows[y][x] = '#';
            }
        }
        for (size_t y = 17; y < 123; y++) {
            for (size_t x = 0; x < 6; x++) {
                rows[y][x] = '#';
            }
        }

        TestRig rig;
        ConfigureSpriteMesh(rig, true, 10, 16, 64.0f);
        BakedImageSequence sequence = ReadSingleDirSequence(BakeAlphaMask(rig, rows));
        const BakedImageFrame& frame = sequence.Frames.front();
        REQUIRE(frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(frame.Mesh.Vertices.size() <= 6);
        CHECK(frame.Mesh.Indices.size() <= 12);
        CHECK(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {2.5f, 100.5f})));
        CHECK_FALSE(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {40.5f, 100.5f})));
    }

    SECTION("SpriteMeshExpandedSimplificationBuildsConcaveWallCandidate")
    {
        vector<string> rows(123, string(64, '.'));
        vector<size_t> top_left {60, 56, 52, 49, 45, 41, 37, 33, 29, 25, 21, 17, 13, 9, 5, 1};
        for (size_t y = 2; y <= 17; y++) {
            size_t left = top_left[y - 2];
            for (size_t x = left; x < 64; x++) {
                rows[y][x] = '#';
            }
        }
        for (size_t y = 18; y <= 32; y++) {
            for (size_t x = 0; x < 64; x++) {
                rows[y][x] = '#';
            }
        }
        vector<size_t> bottom_right {59, 56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16, 12, 8};
        for (size_t y = 33; y <= 46; y++) {
            size_t right = bottom_right[y - 33];
            for (size_t x = 0; x <= right; x++) {
                rows[y][x] = '#';
            }
        }
        for (size_t y = 47; y <= 114; y++) {
            for (size_t x = 0; x <= 4; x++) {
                rows[y][x] = '#';
            }
        }
        for (size_t y = 115; y <= 122; y++) {
            for (size_t x = 0; x <= 5; x++) {
                rows[y][x] = '#';
            }
        }

        TestRig rig;
        ConfigureSpriteMesh(rig, true, 10, 16, 200.0f);
        BakedImageSequence sequence = ReadSingleDirSequence(BakeAlphaMask(rig, rows));
        const BakedImageFrame& frame = sequence.Frames.front();
        REQUIRE(frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(frame.Mesh.Vertices.size() <= 7);
        CHECK(frame.Mesh.Indices.size() >= 12);
        CHECK(frame.Mesh.Indices.size() <= 15);
        CHECK(BakedMeshDoubleArea(frame) < 8000);
        CHECK(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {2.5f, 100.5f})));
        CHECK_FALSE(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {40.5f, 100.5f})));
    }

    SECTION("SpriteMeshExpandedSimplificationEnclosesThinLShape")
    {
        vector<string> rows(123, string(64, '.'));
        vector<size_t> top_left {60, 55, 52, 50, 46, 42, 38, 34, 30, 26, 22, 18, 12, 7, 4, 2};
        for (size_t y = 0; y < top_left.size(); y++) {
            for (size_t x = top_left[y]; x < 64; x++) {
                rows[y][x] = '#';
            }
        }
        for (size_t y = 16; y <= 25; y++) {
            for (size_t x = 0; x < 64; x++) {
                rows[y][x] = '#';
            }
        }
        vector<size_t> lower_right {54, 54, 50, 50, 42, 38, 34, 30, 26, 22, 20, 19, 10, 6, 5, 4, 4};
        for (size_t y = 26; y <= 42; y++) {
            for (size_t x = 0; x <= lower_right[y - 26]; x++) {
                rows[y][x] = '#';
            }
        }
        for (size_t y = 43; y < 123; y++) {
            for (size_t x = 0; x <= 3; x++) {
                rows[y][x] = '#';
            }
        }

        TestRig rig;
        ConfigureSpriteMesh(rig, true, 10, 16, 100.0f);
        BakedImageSequence sequence = ReadSingleDirSequence(BakeAlphaMask(rig, rows));
        const BakedImageFrame& frame = sequence.Frames.front();
        REQUIRE(frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(frame.Mesh.Indices.size() > 6);
        CHECK(frame.Mesh.Indices.size() <= 27);
        CHECK(BakedMeshDoubleArea(frame) <= 5454);
        CHECK(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {1.5f, 100.5f})));
        CHECK_FALSE(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {40.5f, 100.5f})));
    }

    SECTION("SpriteMeshClustersMoreComponentsThanTriangleBudget")
    {
        vector<string> rows(31, string(101, '.'));
        for (size_t y = 5; y <= 25; y++) {
            for (size_t x = 5; x <= 15; x++) {
                rows[y][x] = '#';
            }
            for (size_t x = 21; x <= 31; x++) {
                rows[y][x] = '#';
            }
            for (size_t x = 81; x <= 95; x++) {
                rows[y][x] = '#';
            }
        }

        TestRig rig;
        ConfigureSpriteMesh(rig, true, 0, 4, 200.0f);
        BakedImageSequence sequence = ReadSingleDirSequence(BakeAlphaMask(rig, rows));
        const BakedImageFrame& frame = sequence.Frames.front();
        REQUIRE(frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(frame.Mesh.Indices.size() <= 12);
        CHECK_FALSE(IsPointInsideBakedMesh(frame, SourcePointToBakedMeshPoint(frame, {55.5f, 15.5f})));
    }

    SECTION("SpriteMeshUnsafeTouchingTopologyFallsBackToQuad")
    {
        TestRig diagonal_islands;
        ConfigureSpriteMesh(diagonal_islands);
        CHECK(ReadSingleFrame(BakeAlphaMask(diagonal_islands, {"#.", ".#"})).MeshKind == SpriteMeshKind::Quad);

        TestRig touching_hole;
        ConfigureSpriteMesh(touching_hole);
        CHECK(ReadSingleFrame(BakeAlphaMask(touching_hole, {".##", "#.#", "###"})).MeshKind == SpriteMeshKind::Quad);

        TestRig diagonal_holes;
        ConfigureSpriteMesh(diagonal_holes);
        CHECK(ReadSingleFrame(BakeAlphaMask(diagonal_holes, {"#####", "#.###", "##.##", "#####", "#####"})).MeshKind == SpriteMeshKind::Quad);

        TestRig checkerboard;
        ConfigureSpriteMesh(checkerboard);
        CHECK(ReadSingleFrame(BakeAlphaMask(checkerboard, {"#.#.", ".#.#", "#.#.", ".#.#"})).MeshKind == SpriteMeshKind::Quad);
    }

    SECTION("SpriteMeshOutputIsDeterministic")
    {
        vector<string> mask {"......", ".####.", ".#..#.", ".####.", "......"};

        TestRig first;
        ConfigureSpriteMesh(first);
        vector<uint8_t> first_output = BakeAlphaMask(first, mask);

        TestRig second;
        ConfigureSpriteMesh(second);
        vector<uint8_t> second_output = BakeAlphaMask(second, mask);

        CHECK(first_output == second_output);
    }

    SECTION("SpriteMeshSharedFrameHasNoDuplicatePayload")
    {
        TestRig rig;
        ConfigureSpriteMesh(rig);
        rig.AddSourceFile("gfx/shared-mesh.toy", "payload");

        vector<uint8_t> rgba(5 * 5 * 4);
        rgba[(2 * 5 + 2) * 4 + 3] = 255;

        ImageBaker baker {rig.MakeContext()};
        baker.AddLoader(
            [rgba](string_view fname, string_view opt, FileReader reader, const ImageImport::Context& files) {
                ignore_unused(fname, opt, reader, files);

                ImageImport::FrameCollection collection;
                [[maybe_unused]] uint16_t sequence_size {};
                uint32_t duration_ms {100};
                bool have_dirs {};
                sequence_size = 2;
                duration_ms = 100;
                collection.Main.Frames = {
                    ImageImport::FrameShot {.Width = 5, .Height = 5, .Data = rgba},
                    ImageImport::FrameShot {.Shared = true, .SharedIndex = 0},
                };

                if (!have_dirs) {
                    for (auto& direction : collection.Dirs) {
                        direction.Frames.clear();
                    }
                }
                else {
                    for (int32_t direction = GameSettings::MAP_DIR_COUNT - 1; direction < ImageImport::MAX_DIRS_MINUS_ONE; direction++) {
                        collection.Dirs[direction].Frames.clear();
                    }
                }

                for (int32_t direction = 0; direction < (have_dirs ? GameSettings::MAP_DIR_COUNT : 1); direction++) {
                    auto& frames = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

                    for (size_t index = 0; index < frames.Frames.size(); index++) {
                        auto& shot = frames.Frames[index];

                        if (shot.Shared && shot.SharedIndex < index) {
                            uint16_t reference = shot.SharedIndex;
                            shot = frames.Frames[reference];
                            shot.Data.clear();
                            shot.Shared = true;
                            shot.SharedIndex = reference;
                        }
                    }

                    ImageImport::SetFrameSequenceDuration(frames, duration_ms);
                }

                return collection;
            },
            {"toy"});
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/shared-mesh.toy");

        auto resource = ReadSpriteResource(rig.Outputs.at("gfx/shared-mesh.toy"));
        REQUIRE(resource.Directions.size() == 1);
        auto sequence = ReadDirectionSequence(resource, 0);
        REQUIRE(sequence.Frames.size() == 2);
        CHECK(sequence.AnimTicks == 100);
        const auto& first_frame = sequence.Frames[0];
        CHECK(first_frame.MeshKind == SpriteMeshKind::Mesh);
        CHECK(first_frame.Offset.x == numeric_cast<int32_t>(first_frame.Width) / 2 - 5 / 2 + first_frame.Mesh.SourceOffset.x);
        CHECK(first_frame.Offset.y == numeric_cast<int32_t>(first_frame.Height) - 5 + first_frame.Mesh.SourceOffset.y);
        CHECK(resource.Directions[0].Frames[1].SharedFrameIndex == optional<uint16_t> {0});
        CHECK(sequence.Frames[1].Offset == first_frame.Offset);
    }

    SECTION("SpriteMeshRejectsInvalidSettings")
    {
        TestRig rig;
        ConfigureSpriteMesh(rig);

        SECTION("AlphaThreshold")
        {
            OverrideSetting(rig.Settings.SpriteMesh.AlphaThreshold, 255);
        }
        SECTION("MaxTriangles")
        {
            OverrideSetting(rig.Settings.SpriteMesh.MaxTriangles, 0);
        }
        SECTION("AreaSavingsWeight")
        {
            OverrideSetting(rig.Settings.SpriteMesh.AreaSavingsWeight, -1.0f);
        }

        rig.AddSourceFile("gfx/invalid-settings.mask", "mask");

        ImageBaker baker {rig.MakeContext()};
        baker.AddLoader(
            [](string_view fname, string_view opt, FileReader reader, const ImageImport::Context& files) {
                ignore_unused(fname, opt, reader, files);

                ImageImport::FrameCollection collection;
                [[maybe_unused]] uint16_t sequence_size {};
                uint32_t duration_ms {100};
                bool have_dirs {};
                sequence_size = 1;
                duration_ms = 100;
                collection.Main.Frames = {ImageImport::FrameShot {.Width = 1, .Height = 1, .Data = {255, 255, 255, 255}}};

                if (!have_dirs) {
                    for (auto& direction : collection.Dirs) {
                        direction.Frames.clear();
                    }
                }
                else {
                    for (int32_t direction = GameSettings::MAP_DIR_COUNT - 1; direction < ImageImport::MAX_DIRS_MINUS_ONE; direction++) {
                        collection.Dirs[direction].Frames.clear();
                    }
                }

                for (int32_t direction = 0; direction < (have_dirs ? GameSettings::MAP_DIR_COUNT : 1); direction++) {
                    auto& frames = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

                    for (size_t index = 0; index < frames.Frames.size(); index++) {
                        auto& shot = frames.Frames[index];

                        if (shot.Shared && shot.SharedIndex < index) {
                            uint16_t reference = shot.SharedIndex;
                            shot = frames.Frames[reference];
                            shot.Data.clear();
                            shot.Shared = true;
                            shot.SharedIndex = reference;
                        }
                    }

                    ImageImport::SetFrameSequenceDuration(frames, duration_ms);
                }

                return collection;
            },
            {"mask"});

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/invalid-settings.mask"), ImageBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("BakesRawTgaTarget")
    {
        TestRig rig;
        rig.AddSourceFile("ignored.txt", "not an image");
        AddSourceBinaryFile(rig, "ui/icon.tga", MakeRawTga(2, 1, 32, {0, 0, 255, 128, 0, 255, 0, 255}), 10);

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "ui/icon.tga");

        REQUIRE(rig.Outputs.contains("ui/icon.tga"));
        auto frame = ReadSingleFrame(rig.Outputs.at("ui/icon.tga"));

        CHECK(frame.Width == 2);
        CHECK(frame.Height == 1);
        CHECK(frame.NextX == 0);
        CHECK(frame.NextY == 0);
        CHECK(frame.Data == vector<uint8_t> {255, 0, 0, 128, 0, 255, 0, 255});
        CHECK(rig.Outputs.size() == 1);
    }

    SECTION("TargetEarlyReturnsDoNotWrite")
    {
        TestRig rig;
        rig.AddSourceFile("ui/readme.txt", "not an image", 11);

        ImageBaker baker {rig.MakeContext()};

        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "ui/readme.txt"));
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "ui/missing.tga"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("BakesRleTgaWhenScanningAllFiles")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/rle.tga", MakeRleTga(2, 1, 24, {0x81, 255, 0, 0}), 20);
        AddSourceBinaryFile(rig, "gfx/rle-raw-packets.tga",
            MakeRleTga(2, 1, 24,
                {
                    0x00,
                    0x30,
                    0x20,
                    0x10,
                    0x01,
                    0x60,
                    0x50,
                    0x40,
                    0x90,
                    0x80,
                    0x70,
                }),
            30);

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.contains("gfx/rle.tga"));
        auto frame = ReadSingleFrame(rig.Outputs.at("gfx/rle.tga"));

        CHECK(frame.Width == 2);
        CHECK(frame.Height == 1);
        CHECK(frame.Data == vector<uint8_t> {0, 0, 255, 255, 0, 0, 255, 255});

        REQUIRE(rig.Outputs.contains("gfx/rle-raw-packets.tga"));
        auto raw_packet_frame = ReadSingleFrame(rig.Outputs.at("gfx/rle-raw-packets.tga"));

        CHECK(raw_packet_frame.Width == 2);
        CHECK(raw_packet_frame.Height == 1);
        CHECK(raw_packet_frame.Data == vector<uint8_t> {0x10, 0x20, 0x30, 0xFF, 0x40, 0x50, 0x60, 0xFF});

        REQUIRE(rig.Outputs.contains("SpriteInfo/TestPack.foinfo"));
        const vector<uint8_t>& sprite_info_data = rig.Outputs.at("SpriteInfo/TestPack.foinfo");
        string sprite_info_text(sprite_info_data.begin(), sprite_info_data.end());
        vector<SpriteInfoFileEntry> sprite_info_entries = ReadSpriteInfoFile("SpriteInfo/TestPack.foinfo", sprite_info_text);
        REQUIRE(sprite_info_entries.size() == 2);
        CHECK(sprite_info_entries[0].Info.Directions[0].Frames.size() == 1);
        CHECK(sprite_info_entries[0].Info.Directions.front().Frames.front().Size == isize32 {2, 1});
        CHECK(sprite_info_entries[1].Info.Directions[0].Frames.size() == 1);
        CHECK(sprite_info_entries[1].Info.Directions.front().Frames.front().Size == isize32 {2, 1});
    }

#if FO_IMAGE_FRM
    SECTION("BakesFrmWithCritterNewNameAndCustomPalette")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "art/critters/RAT.frm",
            MakeFrm(5,
                {
                    FrmFrameSpec {.Width = 1, .Height = 1, .NextX = 2, .NextY = -3, .Indices = {1}},
                    FrmFrameSpec {.Width = 1, .Height = 1, .NextX = -4, .NextY = 5, .Indices = {2}},
                },
                7, -8));
        AddSourceBinaryFile(rig, "art/critters/RAT.pal", MakeFrmPalette());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "art/critters/rat.frm");

        CHECK_FALSE(rig.Outputs.contains("art/critters/RAT.frm"));
        REQUIRE(rig.Outputs.contains("art/critters/rat.frm"));
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("art/critters/rat.frm"));

        CHECK(sequence.SequenceSize == 2);
        CHECK(sequence.AnimTicks == 400);
        REQUIRE(sequence.Frames.size() == 2);
        CHECK(sequence.Frames[0].Offset == ipos32 {7, -8});
        CHECK(sequence.Frames[1].Offset == ipos32 {7, -8});
        CHECK(sequence.Frames[0].NextX == 2);
        CHECK(sequence.Frames[0].NextY == -3);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {4, 8, 12, 255});
        CHECK(sequence.Frames[1].NextX == -4);
        CHECK(sequence.Frames[1].NextY == 5);
        CHECK(sequence.Frames[1].Data == vector<uint8_t> {16, 20, 24, 255});
    }
#endif

#if FO_IMAGE_FRM
    SECTION("BakesFrmAnimatedPalettePixels")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/animated.frm",
            MakeFrm(10,
                {
                    FrmFrameSpec {.Width = 6, .Height = 1, .Indices = {229, 233, 238, 243, 248, 254}},
                }));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/animated.frm");

        REQUIRE(rig.Outputs.contains("gfx/animated.frm"));
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/animated.frm"));

        CHECK(sequence.SequenceSize == 60);
        CHECK(sequence.AnimTicks == 6000);
        REQUIRE(sequence.Frames.size() == 60);
        CHECK(sequence.Frames.front().Width == 6);
        CHECK(sequence.Frames.front().Height == 1);
        CHECK(sequence.Frames.front().Data.size() == 24);
        CHECK(sequence.Frames[1].Data.size() == 24);
        CHECK(sequence.Frames.front().Data != sequence.Frames[1].Data);
    }
#endif

#if FO_IMAGE_FRM
    SECTION("BakesFrxCritterDirsIntoLowerCaseFofrm")
    {
        TestRig rig;

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            AddSourceBinaryFile(rig, strex("art/critters/RAT.fr{}", dir),
                MakeFrxDir(5, numeric_cast<uint8_t>(dir),
                    {
                        FrmFrameSpec {
                            .Width = 1,
                            .Height = 1,
                            .NextX = numeric_cast<int16_t>(dir + 1),
                            .NextY = numeric_cast<int16_t>(-dir - 1),
                            .Indices = {numeric_cast<uint8_t>(dir + 1)},
                        },
                    },
                    numeric_cast<int16_t>(10 + dir), numeric_cast<int16_t>(-20 - dir)));
        }

        AddSourceBinaryFile(rig, "art/critters/RAT.pal", MakeFrmPalette());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "art/critters/rat.fofrm");

        CHECK_FALSE(rig.Outputs.contains("art/critters/RAT.fr0"));
        REQUIRE(rig.Outputs.contains("art/critters/rat.fofrm"));

        auto resource = ReadSpriteResource(rig.Outputs.at("art/critters/rat.fofrm"));
        REQUIRE(resource.Directions.size() == GameSettings::MAP_DIR_COUNT);

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            auto sequence = ReadDirectionSequence(resource, numeric_cast<size_t>(dir));
            REQUIRE(sequence.Frames.size() == 1);
            CHECK(sequence.AnimTicks == 200);
            const auto& frame = sequence.Frames[0];
            CHECK(frame.Offset == ipos32 {10 + dir, -20 - dir});
            CHECK(frame.Width == 1);
            CHECK(frame.Height == 1);
            CHECK(frame.NextX == dir + 1);
            CHECK(frame.NextY == -dir - 1);
            uint8_t color_base = numeric_cast<uint8_t>(dir * 3 + 1);
            CHECK(frame.Data == vector<uint8_t> {numeric_cast<uint8_t>(color_base * 4), numeric_cast<uint8_t>((color_base + 1) * 4), numeric_cast<uint8_t>((color_base + 2) * 4), 255});
        }
    }
#endif

#if FO_IMAGE_FRM
    SECTION("BakesFrmEmbeddedDirectionsAndReportsTruncatedDirTable")
    {
        vector<FrmFrameSpec> frames {
            FrmFrameSpec {
                .Width = 1,
                .Height = 1,
                .NextX = 2,
                .NextY = 3,
                .Indices = {1},
            },
        };

        TestRig dirs;
        AddSourceBinaryFile(dirs, "gfx/directional.frm", MakeFrmWithDirTable(5, frames, GameSettings::MAP_DIR_COUNT));

        ImageBaker dirs_baker {dirs.MakeContext()};
        dirs_baker.BakeFiles(dirs.GetAllSourceFiles(), "gfx/directional.frm");

        REQUIRE(dirs.Outputs.contains("gfx/directional.frm"));

        auto resource = ReadSpriteResource(dirs.Outputs.at("gfx/directional.frm"));
        REQUIRE(resource.Directions.size() == GameSettings::MAP_DIR_COUNT);

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            auto sequence = ReadDirectionSequence(resource, numeric_cast<size_t>(dir));
            REQUIRE(sequence.Frames.size() == 1);
            CHECK(sequence.AnimTicks == 200);
            const auto& frame = sequence.Frames[0];
            CHECK(frame.Offset == (dir == 0 ? ipos32 {1, -1} : ipos32 {10 + dir, -10 - dir}));
            CHECK(frame.Width == 1);
            CHECK(frame.Height == 1);
            CHECK(frame.NextX == 2);
            CHECK(frame.NextY == 3);
            CHECK(frame.Data.size() == 4);
        }

        TestRig truncated;
        AddSourceBinaryFile(truncated, "gfx/truncated-dirs.frm", MakeFrmWithDirTable(5, frames, 2));

        ImageBaker truncated_baker {truncated.MakeContext()};
        CHECK_THROWS_AS(truncated_baker.BakeFiles(truncated.GetAllSourceFiles(), "gfx/truncated-dirs.frm"), ImageBakerException);
        CHECK(truncated.Outputs.empty());
    }
#endif

#if FO_IMAGE_FRM
    SECTION("BakesFrxAnimatedPalettePixels")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/animated.fr0",
            MakeFrxDir(10, 0,
                {
                    FrmFrameSpec {.Width = 6, .Height = 1, .Indices = {229, 233, 238, 243, 248, 254}},
                },
                3, -4));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/animated.frm");

        CHECK_FALSE(rig.Outputs.contains("gfx/animated.fr0"));
        REQUIRE(rig.Outputs.contains("gfx/animated.frm"));
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/animated.frm"));

        CHECK(sequence.SequenceSize == 60);
        CHECK(sequence.AnimTicks == 6000);
        REQUIRE(sequence.Frames.size() == 60);
        CHECK(sequence.Frames.front().Offset == ipos32 {3, -4});
        CHECK(sequence.Frames.front().Width == 6);
        CHECK(sequence.Frames.front().Height == 1);
        CHECK(sequence.Frames.front().Data.size() == 24);
        CHECK(sequence.Frames[1].Data.size() == 24);
        CHECK(sequence.Frames.front().Data != sequence.Frames[1].Data);
    }
#endif

#if FO_IMAGE_FRM
    SECTION("InvalidFrxMissingDirectionIsReported")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "art/critters/RAT.fr0", MakeFrxDir(5, 0, {FrmFrameSpec {.Width = 1, .Height = 1, .Indices = {1}}}, 0, 0));
        AddSourceBinaryFile(rig, "art/critters/RAT.fr1", MakeFrxDir(5, 1, {FrmFrameSpec {.Width = 1, .Height = 1, .Indices = {1}}}, 0, 0));
        AddSourceBinaryFile(rig, "art/critters/RAT.pal", MakeFrmPalette());

        ImageBaker baker {rig.MakeContext()};

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "art/critters/RAT.fr0"), ImageBakerException);
        CHECK(rig.Outputs.empty());
    }
#endif

    SECTION("BakesPalettePngWithTransparency")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/indexed.png", MakePaletteTransparencyPng());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/indexed.png");

        REQUIRE(rig.Outputs.contains("gfx/indexed.png"));
        auto frame = ReadSingleFrame(rig.Outputs.at("gfx/indexed.png"));

        CHECK(frame.Width == 2);
        CHECK(frame.Height == 1);
        CHECK(frame.Data == vector<uint8_t> {255, 0, 0, 0, 0, 255, 0, 255});
    }

#if FO_IMAGE_BAM || FO_IMAGE_MOS || FO_IMAGE_RIX || FO_IMAGE_TIL || FO_IMAGE_ZAR
    SECTION("BakesSimpleLegacyFormats")
    {
        TestRig rig;
#if FO_IMAGE_RIX
        AddSourceBinaryFile(rig, "legacy/pixel.rix", MakeRix());
#endif
#if FO_IMAGE_ZAR
        AddSourceBinaryFile(rig, "legacy/blank.zar", MakeSimpleZar());
#endif
#if FO_IMAGE_ZAR
        AddSourceBinaryFile(rig, "legacy/palette.zar", MakePaletteZar());
#endif
#if FO_IMAGE_MOS
        AddSourceBinaryFile(rig, "legacy/panel.mos", MakeSimpleMos());
#endif
#if FO_IMAGE_BAM
        AddSourceBinaryFile(rig, "legacy/cycle.bam", MakeSimpleBam());
#endif
#if FO_IMAGE_TIL
        AddSourceBinaryFile(rig, "legacy/floor.til", MakeSimpleTil());
#endif

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

#if FO_IMAGE_RIX
        REQUIRE(rig.Outputs.contains("legacy/pixel.rix"));
        auto rix_frame = ReadSingleFrame(rig.Outputs.at("legacy/pixel.rix"));

        CHECK(rix_frame.Width == 1);
        CHECK(rix_frame.Height == 1);
        CHECK(rix_frame.Data == vector<uint8_t> {12, 8, 4, 255});
#endif

#if FO_IMAGE_ZAR
        REQUIRE(rig.Outputs.contains("legacy/blank.zar"));
        auto zar_frame = ReadSingleFrame(rig.Outputs.at("legacy/blank.zar"));

        CHECK(zar_frame.Width == 1);
        CHECK(zar_frame.Height == 1);
        CHECK(zar_frame.Data == vector<uint8_t> {0, 0, 0, 0});
#endif

#if FO_IMAGE_ZAR
        REQUIRE(rig.Outputs.contains("legacy/palette.zar"));
        auto palette_zar_frame = ReadSingleFrame(rig.Outputs.at("legacy/palette.zar"));

        CHECK(palette_zar_frame.Width == 3);
        CHECK(palette_zar_frame.Height == 1);
        CHECK(palette_zar_frame.Data == vector<uint8_t> {0x11, 0x22, 0x33, 0xFF, 0x11, 0x22, 0x33, 0x7F, 0x11, 0x22, 0x33, 0x40});
#endif

#if FO_IMAGE_MOS
        REQUIRE(rig.Outputs.contains("legacy/panel.mos"));
        auto mos_frame = ReadSingleFrame(rig.Outputs.at("legacy/panel.mos"));

        CHECK(mos_frame.Width == 2);
        CHECK(mos_frame.Height == 1);
        CHECK(mos_frame.Data == vector<uint8_t> {1, 2, 3, 255, 0, 0, 0, 0});
#endif

#if FO_IMAGE_BAM
        REQUIRE(rig.Outputs.contains("legacy/cycle.bam"));
        auto bam_frame = ReadSingleFrame(rig.Outputs.at("legacy/cycle.bam"));

        CHECK(bam_frame.Width == 4);
        CHECK(bam_frame.Height == 1);
        CHECK(bam_frame.NextX == 1);
        CHECK(bam_frame.NextY == -1);
        CHECK(bam_frame.Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF, 0, 0, 0, 0, 6, 5, 4, 0xFF, 6, 5, 4, 0xFF});
#endif

#if FO_IMAGE_TIL
        REQUIRE(rig.Outputs.contains("legacy/floor.til"));
        auto til_frame = ReadSingleFrame(rig.Outputs.at("legacy/floor.til"));

        CHECK(til_frame.Width == 3);
        CHECK(til_frame.Height == 1);
        CHECK(til_frame.Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF, 0x33, 0x22, 0x11, 0x7F, 0x33, 0x22, 0x11, 0x40});
#endif
    }
#endif

#if FO_IMAGE_ART
    SECTION("BakesArtFrameOptionsThroughFofrm")
    {
        // Palette 1, transparency from color, flipped (hot x = w - hot x - 2) and vertically mirrored
        TestRig rig;
        rig.AddSourceFile("gfx/art.fofrm", "Frame = marker$1thv.art\n");
        AddSourceBinaryFile(rig, "gfx/marker.art", MakeSimpleArt());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/art.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/art.fofrm"));
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/art.fofrm"));

        CHECK(sequence.SequenceSize == 1);
        CHECK(sequence.AnimTicks == 100);
        REQUIRE(sequence.Frames.size() == 1);
        CHECK(sequence.Frames[0].Width == 2);
        CHECK(sequence.Frames[0].Height == 2);
        CHECK(sequence.Frames[0].NextX == -3);
        CHECK(sequence.Frames[0].NextY == 2);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0, 0, 0, 0, 0, 0, 0, 0, 0x48, 0x09, 0x0A, 0x48, 0x44, 0x05, 0x06, 0x44});
    }
#endif

#if FO_IMAGE_BAM
    SECTION("BakesBamSpecificFrameOptionsThroughFofrm")
    {
        TestRig specific_frame;
        specific_frame.AddSourceFile("legacy/bam-specific.fofrm", "Frame = multi$9-1.bam\n");
        AddSourceBinaryFile(specific_frame, "legacy/multi.bam", MakeMultiFrameBam());

        ImageBaker specific_frame_baker {specific_frame.MakeContext()};
        specific_frame_baker.BakeFiles(specific_frame.GetAllSourceFiles(), "legacy/bam-specific.fofrm");

        REQUIRE(specific_frame.Outputs.contains("legacy/bam-specific.fofrm"));
        auto selected = ReadSingleDirSequence(specific_frame.Outputs.at("legacy/bam-specific.fofrm"));

        CHECK(selected.SequenceSize == 1);
        CHECK(selected.AnimTicks == 100);
        REQUIRE(selected.Frames.size() == 1);
        CHECK(selected.Frames[0].Width == 2);
        CHECK(selected.Frames[0].Height == 1);
        CHECK(selected.Frames[0].NextX == -1);
        CHECK(selected.Frames[0].NextY == -2);
        CHECK(selected.Frames[0].Data == vector<uint8_t> {0x06, 0x05, 0x04, 0xFF, 0x33, 0x22, 0x11, 0xFF});

        TestRig clamped_frame;
        clamped_frame.AddSourceFile("legacy/bam-clamped.fofrm", "Frame = multi$0-9.bam\n");
        AddSourceBinaryFile(clamped_frame, "legacy/multi.bam", MakeMultiFrameBam());

        ImageBaker clamped_frame_baker {clamped_frame.MakeContext()};
        clamped_frame_baker.BakeFiles(clamped_frame.GetAllSourceFiles(), "legacy/bam-clamped.fofrm");

        REQUIRE(clamped_frame.Outputs.contains("legacy/bam-clamped.fofrm"));
        auto clamped = ReadSingleDirSequence(clamped_frame.Outputs.at("legacy/bam-clamped.fofrm"));

        CHECK(clamped.SequenceSize == 1);
        CHECK(clamped.AnimTicks == 100);
        REQUIRE(clamped.Frames.size() == 1);
        CHECK(clamped.Frames[0].Width == 1);
        CHECK(clamped.Frames[0].Height == 1);
        CHECK(clamped.Frames[0].NextX == 0);
        CHECK(clamped.Frames[0].NextY == 1);
        CHECK(clamped.Frames[0].Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF});
    }
#endif

#if FO_IMAGE_ART
    SECTION("BakesArtFrameRangeAndRleThroughFofrm")
    {
        // Missing palette 3 falls back to the first present palette; frames share one canvas around their hot spots
        TestRig rig;
        rig.AddSourceFile("gfx/art-range.fofrm", "Frame = range$3F1-0.art\n");
        AddSourceBinaryFile(rig, "gfx/range.art", MakeRleFrameRangeArt());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/art-range.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/art-range.fofrm"));
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/art-range.fofrm"));

        CHECK(sequence.SequenceSize == 2);
        CHECK(sequence.AnimTicks == 200);
        REQUIRE(sequence.Frames.size() == 2);
        CHECK(sequence.Frames[0].Width == 3);
        CHECK(sequence.Frames[0].Height == 1);
        CHECK(sequence.Frames[0].NextX == 2);
        CHECK(sequence.Frames[0].NextY == -1);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0x04, 0x05, 0x06, 0xFF, 0x04, 0x05, 0x06, 0xFF, 0x08, 0x09, 0x0A, 0xFF});
        CHECK(sequence.Frames[1].Width == 3);
        CHECK(sequence.Frames[1].Height == 1);
        CHECK(sequence.Frames[1].Data == vector<uint8_t> {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});

        TestRig forward;
        forward.AddSourceFile("gfx/art-forward.fofrm", "Frame = range$1F0-1.art\n");
        AddSourceBinaryFile(forward, "gfx/range.art", MakeRleFrameRangeArt());

        ImageBaker forward_baker {forward.MakeContext()};
        forward_baker.BakeFiles(forward.GetAllSourceFiles(), "gfx/art-forward.fofrm");

        REQUIRE(forward.Outputs.contains("gfx/art-forward.fofrm"));
        auto forward_sequence = ReadSingleDirSequence(forward.Outputs.at("gfx/art-forward.fofrm"));

        CHECK(forward_sequence.SequenceSize == 2);
        REQUIRE(forward_sequence.Frames.size() == 2);
        CHECK(forward_sequence.Frames[0].Data == vector<uint8_t> {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
        CHECK(forward_sequence.Frames[1].NextX == 2);
        CHECK(forward_sequence.Frames[1].NextY == -1);
        CHECK(forward_sequence.Frames[1].Data == vector<uint8_t> {0x04, 0x05, 0x06, 0xFF, 0x04, 0x05, 0x06, 0xFF, 0x08, 0x09, 0x0A, 0xFF});
    }
#endif

#if FO_IMAGE_SPR
    SECTION("BakesSprSequenceOptionsThroughFofrm")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/spr.fofrm", "Frame = actor$[1,5,-10,20]walk.spr\n");
        AddSourceBinaryFile(rig, "gfx/actor.spr", MakeSimpleSpr());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/spr.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/spr.fofrm"));
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/spr.fofrm"));

        CHECK(sequence.SequenceSize == 1);
        CHECK(sequence.AnimTicks == 100);
        REQUIRE(sequence.Frames.size() == 1);
        CHECK(sequence.Frames[0].Width == 3);
        CHECK(sequence.Frames[0].Height == 1);
        CHECK(sequence.Frames[0].NextX == 1);
        CHECK(sequence.Frames[0].NextY == 1);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF, 0x2D, 0x14, 0x28, 0xFF, 0, 0, 0, 0x40});
    }
#endif

#if FO_IMAGE_SPR
    SECTION("BakesSprSkippedInvalidSequenceAndAllColorOffsets")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/spr-all-offsets.fofrm", "Frame = actor$[9,1,2,3]walk.spr\n");
        AddSourceBinaryFile(rig, "gfx/actor.spr", MakeSprWithSequenceFrames({-43, 11, 12, 13, 5}));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/spr-all-offsets.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/spr-all-offsets.fofrm"));
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/spr-all-offsets.fofrm"));

        CHECK(sequence.SequenceSize == 1);
        CHECK(sequence.AnimTicks == 100);
        REQUIRE(sequence.Frames.size() == 1);
        CHECK(sequence.Frames[0].Width == 3);
        CHECK(sequence.Frames[0].Height == 1);
        CHECK(sequence.Frames[0].NextX == 1);
        CHECK(sequence.Frames[0].NextY == 1);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0x34, 0x24, 0x14, 0xFF, 0x29, 0x20, 0x17, 0xFF, 0x01, 0x02, 0x03, 0x40});
    }
#endif

#if FO_IMAGE_SPR
    SECTION("BakesSprRepeatedFramesAsShared")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/repeated.spr", MakeSprWithSequenceFrames({0, 0}, 2));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/repeated.spr");

        REQUIRE(rig.Outputs.contains("gfx/repeated.spr"));

        auto resource = ReadSpriteResource(rig.Outputs.at("gfx/repeated.spr"));
        REQUIRE(resource.Directions.size() == GameSettings::MAP_DIR_COUNT);

        for (size_t dir = 0; dir < resource.Directions.size(); dir++) {
            auto sequence = ReadDirectionSequence(resource, dir);
            REQUIRE(sequence.Frames.size() == 2);
            CHECK(sequence.AnimTicks == 200);
            const auto& frame = sequence.Frames[0];
            CHECK(frame.Offset == ipos32 {});
            CHECK(frame.Height == 1);

            if (frame.Width == 3) {
                CHECK(frame.NextX == 1);
                CHECK(frame.NextY == 1);
                CHECK(frame.Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF, 0x28, 0x1E, 0x14, 0xFF, 0, 0, 0, 0x40});
            }
            else {
                CHECK(frame.Width == 1);
                CHECK(frame.NextX == 0);
                CHECK(frame.NextY == 1);
                CHECK(frame.Data == vector<uint8_t> {0, 0, 0, 0});
            }

            CHECK(resource.Directions[dir].Frames[1].SharedFrameIndex == optional<uint16_t> {0});
            CHECK(sequence.Frames[1].Offset == frame.Offset);
            CHECK(sequence.Frames[1].NextX == frame.NextX);
            CHECK(sequence.Frames[1].NextY == frame.NextY);
        }
    }
#endif

    SECTION("BakesFofrmFrameOffsetsAsDrawPlacement")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/offsets.fofrm", R"(Offset = 10 20
Frames = \
    "first frame.tga" \
    "first frame.tga" \
    "first frame.tga"
FrameOffsets = \
    6 -8 \
    -3 12 \
    0 0
)");
        AddSourceBinaryFile(rig, "gfx/first frame.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/offsets.fofrm");
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/offsets.fofrm"));
        REQUIRE(sequence.Frames.size() == 3);
        CHECK(sequence.Frames[0].Offset == ipos32 {16, 12});
        CHECK(sequence.Frames[1].Offset == ipos32 {7, 32});
        CHECK(sequence.Frames[2].Offset == ipos32 {10, 20});

        for (const auto& frame : sequence.Frames) {
            CHECK(frame.NextX == 0);
            CHECK(frame.NextY == 0);
        }
    }

    SECTION("BakesFofrmFrameOffsetsPerReferenceBeforeFlattening")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/parent.fofrm", "Offset = 3 4\nFrame = child.fofrm\nFrameOffsets = 5 6\n");
        rig.AddSourceFile("gfx/child.fofrm", "Offset = 99 88\nFrames = first.tga second.tga\nFrameOffsets = 2 3 -4 5\n");
        AddSourceBinaryFile(rig, "gfx/first.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
        AddSourceBinaryFile(rig, "gfx/second.tga", MakeRawTga(1, 1, 24, {0, 255, 0}));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/parent.fofrm");
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/parent.fofrm"));
        REQUIRE(sequence.Frames.size() == 2);
        CHECK(sequence.AnimTicks == 200);
        CHECK(sequence.Frames[0].Offset == ipos32 {10, 13});
        CHECK(sequence.Frames[1].Offset == ipos32 {4, 15});
    }

    SECTION("PreservesFofrmFrameOffsetsThroughMeshCropping")
    {
        TestRig rig;
        ConfigureSpriteMesh(rig);
        rig.AddSourceFile("gfx/base-offset.fofrm", "Offset = 10 20\nFrames = shape.tga shape.tga\n");
        rig.AddSourceFile("gfx/shifted-offset.fofrm", "Offset = 10 20\nFrames = shape.tga shape.tga\nFrameOffsets = 6 -8 -3 12\n");
        vector<uint8_t> pixels(20 * 20 * 4);
        pixels[(10 * 20 + 10) * 4 + 3] = 255;
        AddSourceBinaryFile(rig, "gfx/shape.tga", MakeRawTga(20, 20, 32, pixels));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/base-offset.fofrm");
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/shifted-offset.fofrm");
        auto base = ReadSingleDirSequence(rig.Outputs.at("gfx/base-offset.fofrm"));
        auto shifted = ReadSingleDirSequence(rig.Outputs.at("gfx/shifted-offset.fofrm"));
        REQUIRE(base.Frames.size() == 2);
        REQUIRE(shifted.Frames.size() == 2);
        REQUIRE(base.Frames[0].MeshKind == SpriteMeshKind::Mesh);
        CHECK(shifted.Frames[0].Offset == base.Frames[0].Offset + ipos32 {6, -8});
        CHECK(shifted.Frames[1].Offset == base.Frames[1].Offset + ipos32 {-3, 12});
        CHECK(shifted.Frames[0].Data == base.Frames[0].Data);
        CHECK(shifted.Frames[1].Mesh.Vertices == base.Frames[1].Mesh.Vertices);

        rig.AddSourceFile("gfx/large-offset.fofrm", "Frame = shape.tga\nFrameOffsets = 32768 0\n");
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/large-offset.fofrm");
        CHECK(ReadSingleFrame(rig.Outputs.at("gfx/large-offset.fofrm")).Offset.x == base.Frames[0].Offset.x - 10 + 32768);

        for (string_view value : {"0 -2147483648"}) {
            rig.AddSourceFile("gfx/extreme-offset.fofrm", strex("Frame = shape.tga\nFrameOffsets = {}\n", value).str());
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/extreme-offset.fofrm"), ImageBakerException);
            CHECK_FALSE(rig.Outputs.contains("gfx/extreme-offset.fofrm"));
        }
    }

    SECTION("RejectsInvalidFofrmFrameOffsets")
    {
        for (string_view value : {"", "1", "1 2", "1 2 3", "1 2 3 4 5 6", "a 2 3 4", "1.5 2 3 4", "2147483648 2 3 4"}) {
            CAPTURE(value);
            TestRig rig;
            rig.AddSourceFile("gfx/bad.fofrm", strex("Frames = base.tga base.tga\nFrameOffsets = {}\n", value).str());
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/bad.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("RejectsIndexedFofrmFrameDeltasBeforeAlias")
    {
        FO_TEMPORARY_COMPAT(FofrmLegacyDescriptor, "2026-12-31");

        for (string_view key : {"NextX_0", "NextY_0"}) {
            TestRig rig;
            rig.AddSourceFile("gfx/old.fofrm", strex("Source = base.tga\n{} = 1\n", key).str());
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/old.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("RejectsCompositeFofrmFrameOffsetTokens")
    {
        for (string_view value : {R"("1 2" "")", R"("1 " " 2")"}) {
            TestRig rig;
            rig.AddSourceFile("gfx/token.fofrm", strex("Frame = base.tga\nFrameOffsets = {}\n", value).str());
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/token.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("RejectsInvalidFrameOffsetsInRootWithDirections")
    {
        for (string_view root : {"FrameOffsets = 1 2\n", "Frame = base.tga\nFrameOffsets = 1 2 3 4\n"}) {
            TestRig rig;
            string descriptor {root};

            for (int32_t dir = 1; dir < GameSettings::MAP_DIR_COUNT; dir++) {
                descriptor += strex("[{} {}]\nFrame = base.tga\n", fofrm_direction_prefix, dir);
            }

            rig.AddSourceFile("gfx/unused-root.fofrm", descriptor);
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/unused-root.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("BakesFofrmFrameListsAndPairedOffset")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/list.fofrm", R"(Fps = 5
Offset = -3 4
Frames = \
    first.tga \
    second.tga \
    first.tga
FrameOffsets = 2 0 0 0 0 0
)");
        AddSourceBinaryFile(rig, "gfx/first.tga", MakeRawTga(1, 1, 32, {0, 0, 255, 255}));
        AddSourceBinaryFile(rig, "gfx/second.tga", MakeRawTga(1, 1, 32, {0, 255, 0, 128}));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/list.fofrm");

        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/list.fofrm"));
        CHECK(sequence.SequenceSize == 3);
        CHECK(sequence.AnimTicks == 600);
        REQUIRE(sequence.Frames.size() == 3);
        CHECK(sequence.Frames[0].Offset == ipos32 {-1, 4});
        CHECK(sequence.Frames[1].Offset == ipos32 {-3, 4});
        CHECK(sequence.Frames[2].Offset == ipos32 {-3, 4});
        CHECK(sequence.Frames[0].NextX == 0);
        CHECK(sequence.Frames[1].Data == vector<uint8_t> {0, 255, 0, 128});
        CHECK(sequence.Frames[2].Data == sequence.Frames[0].Data);
    }

    SECTION("BakesFofrmQuotedFramePaths")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/quoted.fofrm", R"(Fps = 5
Frames = \
    "first frame.tga" \
    second.tga \
    "first frame.tga"
FrameOffsets = 2 0 0 0 0 0
)");
        AddSourceBinaryFile(rig, "gfx/first frame.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
        AddSourceBinaryFile(rig, "gfx/second.tga", MakeRawTga(1, 1, 24, {0, 255, 0}));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/quoted.fofrm");

        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/quoted.fofrm"));
        CHECK(sequence.SequenceSize == 3);
        CHECK(sequence.AnimTicks == 600);
        REQUIRE(sequence.Frames.size() == 3);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {255, 0, 0, 255});
        CHECK(sequence.Frames[1].Data == vector<uint8_t> {0, 255, 0, 255});
        CHECK(sequence.Frames[2].Data == sequence.Frames[0].Data);
        CHECK(sequence.Frames[0].NextX == 0);
        CHECK(sequence.Frames[0].Offset == ipos32 {2, 0});
    }

    SECTION("RejectsMalformedFofrmQuotedFrames")
    {
        for (string_view frames : {R"("first frame.tga)", R"("first frame.tga"second.tga)", "first.tga\\"}) {
            CAPTURE(frames);
            TestRig rig;
            rig.AddSourceFile("gfx/invalid-quotes.fofrm", strex("Frames = {}\n", frames).str());
            AddSourceBinaryFile(rig, "gfx/first.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/invalid-quotes.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("BakesFofrmSingleFramePathWithSpaces")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/single.fofrm", "Offset = +3\t-4\nFrame = first frame.tga\n");
        AddSourceBinaryFile(rig, "gfx/first frame.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/single.fofrm");

        auto frame = ReadSingleFrame(rig.Outputs.at("gfx/single.fofrm"));
        CHECK(frame.Offset == ipos32 {3, -4});
        CHECK(frame.Data == vector<uint8_t> {255, 0, 0, 255});
    }

    SECTION("BakesFofrmIndexedDirectionsWithImplicitZeroAndOffsetCarry")
    {
        TestRig rig;
        string descriptor = "Offset = 7 -8\n";

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            if (dir != 0) {
                descriptor += strex("[{} {}]\n", fofrm_direction_prefix, dir);
            }

            descriptor += strex("Frame = frame{}.tga\n", dir);

            if (dir == 0) {
                descriptor += "FrameOffsets = 5 -6\n";
            }

            if (dir == 2) {
                descriptor += "Offset = -3 2\n";
            }

            AddSourceBinaryFile(rig, strex("gfx/frame{}.tga", dir).str(), MakeRawTga(1, 1, 24, {0, 0, numeric_cast<uint8_t>(dir + 1)}));
        }

        rig.AddSourceFile("gfx/directions.fofrm", descriptor);
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/directions.fofrm");

        auto sprite = ReadSpriteResource(rig.Outputs.at("gfx/directions.fofrm"));
        REQUIRE(sprite.Directions.size() == GameSettings::MAP_DIR_COUNT);

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            REQUIRE(sprite.Directions[dir].Frames.size() == 1);
            CHECK(sprite.Directions[dir].Frames.front().Offset == (dir == 0 ? ipos32 {12, -14} : dir == 1 ? ipos32 {7, -8} : ipos32 {-3, 2}));
            CHECK(sprite.Directions[dir].Frames.front().Pixels == vector<ucolor> {ucolor {numeric_cast<uint8_t>(dir + 1), 0, 0, 255}});
        }
    }

    SECTION("RejectsExplicitlyEmptySecondDirectionFrames")
    {
        for (string_view key : {"Frame", "Frames"}) {
            CAPTURE(key);
            TestRig rig;
            rig.AddSourceFile("gfx/empty-direction.fofrm", strex("Frame = base.tga\n[{} 1]\n{} =\n", fofrm_direction_prefix, key).str());
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/empty-direction.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("RejectsRetiredFofrmFieldsBeforeAlias")
    {
        FO_TEMPORARY_COMPAT(FofrmLegacyDescriptor, "2026-12-31");

        for (string_view field : {"Count = 1", "OffsetX = 1", "OffsetY = 2", "Frm = base.tga", "Frm_0 = base.tga"}) {
            CAPTURE(field);
            TestRig rig;
            rig.AddSourceFile("gfx/invalid.fofrm", strex("Source = base.tga\n{}\n", field).str());
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/invalid.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("RejectsInvalidFofrmFrameListsAndOffsets")
    {
        for (string_view descriptor : {"Frame = base.tga\nFrames = base.tga\n", "Frame =\n", "Frames =\n", "Frame = base.tga\nOffset = 1\n", "Frame = base.tga\nOffset = 1 2 3\n", "Frame = base.tga\nOffset = a 2\n", "Frame = base.tga\nOffset = 1.5 2\n", "Frame = base.tga\nOffset = 1-2\n", "Frame = base.tga\nOffset = 2147483648 0\n"}) {
            CAPTURE(descriptor);
            TestRig rig;
            rig.AddSourceFile("gfx/invalid.fofrm", descriptor);
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/invalid.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("RejectsNonPascalCaseFofrmKeys")
    {
        FO_TEMPORARY_COMPAT(FofrmLegacyDescriptor, "2026-12-31");

        for (string_view field : {"fps=10", "count=1", "offs_x=1", "offs_y=2", "effect=unused.fofx", "frm=base.tga", "frm_0=base.tga", "next_x_0=3", "next_y_0=4", "offsetX=1", "offsetY=2", "nextX_0=3", "nextY_0=4", "alias=base.tga"}) {
            CAPTURE(field);

            TestRig rig;
            rig.AddSourceFile("gfx/invalid.fofrm", strex("Frame = base.tga\n{}\n", field).str());
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));

            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/invalid.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("RejectsNonPascalCaseFofrmSectionsBeforeAlias")
    {
        FO_TEMPORARY_COMPAT(FofrmLegacyDescriptor, "2026-12-31");

        for (string_view section : {"dir_0", "dir_1", "DIR_0", "Dir_00", "Dir_99", "Dir_0", "Dir30", "Dir 1", "Dir 360"}) {
            CAPTURE(section);

            TestRig rig;
            rig.AddSourceFile("gfx/invalid.fofrm", strex("Source = base.tga\n[{}]\nFrame = base.tga\n", section).str());
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));

            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/invalid.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("RejectsFofrmDegreeSectionsBeforeAlias")
    {
        FO_TEMPORARY_COMPAT(FofrmLegacyDescriptor, "2026-12-31");

        for (string_view section : {"Dir 30", "Dir 90", "Dir 330"}) {
            TestRig rig;
            rig.AddSourceFile("gfx/degree.fofrm", strex("Source = base.tga\n[{}]\nFrame = base.tga\n", section).str());
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/degree.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("RejectsFofrmNamedZeroAndInvalidDirectionIndexesBeforeAlias")
    {
        for (string_view section : {"HexDir 0", "SquareDir 0", "HexDir 6", "SquareDir 8", "HexDir -1", "HexDir 01", "HexDir 1.5", "HexDir1", "hexDir 1"}) {
            TestRig rig;
            rig.AddSourceFile("gfx/index.fofrm", strex("Source = base.tga\n[{}]\nFrame = base.tga\n", section).str());
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/index.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("RejectsNonPascalCaseFofrmKeysBeforeAlias")
    {
        FO_TEMPORARY_COMPAT(FofrmLegacyDescriptor, "2026-12-31");
        TestRig rig;
        rig.AddSourceFile("gfx/invalid.fofrm", "Source=base.tga\nfps=10\n");
        AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));

        ImageBaker baker {rig.MakeContext()};
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/invalid.fofrm"), ImageBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsUnknownFofrmKeysInCanonicalDirectionBeforeAlias")
    {
        for (string_view field : {"frm_0=base.tga", "offsetX=1", "Unknown=1", "Frm_=base.tga", "NextX_bad=1"}) {
            CAPTURE(field);

            TestRig rig;
            rig.AddSourceFile("gfx/invalid.fofrm", strex("Source = base.tga\n[{} 1]\n{}\n", fofrm_direction_prefix, field).str());
            AddSourceBinaryFile(rig, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));

            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/invalid.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("BakesFofrmSequenceThroughNestedImages")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/anim.fofrm", R"(
Fps = 5
Offset = 3 4
Frames = first.tga second.tga
FrameOffsets = 5 6 -2 8
)");
        AddSourceBinaryFile(rig, "gfx/first.tga", MakeRawTga(1, 1, 32, {0, 0, 255, 255}));
        AddSourceBinaryFile(rig, "gfx/second.tga", MakeRawTga(1, 1, 32, {0, 255, 0, 128}));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/anim.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/anim.fofrm"));
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/anim.fofrm"));

        CHECK(sequence.SequenceSize == 2);
        CHECK(sequence.AnimTicks == 400);
        REQUIRE(sequence.Frames.size() == 2);
        CHECK(sequence.Frames[0].Offset == ipos32 {8, 10});
        CHECK(sequence.Frames[1].Offset == ipos32 {1, 12});
        CHECK(sequence.Frames[0].NextX == 0);
        CHECK(sequence.Frames[0].NextY == 0);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {255, 0, 0, 255});
        CHECK(sequence.Frames[1].NextX == 0);
        CHECK(sequence.Frames[1].NextY == 0);
        CHECK(sequence.Frames[1].Data == vector<uint8_t> {0, 255, 0, 128});
    }

    SECTION("FofrmDirectionEdgeCases")
    {
        TestRig dir_one_without_frames;
        dir_one_without_frames.AddSourceFile("gfx/no-dir.fofrm",
            strex(R"(
Frame = base.tga

[{} 1]
Offset = 5 0
)",
                fofrm_direction_prefix)
                .str());
        AddSourceBinaryFile(dir_one_without_frames, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));

        ImageBaker dir_one_baker {dir_one_without_frames.MakeContext()};
        CHECK_THROWS_AS(dir_one_baker.BakeFiles(dir_one_without_frames.GetAllSourceFiles(), "gfx/no-dir.fofrm"), ImageBakerException);
        CHECK(dir_one_without_frames.Outputs.empty());

        TestRig direction_gap;
        direction_gap.AddSourceFile("gfx/gap.fofrm",
            strex(R"(
Frame = base.tga

[{} 1]
Frame = dir1.tga
)",
                fofrm_direction_prefix)
                .str());
        AddSourceBinaryFile(direction_gap, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
        AddSourceBinaryFile(direction_gap, "gfx/dir1.tga", MakeRawTga(1, 1, 24, {0, 255, 0}));

        ImageBaker direction_gap_baker {direction_gap.MakeContext()};
        CHECK_THROWS_AS(direction_gap_baker.BakeFiles(direction_gap.GetAllSourceFiles(), "gfx/gap.fofrm"), ImageBakerException);
        CHECK(direction_gap.Outputs.empty());

        auto add_toy_loader = [](ImageBaker& baker) {
            baker.AddLoader(
                [](string_view fname, string_view opt, FileReader reader, const ImageImport::Context& files) {
                    ignore_unused(opt, reader, files);

                    string fname_str {fname};
                    ImageImport::FrameCollection collection;
                    [[maybe_unused]] uint16_t sequence_size {};
                    uint32_t duration_ms {100};
                    bool have_dirs {};
                    sequence_size = fname_str.find("two") != string::npos ? 2 : 1;
                    duration_ms = 100;
                    collection.Main.Frames.resize(sequence_size);

                    if (fname_str.find("shared") != string::npos) {
                        collection.Main.Frames[0] = ImageImport::FrameShot {.Shared = true, .SharedIndex = 0};

                        if (!have_dirs) {
                            for (auto& direction : collection.Dirs) {
                                direction.Frames.clear();
                            }
                        }
                        else {
                            for (int32_t direction = GameSettings::MAP_DIR_COUNT - 1; direction < ImageImport::MAX_DIRS_MINUS_ONE; direction++) {
                                collection.Dirs[direction].Frames.clear();
                            }
                        }

                        for (int32_t direction = 0; direction < (have_dirs ? GameSettings::MAP_DIR_COUNT : 1); direction++) {
                            auto& frames = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

                            for (size_t index = 0; index < frames.Frames.size(); index++) {
                                auto& shot = frames.Frames[index];

                                if (shot.Shared && shot.SharedIndex < index) {
                                    uint16_t reference = shot.SharedIndex;
                                    shot = frames.Frames[reference];
                                    shot.Data.clear();
                                    shot.Shared = true;
                                    shot.SharedIndex = reference;
                                }
                            }

                            ImageImport::SetFrameSequenceDuration(frames, duration_ms);
                        }

                        return collection;
                    }

                    for (uint16_t i = 0; i < sequence_size; i++) {
                        collection.Main.Frames[i] = ImageImport::FrameShot {
                            .Width = 1,
                            .Height = 1,
                            .Data = {numeric_cast<uint8_t>(10 + i), 20, 30, 255},
                        };
                    }

                    ImageImport::SetFrameSequenceDuration(collection.Main, duration_ms);

                    return collection;
                },
                {"toy"});
        };

        TestRig mismatched_direction;
        mismatched_direction.AddSourceFile("gfx/mismatch.fofrm",
            strex(R"(
Frame = two.toy

[{} 1]
Frame = one.toy
)",
                fofrm_direction_prefix)
                .str());
        mismatched_direction.AddSourceFile("gfx/two.toy", "two");
        mismatched_direction.AddSourceFile("gfx/one.toy", "one");

        ImageBaker mismatched_direction_baker {mismatched_direction.MakeContext()};
        add_toy_loader(mismatched_direction_baker);
        CHECK_THROWS_AS(mismatched_direction_baker.BakeFiles(mismatched_direction.GetAllSourceFiles(), "gfx/mismatch.fofrm"), ImageBakerException);
        CHECK(mismatched_direction.Outputs.empty());

        TestRig shared_nested_frame;
        shared_nested_frame.AddSourceFile("gfx/shared.fofrm", "Frame = shared.toy\n");
        shared_nested_frame.AddSourceFile("gfx/shared.toy", "shared");

        ImageBaker shared_nested_frame_baker {shared_nested_frame.MakeContext()};
        add_toy_loader(shared_nested_frame_baker);
        CHECK_THROWS_AS(shared_nested_frame_baker.BakeFiles(shared_nested_frame.GetAllSourceFiles(), "gfx/shared.fofrm"), ImageBakerException);
        CHECK(shared_nested_frame.Outputs.empty());
    }

    SECTION("ImageImportFailuresPreserveSourceAndNestedContext")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/broken.toy", "x");
        rig.AddSourceFile("gfx/alias.fofrm", "Source=broken$probe.toy\n");
        rig.AddSourceFile("gfx/composed.fofrm", "Frame = broken$probe.toy\n");
        ImageBaker baker {rig.MakeContext()};
        baker.AddLoader(
            [](string_view, string_view, FileReader reader, const ImageImport::Context&) -> ImageImport::FrameCollection {
                (void)reader.GetLEUInt32();
                ImageImport::FrameCollection collection;
                return collection;
            },
            {"toy"});

        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/broken.toy");
        CHECK(rig.Outputs.empty());

        for (string_view path : {"gfx/alias.fofrm", "gfx/composed.fofrm"}) {
            try {
                baker.BakeFiles(rig.GetAllSourceFiles(), path);
                FAIL("Truncated image imported without an error");
            }
            catch (const ImageBakerException& ex) {
                string_view message = ex.what();
                CHECK(message.find("Image import failed") != string_view::npos);
                CHECK(message.find(path) != string_view::npos);
                CHECK(message.find("gfx/broken.toy") != string_view::npos);
                CHECK(message.find("Invalid read size") != string_view::npos);
            }

            CHECK(rig.Outputs.empty());
        }

        CHECK_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/alias.fofrm"), Catch::Matchers::ContainsSubstring("probe"));
        CHECK_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("gfx/broken.toy"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("CustomLoaderBakesDirsSharedFramesAndNewName")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/custom.toy", "payload");

        ImageBaker baker {rig.MakeContext()};
        baker.AddLoader(
            [](string_view fname, string_view opt, FileReader reader, const ImageImport::Context& files) {
                ignore_unused(files);

                CHECK(fname == "gfx/custom.toy");
                CHECK(opt.empty());
                CHECK(reader.GetSize() == 7);

                ImageImport::FrameCollection collection;
                [[maybe_unused]] uint16_t sequence_size {};
                uint32_t duration_ms {100};
                bool have_dirs {};
                sequence_size = 2;
                duration_ms = 77;
                have_dirs = true;
                collection.NewName = "gfx/custom.baked";

                collection.Main.OffsX = 1;
                collection.Main.OffsY = 2;
                collection.Main.Frames = {
                    ImageImport::FrameShot {.Width = 1, .Height = 1, .NextX = 3, .NextY = 4, .Data = {10, 20, 30, 40}},
                    ImageImport::FrameShot {.Shared = true, .SharedIndex = 0},
                };

                for (int32_t dir = 0; dir < ImageImport::MAX_DIRS_MINUS_ONE; dir++) {
                    auto& sequence = collection.Dirs[dir];
                    sequence.OffsX = numeric_cast<int16_t>(10 + dir);
                    sequence.OffsY = numeric_cast<int16_t>(20 + dir);
                    sequence.Frames = {
                        ImageImport::FrameShot {.Width = 1, .Height = 1, .NextX = numeric_cast<int16_t>(30 + dir), .NextY = numeric_cast<int16_t>(40 + dir), .Data = {numeric_cast<uint8_t>(dir), 1, 2, 3}},
                        ImageImport::FrameShot {.Shared = true, .SharedIndex = 0},
                    };
                }

                if (!have_dirs) {
                    for (auto& direction : collection.Dirs) {
                        direction.Frames.clear();
                    }
                }
                else {
                    for (int32_t direction = GameSettings::MAP_DIR_COUNT - 1; direction < ImageImport::MAX_DIRS_MINUS_ONE; direction++) {
                        collection.Dirs[direction].Frames.clear();
                    }
                }

                for (int32_t direction = 0; direction < (have_dirs ? GameSettings::MAP_DIR_COUNT : 1); direction++) {
                    auto& frames = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

                    for (size_t index = 0; index < frames.Frames.size(); index++) {
                        auto& shot = frames.Frames[index];

                        if (shot.Shared && shot.SharedIndex < index) {
                            uint16_t reference = shot.SharedIndex;
                            shot = frames.Frames[reference];
                            shot.Data.clear();
                            shot.Shared = true;
                            shot.SharedIndex = reference;
                        }
                    }

                    ImageImport::SetFrameSequenceDuration(frames, duration_ms);
                }

                return collection;
            },
            {"toy"}, [](string_view) { return string {"gfx/custom.baked"}; });

        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/custom.baked");

        REQUIRE_FALSE(rig.Outputs.contains("gfx/custom.toy"));
        REQUIRE(rig.Outputs.contains("gfx/custom.baked"));

        auto resource = ReadSpriteResource(rig.Outputs.at("gfx/custom.baked"));
        REQUIRE(resource.Directions.size() == GameSettings::MAP_DIR_COUNT);

        for (size_t dir = 0; dir < resource.Directions.size(); dir++) {
            auto sequence = ReadDirectionSequence(resource, dir);
            REQUIRE(sequence.Frames.size() == 2);
            CHECK(sequence.AnimTicks == 77);
            const auto& frame = sequence.Frames[0];
            CHECK(frame.Offset == (dir == 0 ? ipos32 {1, 2} : ipos32 {9 + numeric_cast<int32_t>(dir), 19 + numeric_cast<int32_t>(dir)}));
            CHECK(frame.Width == 1);
            CHECK(frame.Height == 1);
            CHECK(frame.Data.size() == 4);
            CHECK(resource.Directions[dir].Frames[1].SharedFrameIndex == optional<uint16_t> {0});
            CHECK(sequence.Frames[0].DurationMs == 38);
            CHECK(sequence.Frames[1].DurationMs == 39);
        }
    }

    SECTION("BakeCheckerSkipsTarget")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/skipped.tga", MakeRawTga(1, 1, 24, {0, 0, 255}), 30);

        ImageBaker baker {rig.MakeContext("TestPack", [](string_view path, uint64_t write_time) {
            CHECK(path == "gfx/skipped.tga");
            CHECK(write_time == 30);
            return false;
        })};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/skipped.tga");

        CHECK(rig.Outputs.empty());
    }

    SECTION("ScanModeRejectsIncompleteSpriteInfoIndex")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/skipped-scan.tga", MakeRawTga(1, 1, 24, {0, 0, 255}), 31);

        vector<pair<string, uint64_t>> checked;

        ImageBaker baker {rig.MakeContext("TestPack", [&checked](string_view path, uint64_t write_time) {
            checked.emplace_back(string {path}, write_time);
            return false;
        })};
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), VerificationException);

        CHECK(rig.Outputs.empty());
        CHECK(checked ==
            vector<pair<string, uint64_t>> {
                {"gfx/skipped-scan.tga", 31},
                {"SpriteInfo/TestPack.foinfo", 31},
            });
    }

#if FO_IMAGE_FRM
    SECTION("ScanModeRegistersTheResourceEachSourceBakesTo")
    {
        // The master baker keeps the spelling the checker registered, so a critter frame registered under its source
        // spelling was renamed back to it after baking, where the critter frame lookup never finds it
        TestRig rig;
        vector<FrmFrameSpec> frames {FrmFrameSpec {.Width = 1, .Height = 1, .Indices = {1}}};
        AddSourceBinaryFile(rig, "art/critters/RAT.FRM", MakeFrm(5, frames));
        AddSourceBinaryFile(rig, "art/scenery/ROCK.FRM", MakeFrm(5, frames));

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            AddSourceBinaryFile(rig, strex("art/critters/DOG.fr{}", dir), MakeFrxDir(5, numeric_cast<uint8_t>(dir), frames, 0, 0));
            AddSourceBinaryFile(rig, strex("art/misc/FIRE.fr{}", dir), MakeFrxDir(5, numeric_cast<uint8_t>(dir), frames, 0, 0));
        }

        set<string> checked;

        ImageBaker baker {rig.MakeContext("TestPack", [&checked](string_view path, uint64_t write_time) {
            ignore_unused(write_time);
            checked.emplace(path);
            return true;
        })};
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        CHECK(checked == set<string> {"art/critters/rat.frm", "art/scenery/ROCK.FRM", "art/critters/dog.fofrm", "art/misc/FIRE.frm", "SpriteInfo/TestPack.foinfo"});
        CHECK(rig.Outputs.size() == checked.size());

        for (const string& path : checked) {
            CHECK(rig.Outputs.contains(path));
        }
    }
#endif

    SECTION("BakesSpriteInfoIndexAsRuntimeTarget")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/runtime-info.tga", MakeRawTga(1, 1, 24, {0, 0, 255}), 32);

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "SpriteInfo/TestPack.foinfo");

        REQUIRE(rig.Outputs.contains("gfx/runtime-info.tga"));
        REQUIRE(rig.Outputs.contains("SpriteInfo/TestPack.foinfo"));
        const vector<uint8_t>& sprite_info_data = rig.Outputs.at("SpriteInfo/TestPack.foinfo");
        vector<SpriteInfoFileEntry> entries = ReadSpriteInfoFile("SpriteInfo/TestPack.foinfo", string(sprite_info_data.begin(), sprite_info_data.end()));
        REQUIRE(entries.size() == 1);
        CHECK(entries.front().SourcePath == "gfx/runtime-info.tga");
        CHECK(entries.front().ResourcePath == "gfx/runtime-info.tga");
        CHECK(entries.front().Info.Directions[0].Frames.size() == 1);
        CHECK(entries.front().Info.Directions.front().Frames.front().Size == isize32 {1, 1});
    }

    SECTION("ScanModeRemovesStaleSpriteInfoEntries")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/current.tga", MakeRawTga(1, 1, 24, {0, 0, 255}), 31);

        SpriteInfo info {
            .Directions = {SpriteDirInfo {.Frames = {SpriteFrameInfo {.Size = {1, 1}, .DurationMs = 100}}}},
        };

        rig.AddBakedFile("SpriteInfo/TestPack.foinfo",
            WriteSpriteInfoFile({
                SpriteInfoFileEntry {.SourcePath = "gfx/current.tga", .ResourcePath = "gfx/current.tga", .Info = info},
                SpriteInfoFileEntry {.SourcePath = "gfx/removed.tga", .ResourcePath = "gfx/removed.tga", .Info = info},
            }));

        ImageBaker baker {rig.MakeContext("TestPack", [](string_view, uint64_t) { return false; })};
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.contains("SpriteInfo/TestPack.foinfo"));
        const vector<uint8_t>& sprite_info_data = rig.Outputs.at("SpriteInfo/TestPack.foinfo");
        vector<SpriteInfoFileEntry> entries = ReadSpriteInfoFile("SpriteInfo/TestPack.foinfo", string(sprite_info_data.begin(), sprite_info_data.end()));
        REQUIRE(entries.size() == 1);
        CHECK(entries.front().SourcePath == "gfx/current.tga");
    }

    SECTION("InvalidTgaInputsAreReported")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/truncated.tga", vector<uint8_t> {0, 0, 2}, 1);
        AddSourceBinaryFile(rig, "gfx/indexed.tga", [] {
            auto data = MakeRawTga(1, 1, 24, {0, 0, 0});
            data[2] = 1;
            return data;
        }());
        AddSourceBinaryFile(rig, "gfx/gray.tga", [] {
            auto data = MakeRawTga(1, 1, 24, {0, 0, 0});
            data[2] = 3;
            return data;
        }());
        AddSourceBinaryFile(rig, "gfx/bad-depth.tga", MakeRawTga(1, 1, 16, {0, 0}));

        ImageBaker baker {rig.MakeContext()};

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/truncated.tga"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/indexed.tga"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/gray.tga"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/bad-depth.tga"), ImageBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("InvalidPngInputIsReported")
    {
        TestRig rig;
        auto corrupted_png = MakePaletteTransparencyPng();
        corrupted_png[80] ^= 0xFF;
        AddSourceBinaryFile(rig, "gfx/corrupted.png", corrupted_png);

        ImageBaker baker {rig.MakeContext()};

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/corrupted.png"), ImageBakerException);
        CHECK(rig.Outputs.empty());
    }

#if FO_IMAGE_BAM || FO_IMAGE_MOS || FO_IMAGE_TIL || FO_IMAGE_ZAR
    SECTION("InvalidLegacyHeadersAreReported")
    {
        TestRig rig;
#if FO_IMAGE_ZAR
        AddSourceBinaryFile(rig, "legacy/bad.zar", vector<uint8_t> {'b', 'a', 'd', 'z', 'a', 'r'});
        AddSourceBinaryFile(rig, "legacy/bad-palette.zar", MakeZarWithPaletteCount(257));
#endif
#if FO_IMAGE_TIL
        AddSourceBinaryFile(rig, "legacy/bad.til", vector<uint8_t> {'b', 'a', 'd', 't', 'i', 'l', '\0'});
        AddSourceBinaryFile(rig, "legacy/missing-tiledata.til", MakeTilWithoutTiledata());
        AddSourceBinaryFile(rig, "legacy/bad-zar-header.til", MakeTilWithBadZarHeader());
        AddSourceBinaryFile(rig, "legacy/bad-palette.til", MakeTilWithPaletteCount(257));
#endif
#if FO_IMAGE_MOS
        AddSourceBinaryFile(rig, "legacy/bad.mos", vector<uint8_t> {'b', 'a', 'd', 'm', 'o', 's', '\0', '\0'});
#endif
#if FO_IMAGE_BAM
        AddSourceBinaryFile(rig, "legacy/bad.bam", vector<uint8_t> {'b', 'a', 'd', 'b', 'a', 'm', '\0', '\0'});
#endif

        ImageBaker baker {rig.MakeContext()};

#if FO_IMAGE_ZAR
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad.zar"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad-palette.zar"), ImageBakerException);
#endif
#if FO_IMAGE_TIL
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad.til"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/missing-tiledata.til"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad-zar-header.til"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad-palette.til"), ImageBakerException);
#endif
#if FO_IMAGE_MOS
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad.mos"), ImageBakerException);
#endif
#if FO_IMAGE_BAM
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad.bam"), ImageBakerException);
#endif
        CHECK(rig.Outputs.empty());
    }
#endif

#if FO_IMAGE_SPR
    SECTION("InvalidSprInputsAreReported")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/bad-header.spr", vector<uint8_t>(11));
        rig.AddSourceFile("gfx/missing-sequence.fofrm", "Frame = actor$run.spr\n");
        AddSourceBinaryFile(rig, "gfx/actor.spr", MakeSimpleSpr());

        auto missing_spranim = MakeSimpleSpr();
        ReplaceFirstTagByte(missing_spranim, "<spranim>", numeric_cast<uint8_t>('x'));
        AddSourceBinaryFile(rig, "gfx/missing-spranim.spr", missing_spranim);

        auto bad_internal_zar = MakeSimpleSpr();
        ReplaceFirstTagByte(bad_internal_zar, "<zar>", numeric_cast<uint8_t>('x'));
        AddSourceBinaryFile(rig, "gfx/bad-internal-zar.spr", bad_internal_zar);

        ImageBaker baker {rig.MakeContext()};

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/bad-header.spr"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/missing-sequence.fofrm"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/missing-spranim.spr"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/bad-internal-zar.spr"), ImageBakerException);
        CHECK(rig.Outputs.empty());
    }
#endif

    SECTION("InvalidFofrmNestedFramesAreReported")
    {
        TestRig missing_frame;
        missing_frame.AddSourceFile("gfx/missing.fofrm", "Frame = missing.tga\n");

        ImageBaker missing_baker {missing_frame.MakeContext()};
        CHECK_THROWS_AS(missing_baker.BakeFiles(missing_frame.GetAllSourceFiles(), "gfx/missing.fofrm"), ImageBakerException);
        CHECK(missing_frame.Outputs.empty());

        TestRig unsupported_frame;
        unsupported_frame.AddSourceFile("gfx/unsupported.fofrm", "Frame = frame.unsupported\n");
        unsupported_frame.AddSourceFile("gfx/frame.unsupported", "payload");

        ImageBaker unsupported_baker {unsupported_frame.MakeContext()};
        CHECK_THROWS_AS(unsupported_baker.BakeFiles(unsupported_frame.GetAllSourceFiles(), "gfx/unsupported.fofrm"), ImageBakerException);
        CHECK(unsupported_frame.Outputs.empty());
    }
}

TEST_CASE("FofrmResizing")
{
    using namespace BakerTests;

    SECTION("BakesFofrmSizeAndKeepsPixelOffsets")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/size.fofrm", "Frame = source.tga\nSize = 4 2\nOffset = 10 20\nFrameOffsets = 5 -6\n");
        AddSourceBinaryFile(rig, "gfx/source.tga", MakeRawTga(2, 1, 24, {0, 0, 255, 255, 0, 0}));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "SpriteInfo/TestPack.foinfo");
        auto frame = ReadSingleFrame(rig.Outputs.at("gfx/size.fofrm"));
        CHECK(frame.Width == 4);
        CHECK(frame.Height == 2);
        CHECK(frame.Offset == ipos32 {15, 14});
        CHECK(frame.Data == vector<uint8_t> {255, 0, 0, 255, 191, 0, 64, 255, 64, 0, 191, 255, 0, 0, 255, 255, 255, 0, 0, 255, 191, 0, 64, 255, 64, 0, 191, 255, 0, 0, 255, 255});
        const auto& info_data = rig.Outputs.at("SpriteInfo/TestPack.foinfo");
        auto info = ReadSpriteInfoFile("SpriteInfo/TestPack.foinfo", string(info_data.begin(), info_data.end()));
        auto entry = std::find_if(info.begin(), info.end(), [](const auto& value) { return value.SourcePath == "gfx/size.fofrm"; });
        REQUIRE(entry != info.end());
        CHECK(entry->Info.Directions.front().Frames.front().Size == isize32 {4, 2});
    }

    SECTION("BakesFofrmFrameSizesBeforeFlattening")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/parent.fofrm", "Frames = child.fofrm child.fofrm\nFrameSizes = \\\n 3 2 \\\n 1 4\nOffset = 3 4\nFrameOffsets = 5 6 -1 -2\n");
        rig.AddSourceFile("gfx/child.fofrm", "Frames = a.tga b.tga\nOffset = 99 88\nFrameOffsets = 2 3 -4 5\n");
        AddSourceBinaryFile(rig, "gfx/a.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
        AddSourceBinaryFile(rig, "gfx/b.tga", MakeRawTga(1, 1, 24, {0, 255, 0}));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/parent.fofrm");
        auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/parent.fofrm"));
        REQUIRE(sequence.Frames.size() == 4);
        CHECK(sequence.AnimTicks == 400);

        for (size_t index = 0; index < 4; index++) {
            CHECK(sequence.Frames[index].Width == (index < 2 ? 3 : 1));
            CHECK(sequence.Frames[index].Height == (index < 2 ? 2 : 4));
        }

        CHECK(sequence.Frames[0].Offset == ipos32 {10, 13});
        CHECK(sequence.Frames[1].Offset == ipos32 {4, 15});
        CHECK(sequence.Frames[2].Offset == ipos32 {4, 5});
        CHECK(sequence.Frames[3].Offset == ipos32 {-2, 7});
    }

    SECTION("InheritsRootSizeWithoutCarryingLocalSizes")
    {
        TestRig rig;
        string_view prefix = FO_GEOMETRY == 1 ? "HexDir" : "SquareDir";
        string descriptor = "Frame = source.tga\nSize = 4 3\n";

        for (int32_t dir = 1; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            descriptor += strex("[{} {}]\nFrame = source.tga\n", prefix, dir);

            if (dir == 1) {
                descriptor += "Size = 2 1\n";
            }
            else if (dir == 2) {
                descriptor += "FrameSizes = 3 2\n";
            }
        }

        rig.AddSourceFile("gfx/dirs.fofrm", descriptor);
        AddSourceBinaryFile(rig, "gfx/source.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/dirs.fofrm");
        auto sprite = ReadSpriteResource(rig.Outputs.at("gfx/dirs.fofrm"));
        REQUIRE(sprite.Directions.size() == GameSettings::MAP_DIR_COUNT);

        for (size_t dir = 0; dir < sprite.Directions.size(); dir++) {
            CHECK(sprite.Directions[dir].Frames.front().Size == (dir == 1 ? isize32 {2, 1} : dir == 2 ? isize32 {3, 2} : isize32 {4, 3}));
        }
    }

    SECTION("ResamplesPremultipliedAlphaWithoutTransparentColorBleed")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/alpha.fofrm", "Frame = source.tga\nFrameSizes = 1 1\n");
        AddSourceBinaryFile(rig, "gfx/source.tga", MakeRawTga(2, 2, 32, {255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 0, 0, 255, 255}));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/alpha.fofrm");
        CHECK(ReadSingleFrame(rig.Outputs.at("gfx/alpha.fofrm")).Data == vector<uint8_t> {255, 0, 0, 64});
    }

    SECTION("PreservesIdentityResizeBytesIncludingTransparentRgb")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/plain.fofrm", "Frame = source.tga\n");
        rig.AddSourceFile("gfx/identity.fofrm", "Frame = source.tga\nSize = 2 1\n");
        AddSourceBinaryFile(rig, "gfx/source.tga", MakeRawTga(2, 1, 32, {255, 0, 0, 0, 0, 0, 255, 255}));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/plain.fofrm");
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/identity.fofrm");
        CHECK(rig.Outputs.at("gfx/plain.fofrm") == rig.Outputs.at("gfx/identity.fofrm"));
    }

    SECTION("ResizesEmptyFramesAndPreservesImportedMotion")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/empty.fofrm", "Frame = source.toy\nSize = 3 2\n");
        rig.AddSourceFile("gfx/source.toy", "payload");
        ImageBaker baker {rig.MakeContext()};
        baker.AddLoader(
            [](string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) {
                ignore_unused(fname, opt, reader, context);
                ImageImport::FrameCollection collection;
                [[maybe_unused]] uint16_t sequence_size {};
                uint32_t duration_ms {100};
                bool have_dirs {};
                sequence_size = 1;
                collection.Main.Frames.emplace_back(ImageImport::FrameShot {.NextX = 7, .NextY = -9, .Offset = {3, -4}});

                if (!have_dirs) {
                    for (auto& direction : collection.Dirs) {
                        direction.Frames.clear();
                    }
                }
                else {
                    for (int32_t direction = GameSettings::MAP_DIR_COUNT - 1; direction < ImageImport::MAX_DIRS_MINUS_ONE; direction++) {
                        collection.Dirs[direction].Frames.clear();
                    }
                }

                for (int32_t direction = 0; direction < (have_dirs ? GameSettings::MAP_DIR_COUNT : 1); direction++) {
                    auto& frames = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

                    for (size_t index = 0; index < frames.Frames.size(); index++) {
                        auto& shot = frames.Frames[index];

                        if (shot.Shared && shot.SharedIndex < index) {
                            uint16_t reference = shot.SharedIndex;
                            shot = frames.Frames[reference];
                            shot.Data.clear();
                            shot.Shared = true;
                            shot.SharedIndex = reference;
                        }
                    }

                    ImageImport::SetFrameSequenceDuration(frames, duration_ms);
                }

                return collection;
            },
            {"toy"});
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/empty.fofrm");
        auto frame = ReadSingleFrame(rig.Outputs.at("gfx/empty.fofrm"));
        CHECK(frame.Width == 3);
        CHECK(frame.Height == 2);
        CHECK(frame.Data == vector<uint8_t>(24));
        CHECK(frame.NextX == 7);
        CHECK(frame.NextY == -9);
        CHECK(frame.Offset == ipos32 {3, -4});
    }

    SECTION("RejectsInvalidFofrmSizes")
    {
        for (string_view value : {"", "1", "0 1", "1 0", "-1 2", "2 -1", "65536 1", "1 65536", "2147483648 2", "1.5 2", "1 2 3", "a 2"}) {
            INFO(value);
            TestRig rig;
            rig.AddSourceFile("gfx/bad.fofrm", strex("Frame = source.tga\nSize = {}\n", value).str());
            AddSourceBinaryFile(rig, "gfx/source.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/bad.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }

        for (string_view suffix : {"FrameSizes =\n", "FrameSizes = 1 2\n", "FrameSizes = 1 2 3\n", "FrameSizes = 1 2 3 0\n", "Size = 1 2\nFrameSizes = 1 2 3 4\n"}) {
            INFO(suffix);
            TestRig rig;
            rig.AddSourceFile("gfx/bad.fofrm", strex("Frames = source.tga source.tga\n{}", suffix).str());
            AddSourceBinaryFile(rig, "gfx/source.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/bad.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }

    SECTION("AcceptsQuotedIntegerSizeTokens")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/quoted-size.fofrm", "Frame = source.tga\nSize = \"2\" \"3\"\n");
        AddSourceBinaryFile(rig, "gfx/source.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/quoted-size.fofrm");
        auto frame = ReadSingleFrame(rig.Outputs.at("gfx/quoted-size.fofrm"));
        CHECK(frame.Width == 2);
        CHECK(frame.Height == 3);
    }

    SECTION("RejectsEmbeddedWhitespaceAndEmptySizeTokens")
    {
        for (string_view value : {"\"1 2\" \"\"", "\"1 \" 2", "\"\" \"1 2\"", "\"1 2\" \"3 4\""}) {
            TestRig rig;
            rig.AddSourceFile("gfx/tokens.fofrm", strex("Frame = source.tga\nFrameSizes = {}\n", value).str());
            AddSourceBinaryFile(rig, "gfx/source.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/tokens.fofrm"), ImageBakerException);
        }
    }

    SECTION("RejectsUnfinishedSizeContinuationWithoutFinalNewline")
    {
        for (string_view key : {"Size", "FrameSizes"}) {
            TestRig rig;
            rig.AddSourceFile("gfx/unfinished.fofrm", strex("Frame = source.tga\n{} = \\", key).str());
            AddSourceBinaryFile(rig, "gfx/source.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/unfinished.fofrm"), ImageBakerException);
        }
    }

    SECTION("RejectsResizeListsWithoutReferencesAndLegacyAlias")
    {
        FO_TEMPORARY_COMPAT(FofrmWholeCollectionSource, "2026-12-31");

        for (string_view descriptor : {"FrameSizes = 1 2\n", "Alias = source.tga\nSize = 1 2\n", "Alias = source.tga\nFrameSizes = 1 2\n"}) {
            TestRig rig;
            rig.AddSourceFile("gfx/bad.fofrm", descriptor);
            AddSourceBinaryFile(rig, "gfx/source.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
            ImageBaker baker {rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/bad.fofrm"), ImageBakerException);
            CHECK(rig.Outputs.empty());
        }
    }
}

TEST_CASE("DisabledImageImportersIdentifyTheirRequiredOptions")
{
#if !FO_IMAGE_FRM
    CHECK_THROWS_WITH(ImageImport::CheckEnabled("gfx/image.frm"), Catch::Matchers::ContainsSubstring("FO_IMAGE_FRM"));
#endif
#if !FO_IMAGE_RIX
    CHECK_THROWS_WITH(ImageImport::CheckEnabled("gfx/image.rix"), Catch::Matchers::ContainsSubstring("FO_IMAGE_RIX"));
#endif
#if !FO_IMAGE_ART
    CHECK_THROWS_WITH(ImageImport::CheckEnabled("gfx/image.art"), Catch::Matchers::ContainsSubstring("FO_IMAGE_ART"));

    BakerTests::TestRig alias;
    alias.AddSourceFile("gfx/alias.fofrm", "Source=image$r=3.art\n");
    alias.AddSourceFile("gfx/image.art", "payload");
    ImageBaker alias_baker {alias.MakeContext()};
    CHECK_THROWS_WITH(alias_baker.BakeFiles(alias.GetAllSourceFiles(), "gfx/alias.fofrm"), Catch::Matchers::ContainsSubstring("FO_IMAGE_ART"));
    CHECK(alias.Outputs.empty());
#endif
#if !FO_IMAGE_SPR
    CHECK_THROWS_WITH(ImageImport::CheckEnabled("gfx/image.spr"), Catch::Matchers::ContainsSubstring("FO_IMAGE_SPR"));
#endif
#if !FO_IMAGE_ZAR
    CHECK_THROWS_WITH(ImageImport::CheckEnabled("gfx/image.zar"), Catch::Matchers::ContainsSubstring("FO_IMAGE_ZAR"));
#endif
#if !FO_IMAGE_TIL
    CHECK_THROWS_WITH(ImageImport::CheckEnabled("gfx/image.til"), Catch::Matchers::ContainsSubstring("FO_IMAGE_TIL"));
#endif
#if !FO_IMAGE_MOS
    CHECK_THROWS_WITH(ImageImport::CheckEnabled("gfx/image.mos"), Catch::Matchers::ContainsSubstring("FO_IMAGE_MOS"));
#endif
#if !FO_IMAGE_BAM
    CHECK_THROWS_WITH(ImageImport::CheckEnabled("gfx/image.bam"), Catch::Matchers::ContainsSubstring("FO_IMAGE_BAM"));
#endif
    CHECK_NOTHROW(ImageImport::CheckEnabled("gfx/image.png"));
    CHECK_NOTHROW(ImageImport::CheckEnabled("gfx/image.tga"));
    CHECK_NOTHROW(ImageImport::CheckEnabled("gfx/image.fofrm"));

#if !FO_IMAGE_FRM
    BakerTests::TestRig nested;
    nested.AddSourceFile("gfx/nested.fofrm", "Frame = image.frm\n");
    nested.AddSourceFile("gfx/image.frm", "payload");
    ImageBaker baker {nested.MakeContext()};
    CHECK_THROWS_WITH(baker.BakeFiles(nested.GetAllSourceFiles(), "gfx/nested.fofrm"), Catch::Matchers::ContainsSubstring("FO_IMAGE_FRM"));
    CHECK(nested.Outputs.empty());
#endif
}

TEST_CASE("FofrmSourceCollection")
{
    BakerTests::TestRig rig;
    rig.AddSourceFile("gfx/source.toy", "collection");
    string descriptor = "Source = source.toy\n";
    bool transformed = false;
    bool local_override = false;

    SECTION("SourcePreservesAnchorsAndTiming")
    {
    }

    SECTION("SourceTransformPreservesAllDirections")
    {
        transformed = true;
        descriptor += "Size = 2 3\nOffset = 7 8\n";
    }

    SECTION("NamedSourceDirectionOverridesNeedNoRepeatedReferences")
    {
        transformed = true;
        local_override = true;
        descriptor += strex("Size = 2 3\nOffset = 7 8\n[{} 1]\nSize = 4 5\n", FO_GEOMETRY == 1 ? "HexDir" : "SquareDir").str();
    }

    rig.AddSourceFile("gfx/root.fofrm", descriptor);
    ImageBaker baker {rig.MakeContext()};
    baker.AddLoader(
        [](string_view, string_view, FileReader, const ImageImport::Context&) {
            ImageImport::FrameCollection collection;
            [[maybe_unused]] uint16_t sequence_size {};
            uint32_t duration_ms {100};
            bool have_dirs {};
            sequence_size = 2;
            duration_ms = 200;
            have_dirs = true;

            for (int32_t direction = 0; direction < GameSettings::MAP_DIR_COUNT; direction++) {
                auto& sequence = direction == 0 ? collection.Main : collection.Dirs[direction - 1];
                sequence.OffsX = numeric_cast<int16_t>(10 + direction);
                sequence.OffsY = numeric_cast<int16_t>(20 + direction);
                sequence.Frames = {
                    ImageImport::FrameShot {.Width = 1, .Height = 1, .NextX = 1, .NextY = -1, .Offset = {3, 4}, .Data = {40, 50, 60, 255}},
                    ImageImport::FrameShot {.Shared = true, .SharedIndex = 0},
                };
            }

            if (!have_dirs) {
                for (auto& direction : collection.Dirs) {
                    direction.Frames.clear();
                }
            }
            else {
                for (int32_t direction = GameSettings::MAP_DIR_COUNT - 1; direction < ImageImport::MAX_DIRS_MINUS_ONE; direction++) {
                    collection.Dirs[direction].Frames.clear();
                }
            }

            for (int32_t direction = 0; direction < (have_dirs ? GameSettings::MAP_DIR_COUNT : 1); direction++) {
                auto& frames = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

                for (size_t index = 0; index < frames.Frames.size(); index++) {
                    auto& shot = frames.Frames[index];

                    if (shot.Shared && shot.SharedIndex < index) {
                        uint16_t reference = shot.SharedIndex;
                        shot = frames.Frames[reference];
                        shot.Data.clear();
                        shot.Shared = true;
                        shot.SharedIndex = reference;
                    }
                }

                ImageImport::SetFrameSequenceDuration(frames, duration_ms);
            }

            return collection;
        },
        {"toy"});
    baker.BakeFiles(rig.GetAllSourceFiles(), "");
    auto resource = ReadSpriteResource(rig.Outputs.at("gfx/root.fofrm"));
    REQUIRE(resource.Directions.size() == GameSettings::MAP_DIR_COUNT);
    REQUIRE(resource.Animation.Sprite.has_value());
    CHECK(GetDurationMs(resource.Animation.Sprite->Directions[0]) == 200);

    for (size_t direction = 0; direction < resource.Directions.size(); direction++) {
        REQUIRE(resource.Directions[direction].Frames.size() == 2);
        const auto& frame = resource.Directions[direction].Frames[0];
        isize32 size = local_override && direction == 1 ? isize32 {4, 5} : transformed ? isize32 {2, 3} : isize32 {1, 1};
        CHECK(frame.Size == size);
        CHECK(frame.Offset == ipos32 {numeric_cast<int32_t>(13 + direction + (transformed ? 7 : 0)), numeric_cast<int32_t>(24 + direction + (transformed ? 8 : 0))});
        CHECK(frame.NextOffset == ipos32 {1, -1});
        CHECK(frame.Pixels[0] == ucolor {40, 50, 60, 255});
    }
}

TEST_CASE("FofrmFlattenedDefaultTimingUsesFinalFrames")
{
    BakerTests::TestRig rig;
    rig.AddSourceFile("source.toy", "two frames");
    rig.AddSourceFile("root.fofrm", "Frame = source.toy\n");
    ImageBaker baker {rig.MakeContext()};
    baker.AddLoader(
        [](string_view, string_view, FileReader, const ImageImport::Context&) {
            ImageImport::FrameCollection collection;
            [[maybe_unused]] uint16_t sequence_size {};
            uint32_t duration_ms {100};
            bool have_dirs {};
            sequence_size = 2;
            duration_ms = 240;
            collection.Main.OffsX = 100;
            collection.Main.OffsY = 200;
            collection.Main.Frames = {
                ImageImport::FrameShot {.Width = 1, .Height = 1, .Offset = {3, 4}, .Data = {40, 50, 60, 255}},
                ImageImport::FrameShot {.Width = 1, .Height = 1, .Data = {70, 80, 90, 255}},
            };

            if (!have_dirs) {
                for (auto& direction : collection.Dirs) {
                    direction.Frames.clear();
                }
            }
            else {
                for (int32_t direction = GameSettings::MAP_DIR_COUNT - 1; direction < ImageImport::MAX_DIRS_MINUS_ONE; direction++) {
                    collection.Dirs[direction].Frames.clear();
                }
            }

            for (int32_t direction = 0; direction < (have_dirs ? GameSettings::MAP_DIR_COUNT : 1); direction++) {
                auto& frames = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

                for (size_t index = 0; index < frames.Frames.size(); index++) {
                    auto& shot = frames.Frames[index];

                    if (shot.Shared && shot.SharedIndex < index) {
                        uint16_t reference = shot.SharedIndex;
                        shot = frames.Frames[reference];
                        shot.Data.clear();
                        shot.Shared = true;
                        shot.SharedIndex = reference;
                    }
                }

                ImageImport::SetFrameSequenceDuration(frames, duration_ms);
            }

            return collection;
        },
        {"toy"});
    baker.BakeFiles(rig.GetAllSourceFiles(), "");
    auto resource = ReadSpriteResource(rig.Outputs.at("root.fofrm"));
    REQUIRE(resource.Animation.Sprite.has_value());
    CHECK(GetDurationMs(resource.Animation.Sprite->Directions[0]) == 200);
    CHECK(resource.Directions[0].Frames[0].Offset == ipos32 {3, 4});
}

#if FO_IMAGE_FRM
TEST_CASE("FrmPaletteTimelineIsLocalToItsDirection")
{
    BakerTests::TestRig rig;
    vector<FrmFrameSpec> animated {FrmFrameSpec {.Width = 1, .Height = 1, .Indices = {229}}};
    vector<FrmFrameSpec> still {FrmFrameSpec {.Width = 1, .Height = 1, .Indices = {1}}};
    auto data = MakeFrm(10, animated);

    for (int32_t direction = 1; direction < 6; direction++) {
        SetBe32(data, 0x22 + numeric_cast<size_t>(direction) * 4, numeric_cast<uint32_t>(data.size() - 0x3E));
        AppendFrmFrames(data, still);
    }

    AddSourceBinaryFile(rig, "Source.frm", data);
    rig.AddSourceFile("Root.fofrm", "Source = Source.frm\n");
    ImageBaker baker {rig.MakeContext()};
    baker.BakeFiles(rig.GetAllSourceFiles(), "Root.fofrm");
    auto resource = ReadSpriteResource(rig.Outputs.at("Root.fofrm"));
    REQUIRE(resource.Directions.size() == GameSettings::MAP_DIR_COUNT);
    CHECK(resource.Directions[0].Frames.size() == 4);
    CHECK(GetDurationMs(resource.Animation.Sprite->Directions[0]) == 800);

    for (size_t direction = 1; direction < resource.Directions.size(); direction++) {
        CHECK(resource.Directions[direction].Frames.size() == 1);
        CHECK(GetDurationMs(resource.Animation.Sprite->Directions[direction]) == 100);
    }
}
#endif

TEST_CASE("FofrmDataSurvivesMeshOptimization")
{
    string usage = GENERATE(string {"Data"}, string {"Normal"});
    BakerTests::TestRig rig;
    ConfigureSpriteMesh(rig);
    rig.AddSourceFile("Source.toy", "data channels");
    rig.AddSourceFile("Root.fofrm", strex("Frame = Source.toy\nPixelUsage = {}\n", usage).str());
    ImageBaker baker {rig.MakeContext()};
    baker.AddLoader(
        [](string_view, string_view, FileReader, const ImageImport::Context&) {
            ImageImport::FrameCollection collection;
            auto& frame = collection.Main.Frames.emplace_back();
            frame.Width = 64;
            frame.Height = 64;
            frame.DurationMs = 100;

            for (int32_t y = 0; y < 64; y++) {
                for (int32_t x = 0; x < 64; x++) {
                    frame.Data.insert(frame.Data.end(), {10, 20, 30, numeric_cast<uint8_t>(x >= 28 && x < 36 && y >= 28 && y < 36 ? 255 : 0)});
                }
            }

            return collection;
        },
        {"toy"});
    baker.BakeFiles(rig.GetAllSourceFiles(), "Root.fofrm");
    auto resource = ReadSpriteResource(rig.Outputs.at("Root.fofrm"));
    auto image = ExtractSpriteResourceFrameImage(resource.Directions[0].Frames[0]);
    CHECK(image.Size == isize32 {64, 64});
    CHECK(image.Pixels[0] == ucolor {10, 20, 30, 0});
    CHECK(image.Pixels[30 * 64 + 30] == ucolor {10, 20, 30, 255});
}

FO_END_NAMESPACE
