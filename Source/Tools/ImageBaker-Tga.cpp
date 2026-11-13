// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ImageBaker-Tga.h"

FO_BEGIN_NAMESPACE

namespace ImageBakerTga
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    static auto TgaLoad(span<const uint8_t> data, int32_t& result_width, int32_t& result_height) -> vector<uint8_t>;

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(fname);
        ignore_unused(opt);
        ignore_unused(context);

        int32_t width {0};
        int32_t height {0};
        const_span<uint8_t> tga_data {reader.GetDataSpan()};
        auto data {TgaLoad(tga_data, width, height)};

        FrameCollection collection {};
        int32_t sequence_size {};
        uint32_t duration_ms {};
        bool have_dirs {};
        sequence_size = 1;
        duration_ms = 100;

        collection.Main.Frames.resize(numeric_cast<size_t>(sequence_size));
        collection.Main.Frames[0].Size = {width, height};
        collection.Main.Frames[0].Data = std::move(data);

        // Raw formats without a playable clock use the default cadence; positive sub-ms
        // averages normalize to one ms per frame. Explicit FOFRM timelines are strict
        if (!have_dirs) {
            for (auto& sequence : collection.Dirs) {
                sequence.Frames.clear();
            }
        }

        for (uint8_t direction {0}; direction < ImageImport::GetDirectionCount(collection); direction++) {
            auto& sequence {direction == 0 ? collection.Main : collection.Dirs[numeric_cast<size_t>(direction - 1)]};

            if (std::all_of(sequence.Frames.begin(), sequence.Frames.end(), [](const ImageImport::FrameShot& frame) { return frame.Duration == timespan::zero; })) {
                uint32_t count {numeric_cast<uint32_t>(sequence.Frames.size())};
                ImageImport::SetFrameSequenceDuration(sequence, std::chrono::milliseconds {duration_ms == 0 ? count * 100 : std::max(duration_ms, count)});
            }

            (void)ImageImport::GetDuration(sequence);
        }

        return collection;
    }

    static auto TgaLoad(span<const uint8_t> data, int32_t& result_width, int32_t& result_height) -> vector<uint8_t>
    {
        size_t cur_pos {0};

        auto read_tga {[&](ptr<void> out, size_t len) {
            FO_VERIFY_AND_THROW(cur_pos <= data.size(), "TGA read position is outside the file");

            if (len > data.size() - cur_pos) {
                throw ImageBakerException("TGA reading error");
            }

            if (len != 0) {
                auto source {make_ptr(data.data() + cur_pos)};
                memory::copy(out, source, len);
            }

            cur_pos += len;
        }};

        // Load header
        uint8_t type {0};
        uint8_t pixel_depth {0};
        uint16_t width_wire {0};
        uint16_t height_wire {0};
        uint8_t unused_char {0};
        uint16_t unused_short {0};
        read_tga(&unused_char, 1);
        read_tga(&unused_char, 1);
        read_tga(&type, 1);
        read_tga(&unused_short, 2);
        read_tga(&unused_short, 2);
        read_tga(&unused_char, 1);
        read_tga(&unused_short, 2);
        read_tga(&unused_short, 2);
        read_tga(&width_wire, 2);
        read_tga(&height_wire, 2);
        read_tga(&pixel_depth, 1);
        read_tga(&unused_char, 1);

        int32_t width {numeric_cast<int32_t>(width_wire)};
        int32_t height {numeric_cast<int32_t>(height_wire)};

        // Check if the image is color indexed
        if (type == 1) {
            throw ImageBakerException("Indexed TGA is not supported");
        }

        // Check for TrueColor
        if (type != 2 && type != 10) {
            throw ImageBakerException("TGA TrueColor is not supported");
        }

        // Check for RGB(A)
        if (pixel_depth != 24 && pixel_depth != 32) {
            throw ImageBakerException("TGA support only 24 and 32 bpp");
        }

        FO_VERIFY_AND_THROW(width > 0 && height > 0, "TGA dimensions must be positive", width, height);

        // Read
        int32_t bpp {pixel_depth / 8};
        size_t bpp_size {numeric_cast<size_t>(bpp)};
        int64_t pixel_count {checked_mul(numeric_cast<int64_t>(width), numeric_cast<int64_t>(height))};
        size_t read_size {numeric_cast<size_t>(checked_mul(pixel_count, numeric_cast<int64_t>(bpp)))};
        size_t result_size {numeric_cast<size_t>(checked_mul(pixel_count, int64_t {4}))};
        vector<uint8_t> read_data {};

        if (type == 2) {
            FO_VERIFY_AND_THROW(read_size <= data.size() - cur_pos, "Truncated TGA pixel payload", read_size, data.size() - cur_pos);
            read_data.resize(read_size);

            if (read_size != 0) {
                auto read_data_begin {make_ptr(read_data.data())};
                read_tga(make_nptr(read_data_begin.get()).void_cast(), read_size);
            }
        }
        else {
            int64_t minimum_packet_count {checked_add(checked_div(checked_sub(pixel_count, int64_t {1}), int64_t {128}), int64_t {1})};
            int64_t minimum_packet_size {checked_add(int64_t {1}, numeric_cast<int64_t>(bpp))};
            size_t minimum_packet_bytes {numeric_cast<size_t>(checked_mul(minimum_packet_count, minimum_packet_size))};
            FO_VERIFY_AND_THROW(minimum_packet_bytes <= data.size() - cur_pos, "Truncated TGA RLE payload", minimum_packet_bytes, data.size() - cur_pos);
            read_data.resize(read_size);

            size_t bytes_read {0};

            while (bytes_read < read_size) {
                uint8_t packet_header {0};
                read_tga(&packet_header, 1);

                int32_t run_length {checked_add(numeric_cast<int32_t>(packet_header & 0x7Fu), int32_t {1})};
                int32_t run_bytes_wide {checked_mul(run_length, bpp)};
                size_t run_bytes {numeric_cast<size_t>(run_bytes_wide)};

                if ((packet_header & 0x80) != 0) {
                    FO_VERIFY_AND_THROW(run_bytes <= read_size - bytes_read, "TGA RLE run packet exceeds image data", run_bytes, read_size - bytes_read);

                    uint8_t color[4] {};
                    read_tga(color, bpp_size);

                    for (size_t offset {0}; offset < run_bytes; offset += bpp_size) {
                        for (size_t c {0}; c < bpp_size; c++) {
                            read_data[bytes_read + offset + c] = color[c];
                        }
                    }

                    bytes_read += run_bytes;
                }
                else {
                    size_t to_copy {std::min(run_bytes, read_size - bytes_read)};
                    FO_VERIFY_AND_THROW(run_bytes <= data.size() - cur_pos, "Truncated TGA RLE raw packet", run_bytes, data.size() - cur_pos);
                    read_tga(read_data.data() + bytes_read, to_copy);
                    cur_pos += run_bytes - to_copy;
                    bytes_read += run_bytes;
                }
            }
        }

        // Copy data
        vector<uint8_t> result {vector<uint8_t>(result_size)};

        size_t source_row_size {numeric_cast<size_t>(checked_mul(numeric_cast<int64_t>(width), numeric_cast<int64_t>(bpp)))};
        size_t result_row_size {numeric_cast<size_t>(checked_mul(numeric_cast<int64_t>(width), int64_t {4}))};

        for (int32_t y {0}; y < height; y++) {
            int32_t flipped_y {checked_sub(checked_sub(height, y), int32_t {1})};
            size_t source_row {checked_mul(numeric_cast<size_t>(y), source_row_size)};
            size_t result_row {checked_mul(numeric_cast<size_t>(flipped_y), result_row_size)};

            for (int32_t x {0}; x < width; x++) {
                size_t source_x {checked_mul(numeric_cast<size_t>(x), bpp_size)};
                size_t result_x {checked_mul(numeric_cast<size_t>(x), size_t {4})};
                size_t source_pos {checked_add(source_row, source_x)};
                size_t result_pos {checked_add(result_row, result_x)};
                result[result_pos + 0] = read_data[source_pos + 2];
                result[result_pos + 1] = read_data[source_pos + 1];
                result[result_pos + 2] = read_data[source_pos + 0];
                result[result_pos + 3] = bpp == 4 ? read_data[source_pos + 3] : 0xFF;
            }
        }

        // Return data
        result_width = width;
        result_height = height;

        return result;
    }
}

FO_END_NAMESPACE
