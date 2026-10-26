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

#include "ImageTransforms.h"

#include "AnyData.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

auto ReadImageIntegers(string_view value, string_view fname, string_view section, string_view key) -> vector<int32_t>
{
    auto parsed = AnyData::ParseValue(string {value}, false, true, AnyData::ValueType::String);
    vector<int32_t> result;

    for (const auto& token : parsed.AsArray()) {
        string_view text = token.AsString();
        string_view digits = text.substr(text.starts_with('+') || text.starts_with('-') ? 1 : 0);
        FO_VERIFY_AND_THROW(!digits.empty() && std::all_of(digits.begin(), digits.end(), [](char ch) { return ch >= '0' && ch <= '9'; }), "FOFRM requires signed int32 tokens", fname, section, key, text);

        if (text.starts_with('+')) {
            text.remove_prefix(1);
        }

        int32_t number {};
        auto conversion = std::from_chars(text.data(), text.data() + text.size(), number);
        FO_VERIFY_AND_THROW(!text.empty() && conversion.ec == std::errc {} && conversion.ptr == text.data() + text.size(), "FOFRM requires signed int32 tokens", fname, section, key, token.AsString());
        result.emplace_back(number);
    }

    return result;
}

auto ReadImageTransforms(const ConfigFile& recipe, string_view section, size_t reference_count, string_view fname, bool source) -> vector<ImageTransform>
{
    vector<ImageTransform> result(reference_count);

    auto read = [&](string_view key, string_view array_key, size_t width, bool flip = false) -> optional<vector<int32_t>> {
        bool local = recipe.HasKey(section, key);
        bool array = !array_key.empty() && recipe.HasKey(section, array_key);
        FO_VERIFY_AND_THROW(!(local && array), "FOFRM common and per-reference forms are mutually exclusive", fname, section, key, array_key);

        string_view owner = section;
        bool list = array;

        if (!local && !array && !section.empty()) {
            owner = "";
            list = source && !array_key.empty() && recipe.HasKey(owner, array_key);
            local = recipe.HasKey(owner, key);
        }

        if (!local && !list) {
            return {};
        }

        vector<int32_t> values;

        if (flip) {
            auto parsed = AnyData::ParseValue(string {recipe.GetAsStr(owner, list ? array_key : key)}, false, true, AnyData::ValueType::String);

            for (const auto& token : parsed.AsArray()) {
                string_view name = token.AsString();
                FO_VERIFY_AND_THROW(name == "None" || name == "X" || name == "Y" || name == "XY", "FOFRM Flip requires None, X, Y or XY", fname, owner, name);
                values.emplace_back(name == "None" ? 0 : name == "X" ? 1 : name == "Y" ? 2 : 3);
            }
        }
        else {
            values = ReadImageIntegers(recipe.GetAsStr(owner, list ? array_key : key), fname, owner, list ? array_key : key);
        }

        FO_VERIFY_AND_THROW(values.size() == width * (list ? reference_count : 1) && (!list || reference_count > 0), "FOFRM transform requires one entry per source reference", fname, section, key, array_key, reference_count, values.size());

        if (!list) {
            vector<int32_t> expanded;

            for (size_t index = 0; index < reference_count; index++) {
                expanded.insert(expanded.end(), values.begin(), values.end());
            }

            return expanded;
        }

        return values;
    };

    auto directions = read("InputDir", "FrameInputDirs", 1);
    auto crops = read("Crop", "FrameCrops", 4);
    auto grids = read("Grid", "", 2);
    auto flips = read("Flip", "FrameFlips", 1, true);
    auto rotations = read("Rotate", "FrameRotations", 1);
    auto canvases = read("Canvas", "FrameCanvases", 2);
    auto canvas_offsets = read("CanvasOffset", "FrameCanvasOffsets", 2);
    auto motions = read("Motion", "FrameMotions", 2);

    auto read_enum = [&](string_view key, const vector<string>& choices, int32_t fallback) -> int32_t {
        string_view owner = recipe.HasKey(section, key) ? section : string_view {};

        if (!recipe.HasKey(owner, key)) {
            return fallback;
        }

        auto parsed = AnyData::ParseValue(string {recipe.GetAsStr(owner, key)}, false, true, AnyData::ValueType::String);
        FO_VERIFY_AND_THROW(parsed.AsArray().Size() == 1, "FOFRM filter requires exactly one enum token", fname, section, key);
        string_view value = parsed.AsArray()[0].AsString();
        auto found = std::find(choices.begin(), choices.end(), value);
        FO_VERIFY_AND_THROW(found != choices.end(), "FOFRM unknown filter value", fname, section, key, value);

        return numeric_cast<int32_t>(std::distance(choices.begin(), found));
    };

    auto resample = static_cast<ImageResample>(read_enum("Resample", {"Nearest", "Bilinear", "Area"}, 1));
    auto usage = static_cast<ImagePixelUsage>(read_enum("PixelUsage", {"Color", "Data", "Normal"}, 0));
    auto color_space = static_cast<ImageColorSpace>(read_enum("ColorSpace", {"Encoded", "Linear"}, 0));
    FO_VERIFY_AND_THROW(usage == ImagePixelUsage::Color || (!recipe.HasKey(section, "ColorSpace") && !recipe.HasKey("", "ColorSpace")), "FOFRM ColorSpace is valid only for PixelUsage Color", fname, section);

    FO_VERIFY_AND_THROW(!canvas_offsets || canvases, "FOFRM CanvasOffset requires Canvas", fname, section);

    for (size_t index = 0; index < reference_count; index++) {
        result[index].Resample = resample;
        result[index].PixelUsage = usage;
        result[index].ColorSpace = color_space;

        if (flips) {
            result[index].Flip = (*flips)[index];
        }

        if (rotations) {
            int32_t angle = (*rotations)[index];
            FO_VERIFY_AND_THROW(angle == 0 || angle == 90 || angle == 180 || angle == 270, "FOFRM Rotate requires 0, 90, 180 or 270 clockwise degrees", fname, section, angle);
            result[index].Rotate = angle;
        }

        if (canvases) {
            isize32 canvas {(*canvases)[index * 2], (*canvases)[index * 2 + 1]};
            FO_VERIFY_AND_THROW(canvas.width > 0 && canvas.height > 0 && canvas.width <= 65535 && canvas.height <= 65535, "FOFRM Canvas requires positive uint16 dimensions", fname, section);
            result[index].Canvas = canvas;
        }

        if (canvas_offsets) {
            result[index].CanvasOffset = {(*canvas_offsets)[index * 2], (*canvas_offsets)[index * 2 + 1]};
            FO_VERIFY_AND_THROW(result[index].CanvasOffset.x >= 0 && result[index].CanvasOffset.y >= 0, "FOFRM CanvasOffset must be nonnegative", fname, section);
        }

        if (motions) {
            ipos32 motion {(*motions)[index * 2], (*motions)[index * 2 + 1]};
            FO_VERIFY_AND_THROW(motion.x >= -32768 && motion.x <= 32767 && motion.y >= -32768 && motion.y <= 32767, "FOFRM Motion requires int16 components", fname, section);
            result[index].Motion = motion;
        }

        if (directions) {
            int32_t dir = (*directions)[index];
            FO_VERIFY_AND_THROW(dir >= 0 && dir < GameSettings::MAP_DIR_COUNT, "FOFRM InputDir is outside the active geometry", fname, section, dir);
            result[index].InputDir = dir;
        }

        if (crops) {
            size_t start = index * 4;
            array<int32_t, 4> crop {(*crops)[start], (*crops)[start + 1], (*crops)[start + 2], (*crops)[start + 3]};
            FO_VERIFY_AND_THROW(crop[0] >= 0 && crop[1] >= 0 && crop[2] > 0 && crop[3] > 0, "FOFRM Crop requires nonnegative origin and positive size", fname, section);
            result[index].Crop = crop;
        }

        if (grids) {
            isize32 grid {(*grids)[index * 2], (*grids)[index * 2 + 1]};
            FO_VERIFY_AND_THROW(grid.width > 0 && grid.height > 0 && grid.width <= 65535 && grid.height <= 65535, "FOFRM Grid requires positive uint16 dimensions", fname, section);
            result[index].Grid = grid;
        }
    }

    return result;
}

auto SelectImageSequence(const ImageImport::FrameCollection& source, optional<int32_t> direction, string_view fname) -> const ImageImport::FrameSequence&
{
    int32_t dir = direction.value_or(0);
    FO_VERIFY_AND_THROW(dir >= 0 && dir < GameSettings::MAP_DIR_COUNT && dir < ImageImport::GetDirectionCount(source), "FOFRM InputDir requires an existing source direction", fname, dir);

    return dir == 0 ? source.Main : source.Dirs[dir - 1];
}

static auto CropImageFrame(const ImageImport::FrameShot& source, array<int32_t, 4> crop, string_view fname) -> ImageImport::FrameShot
{
    FO_VERIFY_AND_THROW(source.Data.size() == numeric_cast<size_t>(source.Size.width) * source.Size.height * 4, "FOFRM source RGBA payload does not match dimensions", fname);
    FO_VERIFY_AND_THROW(crop[0] >= 0 && crop[1] >= 0 && crop[2] > 0 && crop[3] > 0 && numeric_cast<int64_t>(crop[0]) + crop[2] <= source.Size.width && numeric_cast<int64_t>(crop[1]) + crop[3] <= source.Size.height, "FOFRM Crop lies outside its input frame", fname);

    auto result = source;
    result.Size = {crop[2], crop[3]};
    result.Data.resize(numeric_cast<size_t>(result.Size.width) * result.Size.height * 4);

    for (int32_t y = 0; y < result.Size.height; y++) {
        size_t start = (numeric_cast<size_t>(y + crop[1]) * source.Size.width + crop[0]) * 4;
        std::copy_n(source.Data.begin() + start, numeric_cast<size_t>(result.Size.width) * 4, result.Data.begin() + numeric_cast<size_t>(y) * result.Size.width * 4);
    }

    return result;
}

static auto ResizeEncodedColorFrame(const ImageImport::FrameShot& source, isize32 size, string_view fname) -> ImageImport::FrameShot
{
    FO_VERIFY_AND_THROW(source.Data.size() == numeric_cast<size_t>(source.Size.width) * source.Size.height * 4, "FOFRM resize source RGBA payload does not match dimensions", fname, source.Size, source.Data.size());

    if (source.Size == size) {
        return source;
    }

    ImageImport::FrameShot resized {
        .Size = size,
        .Offset = source.Offset,
        .Motion = source.Motion,
        .Duration = source.Duration,
        .PreserveHiddenColor = source.PreserveHiddenColor,
    };

    resized.Data.resize(numeric_cast<size_t>(size.width) * size.height * 4);

    if (source.Size.width == 0 || source.Size.height == 0) {
        return resized;
    }

    // Fixed-point pixel centers keep premultiplied-alpha resampling deterministic across baker platforms.
    constexpr int64_t pixel_unit = 65536;
    constexpr uint64_t weight_unit = uint64_t {1} << 32;

    for (int32_t y = 0; y < size.height; y++) {
        int64_t source_y = std::clamp((2 * numeric_cast<int64_t>(y) + 1) * source.Size.height * pixel_unit / (2 * size.height) - pixel_unit / 2, int64_t {0}, (numeric_cast<int64_t>(source.Size.height) - 1) * pixel_unit);
        int32_t y0 = numeric_cast<int32_t>(source_y / pixel_unit);
        int32_t y1 = std::min(y0 + 1, source.Size.height - 1);
        uint64_t fy = numeric_cast<uint64_t>(source_y % pixel_unit);

        for (int32_t x = 0; x < size.width; x++) {
            int64_t source_x = std::clamp((2 * numeric_cast<int64_t>(x) + 1) * source.Size.width * pixel_unit / (2 * size.width) - pixel_unit / 2, int64_t {0}, (numeric_cast<int64_t>(source.Size.width) - 1) * pixel_unit);
            int32_t x0 = numeric_cast<int32_t>(source_x / pixel_unit);
            int32_t x1 = std::min(x0 + 1, source.Size.width - 1);
            uint64_t fx = numeric_cast<uint64_t>(source_x % pixel_unit);
            array<size_t, 4> positions {
                (numeric_cast<size_t>(y0) * source.Size.width + x0) * 4,
                (numeric_cast<size_t>(y0) * source.Size.width + x1) * 4,
                (numeric_cast<size_t>(y1) * source.Size.width + x0) * 4,
                (numeric_cast<size_t>(y1) * source.Size.width + x1) * 4,
            };

            array<uint64_t, 4> weights {
                (pixel_unit - fx) * (pixel_unit - fy),
                fx * (pixel_unit - fy),
                (pixel_unit - fx) * fy,
                fx * fy,
            };

            array<uint64_t, 4> sums {};

            for (size_t sample = 0; sample < 4; sample++) {
                uint64_t alpha_weight = weights[sample] * source.Data[positions[sample] + 3];
                sums[3] += alpha_weight;

                for (size_t channel = 0; channel < 3; channel++) {
                    sums[channel] += alpha_weight * source.Data[positions[sample] + channel];
                }
            }

            size_t destination = (numeric_cast<size_t>(y) * size.width + x) * 4;
            uint8_t alpha = numeric_cast<uint8_t>((sums[3] + weight_unit / 2) / weight_unit);
            resized.Data[destination + 3] = alpha;

            if (alpha != 0) {
                for (size_t channel = 0; channel < 3; channel++) {
                    resized.Data[destination + channel] = numeric_cast<uint8_t>((sums[channel] + sums[3] / 2) / sums[3]);
                }
            }
        }
    }

    return resized;
}

static constexpr array<uint16_t, 256> SRGB_LINEAR16 {
    0,
    20,
    40,
    60,
    80,
    99,
    119,
    139,
    159,
    179,
    199,
    219,
    241,
    264,
    288,
    313,
    340,
    367,
    396,
    427,
    458,
    491,
    526,
    562,
    599,
    637,
    677,
    718,
    761,
    805,
    851,
    898,
    947,
    997,
    1048,
    1101,
    1156,
    1212,
    1270,
    1330,
    1391,
    1453,
    1517,
    1583,
    1651,
    1720,
    1790,
    1863,
    1937,
    2013,
    2090,
    2170,
    2250,
    2333,
    2418,
    2504,
    2592,
    2681,
    2773,
    2866,
    2961,
    3058,
    3157,
    3258,
    3360,
    3464,
    3570,
    3678,
    3788,
    3900,
    4014,
    4129,
    4247,
    4366,
    4488,
    4611,
    4736,
    4864,
    4993,
    5124,
    5257,
    5392,
    5530,
    5669,
    5810,
    5953,
    6099,
    6246,
    6395,
    6547,
    6700,
    6856,
    7014,
    7174,
    7335,
    7500,
    7666,
    7834,
    8004,
    8177,
    8352,
    8528,
    8708,
    8889,
    9072,
    9258,
    9445,
    9635,
    9828,
    10022,
    10219,
    10417,
    10619,
    10822,
    11028,
    11235,
    11446,
    11658,
    11873,
    12090,
    12309,
    12530,
    12754,
    12980,
    13209,
    13440,
    13673,
    13909,
    14146,
    14387,
    14629,
    14874,
    15122,
    15371,
    15623,
    15878,
    16135,
    16394,
    16656,
    16920,
    17187,
    17456,
    17727,
    18001,
    18277,
    18556,
    18837,
    19121,
    19407,
    19696,
    19987,
    20281,
    20577,
    20876,
    21177,
    21481,
    21787,
    22096,
    22407,
    22721,
    23038,
    23357,
    23678,
    24002,
    24329,
    24658,
    24990,
    25325,
    25662,
    26001,
    26344,
    26688,
    27036,
    27386,
    27739,
    28094,
    28452,
    28813,
    29176,
    29542,
    29911,
    30282,
    30656,
    31033,
    31412,
    31794,
    32179,
    32567,
    32957,
    33350,
    33745,
    34143,
    34544,
    34948,
    35355,
    35764,
    36176,
    36591,
    37008,
    37429,
    37852,
    38278,
    38706,
    39138,
    39572,
    40009,
    40449,
    40891,
    41337,
    41785,
    42236,
    42690,
    43147,
    43606,
    44069,
    44534,
    45002,
    45473,
    45947,
    46423,
    46903,
    47385,
    47871,
    48359,
    48850,
    49344,
    49841,
    50341,
    50844,
    51349,
    51858,
    52369,
    52884,
    53401,
    53921,
    54445,
    54971,
    55500,
    56032,
    56567,
    57105,
    57646,
    58190,
    58737,
    59287,
    59840,
    60396,
    60955,
    61517,
    62082,
    62650,
    63221,
    63795,
    64372,
    64952,
    65535,
};

static auto IntegerSqrt(uint64_t value) -> uint64_t
{
    uint64_t result = 0;
    uint64_t bit = uint64_t {1} << 62;

    while (bit > value) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else {
            result >>= 1;
        }

        bit >>= 2;
    }

    return result;
}

static auto EncodeLinear16(uint64_t value) -> uint8_t
{
    auto high = std::lower_bound(SRGB_LINEAR16.begin(), SRGB_LINEAR16.end(), value);

    if (high == SRGB_LINEAR16.begin()) {
        return 0;
    }

    if (high == SRGB_LINEAR16.end()) {
        return 255;
    }

    bool upper = *high - value <= value - *(high - 1);

    return numeric_cast<uint8_t>(std::distance(SRGB_LINEAR16.begin(), high) - (upper ? 0 : 1));
}

static auto ResizeImageFrame(const ImageImport::FrameShot& source, isize32 size, const ImageTransform& transform, string_view fname) -> ImageImport::FrameShot
{
    FO_VERIFY_AND_THROW(source.Size.width >= 0 && source.Size.height >= 0, "FOFRM resize source requires nonnegative dimensions", fname, source.Size);

    if (transform.Resample == ImageResample::Area) {
        FO_VERIFY_AND_THROW(size.width <= source.Size.width && size.height <= source.Size.height, "FOFRM Area resampling supports downsampling or identity only", fname);
    }

    if (transform.Resample == ImageResample::Bilinear && transform.PixelUsage == ImagePixelUsage::Color && transform.ColorSpace == ImageColorSpace::Encoded) {
        return ResizeEncodedColorFrame(source, size, fname);
    }

    FO_VERIFY_AND_THROW(source.Data.size() == numeric_cast<size_t>(source.Size.width) * source.Size.height * 4, "FOFRM resize source RGBA payload does not match dimensions", fname, source.Size, source.Data.size());

    if (source.Size == size) {
        return source;
    }

    auto result = source;
    result.Size = size;
    result.Data.assign(numeric_cast<size_t>(result.Size.width) * result.Size.height * 4, 0);

    if (source.Size.width == 0 || source.Size.height == 0) {
        return result;
    }

    constexpr int64_t unit = 65536;

    auto axis = [](int32_t position, int32_t extent, int32_t target) {
        int64_t coordinate = std::clamp((2 * numeric_cast<int64_t>(position) + 1) * extent * unit / (2 * target) - unit / 2, int64_t {0}, (numeric_cast<int64_t>(extent) - 1) * unit);
        int32_t low = numeric_cast<int32_t>(coordinate / unit);

        return array<int32_t, 3> {low, std::min(low + 1, extent - 1), numeric_cast<int32_t>(coordinate % unit)};
    };

    for (int32_t y = 0; y < size.height; y++) {
        for (int32_t x = 0; x < size.width; x++) {
            size_t destination = (numeric_cast<size_t>(y) * result.Size.width + x) * 4;

            if (transform.Resample == ImageResample::Nearest) {
                int32_t sx = numeric_cast<int32_t>((2 * numeric_cast<int64_t>(x) + 1) * source.Size.width / (2 * size.width));
                int32_t sy = numeric_cast<int32_t>((2 * numeric_cast<int64_t>(y) + 1) * source.Size.height / (2 * size.height));
                size_t position = (numeric_cast<size_t>(sy) * source.Size.width + sx) * 4;
                std::copy_n(source.Data.begin() + position, 4, result.Data.begin() + destination);
                continue;
            }

            array<uint64_t, 4> sums {};
            array<int64_t, 3> normals {};
            uint64_t total = 0;

            auto sample = [&](int32_t sx, int32_t sy, uint64_t weight) {
                size_t position = (numeric_cast<size_t>(sy) * source.Size.width + sx) * 4;
                uint64_t alpha = source.Data[position + 3];
                total += weight;
                sums[3] += weight * alpha;

                for (size_t channel = 0; channel < 3; channel++) {
                    uint8_t value = source.Data[position + channel];

                    if (transform.PixelUsage == ImagePixelUsage::Normal) {
                        normals[channel] += numeric_cast<int64_t>(weight) * (2 * value - 255);
                    }
                    else if (transform.PixelUsage == ImagePixelUsage::Data) {
                        sums[channel] += weight * value;
                    }
                    else {
                        sums[channel] += weight * alpha * (transform.ColorSpace == ImageColorSpace::Linear ? SRGB_LINEAR16[value] : value);
                    }
                }
            };

            if (transform.Resample == ImageResample::Bilinear) {
                auto ax = axis(x, source.Size.width, size.width);
                auto ay = axis(y, source.Size.height, size.height);
                sample(ax[0], ay[0], numeric_cast<uint64_t>(unit - ax[2]) * (unit - ay[2]));
                sample(ax[1], ay[0], numeric_cast<uint64_t>(ax[2]) * (unit - ay[2]));
                sample(ax[0], ay[1], numeric_cast<uint64_t>(unit - ax[2]) * ay[2]);
                sample(ax[1], ay[1], numeric_cast<uint64_t>(ax[2]) * ay[2]);
            }
            else {
                int64_t left = numeric_cast<int64_t>(x) * source.Size.width;
                int64_t right = numeric_cast<int64_t>(x + 1) * source.Size.width;
                int64_t top = numeric_cast<int64_t>(y) * source.Size.height;
                int64_t bottom = numeric_cast<int64_t>(y + 1) * source.Size.height;

                for (int32_t sy = numeric_cast<int32_t>(top / size.height); sy < (bottom + size.height - 1) / size.height; sy++) {
                    int64_t wy = std::min(numeric_cast<int64_t>(sy + 1) * size.height, bottom) - std::max(numeric_cast<int64_t>(sy) * size.height, top);

                    for (int32_t sx = numeric_cast<int32_t>(left / size.width); sx < (right + size.width - 1) / size.width; sx++) {
                        int64_t wx = std::min(numeric_cast<int64_t>(sx + 1) * size.width, right) - std::max(numeric_cast<int64_t>(sx) * size.width, left);
                        sample(sx, sy, numeric_cast<uint64_t>(wx * wy));
                    }
                }
            }

            uint8_t alpha = numeric_cast<uint8_t>((sums[3] + total / 2) / total);
            result.Data[destination + 3] = alpha;

            if (transform.PixelUsage == ImagePixelUsage::Normal) {
                uint64_t squared = 0;

                for (int64_t& component : normals) {
                    int64_t value = component * unit;
                    int64_t divisor = numeric_cast<int64_t>(total);
                    component = value >= 0 ? (value + divisor / 2) / divisor : -((-value + divisor / 2) / divisor);
                    squared += numeric_cast<uint64_t>(component * component);
                }

                int64_t length = numeric_cast<int64_t>(IntegerSqrt(squared));

                for (size_t channel = 0; channel < 3; channel++) {
                    result.Data[destination + channel] = length == 0 ? numeric_cast<uint8_t>(channel == 2 ? 255 : 128) : numeric_cast<uint8_t>(std::clamp(((normals[channel] + length) * 255 + length) / (2 * length), int64_t {0}, int64_t {255}));
                }
            }
            else if (transform.PixelUsage == ImagePixelUsage::Data) {
                for (size_t channel = 0; channel < 3; channel++) {
                    result.Data[destination + channel] = numeric_cast<uint8_t>((sums[channel] + total / 2) / total);
                }
            }
            else if (alpha != 0) {
                for (size_t channel = 0; channel < 3; channel++) {
                    uint64_t value = (sums[channel] + sums[3] / 2) / sums[3];
                    result.Data[destination + channel] = transform.ColorSpace == ImageColorSpace::Linear ? EncodeLinear16(value) : numeric_cast<uint8_t>(value);
                }
            }
        }
    }

    return result;
}

static auto TransformImagePixels(ImageImport::FrameShot frame, const ImageTransform& transform, string_view fname) -> ImageImport::FrameShot
{
    frame.PreserveHiddenColor = frame.PreserveHiddenColor || transform.PixelUsage != ImagePixelUsage::Color;
    FO_VERIFY_AND_THROW(frame.Data.size() == numeric_cast<size_t>(frame.Size.width) * frame.Size.height * 4, "FOFRM source RGBA payload does not match dimensions", fname);

    if (transform.Flip != 0 || transform.Rotate != 0) {
        auto result = frame;
        bool swap_axes = transform.Rotate == 90 || transform.Rotate == 270;
        result.Size = swap_axes ? isize32 {frame.Size.height, frame.Size.width} : frame.Size;

        for (int32_t y = 0; y < frame.Size.height; y++) {
            for (int32_t x = 0; x < frame.Size.width; x++) {
                int32_t fx = (transform.Flip & 1) != 0 ? frame.Size.width - 1 - x : x;
                int32_t fy = (transform.Flip & 2) != 0 ? frame.Size.height - 1 - y : y;
                int32_t dx = transform.Rotate == 90 ? frame.Size.height - 1 - fy : transform.Rotate == 180 ? frame.Size.width - 1 - fx : transform.Rotate == 270 ? fy : fx;
                int32_t dy = transform.Rotate == 90 ? fx : transform.Rotate == 180 ? frame.Size.height - 1 - fy : transform.Rotate == 270 ? frame.Size.width - 1 - fx : fy;
                size_t src = (numeric_cast<size_t>(y) * frame.Size.width + x) * 4;
                size_t dst = (numeric_cast<size_t>(dy) * result.Size.width + dx) * 4;
                std::copy_n(frame.Data.begin() + src, 4, result.Data.begin() + dst);
            }
        }

        frame = std::move(result);
    }

    if (transform.Size) {
        frame = ResizeImageFrame(frame, *transform.Size, transform, fname);
    }

    if (transform.Canvas) {
        isize32 canvas = *transform.Canvas;
        ipos32 offset = transform.CanvasOffset;
        FO_VERIFY_AND_THROW(offset.x >= 0 && offset.y >= 0 && numeric_cast<int64_t>(offset.x) + frame.Size.width <= canvas.width && numeric_cast<int64_t>(offset.y) + frame.Size.height <= canvas.height, "FOFRM Canvas would clip its image", fname);
        auto result = frame;
        result.Size = canvas;
        result.Data.assign(numeric_cast<size_t>(result.Size.width) * result.Size.height * 4, 0);

        for (int32_t y = 0; y < frame.Size.height; y++) {
            size_t destination = (numeric_cast<size_t>(y + offset.y) * result.Size.width + offset.x) * 4;
            std::copy_n(frame.Data.begin() + numeric_cast<size_t>(y) * frame.Size.width * 4, numeric_cast<size_t>(frame.Size.width) * 4, result.Data.begin() + destination);
        }

        frame = std::move(result);
    }

    if (transform.Motion) {
        frame.Motion = *transform.Motion;
    }

    return frame;
}

auto TransformImageFrame(const ImageImport::FrameShot& source, const ImageTransform& transform, string_view fname) -> vector<ImageImport::FrameShot>
{
    FO_VERIFY_AND_THROW(source.Size.width >= 0 && source.Size.height >= 0, "FOFRM source requires nonnegative dimensions", fname, source.Size);

    auto frame = transform.Crop ? CropImageFrame(source, *transform.Crop, fname) : source;

    if (!transform.Grid) {
        return {TransformImagePixels(std::move(frame), transform, fname)};
    }

    isize32 grid = *transform.Grid;
    FO_VERIFY_AND_THROW(frame.Size.width > 0 && frame.Size.height > 0 && frame.Size.width % grid.width == 0 && frame.Size.height % grid.height == 0, "FOFRM Grid must divide its input frame exactly", fname, frame.Size, grid);
    vector<ImageImport::FrameShot> frames;

    for (int32_t y = 0; y < frame.Size.height; y += grid.height) {
        for (int32_t x = 0; x < frame.Size.width; x += grid.width) {
            FO_VERIFY_AND_THROW(frames.size() < 65535, "FOFRM Grid exceeds the frame-count limit", fname);
            frames.emplace_back(TransformImagePixels(CropImageFrame(frame, {x, y, grid.width, grid.height}, fname), transform, fname));
        }
    }

    return frames;
}

auto SelectImageFrames(vector<ImageImport::FrameShot> frames, const ConfigFile& recipe, string_view section, string_view fname, bool inherit) -> vector<ImageImport::FrameShot>
{
    string_view owner = section;

    if (!recipe.HasKey(owner, "FrameIndices") && inherit) {
        owner = "";
    }

    if (!recipe.HasKey(owner, "FrameIndices")) {
        return frames;
    }

    auto indices = ReadImageIntegers(recipe.GetAsStr(owner, "FrameIndices"), fname, owner, "FrameIndices");
    FO_VERIFY_AND_THROW(!indices.empty() && indices.size() <= 65535, "FOFRM FrameIndices requires 1..65535 indices", fname, section);
    vector<ImageImport::FrameShot> selected;
    selected.reserve(indices.size());

    for (int32_t index : indices) {
        FO_VERIFY_AND_THROW(index >= 0 && numeric_cast<size_t>(index) < frames.size(), "FOFRM FrameIndices index is out of range", fname, section, index, frames.size());
        selected.emplace_back(frames[index]);
    }

    return selected;
}

FO_END_NAMESPACE
