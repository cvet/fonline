// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ImageBaker-ZarDecoder.h"

#if FO_IMAGE_ZAR || FO_IMAGE_TIL

FO_BEGIN_NAMESPACE

auto ImageBakerZarDecoder::Decode(string_view fname, FileReader& reader, bool swap_red_blue) -> ImageImport::FrameShot
{
    FO_TRACE_ZONE(Baking);

    using ImageImport::FrameShot;

    // Read header
    char head[6] {};
    reader.ReadObjectArray<char>(head);

    if (head[5] != 0 || strcmp(head, "<zar>") != 0) {
        throw ImageBakerException("Invalid ZAR header", fname);
    }

    uint8_t type {reader.ReadUInt8()};
    reader.GoForward(1); // \0
    int32_t width {reader.ReadLEInt32()};
    int32_t height {reader.ReadLEInt32()};
    uint8_t palette_present {reader.ReadUInt8()};

    // Read palette
    uint32_t palette[256] {};
    uint8_t def_color {0};

    if (palette_present != 0) {
        int32_t palette_count {reader.ReadLEInt32()};

        if (palette_count < 0 || palette_count > 256) {
            throw ImageBakerException("Invalid ZAR palette count", fname);
        }

        reader.ReadObjectArray(span<uint32_t> {palette, numeric_cast<size_t>(palette_count)});

        if (type == 0x34) {
            def_color = reader.ReadUInt8();
        }
    }

    // Read image
    uint32_t rle_size {reader.ReadLEUInt32()};
    size_t rle_size_value {numeric_cast<size_t>(rle_size)};
    const_span<uint8_t> rle_data {reader.GetCurDataSpan(rle_size_value)};
    reader.GoForward(rle_size_value);

    FO_VERIFY_AND_THROW(width > 0 && height > 0 && width <= numeric_cast<int32_t>(std::numeric_limits<uint16_t>::max()) && height <= numeric_cast<int32_t>(std::numeric_limits<uint16_t>::max()), "Invalid ZAR dimensions", fname, width, height);

    // Allocate data
    int64_t pixel_count {checked_mul(numeric_cast<int64_t>(width), numeric_cast<int64_t>(height))};
    size_t data_size {numeric_cast<size_t>(checked_mul(pixel_count, int64_t {4}))};
    vector<uint8_t> data {vector<uint8_t>(data_size)};
    span<uint32_t> pixels {bytes_to_objects<uint32_t>(span {data})};
    size_t pixel_pos {0};
    size_t rle_pos {0};

    // Decode
    while (rle_pos < rle_data.size()) {
        auto control {rle_data[rle_pos]};
        rle_pos++;

        int32_t control_mode {control & 3};
        int32_t control_count {control >> 2};

        int32_t payload_width {control_mode == 2 ? 2 : (control_mode == 0 ? 0 : 1)};
        size_t payload_size {checked_mul(numeric_cast<size_t>(control_count), numeric_cast<size_t>(payload_width))};
        FO_VERIFY_AND_THROW(payload_size <= rle_data.size() - rle_pos && numeric_cast<size_t>(control_count) <= pixels.size() - pixel_pos, "Invalid ZAR run length", fname, rle_pos, pixel_pos);

        for (int32_t i {0}; i < control_count; i++) {
            uint32_t col {0};

            switch (control_mode) {
            case 1:
                col = palette[rle_data[rle_pos + numeric_cast<size_t>(i)]];
                set_object_byte(col, 3, 0xFF);
                break;
            case 2:
                col = palette[rle_data[rle_pos + numeric_cast<size_t>(2) * numeric_cast<size_t>(i)]];
                set_object_byte(col, 3, rle_data[rle_pos + numeric_cast<size_t>(2) * numeric_cast<size_t>(i) + 1]);
                break;
            case 3:
                col = palette[def_color];
                set_object_byte(col, 3, rle_data[rle_pos + numeric_cast<size_t>(i)]);
                break;
            default:
                break;
            }

            if (swap_red_blue) {
                ucolor color {col};
                std::swap(color.comp.r, color.comp.b);
                col = std::bit_cast<uint32_t>(color);
            }

            pixels[pixel_pos] = col;
            pixel_pos++;
        }

        if (control_mode != 0) {
            rle_pos += numeric_cast<size_t>(control_count);

            if (control_mode == 2) {
                rle_pos += numeric_cast<size_t>(control_count);
            }
        }
    }

    FrameShot shot {};
    shot.Size = {width, height};
    shot.Data = std::move(data);
    return shot;
}

FO_END_NAMESPACE

#endif
