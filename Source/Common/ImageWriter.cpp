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

#include "ImageWriter.h"

#include "zlib.h"

FO_BEGIN_NAMESPACE

static auto EncodePng(isize32 size, const_span<ucolor> data, bool opaque) -> vector<uint8_t>;
static void FilterPngRow(const_span<uint8_t> prev_row, const_span<uint8_t> row, size_t pixel_bytes, span<uint8_t> scanline);
static auto FilterPngByte(uint8_t filter, const_span<uint8_t> prev_row, const_span<uint8_t> row, size_t index, size_t pixel_bytes) -> uint8_t;
static void AppendPngChunk(vector<uint8_t>& png, string_view type, const_span<uint8_t> payload);
static void AppendPngBigEndian(vector<uint8_t>& buf, uint32_t value);

void ImageWriter::WritePng(string_view fname, isize32 size, const_span<ucolor> data)
{
    FO_TRACE_ZONE(Render);

    vector<uint8_t> png = EncodePng(size, data, false);

    string dir = strex(fname).extract_dir().str();

    if (!dir.empty()) {
        bool dir_ok = fs::create_directories(dir);
        FO_VERIFY_AND_THROW(dir_ok, "Failed to create output directory for PNG image", dir, fname);
    }

    std::ofstream file {std::filesystem::path {fs::make_path(fname)}, std::ios::binary | std::ios::trunc};
    FO_VERIFY_AND_THROW(file, "Failed to open PNG image file for writing", fname, size, data.size());

    auto png_data = make_nptr(png.data());
    FO_VERIFY_AND_THROW(png_data, "PNG buffer data is null");
    file.write(png_data.reinterpret_as<char>().get(), static_cast<std::streamsize>(png.size()));

    FO_VERIFY_AND_THROW(file, "Failed while writing PNG image file", fname, size, data.size());
}

auto ImageWriter::EncodeCompactPng(isize32 size, const_span<ucolor> data) -> vector<uint8_t>
{
    return EncodePng(size, data, true);
}

static auto EncodePng(isize32 size, const_span<ucolor> data, bool opaque) -> vector<uint8_t>
{
    FO_TRACE_ZONE(Render);

    FO_VERIFY_AND_THROW(size.width > 0 && size.height > 0, "PNG image size must be positive", size);
    FO_VERIFY_AND_THROW(numeric_cast<size_t>(size.width) * numeric_cast<size_t>(size.height) == data.size(), "PNG pixel count does not match image size", size, data.size());

    vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    vector<uint8_t> header;
    AppendPngBigEndian(header, numeric_cast<uint32_t>(size.width));
    AppendPngBigEndian(header, numeric_cast<uint32_t>(size.height));
    header.emplace_back(8); // Bits per channel
    header.emplace_back(opaque ? 2 : 6); // Truecolor, with alpha unless the image is opaque; no channel swap either way
    header.emplace_back(0); // Deflate
    header.emplace_back(0); // Adaptive filtering
    header.emplace_back(0); // No interlace
    AppendPngChunk(png, "IHDR", header);

    // Every scanline is prefixed with its filter byte. A dump keeps the None filter, which leaves the encoder
    // trivial at the cost of a larger file; a compact image pays for per-row filtering to shrink the deflate stream
    size_t width = numeric_cast<size_t>(size.width);
    size_t pixel_bytes = opaque ? 3 : 4;
    size_t row_bytes = width * pixel_bytes;
    vector<uint8_t> scanlines((row_bytes + 1) * numeric_cast<size_t>(size.height));
    vector<uint8_t> prev_row(row_bytes);
    vector<uint8_t> row(row_bytes);

    for (int32_t y = 0; y < size.height; y++) {
        size_t row_start = numeric_cast<size_t>(y) * width;

        for (size_t x = 0; x < width; x++) {
            ucolor pixel = data[row_start + x];
            row[x * pixel_bytes] = pixel.comp.r;
            row[x * pixel_bytes + 1] = pixel.comp.g;
            row[x * pixel_bytes + 2] = pixel.comp.b;

            if (!opaque) {
                row[x * pixel_bytes + 3] = pixel.comp.a;
            }
        }

        span<uint8_t> scanline = span<uint8_t>(scanlines).subspan(numeric_cast<size_t>(y) * (row_bytes + 1), row_bytes + 1);

        if (opaque) {
            FilterPngRow(prev_row, row, pixel_bytes, scanline);
        }
        else {
            scanline[0] = 0;
            std::copy(row.begin(), row.end(), scanline.begin() + 1);
        }

        std::swap(prev_row, row);
    }

    AppendPngChunk(png, "IDAT", compressor::compress(scanlines));
    AppendPngChunk(png, "IEND", {});
    return png;
}

// Picks the filter with the smallest sum of absolute signed residuals, the heuristic the PNG specification recommends
static void FilterPngRow(const_span<uint8_t> prev_row, const_span<uint8_t> row, size_t pixel_bytes, span<uint8_t> scanline)
{
    FO_VERIFY_AND_THROW(prev_row.size() == row.size() && scanline.size() == row.size() + 1, "PNG row buffers do not match", prev_row.size(), row.size(), scanline.size());

    uint8_t best_filter = 0;
    size_t best_cost = std::numeric_limits<size_t>::max();

    for (uint8_t filter = 0; filter <= 4; filter++) {
        size_t cost = 0;

        for (size_t i = 0; i < row.size(); i++) {
            uint8_t residual = FilterPngByte(filter, prev_row, row, i, pixel_bytes);
            cost += residual < 128 ? residual : 256 - residual;
        }

        if (cost < best_cost) {
            best_cost = cost;
            best_filter = filter;
        }
    }

    scanline[0] = best_filter;

    for (size_t i = 0; i < row.size(); i++) {
        scanline[i + 1] = FilterPngByte(best_filter, prev_row, row, i, pixel_bytes);
    }
}

static auto FilterPngByte(uint8_t filter, const_span<uint8_t> prev_row, const_span<uint8_t> row, size_t index, size_t pixel_bytes) -> uint8_t
{
    int32_t left = index >= pixel_bytes ? row[index - pixel_bytes] : 0;
    int32_t up = prev_row[index];
    int32_t up_left = index >= pixel_bytes ? prev_row[index - pixel_bytes] : 0;
    int32_t predictor = 0;

    switch (filter) {
    case 1:
        predictor = left;
        break;
    case 2:
        predictor = up;
        break;
    case 3:
        predictor = (left + up) / 2;
        break;
    case 4: {
        int32_t estimate = left + up - up_left;
        int32_t to_left = std::abs(estimate - left);
        int32_t to_up = std::abs(estimate - up);
        int32_t to_up_left = std::abs(estimate - up_left);
        predictor = to_left <= to_up && to_left <= to_up_left ? left : (to_up <= to_up_left ? up : up_left);
        break;
    }
    default:
        break;
    }

    return numeric_cast<uint8_t>((row[index] - predictor) & 0xFF);
}

static void AppendPngChunk(vector<uint8_t>& png, string_view type, const_span<uint8_t> payload)
{
    vector<uint8_t> type_bytes;
    type_bytes.reserve(type.length());

    for (char c : type) {
        type_bytes.emplace_back(numeric_cast<uint8_t>(c));
    }

    uint32_t length = numeric_cast<uint32_t>(payload.size());
    auto type_data = make_nptr(type_bytes.data());
    FO_VERIFY_AND_THROW(type_data, "Chunk type data is null");

    // The checksum covers the chunk type and its payload, but not the length field written before them
    uLong crc = crc32(crc32(0, Z_NULL, 0), type_data.get(), numeric_cast<uInt>(type_bytes.size()));

    if (!payload.empty()) {
        auto payload_data = make_nptr(payload.data());
        FO_VERIFY_AND_THROW(payload_data, "Chunk payload data is null");
        crc = crc32(crc, payload_data.get(), numeric_cast<uInt>(payload.size()));
    }

    uint32_t checksum = numeric_cast<uint32_t>(crc);

    AppendPngBigEndian(png, length);
    png.insert(png.end(), type_bytes.begin(), type_bytes.end());
    png.insert(png.end(), payload.begin(), payload.end());
    AppendPngBigEndian(png, checksum);
}

static void AppendPngBigEndian(vector<uint8_t>& buf, uint32_t value)
{
    buf.emplace_back(numeric_cast<uint8_t>((value >> 24) & 0xFF));
    buf.emplace_back(numeric_cast<uint8_t>((value >> 16) & 0xFF));
    buf.emplace_back(numeric_cast<uint8_t>((value >> 8) & 0xFF));
    buf.emplace_back(numeric_cast<uint8_t>(value & 0xFF));
}

FO_END_NAMESPACE
