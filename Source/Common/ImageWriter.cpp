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

static void WritePngChunk(std::ofstream& file, string_view type, const_span<uint8_t> payload);
static void AppendPngBigEndian(vector<uint8_t>& buf, uint32_t value);

void ImageWriter::WriteSimpleTga(string_view fname, isize32 size, vector<ucolor> data)
{
    FO_STACK_TRACE_ENTRY();

    string dir = strex(fname).extract_dir().str();

    if (!dir.empty()) {
        bool dir_ok = fs_create_directories(dir);
        FO_VERIFY_AND_THROW(dir_ok, "Failed to create output directory for TGA image", dir, fname);
    }

    std::ofstream file {std::filesystem::path {fs_make_path(fname)}, std::ios::binary | std::ios::trunc};
    FO_VERIFY_AND_THROW(file, "Failed to open TGA image file for writing", fname, size, data.size());

    // ucolor keeps pixels in R, G, B, A byte order, but a TrueColor TGA stores them as B, G, R, A
    // (matching the engine's own TgaLoad reader), so swap red and blue before writing the payload
    for (auto& pixel : data) {
        std::swap(pixel.comp.r, pixel.comp.b);
    }

    const uint8_t header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
        numeric_cast<uint8_t>(size.width % 256), numeric_cast<uint8_t>(size.width / 256), //
        numeric_cast<uint8_t>(size.height % 256), numeric_cast<uint8_t>(size.height / 256), 4 * 8, 0x20};
    ptr<const uint8_t> header_bytes = header;
    file.write(header_bytes.reinterpret_as<char>().get(), static_cast<std::streamsize>(sizeof(header)));

    if (!data.empty()) {
        auto pixels = make_nptr(data.data());
        file.write(pixels.reinterpret_as<char>().get(), static_cast<std::streamsize>(data.size() * sizeof(uint32_t)));
    }

    FO_VERIFY_AND_THROW(file, "Failed while writing TGA image file", fname, size, data.size());
}

void ImageWriter::WriteSimplePng(string_view fname, isize32 size, const_span<ucolor> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0 && size.height > 0, "PNG image size must be positive", fname, size);
    FO_VERIFY_AND_THROW(numeric_cast<size_t>(size.width) * numeric_cast<size_t>(size.height) == data.size(), "PNG pixel count does not match image size", fname, size, data.size());

    string dir = strex(fname).extract_dir().str();

    if (!dir.empty()) {
        bool dir_ok = fs_create_directories(dir);
        FO_VERIFY_AND_THROW(dir_ok, "Failed to create output directory for PNG image", dir, fname);
    }

    std::ofstream file {std::filesystem::path {fs_make_path(fname)}, std::ios::binary | std::ios::trunc};
    FO_VERIFY_AND_THROW(file, "Failed to open PNG image file for writing", fname, size, data.size());

    const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    ptr<const uint8_t> signature_bytes = signature;
    file.write(signature_bytes.reinterpret_as<char>().get(), static_cast<std::streamsize>(sizeof(signature)));

    vector<uint8_t> header;
    AppendPngBigEndian(header, numeric_cast<uint32_t>(size.width));
    AppendPngBigEndian(header, numeric_cast<uint32_t>(size.height));
    header.emplace_back(8); // Bits per channel
    header.emplace_back(6); // Truecolor with alpha, so ucolor is stored as is without a channel swap
    header.emplace_back(0); // Deflate
    header.emplace_back(0); // Adaptive filtering
    header.emplace_back(0); // No interlace
    WritePngChunk(file, "IHDR", header);

    // Every scanline is prefixed with its filter byte. The None filter keeps the encoder trivial at the
    // cost of a somewhat larger file, which is a fair trade for a diagnostic screenshot
    size_t row_bytes = numeric_cast<size_t>(size.width) * sizeof(ucolor);
    vector<uint8_t> scanlines((row_bytes + 1) * numeric_cast<size_t>(size.height));
    auto scanlines_data = make_nptr(scanlines.data());
    FO_VERIFY_AND_THROW(scanlines_data, "Scanline buffer data is null");
    auto pixels_data = make_nptr(data.data());
    FO_VERIFY_AND_THROW(pixels_data, "Pixel data is null");

    for (int32_t y = 0; y < size.height; y++) {
        size_t row_start = numeric_cast<size_t>(y) * (row_bytes + 1);
        scanlines[row_start] = 0;
        MemCopy(scanlines_data.get() + row_start + 1, pixels_data.get() + numeric_cast<size_t>(y) * numeric_cast<size_t>(size.width), row_bytes);
    }

    WritePngChunk(file, "IDAT", Compressor::Compress(scanlines));
    WritePngChunk(file, "IEND", {});

    FO_VERIFY_AND_THROW(file, "Failed while writing PNG image file", fname, size, data.size());
}

static void WritePngChunk(std::ofstream& file, string_view type, const_span<uint8_t> payload)
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> chunk;
    chunk.reserve(4 + type.length() + payload.size() + 4);
    AppendPngBigEndian(chunk, numeric_cast<uint32_t>(payload.size()));

    for (char c : type) {
        chunk.emplace_back(numeric_cast<uint8_t>(c));
    }

    chunk.insert(chunk.end(), payload.begin(), payload.end());

    auto body_data = make_nptr(chunk.data());
    FO_VERIFY_AND_THROW(body_data, "Chunk buffer data is null");

    // The checksum covers the chunk type and its payload, but not the length field written before them
    uint32_t crc = numeric_cast<uint32_t>(crc32(crc32(0, Z_NULL, 0), body_data.get() + 4, numeric_cast<uInt>(chunk.size() - 4)));
    AppendPngBigEndian(chunk, crc);

    auto chunk_data = make_nptr(chunk.data());
    FO_VERIFY_AND_THROW(chunk_data, "Chunk buffer data is null");
    file.write(chunk_data.reinterpret_as<char>().get(), static_cast<std::streamsize>(chunk.size()));
}

static void AppendPngBigEndian(vector<uint8_t>& buf, uint32_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    buf.emplace_back(numeric_cast<uint8_t>((value >> 24) & 0xFF));
    buf.emplace_back(numeric_cast<uint8_t>((value >> 16) & 0xFF));
    buf.emplace_back(numeric_cast<uint8_t>((value >> 8) & 0xFF));
    buf.emplace_back(numeric_cast<uint8_t>(value & 0xFF));
}

FO_END_NAMESPACE
