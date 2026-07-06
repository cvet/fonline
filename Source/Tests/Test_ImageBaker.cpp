//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "DataSerialization.h"
#include "ImageBaker.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

struct BakedImageFrame
{
    uint16_t Width {};
    uint16_t Height {};
    int16_t NextX {};
    int16_t NextY {};
    vector<uint8_t> Data {};
};

struct BakedImageSequence
{
    uint16_t SequenceSize {};
    uint16_t AnimTicks {};
    int16_t OffsX {};
    int16_t OffsY {};
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

[[nodiscard]] static auto MakeFrmWithDirTable(uint16_t fps, const vector<FrmFrameSpec>& frames, int32_t dirs_to_fill) -> vector<uint8_t>
{
    auto data = MakeFrm(fps, frames, 1, -1);

    for (int32_t dir = 1; dir < dirs_to_fill; dir++) {
        const size_t dir_pos = numeric_cast<size_t>(dir) * 2;
        const size_t data_offset = data.size() - 0x3E;

        SetBeInt16(data, 0xA + dir_pos, numeric_cast<int16_t>(10 + dir));
        SetBeInt16(data, 0x16 + dir_pos, numeric_cast<int16_t>(-10 - dir));
        SetBe32(data, 0x22 + numeric_cast<size_t>(dir) * 4, numeric_cast<uint32_t>(data_offset));
        AppendFrmFrames(data, frames);
    }

    return data;
}

[[nodiscard]] static auto MakeFrxDir(uint16_t fps, uint8_t dir, const vector<FrmFrameSpec>& frames, int16_t offs_x, int16_t offs_y) -> vector<uint8_t>
{
    auto data = MakeFrm(fps, frames);
    const size_t dir_pos = numeric_cast<size_t>(dir) * 2;

    SetBeInt16(data, 0xA + dir_pos, offs_x);
    SetBeInt16(data, 0x16 + dir_pos, offs_y);
    SetBe32(data, 0x22 + numeric_cast<size_t>(dir) * 4, 0);

    return data;
}

[[nodiscard]] static auto MakeFrmPalette() -> vector<uint8_t>
{
    vector<uint8_t> palette(256 * 3);

    for (uint8_t index = 1; index <= GameSettings::MAP_DIR_COUNT; index++) {
        const uint8_t color_base = numeric_cast<uint8_t>((index - 1) * 3 + 1);
        const size_t palette_pos = numeric_cast<size_t>(index) * 3;
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

[[nodiscard]] static auto MakeRix() -> vector<uint8_t>
{
    vector<uint8_t> data;
    data.resize(0xA + 256 * 3 + 1);
    data[4] = 1;
    data[6] = 1;

    const size_t palette_index = 0xA + 1 * 3;
    data[palette_index + 0] = 1;
    data[palette_index + 1] = 2;
    data[palette_index + 2] = 3;
    data[0xA + 256 * 3] = 1;

    return data;
}

[[nodiscard]] static auto MakeSimpleZar() -> vector<uint8_t>
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

[[nodiscard]] static auto MakeZarWithPaletteCount(uint32_t palette_count) -> vector<uint8_t>
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

[[nodiscard]] static auto MakePaletteZar() -> vector<uint8_t>
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

    const vector<uint8_t> rle = {0x05, 1, 0x06, 1, 0x7F, 0x07, 0x40};
    AppendLe32(data, numeric_cast<uint32_t>(rle.size()));
    data.insert(data.end(), rle.begin(), rle.end());
    return data;
}

[[nodiscard]] static auto MakeSimpleMos() -> vector<uint8_t>
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

[[nodiscard]] static auto MakeSimpleBam() -> vector<uint8_t>
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

[[nodiscard]] static auto MakeMultiFrameBam() -> vector<uint8_t>
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

[[nodiscard]] static auto MakeSimpleTil() -> vector<uint8_t>
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

    const vector<uint8_t> rle = {0x05, 1, 0x06, 1, 0x7F, 0x07, 0x40};
    AppendLe32(data, numeric_cast<uint32_t>(rle.size()));
    data.insert(data.end(), rle.begin(), rle.end());
    return data;
}

[[nodiscard]] static auto MakeTilWithoutTiledata() -> vector<uint8_t>
{
    vector<uint8_t> data;
    AppendTilPrefix(data);
    return data;
}

[[nodiscard]] static auto MakeTilWithBadZarHeader() -> vector<uint8_t>
{
    vector<uint8_t> data;
    AppendTilPrefix(data);
    data.insert(data.end(), {'<', 't', 'i', 'l', 'e', 'd', 'a', 't', 'a', '>'});
    data.insert(data.end(), 3, 0);
    AppendLe32(data, 1);
    data.insert(data.end(), {'b', 'a', 'd', 'z', 'a', 'r'});
    return data;
}

[[nodiscard]] static auto MakeTilWithPaletteCount(uint32_t palette_count) -> vector<uint8_t>
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

static void AppendArtFrameInfo(vector<uint8_t>& data, int32_t width, int32_t height, int32_t frame_size, int32_t offset_x, int32_t offset_y)
{
    AppendLeInt32(data, width);
    AppendLeInt32(data, height);
    AppendLeInt32(data, frame_size);
    AppendLeInt32(data, offset_x);
    AppendLeInt32(data, offset_y);
    AppendLeInt32(data, 0);
    AppendLeInt32(data, 0);
}

[[nodiscard]] static auto MakeArtWithFrameIndices(int32_t flags, const vector<uint8_t>& indices) -> vector<uint8_t>
{
    vector<uint8_t> data;
    AppendLeInt32(data, flags);
    AppendLeInt32(data, 10); // FrameRate
    AppendLeInt32(data, 1); // RotationCount
    AppendLe32(data, 1);
    AppendLe32(data, 1);
    AppendLe32(data, 0);
    AppendLe32(data, 0);
    AppendLeInt32(data, 0); // ActionFrame
    AppendLeInt32(data, 1); // FrameCount

    for (int32_t i = 0; i < 24; i++) {
        AppendLeInt32(data, 0);
    }

    for (uint32_t i = 0; i < 256; i++) {
        AppendLe32(data, 0);
    }

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t color = 0;

        if (i == 1) {
            color = 0x001E140A;
        }
        else if (i == 2) {
            color = 0x00463228;
        }

        AppendLe32(data, color);
    }

    AppendArtFrameInfo(data, 0, 0, 0, 0, 0);
    AppendArtFrameInfo(data, numeric_cast<int32_t>(indices.size()), 1, numeric_cast<int32_t>(indices.size()), 5, 7);
    data.insert(data.end(), indices.begin(), indices.end());
    return data;
}

[[nodiscard]] static auto MakeSimpleArt() -> vector<uint8_t>
{
    return MakeArtWithFrameIndices(0, {1, 2});
}

[[nodiscard]] static auto MakeRleFrameRangeArt() -> vector<uint8_t>
{
    vector<uint8_t> data;
    AppendLeInt32(data, 0); // Flags
    AppendLeInt32(data, 10); // FrameRate
    AppendLeInt32(data, 1); // RotationCount
    AppendLe32(data, 1);
    AppendLe32(data, 0);
    AppendLe32(data, 0);
    AppendLe32(data, 0);
    AppendLeInt32(data, 0); // ActionFrame
    AppendLeInt32(data, 2); // FrameCount

    for (int32_t i = 0; i < 24; i++) {
        AppendLeInt32(data, 0);
    }

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t color = 0;

        if (i == 1) {
            color = 0x00332211;
        }
        else if (i == 2) {
            color = 0x00665544;
        }

        AppendLe32(data, color);
    }

    AppendArtFrameInfo(data, 0, 0, 0, 0, 0);
    AppendArtFrameInfo(data, 0, 0, 0, 0, 0);
    AppendArtFrameInfo(data, 0, 0, 0, 0, 0);
    AppendArtFrameInfo(data, 3, 1, 4, 1, 2);
    data.emplace_back(2);
    data.emplace_back(1);
    data.emplace_back(129);
    data.emplace_back(2);
    return data;
}

[[nodiscard]] static auto MakeDirectionalEmptyArt() -> vector<uint8_t>
{
    vector<uint8_t> data;
    AppendLeInt32(data, 0); // Flags
    AppendLeInt32(data, 5); // FrameRate
    AppendLeInt32(data, 8); // RotationCount
    AppendLe32(data, 1);
    AppendLe32(data, 0);
    AppendLe32(data, 0);
    AppendLe32(data, 0);
    AppendLeInt32(data, 0); // ActionFrame
    AppendLeInt32(data, 2); // FrameCount

    for (int32_t i = 0; i < 24; i++) {
        AppendLeInt32(data, 0);
    }

    for (uint32_t i = 0; i < 256; i++) {
        AppendLe32(data, 0);
    }

    for (int32_t dir = 0; dir < 8; dir++) {
        for (int32_t frame = 0; frame < 2; frame++) {
            if (dir == 1 && frame == 1) {
                AppendArtFrameInfo(data, 0, 0, 0, 7, 11);
            }
            else {
                AppendArtFrameInfo(data, 0, 0, 0, 0, 0);
            }
        }
    }

    return data;
}

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

    for (const int16_t frame : sequence_frames) {
        AppendLeInt16(data, frame);
    }

    for (size_t i = 0; i < sequence_frames.size(); i++) {
        AppendLeInt32(data, 0);
    }

    AppendLeInt32(data, 4);
    data.insert(data.end(), {'w', 'a', 'l', 'k'});
    AppendLe16(data, 0); // Animation index

    data.insert(data.end(), {'<', 's', 'p', 'r', 'a', 'n', 'i', 'm', '>', '\0', '\0', '\0'});
    const size_t file_offset_pos = data.size();
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

static void ReplaceFirstTagByte(vector<uint8_t>& data, string_view tag, uint8_t replacement)
{
    const auto it = std::search(data.begin(), data.end(), tag.begin(), tag.end(), [](uint8_t lhs, char rhs) { return lhs == numeric_cast<uint8_t>(rhs); });
    FO_VERIFY_AND_THROW(it != data.end(), "Test tag not found", tag);
    *it = replacement;
}

[[nodiscard]] static auto MakeSimpleSpr() -> vector<uint8_t>
{
    return MakeSprWithSequenceFrames({0});
}

static void AddSourceBinaryFile(BakerTests::TestRig& rig, string_view path, const vector<uint8_t>& data, uint64_t write_time = 1)
{
    string bytes;
    bytes.resize(data.size());

    if (!data.empty()) {
        MemCopy(bytes.data(), data.data(), data.size());
    }

    rig.AddSourceFile(path, bytes, write_time);
}

[[nodiscard]] static auto ReadSingleFrame(const vector<uint8_t>& baked_data) -> BakedImageFrame
{
    DataReader reader {baked_data};

    CHECK(reader.Read<uint8_t>() == 42);
    CHECK(reader.Read<uint16_t>() == 1);
    CHECK(reader.Read<uint16_t>() == 100);
    CHECK(reader.Read<uint8_t>() == 1);
    CHECK(reader.Read<int16_t>() == 0);
    CHECK(reader.Read<int16_t>() == 0);
    CHECK_FALSE(reader.Read<bool>());

    BakedImageFrame frame;
    frame.Width = reader.Read<uint16_t>();
    frame.Height = reader.Read<uint16_t>();
    frame.NextX = reader.Read<int16_t>();
    frame.NextY = reader.Read<int16_t>();

    const size_t data_size = numeric_cast<size_t>(frame.Width) * frame.Height * 4;
    const auto frame_data = reader.ReadBytes(data_size);
    frame.Data.assign(frame_data.begin(), frame_data.end());

    CHECK(reader.Read<uint8_t>() == 42);
    CHECK_NOTHROW(reader.VerifyEnd());

    return frame;
}

[[nodiscard]] static auto ReadSingleDirSequence(const vector<uint8_t>& baked_data) -> BakedImageSequence
{
    DataReader reader {baked_data};

    CHECK(reader.Read<uint8_t>() == 42);

    BakedImageSequence sequence;
    sequence.SequenceSize = reader.Read<uint16_t>();
    sequence.AnimTicks = reader.Read<uint16_t>();
    CHECK(reader.Read<uint8_t>() == 1);
    sequence.OffsX = reader.Read<int16_t>();
    sequence.OffsY = reader.Read<int16_t>();
    sequence.Frames.reserve(sequence.SequenceSize);

    for (uint16_t i = 0; i < sequence.SequenceSize; i++) {
        CHECK_FALSE(reader.Read<bool>());

        auto& frame = sequence.Frames.emplace_back();
        frame.Width = reader.Read<uint16_t>();
        frame.Height = reader.Read<uint16_t>();
        frame.NextX = reader.Read<int16_t>();
        frame.NextY = reader.Read<int16_t>();

        const size_t data_size = numeric_cast<size_t>(frame.Width) * frame.Height * 4;
        const_span<uint8_t> frame_data = reader.ReadBytes(data_size);
        frame.Data.assign(frame_data.begin(), frame_data.end());
    }

    CHECK(reader.Read<uint8_t>() == 42);
    CHECK_NOTHROW(reader.VerifyEnd());

    return sequence;
}

TEST_CASE("ImageBaker")
{
    using namespace BakerTests;

    SECTION("Setup")
    {
        TestRig rig;
        const auto bakers = MakeRequestedBakers({string(ImageBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == ImageBaker::NAME);
        CHECK(bakers.front()->GetOrder() == 4);
        CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
    }

    SECTION("BakesRawTgaTarget")
    {
        TestRig rig;
        rig.AddSourceFile("ignored.txt", "not an image");
        AddSourceBinaryFile(rig, "ui/icon.tga", MakeRawTga(2, 1, 32, {0, 0, 255, 128, 0, 255, 0, 255}), 10);

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "ui/icon.tga");

        REQUIRE(rig.Outputs.contains("ui/icon.tga"));
        const auto frame = ReadSingleFrame(rig.Outputs.at("ui/icon.tga"));

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
        const auto frame = ReadSingleFrame(rig.Outputs.at("gfx/rle.tga"));

        CHECK(frame.Width == 2);
        CHECK(frame.Height == 1);
        CHECK(frame.Data == vector<uint8_t> {0, 0, 255, 255, 0, 0, 255, 255});

        REQUIRE(rig.Outputs.contains("gfx/rle-raw-packets.tga"));
        const auto raw_packet_frame = ReadSingleFrame(rig.Outputs.at("gfx/rle-raw-packets.tga"));

        CHECK(raw_packet_frame.Width == 2);
        CHECK(raw_packet_frame.Height == 1);
        CHECK(raw_packet_frame.Data == vector<uint8_t> {0x10, 0x20, 0x30, 0xFF, 0x40, 0x50, 0x60, 0xFF});
    }

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
        baker.BakeFiles(rig.GetAllSourceFiles(), "art/critters/RAT.frm");

        CHECK_FALSE(rig.Outputs.contains("art/critters/RAT.frm"));
        REQUIRE(rig.Outputs.contains("art/critters/rat.frm"));
        const auto sequence = ReadSingleDirSequence(rig.Outputs.at("art/critters/rat.frm"));

        CHECK(sequence.SequenceSize == 2);
        CHECK(sequence.AnimTicks == 400);
        CHECK(sequence.OffsX == 7);
        CHECK(sequence.OffsY == -8);
        REQUIRE(sequence.Frames.size() == 2);
        CHECK(sequence.Frames[0].NextX == 2);
        CHECK(sequence.Frames[0].NextY == -3);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {4, 8, 12, 255});
        CHECK(sequence.Frames[1].NextX == -4);
        CHECK(sequence.Frames[1].NextY == 5);
        CHECK(sequence.Frames[1].Data == vector<uint8_t> {16, 20, 24, 255});
    }

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
        const auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/animated.frm"));

        CHECK(sequence.SequenceSize == 60);
        CHECK(sequence.AnimTicks == 6000);
        REQUIRE(sequence.Frames.size() == 60);
        CHECK(sequence.Frames.front().Width == 6);
        CHECK(sequence.Frames.front().Height == 1);
        CHECK(sequence.Frames.front().Data.size() == 24);
        CHECK(sequence.Frames[1].Data.size() == 24);
        CHECK(sequence.Frames.front().Data != sequence.Frames[1].Data);
    }

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
        baker.BakeFiles(rig.GetAllSourceFiles(), "art/critters/RAT.fr0");

        CHECK_FALSE(rig.Outputs.contains("art/critters/RAT.fr0"));
        REQUIRE(rig.Outputs.contains("art/critters/rat.fofrm"));

        DataReader reader {rig.Outputs.at("art/critters/rat.fofrm")};
        CHECK(reader.Read<uint8_t>() == 42);
        CHECK(reader.Read<uint16_t>() == 1);
        CHECK(reader.Read<uint16_t>() == 200);
        CHECK(reader.Read<uint8_t>() == GameSettings::MAP_DIR_COUNT);

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            CHECK(reader.Read<int16_t>() == 10 + dir);
            CHECK(reader.Read<int16_t>() == -20 - dir);
            CHECK_FALSE(reader.Read<bool>());
            CHECK(reader.Read<uint16_t>() == 1);
            CHECK(reader.Read<uint16_t>() == 1);
            CHECK(reader.Read<int16_t>() == dir + 1);
            CHECK(reader.Read<int16_t>() == -dir - 1);

            const_span<uint8_t> data = reader.ReadBytes(4);
            const uint8_t color_base = numeric_cast<uint8_t>(dir * 3 + 1);
            const vector<uint8_t> expected_data {
                numeric_cast<uint8_t>(color_base * 4),
                numeric_cast<uint8_t>((color_base + 1) * 4),
                numeric_cast<uint8_t>((color_base + 2) * 4),
                255,
            };
            CHECK(vector<uint8_t>(data.begin(), data.end()) == expected_data);
        }

        CHECK(reader.Read<uint8_t>() == 42);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("BakesFrmEmbeddedDirectionsAndReportsTruncatedDirTable")
    {
        const vector<FrmFrameSpec> frames {
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

        DataReader reader {dirs.Outputs.at("gfx/directional.frm")};
        CHECK(reader.Read<uint8_t>() == 42);
        CHECK(reader.Read<uint16_t>() == 1);
        CHECK(reader.Read<uint16_t>() == 200);
        CHECK(reader.Read<uint8_t>() == GameSettings::MAP_DIR_COUNT);

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            if (dir == 0) {
                CHECK(reader.Read<int16_t>() == 1);
                CHECK(reader.Read<int16_t>() == -1);
            }
            else {
                CHECK(reader.Read<int16_t>() == 10 + dir);
                CHECK(reader.Read<int16_t>() == -10 - dir);
            }

            CHECK_FALSE(reader.Read<bool>());
            CHECK(reader.Read<uint16_t>() == 1);
            CHECK(reader.Read<uint16_t>() == 1);
            CHECK(reader.Read<int16_t>() == 2);
            CHECK(reader.Read<int16_t>() == 3);
            CHECK(reader.ReadBytes(4).size() == 4);
        }

        CHECK(reader.Read<uint8_t>() == 42);
        CHECK_NOTHROW(reader.VerifyEnd());

        TestRig truncated;
        AddSourceBinaryFile(truncated, "gfx/truncated-dirs.frm", MakeFrmWithDirTable(5, frames, 2));

        ImageBaker truncated_baker {truncated.MakeContext()};
        CHECK_THROWS_AS(truncated_baker.BakeFiles(truncated.GetAllSourceFiles(), "gfx/truncated-dirs.frm"), ImageBakerException);
        CHECK(truncated.Outputs.empty());
    }

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
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/animated.fr0");

        CHECK_FALSE(rig.Outputs.contains("gfx/animated.fr0"));
        REQUIRE(rig.Outputs.contains("gfx/animated.frm"));
        const auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/animated.frm"));

        CHECK(sequence.SequenceSize == 60);
        CHECK(sequence.AnimTicks == 6000);
        CHECK(sequence.OffsX == 3);
        CHECK(sequence.OffsY == -4);
        REQUIRE(sequence.Frames.size() == 60);
        CHECK(sequence.Frames.front().Width == 6);
        CHECK(sequence.Frames.front().Height == 1);
        CHECK(sequence.Frames.front().Data.size() == 24);
        CHECK(sequence.Frames[1].Data.size() == 24);
        CHECK(sequence.Frames.front().Data != sequence.Frames[1].Data);
    }

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

    SECTION("BakesPalettePngWithTransparency")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/indexed.png", MakePaletteTransparencyPng());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/indexed.png");

        REQUIRE(rig.Outputs.contains("gfx/indexed.png"));
        const auto frame = ReadSingleFrame(rig.Outputs.at("gfx/indexed.png"));

        CHECK(frame.Width == 2);
        CHECK(frame.Height == 1);
        CHECK(frame.Data == vector<uint8_t> {255, 0, 0, 0, 0, 255, 0, 255});
    }

    SECTION("BakesSimpleLegacyFormats")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "legacy/pixel.rix", MakeRix());
        AddSourceBinaryFile(rig, "legacy/blank.zar", MakeSimpleZar());
        AddSourceBinaryFile(rig, "legacy/palette.zar", MakePaletteZar());
        AddSourceBinaryFile(rig, "legacy/panel.mos", MakeSimpleMos());
        AddSourceBinaryFile(rig, "legacy/cycle.bam", MakeSimpleBam());
        AddSourceBinaryFile(rig, "legacy/floor.til", MakeSimpleTil());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.contains("legacy/pixel.rix"));
        const auto rix_frame = ReadSingleFrame(rig.Outputs.at("legacy/pixel.rix"));

        CHECK(rix_frame.Width == 1);
        CHECK(rix_frame.Height == 1);
        CHECK(rix_frame.Data == vector<uint8_t> {12, 8, 4, 255});

        REQUIRE(rig.Outputs.contains("legacy/blank.zar"));
        const auto zar_frame = ReadSingleFrame(rig.Outputs.at("legacy/blank.zar"));

        CHECK(zar_frame.Width == 1);
        CHECK(zar_frame.Height == 1);
        CHECK(zar_frame.Data == vector<uint8_t> {0, 0, 0, 0});

        REQUIRE(rig.Outputs.contains("legacy/palette.zar"));
        const auto palette_zar_frame = ReadSingleFrame(rig.Outputs.at("legacy/palette.zar"));

        CHECK(palette_zar_frame.Width == 3);
        CHECK(palette_zar_frame.Height == 1);
        CHECK(palette_zar_frame.Data == vector<uint8_t> {0x11, 0x22, 0x33, 0xFF, 0x11, 0x22, 0x33, 0x7F, 0x11, 0x22, 0x33, 0x40});

        REQUIRE(rig.Outputs.contains("legacy/panel.mos"));
        const auto mos_frame = ReadSingleFrame(rig.Outputs.at("legacy/panel.mos"));

        CHECK(mos_frame.Width == 2);
        CHECK(mos_frame.Height == 1);
        CHECK(mos_frame.Data == vector<uint8_t> {1, 2, 3, 255, 0, 0, 0, 0});

        REQUIRE(rig.Outputs.contains("legacy/cycle.bam"));
        const auto bam_frame = ReadSingleFrame(rig.Outputs.at("legacy/cycle.bam"));

        CHECK(bam_frame.Width == 4);
        CHECK(bam_frame.Height == 1);
        CHECK(bam_frame.NextX == 1);
        CHECK(bam_frame.NextY == -1);
        CHECK(bam_frame.Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF, 0, 0, 0, 0, 6, 5, 4, 0xFF, 6, 5, 4, 0xFF});

        REQUIRE(rig.Outputs.contains("legacy/floor.til"));
        const auto til_frame = ReadSingleFrame(rig.Outputs.at("legacy/floor.til"));

        CHECK(til_frame.Width == 3);
        CHECK(til_frame.Height == 1);
        CHECK(til_frame.Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF, 0x33, 0x22, 0x11, 0x7F, 0x33, 0x22, 0x11, 0x40});
    }

    SECTION("BakesArtFrameOptionsThroughFofrm")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/art.fofrm", "Frm=marker$1thv.art\n");
        AddSourceBinaryFile(rig, "gfx/marker.art", MakeSimpleArt());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/art.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/art.fofrm"));
        const auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/art.fofrm"));

        CHECK(sequence.SequenceSize == 1);
        CHECK(sequence.AnimTicks == 100);
        REQUIRE(sequence.Frames.size() == 1);
        CHECK(sequence.Frames[0].Width == 2);
        CHECK(sequence.Frames[0].Height == 1);
        CHECK(sequence.Frames[0].NextX == 4);
        CHECK(sequence.Frames[0].NextY == 6);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0x46, 0x32, 0x28, 0x46, 0x1E, 0x14, 0x0A, 0x1E});
    }

    SECTION("BakesStaticArtFlagZeroPixelsAndOptionEdgesThroughFofrm")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/static-art.fofrm", "Frm=static$1f0x.art\n");
        AddSourceBinaryFile(rig, "gfx/static.art", MakeArtWithFrameIndices(1, {0, 1}));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/static-art.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/static-art.fofrm"));
        const auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/static-art.fofrm"));

        CHECK(sequence.SequenceSize == 1);
        CHECK(sequence.AnimTicks == 100);
        REQUIRE(sequence.Frames.size() == 1);
        CHECK(sequence.Frames[0].Width == 2);
        CHECK(sequence.Frames[0].Height == 1);
        CHECK(sequence.Frames[0].NextX == -4);
        CHECK(sequence.Frames[0].NextY == -6);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0, 0, 0, 0, 0x1E, 0x14, 0x0A, 0xFF});
    }

    SECTION("BakesBamSpecificFrameOptionsThroughFofrm")
    {
        TestRig specific_frame;
        specific_frame.AddSourceFile("legacy/bam-specific.fofrm", "Frm=multi$9-1.bam\n");
        AddSourceBinaryFile(specific_frame, "legacy/multi.bam", MakeMultiFrameBam());

        ImageBaker specific_frame_baker {specific_frame.MakeContext()};
        specific_frame_baker.BakeFiles(specific_frame.GetAllSourceFiles(), "legacy/bam-specific.fofrm");

        REQUIRE(specific_frame.Outputs.contains("legacy/bam-specific.fofrm"));
        const auto selected = ReadSingleDirSequence(specific_frame.Outputs.at("legacy/bam-specific.fofrm"));

        CHECK(selected.SequenceSize == 1);
        CHECK(selected.AnimTicks == 100);
        REQUIRE(selected.Frames.size() == 1);
        CHECK(selected.Frames[0].Width == 2);
        CHECK(selected.Frames[0].Height == 1);
        CHECK(selected.Frames[0].NextX == -1);
        CHECK(selected.Frames[0].NextY == -2);
        CHECK(selected.Frames[0].Data == vector<uint8_t> {0x06, 0x05, 0x04, 0xFF, 0x33, 0x22, 0x11, 0xFF});

        TestRig clamped_frame;
        clamped_frame.AddSourceFile("legacy/bam-clamped.fofrm", "Frm=multi$0-9.bam\n");
        AddSourceBinaryFile(clamped_frame, "legacy/multi.bam", MakeMultiFrameBam());

        ImageBaker clamped_frame_baker {clamped_frame.MakeContext()};
        clamped_frame_baker.BakeFiles(clamped_frame.GetAllSourceFiles(), "legacy/bam-clamped.fofrm");

        REQUIRE(clamped_frame.Outputs.contains("legacy/bam-clamped.fofrm"));
        const auto clamped = ReadSingleDirSequence(clamped_frame.Outputs.at("legacy/bam-clamped.fofrm"));

        CHECK(clamped.SequenceSize == 1);
        CHECK(clamped.AnimTicks == 100);
        REQUIRE(clamped.Frames.size() == 1);
        CHECK(clamped.Frames[0].Width == 1);
        CHECK(clamped.Frames[0].Height == 1);
        CHECK(clamped.Frames[0].NextX == 0);
        CHECK(clamped.Frames[0].NextY == 1);
        CHECK(clamped.Frames[0].Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF});
    }

    SECTION("BakesArtFrameRangeAndRleThroughFofrm")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/art-range.fofrm", "Frm=range$3F1-0.art\n");
        AddSourceBinaryFile(rig, "gfx/range.art", MakeRleFrameRangeArt());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/art-range.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/art-range.fofrm"));
        const auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/art-range.fofrm"));

        CHECK(sequence.SequenceSize == 2);
        CHECK(sequence.AnimTicks == 100);
        REQUIRE(sequence.Frames.size() == 2);
        CHECK(sequence.Frames[0].Width == 3);
        CHECK(sequence.Frames[0].Height == 1);
        CHECK(sequence.Frames[0].NextX == 0);
        CHECK(sequence.Frames[0].NextY == -1);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF, 0x33, 0x22, 0x11, 0xFF, 0x66, 0x55, 0x44, 0xFF});
        CHECK(sequence.Frames[1].Width == 0);
        CHECK(sequence.Frames[1].Height == 0);
        CHECK(sequence.Frames[1].Data.empty());

        TestRig forward;
        forward.AddSourceFile("gfx/art-forward.fofrm", "Frm=range$1F0-1.art\n");
        AddSourceBinaryFile(forward, "gfx/range.art", MakeRleFrameRangeArt());

        ImageBaker forward_baker {forward.MakeContext()};
        forward_baker.BakeFiles(forward.GetAllSourceFiles(), "gfx/art-forward.fofrm");

        REQUIRE(forward.Outputs.contains("gfx/art-forward.fofrm"));
        const auto forward_sequence = ReadSingleDirSequence(forward.Outputs.at("gfx/art-forward.fofrm"));

        CHECK(forward_sequence.SequenceSize == 2);
        CHECK(forward_sequence.AnimTicks == 100);
        REQUIRE(forward_sequence.Frames.size() == 2);
        CHECK(forward_sequence.Frames[0].Width == 0);
        CHECK(forward_sequence.Frames[0].Height == 0);
        CHECK(forward_sequence.Frames[0].Data.empty());
        CHECK(forward_sequence.Frames[1].Width == 3);
        CHECK(forward_sequence.Frames[1].Height == 1);
        CHECK(forward_sequence.Frames[1].NextX == 0);
        CHECK(forward_sequence.Frames[1].NextY == -1);
        CHECK(forward_sequence.Frames[1].Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF, 0x33, 0x22, 0x11, 0xFF, 0x66, 0x55, 0x44, 0xFF});
    }

    SECTION("BakesDirectionalArtOptionEdgesThroughFofrm")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/art-dirs.fofrm", "Frm=dirs$02THVf5.art\n");
        AddSourceBinaryFile(rig, "gfx/dirs.art", MakeDirectionalEmptyArt());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/art-dirs.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/art-dirs.fofrm"));
        const auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/art-dirs.fofrm"));

        CHECK(sequence.SequenceSize == 1);
        CHECK(sequence.AnimTicks == 100);
        REQUIRE(sequence.Frames.size() == 1);
        CHECK(sequence.Frames[0].Width == 0);
        CHECK(sequence.Frames[0].Height == 0);
        CHECK(sequence.Frames[0].NextX == 7);
        CHECK(sequence.Frames[0].NextY == 11);
        CHECK(sequence.Frames[0].Data.empty());
    }

    SECTION("BakesSprSequenceOptionsThroughFofrm")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/spr.fofrm", "Frm=actor$[1,5,-10,20]walk.spr\n");
        AddSourceBinaryFile(rig, "gfx/actor.spr", MakeSimpleSpr());

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/spr.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/spr.fofrm"));
        const auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/spr.fofrm"));

        CHECK(sequence.SequenceSize == 1);
        CHECK(sequence.AnimTicks == 100);
        REQUIRE(sequence.Frames.size() == 1);
        CHECK(sequence.Frames[0].Width == 3);
        CHECK(sequence.Frames[0].Height == 1);
        CHECK(sequence.Frames[0].NextX == 1);
        CHECK(sequence.Frames[0].NextY == 1);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF, 0x2D, 0x14, 0x28, 0xFF, 0, 0, 0, 0x40});
    }

    SECTION("BakesSprSkippedInvalidSequenceAndAllColorOffsets")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/spr-all-offsets.fofrm", "Frm=actor$[9,1,2,3]walk.spr\n");
        AddSourceBinaryFile(rig, "gfx/actor.spr", MakeSprWithSequenceFrames({-43, 11, 12, 13, 5}));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/spr-all-offsets.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/spr-all-offsets.fofrm"));
        const auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/spr-all-offsets.fofrm"));

        CHECK(sequence.SequenceSize == 1);
        CHECK(sequence.AnimTicks == 100);
        REQUIRE(sequence.Frames.size() == 1);
        CHECK(sequence.Frames[0].Width == 3);
        CHECK(sequence.Frames[0].Height == 1);
        CHECK(sequence.Frames[0].NextX == 1);
        CHECK(sequence.Frames[0].NextY == 1);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {0x34, 0x24, 0x14, 0xFF, 0x29, 0x20, 0x17, 0xFF, 0x01, 0x02, 0x03, 0x40});
    }

    SECTION("BakesSprRepeatedFramesAsShared")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/repeated.spr", MakeSprWithSequenceFrames({0, 0}, 2));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/repeated.spr");

        REQUIRE(rig.Outputs.contains("gfx/repeated.spr"));

        DataReader reader {rig.Outputs.at("gfx/repeated.spr")};
        CHECK(reader.Read<uint8_t>() == 42);
        CHECK(reader.Read<uint16_t>() == 2);
        CHECK(reader.Read<uint16_t>() == 200);
        CHECK(reader.Read<uint8_t>() == GameSettings::MAP_DIR_COUNT);

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            CHECK(reader.Read<int16_t>() == 0);
            CHECK(reader.Read<int16_t>() == 0);
            CHECK_FALSE(reader.Read<bool>());

            const auto width = reader.Read<uint16_t>();
            const auto height = reader.Read<uint16_t>();
            const auto next_x = reader.Read<int16_t>();
            const auto next_y = reader.Read<int16_t>();

            const size_t data_size = numeric_cast<size_t>(width) * height * 4;
            const_span<uint8_t> data = reader.ReadBytes(data_size);

            if (width == 3) {
                CHECK(width == 3);
                CHECK(height == 1);
                CHECK(next_x == 1);
                CHECK(next_y == 1);
                CHECK(vector<uint8_t>(data.begin(), data.end()) == vector<uint8_t> {0x33, 0x22, 0x11, 0xFF, 0x28, 0x1E, 0x14, 0xFF, 0, 0, 0, 0x40});
            }
            else {
                CHECK(width == 1);
                CHECK(height == 1);
                CHECK(next_x == 0);
                CHECK(next_y == 1);
                CHECK(vector<uint8_t>(data.begin(), data.end()) == vector<uint8_t> {0, 0, 0, 0});
            }

            CHECK(reader.Read<bool>());
            CHECK(reader.Read<uint16_t>() == 0);
        }

        CHECK(reader.Read<uint8_t>() == 42);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("BakesFofrmSequenceThroughNestedImages")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/anim.fofrm", R"(
Fps=5
Count=2
OffsetX=1
OffsetY=2

[Dir_0]
OffsetX=3
OffsetY=4
Frm_0=first.tga
frm_1=second.tga
NextX_0=5
next_y_0=6
next_x_1=-2
NextY_1=8
)");
        AddSourceBinaryFile(rig, "gfx/first.tga", MakeRawTga(1, 1, 32, {0, 0, 255, 255}));
        AddSourceBinaryFile(rig, "gfx/second.tga", MakeRawTga(1, 1, 32, {0, 255, 0, 128}));

        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/anim.fofrm");

        REQUIRE(rig.Outputs.contains("gfx/anim.fofrm"));
        const auto sequence = ReadSingleDirSequence(rig.Outputs.at("gfx/anim.fofrm"));

        CHECK(sequence.SequenceSize == 2);
        CHECK(sequence.AnimTicks == 400);
        CHECK(sequence.OffsX == 3);
        CHECK(sequence.OffsY == 4);
        REQUIRE(sequence.Frames.size() == 2);
        CHECK(sequence.Frames[0].NextX == 5);
        CHECK(sequence.Frames[0].NextY == 6);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {255, 0, 0, 255});
        CHECK(sequence.Frames[1].NextX == -2);
        CHECK(sequence.Frames[1].NextY == 8);
        CHECK(sequence.Frames[1].Data == vector<uint8_t> {0, 255, 0, 128});
    }

    SECTION("FofrmDirectionEdgeCases")
    {
        TestRig dir_one_without_frames;
        dir_one_without_frames.AddSourceFile("gfx/no-dir.fofrm", R"(
Frm=base.tga

[Dir_1]
OffsetX=5
)");
        AddSourceBinaryFile(dir_one_without_frames, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));

        ImageBaker dir_one_baker {dir_one_without_frames.MakeContext()};
        dir_one_baker.BakeFiles(dir_one_without_frames.GetAllSourceFiles(), "gfx/no-dir.fofrm");

        REQUIRE(dir_one_without_frames.Outputs.contains("gfx/no-dir.fofrm"));
        const auto sequence = ReadSingleDirSequence(dir_one_without_frames.Outputs.at("gfx/no-dir.fofrm"));

        CHECK(sequence.SequenceSize == 1);
        CHECK(sequence.AnimTicks == 100);
        CHECK(sequence.OffsX == 0);
        CHECK(sequence.OffsY == 0);
        REQUIRE(sequence.Frames.size() == 1);
        CHECK(sequence.Frames[0].Data == vector<uint8_t> {255, 0, 0, 255});

        TestRig direction_gap;
        direction_gap.AddSourceFile("gfx/gap.fofrm", R"(
Frm=base.tga

[Dir_1]
Frm=dir1.tga
)");
        AddSourceBinaryFile(direction_gap, "gfx/base.tga", MakeRawTga(1, 1, 24, {0, 0, 255}));
        AddSourceBinaryFile(direction_gap, "gfx/dir1.tga", MakeRawTga(1, 1, 24, {0, 255, 0}));

        ImageBaker direction_gap_baker {direction_gap.MakeContext()};
        CHECK_THROWS_AS(direction_gap_baker.BakeFiles(direction_gap.GetAllSourceFiles(), "gfx/gap.fofrm"), ImageBakerException);
        CHECK(direction_gap.Outputs.empty());

        const auto add_toy_loader = [](ImageBaker& baker) {
            baker.AddLoader(
                [](string_view fname, string_view opt, FileReader reader, const FileCollection& files) {
                    ignore_unused(opt, reader, files);

                    const string fname_str {fname};
                    ImageBaker::FrameCollection collection;
                    collection.SequenceSize = fname_str.find("two") != string::npos ? 2 : 1;
                    collection.AnimTicks = 100;
                    collection.Main.Frames.resize(collection.SequenceSize);

                    if (fname_str.find("shared") != string::npos) {
                        collection.Main.Frames[0] = ImageBaker::FrameShot {.Shared = true, .SharedIndex = 0};
                        return collection;
                    }

                    for (uint16_t i = 0; i < collection.SequenceSize; i++) {
                        collection.Main.Frames[i] = ImageBaker::FrameShot {
                            .Width = 1,
                            .Height = 1,
                            .Data = {numeric_cast<uint8_t>(10 + i), 20, 30, 255},
                        };
                    }

                    return collection;
                },
                {"toy"});
        };

        TestRig mismatched_direction;
        mismatched_direction.AddSourceFile("gfx/mismatch.fofrm", R"(
Frm=two.toy

[Dir_1]
Frm=one.toy
)");
        mismatched_direction.AddSourceFile("gfx/two.toy", "two");
        mismatched_direction.AddSourceFile("gfx/one.toy", "one");

        ImageBaker mismatched_direction_baker {mismatched_direction.MakeContext()};
        add_toy_loader(mismatched_direction_baker);
        CHECK_THROWS_AS(mismatched_direction_baker.BakeFiles(mismatched_direction.GetAllSourceFiles(), "gfx/mismatch.fofrm"), ImageBakerException);
        CHECK(mismatched_direction.Outputs.empty());

        TestRig shared_nested_frame;
        shared_nested_frame.AddSourceFile("gfx/shared.fofrm", "Frm=shared.toy\n");
        shared_nested_frame.AddSourceFile("gfx/shared.toy", "shared");

        ImageBaker shared_nested_frame_baker {shared_nested_frame.MakeContext()};
        add_toy_loader(shared_nested_frame_baker);
        CHECK_THROWS_AS(shared_nested_frame_baker.BakeFiles(shared_nested_frame.GetAllSourceFiles(), "gfx/shared.fofrm"), ImageBakerException);
        CHECK(shared_nested_frame.Outputs.empty());
    }

    SECTION("CustomLoaderBakesDirsSharedFramesAndNewName")
    {
        TestRig rig;
        rig.AddSourceFile("gfx/custom.toy", "payload");

        ImageBaker baker {rig.MakeContext()};
        baker.AddLoader(
            [](string_view fname, string_view opt, FileReader reader, const FileCollection& files) {
                ignore_unused(files);

                CHECK(fname == "gfx/custom.toy");
                CHECK(opt.empty());
                CHECK(reader.GetSize() == 7);

                ImageBaker::FrameCollection collection;
                collection.SequenceSize = 2;
                collection.AnimTicks = 77;
                collection.HaveDirs = true;
                collection.NewName = "gfx/custom.baked";

                collection.Main.OffsX = 1;
                collection.Main.OffsY = 2;
                collection.Main.Frames = {
                    ImageBaker::FrameShot {.Width = 1, .Height = 1, .NextX = 3, .NextY = 4, .Data = {10, 20, 30, 40}},
                    ImageBaker::FrameShot {.Shared = true, .SharedIndex = 0},
                };

                for (int32_t dir = 0; dir < ImageBaker::MAX_DIRS_MINUS_ONE; dir++) {
                    auto& sequence = collection.Dirs[dir];
                    sequence.OffsX = numeric_cast<int16_t>(10 + dir);
                    sequence.OffsY = numeric_cast<int16_t>(20 + dir);
                    sequence.Frames = {
                        ImageBaker::FrameShot {.Width = 1, .Height = 1, .NextX = numeric_cast<int16_t>(30 + dir), .NextY = numeric_cast<int16_t>(40 + dir), .Data = {numeric_cast<uint8_t>(dir), 1, 2, 3}},
                        ImageBaker::FrameShot {.Shared = true, .SharedIndex = 0},
                    };
                }

                return collection;
            },
            {"toy"});

        baker.BakeFiles(rig.GetAllSourceFiles(), "gfx/custom.toy");

        REQUIRE_FALSE(rig.Outputs.contains("gfx/custom.toy"));
        REQUIRE(rig.Outputs.contains("gfx/custom.baked"));

        DataReader reader {rig.Outputs.at("gfx/custom.baked")};
        CHECK(reader.Read<uint8_t>() == 42);
        CHECK(reader.Read<uint16_t>() == 2);
        CHECK(reader.Read<uint16_t>() == 77);
        CHECK(reader.Read<uint8_t>() == GameSettings::MAP_DIR_COUNT);

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            const int16_t offs_x = reader.Read<int16_t>();
            const int16_t offs_y = reader.Read<int16_t>();

            if (dir == 0) {
                CHECK(offs_x == 1);
                CHECK(offs_y == 2);
            }
            else {
                CHECK(offs_x == 9 + dir);
                CHECK(offs_y == 19 + dir);
            }

            CHECK_FALSE(reader.Read<bool>());
            CHECK(reader.Read<uint16_t>() == 1);
            CHECK(reader.Read<uint16_t>() == 1);
            (void)reader.Read<int16_t>();
            (void)reader.Read<int16_t>();
            (void)reader.ReadBytes(4);

            CHECK(reader.Read<bool>());
            CHECK(reader.Read<uint16_t>() == 0);
        }

        CHECK(reader.Read<uint8_t>() == 42);
        CHECK_NOTHROW(reader.VerifyEnd());
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

    SECTION("BakeCheckerSkipsScanModeFiles")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/skipped-scan.tga", MakeRawTga(1, 1, 24, {0, 0, 255}), 31);

        vector<pair<string, uint64_t>> checked;

        ImageBaker baker {rig.MakeContext("TestPack", [&checked](string_view path, uint64_t write_time) {
            checked.emplace_back(string {path}, write_time);
            return false;
        })};
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        CHECK(rig.Outputs.empty());
        CHECK(checked == vector<pair<string, uint64_t>> {{"gfx/skipped-scan.tga", 31}});
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

    SECTION("InvalidLegacyHeadersAreReported")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "legacy/bad.zar", vector<uint8_t> {'b', 'a', 'd', 'z', 'a', 'r'});
        AddSourceBinaryFile(rig, "legacy/bad-palette.zar", MakeZarWithPaletteCount(257));
        AddSourceBinaryFile(rig, "legacy/bad.til", vector<uint8_t> {'b', 'a', 'd', 't', 'i', 'l', '\0'});
        AddSourceBinaryFile(rig, "legacy/missing-tiledata.til", MakeTilWithoutTiledata());
        AddSourceBinaryFile(rig, "legacy/bad-zar-header.til", MakeTilWithBadZarHeader());
        AddSourceBinaryFile(rig, "legacy/bad-palette.til", MakeTilWithPaletteCount(257));
        AddSourceBinaryFile(rig, "legacy/bad.mos", vector<uint8_t> {'b', 'a', 'd', 'm', 'o', 's', '\0', '\0'});
        AddSourceBinaryFile(rig, "legacy/bad.bam", vector<uint8_t> {'b', 'a', 'd', 'b', 'a', 'm', '\0', '\0'});

        ImageBaker baker {rig.MakeContext()};

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad.zar"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad-palette.zar"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad.til"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/missing-tiledata.til"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad-zar-header.til"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad-palette.til"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad.mos"), ImageBakerException);
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "legacy/bad.bam"), ImageBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("InvalidSprInputsAreReported")
    {
        TestRig rig;
        AddSourceBinaryFile(rig, "gfx/bad-header.spr", vector<uint8_t>(11));
        rig.AddSourceFile("gfx/missing-sequence.fofrm", "Frm=actor$run.spr\n");
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

    SECTION("InvalidFofrmNestedFramesAreReported")
    {
        TestRig missing_frame;
        missing_frame.AddSourceFile("gfx/missing.fofrm", "frm=missing.tga\n");

        ImageBaker missing_baker {missing_frame.MakeContext()};
        CHECK_THROWS_AS(missing_baker.BakeFiles(missing_frame.GetAllSourceFiles(), "gfx/missing.fofrm"), ImageBakerException);
        CHECK(missing_frame.Outputs.empty());

        TestRig unsupported_frame;
        unsupported_frame.AddSourceFile("gfx/unsupported.fofrm", "frm=frame.unsupported\n");
        unsupported_frame.AddSourceFile("gfx/frame.unsupported", "payload");

        ImageBaker unsupported_baker {unsupported_frame.MakeContext()};
        CHECK_THROWS_AS(unsupported_baker.BakeFiles(unsupported_frame.GetAllSourceFiles(), "gfx/unsupported.fofrm"), ImageBakerException);
        CHECK(unsupported_frame.Outputs.empty());
    }
}

FO_END_NAMESPACE
