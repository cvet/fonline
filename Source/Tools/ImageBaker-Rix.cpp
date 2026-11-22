// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ImageBaker-Rix.h"

#if FO_IMAGE_RIX

FO_BEGIN_NAMESPACE

namespace ImageBakerRix
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(opt);
        ignore_unused(context);

        reader.SetCurPos(0x4);
        uint16_t width_wire {0};
        reader.ReadObject(width_wire);
        int32_t w {numeric_cast<int32_t>(width_wire)};
        uint16_t height_wire {0};
        reader.ReadObject(height_wire);
        int32_t h {numeric_cast<int32_t>(height_wire)};

        reader.SetCurPos(0xA);
        const_span<uint8_t> palette {reader.GetCurDataSpan(numeric_cast<size_t>(256 * 3))};

        FO_VERIFY_AND_THROW(w > 0 && h > 0, "Invalid RIX dimensions", fname, w, h);
        int64_t pixel_count {checked_mul(numeric_cast<int64_t>(w), numeric_cast<int64_t>(h))};
        size_t data_size {numeric_cast<size_t>(checked_mul(pixel_count, int64_t {4}))};
        vector<uint8_t> data {vector<uint8_t>(data_size)};
        span<ucolor> pixels {bytes_to_objects<ucolor>(span {data})};
        reader.SetCurPos(numeric_cast<size_t>(0xA + 256 * 3));

        for (size_t i {0}; i < pixels.size(); i++) {
            size_t index {checked_mul(numeric_cast<size_t>(reader.ReadUInt8()), size_t {3})};
            uint8_t r {numeric_cast<uint8_t>(numeric_cast<int32_t>(palette[checked_add(index, size_t {2})]) * 4)};
            uint8_t g {numeric_cast<uint8_t>(numeric_cast<int32_t>(palette[checked_add(index, size_t {1})]) * 4)};
            uint8_t b {numeric_cast<uint8_t>(numeric_cast<int32_t>(palette[index]) * 4)};
            pixels[i] = ucolor {r, g, b};
        }

        FrameCollection collection {};
        int32_t sequence_size {};
        uint32_t duration_ms {};
        bool have_dirs {};
        sequence_size = 1;
        duration_ms = 100;

        collection.Main.Frames.resize(numeric_cast<size_t>(sequence_size));
        collection.Main.Frames[0].Size = {w, h};
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
}

FO_END_NAMESPACE

#endif
