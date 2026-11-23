// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ImageBaker-Til.h"

#include "ImageBaker-ZarDecoder.h"

#if FO_IMAGE_TIL

FO_BEGIN_NAMESPACE

namespace ImageBakerTil
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(opt);
        ignore_unused(context);

        // Read header
        char head[7] {};
        reader.ReadObjectArray<char>(head);

        if (head[6] != 0 || strcmp(head, "<tile>") != 0) {
            throw ImageBakerException("Invalid TIL file header", fname);
        }

        while (reader.ReadUInt8() != 0) {
            // Nop
        }

        reader.GoForward(7 + 4); // Unknown

        int32_t width {reader.ReadLEInt32()};
        ignore_unused(width);
        int32_t height {reader.ReadLEInt32()};
        ignore_unused(height);

        if (!reader.SeekFragment("<tiledata>")) {
            throw ImageBakerException("Tiledata in TIL file not found", fname);
        }

        reader.GoForward(10 + 3); // Signature
        int32_t frames_count {reader.ReadLEInt32()};
        FO_VERIFY_AND_THROW(frames_count > 0 && frames_count <= numeric_cast<int32_t>(std::numeric_limits<uint16_t>::max()), "TIL frame count is outside the serialized range", fname, frames_count);

        FrameCollection collection {};
        int32_t sequence_size {};
        uint32_t duration_ms {};
        bool have_dirs {};
        sequence_size = numeric_cast<int32_t>(frames_count);
        duration_ms = numeric_cast<uint32_t>(checked_mul(int64_t {100}, numeric_cast<int64_t>(frames_count)));

        collection.Main.Frames.resize(numeric_cast<size_t>(sequence_size));

        for (int32_t frm {0}; frm < frames_count; frm++) {
            collection.Main.Frames[numeric_cast<size_t>(frm)] = ImageBakerZarDecoder::Decode(fname, reader, true);
        }

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
