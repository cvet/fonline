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

#include "ImageBaker-Png.h"

#include "png.h"

FO_BEGIN_NAMESPACE

namespace ImageBakerPng
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    static auto PngLoad(ptr<const uint8_t> data, int32_t& result_width, int32_t& result_height) -> vector<uint8_t>;

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(fname);
        ignore_unused(opt);
        ignore_unused(context);

        int32_t width = 0;
        int32_t height = 0;
        const_span<uint8_t> png_data = reader.GetDataSpan();
        FO_VERIFY_AND_THROW(!png_data.empty(), "PNG file has no data to load");
        auto png_data_ptr = make_ptr(png_data.data());
        auto data = PngLoad(png_data_ptr, width, height);

        FrameCollection collection;
        uint16_t sequence_size {};
        uint32_t anim_ticks {};
        bool have_dirs {};
        sequence_size = 1;
        anim_ticks = 100;

        collection.Main.Frames.resize(sequence_size);
        collection.Main.Frames[0].Width = numeric_cast<uint16_t>(width);
        collection.Main.Frames[0].Height = numeric_cast<uint16_t>(height);
        collection.Main.Frames[0].Data = std::move(data);

        // Raw formats without a playable clock use the default cadence; positive sub-ms
        // averages normalize to one ms per frame. Explicit FOFRM timelines are strict.
        if (!have_dirs) {
            for (auto& sequence : collection.Dirs) {
                sequence.Frames.clear();
            }
        }

        for (uint8_t direction = 0; direction < ImageImport::GetDirectionCount(collection); direction++) {
            auto& sequence = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

            if (std::all_of(sequence.Frames.begin(), sequence.Frames.end(), [](const ImageImport::FrameShot& frame) { return frame.DurationMs == 0; })) {
                uint32_t count = numeric_cast<uint32_t>(sequence.Frames.size());
                ImageImport::SetFrameSequenceDuration(sequence, anim_ticks == 0 ? count * 100 : std::max(anim_ticks, count));
            }

            (void)ImageImport::GetDurationMs(sequence);
        }

        return collection;
    }

    static auto PngMalloc(png_structp png_ptr, png_alloc_size_t size) -> png_voidp
    {
        ignore_unused(png_ptr);
        return safe_alloc::malloc_raw(size).get();
    }

    static void PngFree(png_structp png_ptr, png_voidp mem)
    {
        ignore_unused(png_ptr);
        safe_alloc::free_raw(mem);
    }

    static auto PngLoad(ptr<const uint8_t> data, int32_t& result_width, int32_t& result_height) -> vector<uint8_t>
    {
        // Created through the _2 form so libpng allocates from the engine memory system rather than the CRT heap
        auto png_ptr = make_nptr(png_create_read_struct_2(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr, nullptr, &PngMalloc, &PngFree));
        FO_VERIFY_AND_THROW(png_ptr, "Failed to create PNG read structure");
        nptr<png_info> info_ptr;

        try {
            struct PngMessage
            {
                [[noreturn]] static void Error(png_structp png_ptr, png_const_charp error_msg)
                {
                    ignore_unused(png_ptr);
                    throw ImageBakerException("PNG loading error", error_msg);
                }

                static void Warning(png_structp png_ptr, png_const_charp error_msg)
                {
                    ignore_unused(png_ptr);
                    ignore_unused(error_msg);
                }
            };

            png_set_error_fn(png_ptr.get(), png_get_error_ptr(png_ptr.get()), &PngMessage::Error, &PngMessage::Warning);

            info_ptr = png_create_info_struct(png_ptr.get());
            FO_VERIFY_AND_THROW(info_ptr, "Failed to create PNG info structure");

            struct PngReader
            {
                static void Read(png_structp png_ptr, png_bytep png_data, png_size_t length)
                {
                    if (length == 0) {
                        return;
                    }

                    auto io_ptr = cast_from_void<const uint8_t**>(png_get_io_ptr(png_ptr));
                    FO_VERIFY_AND_THROW(io_ptr, "PNG read cursor is null");
                    auto source = make_ptr(*io_ptr);
                    auto target = make_ptr(png_data);
                    memory::copy(target, source, length);
                    auto next_source = source.offset(length);
                    *io_ptr = next_source.get();
                }
            };

            // Get info
            nptr<const uint8_t> png_data_cursor = data;
            png_set_read_fn(png_ptr.get(), make_nptr(png_data_cursor.get_pp()).void_cast(), &PngReader::Read);
            png_read_info(png_ptr.get(), info_ptr.get());

            png_uint_32 width = 0;
            png_uint_32 height = 0;
            int32_t bit_depth = 0;
            int32_t color_type = 0;
            png_get_IHDR(png_ptr.get(), info_ptr.get(), &width, &height, &bit_depth, &color_type, nullptr, nullptr, nullptr);

            // Settings
            png_set_strip_16(png_ptr.get());
            png_set_packing(png_ptr.get());

            if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
                png_set_expand(png_ptr.get());
            }
            if (color_type == PNG_COLOR_TYPE_PALETTE) {
                png_set_expand(png_ptr.get());
            }
            if (png_get_valid(png_ptr.get(), info_ptr.get(), PNG_INFO_tRNS) != 0) {
                png_set_expand(png_ptr.get());
            }

            png_set_filler(png_ptr.get(), 0x000000ff, PNG_FILLER_AFTER);
            png_read_update_info(png_ptr.get(), info_ptr.get());

            // Read
            vector<png_bytep> row_pointers;
            row_pointers.resize(height);

            vector<uint8_t> result;
            result.resize(numeric_cast<size_t>(width) * height * 4);

            for (png_uint_32 i = 0; i < height; i++) {
                auto row = make_ptr(result.data() + numeric_cast<size_t>(i) * numeric_cast<size_t>(width) * 4);
                row_pointers[i] = row.get();
            }

            auto rows = make_ptr(row_pointers.data());
            png_read_image(png_ptr.get(), rows.get());
            png_read_end(png_ptr.get(), info_ptr.get());
            png_destroy_read_struct(png_ptr.get_pp(), info_ptr.get_pp(), static_cast<png_infopp>(nullptr));

            result_width = numeric_cast<int32_t>(width);
            result_height = numeric_cast<int32_t>(height);

            return result;
        }
        catch (...) {
            png_destroy_read_struct(png_ptr.get_pp(), info_ptr.get_pp(), static_cast<png_infopp>(nullptr));

            throw;
        }
    }
}

FO_END_NAMESPACE
